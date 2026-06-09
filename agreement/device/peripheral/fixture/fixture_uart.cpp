#include "fixture_uart.h"

#include <QDateTime>
#include <QEventLoop>
#include <QMessageBox>

#include "fixture_camera_uart_protocol.h"
#include "fixture_imu_uart_protocol.h"
#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_fixture_uart.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Fixture_uart::Fixture_uart(QWidget* parent) : QWidget(parent), ui(new Ui::Fixture_uart), fixtureSerialPort(new QSerialPort(this)) {
    // 串口解析在 QtConcurrent 线程 emit，跨线程 QueuedConnection 须注册自定义类型
    qRegisterMetaType<FixturePacketData>("FixturePacketData");
    ui->setupUi(this);
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }

    connect(this->fixtureSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(FixturehandleError(QSerialPort::SerialPortError)));

    connect(fixtureSerialPort, &QSerialPort::readyRead, this, [=]() {
        fixtureSerialPortTimer->start(10);
        fixtureSerialPortBuf.append(fixtureSerialPort->readAll());
    });

    fixRingBuf = new RingBuf(&p_fixRingBuffer, fix_ring_buffer, 1, sizeof(fix_ring_buffer));
    pcbaProtocol_ = new FixturePcbaUartProtocol(fixRingBuf, &p_fixRingBuffer, frame_buf, sizeof(frame_buf));

    future = QtConcurrent::run([this]() {
        while (running.load()) {
            solve_frame();
            QThread::msleep(10);
        }
    });
    running.store(true);
    FixtureCommandInit();
}

Fixture_uart::~Fixture_uart() {
    running.store(false);
    future.waitForFinished();
    delete pcbaProtocol_;
    delete ui;
}

void Fixture_uart::closeFixtureSerialPort() {
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixturecomNameCombo->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);

    refresh_Fixtureuart_state(0);

    if (fixtureSerialPort->isOpen()) {
        fixtureSerialPort->close();
    }
    disconnect(fixtureSerialPortTimer, &QTimer::timeout, this, &Fixture_uart::readFixtureSerialPortData);
}

void Fixture_uart::refresh_Fixtureuart_state(int state) {
    if (state) {
        ui->Fixtureuartstate->setText("治具串口连接：<font color='green'>成功</font>");
    } else {
        ui->Fixtureuartstate->setText("治具串口连接：<font color='red'>失败</font>");
    }
}

void Fixture_uart::on_FixtureconnectButton_clicked() {
    openFixtureSerialPort();
}

void Fixture_uart::on_FixturedisconnectButton_clicked() {
    closeFixtureSerialPort();
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);
}

void Fixture_uart::on_FixturerefreshCom_clicked() {
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }
}

void Fixture_uart::openFixtureSerialPort(void) {
    fixtureSerialPort->setPortName(ui->FixturecomNameCombo->currentText());
    fixtureSerialPort->setBaudRate(fixBaudRate);
    fixtureSerialPort->setDataBits(QSerialPort::Data8);
    fixtureSerialPort->setParity(QSerialPort::NoParity);
    fixtureSerialPort->setStopBits(QSerialPort::OneStop);
    fixtureSerialPort->setReadBufferSize(4096);
    fixtureSerialPort->setFlowControl(QSerialPort::NoFlowControl);
    if (fixtureSerialPort->open(QIODevice::ReadWrite)) {
        fixtureSerialPort->setRequestToSend(true);
        fixtureSerialPort->setDataTerminalReady(true);
        refresh_Fixtureuart_state(1);
        ui->FixturerefreshCom->setEnabled(false);
        ui->FixturecomNameCombo->setEnabled(false);
        ui->FixtureconnectButton->setEnabled(false);
        fixtureSerialPort->setDataTerminalReady(true);

        connect(fixtureSerialPortTimer, &QTimer::timeout, this, &Fixture_uart::readFixtureSerialPortData);
    } else {
        QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
    }
}

bool Fixture_uart::isFixtureSerialOpen() const {
    return fixtureSerialPort && fixtureSerialPort->isOpen();
}

void Fixture_uart::sendPcbaFrame(const QByteArray& frame) {
    if (!isFixtureSerialOpen()) {
        qDebug() << "治具串口未打开，无法发送 PCBA 帧";
        return;
    }
    writeFixturePort(frame, true, false);
}

void Fixture_uart::writeFixturePort(const QByteArray& data, bool logTx, bool startAction) {
    if (data.isEmpty()) {
        return;
    }
    qDebug().noquote() << "FIXTURE TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    fixtureSerialPort->write(data);
    if (logTx) {
        log_.save_fixture_uart_log(1, data);
    }
    if (startAction) {
        start_fix_action(1);
    }
}

void Fixture_uart::readFixtureSerialPortData() {
    fixtureSerialPortTimer->stop();
    const QByteArray dataTemp = fixtureSerialPortBuf;
    fixtureSerialPortBuf.clear();

    qDebug().noquote() << "FIXTURE RX:" << QString::fromLatin1(dataTemp.toHex(' ').toUpper());
    qDebug() << "hyj接收到治具数据" << dataTemp;

    const int writeLen = fixRingBuf->usmile_ring_buffer_write(
        &p_fixRingBuffer, reinterpret_cast<uint8_t*>(const_cast<char*>(dataTemp.data())), dataTemp.size());
    if (writeLen < dataTemp.size()) {
        qDebug() << "write_len:" << writeLen << "len:" << dataTemp.size();
    }
    qDebug() << "开始处理 << dataTemp.size()";

    dispatchTextProtocols(dataTemp);
    log_.save_fixture_uart_log(0, dataTemp);
}

void Fixture_uart::dispatchTextProtocols(const QByteArray& data) {
    const FixtureImuUartEvent imuEv = FixtureImuUartProtocol::parseReceived(data);
    if (imuEv.ok) {
        emit send_data_to_mechine_imu(1);
    }
    if (imuEv.error) {
        emit send_data_to_mechine_imu(0);
    }

    const FixturePressUartEvent pressEv = FixturePressUartProtocol::parseReceived(data);
#if Y20_Pro_PRESS_TEST
    if (pressEv.requestCheckStatus) {
        send_command_to_machine(COMMAND_ID_CHEAK_STATUS, 0);
    }
#endif
    if (pressEv.pressNotifyState > 0) {
        emit send_data_to_mechine_press(pressEv.pressNotifyState);
    }
}

void Fixture_uart::solve_frame(void) {
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

void Fixture_uart::sendimuData(imuFixtureState fixstate) {
    if (!fixtureSerialPort->isOpen()) {
        qDebug() << "带页面的治具串口未打开，无法发送数据";
        return;
    }
    const QByteArray dataToSend = FixtureImuUartProtocol::buildCommand(fixstate);
    writeFixturePort(dataToSend, true, true);
}

void Fixture_uart::set_camera_action(camreaFixtureState fixstate) {
    if (!fixtureSerialPort->isOpen()) {
        qDebug() << "带页面的治具串口未打开，无法发送数据";
        return;
    }
    const QByteArray dataToSend = FixtureCameraUartProtocol::buildCommand(fixstate);
    writeFixturePort(dataToSend, true, true);
}

void Fixture_uart::sendFixtureData(FixtureState fixstate) {
    if (!fixtureSerialPort->isOpen()) {
        QMessageBox::warning(NULL, "警告", " 未打开治具串口\t\r\n  无法发送数据！\r\n");
        return;
    }
    const QByteArray dataToSend = FixturePressUartProtocol::buildFixtureStateCommand(fixstate);
    writeFixturePort(dataToSend, false, false);
}

void Fixture_uart::send_start_command(int i) {
    const QByteArray dataToSend = FixturePcbaUartProtocol::buildStartTestCommand(i);
    if (!dataToSend.isEmpty()) {
        writeFixturePort(dataToSend, true, false);
        qDebug() << "已发送开始命令" << i;
    } else {
        qDebug() << "Invalid command index";
    }
}

void Fixture_uart::send_start_sleep_command(int i) {
    const QByteArray dataToSend = FixturePcbaUartProtocol::buildSleepCommand(i);
    if (!dataToSend.isEmpty()) {
        writeFixturePort(dataToSend, true, false);
        qDebug() << "已发送开始休眠命令" << i;
    } else {
        qDebug() << "Invalid command index";
    }
}

void Fixture_uart::send_start_white_modle_command(int i) {
    const QByteArray dataToSend = FixturePcbaUartProtocol::buildWhiteModeCommand(i);
    if (!dataToSend.isEmpty()) {
        writeFixturePort(dataToSend, true, false);
        qDebug() << "已发送已经进入亮白模式命令" << i;
    } else {
        qDebug() << "Invalid command index";
    }
}

void Fixture_uart::FixturehandleError(QSerialPort::SerialPortError error) {
    qDebug() << "串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        if (fixtureSerialPort->isOpen()) {
            fixtureSerialPort->close();
        }

        disconnect(fixtureSerialPortTimer, &QTimer::timeout, this, &Fixture_uart::readFixtureSerialPortData);

        ui->FixturerefreshCom->setEnabled(true);
        ui->FixturecomNameCombo->setEnabled(true);
        ui->FixtureconnectButton->setEnabled(true);
        QMessageBox::warning(NULL, "警告", " 治具串口被拔出！\t\r\n");
        ui->Fixtureuartstate->setText("串口连接：<font color='red'>拔出</font>");
    }
}

void Fixture_uart::FixtureCommandInit(void) {
    pressProtocol_.initCommands();
}

void Fixture_uart::delay_msec(unsigned int msec) {
    QEventLoop loop;
    QTimer::singleShot(msec, &loop, SLOT(quit()));
    loop.exec();
}

void Fixture_uart::send_command_to_machine(int command_id, int numb) {
    if (!fixtureSerialPort->isOpen()) {
        QMessageBox::warning(NULL, "警告", " 未打开串口\t\r\n  无法发送数据！\r\n");
        return;
    }
    if (!pressProtocol_.isValidCommand(command_id, numb)) {
        qDebug() << "Invalid command parameters - command_id:" << command_id << "numb:" << numb;
        return;
    }

    const qint64 current_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    qDebug() << "last" << last_sent_timestamp << "current" << current_timestamp;
    if (current_timestamp - last_sent_timestamp < 100 && last_sent_timestamp != 0) {
        qDebug() << "short time,current_time - last:" << current_timestamp - last_sent_timestamp;
        delay_msec(100);
    }
    last_sent_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    last_commid_timestamp = last_sent_timestamp;

    QByteArray dataToSend;
    if (!pressProtocol_.commandBytes(command_id, numb, &dataToSend)) {
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
