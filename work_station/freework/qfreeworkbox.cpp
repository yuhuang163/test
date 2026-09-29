#include "qfreeworkbox.h"

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QLabel>
#include <QTimer>

#include "Abini.h"
#include "application_shutdown.h"
#include "asd9026a_device.h"
#include "factory_cloud_client.h"
#include "qfixturemanager.h"
#include "qfreework.h"
#include "screen_inspect_analyzer.h"
#include "shared_instrument.h"
#include "test_case_paths.h"
#include "test_case_store.h"
#include "ui_qfreeworkbox.h"
#include "xwd_raw_uart_codec.h"

#include "ui_qfreework.h"
#include <QDateTime>
#include <QMutexLocker>
#include <QRegularExpression>

QFreeWorkBox::QFreeWorkBox(QWidget* parent) : box_base(parent), ui(new Ui::QFreeWorkBox) {
    ui->setupUi(this);
    asd9026aDevice_ = new Asd9026aDevice(this);
    // 信捷 PLC 自动扫码：Connect 只负责 open 端口名，须先绑定 SerialChannel
    sharedPlcSerialChannel_ = new SerialChannel(this);
    sharedPlcModbusManager_.attachSerialChannel(sharedPlcSerialChannel_);
    plcMonitorTimer_ = new QTimer(this);
    connect(plcMonitorTimer_, &QTimer::timeout, this, &QFreeWorkBox::onPlcMonitorTimeout);
    CreatWindow<QFreeWork>(this);
    signalAndslot();
    recoverCustom();
    // 扫口异步完成后可能冲掉选中项，稍后再恢复一次已保存串口
    QTimer::singleShot(800, this, [this]() { recoverCustom(); });
    ShowData(this);
    setWindowTitle("自由工站");
    // 启动时清一次历史屏幕检测图（仅根目录测试抓拍；不碰「参考图」子目录）
    {
        const QString inspectRoot =
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
        QDir().mkpath(QDir(inspectRoot).filePath(QStringLiteral("参考图")));
        ScreenInspectAnalyzer::cleanupStoredImages(inspectRoot);
    }
    // 右下角：整包 OTA 版本 + 本文件编译时刻（与工站 FREE_VER 窗口标题无关）
    ui->statusbar->addPermanentWidget(new QLabel(FactoryCloudClient::appVersion() + QStringLiteral(" ") +
                                                 QString(__DATE__) + QStringLiteral(" ") + QString(__TIME__)));

    fixtureConnectAct_ = ui->menubar->addAction(QStringLiteral("连接治具串口"));
    connect(fixtureConnectAct_, &QAction::triggered, this, [this]() { ensureFixtureUartWindow(true); });

    plcFixtureConnectAct_ = ui->menubar->addAction(QStringLiteral("连接PLC治具串口"));
    connect(plcFixtureConnectAct_, &QAction::triggered, this, [this]() { ensurePlcFixtureUartWindow(true); });

    // 一个窗口两页签：PLC按键扫码 + 本工站 XWD 到位扫码（含监控治具串口）
    QAction* autoScanAct = ui->menubar->addAction(QStringLiteral("自动扫码配置"));
    connect(autoScanAct, &QAction::triggered, this, [this]() {
        if (preStartMonitorUi_ == nullptr)
            preStartMonitorUi_ = new Fixture_uart(nullptr, FixtureUartRole::PreStartMonitor);
        preStartMonitorUi_->reloadStationConfig();
        preStartMonitorUi_->raise();
        preStartMonitorUi_->show();
        preStartMonitorUi_->activateWindow();
    });

    QAction* startTest_act = ui->menubar->addAction("开始测试");
    connect(startTest_act, &QAction::triggered, this, &QFreeWorkBox::startTest);

    QAction* use_mes_act = ui->menubar->addAction(QStringLiteral("开关MES"));
    connect(use_mes_act, &QAction::triggered, this, [this]() {
        // 以工位0当前状态为准，全体工位同步勾选/取消「使用MES」与「从MES取MAC」
        const bool nextChecked =
            (!testList.isEmpty() && testList[0]->getIsUseMes())
                ? !testList[0]->getIsUseMes()->isChecked()
                : false;
        for (test_base* t : testList) {
            if (auto* use = t->getIsUseMes())
                use->setChecked(nextChecked);
            if (auto* form = t->getIsFormMes())
                form->setChecked(nextChecked);
        }
    });

    applyFixtureMenuLabel();

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
    delete xwdMonitorUartUi_;
    Fixture_uart_ui = nullptr;
    plcFixtureUartUi_ = nullptr;
    xwdMonitorUartUi_ = nullptr;
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
    releaseXwdMonitorUart();
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
            // 同一台 XWD：PlcFixture 与监控口任填一处即可
            QString flowPort =
                flowSettings.value(QStringLiteral("PreStart_Monitor/PlcFixtureComPort")).toString().trimmed();
            if (flowPort.isEmpty())
                flowPort =
                    flowSettings.value(QStringLiteral("PreStart_Monitor/XwdMonitorComPort")).toString().trimmed();
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

Fixture_uart* QFreeWorkBox::ensureXwdMonitorUartConnected(QString* detailOut) {
    const QString plcPort = resolvedPlcFixtureComName();
    QString monitorPort = sharedPlcConfig_.xwdMonitorComPort.trimmed();
    if (monitorPort.isEmpty())
        monitorPort = plcPort;

    if (monitorPort.isEmpty()) {
        if (detailOut)
            *detailOut = QStringLiteral("未配置 XWD 串口（「连接PLC治具串口」或「自动扫码监控串口」任填一处）");
        return nullptr;
    }

    // 同一台设备：监控口与 PLC 治具口相同，或只配了一处 → 共用 PLC 治具通道（SET_ALL_UP 同路）
    const bool sameDevice =
        plcPort.isEmpty() || monitorPort.compare(plcPort, Qt::CaseInsensitive) == 0;
    if (sameDevice) {
        xwdMonitorUsesSharedPlcUart_ = true;
        ensurePlcFixtureUartWindow(false);
        const int baud = sharedPlcConfig_.xwdMonitorBaudRate > 0 ? sharedPlcConfig_.xwdMonitorBaudRate
                                                                 : 115200;
        if (plcFixtureUartUi_->isFixtureSerialOpen()) {
            const QString opened = plcFixtureUartUi_->fixtureManager()
                                       ? plcFixtureUartUi_->fixtureManager()->portName()
                                       : QString();
            if (opened.compare(monitorPort, Qt::CaseInsensitive) == 0) {
                if (detailOut)
                    *detailOut = monitorPort;
                return plcFixtureUartUi_;
            }
            plcFixtureUartUi_->closeSerialPort();
        }
        plcFixtureUartUi_->setFixBaudRate(baud);
        if (!plcFixtureUartUi_->tryOpenSerialPort(monitorPort, true)) {
            if (detailOut)
                *detailOut = QStringLiteral("打开 XWD 共享口失败：%1 @ %2").arg(monitorPort).arg(baud);
            return nullptr;
        }
        if (detailOut)
            *detailOut = monitorPort;
        return plcFixtureUartUi_;
    }

    // 两处配成不同 COM（少见）：单独开监控口
    xwdMonitorUsesSharedPlcUart_ = false;
    const int baud = sharedPlcConfig_.xwdMonitorBaudRate > 0 ? sharedPlcConfig_.xwdMonitorBaudRate : 115200;
    if (xwdMonitorUartUi_ == nullptr)
        xwdMonitorUartUi_ = new Fixture_uart(nullptr, FixtureUartRole::FixturePort);

    if (xwdMonitorUartUi_->isFixtureSerialOpen()) {
        const QString opened = xwdMonitorUartUi_->fixtureManager()
                                   ? xwdMonitorUartUi_->fixtureManager()->portName()
                                   : QString();
        if (opened.compare(monitorPort, Qt::CaseInsensitive) == 0 && xwdMonitorUartUi_->fixBaudRate == baud) {
            if (detailOut)
                *detailOut = monitorPort;
            return xwdMonitorUartUi_;
        }
        xwdMonitorUartUi_->closeSerialPort();
    }

    xwdMonitorUartUi_->setFixBaudRate(baud);
    if (!xwdMonitorUartUi_->tryOpenSerialPort(monitorPort, true)) {
        if (detailOut)
            *detailOut = QStringLiteral("自动连接监控串口失败：%1 @ %2").arg(monitorPort).arg(baud);
        return nullptr;
    }
    if (detailOut)
        *detailOut = monitorPort;
    return xwdMonitorUartUi_;
}

void QFreeWorkBox::releaseXwdMonitorUart() {
    // 共用 PLC 治具通道时不关口，留给流程 SET_ALL_UP / DOWN?
    if (xwdMonitorUsesSharedPlcUart_)
        return;
    if (!xwdMonitorUartUi_ || !xwdMonitorUartUi_->isFixtureSerialOpen())
        return;
    xwdMonitorUartUi_->closeSerialPort();
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
    // 测试步骤与自动扫码共用该口：有工位在测时不可关
    if (isAnyStationTesting())
        return;
    QMutexLocker lock(&sharedXinjePlcMutex_);
    QVariant isConn;
    sharedPlcModbusManager_.exec(XinjePlcCmd::IsConnected, {}, &isConn, nullptr);
    if (isConn.toBool())
        sharedPlcModbusManager_.exec(XinjePlcCmd::Disconnect, {}, nullptr, nullptr);
    else if (sharedPlcSerialChannel_ && sharedPlcSerialChannel_->isOpen())
        sharedPlcSerialChannel_->close();
}

QMutex* QFreeWorkBox::sharedXinjePlcMutex() {
    return &sharedXinjePlcMutex_;
}

QModbusManager& QFreeWorkBox::sharedXinjePlcManager() {
    return sharedPlcModbusManager_;
}

QString QFreeWorkBox::sharedXinjePlcComPort() const {
    return sharedPlcConfig_.plcComPort.trimmed();
}

bool QFreeWorkBox::ensureSharedXinjePlcConnected(const QVariant& param, QString* errorMessage) {
    sharedPlcModbusManager_.setDeviceRoute(ModbusDeviceRoute::XinjiePlcRtu);
    QVariantMap connectParams;
    if (param.canConvert<QVariantMap>())
        connectParams = param.toMap();

    QString comPort = connectParams.value(QStringLiteral("comPort")).toString().trimmed();
    if (comPort.isEmpty())
        comPort = connectParams.value(QStringLiteral("portName")).toString().trimmed();
    if (comPort.isEmpty())
        comPort = sharedPlcConfig_.plcComPort.trimmed();
    if (comPort.isEmpty())
        comPort = SETTINGS.value(QStringLiteral("XINJE_PLC/ComPort")).toString().trimmed();
    if (comPort.isEmpty())
        comPort = selectedFixtureComName(0);
    if (!comPort.isEmpty()) {
        connectParams.insert(QStringLiteral("comPort"), comPort);
        sharedPlcConfig_.plcComPort = comPort;
    }

    if (!connectParams.contains(QStringLiteral("baudRate"))) {
        const int baud = sharedPlcConfig_.plcBaudRate > 0 ? sharedPlcConfig_.plcBaudRate
                                                          : SETTINGS.value(QStringLiteral("XINJE_PLC/BaudRate"), 19200).toInt();
        if (baud > 0)
            connectParams.insert(QStringLiteral("baudRate"), baud);
    }
    if (!connectParams.contains(QStringLiteral("slaveId")))
        connectParams.insert(QStringLiteral("slaveId"), sharedPlcConfig_.plcSlaveId > 0 ? sharedPlcConfig_.plcSlaveId : 1);
    // 现场默认偶校验，避免 SETTINGS 缺项时落到 8N1
    if (!connectParams.contains(QStringLiteral("parity")))
        connectParams.insert(QStringLiteral("parity"), QStringLiteral("even"));

    QVariant isConn;
    sharedPlcModbusManager_.exec(XinjePlcCmd::IsConnected, {}, &isConn, nullptr);
    if (isConn.toBool() && sharedPlcSerialChannel_ && sharedPlcSerialChannel_->isOpen()) {
        const QString opened = sharedPlcSerialChannel_->portName().trimmed();
        if (comPort.isEmpty() || opened.compare(comPort, Qt::CaseInsensitive) == 0)
            return true;
    }
    return sharedPlcModbusManager_.exec(XinjePlcCmd::Connect, connectParams, nullptr, errorMessage);
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
        const bool plcEnabled = settings.contains("Enabled") && settings.value("Enabled").toBool();
        const bool xwdEnabled = settings.value(QStringLiteral("XwdDownScanEnabled"), false).toBool();
        sharedPlcConfig_.xwdDownScanEnabled = xwdEnabled;
        // 自动扫码监控口：独立键；旧配置若只有 PlcFixtureComPort 则回退一次便于迁移
        sharedPlcConfig_.xwdMonitorComPort =
            settings.value(QStringLiteral("XwdMonitorComPort")).toString().trimmed();
        if (sharedPlcConfig_.xwdMonitorComPort.isEmpty()) {
            sharedPlcConfig_.xwdMonitorComPort =
                settings.value(QStringLiteral("PlcFixtureComPort")).toString().trimmed();
        }
        sharedPlcConfig_.xwdMonitorBaudRate =
            settings.value(QStringLiteral("XwdMonitorBaudRate"), 0).toInt();
        if (sharedPlcConfig_.xwdMonitorBaudRate <= 0) {
            sharedPlcConfig_.xwdMonitorBaudRate =
                settings.value(QStringLiteral("PlcFixtureBaudRate"), 115200).toInt();
        }
        if (sharedPlcConfig_.xwdMonitorBaudRate <= 0)
            sharedPlcConfig_.xwdMonitorBaudRate = 115200;
        sharedPlcConfig_.scannerComPort = settings.value(QStringLiteral("ScannerComPort")).toString().trimmed();
        sharedPlcConfig_.scannerBaudRate = settings.value(QStringLiteral("ScannerBaudRate"), 9600).toInt();
        sharedPlcConfig_.scannerHexCmd = settings.value(QStringLiteral("ScannerHexCmd")).toString().trimmed();
        sharedPlcConfig_.autoIncrementComByStation =
            settings.value(QStringLiteral("AutoIncrementComByStation"), true).toBool();
        if (xwdEnabled) {
            sharedPlcConfig_.scannerTimeoutMs = settings.value("ScannerTimeoutMs", 3000).toInt();
            sharedPlcConfig_.plcPollIntervalMs = qMax(50, settings.value("PlcPollIntervalMs", 500).toInt());
        }
        if (plcEnabled) {
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
            if (sharedPlcConfig_.plcBaudRate <= 0)
                sharedPlcConfig_.plcBaudRate = 19200;
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
            if (!xwdEnabled)
                sharedPlcConfig_.scannerTimeoutMs = settings.value("ScannerTimeoutMs", 1000).toInt();
            sharedPlcConfig_.autoIncrementIpByStation = settings.value("AutoIncrementIpByStation", true).toBool();
        }
        settings.endGroup();
    }

    const bool anyAutoScan = sharedPlcConfig_.enabled || sharedPlcConfig_.xwdDownScanEnabled;

    // 跨工位同步勾选框显示与选中态
    for (test_base* t : testList) {
        if (auto* fw = qobject_cast<QFreeWork*>(t)) {
            if (fw->ui && fw->ui->autoStartCheckBox) {
                fw->ui->autoStartCheckBox->blockSignals(true);
                fw->ui->autoStartCheckBox->setVisible(anyAutoScan);
                fw->ui->autoStartCheckBox->setChecked(isAutoStartChecked);
                fw->ui->autoStartCheckBox->blockSignals(false);
            }
        }
    }

    const bool anyTesting = isAnyStationTesting();
    if (anyAutoScan && !anyTesting && isAutoStartChecked) {
        if (!plcMonitorRunning_) {
            plcMonitorTimer_->start(sharedPlcConfig_.plcPollIntervalMs);
            plcMonitorRunning_ = true;
        }
    } else {
        if (plcMonitorRunning_) {
            plcMonitorTimer_->stop();
            plcMonitorRunning_ = false;
            releaseXwdMonitorUart();
        }
    }
}

void QFreeWorkBox::onPlcMonitorTimeout() {
    if (isAnyStationTesting() || (!sharedPlcConfig_.enabled && !sharedPlcConfig_.xwdDownScanEnabled)) {
        updatePlcMonitorState();
        return;
    }

    if (sharedPlcConfig_.xwdDownScanEnabled) {
        if (pollXwdDownAndTriggerScan())
            return;
        if (!sharedPlcConfig_.enabled)
            return;
    }

    const bool isXinjie = sharedPlcConfig_.plcDevice.contains(QLatin1String("Xinjie"), Qt::CaseInsensitive)
                          || sharedPlcConfig_.plcDevice.contains(QLatin1String("Xinje"), Qt::CaseInsensitive);

    if (isXinjie) {
        QMutexLocker lock(&sharedXinjePlcMutex_);
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
            connectParams.insert(QStringLiteral("parity"), QStringLiteral("even"));

            if (!sharedPlcModbusManager_.exec(XinjePlcCmd::Connect, connectParams, nullptr, &err)) {
                return;
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
            if (triggered) {
                plcMonitorTimer_->stop();
                plcMonitorRunning_ = false;
                // 故意不断开串口：后续测试步骤仍走同一信捷 COM
                lock.unlock();
                triggerAllStationScanners();
            }
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
            return;
        }
    }

    QString plcErr;
    QVariant param = sharedPlcConfig_.plcWaitAddressM;
    QVariant result;
    bool ok = sharedPlcModbusManager_.exec(PlcCmd::ReadCoil, param, &result, &plcErr);

    if (ok) {
        const bool triggered = (result.isValid() && result.toBool());
        if (triggered) {
            plcMonitorTimer_->stop();
            plcMonitorRunning_ = false;
            triggerAllStationScanners();
        }
    }
}

void QFreeWorkBox::triggerAllStationScanners() {
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

        fw->triggerScanner(stationScannerIp, sharedPlcConfig_.scannerPort, sharedPlcConfig_.scannerTimeoutMs);
    }
}

bool QFreeWorkBox::pollXwdDownAndTriggerScan() {
    QString detail;
    Fixture_uart* monitorUart = ensureXwdMonitorUartConnected(&detail);
    if (!monitorUart || !monitorUart->isFixtureSerialOpen()) {
        return false;
    }

    // 与测试步骤 DOWN? 一致：发 DOWN?\r\n；到位通过只认 ALL_DOWN_OK（一拖多全部下压）
    const QByteArray request = XwdRawUartCodec::encodeRawText(QStringLiteral("DOWN?\\r\\n"));
    QByteArray response;
    QString err;
    QMutex* mu = xwdMonitorUsesSharedPlcUart_ ? sharedPlcFixtureMutex() : sharedXwdMonitorMutex();
    bool okExchange = false;
    {
        QMutexLocker locker(mu);
        okExchange = monitorUart->exchangeRawFrame(request, &response, 400, &err);
    }
    if (!okExchange) {
        // 未到位/无应答属常态，不刷屏
        return false;
    }

    const QString text = QString::fromUtf8(response).trimmed();
    const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts);
    QString matchedLine;
    for (const QString& line : lines) {
        const QString t = line.trimmed();
        if (t.contains(QStringLiteral("ALL_DOWN_OK"), Qt::CaseInsensitive)) {
            matchedLine = t;
            break;
        }
    }
    if (matchedLine.isEmpty()) {
        return false;
    }

    plcMonitorTimer_->stop();
    plcMonitorRunning_ = false;
    // 独立监控口才释放；共用 PLC 治具通道则保持打开供后续 SET_ALL_UP
    releaseXwdMonitorUart();
    triggerAllStationSerialScanners();
    return true;
}

void QFreeWorkBox::triggerAllStationSerialScanners() {
    for (test_base* t : testList) {
        auto* fw = qobject_cast<QFreeWork*>(t);
        if (!fw || fw->isTestContinue)
            continue;

        QString comPort = sharedPlcConfig_.scannerComPort.trimmed();
        // 一拖多：1 号用配置口，2 号及以后 COM 号自动 +1（COM8→工位2=COM9）
        if (sharedPlcConfig_.autoIncrementComByStation && !comPort.isEmpty() && fw->getIndex() > 1) {
            static const QRegularExpression re(QStringLiteral("^COM(\\d+)$"),
                                               QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch m = re.match(comPort);
            if (m.hasMatch()) {
                const int base = m.captured(1).toInt();
                comPort = QStringLiteral("COM%1").arg(base + (fw->getIndex() - 1));
            }
        }

        const int baud = sharedPlcConfig_.scannerBaudRate > 0 ? sharedPlcConfig_.scannerBaudRate : 9600;
        const int timeout = sharedPlcConfig_.scannerTimeoutMs > 0 ? sharedPlcConfig_.scannerTimeoutMs : 3000;
        fw->triggerSerialScanner(comPort, baud, sharedPlcConfig_.scannerHexCmd, timeout);
    }
}

QMutex* QFreeWorkBox::sharedPlcFixtureMutex() {
    return &sharedPlcFixtureMutex_;
}

QMutex* QFreeWorkBox::sharedXwdMonitorMutex() {
    return &sharedXwdMonitorMutex_;
}
