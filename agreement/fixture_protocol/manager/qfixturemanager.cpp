#include "qfixturemanager.h"

#include <QDateTime>
#include <QEventLoop>
#include <QDebug>
#include <QThread>
#include <QtConcurrent>

#include "fixture_pcba_device.h"
#include "fixture_press_device.h"
#include "fixture_imu_device.h"
#include "fixture_camera_device.h"
#include "qlog.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QFixtureManager::QFixtureManager(QObject* parent)
    : QObject(parent),
      fixtureSerialPort_(new QSerialPort(this)),
      fixtureSerialPortTimer_(new QTimer(this)),
      log_(new Qlog),
      pressProtocol_(new FixturePressUartProtocol) {

    qRegisterMetaType<FixturePacketData>("FixturePacketData");

    connect(fixtureSerialPort_, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleError(QSerialPort::SerialPortError)));

    connect(fixtureSerialPort_, &QSerialPort::readyRead, this, [=]() {
        fixtureSerialPortTimer_->start(10);
        fixtureSerialPortBuf_.append(fixtureSerialPort_->readAll());
    });

    fixRingBuf_ = new RingBuf(&p_fixRingBuffer_, fix_ring_buffer_, 1, sizeof(fix_ring_buffer_));
    pcbaProtocol_ = new FixturePcbaUartProtocol(fixRingBuf_, &p_fixRingBuffer_, frame_buf_, sizeof(frame_buf_));

    running_.store(true);
    future_ = QtConcurrent::run([this]() {
        while (running_.load()) {
            solve_frame();
            QThread::msleep(10);
        }
    });

    pressProtocol_->initCommands();
}

QFixtureManager::~QFixtureManager() {
    running_.store(false);
    future_.waitForFinished();
    close();
    delete pcbaProtocol_;
    delete pressProtocol_;
    delete fixRingBuf_;
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
    writeFixturePort(frame, true, false);
}

void QFixtureManager::writeFixturePort(const QByteArray& data, bool logTx, bool startAction) {
    if (data.isEmpty()) {
        return;
    }
    qDebug().noquote() << "FIXTURE TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    fixtureSerialPort_->write(data);
    if (logTx && log_) {
        log_->save_fixture_uart_log(1, data);
    }
    if (startAction) {
        emit start_fix_action(1);
    }
}

void QFixtureManager::readFixtureSerialPortData() {
    fixtureSerialPortTimer_->stop();
    const QByteArray dataTemp = fixtureSerialPortBuf_;
    fixtureSerialPortBuf_.clear();

    qDebug().noquote() << "FIXTURE RX:" << QString::fromLatin1(dataTemp.toHex(' ').toUpper());
    qDebug() << "接收到治具数据" << dataTemp;

    const int writeLen = fixRingBuf_->usmile_ring_buffer_write(
        &p_fixRingBuffer_, reinterpret_cast<uint8_t*>(const_cast<char*>(dataTemp.data())), dataTemp.size());
    if (writeLen < dataTemp.size()) {
        qDebug() << "write_len:" << writeLen << "len:" << dataTemp.size();
    }
    qDebug() << "开始处理" << dataTemp.size();

    dispatchTextProtocols(dataTemp);
    if (log_) {
        log_->save_fixture_uart_log(0, dataTemp);
    }
}

void QFixtureManager::dispatchTextProtocols(const QByteArray& data) {
    const FixtureImuUartEvent imuEv = FixtureImuUartProtocol::parseReceived(data);
    if (imuEv.ok) {
        emit send_data_to_mechine_imu(1);
    }
    if (imuEv.error) {
        emit send_data_to_mechine_imu(0);
    }

    const FixturePressUartEvent pressEv = FixturePressUartProtocol::parseReceived(data);
    if (pressEv.pressNotifyState > 0) {
        emit send_data_to_mechine_press(pressEv.pressNotifyState);
    }
}

void QFixtureManager::solve_frame() {
    if (!pcbaProtocol_) {
        return;
    }
    pcbaProtocol_->pollFrames([this](const FixturePcbaUartEvent& ev) {
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
    const QByteArray dataToSend = FixtureImuUartProtocol::buildCommand(fixstate);
    writeFixturePort(dataToSend, true, true);
}

void QFixtureManager::set_camera_action(camreaFixtureState fixstate) {
    if (!isOpen()) {
        qDebug() << "治具串口未打开，无法发送数据";
        return;
    }
    const QByteArray dataToSend = FixtureCameraUartProtocol::buildCommand(fixstate);
    writeFixturePort(dataToSend, true, true);
}

void QFixtureManager::sendFixtureData(FixtureState fixstate) {
    if (!isOpen()) {
        qDebug() << "未打开治具串口，无法发送数据";
        return;
    }
    const QByteArray dataToSend = FixturePressUartProtocol::buildFixtureStateCommand(fixstate);
    writeFixturePort(dataToSend, false, false);
}

void QFixtureManager::send_start_command(int i) {
    const QByteArray dataToSend = FixturePcbaUartProtocol::buildStartTestCommand(i);
    if (!dataToSend.isEmpty()) {
        writeFixturePort(dataToSend, true, false);
        qDebug() << "已发送开始命令" << i;
    } else {
        qDebug() << "Invalid command index";
    }
}

void QFixtureManager::send_start_sleep_command(int i) {
    const QByteArray dataToSend = FixturePcbaUartProtocol::buildSleepCommand(i);
    if (!dataToSend.isEmpty()) {
        writeFixturePort(dataToSend, true, false);
        qDebug() << "已发送开始休眠命令" << i;
    } else {
        qDebug() << "Invalid command index";
    }
}

void QFixtureManager::send_start_white_modle_command(int i) {
    const QByteArray dataToSend = FixturePcbaUartProtocol::buildWhiteModeCommand(i);
    if (!dataToSend.isEmpty()) {
        writeFixturePort(dataToSend, true, false);
        qDebug() << "已发送已经进入亮白模式命令" << i;
    } else {
        qDebug() << "Invalid command index";
    }
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
    return last_commid_timestamp_;
}

void QFixtureManager::setLastCommidTimestamp(qint64 timestamp) {
    last_commid_timestamp_ = timestamp;
}

machine_command_id_e QFixtureManager::lastCommid() const {
    return last_commid_;
}

void QFixtureManager::setLastCommid(machine_command_id_e commandId) {
    last_commid_ = commandId;
}

void QFixtureManager::send_command_to_machine(int command_id, int numb) {
    if (!isOpen()) {
        qDebug() << "未打开串口，无法发送数据";
        return;
    }
    if (!pressProtocol_->isValidCommand(command_id, numb)) {
        qDebug() << "Invalid command parameters - command_id:" << command_id << "numb:" << numb;
        return;
    }

    const qint64 current_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    qDebug() << "last" << last_sent_timestamp_ << "current" << current_timestamp;
    if (current_timestamp - last_sent_timestamp_ < 100 && last_sent_timestamp_ != 0) {
        qDebug() << "short time,current_time - last:" << current_timestamp - last_sent_timestamp_;
        delayMsec(100);
    }
    last_sent_timestamp_ = QDateTime::currentDateTime().toMSecsSinceEpoch();
    last_commid_timestamp_ = last_sent_timestamp_;

    QByteArray dataToSend;
    if (!pressProtocol_->commandBytes(command_id, numb, &dataToSend)) {
        qDebug() << "Invalid command index";
        return;
    }
    qDebug() << "command_id:" << command_id << "numb" << numb;
    writeFixturePort(dataToSend, true, false);
    qDebug() << "已发送命令" << dataToSend;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
