#include "fixture_uart.h"

#include <QApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QMessageBox>
#include <QSettings>
#include <QTabBar>
#include <QTimer>

#include "box_base.h"
#include "qdebug.h"
#include "qfreework.h"
#include "qfreeworkbox.h"
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
    if (ui->xwdPlcFixtureBaudCombo) {
        ui->xwdPlcFixtureBaudCombo->clear();
        ui->xwdPlcFixtureBaudCombo->addItems({QStringLiteral("115200"), QStringLiteral("9600"), QStringLiteral("19200"),
                                             QStringLiteral("38400"), QStringLiteral("57600")});
    }
    if (ui->serialScannerBaudCombo) {
        ui->serialScannerBaudCombo->clear();
        ui->serialScannerBaudCombo->addItems({QStringLiteral("9600"), QStringLiteral("115200"), QStringLiteral("19200"),
                                             QStringLiteral("38400"), QStringLiteral("57600")});
    }
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
        setWindowTitle(QStringLiteral("PLC治具串口配置 - %1").arg(stationLabel));
    else if (role_ == FixtureUartRole::PreStartMonitor)
        setWindowTitle(QStringLiteral("自动扫码配置 - %1").arg(stationLabel));
    else
        setWindowTitle(QStringLiteral("治具串口配置 - %1").arg(stationLabel));
}

void Fixture_uart::applyRolePageVisibility() {
    const bool monitorRole = (role_ == FixtureUartRole::PreStartMonitor);
    ui->fixtureUartGroup->setVisible(!monitorRole);
    if (ui->autoScanTabWidget)
        ui->autoScanTabWidget->setVisible(monitorRole);
    if (!monitorRole && plcSharedPort_)
        ui->fixtureUartGroup->setTitle(QStringLiteral("PLC 治具串口（流程 UP/DOWN）"));
    if (monitorRole) {
        // 双页签都显示：PLC按键扫码 + 本工站 XWD 到位扫码（含监控治具口）
        if (ui->xwdPlcFixtureGroup)
            ui->xwdPlcFixtureGroup->setVisible(true);
        if (ui->autoScanTabWidget) {
            const int plcIdx = ui->autoScanTabWidget->indexOf(ui->plcScanTab);
            const int xwdIdx = ui->autoScanTabWidget->indexOf(ui->xwdScanTab);
            if (plcIdx >= 0)
                ui->autoScanTabWidget->setTabVisible(plcIdx, true);
            if (xwdIdx >= 0)
                ui->autoScanTabWidget->setTabVisible(xwdIdx, true);
            ui->autoScanTabWidget->tabBar()->setVisible(true);
        }
        const bool plcEnabled = ui->preStartMonitorEnabledCheckBox
                                && ui->preStartMonitorEnabledCheckBox->isChecked();
        const bool xwdEnabled = ui->xwdDownScanEnabledCheckBox
                                && ui->xwdDownScanEnabledCheckBox->isChecked();
        applyPreStartMonitorFormEnabled(plcEnabled);
        applyXwdDownScanFormEnabled(xwdEnabled);
    }
    adjustSize();
}

void Fixture_uart::applyPreStartMonitorFormEnabled(bool enabled) {
    if (role_ != FixtureUartRole::PreStartMonitor)
        return;
    // 表单常显，便于先填再勾选；Enabled 只控制是否开监控
    if (ui->preStartMonitorGroup)
        ui->preStartMonitorGroup->setVisible(true);
    if (ui->preStartMonitorHintLabel) {
        ui->preStartMonitorHintLabel->setText(
            enabled ? QStringLiteral("已启用：修改后自动保存到当前工站 flow.ini（PreStart_Monitor）。")
                    : QStringLiteral("未启用监控；下方参数仍会写入本工站 flow.ini，勾选后生效。"));
    }
}

void Fixture_uart::applyXwdDownScanFormEnabled(bool enabled) {
    if (role_ != FixtureUartRole::PreStartMonitor)
        return;
    if (ui->serialScannerGroup)
        ui->serialScannerGroup->setVisible(true);
    if (ui->xwdDownScanHintLabel) {
        ui->xwdDownScanHintLabel->setText(
            enabled ? QStringLiteral("已启用：轮询 XWD 共享口 DOWN?（与 SET_ALL_UP 同设备同通道）。")
                    : QStringLiteral("未启用。监控口可留空则用「连接PLC治具串口」；同一台设备时勿配成两个不同 COM。"));
    }
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
    if (ui->xwdDownScanEnabledCheckBox)
        ui->xwdDownScanEnabledCheckBox->blockSignals(true);
    ui->plcDeviceCombo->blockSignals(true);
    ui->plcComPortCombo->blockSignals(true);
    ui->plcBaudRateCombo->blockSignals(true);
    ui->plcIpLineEdit->blockSignals(true);
    ui->plcPortSpinBox->blockSignals(true);
    ui->plcWaitAddressLineEdit->blockSignals(true);
    ui->scannerIpLineEdit->blockSignals(true);
    ui->scannerPortSpinBox->blockSignals(true);
    if (ui->xwdPlcFixtureComCombo)
        ui->xwdPlcFixtureComCombo->blockSignals(true);
    if (ui->xwdPlcFixtureBaudCombo)
        ui->xwdPlcFixtureBaudCombo->blockSignals(true);
    if (ui->serialScannerComCombo)
        ui->serialScannerComCombo->blockSignals(true);
    if (ui->serialScannerBaudCombo)
        ui->serialScannerBaudCombo->blockSignals(true);
    if (ui->serialScannerHexEdit)
        ui->serialScannerHexEdit->blockSignals(true);
    if (ui->serialScannerTimeoutSpin)
        ui->serialScannerTimeoutSpin->blockSignals(true);

    QSettings settings(flowPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("PreStart_Monitor"));

    auto unblockAll = [this]() {
        ui->FixturecomNameCombo->blockSignals(false);
        if (ui->preStartMonitorEnabledCheckBox)
            ui->preStartMonitorEnabledCheckBox->blockSignals(false);
        if (ui->xwdDownScanEnabledCheckBox)
            ui->xwdDownScanEnabledCheckBox->blockSignals(false);
        ui->plcDeviceCombo->blockSignals(false);
        ui->plcComPortCombo->blockSignals(false);
        ui->plcBaudRateCombo->blockSignals(false);
        ui->plcIpLineEdit->blockSignals(false);
        ui->plcPortSpinBox->blockSignals(false);
        ui->plcWaitAddressLineEdit->blockSignals(false);
        ui->scannerIpLineEdit->blockSignals(false);
        ui->scannerPortSpinBox->blockSignals(false);
        if (ui->xwdPlcFixtureComCombo)
            ui->xwdPlcFixtureComCombo->blockSignals(false);
        if (ui->xwdPlcFixtureBaudCombo)
            ui->xwdPlcFixtureBaudCombo->blockSignals(false);
        if (ui->serialScannerComCombo)
            ui->serialScannerComCombo->blockSignals(false);
        if (ui->serialScannerBaudCombo)
            ui->serialScannerBaudCombo->blockSignals(false);
        if (ui->serialScannerHexEdit)
            ui->serialScannerHexEdit->blockSignals(false);
        if (ui->serialScannerTimeoutSpin)
            ui->serialScannerTimeoutSpin->blockSignals(false);
    };

    if (role_ != FixtureUartRole::PreStartMonitor) {
        const QString fixturePortKey =
            plcSharedPort_ ? QStringLiteral("PlcFixtureComPort") : QStringLiteral("FixtureComPort");
        QString fixtureCom = settings.value(fixturePortKey).toString().trimmed();
        if (fixtureCom.isEmpty() && plcSharedPort_) {
            // 同一台 XWD：流程口空时回退监控口
            fixtureCom = settings.value(QStringLiteral("XwdMonitorComPort")).toString().trimmed();
        }
        if (fixtureCom.isEmpty() && !plcSharedPort_) {
            fixtureCom = SETTINGS.value(QStringLiteral("mechine/0/masterFixturecomName")).toString().trimmed();
        }
        if (!fixtureCom.isEmpty()) {
            // 口此刻未枚举到也要写入下拉并选中，否则重启后界面空白且 hide 时可能用空串覆盖 ini
            if (ui->FixturecomNameCombo->findText(fixtureCom) < 0)
                ui->FixturecomNameCombo->addItem(fixtureCom);
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

    // 本工站 XWD 到位扫码 + 一拖多治具串口
    const bool xwdEnabled = settings.value(QStringLiteral("XwdDownScanEnabled")).toBool();
    if (ui->xwdDownScanEnabledCheckBox)
        ui->xwdDownScanEnabledCheckBox->setChecked(xwdEnabled);

    const TestCaseSerialUiConfig serialUi = TestCaseStore::loadStationSerialUiConfig(stationKey);
    // 自动扫码监控口（与流程 PlcFixtureComPort 独立）
    QString monitorCom = settings.value(QStringLiteral("XwdMonitorComPort")).toString().trimmed();
    if (monitorCom.isEmpty())
        monitorCom = settings.value(QStringLiteral("PlcFixtureComPort")).toString().trimmed();
    int monitorBaud = settings.value(QStringLiteral("XwdMonitorBaudRate"), 0).toInt();
    if (monitorBaud <= 0) {
        monitorBaud = serialUi.plcFixtureBaudRate > 0
                          ? serialUi.plcFixtureBaudRate
                          : settings.value(QStringLiteral("PlcFixtureBaudRate"), 115200).toInt();
    }
    if (monitorBaud <= 0)
        monitorBaud = 115200;
    if (ui->xwdPlcFixtureComCombo) {
        ui->xwdPlcFixtureComCombo->setEditText(monitorCom);
        if (ui->xwdPlcFixtureComCombo->findText(monitorCom) >= 0)
            ui->xwdPlcFixtureComCombo->setCurrentText(monitorCom);
    }
    if (ui->xwdPlcFixtureBaudCombo)
        ui->xwdPlcFixtureBaudCombo->setCurrentText(QString::number(monitorBaud));

    const QString scannerCom = settings.value(QStringLiteral("ScannerComPort")).toString().trimmed();
    if (ui->serialScannerComCombo) {
        ui->serialScannerComCombo->setEditText(scannerCom);
        if (ui->serialScannerComCombo->findText(scannerCom) >= 0)
            ui->serialScannerComCombo->setCurrentText(scannerCom);
    }
    const int scannerBaud = settings.value(QStringLiteral("ScannerBaudRate"), 9600).toInt();
    if (ui->serialScannerBaudCombo)
        ui->serialScannerBaudCombo->setCurrentText(QString::number(scannerBaud > 0 ? scannerBaud : 9600));
    if (ui->serialScannerHexEdit)
        ui->serialScannerHexEdit->setText(settings.value(QStringLiteral("ScannerHexCmd")).toString().trimmed());
    if (ui->serialScannerTimeoutSpin)
        ui->serialScannerTimeoutSpin->setValue(
            settings.value(QStringLiteral("ScannerTimeoutMs"), 3000).toInt());

    settings.endGroup();
    unblockAll();
    applyRolePageVisibility();
    applyPreStartMonitorFormEnabled(enabled);
    applyXwdDownScanFormEnabled(xwdEnabled);
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

    TestCaseStore::ensureProfileDirectory(stationKey, TestCaseStore::flowStationDisplayName(stationKey), QString());
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
    if (!flowPath.isEmpty()) {
        // 同一份 flow.ini 只能开一个 QSettings：两个对象会共享缓存，后一个在 setIniCodec 前就会把中文当 Latin-1 写乱
        QSettings settings(flowPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        if (plcSharedPort_) {
            // 空串不落盘：避免扫口未就绪 / 关闭窗口时把已保存的 PlcFixtureComPort 冲掉
            if (fixturePort.isEmpty()) {
                settings.sync();
                return;
            }
            settings.beginGroup(QStringLiteral("PreStart_Monitor"));
            settings.setValue(QStringLiteral("PlcFixtureComPort"), fixturePort);
            settings.setValue(QStringLiteral("PlcFixtureBaudRate"), fixBaudRate);
            // 同一台 XWD：监控口未单独配置时同步，避免只配了流程口却扫不了 DOWN?
            if (settings.value(QStringLiteral("XwdMonitorComPort")).toString().trimmed().isEmpty()) {
                settings.setValue(QStringLiteral("XwdMonitorComPort"), fixturePort);
                settings.setValue(QStringLiteral("XwdMonitorBaudRate"), fixBaudRate);
            }
            settings.endGroup();
            settings.beginGroup(QStringLiteral("SerialUi"));
            settings.setValue(QStringLiteral("PlcFixtureBaudRate"), fixBaudRate);
            settings.endGroup();
        } else {
            settings.beginGroup(QStringLiteral("SerialUi"));
            settings.setValue(QStringLiteral("FixtureBaudRate"), fixBaudRate);
            settings.endGroup();
            settings.beginGroup(QStringLiteral("PreStart_Monitor"));
            // 工站未启用自动扫码/PLC监控时，不创建、不回写 PreStart_Monitor
            if (settings.value(QStringLiteral("Enabled")).toBool())
                settings.setValue(QStringLiteral("FixtureComPort"), fixturePort);
            settings.endGroup();
        }
        settings.beginGroup(QStringLiteral("SerialUi"));
        const QString jigLabel = settings.value(QStringLiteral("JigLabel")).toString();
        const QString productLabel = settings.value(QStringLiteral("ProductLabel")).toString();
        const QString usbLabel = settings.value(QStringLiteral("UsbLabel")).toString();
        const auto labelBroken = [](const QString& text) {
            return text.contains(QChar(0x00C3)) || text.contains(QChar(0x00E6));
        };
        if (labelBroken(jigLabel))
            settings.setValue(QStringLiteral("JigLabel"), QStringLiteral("治具串口"));
        if (labelBroken(productLabel))
            settings.setValue(QStringLiteral("ProductLabel"), QStringLiteral("产品串口(仪器)"));
        if (labelBroken(usbLabel))
            settings.setValue(QStringLiteral("UsbLabel"), QStringLiteral("万用表串口"));
        settings.endGroup();
        settings.sync();
        // PLC 治具串口只存本工站 flow.ini，不写 上位机设置.ini 全局项
        return;
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

    TestCaseStore::ensureProfileDirectory(stationKey, TestCaseStore::flowStationDisplayName(stationKey), QString());
    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
    if (flowPath.isEmpty())
        return;

    const bool enabled = ui->preStartMonitorEnabledCheckBox && ui->preStartMonitorEnabledCheckBox->isChecked();
    const bool xwdEnabled = ui->xwdDownScanEnabledCheckBox && ui->xwdDownScanEnabledCheckBox->isChecked();

    QSettings settings(flowPath, QSettings::IniFormat);
    settings.setIniCodec("UTF-8");
    settings.beginGroup(QStringLiteral("PreStart_Monitor"));

    // 开关与明细始终落盘（避免「未勾选就不写」导致下次打开空白）
    settings.setValue(QStringLiteral("Enabled"), enabled);
    settings.setValue(QStringLiteral("XwdDownScanEnabled"), xwdEnabled);

    settings.setValue(QStringLiteral("PlcDevice"),
                      ui->plcDeviceCombo ? ui->plcDeviceCombo->currentData().toString()
                                         : QStringLiteral("XinjiePlcRtu"));
    settings.setValue(QStringLiteral("PlcComPort"),
                      ui->plcComPortCombo ? ui->plcComPortCombo->currentText().trimmed() : QString());
    settings.setValue(QStringLiteral("PlcBaudRate"),
                      ui->plcBaudRateCombo ? ui->plcBaudRateCombo->currentText().toInt() : 19200);
    settings.setValue(QStringLiteral("PlcIp"),
                      ui->plcIpLineEdit ? ui->plcIpLineEdit->text().trimmed() : QString());
    settings.setValue(QStringLiteral("PlcPort"), ui->plcPortSpinBox ? ui->plcPortSpinBox->value() : 502);
    const QString waitAddr =
        ui->plcWaitAddressLineEdit ? ui->plcWaitAddressLineEdit->text().trimmed() : QString();
    settings.setValue(QStringLiteral("PlcWaitAddress"), waitAddr);
    bool okNum = false;
    const int addrNum = waitAddr.startsWith(QLatin1Char('M'), Qt::CaseInsensitive) ? waitAddr.mid(1).toInt(&okNum)
                                                                                    : waitAddr.toInt(&okNum);
    if (okNum)
        settings.setValue(QStringLiteral("PlcWaitAddressM"), addrNum);
    settings.setValue(QStringLiteral("ScannerIp"),
                      ui->scannerIpLineEdit ? ui->scannerIpLineEdit->text().trimmed() : QString());
    settings.setValue(QStringLiteral("ScannerPort"),
                      ui->scannerPortSpinBox ? ui->scannerPortSpinBox->value() : 2001);

    settings.setValue(QStringLiteral("ScannerComPort"),
                      ui->serialScannerComCombo ? ui->serialScannerComCombo->currentText().trimmed()
                                               : QString());
    int scannerBaud = 9600;
    if (ui->serialScannerBaudCombo) {
        bool ok = false;
        const int b = ui->serialScannerBaudCombo->currentText().toInt(&ok);
        if (ok && b > 0)
            scannerBaud = b;
    }
    settings.setValue(QStringLiteral("ScannerBaudRate"), scannerBaud);
    settings.setValue(QStringLiteral("ScannerHexCmd"),
                      ui->serialScannerHexEdit ? ui->serialScannerHexEdit->text().trimmed() : QString());
    settings.setValue(QStringLiteral("ScannerTimeoutMs"),
                      ui->serialScannerTimeoutSpin ? ui->serialScannerTimeoutSpin->value() : 3000);
    settings.setValue(QStringLiteral("AutoIncrementComByStation"), true);

    // 仅写自动扫码监控口，绝不覆盖「连接PLC治具串口」的 PlcFixtureComPort
    const QString monitorCom =
        ui->xwdPlcFixtureComCombo ? ui->xwdPlcFixtureComCombo->currentText().trimmed() : QString();
    int monitorBaud = 115200;
    if (ui->xwdPlcFixtureBaudCombo) {
        bool ok = false;
        const int b = ui->xwdPlcFixtureBaudCombo->currentText().toInt(&ok);
        if (ok && b > 0)
            monitorBaud = b;
    }
    settings.setValue(QStringLiteral("XwdMonitorComPort"), monitorCom);
    settings.setValue(QStringLiteral("XwdMonitorBaudRate"), monitorBaud);
    // 同一台 XWD：流程口未配时用监控口补上，SET_ALL_UP 与 DOWN? 监控都能通
    if (settings.value(QStringLiteral("PlcFixtureComPort")).toString().trimmed().isEmpty()
        && !monitorCom.isEmpty()) {
        settings.setValue(QStringLiteral("PlcFixtureComPort"), monitorCom);
        settings.setValue(QStringLiteral("PlcFixtureBaudRate"), monitorBaud);
    }
    settings.endGroup();
    settings.sync();

    qDebug().noquote() << QStringLiteral("[扫码配置] 已保存 %1 Enabled=%2 XwdDownScan=%3 监控口=%4@%5")
                              .arg(flowPath)
                              .arg(enabled)
                              .arg(xwdEnabled)
                              .arg(monitorCom.isEmpty() ? QStringLiteral("(空)") : monitorCom)
                              .arg(monitorBaud);

    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* box = qobject_cast<QFreeWorkBox*>(w)) {
            box->applyFixtureMenuLabel();
            for (test_base* t : box->testList) {
                if (auto* fw = qobject_cast<QFreeWork*>(t))
                    fw->updatePreStartMonitorState();
            }
        } else if (auto* base = qobject_cast<box_base*>(w)) {
            for (test_base* t : base->testList) {
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

void Fixture_uart::on_xwdDownScanEnabledCheckBox_toggled(bool checked) {
    applyXwdDownScanFormEnabled(checked);
    savePreStartMonitorConfig();
}

void Fixture_uart::on_xwdPlcFixtureComCombo_currentTextChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_xwdPlcFixtureBaudCombo_currentTextChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_serialScannerComCombo_currentTextChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_serialScannerBaudCombo_currentTextChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_serialScannerHexEdit_textChanged(const QString&) {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_serialScannerHexEdit_editingFinished() {
    savePreStartMonitorConfig();
}

void Fixture_uart::on_serialScannerTimeoutSpin_valueChanged(int) {
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
    if (ui->xwdPlcFixtureComCombo) {
        QSignalBlocker blocker(ui->xwdPlcFixtureComCombo);
        SerialChannel::updateComboBoxPorts(ui->xwdPlcFixtureComCombo);
    }
    if (ui->serialScannerComCombo) {
        QSignalBlocker blocker(ui->serialScannerComCombo);
        SerialChannel::updateComboBoxPorts(ui->serialScannerComCombo);
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

QString Fixture_uart::currentFixtureComName() const {
    if (!ui || !ui->FixturecomNameCombo)
        return QString();
    return ui->FixturecomNameCombo->currentText().trimmed();
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
