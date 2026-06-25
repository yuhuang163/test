#include "qfreework.h"

#include "test_case.h"

#include <QMessageBox>
#include <QPushButton>
#include <QAbstractButton>
#include <QLabel>
#include <QFont>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QTabBar>
#include <QTabWidget>
#include <QThread>
#include <QtGlobal>
#include "qproduct.h"
#include "test_data_upload_service.h"
#include "test_record_store.h"
#include "ui_qfreework.h"

namespace {

/** 测试项提示弹窗：产线可读性，正文与按钮字号加大 */
void applyTestItemPromptFont(QMessageBox* box) {
    if (!box) {
        return;
    }
    QFont font = box->font();
    font.setPointSize(18);
    box->setFont(font);
    for (QLabel* label : box->findChildren<QLabel*>()) {
        label->setFont(font);
    }
    for (QAbstractButton* btn : box->findChildren<QAbstractButton*>()) {
        btn->setFont(font);
        btn->setMinimumHeight(44);
        btn->setMinimumWidth(96);
    }
}

/** MES 分段用 | 拼接，value 内禁止裸 |，避免解析错位。 */
QString sanitizeMesValuePipes(QString v) {
    v.replace(QLatin1Char('|'), QStringLiteral("｜"));
    return v;
}

/** value 尽量 ASCII：PASS / FAIL；通过且有数据时只带数据；失败为 FAIL 或 FAIL;数据。 */
QString freeWorkMesValueEnglish(bool stepPass, const QString& testData) {
    const bool hasData = !testData.trimmed().isEmpty() && testData != QStringLiteral("-");
    if (stepPass)
        return hasData ? sanitizeMesValuePipes(testData) : QStringLiteral("PASS");
    return hasData ? (QStringLiteral("FAIL;") + sanitizeMesValuePipes(testData)) : QStringLiteral("FAIL");
}

static void pushMesSeg(QVector<QPair<QString, QString>>* out, const QString& key, const QString& value) {
    const QString k = key.trimmed();
    if (k.isEmpty())
        return;
    out->append(qMakePair(k, sanitizeMesValuePipes(value)));
}

static void appendOneMesStep(QVector<QPair<QString, QString>>* out, const QString& tag, bool pass, const QString& testData) {
    pushMesSeg(out, tag, freeWorkMesValueEnglish(pass, testData));
}

/** 与 hqmes/wksmes 一致：每段一个 ASCII ':' 分隔键值，多段用 | 连接。 */
QString joinFreeWorkMesItemvalue(const QVector<QPair<QString, QString>>& segments, const QString& overallResult,
                                 const QString& failValueLiteral) {
    QStringList parts;
    parts.reserve(segments.size() + 1);
    for (const auto& p : segments) {
        const QString k = p.first.trimmed();
        if (k.isEmpty())
            continue;
        parts << k + QLatin1Char(':') + p.second;
    }
    if (parts.isEmpty()) {
        const QString v = (overallResult == failValueLiteral) ? QStringLiteral("FAIL") : QStringLiteral("PASS");
        parts << QStringLiteral("SUMMARY:") + v;
    }
    return QStringLiteral("|") + parts.join(QStringLiteral("|")) + QStringLiteral("|");
}

bool isDongleBleConnectStepName(const QString& name) {
    return name.contains(QStringLiteral("直连接蓝牙")) || name.contains(QStringLiteral("扫描连接蓝牙"));
}

/** 按当前标签文字重算 min-width，覆盖全局 Ubuntu.qss 对 Tab 的裁切。 */
void setupFreeWorkTabBar(QTabWidget* tabWidget) {
    if (!tabWidget)
        return;

    tabWidget->setUsesScrollButtons(true);
    QTabBar* bar = tabWidget->tabBar();
    if (!bar)
        return;

    bar->setExpanding(false);
    bar->setElideMode(Qt::ElideNone);
    bar->setUsesScrollButtons(true);

    QFont font = bar->font();
    font.setPixelSize(14);
    bar->setFont(font);

    const QFontMetrics fm(font);
    int maxTextWidth = 0;
    for (int i = 0; i < bar->count(); ++i)
        maxTextWidth = qMax(maxTextWidth, fm.horizontalAdvance(bar->tabText(i)));

    constexpr int hPad = 32;
    const int minTabWidth = maxTextWidth + hPad;
    // updateMainStyle 之后覆盖全局 QTabBar 规则
    bar->setStyleSheet(QStringLiteral(
                           "QTabBar::tab {"
                           "  min-width: %1px;"
                           "  padding: 6px 16px;"
                           "  font-size: 14px;"
                           "}"
                           "QTabBar::tab:selected {"
                           "  font-weight: bold;"
                           "}")
                           .arg(minTabWidth));
    bar->updateGeometry();
}

} // namespace

void QFreeWork::onTestCaseStepMarkedDone(bool pass, const QString& testData, const QString& ask) {
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = testData;
    if (!ask.isEmpty())
        stepRuntime_.ask = ask;
}

void QFreeWork::appendTestCaseMes(const TestCaseDefinition& def, bool pass, const QString& testData) {
    const QString tag = def.meta.mesTag.trimmed().isEmpty() ? def.meta.name.trimmed() : def.meta.mesTag.trimmed();
    appendOneMesStep(&freeWorkMesSegments_, tag, pass, testData);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QFreeWork::QFreeWork(int index, QWidget* parent) : test_base(parent), ui(new Ui::QFreeWork) {
    registerFreeWorkTestCaseHooks();
    registerQFreeWorkCatalogTestCaseHooks();
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = FREE_VER;

    setupModbusManager();
    plcFacade_.setModbusManager(&modbusManager);

    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    setupFreeWorkTabBar(ui->tabWidget);
    scanSerialPorts(); // 要搜索一下一开始
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
    });

    connect(comparewaittime, &QTimer::timeout, [=]() {
        iscompareovertime = 1;
        comparewaittime->stop();
    });

    HighRssi = SETTINGS.value("WIFI/HighRssi").toDouble();
    LowRssi = SETTINGS.value("WIFI/LowRssi").toDouble();
    BleHighRssi = SETTINGS.value("BLE/HighRssi").toDouble();
    BleLowRssi = SETTINGS.value("BLE/LowRssi").toDouble();
    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();
    HighCurrent = SETTINGS.value("Current/HighCharCurrent").toDouble();
    LowCurrent = SETTINGS.value("Current/LowCharCurrent").toDouble();
    lowKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/Low"), 1).toUInt();
    highKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/High"), 65535).toUInt();

    measure_wait_time = SETTINGS.value("Current/measure_wait_time").toInt();

    RssiTestTime = SETTINGS.value("BLE/RssiCount").toInt();
    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "usmile123").toString());

    showlog("HighCurrent=" + QString::number(HighCurrent));
    showlog("LowCurrent=" + QString::number(LowCurrent));
    showlog("measure_wait_time=" + QString::number(measure_wait_time));

    showlog("machineNo=" + pack.machineNo);
    showlog("standbattary=" + QString::number(standbattary));
    showlog("model=" + pack.model);
    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);

    if (pack.factory == "lx" || pack.factory == "jj") {
        usbBaudRate = 9600;
    } else {
        usbBaudRate = 115200;
        // ui->usbdisconnectButton->setDisabled(true);
        // ui->usbconnectButton->setDisabled(true);
        // ui->usbcomNameCombo->setDisabled(true);
    }
    if (pack.factory == "hq" || pack.factory == "jj") {
        ui->jigComNameCombo->setEnabled(false);
        ui->jigConnectButton->setEnabled(false);
        ui->jigDisconnectButton->setEnabled(false);
    }

    refreshOrderedTestIndexes();
    testResultTableInit();
    if (product) {
        connect(product, &Qproduct::instrumentStopReceiveSeen, this, &QFreeWork::onProductInstrumentStopReceiveAckForPer);
    }
    ui->tabWidget->setCurrentIndex(0); // 设置当前页为第一页
}
void QFreeWork::refreshOrderedTestIndexes() {
    const QString stationName = SETTINGS.value("TestOrderMeta/SelectedStationName").toString().trimmed();
    const QString tabName = stationName.isEmpty() ? "自由工站" : stationName;
    ui->tabWidget->setTabText(0, tabName);
    setupFreeWorkTabBar(ui->tabWidget);
    qDebug() << "[FreeWork] refresh tab, SelectedStationName =" << stationName << ", tabName =" << tabName;

    orderedTestCaseNames_.clear();
    stopFlowOnTestFail_ = true;

    QString stationKey = TestCaseStore::resolveFlowStationKey(
        SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString());
    if (TestCaseStore::loadStationFlowItems(stationKey).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(
            SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStationName")).toString());
        if (!byName.isEmpty())
            stationKey = byName;
    }
    if (stationKey.isEmpty())
        stationKey = QStringLiteral("default");

    if (!QFile::exists(TestCasePaths::flowIniPath())) {
        showlog(QStringLiteral("未找到测试流程文件，请在设置页「测试流程编排」中配置"));
        qDebug() << "[FreeWork] flow ini missing:" << TestCasePaths::flowIniPath();
        return;
    }

    stopFlowOnTestFail_ = TestCaseStore::loadStationStopFlowOnTestFail(stationKey, true);
    const QVector<TestFlowItemEntry> flowItems = TestCaseStore::loadStationFlowItems(stationKey);
    for (const TestFlowItemEntry& entry : flowItems) {
        if (entry.enabled)
            orderedTestCaseNames_.append(entry.caseName);
    }

    if (orderedTestCaseNames_.isEmpty()) {
        if (!flowItems.isEmpty()) {
            showlog(QStringLiteral("当前工站流程步骤均已取消勾选，请在设置页「测试流程编排」中重新勾选"));
            qDebug() << "[FreeWork] all flow items disabled, station =" << stationKey;
        } else {
            showlog(QStringLiteral("当前工站未配置测试步骤，请在设置页「测试流程编排」中添加"));
            qDebug() << "[FreeWork] empty flow, station =" << stationKey;
        }
    } else {
        qDebug() << "[FreeWork] 使用 test_case 流程, station =" << stationKey << ", items =" << orderedTestCaseNames_;
    }
}

bool QFreeWork::currentOrderedStepIsDongleBleConnect() const {
    if (teststate < 0 || teststate >= orderedTestCaseNames_.count()) {
        return false;
    }
    TestCaseDefinition caseDef;
    if (!TestCaseRunner::loadCase(orderedTestCaseNames_.at(teststate), caseDef)) {
        return false;
    }
    if (TestCaseRunner::isDongleBleConnectStep(caseDef)) {
        return true;
    }
    return isDongleBleConnectStepName(caseDef.meta.name);
}

bool QFreeWork::canRunOrderedTestStepLoop() const {
    if (at->getConnected()) {
        return true;
    }
    if (stepRuntime_.started) {
        return true;
    }
    if (teststate >= 0 && teststate < orderedTestCaseNames_.count()) {
        TestCaseDefinition caseDef;
        if (TestCaseRunner::loadCase(orderedTestCaseNames_.at(teststate), caseDef)) {
            return !TestCaseRunner::stepRequiresProductBle(caseDef);
        }
        return true;
    }
    return currentOrderedStepIsDongleBleConnect();
}

bool QFreeWork::isBydFactory() const {
    return pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0;
}

QString QFreeWork::resolvedExpectedTailSnText() const {
    if (isBydFactory()) {
        return QString::fromUtf8(expectedTailSnFromMes.trimmed());
    }
    if (ui && ui->getMac) {
        return ui->getMac->text().trimmed();
    }
    return {};
}

QByteArray QFreeWork::resolvedTailSnToWrite() const {
    return resolvedExpectedTailSnText().toUtf8();
}

void QFreeWork::runTestFlowBootstrap() {
    showlog(QStringLiteral("开始测试"));
    initData();
    // 每次开始测试都重新读取配置，避免设置页调整后本页仍使用旧队列。
    refreshOrderedTestIndexes();
    waitWork(1000);
    showlog(QStringLiteral("MAC地址为：") + ui->macInput->text());
    teststate = 0;
}

bool QFreeWork::tickOrderedTestStepLoop() {
    const int stepCount = orderedTestCaseNames_.count();
    for (; teststate < stepCount;) {
        TestCaseDefinition caseDef;
        QString functionName;
        const QString caseName = orderedTestCaseNames_.at(teststate);
        if (!TestCaseRunner::loadCase(caseName, caseDef)) {
            ++teststate;
            stepRuntime_.reset();
            clearActiveTestCase();
            break;
        }
        functionName = TestCaseRunner::stepLabel(caseDef);
        const bool needCaseDone = TestCaseRunner::needAsyncDone(caseDef);

        if (!stepRuntime_.started) {
            if (!canGoNext) {
                break;
            }
            stepRuntime_.started = true;
            stepRuntime_.done = false;
            stepRuntime_.pass = true;
            stepRuntime_.testData = QStringLiteral("-");
            stepRuntime_.ask = QStringLiteral("通过");
            stepRuntime_.caseTimer.restart();
            lastCommandRetryCount = 0;
            testCasePromptAcknowledged_ = false;
            testCasePromptProgrammaticClose_ = false;
            showlog(QStringLiteral("开始测试内容：") + functionName);
            showTestCasePromptForStep(caseDef);
            TestCaseRunner::beginStep(this, caseDef);
            qDebug() << "程序在跑" << teststate << stepCount;
            break;
        }

        if (!canGoNext) {
            break;
        }

        if (sendRetryOver) {
            sendRetryOver = false;
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("协议FAIL或超时");
            TestResult = failValue;
            showlog(QStringLiteral("步骤失败：%1（超时未响应或协议 FAIL）").arg(functionName));
        }

        if (!caseDef.gate.enabled && canGoNext && !stepRuntime_.done && !sendRetryOver) {
            const bool dongleBleConnect = TestCaseRunner::isDongleBleConnectStep(caseDef);
            const bool productGet = !caseDef.hook.enabled && !dongleBleConnect && caseDef.send.channel == TestCaseSendChannel::Product && caseDef.send.action == TestCaseSendAction::Get;
            // 治具/产品串口 Get 等异步等待须由回包回调 markActiveTestCaseStepDone，不可在此直接 done
            const bool fixtureOrSerialAsync =
                caseDef.send.channel == TestCaseSendChannel::Fixture ||
                caseDef.send.channel == TestCaseSendChannel::ProductSerial;
            if (!caseDef.hook.enabled && !dongleBleConnect && !productGet && !fixtureOrSerialAsync) {
                if (!TestCaseRunner::stepWaitsForPromptAck(caseDef) || testCasePromptAcknowledged_) {
                    stepRuntime_.done = true;
                    stepRuntime_.pass = true;
                    if (stepRuntime_.testData == QLatin1String("-"))
                        stepRuntime_.testData = QStringLiteral("ok");
                }
            } else if ((caseDef.hook.enabled || dongleBleConnect) && (lastCommandRetryCount > 0 || (dongleBleConnect && at->getConnected()))) {
                stepRuntime_.done = true;
                stepRuntime_.pass = true;
                if (dongleBleConnect && at->getConnected()) {
                    stepRuntime_.testData = QStringLiteral("已连接");
                } else if (stepRuntime_.testData == QLatin1String("-")) {
                    stepRuntime_.testData = QStringLiteral("ok");
                }
            }
        }

        if (needCaseDone && !stepRuntime_.done) {
            break;
        }

        if (!stepRuntime_.pass) {
            TestResult = failValue;
        }

        const qint64 caseElapsedMs = stepRuntime_.caseTimer.isValid() ? stepRuntime_.caseTimer.elapsed() : 0;
        const int caseRetryCount = lastCommandRetryCount;
        showlog(QStringLiteral("测试内容完成：%1，重试次数=%2，测试时长=%3ms")
                    .arg(functionName)
                    .arg(caseRetryCount)
                    .arg(caseElapsedMs));
        if (!testCaseMultiGateTableEmitted_) {
            TestItem test;
            test.testItem = functionName;
            test.testData = stepRuntime_.testData;
            test.testResult = stepRuntime_.pass ? QStringLiteral("通过") : QStringLiteral("失败");
            test.ask = stepRuntime_.ask;
            testItems.append(test);
        }
        testCaseMultiGateTableEmitted_ = false;
        appendTestCaseMes(caseDef, stepRuntime_.pass, stepRuntime_.testData);
        if (caseDef.timing.delayAfterMs > 0) {
            waitWork(caseDef.timing.delayAfterMs);
        }
        closeTestCasePrompt();
        closeKeyWaitPrompt();
        clearActiveTestCase();
        testResultTableUpdate(testItems);

        ++teststate;
        if (stopFlowOnTestFail_ && !stepRuntime_.pass) {
            showlog(QStringLiteral("测试失败，按流程设置结束后续步骤"));
            teststate = orderedTestCaseNames_.count();
        }
        stepRuntime_.reset();

        break;
    }
    return true;
}

void QFreeWork::finalizeTestFlowIfComplete() {
    const int flowStepCount = orderedTestCaseNames_.count();
    if (teststate != flowStepCount || teststate == 0) {
        return;
    }

    // testItems 在 testResultTableUpdate 内会 clear；MES 分项与上表同步写入 freeWorkMesSegments_。
    const QString mesItemValue = joinFreeWorkMesItemvalue(freeWorkMesSegments_, TestResult, failValue);
    showlog(QStringLiteral("mesItemValue======") + mesItemValue);
    pack.itemvalue = mesItemValue;
    pack.sn = ui->getMac->text();
    pack.product = SETTINGS.value("Mes/Product_Name").toString();
    pack.instruct_num = QStringLiteral("079");
    if (TestResult == failValue) {
        ui->test_result->setText(QStringLiteral("FAIL"));
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
            "border-radius: 10px; padding: 10px; text-align: center; ");
        pack.result = QStringLiteral("NG");
    } else {
        ui->test_result->setText(QStringLiteral("PASS"));
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
            "border-radius: 10px; padding: 10px; text-align: center;");
        pack.result = QStringLiteral("PASS");
    }

    finishTestRecord(pack, ui->isusemes->checkState());

    qDebug() << "测试结束";
    teststate = -1;
    stepRuntime_.reset();
    ui->macInput->clear();
    ui->snInput->clear();
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);
    waitWork(50);
    on_disconnectButton_clicked();
    emit send_end_test(getIndex());
    ui->getMac->clear();
    isTestContinue = false;
}

void QFreeWork::startTask() {
    if (!isTestContinue) {
        return;
    }

    ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
    if (teststate == -1) {
        runTestFlowBootstrap();
    }
    if (canRunOrderedTestStepLoop()) {
        tickOrderedTestStepLoop();
    }
    finalizeTestFlowIfComplete();
}

QFreeWork::~QFreeWork() {
    if (jigSerialPort->isOpen()) {
        disconnect(jigSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        jigSerialPort->close();
        qDebug() << getIndex() << "已关闭jig串口";
    }
    delete ui;
}

void QFreeWork::refreshDongleWifi(QString data) {
    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
}

void QFreeWork::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        // protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        // showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void QFreeWork::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}
void QFreeWork::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        showlog("usb串口连接断开");

        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }
}

void QFreeWork::refreshJigUartState(int state) {
    if (state)
        showlog("治具串口连接成功");
    else {
        ui->jigComNameCombo->setEnabled(true);
        ui->jigConnectButton->setEnabled(true);
        showlog("治具串口连接断开");
    }
}

void QFreeWork::refreshProductUartState(int state) {
    if (state) {
        showlog(QStringLiteral("产品串口(仪器)连接成功"));
    } else {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        showlog(QStringLiteral("产品串口(仪器)连接断开"));
    }
}

void QFreeWork::startKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                                   const QString& enableKey) {
    if (!SETTINGS.value(enableKey).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = "按键配置未启用";
        stepRuntime_.ask = "请检查配置";
        TestResult = failValue;
        showlog(testName + "失败：按键配置未启用");
        return;
    }

    currentKeyTestName_ = testName;
    currentKeyExpectedKey_ = expectedKey;
    freeWorkKeyWaiting_ = true;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = "等待按键上报";
    stepRuntime_.ask = SETTINGS.value(expectedKey).toString();

    closeTestCasePrompt();
    closeKeyWaitPrompt();
    keyWaitPrompt_ = new QMessageBox(QMessageBox::Information, "按键测试", promptText, QMessageBox::NoButton, this);
    keyWaitPrompt_->setStandardButtons(QMessageBox::NoButton);
    QPushButton* hiddenCloseButton = keyWaitPrompt_->addButton("", QMessageBox::RejectRole);
    hiddenCloseButton->hide();
    keyWaitPrompt_->setAttribute(Qt::WA_DeleteOnClose);
    keyWaitPromptProgrammaticClose_ = false;
    applyTestItemPromptFont(keyWaitPrompt_);
    connect(keyWaitPrompt_, &QObject::destroyed, this, [this]() {
        keyWaitPrompt_ = nullptr;
        if (freeWorkKeyWaiting_ && !keyWaitPromptProgrammaticClose_) {
            ++plcKeyBleWaitSeq_;
            freeWorkKeyWaiting_ = false;
            plcSwitchBlePhase_ = 0;
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = "用户关闭按键弹窗";
            stepRuntime_.ask = SETTINGS.value(currentKeyExpectedKey_).toString();
            TestResult = failValue;
            showlog(currentKeyTestName_ + "失败：用户关闭按键弹窗");
        }
        keyWaitPromptProgrammaticClose_ = false;
    });
    keyWaitPrompt_->show();
    showlog("等待按键测试：" + testName);
}

void QFreeWork::startPlcKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                                      const QString& enableKey, int keyIndex0To6, bool useCapacitanceRead) {
    if (!SETTINGS.value(enableKey).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = "按键配置未启用";
        stepRuntime_.ask = "请检查配置";
        TestResult = failValue;
        showlog(testName + "失败：按键配置未启用");
        return;
    }

    currentKeyTestName_ = testName;
    currentKeyExpectedKey_ = expectedKey;
    plcKeyBlePlcOkSummary_.clear();
    plcSwitchBlePhase_ = 0;
    plcKeyCapPollMode_ = false;
    currentKeyCapRequestKk_ = -1;
    currentKeyConfiguredId_ = 0;
    resetPlcKeyCapSyncReadState();

    const bool capMode = useCapacitanceRead && keyIndex0To6 >= 0 && keyIndex0To6 <= 5;
    freeWorkKeyWaiting_ = !capMode;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = capMode ? QStringLiteral("PLC整步与读取按键电容") : QStringLiteral("PLC整步与等待按键上报");
    stepRuntime_.ask = SETTINGS.value(expectedKey).toString();

    closeKeyWaitPrompt();
    if (!promptText.isEmpty()) {
        showlog(testName + QStringLiteral("：") + promptText);
    }
    if (capMode) {
        const QString keyIdText = SETTINGS.value(expectedKey).toString().trimmed();
        bool keyIdOk = false;
        const int configuredKeyId = keyIdText.toInt(&keyIdOk);
        if (!keyIdOk || configuredKeyId <= 0) {
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("按键ID配置无效:") + keyIdText;
            TestResult = failValue;
            showlog(testName + QStringLiteral("失败：按键ID配置无效 ") + keyIdText);
            return;
        }
        currentKeyConfiguredId_ = configuredKeyId;
        currentKeyCapRequestKk_ = configuredKeyId - 1;
        lowKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/Low"), 1).toUInt();
        highKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/High"), 65535).toUInt();
        const QString capAsk = QStringLiteral("[%1,%2]").arg(lowKeyCap_).arg(highKeyCap_);
        stepRuntime_.ask = QStringLiteral("KK=%1;ID=%2;电容%3")
                               .arg(currentKeyCapRequestKk_)
                               .arg(configuredKeyId)
                               .arg(capAsk);
        plcKeyCapPollMode_ = true;
        showlog(testName + QStringLiteral("：治具下压期间读取电容 KK=%1（配置ID=%2 减1），卡控%3，读%4次").arg(currentKeyCapRequestKk_).arg(configuredKeyId).arg(capAsk).arg(SETTINGS.value(QStringLiteral("KeyCap/ReadCount"), 3).toInt()));
    } else {
        showlog(testName + QStringLiteral("：已等待协议按键，将执行PLC整步"));
    }

    runPlcV3TouchKeyFull(keyIndex0To6, capMode);

    plcKeyCapPollMode_ = false;
    resetPlcKeyCapSyncReadState();

    if (!capMode) {
        if (!stepRuntime_.pass) {
            ++plcKeyBleWaitSeq_;
            freeWorkKeyWaiting_ = false;
            plcSwitchBlePhase_ = 0;
            closeKeyWaitPrompt();
            plcKeyBlePlcOkSummary_.clear();
            return;
        }
        if (stepRuntime_.done) {
            ++plcKeyBleWaitSeq_;
            freeWorkKeyWaiting_ = false;
            plcSwitchBlePhase_ = 0;
            return;
        }
        waitPlcBleKeyReportBlocking();
    }
}

void QFreeWork::waitPlcBleKeyReportBlocking() {
    const int bleWaitMs = SETTINGS.value(QStringLiteral("KeyTest/TimeoutMs"), 5000).toInt();
    const int pollMs = qMax(20, SETTINGS.value(QStringLiteral("KeyTest/PollIntervalMs"), 50).toInt());
    showlog(currentKeyTestName_ + QStringLiteral("：阻塞等待协议上报（waitWork，超时 %1ms）").arg(bleWaitMs));
    QElapsedTimer timer;
    timer.start();
    while (!stepRuntime_.done && timer.elapsed() < bleWaitMs) {
        waitWork(pollMs);
    }
    if (stepRuntime_.done) {
        return;
    }
    ++plcKeyBleWaitSeq_;
    freeWorkKeyWaiting_ = false;
    const int ph = plcSwitchBlePhase_;
    plcSwitchBlePhase_ = 0;
    stepRuntime_.done = true;
    stepRuntime_.pass = false;
    const QString plcPart = plcKeyBlePlcOkSummary_;
    plcKeyBlePlcOkSummary_.clear();
    QString tail = QStringLiteral("等待设备按键上报超时");
    if (ph == 3) {
        tail = QStringLiteral("等待左旋上报超时");
    } else if (ph == 4) {
        tail = QStringLiteral("等待右旋上报超时");
    }
    stepRuntime_.testData = plcPart.isEmpty() ? tail : plcPart + QStringLiteral("；") + tail;
    TestResult = failValue;
    showlog(currentKeyTestName_ + QStringLiteral("失败：%1").arg(tail));
}

void QFreeWork::armPlcBleKeyWaitTimeout() {
    const int bleWaitMs = SETTINGS.value(QStringLiteral("KeyTest/TimeoutMs"), 5000).toInt();
    const quint64 armSeq = ++plcKeyBleWaitSeq_;
    QTimer::singleShot(bleWaitMs, this, [this, armSeq]() {
        if (armSeq != plcKeyBleWaitSeq_) {
            return;
        }
        if (!freeWorkKeyWaiting_) {
            return;
        }
        const int ph = plcSwitchBlePhase_;
        closeKeyWaitPrompt();
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        const QString plcPart = plcKeyBlePlcOkSummary_;
        plcKeyBlePlcOkSummary_.clear();
        QString tail = QStringLiteral("等待设备按键上报超时");
        if (ph == 3) {
            tail = QStringLiteral("等待左旋上报超时");
        } else if (ph == 4) {
            tail = QStringLiteral("等待右旋上报超时");
        }
        stepRuntime_.testData = plcPart.isEmpty() ? tail : plcPart + QStringLiteral("；") + tail;
        TestResult = failValue;
        showlog(currentKeyTestName_ + QStringLiteral("失败：%1").arg(tail));
    });
    showlog(currentKeyTestName_ + QStringLiteral("：等待协议上报（超时 %1ms）").arg(bleWaitMs));
}

void QFreeWork::closeKeyWaitPrompt() {
    if (keyWaitPrompt_ != nullptr) {
        keyWaitPromptProgrammaticClose_ = true;
        keyWaitPrompt_->close();
        keyWaitPrompt_ = nullptr;
    }
}

void QFreeWork::showTestCasePromptForStep(const TestCaseDefinition& def) {
    closeTestCasePrompt();
    // Hook 步骤（按键/PLC 等）在钩子内自行弹窗，避免与 Meta/Prompt 重复叠两个框
    if (def.hook.enabled)
        return;
    if (!def.meta.promptEnabled)
        return;
    const QString text = def.meta.promptText.trimmed();
    if (text.isEmpty())
        return;
    const QString title = def.meta.name.trimmed();
    testCasePrompt_ = new QMessageBox(QMessageBox::Information, title, text, QMessageBox::Yes, this);
    if (QAbstractButton* yesBtn = testCasePrompt_->button(QMessageBox::Yes))
        yesBtn->setText(QStringLiteral("是"));
    testCasePrompt_->setDefaultButton(QMessageBox::Yes);
    testCasePrompt_->setAttribute(Qt::WA_DeleteOnClose);
    testCasePromptProgrammaticClose_ = false;
    applyTestItemPromptFont(testCasePrompt_);
    connect(testCasePrompt_, &QMessageBox::finished, this, [this](int) {
        if (!testCasePromptProgrammaticClose_)
            onTestCasePromptAcknowledged();
    });
    connect(testCasePrompt_, &QObject::destroyed, this, [this]() {
        testCasePrompt_ = nullptr;
        testCasePromptProgrammaticClose_ = false;
    });
    testCasePrompt_->show();
}

void QFreeWork::onTestCasePromptAcknowledged() {
    testCasePromptAcknowledged_ = true;
    if (!stepRuntime_.started || stepRuntime_.done || !testCaseStepActive_)
        return;
    if (!TestCaseRunner::stepWaitsForPromptAck(activeTestCase_))
        return;
    stepRuntime_.done = true;
    stepRuntime_.pass = true;
    if (stepRuntime_.testData == QLatin1String("-"))
        stepRuntime_.testData = QStringLiteral("已确认");
}

void QFreeWork::closeTestCasePrompt() {
    if (testCasePrompt_ == nullptr)
        return;
    QMessageBox* box = testCasePrompt_;
    testCasePrompt_ = nullptr;
    testCasePromptProgrammaticClose_ = true;
    box->close();
}

void QFreeWork::initData() {
    ui->product_sn->setText("芯片存储的整机sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;

    rssitestfailcount = 0;
    wifistate = 0;
    measure_ammeter = 0;
    dongleOutTime = 10;
    canGoNext = 1;
    stepRuntime_.reset();
    isovertime = 0;
    BT_RSSI = "";
    BLE_RSSI = "";
    WIFI_RSSI = "";
    softwareVersionForReport_.clear();
    softwareVersionPassForReport_ = true;
    freeWorkKeyWaiting_ = false;
    keyWaitPromptProgrammaticClose_ = false;
    ++plcKeyBleWaitSeq_;
    plcKeyBlePlcOkSummary_.clear();
    plcSwitchBlePhase_ = 0;
    plcKeyCapPollMode_ = false;
    plcKeyCapPassSummary_.clear();
    currentKeyCapRequestKk_ = -1;
    currentKeyConfiguredId_ = 0;
    resetPlcKeyCapSyncReadState();
    currentKeyTestName_.clear();
    currentKeyExpectedKey_.clear();
    closeKeyWaitPrompt();
    closeTestCasePrompt();
    lastBrushInstrumentProfile_ = -1;
    cmwFacade_.run(CmwGprfCommand::ResetSession, makeCmwRunParams());
    plcFacade_.disconnect();
    clearProductInstrumentWatch();
    is_battary_test = 0;
    charageresult = "未测";
    voltageresult = "未测";
    currentresult = "未测";
    pb->reset_all_pb();
    at->resetConnected();
    TestResult = passValue;
    wifiresult = "未测";
    bleresult = "未测";
    ui->battary_state->setText("充电状态为:");
    ui->battary_value->setText("电量为:");
    ui->battary_voltage->setText("电压为:");
    deviceTailSnFromDevice = "";
    tupleData_ = TupleApplyResult{};
    QTupleService::clearSharedSession();
    freeWorkMesSegments_.clear();
    TestTime.start();
}

PlcV3RunParams QFreeWork::makePlcRunParams(int keyIndex0To6) {
    PlcV3RunParams params;
    params.stationIndex = getIndex();
    params.keyIndex0To6 = keyIndex0To6;
    params.modbus = &modbusManager;
    params.log = [this](const QString& line) { showlog(line); };
    params.isTestContinue = [this]() { return isTestContinue; };
    return params;
}

CmwGprfRunParams QFreeWork::makeCmwRunParams(const QString& scenarioLabel, int brushProfile) {
    CmwGprfRunParams params;
    params.scpi = scpiVisaManager();
    params.scenarioLabel = scenarioLabel;
    params.brushProfile = brushProfile;
    params.log = [this](const QString& line) { showlog(line); };
    params.wait = [this](int ms) { waitWork(ms); };
    return params;
}

void QFreeWork::applyPlcStepResult(const PlcV3RunResult& result, PlcV3Command command, bool finishStepRuntime) {
    if (!result.ok) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = result.summary;
        stepRuntime_.done = true;
        if (!result.summary.isEmpty()) {
            showlog(result.summary);
        }
        return;
    }
    if (command == PlcV3Command::TouchSwitch) {
        return;
    }
    if (command == PlcV3Command::TouchKey) {
        stepRuntime_.pass = true;
        if (finishStepRuntime) {
            stepRuntime_.testData = result.summary;
            stepRuntime_.done = true;
            plcKeyBlePlcOkSummary_.clear();
        } else if (stepRuntime_.done) {
            if (!result.summary.isEmpty()) {
                stepRuntime_.testData = stepRuntime_.testData.isEmpty()
                    ? result.summary
                    : QStringLiteral("%1；%2").arg(result.summary, stepRuntime_.testData);
            }
            plcKeyBlePlcOkSummary_.clear();
        } else {
            stepRuntime_.testData = result.summary;
            stepRuntime_.done = false;
            plcKeyBlePlcOkSummary_ = result.summary;
        }
        if (!result.summary.isEmpty()) {
            showlog(result.summary);
        }
        return;
    }
    stepRuntime_.pass = true;
    stepRuntime_.testData = result.summary;
    stepRuntime_.done = true;
}

PlcV3RunResult QFreeWork::runPlcV3(PlcV3Command command, int keyIndex0To6, bool finishStepRuntime) {
    Q_UNUSED(finishStepRuntime);
    PlcV3RunParams params = makePlcRunParams(keyIndex0To6);
    if (command == PlcV3Command::TouchKey && plcKeyCapPollMode_) {
        params.pollCapDuringPress = [this](QString* errOut, QString* outSummary) {
            return pollKeyCapDuringPress(errOut, outSummary);
        };
    }
    // 业务层 plcFacade_（business/），与 cmwFacade_ 同级。
    return plcFacade_.run(command, params);
}

CmwGprfRunResult QFreeWork::runCmwGprf(CmwGprfCommand command, const QString& scenarioLabel, int brushProfile,
                                       int alignedPostTrigHoldMs, bool* outRanBurst) {
    CmwGprfRunParams params = makeCmwRunParams(scenarioLabel, brushProfile);
    params.alignedPostTrigHoldMs = alignedPostTrigHoldMs;
    params.outRanBurst = outRanBurst;
    return cmwFacade_.run(command, params);
}

void QFreeWork::runPlcSwitchTestDoneResetM() {
    const PlcV3RunResult result = runPlcV3(PlcV3Command::SwitchDoneReset);
    applyPlcStepResult(result, PlcV3Command::SwitchDoneReset);
}

void QFreeWork::runPlcModbusConnectTest() {
    const PlcV3RunResult result = runPlcV3(PlcV3Command::ModbusConnectTest);
    applyPlcStepResult(result, PlcV3Command::ModbusConnectTest);
}

void QFreeWork::runPlcV3TouchKeyFull(int keyIndex0To6, bool finishStepRuntime) {
    const PlcV3RunResult result = runPlcV3(PlcV3Command::TouchKey, keyIndex0To6, finishStepRuntime);
    applyPlcStepResult(result, PlcV3Command::TouchKey, finishStepRuntime);
}

void QFreeWork::runPlcV3TouchSwitchFull(bool finishStepRuntime) {
    Q_UNUSED(finishStepRuntime);
    const PlcV3RunResult result = runPlcV3(PlcV3Command::TouchSwitch);
    applyPlcStepResult(result, PlcV3Command::TouchSwitch, finishStepRuntime);
}

bool QFreeWork::runFreeInstrumentBleCmwBurstForBrushProfile(QString* detail, int brushProfile) {
    const CmwGprfRunResult result =
        runCmwGprf(CmwGprfCommand::ParallelBurstForProfile, QString(), brushProfile);
    if (detail) {
        *detail = result.detail;
    }
    return result.ok || result.skipped;
}

void QFreeWork::on_disconnectwifi_clicked() {
    if (at->getConnected()) {
        protocolManager.set(DeviceCmd::WifiDisconnect);
        showlog("已发送断开wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}
void QFreeWork::on_connectwifi_clicked() {
    QString wifiName = SETTINGS.value(QString("WIFI/Name%1").arg(getIndex())).toString();
    QString wifiPassword = SETTINGS.value("WIFI/Password").toString();

    // QString wifiName = SETTINGS.value("WIFI/Name").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected()) {
        protocolManager.set(DeviceCmd::WifiConnect, QVariant::fromValue(WifiConnectPayload{wifiNameBytes, wifiPasswordBytes}));
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}

void QFreeWork::on_macInput_returnPressed() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    if (pack.factory == "lx" || pack.factory == "jj") {
        if (!usbSerialPort->isOpen()) {
            openUsbSerialPort();
        }
    }

    // 检查是否是mac格式
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        if (ui->just_banding->checkState()) {
            bindingMacSn(macAddress, ui->getMac->text()); //获取测试数据不要绑定测试mac——sn
            ui->test_result->setText("PASS");
            ui->test_result->setStyleSheet(
                "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                "border-radius: 10px; padding: 10px; text-align: center;");
            ui->getMac->clear();
            ui->getMac->setFocus();

            return;
        }

        ui->macLabel->setText("蓝牙mac: " + macAddress);

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: "
                                       "10px; padding: 10px; text-align: center; ");

        isTestContinue = true;
        teststate = -1;

        emit send_go_next_focus();
        ui->getMac->setDisabled(1);
        ui->macInput->setDisabled(1);
    }
}

void QFreeWork::on_pushButton_clicked() {
    runPlcModbusConnectTest();
}

void QFreeWork::on_pushButton_2_clicked() {
    at->set(DongleCmd::BleLog, 1); // 日志开
}

void QFreeWork::on_getMac_returnPressed() {
    testResultTableInit();

    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");
    applyAdaptiveV3ProductBySn(ui->getMac);

    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->getMac->text()).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->getMac->text().length()));
        showlog("要求格式为" + snPattern);
        ui->getMac->clear();
        return;
    }
    if (isBydFactory()) {
        expectedTailSnFromMes.clear();
    }
    showlog("正在查询mac地址");
    processGetMesTestValue(); // mes获取
    // getMac(ui->getMac->text());             // 文件获取
    if (ui->isusemes->checkState()) {
        processInspection(ui->getMac->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
}

void QFreeWork::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = inputSnText;

            pack.mechines = getIndex();

            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit send_process_inspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState()) // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void QFreeWork::processGetMesTestValue() {
    if (pack.factory == "hz") {
        pack.sn = ui->getMac->text();
        pack.mechines = getIndex();
        getTestValue(getIndex(), pack.sn.trimmed());
        return;
    }
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit send_mes_test_value(pack);
    }
}
void QFreeWork::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");             // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) { // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                     // 逐行读取文件
            QString line = in.readLine();         // 读取一行
            QStringList fields = line.split(","); // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {
                QString sn = fields.at(0);  // 第一个字段是sn
                QString mac = fields.at(1); // 第二个字段是mac
                if (sn == sn_to_search) {   // 检查是否是待检索的sn
                    {
                        ui->macInput->setText(mac);
                        on_macInput_returnPressed();
                        showlog("这是从文件获取的mac地址");
                        qDebug() << getIndex() << "The corresponding mac is: " << mac;
                    }

                    break;
                }
            }
        }

        file.close(); // 关闭文件
    }
    if (!ui->isformmes->isChecked() && ui->macInput->text().isEmpty()) {
        ui->getMac->clear();
        showlog("找不到mac地址，清空当前输入的sn");
    }
}
void QFreeWork::getMacAddress(const QByteArray& byte) {
    receivedData = "";
    receivedData = receivedData + QString::fromUtf8(byte);

    if (receivedData.length() >= 2 && receivedData.right(2) == "\r\n") {
        // 使用正则表达式提取设备信息
        static const QRegularExpression regex("deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
        QRegularExpressionMatch match = regex.match(receivedData);
        QString deviceName, deviceAddress, deviceRssi;

        // qDebug() << getIndex()<< "数据长度" << receivedData;
        // 检查是否匹配成功
        if (match.hasMatch()) {
            deviceName = match.captured(1).trimmed();
            deviceAddress = match.captured(2).trimmed();
            deviceRssi = match.captured(3).trimmed();
            // qDebug() << getIndex()<< "设备名称：" << deviceName;
            // qDebug() << getIndex()<< "设备地址：" << deviceAddress;
            // qDebug() << getIndex()<< "设备RSSI：" << deviceRssi;

            deviceMap[deviceAddress]["Name"] = deviceName;
            deviceMap[deviceAddress]["Rssi"] = deviceRssi;

            updateComboBox();
        }
        receivedData.clear();
    }
}
void QFreeWork::on_clear_scan_clicked() {
    deviceMap.clear();
    ui->mac_combo->clear();
}
void QFreeWork::updateComboBox() {
    // 遍历设备信息，根据 rssi 的值进行过滤
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        QString deviceAddress = it.key();
        QString deviceName = it.value()["Name"];
        QString deviceRssi = it.value()["Rssi"];

        if (deviceName.contains(ui->name_range->text()) && deviceRssi.toInt() > ui->rssi_range->text().toInt() &&
            deviceAddress.length() == 17)

        {
            int index = ui->mac_combo->findText(deviceAddress);
            qDebug() << getIndex() << "信号强度:" << deviceRssi;
            if (index == -1) {
                ui->mac_combo->addItem(deviceAddress);
                qDebug() << getIndex() << "有新增" << deviceAddress;
            }
        }
    }
}
void QFreeWork::on_mac_combo_textActivated(const QString& arg1) {
    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = arg1;
        // at->set(DongleCmd::BleScanConnect, macAddress);//发送mac地址
        qDebug() << getIndex() << macAddress;
        bindingMacSn(macAddress, snBinding);
    }
    ui->snbanding->setFocus();
}
void QFreeWork::bindingMacSn(QString bindingMac, QString bindingSn) {
    if (bindingSn == "" || bindingMac == "")
        bindingResult = false;

    // 将网络路径转换为 QFile 能够处理的格式
    QString path;
    if (pack.factory == "xwd")
        path = "\\\\172.30.189.83\\sgpub\\LTC\\MAC\\mac_sn.txt";

    else
        path = "mac_sn.txt";

    // 在 Windows 上，使用 QDir::fromNativeSeparators 将路径中的反斜杠转换为正斜杠
    // path = QDir::fromNativeSeparators(path);
    QFile file(path); // 创建一个文件对象

    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) { //
        QTextStream in(&file);                               // 创建一个文本流对象
        QStringList lines;                                   // 用于存储文件中的每一行数据
        bool found = false;                                  // 标记是否找到了相同的SN
        while (!in.atEnd()) {
            QString line = in.readLine();        // 逐行读取文件
            QStringList parts = line.split(","); // 以逗号分隔每行数据
            if (parts.size() == 2 && parts[0].trimmed() == bindingSn) {
                // 如果找到了相同的SN，替换MAC地址
                lines << (bindingSn + "," + bindingMac);
                found = true;
            } else {
                // 否则，保留原有数据
                lines << line;
            }
        }
        if (!found) {
            // 如果没有找到相同的SN，则追加新的SN和MAC地址
            lines << (bindingSn + "," + bindingMac);
        }
        // 清空文件并写入新的数据
        file.resize(0);
        QTextStream out(&file);
        for (const QString& line : lines) {
            out << line << '\n';
        }
        file.close(); // 关闭文件
        showlog("保存mac_sn文件成功");
    } else {
        showlog("保存mac_sn文件失败");
    }

    // bindingMacSnMes(bindingMac, bindingSn);
}

void QFreeWork::bindingMacSnMes(QString bindingMac, QString bindingSn) {
    Q_UNUSED(bindingSn);
    pack.mechines = 1; // 1脱1,1号上位机
    pack.sn = snBinding;
    pack.result = "PASS";
    pack.itemvalue = QString("|BTMAC:%1|").arg(bindingMac);
    pack.instruct_num = "076";
    if (ui->isusemes->checkState()) {
        emit send_end_test_pass(pack);
    }

    if (bindingResult) {
        ui->banding_result->setText("绑定:PASS");
        ui->banding_result->setStyleSheet("font-size: 33px; background-color: #00FF00; color: black; border: 2px solid "
                                          "#00FF00; border-radius: 10px; padding: 10px; text-align: center;");
    } else {
        ui->banding_result->setText("绑定:NG");
        ui->banding_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                          "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
    ui->snbanding->setFocus();
}
void QFreeWork::on_snbanding_returnPressed() {
    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    snBinding = ui->snbanding->text();
    at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00"); // 发送mac地址
    ui->snbanding->clear();
    bindingResult = true;
}

void QFreeWork::getTestValue(const int mechines, const QString value) {
    // showlog(value);
    QString mesmacAddress;
    if (pack.factory == "hq") {
        // 定义正则表达式，匹配MAC地址的模式
        QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");

        // 在数据中查找匹配的内容
        QRegularExpressionMatch match = regex.match(value);

        // 检查是否有匹配项
        if (match.hasMatch()) {
            // 提取MAC地址
            mesmacAddress = match.captured(1);
            qDebug() << getIndex() << "MAC地址：" << mesmacAddress;
            if (mechines == getIndex()) {
                ui->macInput->setText(mesmacAddress);
                on_macInput_returnPressed();
            }
        } else {
            showlog("mes未找到匹配的MAC地址");
            showlog(value);
        }
    }
    // showlog(value);
    else if (pack.factory == "lx") {
        mesmacAddress = value;

        // 在2、4、6、8、10的位置插入冒号
        mesmacAddress.insert(2, ":");
        mesmacAddress.insert(5, ":");
        mesmacAddress.insert(8, ":");
        mesmacAddress.insert(11, ":");
        mesmacAddress.insert(14, ":");

        // 将小写字母转换成大写字母
        mesmacAddress = mesmacAddress.toUpper();
        if (mechines == getIndex()) {
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    } else if (pack.factory == "hz") {
        if (mechines != getIndex()) {
            return;
        }
        const QString snFromMes = value.trimmed();
        mesmacAddress = parseMacFromSn(snFromMes);
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            showlog(value);
            return;
        }
        expectedTailSnFromMes = snFromMes.toUtf8();
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        // on_macInput_returnPressed();
    } else if (pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0) {
        // BYD MES 回调为整机 SN（如主板绑定行的 value），与 on_getMac_returnPressed 一致用 parseMacFromSn 取蓝牙 MAC
        if (mechines != getIndex()) {
            return;
        }
        const QString snFromMes = value.trimmed();
        mesmacAddress = parseMacFromSn(snFromMes);
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            showlog(value);
            return;
        }
        // BYD 写入/读回 SN 均用 MES 返回的整机 SN（同 ageing writesn/stringsn）
        expectedTailSnFromMes = snFromMes.toUtf8();
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        on_macInput_returnPressed();
    } else {
        if (mechines == getIndex()) {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }
}

void QFreeWork::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void QFreeWork::on_disconnectButton_clicked() {
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}

void QFreeWork::on_jigConnectButton_clicked() {
    openJigSerialPort();
    ui->jigComNameCombo->setEnabled(false);
    ui->jigConnectButton->setEnabled(false);
}

void QFreeWork::on_jigDisconnectButton_clicked() {
    closeJigSerialPort();
    ui->jigComNameCombo->setEnabled(true);
    ui->jigConnectButton->setEnabled(true);
}

void QFreeWork::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void QFreeWork::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void QFreeWork::clearProductInstrumentWatch() {
    disconnect(productInstConn_);
    productInstConn_ = QMetaObject::Connection();
    productInstrumentStopWaitStepName_.clear();
}

bool QFreeWork::ensureProductSerialForInstrumentStep(const QString& stepName) {
    if (!product) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("Qproduct未初始化");
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：Qproduct未初始化"));
        return false;
    }
    QComboBox* const prodCombo = getProductcomNameCombo();
    if (!prodCombo || prodCombo->currentText().trimmed().isEmpty()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("未选择产品串口COM");
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：请先在「产品串口(仪器)」下拉框选择 COM 口"));
        return false;
    }
    // 仪器步骤依赖产品串口：未打开时走与手动点「连接串口」同一槽，避免与界面状态不一致
    if (!productSerialPort || !productSerialPort->isOpen()) {
        showlog(stepName + QStringLiteral("：正在打开产品串口…"));
        on_productConnectButton_clicked();
        if (productSerialPort && productSerialPort->isOpen()) {
            if (product) {
                product->clearProductSerialRxAccum();
            }
            showlog(QStringLiteral("产品串口已打开：%1").arg(prodCombo->currentText()));
        } else {
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("产品串口打开失败");
            TestResult = failValue;
            showlog(stepName + QStringLiteral("失败：无法打开产品串口，请检查端口占用或未插入"));
            return false;
        }
    }
    return true;
}

QByteArray QFreeWork::brushInstrumentStartCmdForProfile(int profile) {
    switch (profile) {
    case 1:
        return Qproduct::buildStartReceiveCmd2440Ble1M();
    case 2:
        return Qproduct::buildStartReceiveCmd2480Ble1M();
    case 3:
        return Qproduct::buildStartReceiveCmd2402Ble2M();
    case 4:
        return Qproduct::buildStartReceiveCmd2440Ble2M();
    case 5:
        return Qproduct::buildStartReceiveCmd2480Ble2M();
    case 0:
    default:
        return Qproduct::buildStartReceiveCmd2402Ble1M();
    }
}

void QFreeWork::startProductInstrumentResetAndWaitAck(QString stepNameIn) {
    QString stepName = stepNameIn.trimmed();
    if (stepName.isEmpty()) {
        stepName = testCaseStepActive_ ? activeTestCaseStepLabel_
                                       : QStringLiteral("产品串口仪器复位应答");
    }
    if (stepName.isEmpty())
        stepName = QStringLiteral("产品串口仪器复位应答");
    clearProductInstrumentWatch();
    if (!ensureProductSerialForInstrumentStep(stepName)) {
        return;
    }
    product->clearProductSerialRxAccum();
    QString err;
    if (!product->writeRaw(Qproduct::cmdReset(), &err)) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = err;
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：写串口 ") + err);
        return;
    }
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("等待040E0405030C00");

    productInstConn_ = connect(product, &Qproduct::instrumentAckResetSeen, this, [this, stepName]() {
        if (!isCurrentInstrumentStep(stepName)) {
            return;
        }
        if (stepRuntime_.done) {
            return;
        }
        disconnect(productInstConn_);
        productInstConn_ = QMetaObject::Connection();
        stepRuntime_.done = true;
        stepRuntime_.pass = true;
        stepRuntime_.testData = QStringLiteral("040E0405030C00");
        showlog(stepName + QStringLiteral("通过"));
    });
}

void QFreeWork::startProductInstrumentStartReceiveForCatalog(const QString& stepName, int profile) {
    clearProductInstrumentWatch();
    if (!ensureProductSerialForInstrumentStep(stepName)) {
        return;
    }
    const QByteArray frame = brushInstrumentStartCmdForProfile(profile);
    lastBrushInstrumentProfile_ = profile;
    cmwFacade_.clearBurstDoneSinceStartRx();
    QString err;
    if (!product->writeRaw(frame, &err)) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = err;
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：写串口 ") + err);
        return;
    }
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("Profile=%1 等待040E0405332000").arg(profile);

    productInstConn_ = connect(product, &Qproduct::instrumentAckStartReceiveSeen, this, [this, stepName]() {
        if (!isCurrentInstrumentStep(stepName)) {
            return;
        }
        if (stepRuntime_.done) {
            return;
        }
        disconnect(productInstConn_);
        productInstConn_ = QMetaObject::Connection();
        stepRuntime_.done = true;
        stepRuntime_.pass = true;
        stepRuntime_.testData = QStringLiteral("040E0405332000");
        showlog(stepName + QStringLiteral("通过"));
    });
}

void QFreeWork::startProductInstrumentStopReceiveAndPer(QString stepNameIn) {
    QString stepName = stepNameIn.trimmed();
    if (stepName.isEmpty()) {
        stepName = testCaseStepActive_ ? activeTestCaseStepLabel_
                                       : QStringLiteral("产品串口停止接收与PER");
    }
    if (stepName.isEmpty())
        stepName = QStringLiteral("产品串口停止接收与PER");
    clearProductInstrumentWatch();
    if (!ensureProductSerialForInstrumentStep(stepName)) {
        return;
    }
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    const int waitPacketMs = SETTINGS.value(QStringLiteral("BrushInstrument/PacketPhaseWaitMs"), 2000).toInt();
    const int stopAckTimeout = SETTINGS.value(QStringLiteral("BrushInstrument/StopAckTimeoutMs"), 5000).toInt();

    product->clearProductSerialRxAccum();

    const QString wfPath = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (wfPath.isEmpty()) {
        showlog(stepName +
                QStringLiteral("：BlePer/CmwWaveformFile 为空，并联 CMW 使用仪侧 ARB（若未就绪请配置该键）"));
    } else {
        showlog(stepName + QStringLiteral("：BlePer/CmwWaveformFile=%1").arg(wfPath));
    }

    bool ranCmwBurst = false;
    const CmwGprfRunResult cmwResult =
        runCmwGprf(CmwGprfCommand::BrushBurstOnStopPer, stepName, lastBrushInstrumentProfile_, waitPacketMs, &ranCmwBurst);
    if (!cmwResult.ok) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = cmwResult.detail;
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：CMW GPRF——") + cmwResult.detail);
        return;
    }

    const int delayBeforeStopMs = ranCmwBurst ? 0 : waitPacketMs;
    showlog(stepName +
            QStringLiteral("：写停止接收前延时 %1 ms（BrushInstrument/PacketPhaseWaitMs=%2；已实际打并联 CMW 突发则不再追加积包延时）")
                .arg(delayBeforeStopMs)
                .arg(waitPacketMs));

    QTimer::singleShot(delayBeforeStopMs, this, [this, stepName, stopAckTimeout]() {
        if (!isCurrentInstrumentStep(stepName)) {
            return;
        }
        QString err;
        if (!product || !product->writeRaw(Qproduct::cmdStopReceive(), &err)) {
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = err.isEmpty() ? QStringLiteral("写停止接收失败") : err;
            TestResult = failValue;
            showlog(stepName + QStringLiteral("失败：写停止 ") + stepRuntime_.testData);
            return;
        }
        // 应答由构造函数中 connect 的 onProductInstrumentStopReceiveAckForPer 处理，此处仅登记当前步骤名
        productInstrumentStopWaitStepName_ = stepName;
        QTimer::singleShot(stopAckTimeout, this, [this, stepName]() {
            if (!isCurrentInstrumentStep(stepName)) {
                return;
            }
            if (stepRuntime_.done) {
                return;
            }
            productInstrumentStopWaitStepName_.clear();
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("超时未收到停止接收应答");
            TestResult = failValue;
            showlog(stepName + QStringLiteral("失败：等待停止应答超时"));
        });
    });
}

void QFreeWork::on_stopTest_clicked() {
    clearProductInstrumentWatch();
    plcFacade_.disconnect();
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

void QFreeWork::resetPlcKeyCapSyncReadState() {
    plcKeyCapSyncReadPending_ = false;
    plcKeyCapSyncReadOk_ = false;
    plcKeyCapSyncReadValue_ = 0;
    plcKeyCapSyncReadAuxId_ = -1;
}

bool QFreeWork::pollKeyCapDuringPress(QString* errOut, QString* outSummary) {
    const int kk = currentKeyCapRequestKk_;
    const int configuredKeyId = currentKeyConfiguredId_;
    const int readCount = qMax(1, SETTINGS.value(QStringLiteral("KeyCap/ReadCount"), 3).toInt());
    const int intervalMs = qMax(0, SETTINGS.value(QStringLiteral("KeyCap/ReadIntervalMs"), 80).toInt());
    const int singleTimeoutMs = qMax(500, SETTINGS.value(QStringLiteral("KeyCap/SingleReadTimeoutMs"), 2000).toInt());

    quint32 bestCap = 0;
    QStringList sampleTexts;

    for (int i = 0; i < readCount; ++i) {
        resetPlcKeyCapSyncReadState();
        plcKeyCapSyncReadPending_ = true;
        QVariantMap m;
        m[QStringLiteral("key")] = kk;
        protocolManager.get(DeviceCmd::KeySignalRead, m);

        QElapsedTimer timer;
        timer.start();
        while (plcKeyCapSyncReadPending_ && timer.elapsed() < singleTimeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
            QThread::msleep(20);
        }

        if (plcKeyCapSyncReadPending_) {
            if (errOut) {
                *errOut = QStringLiteral("第%1/%2次读电容超时(%3ms)")
                              .arg(i + 1)
                              .arg(readCount)
                              .arg(singleTimeoutMs);
            }
            return false;
        }
        if (!plcKeyCapSyncReadOk_) {
            if (errOut) {
                *errOut = QStringLiteral("第%1/%2次读电容应答无效").arg(i + 1).arg(readCount);
            }
            return false;
        }
        if (plcKeyCapSyncReadAuxId_ != kk) {
            if (errOut) {
                *errOut = QStringLiteral("按键编号不一致：请求KK=%1 应答KK=%2")
                              .arg(kk)
                              .arg(plcKeyCapSyncReadAuxId_);
            }
            return false;
        }

        bestCap = qMax(bestCap, plcKeyCapSyncReadValue_);
        sampleTexts.append(QString::number(plcKeyCapSyncReadValue_));
        showlog(currentKeyTestName_ + QStringLiteral("：第%1/%2次读电容 KK=%3 值=%4").arg(i + 1).arg(readCount).arg(kk).arg(plcKeyCapSyncReadValue_));

        if (i + 1 < readCount && intervalMs > 0) {
            QThread::msleep(static_cast<unsigned long>(intervalMs));
        }
    }

    const QString expectedKeyId = QString::number(configuredKeyId);
    const bool idOk = compareVersions(expectedKeyId, QString::number(kk + 1));
    const bool capOk = (bestCap >= lowKeyCap_) && (bestCap <= highKeyCap_);
    const QString capAsk = QStringLiteral("[%1,%2]").arg(lowKeyCap_).arg(highKeyCap_);

    stepRuntime_.ask = capAsk;
    const QString summary = QStringLiteral("KK:%1 采样[%2] 最大:%3 ID:%4 期望ID:%5")
                                .arg(kk)
                                .arg(sampleTexts.join(QLatin1Char(',')))
                                .arg(bestCap)
                                .arg(kk + 1)
                                .arg(expectedKeyId);
    if (outSummary) {
        *outSummary = summary;
    }
    stepRuntime_.testData = summary;

    if (!idOk) {
        if (errOut) {
            *errOut = QStringLiteral("按键ID与配置不符，KK=%1 期望ID=%2").arg(kk).arg(expectedKeyId);
        }
        TestResult = failValue;
        showlog(currentKeyTestName_ + QStringLiteral("失败：") + *errOut);
        return false;
    }
    if (!capOk) {
        if (errOut) {
            *errOut = QStringLiteral("电容卡控失败，最大=%1 允许%2").arg(bestCap).arg(capAsk);
        }
        TestResult = failValue;
        showlog(currentKeyTestName_ + QStringLiteral("卡控失败，采样[%1] 最大=%2 允许%3").arg(sampleTexts.join(QLatin1Char(','))).arg(bestCap).arg(capAsk));
        return false;
    }

    showlog(currentKeyTestName_ + QStringLiteral("卡控通过，KK=%1 最大电容=%2").arg(kk).arg(bestCap));
    return true;
}

void QFreeWork::startPlcSwitchPlcAndWaitRightRotate() {
    const QString rightEn = QStringLiteral("ProductInfo/KeyIdRightRotate_checkBox");
    if (!SETTINGS.value(rightEn).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("右旋按键配置未启用");
        stepRuntime_.ask = QStringLiteral("请检查配置");
        TestResult = failValue;
        showlog(QStringLiteral("PLC+V3旋钮右旋失败：右旋配置未启用"));
        return;
    }

    // phase 4：PLC 旋钮整步后对编码器「右旋」dir=2 校验（qfctp ENCODER_STATUS_REPORT）。
    plcSwitchBlePhase_ = 4;
    currentKeyTestName_ = QStringLiteral("PLC+V3旋钮右旋");
    currentKeyExpectedKey_ = QStringLiteral("ProductInfo/KeyIdRightRotate");
    freeWorkKeyWaiting_ = true;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("PLC旋钮整步与等待右旋上报");
    stepRuntime_.ask = SETTINGS.value(currentKeyExpectedKey_).toString();
    plcKeyBlePlcOkSummary_.clear();

    closeKeyWaitPrompt();
    showlog(QStringLiteral("PLC+V3旋钮右旋：治具将自动完成旋钮动作，等待设备上报右旋"));

    runPlcV3TouchSwitchFull(false);

    if (!stepRuntime_.pass) {
        ++plcKeyBleWaitSeq_;
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        closeKeyWaitPrompt();
        plcKeyBlePlcOkSummary_.clear();
        return;
    }

    if (stepRuntime_.done) {
        ++plcKeyBleWaitSeq_;
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        return;
    }

    waitPlcBleKeyReportBlocking();
}