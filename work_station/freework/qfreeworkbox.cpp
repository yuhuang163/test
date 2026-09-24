#include "qfreeworkbox.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QPixmap>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>

#include "Abini.h"
#include "application_shutdown.h"
#include "asd9026a_device.h"
#include "my_typedef.h"
#include "qfixturemanager.h"
#include "qfreework.h"
#include "screen_inspect_analyzer.h"
#include "shared_instrument.h"
#include "test_case_paths.h"
#include "test_case_store.h"
#include "ui_qfreeworkbox.h"

#include "ui_qfreework.h"
#include <QDateTime>

static QIcon freeWorkFwIcon(const char* name, int px = 16) {
    QPixmap pm(QStringLiteral(":/stytle/picture/fw/%1.png").arg(QLatin1String(name)));
    if (pm.isNull())
        return {};
    return QIcon(pm.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

static void applyFreeWorkMenuIcons(QMenuBar* bar, QAction* fixtureAct, QAction* plcAct) {
    if (!bar)
        return;
    for (QAction* act : bar->actions()) {
        if (!act)
            continue;
        const QString text = act->text();
        if (text == QStringLiteral("功能设置"))
            act->setIcon(freeWorkFwIcon("icon-gear"));
        else if (text.contains(QStringLiteral("开始测试")))
            act->setIcon(freeWorkFwIcon("icon-play"));
        else if (text.contains(QStringLiteral("自动扫码")))
            act->setIcon(freeWorkFwIcon("icon-monitor"));
        else if (act == fixtureAct || (text.contains(QStringLiteral("治具串口")) && !text.contains(QStringLiteral("PLC"))))
            act->setIcon(freeWorkFwIcon("icon-fixture"));
        else if (act == plcAct || text.contains(QStringLiteral("PLC")))
            act->setIcon(freeWorkFwIcon("icon-plc"));
        else if (act->menu() && text.contains(QStringLiteral("帮助")))
            act->setIcon(freeWorkFwIcon("icon-help"));
    }
    if (fixtureAct)
        fixtureAct->setIcon(freeWorkFwIcon("icon-fixture"));
    if (plcAct)
        plcAct->setIcon(freeWorkFwIcon("icon-plc"));
}

QFreeWorkBox::QFreeWorkBox(QWidget* parent) : box_base(parent), ui(new Ui::QFreeWorkBox) {
    ui->setupUi(this);
    applyWidgetStyleSheet(this, QStringLiteral("console_shell.qss"));
    asd9026aDevice_ = new Asd9026aDevice(this);
    plcMonitorTimer_ = new QTimer(this);
    connect(plcMonitorTimer_, &QTimer::timeout, this, &QFreeWorkBox::onPlcMonitorTimeout);
    CreatWindow<QFreeWork>(this);
    signalAndslot();
    recoverCustom();
    // 扫口异步完成后可能冲掉选中项，稍后再恢复一次已保存串口
    QTimer::singleShot(800, this, [this]() { recoverCustom(); });
    ShowData(this);
    setWindowTitle("自由工站");
    // Keep live status labels (including account updates), but align them left.
    const auto statusLabels = ui->statusbar->findChildren<QLabel*>(QString(), Qt::FindDirectChildrenOnly);
    for (QLabel* label : statusLabels) {
        ui->statusbar->removeWidget(label);
        ui->statusbar->addWidget(label);
        label->show();
    }
    ui->statusbar->addWidget(new QLabel(QStringLiteral("自由测试工站"), ui->statusbar));
    ui->statusbar->setSizeGripEnabled(false);
    // 启动时清一次历史屏幕检测图（仅根目录测试抓拍；不碰「参考图」子目录）
    {
        const QString inspectRoot =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
        QDir().mkpath(QDir(inspectRoot).filePath(QStringLiteral("参考图")));
        ScreenInspectAnalyzer::cleanupStoredImages(inspectRoot);
    }
    const QString version = QString::fromUtf8(FREE_VER).simplified().section(QLatin1Char(' '), -1);
    ui->statusbar->addPermanentWidget(new QLabel(version + QStringLiteral("   |   ")
                                                + QString(__DATE__) + QStringLiteral(" ") + QString(__TIME__)));

    fixtureConnectAct_ = ui->menubar->addAction(QStringLiteral("连接治具串口"));
    connect(fixtureConnectAct_, &QAction::triggered, this, [this]() { ensureFixtureUartWindow(true); });

    plcFixtureConnectAct_ = ui->menubar->addAction(QStringLiteral("连接PLC治具串口"));
    connect(plcFixtureConnectAct_, &QAction::triggered, this, [this]() { ensurePlcFixtureUartWindow(true); });

    QAction* preStartMonitorAct = ui->menubar->addAction("自动扫码与PLC线圈监控");
    connect(preStartMonitorAct, &QAction::triggered, this, [this]() {
        if (preStartMonitorUi_ == nullptr)
            preStartMonitorUi_ = new Fixture_uart(nullptr, FixtureUartRole::PreStartMonitor);
        else
            preStartMonitorUi_->reloadStationConfig();
        preStartMonitorUi_->raise();
        preStartMonitorUi_->show();
        preStartMonitorUi_->activateWindow();
    });

    QAction* startTest_act = ui->menubar->addAction("开始测试");
    connect(startTest_act, &QAction::triggered, this, &QFreeWorkBox::startTest);

    applyFixtureMenuLabel();
    applyFreeWorkMenuIcons(ui->menubar, fixtureConnectAct_, plcFixtureConnectAct_);

    // Windows QMenuBar draws an action's icon instead of its text. A toolbar
    // explicitly supports both, while sharing the original actions and menus.
    auto* toolbar = new QToolBar(this);
    toolbar->setObjectName(QStringLiteral("freeWorkToolbar"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(18, 18));
    QAction* helpAction = nullptr;
    for (QAction* action : ui->menubar->actions()) {
        if (action->menu() && action->text().contains(QStringLiteral("帮助")))
            helpAction = action;
        else
            toolbar->addAction(action);
    }
    if (helpAction) {
        toolbar->addAction(helpAction);
        if (auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(helpAction)))
            button->setPopupMode(QToolButton::InstantPopup);
    }
    addToolBar(Qt::TopToolBarArea, toolbar);
    ui->menubar->hide();

    // 延时1秒初始化全局 PLC 监控（等待子工位和 flow.ini 加载就绪）
    QTimer::singleShot(1000, this, [this]() { updatePlcMonitorState(); });
}

void QFreeWorkBox::applyFixtureMenuLabel() {
    QString stationKey = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (stationKey.isEmpty())
        stationKey = QStringLiteral("default");
    const TestCaseSerialUiConfig cfg = TestCaseStore::loadStationSerialUiConfig(stationKey);
    if (fixtureConnectAct_) {
        const QString label = cfg.fixtureMenuLabel.trimmed();
        fixtureConnectAct_->setText(label.isEmpty() ? QStringLiteral("连接治具串口") : label);
    }
    if (plcFixtureConnectAct_) {
        const QString plcLabel = cfg.plcFixtureMenuLabel.trimmed();
        plcFixtureConnectAct_->setText(plcLabel.isEmpty() ? QStringLiteral("连接PLC治具串口") : plcLabel);
    }

    const int fixtureBaud = cfg.fixtureBaudRate > 0 ? cfg.fixtureBaudRate : 9600;
    ensureFixtureUartWindow(false);
    if (Fixture_uart_ui && Fixture_uart_ui->fixBaudRate != fixtureBaud) {
        const bool wasOpen = Fixture_uart_ui->isFixtureSerialOpen();
        const QString port = wasOpen && Fixture_uart_ui->fixtureManager()
                                 ? Fixture_uart_ui->fixtureManager()->portName()
                                 : QString();
        Fixture_uart_ui->setFixBaudRate(fixtureBaud);
        if (wasOpen && !port.isEmpty() && Fixture_uart_ui->fixtureManager()) {
            if (!Fixture_uart_ui->fixtureManager()->applyBaudRate(fixtureBaud)) {
                Fixture_uart_ui->closeSerialPort();
                Fixture_uart_ui->tryOpenSerialPort(port, true);
            }
        }
    }

    const int plcFixtureBaud = cfg.plcFixtureBaudRate > 0 ? cfg.plcFixtureBaudRate : 9600;
    ensurePlcFixtureUartWindow(false);
    if (plcFixtureUartUi_ && plcFixtureUartUi_->fixBaudRate != plcFixtureBaud) {
        const bool wasOpen = plcFixtureUartUi_->isFixtureSerialOpen();
        const QString port = wasOpen && plcFixtureUartUi_->fixtureManager()
                                 ? plcFixtureUartUi_->fixtureManager()->portName()
                                 : QString();
        plcFixtureUartUi_->setFixBaudRate(plcFixtureBaud);
        if (wasOpen && !port.isEmpty() && plcFixtureUartUi_->fixtureManager()) {
            if (!plcFixtureUartUi_->fixtureManager()->applyBaudRate(plcFixtureBaud)) {
                plcFixtureUartUi_->closeSerialPort();
                plcFixtureUartUi_->tryOpenSerialPort(port, true);
            }
        }
    }
    applyFreeWorkMenuIcons(ui->menubar, fixtureConnectAct_, plcFixtureConnectAct_);
}

QFreeWorkBox::~QFreeWorkBox() {
    qDebug() << "QFreeWorkBox析构";
    if (plcMonitorTimer_) {//兜底，可以留着。
        plcMonitorTimer_->stop();
    }
    releaseSharedPlcIfIdle();
    if (Fixture_uart_ui != nullptr) {
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"), Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
        SETTINGS.sync();
    }
    delete Fixture_uart_ui;
    delete plcFixtureUartUi_;
    Fixture_uart_ui = nullptr;
    plcFixtureUartUi_ = nullptr;
    delete preStartMonitorUi_;
    qDeleteAll(sharedTempLoggerMutexes_);
    sharedTempLoggerMutexes_.clear();
    delete ui;
}

void QFreeWorkBox::abortBlockingWaits() {
    if (plcMonitorTimer_) {
        plcMonitorTimer_->stop();
        plcMonitorRunning_ = false;
    }
    if (Fixture_uart_ui && Fixture_uart_ui->fixtureManager())
        Fixture_uart_ui->fixtureManager()->abortPendingWaits();
}

void QFreeWorkBox::closeEvent(QCloseEvent* event) {
    box_base::closeEvent(event);
}

void QFreeWorkBox::startTest() {
    for (int i = 0; i < testList.size(); i++)
        testList[i]->startTest();
    updatePlcMonitorState();
}

void QFreeWorkBox::checkAllover(int fixtureNumber) {
    box_base::checkAllover(fixtureNumber);
    releaseSharedAsd9026aIfIdle();
    releaseSharedTempLoggerIfIdle();
    updatePlcMonitorState();
}

QString QFreeWorkBox::resolvedFixtureComName(int stationIndex) {
    const auto readPort = [](const QString& key) -> QString {
        return SETTINGS.value(key).toString().trimmed();
    };

    const QString stationKey = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (!stationKey.isEmpty()) {
        const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
        if (QFile::exists(flowPath)) {
            QSettings flowSettings(flowPath, QSettings::IniFormat);
            flowSettings.setIniCodec("UTF-8");
            const QString flowPort = flowSettings.value(QStringLiteral("PreStart_Monitor/FixtureComPort")).toString().trimmed();
            if (!flowPort.isEmpty())
                return flowPort;
        }
    }

    QString port = readPort(QStringLiteral("mechine/%1/masterFixturecomName").arg(stationIndex));
    if (!port.isEmpty())
        return port;
    port = readPort(QStringLiteral("mechine/0/masterFixturecomName"));
    if (!port.isEmpty())
        return port;
    return readPort(QStringLiteral("mechine/masterFixturecomName"));
}

QString QFreeWorkBox::selectedFixtureComName(int stationIndex) const {
    if (Fixture_uart_ui && Fixture_uart_ui->ui) {
        const QString selectedPort = Fixture_uart_ui->ui->FixturecomNameCombo->currentText().trimmed();
        if (!selectedPort.isEmpty())
            return selectedPort;
    }
    return resolvedFixtureComName(stationIndex);
}

QString QFreeWorkBox::resolvedPlcFixtureComName() {
    const QString stationKey = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (!stationKey.isEmpty()) {
        const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
        if (QFile::exists(flowPath)) {
            QSettings flowSettings(flowPath, QSettings::IniFormat);
            flowSettings.setIniCodec("UTF-8");
            const QString flowPort =
                flowSettings.value(QStringLiteral("PreStart_Monitor/PlcFixtureComPort")).toString().trimmed();
            if (!flowPort.isEmpty())
                return flowPort;
        }
    }
    return QString();
}

void QFreeWorkBox::ensureFixtureUartWindow(bool show) {
    if (Fixture_uart_ui == nullptr) {
        Fixture_uart_ui = new Fixture_uart(nullptr, FixtureUartRole::FixturePort);
        Fixture_uart_ui->resize(460, 220);
        connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this, SLOT(startTest()));
    } else {
        Fixture_uart_ui->reloadStationConfig();
    }

    const QString stationKey = Fixture_uart_ui->currentStationKey();
    const QString stationName = TestCaseStore::flowStationDisplayName(stationKey);
    const QString label = stationName.isEmpty() ? stationKey : stationName;
    Fixture_uart_ui->setWindowTitle(
        label.isEmpty() ? QStringLiteral("治具串口配置") : QStringLiteral("治具串口配置 - %1").arg(label));

    if (show) {
        Fixture_uart_ui->raise();
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    }
}

void QFreeWorkBox::ensurePlcFixtureUartWindow(bool show) {
    if (plcFixtureUartUi_ == nullptr) {
        plcFixtureUartUi_ = new Fixture_uart(nullptr, FixtureUartRole::PlcSharedPort);
        plcFixtureUartUi_->resize(460, 220);
    } else {
        plcFixtureUartUi_->reloadStationConfig();
    }

    const QString stationKey = plcFixtureUartUi_->currentStationKey();
    const QString stationName = TestCaseStore::flowStationDisplayName(stationKey);
    const QString label = stationName.isEmpty() ? stationKey : stationName;
    plcFixtureUartUi_->setWindowTitle(
        label.isEmpty() ? QStringLiteral("PLC治具串口配置")
                        : QStringLiteral("PLC治具串口配置 - %1").arg(label));

    if (show) {
        plcFixtureUartUi_->raise();
        plcFixtureUartUi_->show();
        plcFixtureUartUi_->activateWindow();
    }
}

void QFreeWorkBox::releaseSharedAsd9026aIfIdle() {
    if (!asd9026aDevice_ || !asd9026aDevice_->isOpen())
        return;
    for (test_base* station : testList) {
        if (station && station->isTestContinue)
            return;
    }
    const QString port = asd9026aDevice_->portName();
    asd9026aDevice_->close();
    emit sendBoxLog(QStringLiteral("ASD9026A 共享串口已释放：%1").arg(port));
}

SerialChannel* QFreeWorkBox::ensureSharedTempLoggerChannel(int deviceIndex0Based, const QString& portName,
                                                           QString* errorOut, int baudRate,
                                                           SerialChannel::RtsDtrMode rtsMode) {
    const QString port = portName.trimmed();
    if (port.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("共享温度仪串口名为空");
        return nullptr;
    }
    SerialChannel::OpenParams params;
    params.portName = port;
    params.baudRate = baudRate > 0 ? baudRate : 115200;
    params.readBufferSize = 4096;
    params.readDebounceMs = 35;
    params.rtsDtrMode = rtsMode;

    SerialChannel*& channel = sharedTempLoggerChannels_[deviceIndex0Based];
    if (!channel)
        channel = new SerialChannel(this);

    const SerialChannel::OpenParams cached = sharedTempLoggerOpenParams_.value(deviceIndex0Based);
    const bool sameParams = channel->isOpen() && cached.portName.compare(port, Qt::CaseInsensitive) == 0
                            && cached.baudRate == params.baudRate && cached.readDebounceMs == params.readDebounceMs
                            && cached.rtsDtrMode == params.rtsDtrMode;
    if (sameParams)
        return channel;
    if (channel->isOpen())
        channel->close();

    if (!channel->open(params)) {
        if (errorOut)
            *errorOut = QStringLiteral("%1：%2").arg(port, channel->errorString());
        return nullptr;
    }
    sharedTempLoggerOpenParams_.insert(deviceIndex0Based, params);
    emit sendBoxLog(QStringLiteral("温度记录仪共享串口已打开：设备%1 %2 波特率%3 RTS=%4")
                        .arg(deviceIndex0Based)
                        .arg(port)
                        .arg(params.baudRate)
                        .arg(SharedInstrument::tempRtsModeLabel(params.rtsDtrMode)));
    return channel;
}

QMutex* QFreeWorkBox::sharedTempLoggerMutex(int deviceIndex0Based) {
    QMutex*& mutex = sharedTempLoggerMutexes_[deviceIndex0Based];
    if (!mutex)
        mutex = new QMutex();
    return mutex;
}

void QFreeWorkBox::releaseSharedTempLoggerIfIdle() {
    for (test_base* station : testList) {
        if (station && station->isTestContinue)
            return;
    }
    for (auto it = sharedTempLoggerChannels_.begin(); it != sharedTempLoggerChannels_.end(); ++it) {
        SerialChannel* ch = it.value();
        if (!ch || !ch->isOpen())
            continue;
        const QString port = ch->portName();
        ch->close();
        emit sendBoxLog(QStringLiteral("温度记录仪共享串口已释放：设备%1 %2").arg(it.key()).arg(port));
    }
}

void QFreeWorkBox::ensureFixtureUartWidgetCreated() {
    ensureFixtureUartWindow(false);
}

Fixture_uart* QFreeWorkBox::ensureFixtureUartConnected(int stationIndex, QString* detailOut,
                                                       bool* autoConnectedOut) {
    if (autoConnectedOut)
        *autoConnectedOut = false;
    ensureFixtureUartWindow(false);
    if (Fixture_uart_ui->isFixtureSerialOpen()) {
        if (detailOut)
            *detailOut = Fixture_uart_ui->ui->FixturecomNameCombo->currentText().trimmed();
        return Fixture_uart_ui;
    }
    const QString port = resolvedFixtureComName(stationIndex);
    if (port.isEmpty()) {
        if (detailOut)
            *detailOut = QStringLiteral("配置未设置治具串口（mechine/*/masterFixturecomName）");
        return nullptr;
    }
    if (!Fixture_uart_ui->tryOpenSerialPort(port, true)) {
        if (detailOut)
            *detailOut = QStringLiteral("自动连接治具串口失败：%1").arg(port);
        return nullptr;
    }
    if (detailOut)
        *detailOut = port;
    if (autoConnectedOut)
        *autoConnectedOut = true;
    return Fixture_uart_ui;
}

Fixture_uart* QFreeWorkBox::ensurePlcFixtureUartConnected(QString* detailOut) {
    ensurePlcFixtureUartWindow(false);
    if (plcFixtureUartUi_->isFixtureSerialOpen()) {
        if (detailOut)
            *detailOut = plcFixtureUartUi_->ui->FixturecomNameCombo->currentText().trimmed();
        return plcFixtureUartUi_;
    }
    const QString port = resolvedPlcFixtureComName();
    if (port.isEmpty()) {
        if (detailOut)
            *detailOut = QStringLiteral("未配置“PLC治具串口”");
        return nullptr;
    }
    plcFixtureUartUi_->reloadStationConfig();
    if (!plcFixtureUartUi_->tryOpenSerialPort(port, true)) {
        if (detailOut)
            *detailOut = QStringLiteral("自动连接PLC治具串口失败：%1").arg(port);
        return nullptr;
    }
    if (detailOut)
        *detailOut = port;
    return plcFixtureUartUi_;
}

bool QFreeWorkBox::isAnyStationTesting() const {
    for (test_base* station : testList) {
        if (station && station->isTestContinue)
            return true;
    }
    return false;
}

void QFreeWorkBox::releaseSharedPlcIfIdle() {
    if (ApplicationShutdown::isExiting())
        return;
    if (sharedPlcModbusManager_.isPlcConnected()) {
        sharedPlcModbusManager_.exec(XinjePlcCmd::Disconnect, {}, nullptr, nullptr);
    }
}

void QFreeWorkBox::updatePlcMonitorState() {
    sharedPlcConfig_ = PreStartMonitorConfig();
    QString stationKey = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (stationKey.isEmpty() && !testList.isEmpty()) {
        if (auto* fw = qobject_cast<QFreeWork*>(testList.first())) {
            stationKey = fw->activeFlowStationKey();
        }
    }

    const QString flowPath = TestCasePaths::profileFlowPath(stationKey);
    bool isAutoStartChecked = false;
    if (QFile::exists(flowPath)) {
        QSettings settings(flowPath, QSettings::IniFormat);
        settings.setIniCodec("UTF-8");
        settings.beginGroup("PreStart_Monitor");
        isAutoStartChecked = settings.value("IsAutoStartChecked", false).toBool();
        if (settings.contains("Enabled") && settings.value("Enabled").toBool()) {
            sharedPlcConfig_.enabled = true;
            QString plcDev = settings.value(QStringLiteral("PlcDevice")).toString().trimmed();
            if (plcDev.isEmpty()) {
                plcDev = SETTINGS.value(QStringLiteral("PreStart_Monitor/PlcDevice")).toString().trimmed();
            }
            if (plcDev.isEmpty()) {
                plcDev = SETTINGS.value(QStringLiteral("PlcDevice")).toString().trimmed();
            }
            if (plcDev.isEmpty()) {
                const QString comCheck = settings.value(QStringLiteral("PlcComPort"), settings.value(QStringLiteral("ComPort"))).toString().trimmed();
                if (!comCheck.isEmpty() || !SETTINGS.value(QStringLiteral("XINJE_PLC/ComPort")).toString().trimmed().isEmpty()
                    || stationKey.contains(QStringLiteral("半成品"))
                    || stationKey.contains(QStringLiteral("组装"))
                    || stationKey.contains(QStringLiteral("Wellness"), Qt::CaseInsensitive)) {
                    plcDev = QStringLiteral("XinjiePlcRtu");
                } else {
                    plcDev = QStringLiteral("InovanceH5uTcp");
                }
            }
            sharedPlcConfig_.plcDevice = plcDev;
            sharedPlcConfig_.plcIp = settings.value("PlcIp", SETTINGS.value("PlcIp", "127.0.0.1")).toString();
            sharedPlcConfig_.plcPort = settings.value("PlcPort", SETTINGS.value("PlcPort", 502)).toInt();

            QString comPort = settings.value(QStringLiteral("PlcComPort"), settings.value(QStringLiteral("ComPort"), QString())).toString().trimmed();
            if (comPort.isEmpty()) {
                comPort = SETTINGS.value(QStringLiteral("PreStart_Monitor/PlcComPort"), QString()).toString().trimmed();
            }
            if (comPort.isEmpty()) {
                comPort = SETTINGS.value(QStringLiteral("XINJE_PLC/ComPort"), QString()).toString().trimmed();
            }
            if (comPort.isEmpty()) {
                comPort = selectedFixtureComName(0);
            }
            sharedPlcConfig_.plcComPort = comPort;

            sharedPlcConfig_.plcBaudRate = settings.value(QStringLiteral("PlcBaudRate"),
                settings.value(QStringLiteral("BaudRate"),
                SETTINGS.value(QStringLiteral("PreStart_Monitor/PlcBaudRate"),
                SETTINGS.value(QStringLiteral("XINJE_PLC/BaudRate"), 19200)))).toInt();
            sharedPlcConfig_.plcSlaveId = settings.value(QStringLiteral("PlcSlaveId"),
                settings.value(QStringLiteral("SlaveId"),
                SETTINGS.value(QStringLiteral("PreStart_Monitor/PlcSlaveId"),
                SETTINGS.value(QStringLiteral("XINJE_PLC/SlaveId"), 1)))).toInt();

            const QString rawAddr = settings.value("PlcWaitAddress", settings.value("PlcWaitAddressM", "5")).toString().trimmed();
            sharedPlcConfig_.plcWaitAddress = rawAddr.isEmpty() ? QStringLiteral("M5") : (rawAddr.at(0).isDigit() ? QStringLiteral("M") + rawAddr : rawAddr);
            sharedPlcConfig_.plcWaitAddressM = settings.value("PlcWaitAddressM", 5).toInt();
            sharedPlcConfig_.plcPollIntervalMs = qMax(50, settings.value("PlcPollIntervalMs", 500).toInt());
            sharedPlcConfig_.scannerIp = settings.value("ScannerIp", "127.0.0.1").toString();
            sharedPlcConfig_.scannerPort = settings.value("ScannerPort", 2001).toInt();
            sharedPlcConfig_.scannerTimeoutMs = settings.value("ScannerTimeoutMs", 1000).toInt();
            sharedPlcConfig_.autoIncrementIpByStation = settings.value("AutoIncrementIpByStation", true).toBool();
        }
        settings.endGroup();
    }

    // 跨工位同步勾选框显示与选中态
    for (test_base* t : testList) {
        if (auto* fw = qobject_cast<QFreeWork*>(t)) {
            if (fw->ui && fw->ui->autoStartCheckBox) {
                fw->ui->autoStartCheckBox->blockSignals(true);
                fw->ui->autoStartCheckBox->setVisible(sharedPlcConfig_.enabled);
                fw->ui->autoStartCheckBox->setChecked(isAutoStartChecked);
                fw->ui->autoStartCheckBox->blockSignals(false);
            }
        }
    }

    const bool anyTesting = isAnyStationTesting();
    if (sharedPlcConfig_.enabled && !anyTesting && isAutoStartChecked) {
        if (!plcMonitorRunning_) {
            plcMonitorTimer_->start(sharedPlcConfig_.plcPollIntervalMs);
            plcMonitorRunning_ = true;
            const bool isXinjie = sharedPlcConfig_.plcDevice.contains(QLatin1String("Xinjie"), Qt::CaseInsensitive)
                                  || sharedPlcConfig_.plcDevice.contains(QLatin1String("Xinje"), Qt::CaseInsensitive);
            const QString startLog = QStringLiteral("[PLC按键监控] 启动全局PLC监控: 设备=%1, 串口/IP=%2, 轮询地址=%3, 周期=%4ms")
                                        .arg(sharedPlcConfig_.plcDevice)
                                        .arg(isXinjie ? (sharedPlcConfig_.plcComPort.isEmpty() ? QStringLiteral("未配置COM口") : sharedPlcConfig_.plcComPort)
                                                      : QStringLiteral("%1:%2").arg(sharedPlcConfig_.plcIp).arg(sharedPlcConfig_.plcPort))
                                        .arg(sharedPlcConfig_.plcWaitAddress)
                                        .arg(sharedPlcConfig_.plcPollIntervalMs);
            qDebug().noquote() << startLog;
            emit sendBoxLog(startLog);
        }
    } else {
        if (plcMonitorRunning_) {
            plcMonitorTimer_->stop();
            plcMonitorRunning_ = false;
            qDebug() << "[QFreeWorkBox] 停止全局PLC按键轮询";
            emit sendBoxLog(QStringLiteral("[PLC按键监控] 停止按键监控"));
        }
    }
}

void QFreeWorkBox::onPlcMonitorTimeout() {
    if (isAnyStationTesting() || !sharedPlcConfig_.enabled) {
        updatePlcMonitorState();
        return;
    }

    const bool isXinjie = sharedPlcConfig_.plcDevice.contains(QLatin1String("Xinjie"), Qt::CaseInsensitive)
                          || sharedPlcConfig_.plcDevice.contains(QLatin1String("Xinje"), Qt::CaseInsensitive);

    if (isXinjie) {
        sharedPlcModbusManager_.setDeviceRoute(ModbusDeviceRoute::XinjiePlcRtu);
        QVariant isConn;
        sharedPlcModbusManager_.exec(XinjePlcCmd::IsConnected, {}, &isConn, nullptr);
        if (!isConn.toBool()) {
            QString err;
            QVariantMap connectParams;
            QString comPort = sharedPlcConfig_.plcComPort;
            if (comPort.isEmpty()) {
                comPort = selectedFixtureComName(0);
            }
            if (!comPort.isEmpty()) {
                connectParams.insert(QStringLiteral("comPort"), comPort);
                sharedPlcConfig_.plcComPort = comPort;
            }
            if (sharedPlcConfig_.plcBaudRate > 0)
                connectParams.insert(QStringLiteral("baudRate"), sharedPlcConfig_.plcBaudRate);
            connectParams.insert(QStringLiteral("slaveId"), sharedPlcConfig_.plcSlaveId);

            if (!sharedPlcModbusManager_.exec(XinjePlcCmd::Connect, connectParams, nullptr, &err)) {
                static qint64 lastConnLog = 0;
                const QString failMsg = QStringLiteral("[信捷PLC按键监控] 串口连接失败: %1 (COM=%2, 波特率=%3, 站号=%4)")
                                            .arg(err)
                                            .arg(comPort.isEmpty() ? QStringLiteral("未配置") : comPort)
                                            .arg(sharedPlcConfig_.plcBaudRate)
                                            .arg(sharedPlcConfig_.plcSlaveId);
                qDebug().noquote() << failMsg;
                if (QDateTime::currentMSecsSinceEpoch() - lastConnLog > 3000) {
                    emit sendBoxLog(failMsg);
                    lastConnLog = QDateTime::currentMSecsSinceEpoch();
                }
                return;
            } else {
                const QString okMsg = QStringLiteral("[信捷PLC按键监控] 串口连接成功: %1 @ %2bps (站号=%3)")
                                          .arg(comPort)
                                          .arg(sharedPlcConfig_.plcBaudRate)
                                          .arg(sharedPlcConfig_.plcSlaveId);
                qDebug().noquote() << okMsg;
                emit sendBoxLog(okMsg);
            }
        }

        QString plcErr;
        QVariant result;
        bool ok = false;
        QString addr = sharedPlcConfig_.plcWaitAddress.trimmed().toUpper();
        if (!addr.isEmpty() && addr.at(0).isDigit()) {
            addr = QStringLiteral("M") + addr;
        }
        QVariantMap param;
        param.insert(QStringLiteral("address"), addr);
        param.insert(QStringLiteral("quantity"), 1);
        if (addr.startsWith(QLatin1Char('X'))) {
            ok = sharedPlcModbusManager_.exec(XinjePlcCmd::ReadDiscreteInputs, param, &result, &plcErr);
        } else {
            ok = sharedPlcModbusManager_.exec(XinjePlcCmd::ReadCoils, param, &result, &plcErr);
        }

        if (ok) {
            const bool triggered = (result.isValid() && result.toBool());
            const int readVal = result.toInt();
            const QString logMsg = QStringLiteral("[信捷PLC按键监控] COM=%1 轮询地址: %2, 读值: %3 (%4)")
                                      .arg(sharedPlcConfig_.plcComPort.isEmpty() ? QStringLiteral("(默认)") : sharedPlcConfig_.plcComPort)
                                      .arg(addr)
                                      .arg(readVal)
                                      .arg(triggered ? QStringLiteral("● 按键已按下/触发") : QStringLiteral("○ 按键未按下/空闲"));
            qDebug().noquote() << logMsg;
            emit sendBoxLog(logMsg);

            if (triggered) {
                qDebug() << "[QFreeWorkBox] Xinjie PLC Trigger detected on" << addr << "! Stopping monitor and triggering all station scanners.";
                emit sendBoxLog(QStringLiteral("[信捷PLC按键监控] 检测到启动按键信号 (%1=1)，停止监控并联动触发一拖多所有扫码枪...").arg(addr));
                plcMonitorTimer_->stop();
                plcMonitorRunning_ = false;
                triggerAllStationScanners();
            }
        } else {
            const QString errLog = QStringLiteral("[信捷PLC按键监控] COM=%1 读取地址 %2 失败: %3")
                                      .arg(sharedPlcConfig_.plcComPort.isEmpty() ? QStringLiteral("(默认)") : sharedPlcConfig_.plcComPort)
                                      .arg(addr, plcErr);
            qDebug().noquote() << errLog;
            emit sendBoxLog(errLog);
        }
        return;
    }

    // Inovance H5U TCP 默认处理
    sharedPlcModbusManager_.setDeviceRoute(ModbusDeviceRoute::InovanceH5uTcp);
    if (!sharedPlcModbusManager_.isPlcConnected()) {
        QString err;
        QVariantMap connectParams;
        if (!sharedPlcConfig_.plcIp.isEmpty()) {
            connectParams.insert(QStringLiteral("host"), sharedPlcConfig_.plcIp);
        }
        if (sharedPlcConfig_.plcPort > 0) {
            connectParams.insert(QStringLiteral("port"), sharedPlcConfig_.plcPort);
        }

        if (!sharedPlcModbusManager_.exec(PlcCmd::Connect, connectParams, nullptr, &err)) {
            static qint64 lastLog = 0;
            if (QDateTime::currentMSecsSinceEpoch() - lastLog > 3000) {
                const QString failMsg = QStringLiteral("[H5U PLC按键监控] 连接失败: %1 (IP=%2:%3)")
                                            .arg(err)
                                            .arg(sharedPlcConfig_.plcIp)
                                            .arg(sharedPlcConfig_.plcPort);
                qDebug().noquote() << failMsg;
                emit sendBoxLog(failMsg);
                lastLog = QDateTime::currentMSecsSinceEpoch();
            }
            return;
        } else {
            const QString okMsg = QStringLiteral("[H5U PLC按键监控] TCP已连接: %1:%2")
                                      .arg(sharedPlcConfig_.plcIp)
                                      .arg(sharedPlcConfig_.plcPort);
            qDebug().noquote() << okMsg;
            emit sendBoxLog(okMsg);
        }
    }

    QString plcErr;
    QVariant param = sharedPlcConfig_.plcWaitAddressM;
    QVariant result;
    bool ok = sharedPlcModbusManager_.exec(PlcCmd::ReadCoil, param, &result, &plcErr);

    if (ok) {
        const bool triggered = (result.isValid() && result.toBool());
        const int readVal = result.toInt();
        const QString logMsg = QStringLiteral("[H5U PLC按键监控] 轮询地址: M%1, 读值: %2 (%3)")
                                  .arg(sharedPlcConfig_.plcWaitAddressM)
                                  .arg(readVal)
                                  .arg(triggered ? QStringLiteral("● 按键已按下/触发") : QStringLiteral("○ 按键未按下/空闲"));
        qDebug().noquote() << logMsg;
        emit sendBoxLog(logMsg);

        if (triggered) {
            qDebug() << "[QFreeWorkBox] PLC Trigger detected! Stopping monitor and triggering all station scanners.";
            emit sendBoxLog(QStringLiteral("[H5U PLC按键监控] 检测到启动按键信号 (M%1=1)，停止监控并联动触发一拖多所有扫码枪...").arg(sharedPlcConfig_.plcWaitAddressM));
            plcMonitorTimer_->stop();
            plcMonitorRunning_ = false;
            triggerAllStationScanners();
        }
    } else {
        const QString errLog = QStringLiteral("[H5U PLC按键监控] 读取地址 M%1 失败: %2")
                                  .arg(sharedPlcConfig_.plcWaitAddressM).arg(plcErr);
        qDebug().noquote() << errLog;
        emit sendBoxLog(errLog);
    }
}

void QFreeWorkBox::triggerAllStationScanners() {
    int triggeredCount = 0;
    for (test_base* t : testList) {
        auto* fw = qobject_cast<QFreeWork*>(t);
        if (!fw || fw->isTestContinue)
            continue;

        QString stationScannerIp = sharedPlcConfig_.scannerIp;
        if (sharedPlcConfig_.autoIncrementIpByStation) {
            QStringList parts = stationScannerIp.split('.');
            if (parts.size() == 4) {
                bool ok = false;
                int lastPart = parts[3].toInt(&ok);
                if (ok) {
                    lastPart += (fw->getIndex() - 1);
                    parts[3] = QString::number(lastPart);
                    stationScannerIp = parts.join('.');
                }
            }
        }

        const QString triggerLog = QStringLiteral("[PLC按键联动] 收到启动信号，触发工位#%1扫码枪 (IP: %2:%3)...")
                                      .arg(fw->getIndex())
                                      .arg(stationScannerIp)
                                      .arg(sharedPlcConfig_.scannerPort);
        emit sendBoxLog(triggerLog);
        fw->triggerScanner(stationScannerIp, sharedPlcConfig_.scannerPort, sharedPlcConfig_.scannerTimeoutMs);
        ++triggeredCount;
    }
    emit sendBoxLog(QStringLiteral("[PLC按键联动] 已向 %1 个一拖多工位并发发送扫码指令").arg(triggeredCount));
}
