#include "qfixturemanager.h"

#include <QDateTime>
#include <QEventLoop>
#include <QDebug>
#include <QThread>
#include <QtConcurrent>

#include "camera_fixture_device.h"
#include "imu_fixture_device.h"
#include "hz_pcba_fixture_device.h"
#include "press_fixture_device.h"
#include "qlog.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QFixtureManager::QFixtureManager(QObject* parent)
    : QObject(parent),
      fixtureSerialPort_(new QSerialPort(this)),
      fixtureSerialPortTimer_(new QTimer(this)),
      log_(new Qlog) {

    qRegisterMetaType<FixturePacketData>("FixturePacketData");

    const auto writeFn = [this](const QByteArray& data, bool logTx, bool startAction) {
        writeFixturePort(data, logTx, startAction);
    };

    hzPcbaDevice_ = new HzPcbaFixtureDevice(writeFn);
    pressDevice_ = new PressFixtureDevice(writeFn, [this](unsigned int msec) { delayMsec(msec); });
    cameraDevice_ = new CameraFixtureDevice(writeFn);
    imuDevice_ = new ImuFixtureDevice(writeFn);

    connect(fixtureSerialPort_, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleError(QSerialPort::SerialPortError)));

    connect(fixtureSerialPort_, &QSerialPort::readyRead, this, [=]() {
        fixtureSerialPortTimer_->start(10);
        fixtureSerialPortBuf_.append(fixtureSerialPort_->readAll());
    });

    running_.store(true);
    future_ = QtConcurrent::run([this]() {
        while (running_.load()) {
            solve_frame();
            QThread::msleep(10);
        }
    });
}

QFixtureManager::~QFixtureManager() {
    running_.store(false);
    future_.waitForFinished();
    close();
    delete hzPcbaDevice_;
    delete pressDevice_;
    delete cameraDevice_;
    delete imuDevice_;
    delete log_;
}

bool QFixtureManager::open(const QString& portName, int baudRate) {
    close();
    fixtureSerialPort_->setPortName(portName);
    fixtureSerialPort_->setBaudRate(baudRate);
    fixtureSerialPort_->setDataBits(QSerialPort::Data8);
    fixtureSerialPort_->setParity(QSerialPort::NoParity);
    fixtureSerialPort_->setStopBits(QSerialPort::OneStop);
    fixtureSerialPort_->setReadBufferSize(4096);
    fixtureSerialPort_->setFlowControl(QSerialPort::NoFlowControl);

    if (fixtureSerialPort_->open(QIODevice::ReadWrite)) {
        fixtureSerialPort_->setRequestToSend(true);
        fixtureSerialPort_->setDataTerminalReady(true);
        fixtureSerialPort_->setDataTerminalReady(true);

        connect(fixtureSerialPortTimer_, &QTimer::timeout, this, &QFixtureManager::readFixtureSerialPortData);
        emit connected();
        return true;
    }
    return false;
}

void QFixtureManager::close() {
    if (fixtureSerialPort_->isOpen()) {
        fixtureSerialPort_->close();
        emit disconnected();
    }
    disconnect(fixtureSerialPortTimer_, &QTimer::timeout, this, &QFixtureManager::readFixtureSerialPortData);
}

bool QFixtureManager::isOpen() const {
    return fixtureSerialPort_ && fixtureSerialPort_->isOpen();
}

QString QFixtureManager::portName() const {
    return fixtureSerialPort_ ? fixtureSerialPort_->portName() : QString();
}

QString QFixtureManager::errorString() const {
    return fixtureSerialPort_ ? fixtureSerialPort_->errorString() : QString();
}

void QFixtureManager::sendPcbaFrame(const QByteArray& frame) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送 PCBA 帧";
        return;
    }
    hzPcbaDevice_->sendFrame(frame);
}

void QFixtureManager::writeFixturePort(const QByteArray& data, bool logTx, bool startAction) {
    if (data.isEmpty())
        return;
    qDebug().noquote() << "FIXTURE TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    fixtureSerialPort_->write(data);
    if (logTx && log_)
        log_->save_fixture_uart_log(1, data);
    if (startAction)
        emit start_fix_action(1);
}

void QFixtureManager::readFixtureSerialPortData() {
    fixtureSerialPortTimer_->stop();
    const QByteArray dataTemp = fixtureSerialPortBuf_;
    fixtureSerialPortBuf_.clear();

    qDebug().noquote() << "FIXTURE RX:" << QString::fromLatin1(dataTemp.toHex(' ').toUpper());
    qDebug() << "接收到治具数据" << dataTemp;
    qDebug() << "开始处理" << dataTemp.size();

    hzPcbaDevice_->onRx(dataTemp);
    dispatchTextProtocols(dataTemp);
    if (log_)
        log_->save_fixture_uart_log(0, dataTemp);
}

void QFixtureManager::dispatchTextProtocols(const QByteArray& data) {
    const FixtureImuUartEvent imuEv = imuDevice_->parseReceived(data);
    if (imuEv.ok)
        emit send_data_to_mechine_imu(1);
    if (imuEv.error)
        emit send_data_to_mechine_imu(0);

    const FixturePressUartEvent pressEv = pressDevice_->parseReceived(data);
    if (pressEv.pressNotifyState > 0)
        emit send_data_to_mechine_press(pressEv.pressNotifyState);
}

void QFixtureManager::solve_frame() {
    if (!hzPcbaDevice_)
        return;
    hzPcbaDevice_->pollFrames([this](const FixturePcbaUartEvent& ev) {
        switch (ev.type) {
        case FixturePcbaUartEvent::Type::ShortSleep:
            emit send_data_to_mechine_sleep(ev.packet);
            break;
        case FixturePcbaUartEvent::Type::ShortStart:
            emit send_data_to_mechine_start();
            break;
        case FixturePcbaUartEvent::Type::FullPacket:
            emit send_data_to_mechine(ev.packet);
            break;
        default:
            break;
        }
    });
}

void QFixtureManager::sendimuData(imuFixtureState fixstate) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送数据";
        return;
    }
    imuDevice_->sendState(fixstate);
}

void QFixtureManager::set_camera_action(camreaFixtureState fixstate) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送数据";
        return;
    }
    cameraDevice_->sendAction(fixstate);
}

void QFixtureManager::sendFixtureData(FixtureState fixstate) {
    if (!isOpen()) {
        qDebug() << "未打开治具串口，无法发送数据";
        return;
    }
    pressDevice_->sendFixtureState(fixstate);
}

void QFixtureManager::send_start_command(int i) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送 PCBA 开始命令";
        return;
    }
    hzPcbaDevice_->sendStartTest(i);
}

void QFixtureManager::send_start_sleep_command(int i) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送 PCBA 休眠命令";
        return;
    }
    hzPcbaDevice_->sendSleep(i);
}

void QFixtureManager::send_start_white_modle_command(int i) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送 PCBA 亮白模式命令";
        return;
    }
    hzPcbaDevice_->sendWhiteMode(i);
}

void QFixtureManager::handleError(QSerialPort::SerialPortError error) {
    qDebug() << "串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        close();
        emit errorOccurred(error, QStringLiteral("治具串口被拔出！"));
    }
}

void QFixtureManager::delayMsec(unsigned int msec) {
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, SLOT(quit()));
    loop.exec();
}

qint64 QFixtureManager::lastCommidTimestamp() const {
    return pressDevice_ ? pressDevice_->lastCommidTimestamp() : 0;
}

void QFixtureManager::setLastCommidTimestamp(qint64 timestamp) {
    if (pressDevice_)
        pressDevice_->setLastCommidTimestamp(timestamp);
}

machine_command_id_e QFixtureManager::lastCommid() const {
    return pressDevice_ ? pressDevice_->lastCommid() : COMMAND_ID_MAX;
}

void QFixtureManager::setLastCommid(machine_command_id_e commandId) {
    if (pressDevice_)
        pressDevice_->setLastCommid(commandId);
}

void QFixtureManager::send_command_to_machine(int command_id, int numb) {
    if (!isOpen()) {
        qDebug() << "未打开串口，无法发送数据";
        return;
    }
    pressDevice_->sendCommand(command_id, numb);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
