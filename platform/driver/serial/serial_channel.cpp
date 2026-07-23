#include "serial_channel.h"

#include <QComboBox>
#include <QEventLoop>
#include <QSerialPortInfo>
#include <QSet>
#include <QTimer>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

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

QStringList SerialChannel::availablePortNames() {
    QStringList names;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo& info : ports)
        names.append(info.portName());
    return names;
}

void SerialChannel::updateComboBoxPorts(QComboBox* comboBox) {
    if (!comboBox)
        return;

    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    QSet<QString> currentItems;
    for (int i = 0; i < comboBox->count(); ++i)
        currentItems.insert(comboBox->itemText(i));

    for (const QSerialPortInfo& info : ports) {
        if (!currentItems.contains(info.portName()))
            comboBox->addItem(info.portName());
        currentItems.remove(info.portName());
    }

    for (const QString& item : currentItems) {
        const int index = comboBox->findText(item);
        if (index != -1)
            comboBox->removeItem(index);
    }
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
