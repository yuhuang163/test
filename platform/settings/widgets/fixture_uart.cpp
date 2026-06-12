#include "fixture_uart.h"

#include <QMessageBox>

#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_fixture_uart.h"
#include "qfixturemanager.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Fixture_uart::Fixture_uart(QWidget* parent) : QWidget(parent), ui(new Ui::Fixture_uart), fixtureManager_(new QFixtureManager(this)) {
    // 串口解析在 QtConcurrent 线程 emit，跨线程 QueuedConnection 须注册自定义类型
    qRegisterMetaType<FixturePacketData>("FixturePacketData");
    ui->setupUi(this);
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }

    connect(fixtureManager_, &QFixtureManager::connected, this, &Fixture_uart::onManagerConnected);
    connect(fixtureManager_, &QFixtureManager::disconnected, this, &Fixture_uart::onManagerDisconnected);
    connect(fixtureManager_, &QFixtureManager::errorOccurred, this, [this](QSerialPort::SerialPortError error, const QString& message) {
        onManagerError(static_cast<int>(error), message);
    });

    // Forward signals
    connect(fixtureManager_, &QFixtureManager::send_data_to_mechine, this, &Fixture_uart::send_data_to_mechine);
    connect(fixtureManager_, &QFixtureManager::send_data_to_mechine_imu, this, &Fixture_uart::send_data_to_mechine_imu);
    connect(fixtureManager_, &QFixtureManager::send_data_to_mechine_sleep, this, &Fixture_uart::send_data_to_mechine_sleep);
    connect(fixtureManager_, &QFixtureManager::send_data_to_mechine_start, this, &Fixture_uart::send_data_to_mechine_start);
    connect(fixtureManager_, &QFixtureManager::start_fix_action, this, &Fixture_uart::start_fix_action);
    connect(fixtureManager_, &QFixtureManager::send_data_to_mechine_press, this, &Fixture_uart::send_data_to_mechine_press);
}

Fixture_uart::~Fixture_uart() {
    delete ui;
}

void Fixture_uart::on_FixtureconnectButton_clicked() {
    const QString portName = ui->FixturecomNameCombo->currentText();
    if (fixtureManager_->open(portName, fixBaudRate)) {
        // Success handled by connected signal
    } else {
        QMessageBox::warning(NULL, "警告", " 串口被占用或无法打开！\t\r\n");
    }
}

void Fixture_uart::on_FixturedisconnectButton_clicked() {
    fixtureManager_->close();
}

void Fixture_uart::on_FixturerefreshCom_clicked() {
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }
}

void Fixture_uart::onManagerConnected() {
    ui->Fixtureuartstate->setText("治具串口连接：<font color='green'>成功</font>");
    ui->FixturerefreshCom->setEnabled(false);
    ui->FixturecomNameCombo->setEnabled(false);
    ui->FixtureconnectButton->setEnabled(false);
}

void Fixture_uart::onManagerDisconnected() {
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixturecomNameCombo->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);
    ui->Fixtureuartstate->setText("治具串口连接：<font color='red'>失败</font>");
}

void Fixture_uart::onManagerError(int error, const QString& message) {
    qDebug() << "串口问题" << message;
    if (error == QSerialPort::PermissionError) {
        fixtureManager_->close();
        ui->FixturerefreshCom->setEnabled(true);
        ui->FixturecomNameCombo->setEnabled(true);
        ui->FixtureconnectButton->setEnabled(true);
        QMessageBox::warning(NULL, "警告", " 治具串口被拔出！\t\r\n");
        ui->Fixtureuartstate->setText("串口连接：<font color='red'>拔出</font>");
    }
}

bool Fixture_uart::isFixtureSerialOpen() const {
    return fixtureManager_->isOpen();
}

void Fixture_uart::sendPcbaFrame(const QByteArray& frame) {
    fixtureManager_->sendPcbaFrame(frame);
}

void Fixture_uart::sendimuData(imuFixtureState fixstate) {
    fixtureManager_->sendimuData(fixstate);
}

void Fixture_uart::set_camera_action(camreaFixtureState fixstate) {
    fixtureManager_->set_camera_action(fixstate);
}

void Fixture_uart::sendFixtureData(FixtureState fixstate) {
    fixtureManager_->sendFixtureData(fixstate);
}

void Fixture_uart::send_start_command(int i) {
    fixtureManager_->send_start_command(i);
}

void Fixture_uart::send_start_sleep_command(int i) {
    fixtureManager_->send_start_sleep_command(i);
}

void Fixture_uart::send_start_white_modle_command(int i) {
    fixtureManager_->send_start_white_modle_command(i);
}

void Fixture_uart::send_command_to_machine(int command_id, int numb) {
    fixtureManager_->send_command_to_machine(command_id, numb);
}

void Fixture_uart::delay_msec(unsigned int msec) {
    fixtureManager_->delayMsec(msec);
}

qint64 Fixture_uart::lastCommidTimestamp() const {
    return fixtureManager_->lastCommidTimestamp();
}

void Fixture_uart::setLastCommidTimestamp(qint64 timestamp) {
    fixtureManager_->setLastCommidTimestamp(timestamp);
}

machine_command_id_e Fixture_uart::lastCommid() const {
    return fixtureManager_->lastCommid();
}

void Fixture_uart::setLastCommid(machine_command_id_e commandId) {
    fixtureManager_->setLastCommid(commandId);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
