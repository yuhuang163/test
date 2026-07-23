#include "serial_channel.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMutex>
#include <QPointer>
#include <QSerialPortInfo>
#include <QSet>
#include <QTimer>
#include <QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr int kPortListCacheTtlMs = 2500;

QMutex g_portListMutex;
QStringList g_cachedPortNames;
QStringList g_lastInfoPortNames; // CreateFile 前的口名集合，未变则跳过探测
QElapsedTimer g_portListCacheTimer;
bool g_portListScanRunning = false;
QList<QPointer<QComboBox>> g_pendingPortCombos;

void applyPortsToComboBox(QComboBox* comboBox, const QStringList& ports) {
    if (!comboBox)
        return;

    QSet<QString> currentItems;
    for (int i = 0; i < comboBox->count(); ++i)
        currentItems.insert(comboBox->itemText(i));

    for (const QString& name : ports) {
        if (!currentItems.contains(name))
            comboBox->addItem(name);
        currentItems.remove(name);
    }

    for (const QString& item : currentItems) {
        const int index = comboBox->findText(item);
        if (index != -1)
            comboBox->removeItem(index);
    }
}

QStringList enumeratePresentPortNames() {
    QStringList infoNames;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports) {
        const QString name = info.portName();
        if (SerialChannel::isHiddenSystemPort(name))
            continue;
        infoNames.append(name);
    }

    {
        QMutexLocker locker(&g_portListMutex);
        // 系统枚举出的口名集合未变时，复用上次 CreateFile 结果，避免无意义卡顿
        if (!g_lastInfoPortNames.isEmpty() && infoNames == g_lastInfoPortNames && !g_cachedPortNames.isEmpty())
            return g_cachedPortNames;
    }

    QStringList names;
    names.reserve(infoNames.size());
    for (const QString& name : infoNames) {
        if (!SerialChannel::isPortPresent(name))
            continue;
        names.append(name);
    }

    QMutexLocker locker(&g_portListMutex);
    g_lastInfoPortNames = infoNames;
    g_cachedPortNames = names;
    if (!g_portListCacheTimer.isValid())
        g_portListCacheTimer.start();
    else
        g_portListCacheTimer.restart();
    return names;
}

void finishPortListScanOnUi(const QStringList& names) {
    QList<QPointer<QComboBox>> combos;
    {
        QMutexLocker locker(&g_portListMutex);
        g_cachedPortNames = names;
        if (!g_portListCacheTimer.isValid())
            g_portListCacheTimer.start();
        else
            g_portListCacheTimer.restart();
        g_portListScanRunning = false;
        combos.swap(g_pendingPortCombos);
    }
    for (const QPointer<QComboBox>& combo : combos) {
        if (combo)
            applyPortsToComboBox(combo, names);
    }
}

void startPortListScanAsync() {
    QtConcurrent::run([]() {
        const QStringList names = enumeratePresentPortNames();
        // 回到 GUI 线程刷新下拉，避免 CreateFile 堵主线程
        if (QCoreApplication::instance()) {
            QTimer::singleShot(0, QCoreApplication::instance(), [names]() { finishPortListScanOnUi(names); });
        }
    });
}

} // namespace

SerialChannel::SerialChannel(QObject* parent) : QObject(parent), port_(new QSerialPort(this)), readTimer_(new QTimer(this)) {
    readTimer_->setSingleShot(true);
    connect(port_, &QSerialPort::readyRead, this, &SerialChannel::onReadyRead);
    connect(readTimer_, &QTimer::timeout, this, &SerialChannel::onReadTimer);
    connect(port_, &QSerialPort::errorOccurred, this, &SerialChannel::onPortError);
}

SerialChannel::~SerialChannel() {
    close();
}

QSerialPort* SerialChannel::port() {
    return port_;
}

const QSerialPort* SerialChannel::port() const {
    return port_;
}

void SerialChannel::setDefaultParams(const OpenParams& params) {
    params_ = params;
}

bool SerialChannel::open(const QString& portName) {
    OpenParams p = params_;
    p.portName = portName;
    return open(p);
}

bool SerialChannel::open(const OpenParams& params) {
    close();
    params_ = params;
    if (params_.portName.isEmpty())
        return false;

    applyLineSettings();
    if (!port_->open(QIODevice::ReadWrite)) {
        emit errorOccurred(port_->error(), port_->errorString());
        return false;
    }

    applyRtsDtr();
    readTimer_->setInterval(qMax(1, params_.readDebounceMs));
    emit opened();
    return true;
}

void SerialChannel::close() {
    readTimer_->stop();
    rxBuffer_.clear();
    if (port_->isOpen()) {
        port_->close();
        emit closed();
    }
}

bool SerialChannel::isOpen() const {
    return port_->isOpen();
}

qint64 SerialChannel::write(const QByteArray& data) {
    if (!port_->isOpen())
        return -1;
    return port_->write(data);
}

QString SerialChannel::portName() const {
    return port_->portName();
}

QString SerialChannel::errorString() const {
    return port_->errorString();
}

void SerialChannel::clearReceiveBuffer() {
    readTimer_->stop();
    rxBuffer_.clear();
    if (port_->isOpen())
        port_->clear(QSerialPort::Input);
}

bool SerialChannel::waitForFrame(QByteArray* outFrame, int timeoutMs) {
    if (!outFrame)
        return false;
    outFrame->clear();
    if (!port_->isOpen())
        return false;

    const int waitMs = qMax(1, timeoutMs);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QByteArray captured;
    const QMetaObject::Connection frameConn =
        connect(this, &SerialChannel::frameReceived, &loop, [&](const QByteArray& frame) {
            captured = frame;
            loop.quit();
        });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(waitMs);
    loop.exec();
    disconnect(frameConn);
    if (captured.isEmpty())
        return false;
    *outFrame = captured;
    return true;
}

bool SerialChannel::exchange(const QByteArray& request, QByteArray* response, int timeoutMs) {
    if (!response)
        return false;
    response->clear();
    if (!port_->isOpen() || request.isEmpty())
        return false;

    clearReceiveBuffer();

    const int waitMs = qMax(1, timeoutMs);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QByteArray captured;
    const QMetaObject::Connection frameConn =
        connect(this, &SerialChannel::frameReceived, &loop, [&](const QByteArray& frame) {
            captured = frame;
            loop.quit();
        });
    connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    if (write(request) != request.size()) {
        disconnect(frameConn);
        return false;
    }
    port_->waitForBytesWritten(qMin(2000, waitMs));

    timeout.start(waitMs);
    loop.exec();
    disconnect(frameConn);
    if (captured.isEmpty())
        return false;
    *response = captured;
    return true;
}

bool SerialChannel::isHiddenSystemPort(const QString& portName) {
    // 电脑主板串口，产测/dongle/治具不会用到，避免误选
    return portName.compare(QStringLiteral("COM1"), Qt::CaseInsensitive) == 0;
}

bool SerialChannel::isPortPresent(const QString& portName) {
    const QString name = portName.trimmed();
    if (name.isEmpty())
        return false;
#ifdef Q_OS_WIN
    // 仅查注册表/SetupAPI 时，已卸载的 USB 口常仍占 COM 号；用 CreateFile 区分幽灵口与占用口
    const QString path = QStringLiteral("\\\\.\\%1").arg(name);
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                OPEN_EXISTING, 0, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
        return true;
    }
    const DWORD err = GetLastError();
    return err == ERROR_ACCESS_DENIED || err == ERROR_SHARING_VIOLATION || err == ERROR_BUSY;
#else
    Q_UNUSED(name);
    return true;
#endif
}

QStringList SerialChannel::availablePortNames() {
    QMutexLocker locker(&g_portListMutex);
    if (g_portListCacheTimer.isValid() && g_portListCacheTimer.elapsed() < kPortListCacheTtlMs &&
        !g_cachedPortNames.isEmpty()) {
        return g_cachedPortNames;
    }
    locker.unlock();
    // 同步调用方：后台探测，避免在 UI 线程 CreateFile
    return enumeratePresentPortNames();
}

void SerialChannel::updateComboBoxPorts(QComboBox* comboBox) {
    if (!comboBox)
        return;

    QStringList cached;
    bool startScan = false;
    {
        QMutexLocker locker(&g_portListMutex);
        cached = g_cachedPortNames;
        const bool fresh = g_portListCacheTimer.isValid() && g_portListCacheTimer.elapsed() < kPortListCacheTtlMs &&
                           !g_cachedPortNames.isEmpty();
        if (!fresh) {
            g_pendingPortCombos.append(comboBox);
            if (!g_portListScanRunning) {
                g_portListScanRunning = true;
                startScan = true;
            }
        }
    }

    // 先刷缓存，再按需后台重扫（CreateFile 探测幽灵口很慢，不能在 UI 线程做）
    if (!cached.isEmpty())
        applyPortsToComboBox(comboBox, cached);
    if (startScan)
        startPortListScanAsync();
}

void SerialChannel::applyLineSettings() {
    port_->setPortName(params_.portName);
    port_->setBaudRate(params_.baudRate);
    port_->setDataBits(QSerialPort::Data8);
    port_->setParity(QSerialPort::NoParity);
    port_->setStopBits(QSerialPort::OneStop);
    port_->setReadBufferSize(params_.readBufferSize);
    port_->setFlowControl(params_.flowControl);
}

void SerialChannel::applyRtsDtr() {
    switch (params_.rtsDtrMode) {
    case RtsDtrMode::None:
        break;
    case RtsDtrMode::Enable:
        port_->setRequestToSend(true);
        port_->setDataTerminalReady(true);
        break;
    case RtsDtrMode::DtrOnly:
        port_->setDataTerminalReady(true);
        break;
    case RtsDtrMode::ToggleReset:
        port_->setRequestToSend(true);
        port_->setDataTerminalReady(true);
        port_->setRequestToSend(false);
        port_->setDataTerminalReady(false);
        port_->setRequestToSend(true);
        port_->setDataTerminalReady(true);
        break;
    case RtsDtrMode::FullReset:
        port_->setRequestToSend(true);
        port_->setDataTerminalReady(true);
        port_->setRequestToSend(false);
        port_->setDataTerminalReady(false);
        port_->setRequestToSend(true);
        port_->setDataTerminalReady(true);
        port_->setRequestToSend(false);
        port_->setDataTerminalReady(false);
        break;
    }
}

void SerialChannel::onReadyRead() {
    rxBuffer_.append(port_->readAll());
    readTimer_->start();
}

void SerialChannel::onReadTimer() {
    if (rxBuffer_.isEmpty())
        return;
    const QByteArray frame = rxBuffer_;
    rxBuffer_.clear();
    emit frameReceived(frame);
}

void SerialChannel::onPortError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::NoError)
        return;
    emit errorOccurred(error, port_->errorString());
}
