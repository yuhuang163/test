#include "fixture_uart.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>

#include "box_base.h"
#include "qdebug.h"
#include "qfreework.h"
#include "qlog.h"
#include "serial_channel.h"
#include "test_case_paths.h"
#include "test_case_store.h"
#include "ui_fixture_uart.h"
#include "qfixturemanager.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Fixture_uart::Fixture_uart(QWidget* parent) : QWidget(parent), ui(new Ui::Fixture_uart), fixtureManager_(new QFixtureManager(this)) {
    isConfigLoading_ = true;
    // 串口解析在 QtConcurrent 线程 emit，跨线程 QueuedConnection 须注册自定义类型
    qRegisterMetaType<FixturePacketData>("FixturePacketData");
    ui->setupUi(this);
    ui->FixturecomNameCombo->clear();

    ui->plcDeviceCombo->addItem(QStringLiteral("串口通信 (Modbus RTU)"), QStringLiteral("XinjiePlcRtu"));
    ui->plcDeviceCombo->addItem(QStringLiteral("网口通信 (Modbus TCP)"), QStringLiteral("InovanceH5uTcp"));

    ui->plcBaudRateCombo->clear();
    ui->plcBaudRateCombo->addItems({QStringLiteral("19200"), QStringLiteral("115200"), QStringLiteral("9600"), QStringLiteral("38400"), QStringLiteral("57600")});

    scanSerialPorts();
    scanSerialPortsTimer_ = new QTimer(this);
    connect(scanSerialPortsTimer_, &QTimer::timeout, this, &Fixture_uart::scanSerialPorts);
    scanSerialPortsTimer_->start(1000);

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

    loadPreStartMonitorConfig();
    isConfigLoading_ = false;
}

Fixture_uart::~Fixture_uart() {
    savePreStartMonitorConfig();
    delete ui;
}

void Fixture_uart::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    loadPreStartMonitorConfig();
}

void Fixture_uart::hideEvent(QHideEvent* event) {
    savePreStartMonitorConfig();
    QWidget::hideEvent(event);
}

void Fixture_uart::closeEvent(QCloseEvent* event) {
    savePreStartMonitorConfig();
    QWidget::closeEvent(event);
}

void Fixture_uart::reloadStationConfig() {
    loadPreStartMonitorConfig();
}

QString Fixture_uart::currentStationKey() const {
    QString stationKey = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (stationKey.isEmpty() || TestCaseStore::loadStationFlowItems(stationKey).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationName());
        if (!byName.isEmpty())
            stationKey = byName;
    }
    if (stationKey.isEmpty())
        stationKey = QStringLiteral("default");
    return stationKey;
}

void Fixture_uart::updateWindowTitleWithStation() {
    const QString key = currentStationKey();
    const QString name = TestCaseStore::flowStationDisplayName(key);
    const QString stationLabel = name.isEmpty() ? key : name;
    setWindowTitle(QStringLiteral("治具串口与自动扫码配置 - %1").arg(stationLabel));
}

void Fixture_uart::updateDeviceFieldsVisibility() {
    const QString dev = ui->plcDeviceCombo->currentData().toString();
    const bool isSerial = (dev != QLatin1String("InovanceH5uTcp"));

    ui->plcComPortLabel->setVisible(isSerial);
    ui->plcComPortCombo->setVisible(isSerial);
    ui->plcBaudRateLabel->setVisible(isSerial);
    ui->plcBaudRateCombo->setVisible(isSerial);

    ui->plcIpLabel->setVisible(!isSerial);
    ui->plcIpLineEdit->setVisible(!isSerial);
    ui->plcPortLabel->setVisible(!isSerial);
    ui->plcPortSpinBox->setVisible(!isSerial);
}

void Fixture_uart::loadPreStartMonitorConfig() {
    isConfigLoading_ = true;

    const QString stationKey = currentStationKey();
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
    updateWindowTitleWithStation();

    ui->plcDeviceCombo->blockSignals(true);
    ui->plcComPortCombo->blockSignals(true);
    ui->plcBaudRateCombo->blockSignals(true);
    ui->plcIpLineEdit->blockSignals(true);
    ui->plcPortSpinBox->blockSignals(true);
    ui->plcWaitAddressLineEdit->blockSignals(true);
    ui->scannerIpLineEdit->blockSignals(true);
    ui->scannerPortSpinBox->blockSignals(true);

    QSettings settings(flowPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("PreStart_Monitor"));

    const QString dev = settings.value(QStringLiteral("PlcDevice"), QStringLiteral("XinjiePlcRtu")).toString();
    int idx = ui->plcDeviceCombo->findData(dev);
    ui->plcDeviceCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    const QString comPort = settings.value(QStringLiteral("PlcComPort")).toString();
    ui->plcComPortCombo->setEditText(comPort);
    if (ui->plcComPortCombo->findText(comPort) >= 0) {
        ui->plcComPortCombo->setCurrentText(comPort);
    }

    const QString baud = settings.value(QStringLiteral("PlcBaudRate"), QStringLiteral("19200")).toString();
    ui->plcBaudRateCombo->setEditText(baud);
    if (ui->plcBaudRateCombo->findText(baud) >= 0) {
        ui->plcBaudRateCombo->setCurrentText(baud);
    }

    ui->plcIpLineEdit->setText(settings.value(QStringLiteral("PlcIp"), QStringLiteral("127.0.0.1")).toString());
    ui->plcPortSpinBox->setValue(settings.value(QStringLiteral("PlcPort"), 502).toInt());

    // 默认线圈为 M5
    const QString waitAddr = settings.value(QStringLiteral("PlcWaitAddress"),
        settings.value(QStringLiteral("PlcWaitAddressM"), QStringLiteral("M5"))).toString();
    ui->plcWaitAddressLineEdit->setText(waitAddr.isEmpty() ? QStringLiteral("M5") : waitAddr);

    // 扫码枪 IP 与端口（默认 192.168.1.64 : 2001）
    const QString scannerIp = settings.value(QStringLiteral("ScannerIp"), QStringLiteral("192.168.1.64")).toString().trimmed();
    ui->scannerIpLineEdit->setText(scannerIp.isEmpty() ? QStringLiteral("192.168.1.64") : scannerIp);
    ui->scannerPortSpinBox->setValue(settings.value(QStringLiteral("ScannerPort"), 2001).toInt());
    const bool missingScannerIp = !settings.contains(QStringLiteral("ScannerIp"));
    settings.endGroup();

    ui->plcDeviceCombo->blockSignals(false);
    ui->plcComPortCombo->blockSignals(false);
    ui->plcBaudRateCombo->blockSignals(false);
    ui->plcIpLineEdit->blockSignals(false);
    ui->plcPortSpinBox->blockSignals(false);
    ui->plcWaitAddressLineEdit->blockSignals(false);
    ui->scannerIpLineEdit->blockSignals(false);
    ui->scannerPortSpinBox->blockSignals(false);

    updateDeviceFieldsVisibility();
    isConfigLoading_ = false;

    if (missingScannerIp && !flowPath.isEmpty()) {
        savePreStartMonitorConfig();
    }
}

void Fixture_uart::savePreStartMonitorConfig() {
    if (isConfigLoading_)
        return;

    const QString stationKey = currentStationKey();
    if (stationKey.isEmpty())
        return;

    TestCaseStore::ensureProfileDirectory(stationKey, TestCaseStore::flowStationDisplayName(stationKey), QString());
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);

    QSettings settings(flowPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("PreStart_Monitor"));
    if (!settings.contains(QStringLiteral("Enabled"))) {
        settings.setValue(QStringLiteral("Enabled"), true);
    }
    settings.setValue(QStringLiteral("PlcDevice"), ui->plcDeviceCombo->currentData().toString());
    settings.setValue(QStringLiteral("PlcComPort"), ui->plcComPortCombo->currentText().trimmed());
    settings.setValue(QStringLiteral("PlcBaudRate"), ui->plcBaudRateCombo->currentText().toInt());
    settings.setValue(QStringLiteral("PlcIp"), ui->plcIpLineEdit->text().trimmed());
    settings.setValue(QStringLiteral("PlcPort"), ui->plcPortSpinBox->value());
    const QString waitAddr = ui->plcWaitAddressLineEdit->text().trimmed();
    settings.setValue(QStringLiteral("PlcWaitAddress"), waitAddr);
    bool okNum = false;
    int addrNum = waitAddr.startsWith(QLatin1Char('M'), Qt::CaseInsensitive) ? waitAddr.mid(1).toInt(&okNum) : waitAddr.toInt(&okNum);
    if (okNum) {
        settings.setValue(QStringLiteral("PlcWaitAddressM"), addrNum);
    }
    settings.setValue(QStringLiteral("ScannerIp"), ui->scannerIpLineEdit->text().trimmed());
    settings.setValue(QStringLiteral("ScannerPort"), ui->scannerPortSpinBox->value());
    settings.endGroup();
    settings.sync();

    // 通知所有打开的自由工站即时生效新配置
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* box = qobject_cast<box_base*>(w)) {
            for (test_base* t : box->testList) {
                if (auto* fw = qobject_cast<QFreeWork*>(t)) {
                    fw->updatePreStartMonitorState();
                }
            }
        }
    }
}

void Fixture_uart::on_plcDeviceCombo_currentIndexChanged(int) {
    updateDeviceFieldsVisibility();
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcIpLineEdit_textChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcIpLineEdit_editingFinished() {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcPortSpinBox_valueChanged(int) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_scannerIpLineEdit_textChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_scannerIpLineEdit_editingFinished() {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_scannerPortSpinBox_valueChanged(int) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcComPortCombo_currentTextChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcBaudRateCombo_currentTextChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcWaitAddressLineEdit_textChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_plcWaitAddressLineEdit_editingFinished() {
    savePreStartMonitorConfig();
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

void Fixture_uart::scanSerialPorts() {
    QElapsedTimer timer;
    timer.start();
    {
        QSignalBlocker blocker(ui->FixturecomNameCombo);
        SerialChannel::updateComboBoxPorts(ui->FixturecomNameCombo);
    }
    {
        QSignalBlocker blocker(ui->plcComPortCombo);
        SerialChannel::updateComboBoxPorts(ui->plcComPortCombo);
    }
    Qlog::saveResidentLog(QStringLiteral("scanPorts"),
                          QStringLiteral("fixture cost=%1ms").arg(timer.elapsed()));
}

void Fixture_uart::onManagerConnected() {
    ui->Fixtureuartstate->setText("治具串口连接：<font color='green'>成功</font>");
    ui->FixturecomNameCombo->setEnabled(false);
    ui->FixtureconnectButton->setEnabled(false);
}

void Fixture_uart::onManagerDisconnected() {
    ui->FixturecomNameCombo->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);
    ui->Fixtureuartstate->setText("治具串口连接：<font color='red'>失败</font>");
}

void Fixture_uart::onManagerError(int error, const QString& message) {
    qDebug() << "串口问题" << message;
    if (error == QSerialPort::PermissionError) {
        fixtureManager_->close();
        ui->FixturecomNameCombo->setEnabled(true);
        ui->FixtureconnectButton->setEnabled(true);
        QMessageBox::warning(NULL, "警告", " 治具串口被拔出！\t\r\n");
        ui->Fixtureuartstate->setText("串口连接：<font color='red'>拔出</font>");
    }
}

bool Fixture_uart::isFixtureSerialOpen() const {
    return fixtureManager_->isOpen();
}

bool Fixture_uart::tryOpenSerialPort(const QString& portName, bool autoConnect) {
    Q_UNUSED(autoConnect);
    return fixtureManager_->open(portName, fixBaudRate);
}

void Fixture_uart::closeSerialPort() {
    fixtureManager_->close();
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
