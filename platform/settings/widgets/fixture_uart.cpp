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

Fixture_uart::Fixture_uart(QWidget* parent, FixtureUartRole role)
    : QWidget(parent), ui(new Ui::Fixture_uart), fixtureManager_(new QFixtureManager(this)),
      plcSharedPort_(role == FixtureUartRole::PlcSharedPort), role_(role) {
    isConfigLoading_ = true;
    // 串口解析在 QtConcurrent 线程 emit，跨线程 QueuedConnection 须注册自定义类型
    qRegisterMetaType<FixturePacketData>("FixturePacketData");
    ui->setupUi(this);
    // 普通治具弹窗不暴露波特率；PLC 治具串口弹窗允许现场改波特率
    const bool showFixtureBaud = role_ == FixtureUartRole::PlcSharedPort;
    ui->fixtureBaudLabel->setVisible(showFixtureBaud);
    ui->FixtureBaudRateCombo->setVisible(showFixtureBaud);
    ui->plcBaudRateLabel->setVisible(false);
    ui->plcBaudRateCombo->setVisible(false);
    ui->FixturecomNameCombo->clear();

    ui->FixtureBaudRateCombo->clear();
    ui->FixtureBaudRateCombo->addItems({QStringLiteral("9600"), QStringLiteral("115200"), QStringLiteral("19200"),
                                       QStringLiteral("38400"), QStringLiteral("57600"), QStringLiteral("460800")});
    connect(ui->FixtureBaudRateCombo, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        bool ok = false;
        int b = text.toInt(&ok);
        if (ok && b > 0 && role_ != FixtureUartRole::PreStartMonitor) {
            fixBaudRate = b;
            saveFixturePortConfig();
        }
    });

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

    loadStationPageConfig();
    isConfigLoading_ = false;
}

Fixture_uart::~Fixture_uart() {
    if (role_ == FixtureUartRole::PreStartMonitor)
        savePreStartMonitorConfig();
    else
        saveFixturePortConfig();
    delete ui;
}

void Fixture_uart::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    loadStationPageConfig();
}

void Fixture_uart::hideEvent(QHideEvent* event) {
    if (role_ == FixtureUartRole::PreStartMonitor)
        savePreStartMonitorConfig();
    else
        saveFixturePortConfig();
    QWidget::hideEvent(event);
}

void Fixture_uart::closeEvent(QCloseEvent* event) {
    if (role_ == FixtureUartRole::PreStartMonitor)
        savePreStartMonitorConfig();
    else
        saveFixturePortConfig();
    QWidget::closeEvent(event);
}

void Fixture_uart::reloadStationConfig() {
    loadStationPageConfig();
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
    if (role_ == FixtureUartRole::PlcSharedPort)
        setWindowTitle(QStringLiteral("PLC一拖多治具串口配置 - %1").arg(stationLabel));
    else if (role_ == FixtureUartRole::PreStartMonitor)
        setWindowTitle(QStringLiteral("自动扫码与PLC线圈监控 - %1").arg(stationLabel));
    else
        setWindowTitle(QStringLiteral("治具串口配置 - %1").arg(stationLabel));
}

void Fixture_uart::applyRolePageVisibility() {
    const bool monitorRole = (role_ == FixtureUartRole::PreStartMonitor);
    ui->fixtureUartGroup->setVisible(!monitorRole);
    if (ui->preStartMonitorEnabledCheckBox)
        ui->preStartMonitorEnabledCheckBox->setVisible(monitorRole);
    if (ui->preStartMonitorHintLabel)
        ui->preStartMonitorHintLabel->setVisible(monitorRole);
    const bool enabled = monitorRole && ui->preStartMonitorEnabledCheckBox
                         && ui->preStartMonitorEnabledCheckBox->isChecked();
    ui->preStartMonitorGroup->setVisible(enabled);
    if (!monitorRole && plcSharedPort_)
        ui->fixtureUartGroup->setTitle(QStringLiteral("PLC 一拖多治具串口"));
    adjustSize();
}

void Fixture_uart::applyPreStartMonitorFormEnabled(bool enabled) {
    if (role_ != FixtureUartRole::PreStartMonitor)
        return;
    ui->preStartMonitorGroup->setVisible(enabled);
    if (ui->preStartMonitorHintLabel) {
        ui->preStartMonitorHintLabel->setText(
            enabled ? QStringLiteral("已启用：修改后自动保存到当前工站 flow.ini。")
                    : QStringLiteral("未启用时不写入扫码枪/线圈明细；勾选启用后可配置并保存。"));
    }
    adjustSize();
}

void Fixture_uart::updateDeviceFieldsVisibility() {
    const QString dev = ui->plcDeviceCombo->currentData().toString();
    const bool isSerial = (dev != QLatin1String("InovanceH5uTcp"));

    ui->plcComPortLabel->setVisible(isSerial);
    ui->plcComPortCombo->setVisible(isSerial);

    ui->plcIpLabel->setVisible(!isSerial);
    ui->plcIpLineEdit->setVisible(!isSerial);
    ui->plcPortLabel->setVisible(!isSerial);
    ui->plcPortSpinBox->setVisible(!isSerial);
}

void Fixture_uart::loadStationPageConfig() {
    isConfigLoading_ = true;

    const QString stationKey = currentStationKey();
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
    updateWindowTitleWithStation();

    ui->FixturecomNameCombo->blockSignals(true);
    if (ui->preStartMonitorEnabledCheckBox)
        ui->preStartMonitorEnabledCheckBox->blockSignals(true);
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

    auto unblockAll = [this]() {
        ui->FixturecomNameCombo->blockSignals(false);
        if (ui->preStartMonitorEnabledCheckBox)
            ui->preStartMonitorEnabledCheckBox->blockSignals(false);
        ui->plcDeviceCombo->blockSignals(false);
        ui->plcComPortCombo->blockSignals(false);
        ui->plcBaudRateCombo->blockSignals(false);
        ui->plcIpLineEdit->blockSignals(false);
        ui->plcPortSpinBox->blockSignals(false);
        ui->plcWaitAddressLineEdit->blockSignals(false);
        ui->scannerIpLineEdit->blockSignals(false);
        ui->scannerPortSpinBox->blockSignals(false);
    };

    if (role_ != FixtureUartRole::PreStartMonitor) {
        const QString fixturePortKey =
            plcSharedPort_ ? QStringLiteral("PlcFixtureComPort") : QStringLiteral("FixtureComPort");
        QString fixtureCom = settings.value(fixturePortKey).toString().trimmed();
        if (fixtureCom.isEmpty() && !plcSharedPort_) {
            fixtureCom = SETTINGS.value(QStringLiteral("mechine/0/masterFixturecomName")).toString().trimmed();
        }
        if (!fixtureCom.isEmpty()) {
            ui->FixturecomNameCombo->setEditText(fixtureCom);
            if (ui->FixturecomNameCombo->findText(fixtureCom) >= 0)
                ui->FixturecomNameCombo->setCurrentText(fixtureCom);
        }

        int savedFixtureBaud = 9600;
        if (plcSharedPort_) {
            const TestCaseSerialUiConfig serialUi = TestCaseStore::loadStationSerialUiConfig(stationKey);
            savedFixtureBaud = serialUi.plcFixtureBaudRate > 0
                                   ? serialUi.plcFixtureBaudRate
                                   : settings.value(QStringLiteral("PlcFixtureBaudRate"), 9600).toInt();
        } else {
            const TestCaseSerialUiConfig serialUi = TestCaseStore::loadStationSerialUiConfig(stationKey);
            savedFixtureBaud = serialUi.fixtureBaudRate > 0 ? serialUi.fixtureBaudRate
                                                            : SETTINGS.value(QStringLiteral("mechine/0/masterFixtureBaudRate"), 9600)
                                                                  .toInt();
        }
        fixBaudRate = savedFixtureBaud > 0 ? savedFixtureBaud : 9600;
        ui->FixtureBaudRateCombo->setCurrentText(QString::number(fixBaudRate));
        settings.endGroup();
        unblockAll();
        applyRolePageVisibility();
        isConfigLoading_ = false;
        return;
    }

    const bool enabled = settings.value(QStringLiteral("Enabled")).toBool();
    if (ui->preStartMonitorEnabledCheckBox)
        ui->preStartMonitorEnabledCheckBox->setChecked(enabled);

    const QString dev = settings.value(QStringLiteral("PlcDevice"), QStringLiteral("XinjiePlcRtu")).toString();
    const int idx = ui->plcDeviceCombo->findData(dev);
    ui->plcDeviceCombo->setCurrentIndex(idx >= 0 ? idx : 0);

    const QString comPort = settings.value(QStringLiteral("PlcComPort")).toString().trimmed();
    ui->plcComPortCombo->setEditText(comPort);
    if (ui->plcComPortCombo->findText(comPort) >= 0)
        ui->plcComPortCombo->setCurrentText(comPort);

    const QString baud = settings.value(QStringLiteral("PlcBaudRate"), QStringLiteral("19200")).toString();
    ui->plcBaudRateCombo->setEditText(baud);
    if (ui->plcBaudRateCombo->findText(baud) >= 0)
        ui->plcBaudRateCombo->setCurrentText(baud);

    ui->plcIpLineEdit->setText(settings.value(QStringLiteral("PlcIp")).toString());
    ui->plcPortSpinBox->setValue(settings.value(QStringLiteral("PlcPort"), 502).toInt());
    ui->plcWaitAddressLineEdit->setText(settings.value(QStringLiteral("PlcWaitAddress"),
                                                       settings.value(QStringLiteral("PlcWaitAddressM")))
                                            .toString());
    ui->scannerIpLineEdit->setText(settings.value(QStringLiteral("ScannerIp")).toString().trimmed());
    ui->scannerPortSpinBox->setValue(settings.value(QStringLiteral("ScannerPort"), 2001).toInt());
    settings.endGroup();
    unblockAll();
    applyRolePageVisibility();
    applyPreStartMonitorFormEnabled(enabled);
    updateDeviceFieldsVisibility();
    isConfigLoading_ = false;
}

void Fixture_uart::saveFixturePortConfig() {
    if (isConfigLoading_ || role_ == FixtureUartRole::PreStartMonitor)
        return;

    const QString stationKey = currentStationKey();
    if (stationKey.isEmpty())
        return;

    const QString fixturePort = ui->FixturecomNameCombo->currentText().trimmed();

    if (plcSharedPort_) {
        TestCaseStore::ensureProfileDirectory(stationKey, TestCaseStore::flowStationDisplayName(stationKey), QString());
        const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
        QSettings settings(flowPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup(QStringLiteral("PreStart_Monitor"));
        settings.setValue(QStringLiteral("PlcFixtureComPort"), fixturePort);
        settings.setValue(QStringLiteral("PlcFixtureBaudRate"), fixBaudRate);
        settings.endGroup();
        settings.beginGroup(QStringLiteral("SerialUi"));
        settings.setValue(QStringLiteral("PlcFixtureBaudRate"), fixBaudRate);
        settings.endGroup();
        settings.sync();
        // PLC 治具串口只存本工站 flow.ini，不写 上位机设置.ini 全局项
        return;
    }

    TestCaseStore::ensureProfileDirectory(stationKey, TestCaseStore::flowStationDisplayName(stationKey), QString());
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
    if (!flowPath.isEmpty()) {
        QSettings flowSettings(flowPath, QSettings::IniFormat);
        flowSettings.setIniCodec("UTF-8");
        flowSettings.beginGroup(QStringLiteral("SerialUi"));
        flowSettings.setValue(QStringLiteral("FixtureBaudRate"), fixBaudRate);
        flowSettings.endGroup();
        QSettings monitorSettings(flowPath, QSettings::IniFormat);
        monitorSettings.setIniCodec("UTF-8");
        monitorSettings.beginGroup(QStringLiteral("PreStart_Monitor"));
        // 工站未启用自动扫码/PLC监控时，不创建、不回写 PreStart_Monitor
        if (monitorSettings.value(QStringLiteral("Enabled")).toBool())
            monitorSettings.setValue(QStringLiteral("FixtureComPort"), fixturePort);
        monitorSettings.endGroup();
        flowSettings.sync();
    }

    if (!fixturePort.isEmpty())
        SETTINGS.setValue(QStringLiteral("mechine/0/masterFixturecomName"), fixturePort);
    SETTINGS.setValue(QStringLiteral("mechine/0/masterFixtureBaudRate"), fixBaudRate);
    SETTINGS.sync();
}

void Fixture_uart::savePreStartMonitorConfig() {
    if (isConfigLoading_ || role_ != FixtureUartRole::PreStartMonitor)
        return;

    const QString stationKey = currentStationKey();
    if (stationKey.isEmpty())
        return;

    const bool enabled = ui->preStartMonitorEnabledCheckBox && ui->preStartMonitorEnabledCheckBox->isChecked();
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);

    // 未启用且工站从未写过监控配置：不落盘，避免无关工站出现空 PreStart_Monitor
    if (!enabled) {
        if (flowPath.isEmpty() || !QFile::exists(flowPath))
            return;
        QSettings probe(flowPath, QSettings::IniFormat);
        probe.setIniCodec("UTF-8");
        probe.beginGroup(QStringLiteral("PreStart_Monitor"));
        const bool hadConfig = probe.contains(QStringLiteral("Enabled")) || probe.contains(QStringLiteral("ScannerIp"))
                               || probe.contains(QStringLiteral("PlcWaitAddress"))
                               || probe.contains(QStringLiteral("PlcDevice"))
                               || probe.contains(QStringLiteral("PlcComPort"));
        if (!hadConfig) {
            probe.endGroup();
            return;
        }
        probe.setValue(QStringLiteral("Enabled"), false);
        probe.endGroup();
        probe.sync();
    } else {
        TestCaseStore::ensureProfileDirectory(stationKey, TestCaseStore::flowStationDisplayName(stationKey), QString());
        if (flowPath.isEmpty())
            return;
        QSettings settings(flowPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup(QStringLiteral("PreStart_Monitor"));
        settings.setValue(QStringLiteral("Enabled"), true);
        settings.setValue(QStringLiteral("PlcDevice"), ui->plcDeviceCombo->currentData().toString());
        settings.setValue(QStringLiteral("PlcComPort"), ui->plcComPortCombo->currentText().trimmed());
        settings.setValue(QStringLiteral("PlcBaudRate"), ui->plcBaudRateCombo->currentText().toInt());
        settings.setValue(QStringLiteral("PlcIp"), ui->plcIpLineEdit->text().trimmed());
        settings.setValue(QStringLiteral("PlcPort"), ui->plcPortSpinBox->value());
        const QString waitAddr = ui->plcWaitAddressLineEdit->text().trimmed();
        settings.setValue(QStringLiteral("PlcWaitAddress"), waitAddr);
        bool okNum = false;
        const int addrNum = waitAddr.startsWith(QLatin1Char('M'), Qt::CaseInsensitive) ? waitAddr.mid(1).toInt(&okNum)
                                                                                      : waitAddr.toInt(&okNum);
        if (okNum)
            settings.setValue(QStringLiteral("PlcWaitAddressM"), addrNum);
        settings.setValue(QStringLiteral("ScannerIp"), ui->scannerIpLineEdit->text().trimmed());
        settings.setValue(QStringLiteral("ScannerPort"), ui->scannerPortSpinBox->value());
        settings.endGroup();
        settings.sync();
    }

    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* box = qobject_cast<box_base*>(w)) {
            for (test_base* t : box->testList) {
                if (auto* fw = qobject_cast<QFreeWork*>(t))
                    fw->updatePreStartMonitorState();
            }
        }
    }
}

void Fixture_uart::on_FixturecomNameCombo_currentTextChanged(const QString&) {
    saveFixturePortConfig();
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

void Fixture_uart::on_preStartMonitorEnabledCheckBox_toggled(bool checked) {
    applyPreStartMonitorFormEnabled(checked);
    if (checked)
        updateDeviceFieldsVisibility();
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

void Fixture_uart::setFixBaudRate(int baudRate) {
    if (baudRate <= 0)
        return;
    fixBaudRate = baudRate;
    const QString text = QString::number(baudRate);
    if (ui->FixtureBaudRateCombo->findText(text) < 0)
        ui->FixtureBaudRateCombo->addItem(text);
    {
        QSignalBlocker blocker(ui->FixtureBaudRateCombo);
        ui->FixtureBaudRateCombo->setCurrentText(text);
    }
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

bool Fixture_uart::sendRawFrame(const QByteArray& frame, QString* errorMessage) {
    return fixtureManager_->sendRawFrame(frame, errorMessage);
}

bool Fixture_uart::exchangeRawFrame(const QByteArray& request, QByteArray* response, int timeoutMs,
                                    QString* errorMessage) {
    return fixtureManager_->exchangeRawFrame(request, response, timeoutMs, errorMessage);
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
