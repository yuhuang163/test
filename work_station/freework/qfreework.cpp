#include "qfreework.h"

#include <algorithm>
#include <functional>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <QSet>
#include <QStringList>
#include <QRegularExpression>
#include <QThread>
#include <QtGlobal>
#include "qproduct.h"
#include "ui_qfreework.h"

/** 本轮测试内是否已对 CMW 做过 GPRF 波形侧初始化（与 BlePer/CmwEnableFixedInit 一致）。 */
static bool gInstrumentCmwGprfPrimed = false;

namespace {

/** Ini：BlePer/CmwVisaTrace，默认 true；打印 [CMW-VISA] >> / << 收发（仅自由工站 CMW 封装路径）。 */
bool freeWorkShouldLogCmwVisaIo() {
    return SETTINGS.value(QStringLiteral("BlePer/CmwVisaTrace"), true).toBool();
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

/** 解析 productKey / deviceName / deviceSecret 各占一条，避免整包塞进一个 value。 */
static bool tryAppendAliTupleFields(QVector<QPair<QString, QString>>* out, const QString& pkKey, const QString& dnKey, const QString& skKey,
                                    const QString& blob) {
    static const QRegularExpression re(
        QStringLiteral(R"(productKey\s*:\s*(\S+)\s+deviceName\s*:\s*(\S+)\s+deviceSecret\s*:\s*(\S+))"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(blob.trimmed());
    if (!m.hasMatch())
        return false;
    pushMesSeg(out, pkKey, m.captured(1));
    pushMesSeg(out, dnKey, m.captured(2));
    pushMesSeg(out, skKey, m.captured(3));
    return true;
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

}  // namespace

static int brushProfileToBleCmwMHz(int profile) {
    switch (profile) {
        case 1:
        case 4:
            return 2440;
        case 2:
        case 5:
            return 2480;
        case 0:
        case 3:
        default:
            return 2402;
    }
}

static bool parseBlePerCmwArbScountFree(const QString& response, double* countTime, int* cycles, int* samplesCurrent) {
    const QString clean = response.trimmed().remove(QLatin1Char('"'));
    const QStringList parts = clean.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return false;
    }
    bool timeOk = false;
    bool cyclesOk = false;
    bool samplesOk = false;
    const double parsedTime = parts.at(0).trimmed().toDouble(&timeOk);
    const int parsedCycles = parts.at(1).trimmed().toInt(&cyclesOk);
    const int parsedSamples = parts.at(2).trimmed().toInt(&samplesOk);
    if (!timeOk || !cyclesOk || !samplesOk) {
        return false;
    }
    if (countTime) {
        *countTime = parsedTime;
    }
    if (cycles) {
        *cycles = parsedCycles;
    }
    if (samplesCurrent) {
        *samplesCurrent = parsedSamples;
    }
    return true;
}

void QFreeWork::appendFreeWorkMesForCompletedStep(const NamedFunction& nf, bool pass, const QString& testData) {
    QVector<QPair<QString, QString>>* const out = &freeWorkMesSegments_;
    const int functionId = nf.id;
    const bool hasData = !testData.trimmed().isEmpty() && testData != QStringLiteral("-");
    // 默认 MES 键与 testFunction.cpp 中 FREEWORK_TEST_LIST 本行的 mesTag 一致
    const QString tag = nf.mesTag.isEmpty() ? QStringLiteral("STEP_%1").arg(functionId) : nf.mesTag;

    switch (functionId) {
    case 62:
        if (pass && hasData && tryAppendAliTupleFields(out, QStringLiteral("CLOUD_PRODUCT_KEY"), QStringLiteral("CLOUD_DEVICE_NAME"),
                                                        QStringLiteral("CLOUD_DEVICE_SECRET"), testData))
            return;
        appendOneMesStep(out, tag, pass, testData);
        return;
    case 66:
        if (pass && hasData && tryAppendAliTupleFields(out, QStringLiteral("READ_PRODUCT_KEY"), QStringLiteral("READ_DEVICE_NAME"),
                                                        QStringLiteral("READ_DEVICE_SECRET"), testData))
            return;
        appendOneMesStep(out, tag, pass, testData);
        return;
    case 8:
        pushMesSeg(out, QStringLiteral("BASE_INFO_RESULT"), pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
        if (hasData)
            pushMesSeg(out, QStringLiteral("BASE_INFO_DETAIL"), testData);
        // refreshBaseData 将 stepRuntime_.testData 置为 "-"，版本单独来自协议回填成员
        {
            const QString sv = softwareVersionForReport_.trimmed();
            if (!sv.isEmpty())
                pushMesSeg(out, QStringLiteral("SOFTWARE_VERSION"), sv);
            if (SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool())
                pushMesSeg(out, QStringLiteral("SOFTWARE_VERSION_CHECK"),
                           softwareVersionPassForReport_ ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
        }
        return;
    case 9: {
        // 获取电量信息：结果 PASS/FAIL 与百分比分两段，value 全 ASCII
        pushMesSeg(out, tag, pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
        if (hasData) {
            pushMesSeg(out, QStringLiteral("BATTERY_PERCENT"), testData);
        }
        return;
    }
    default:
        appendOneMesStep(out, tag, pass, testData);
        return;
    }
}

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif



void QFreeWork::on_get_battery_clicked() {
    // if (at->getConnected()) {
    //     protocolManager.get(DeviceCmd::GetBattery);
    //     showlog("正在获取设备电量");
    // } else {
    //     showlog("请等待连接设备后再试");
    // }
     startPlcKeyButtonTest("PLC+V3模式键", "治具将自动按压模式键，请确认设备按键上报", "ProductInfo/KeyIdMode", "ProductInfo/KeyIdMode_checkBox", 0);
    // runPlcV3TouchKeyFull(0, false);

    // runPlcV3TouchKeyFull(1, false);


    // runPlcV3TouchKeyFull(2, false);

}
void QFreeWork::on_start_wifible_test_clicked()
{


    runPlcV3TouchKeyFull(1, false);
    // const QString host = resolvedPlcIpAddress();
    // const int port = resolvedPlcPort();
    // const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    // QString err;
    // showlog(QStringLiteral("PLC按键整步连接: 键Index=%1 工位=%2 IP=%3 Port=%4 UnitId=%5")
    //             .arg(0)
    //             .arg(getIndex())
    //             .arg(host)
    //             .arg(port)
    //             .arg(resolvedPlcUnitId()));
    // if (!inovancePlcTcp_.connectPlc(host, quint16(port), resolvedPlcUnitId(), connMs, &err)) {
    //     showlog(QStringLiteral("PLC 连接失败: %1").arg(err));
    //     return;
    // }

    // if (!plcSendStepDone(&err)) {
    //     showlog(QStringLiteral("发送 StepDone 失败: %1").arg(err));
    //     return;
    // }
}

void QFreeWork::on_pushButton_clicked() {
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    // // ui->macInput->setText("3c:84:27:06:f7:5e");
    // ui->macInput->setText("3C:84:27:07:A8:D2");
    // // // ui->macInput->setText("3c:84:27:29:50:32");
    // ui->macInput->setText("b4:56:5d:bf:57:9d");

    // on_macInput_returnPressed();
    // // usb-> getlxMEASure();
    // // waitWork(1000);

    // showlog("正在获取设备电量");
    // ui->comNameCombo->setCurrentText("COM134");

    // debugUpdateTupleMacStatus();
    // applyTupleByMac();
    // runPlcModbusConnectTest();
    // startPlcKeyButtonTest("PLC+V3模式键", "治具将自动按压模式键，请确认设备按键上报", "ProductInfo/KeyIdMode", "ProductInfo/KeyIdMode_checkBox", 0);
    // startProductInstrumentResetAndWaitAck();

    runPlcModbusConnectTest();
}
namespace {
QString orderGroupName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    return key.isEmpty() ? "TestOrder_default" : QString("TestOrder_%1").arg(key);
}

QString plcBoolText(bool on) {
    return on ? QStringLiteral("开启") : QStringLiteral("关闭");
}
}

QFreeWork::QFreeWork(int index, QWidget* parent) : test_base(parent), ui(new Ui::QFreeWork) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = FREE_VER;
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();  // 要搜索一下一开始
    // connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
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

    measure_wait_time = SETTINGS.value("Current/measure_wait_time").toInt();

    RssiTestTime = SETTINGS.value("BLE/RssiCount").toInt();
    // ui->wifiUserName->setText(SETTINGS.value("WIFI/Name", "请在配置文件中设置").toString());
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

    createTestFunctions();
    refreshOrderedTestIndexes();
    testResultTableInit();
    if (product) {
        connect(product, &Qproduct::instrumentStopReceiveSeen, this, &QFreeWork::onProductInstrumentStopReceiveAckForPer);
    }
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
    // 隐藏第 2、3 页（待拓展 / 蓝牙绑定）；Qt 5.15+ 标签栏一并隐藏
    // ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(ui->tab_2), false);
    // ui->tabWidget->setTabVisible(ui->tabWidget->indexOf(ui->tab_3), false);
}
void QFreeWork::refreshOrderedTestIndexes() {
    const QString stationName = SETTINGS.value("TestOrderMeta/SelectedStationName").toString().trimmed();
    const QString tabName = stationName.isEmpty() ? "自由工站" : stationName;
    ui->tabWidget->setTabText(0, tabName);
    qDebug() << "[FreeWork] refresh tab, SelectedStationName =" << stationName << ", tabName =" << tabName;

    orderedTestIndexes_.clear();
    QSet<int> validIds;
    for (const auto& testFunction : testFunctions) {
        validIds.insert(testFunction.id);
    }
    const QVector<int> indexes = loadIndexesFromConfig();
    for (int index : indexes) {
        if (validIds.contains(index) && !orderedTestIndexes_.contains(index)) {
            orderedTestIndexes_.append(index);
        }
    }
    if (orderedTestIndexes_.isEmpty()) {
        for (const auto& testFunction : testFunctions) {
            orderedTestIndexes_.append(testFunction.id);
        }
    }
}

QVector<int> QFreeWork::loadIndexesFromConfig() {
    QVector<int> indexes;
    const QString selectedStation = SETTINGS.value("TestOrderMeta/SelectedStation").toString().trimmed();
    qDebug() << "[FreeWork] loadIndexesFromConfig, SelectedStationName =" << selectedStation;

    auto loadFromGroup = [&indexes](const QString& groupPath) {
        SETTINGS.beginGroup(groupPath);
        QStringList keys = SETTINGS.childKeys();
        std::sort(keys.begin(), keys.end(), [](const QString& left, const QString& right) { return left.toInt() < right.toInt(); });
        for (const QString& key : keys) {
            indexes.append(SETTINGS.value(key).toInt());
        }
        SETTINGS.endGroup();
    };

    if (!selectedStation.isEmpty()) {
        loadFromGroup(orderGroupName(selectedStation));
    }
    if (indexes.isEmpty() && !selectedStation.isEmpty()) {
        loadFromGroup(QString("TestOrder/%1").arg(selectedStation));
    }
    if (indexes.isEmpty()) {
        loadFromGroup("TestOrder");
    }
    if (indexes.isEmpty()) {
        loadFromGroup(orderGroupName("default"));
    }
    if (indexes.isEmpty()) {
        loadFromGroup("TestOrder/default");
    }

    qDebug() << "[FreeWork] loaded indexes, station =" << selectedStation << ", indexes =" << indexes;
    return indexes;
}

// 获取下一个状态的函数
QFreeWork::State QFreeWork::getNextState(State currentState) {
    return static_cast<State>((static_cast<int>(currentState) + 1) % 5);
}

const QFreeWork::NamedFunction* QFreeWork::currentOrderedNamedFunction() const {
    if (teststate < 0 || teststate >= orderedTestIndexes_.count()) {
        return nullptr;
    }
    const int functionId = orderedTestIndexes_.at(teststate);
    const auto it = std::find_if(testFunctions.cbegin(), testFunctions.cend(),
                                 [functionId](const QFreeWork::NamedFunction& item) { return item.id == functionId; });
    return it == testFunctions.cend() ? nullptr : &*it;
}

bool QFreeWork::canRunOrderedTestStepLoop() const {
    if (at->getConnected()) {
        return true;
    }
    // 未连 dongle：仍须 tick 的例外（仪器段与 BLE 解耦，避免步间卡死）
    if (stepRuntime_.started) {
        return true;
    }
    const QFreeWork::NamedFunction* const nf = currentOrderedNamedFunction();
    return nf != nullptr
           && (nf->name.startsWith(QStringLiteral("产品串口"))
               || nf->name.startsWith(QStringLiteral("并联CMW")));
}

void QFreeWork::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        if (teststate == -1) {
            showlog("开始测试");
            initDate();
            // 每次开始测试都重新读取配置，避免设置页调整后本页仍使用旧队列。
            refreshOrderedTestIndexes();
            waitWork(1000);
            at->sendDcon(macAddress);  // 开始连接
            showlog("MAC地址为：" + ui->macInput->text());
            teststate++;
        }
        if (canRunOrderedTestStepLoop()) {
            for (; teststate < orderedTestIndexes_.count();) {
                const int functionId = orderedTestIndexes_.at(teststate);
                auto it = std::find_if(testFunctions.begin(), testFunctions.end(),
                                       [functionId](const NamedFunction& item) { return item.id == functionId; });
                if (it == testFunctions.end()) {
                    ++teststate;
                    stepRuntime_.reset();
                    break;
                }
                const NamedFunction& currentFunction = *it;
                const QString functionName = currentFunction.name;

                // 阶段1：首次进入当前步骤，仅触发一次动作（通常是发协议命令）
                if (!stepRuntime_.started) {
                    if (!canGoNext) {
                        break;
                    }
                    stepRuntime_.started = true;
                    stepRuntime_.functionId = functionId;
                    stepRuntime_.done = !currentFunction.needCaseDone;
                    stepRuntime_.pass = true;
                    stepRuntime_.testData = "-";
                    stepRuntime_.ask = "通过";
                    stepRuntime_.caseTimer.restart();
                    lastCommandRetryCount = 0;
                    showlog("开始测试内容：" + functionName);
                    executeFunctionByName(functionName);  //执行操作
                    qDebug() << "程序在跑" << teststate << orderedTestIndexes_.count();
                    break;
                }

                // 阶段2：等待通信链路允许继续
                if (!canGoNext) {
                    break;
                }

                // 发送重试超时：将当前步骤判失败并放行，避免卡住不判定。
                if (sendRetryOver) {
                    sendRetryOver = false;
                    stepRuntime_.done = true;
                    stepRuntime_.pass = false;
                    TestResult = failValue;
                    showlog("步骤超时未收到响应，判定失败：" + functionName);
                }

                // 需要业务判定的步骤必须等异步回调将 done 置位后再推进，
                // 防止“有回包即通过”。
                if (currentFunction.needCaseDone && !stepRuntime_.done) {
                    break;
                }

                if (!stepRuntime_.pass) {
                    TestResult = failValue;
                }

                // static const QSet<QString> skipTableFunctions = {"获取外围设备状态", "获取基本信息"};
                // if (!skipTableFunctions.contains(functionName)) {
                    const qint64 caseElapsedMs = stepRuntime_.caseTimer.isValid() ? stepRuntime_.caseTimer.elapsed() : 0;
                    const int caseRetryCount = lastCommandRetryCount;
                    showlog(QString("测试内容完成：%1，重试次数=%2，测试时长=%3ms")
                                .arg(functionName)
                                .arg(caseRetryCount)
                                .arg(caseElapsedMs));
                    TestItem test;
                    test.testItem = functionName;
                    test.testData = stepRuntime_.testData;
                    test.testResult = stepRuntime_.pass ? "通过" : "失败";
                    test.ask = stepRuntime_.ask;
                    testItems.append(test);
                    appendFreeWorkMesForCompletedStep(currentFunction, stepRuntime_.pass, stepRuntime_.testData);
                    testResultTableUpdate(testItems);
                // }

                ++teststate;
                stepRuntime_.reset();

                break;
            }
        }

        if (teststate == orderedTestIndexes_.count() && teststate != 0) {
            // testItems 在 testResultTableUpdate 内会 clear；MES 分项与上表同步写入 freeWorkMesSegments_。
            const QString mesItemValue = joinFreeWorkMesItemvalue(freeWorkMesSegments_, TestResult, failValue);
            showlog("mesItemValue======" + mesItemValue);
            if (TestResult == failValue) {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");
                pack.itemvalue = mesItemValue;
                pack.result = "NG";
                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                if (ui->isusemes->checkState()) {
                    emit send_end_testPass(pack);
                }
            } else {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
                pack.result = "PASS";
                pack.itemvalue = mesItemValue;
                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                if (ui->isusemes->checkState()) {
                    emit send_end_testPass(pack);
                }
            }

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
    }
}

QFreeWork::~QFreeWork() {
    if (jigSerialPort->isOpen()) {
        disconnect(jigSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        jigSerialPort->close();
        qDebug() << getIndex() << "已关闭jig串口";
    }
    delete ui;
}

void QFreeWork::getDongleWifi(QString data) {
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
        showlog("已发送禁止休眠");
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

    closeKeyWaitPrompt();
    keyWaitPrompt_ = new QMessageBox(QMessageBox::Information, "按键测试", promptText, QMessageBox::NoButton, this);
    keyWaitPrompt_->setStandardButtons(QMessageBox::NoButton);
    QPushButton* hiddenCloseButton = keyWaitPrompt_->addButton("", QMessageBox::RejectRole);
    hiddenCloseButton->hide();
    keyWaitPrompt_->setAttribute(Qt::WA_DeleteOnClose);
    keyWaitPromptProgrammaticClose_ = false;
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
                                      const QString& enableKey, int keyIndex0To6) {
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
    stepRuntime_.testData = "PLC整步与等待按键上报";
    stepRuntime_.ask = SETTINGS.value(expectedKey).toString();
    plcKeyBlePlcOkSummary_.clear();
    plcSwitchBlePhase_ = 0;

    closeKeyWaitPrompt();
    keyWaitPrompt_ = new QMessageBox(QMessageBox::Information, "PLC按键测试", promptText, QMessageBox::NoButton, this);
    keyWaitPrompt_->setStandardButtons(QMessageBox::NoButton);
    {
        QPushButton* hiddenCloseButton = keyWaitPrompt_->addButton("", QMessageBox::RejectRole);
        hiddenCloseButton->hide();
    }
    keyWaitPrompt_->setAttribute(Qt::WA_DeleteOnClose);
    keyWaitPromptProgrammaticClose_ = false;
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
            plcKeyBlePlcOkSummary_.clear();
            TestResult = failValue;
            showlog(currentKeyTestName_ + "失败：用户关闭按键弹窗");
        }
        keyWaitPromptProgrammaticClose_ = false;
    });
    keyWaitPrompt_->show();
    showlog(testName + QStringLiteral("：已等待协议按键，将执行PLC整步"));

    runPlcV3TouchKeyFull(keyIndex0To6, false);

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
        closeKeyWaitPrompt();
        return;
    }

    armPlcBleKeyWaitTimeout();
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

void QFreeWork::startPlcSwitchPlcAndWaitLeftRotate() {
    const QString leftEn = QStringLiteral("ProductInfo/KeyIdLeftRotate_checkBox");
    if (!SETTINGS.value(leftEn).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("左旋按键配置未启用");
        stepRuntime_.ask = QStringLiteral("请检查配置");
        TestResult = failValue;
        showlog(QStringLiteral("PLC+V3旋钮左旋失败：左旋配置未启用"));
        return;
    }

    // phase 3：PLC 旋钮整步后仅校验左旋；右旋为测试项 89。
    plcSwitchBlePhase_ = 3;
    currentKeyTestName_ = QStringLiteral("PLC+V3旋钮左旋");
    currentKeyExpectedKey_ = QStringLiteral("ProductInfo/KeyIdLeftRotate");
    freeWorkKeyWaiting_ = true;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("PLC旋钮整步与等待左旋上报");
    stepRuntime_.ask = SETTINGS.value(currentKeyExpectedKey_).toString();
    plcKeyBlePlcOkSummary_.clear();

    closeKeyWaitPrompt();
    keyWaitPrompt_ = new QMessageBox(QMessageBox::Information, QStringLiteral("PLC旋钮左旋"),
                                     QStringLiteral("治具将自动完成旋钮动作，请确认设备上报左旋"),
                                     QMessageBox::NoButton, this);
    keyWaitPrompt_->setStandardButtons(QMessageBox::NoButton);
    {
        QPushButton* hiddenCloseButton = keyWaitPrompt_->addButton("", QMessageBox::RejectRole);
        hiddenCloseButton->hide();
    }
    keyWaitPrompt_->setAttribute(Qt::WA_DeleteOnClose);
    keyWaitPromptProgrammaticClose_ = false;
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
            plcKeyBlePlcOkSummary_.clear();
            TestResult = failValue;
            showlog(currentKeyTestName_ + "失败：用户关闭按键弹窗");
        }
        keyWaitPromptProgrammaticClose_ = false;
    });
    keyWaitPrompt_->show();
    showlog(QStringLiteral("PLC+V3旋钮左旋：已等待协议，将执行PLC旋钮整步"));

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
        closeKeyWaitPrompt();
        return;
    }

    armPlcBleKeyWaitTimeout();
    showlog(currentKeyTestName_ + QStringLiteral("：PLC旋钮整步完成，等待左旋上报"));
}

void QFreeWork::closeKeyWaitPrompt() {
    if (keyWaitPrompt_ != nullptr) {
        keyWaitPromptProgrammaticClose_ = true;
        keyWaitPrompt_->close();
        keyWaitPrompt_ = nullptr;
    }
}

void QFreeWork::checkbutton(ProtocolButtonStateData data) {
    if (!freeWorkKeyWaiting_ || currentKeyExpectedKey_.isEmpty()) {
        return;
    }

    ++plcKeyBleWaitSeq_;

    const QString actualKeyId = QString::number(data.keyButtonId);
    const QString expectedKeyId = SETTINGS.value(currentKeyExpectedKey_).toString();
    const bool idOk = compareVersions(expectedKeyId, actualKeyId);

    if (plcSwitchBlePhase_ == 3 || plcSwitchBlePhase_ == 4) {
        // 编码器：modeButtonState 为 dir（1左旋/2右旋）。须与旋钮 PLC 步骤的 phase 期望一致。
        const int expectedDir = (plcSwitchBlePhase_ == 3) ? 1 : 2;
        const bool dirOk = (data.modeButtonState == expectedDir);
        const bool pass = idOk && dirOk;
        const QString rotLabel = (plcSwitchBlePhase_ == 3) ? QStringLiteral("左旋") : QStringLiteral("右旋");
        closeKeyWaitPrompt();
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        stepRuntime_.done = true;
        stepRuntime_.pass = pass;
        const QString plcPart = plcKeyBlePlcOkSummary_;
        plcKeyBlePlcOkSummary_.clear();
        const QString keyLine = QStringLiteral("旋钮%1：方向=%2(期望%3) ID:%4 期望ID:%5")
                                    .arg(rotLabel)
                                    .arg(data.modeButtonState)
                                    .arg(expectedDir)
                                    .arg(actualKeyId, expectedKeyId);
        stepRuntime_.testData = plcPart.isEmpty() ? keyLine : QStringLiteral("%1；%2").arg(plcPart, keyLine);
        if (!pass) {
            TestResult = failValue;
        }
        stepRuntime_.ask = expectedKeyId;
        showlog(QStringLiteral("%1%2：%3上报")
                    .arg(currentKeyTestName_)
                    .arg(pass ? QStringLiteral("通过") : QStringLiteral("失败"))
                    .arg(rotLabel));
        return;
    }

    const bool pass = idOk;

    closeKeyWaitPrompt();
    freeWorkKeyWaiting_ = false;
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    if (plcKeyBlePlcOkSummary_.isEmpty()) {
        stepRuntime_.testData = QString("按键ID:%1 期望:%2").arg(actualKeyId, expectedKeyId);
    } else {
        stepRuntime_.testData =
            QString("%1；按键ID:%2 期望:%3").arg(plcKeyBlePlcOkSummary_, actualKeyId, expectedKeyId);
    }
    plcKeyBlePlcOkSummary_.clear();
    stepRuntime_.ask = expectedKeyId;
    if (!pass) {
        TestResult = failValue;
    }

    showlog(QString("%1%2：实际按键ID=%3 期望=%4")
                .arg(currentKeyTestName_)
                .arg(pass ? "通过" : "失败")
                .arg(actualKeyId, expectedKeyId));
}
int QFreeWork::resolvedPlcSwitchTestDoneResetM() const {
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    return SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM"), 211).toInt();
}

void QFreeWork::initDate() {
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
    currentKeyTestName_.clear();
    currentKeyExpectedKey_.clear();
    closeKeyWaitPrompt();
    lastBrushInstrumentProfile_ = -1;
    gInstrumentCmwGprfPrimed = false;
    inovancePlcTcp_.disconnect();
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
    freeWorkMesSegments_.clear();
    TestTime.start();
}
void QFreeWork::runPlcSwitchTestDoneResetM() {
    syncPlcModbusTraceFromSettings();
    const int resetM = resolvedPlcSwitchTestDoneResetM();
    const QString host = resolvedPlcIpAddress();
    const int port = resolvedPlcPort();
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const int pulseMs = SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetPulseMs"), 0).toInt();
    const int offset = resolvedPlcMCoilAddressOffset();

    showlog(QStringLiteral("PLC旋钮测试完成复位: 工位=%1 M%2(addr=%3) PulseMs=%4")
                .arg(getIndex())
                .arg(resetM)
                .arg(resetM + offset)
                .arg(pulseMs));

    QString err;
    if (!inovancePlcTcp_.connectPlc(host, quint16(port), resolvedPlcUnitId(), connMs, &err)) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("复位线圈连接失败: %1").arg(err);
        showlog(QStringLiteral("PLC旋钮测试完成复位失败: %1").arg(err));
        return;
    }
    showlog(QStringLiteral("PLC旋钮测试完成复位: 写 M%1(addr=%2)=1").arg(resetM).arg(resetM + offset));
    if (!plcWriteCoil(resetM, true, &err)) {
        inovancePlcTcp_.disconnect();
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("写复位线圈失败: %1").arg(err);
        showlog(QStringLiteral("PLC旋钮测试完成复位失败: %1").arg(err));
        return;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    if (pulseMs > 0) {
        QThread::msleep(static_cast<unsigned long>(pulseMs));
        showlog(QStringLiteral("PLC旋钮测试完成复位: 脉冲置0 M%1(addr=%2)").arg(resetM).arg(resetM + offset));
        if (!plcWriteCoil(resetM, false, &err)) {
            inovancePlcTcp_.disconnect();
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("复位线圈脉冲回0失败: %1").arg(err);
            showlog(QStringLiteral("PLC旋钮测试完成复位失败: %1").arg(err));
            return;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }
    inovancePlcTcp_.disconnect();
    stepRuntime_.pass = true;
    stepRuntime_.testData =
        pulseMs > 0 ? QStringLiteral("M%1 复位脉冲%2ms").arg(resetM).arg(pulseMs)
                    : QStringLiteral("M%1 复位常1").arg(resetM);
    showlog(QStringLiteral("PLC旋钮测试完成复位通过"));
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
            bandingMacSn(macAddress, ui->getMac->text());  //获取测试数据不要绑定测试mac——sn
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

        ui->start_wifible_test->setEnabled(false);
        // 主状态机流程
        isTestContinue = true;
        teststate = -1;

        emit send_go_next_focus();
        ui->getMac->setDisabled(1);
        ui->macInput->setDisabled(1);
    }
}

void QFreeWork::on_pushButton_2_clicked() {
    at->sendBLELOG(1);  // 日志开
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
    showlog("正在查询mac地址");
    processGetMesTestValue();     // mes获取
    // getMac(ui->getMac->text());             // 文件获取
    if (ui->isusemes->checkState()) {
        processInspection(ui->getMac->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }

// on_macInput_returnPressed();
}

void QFreeWork::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = inputSnText;

            pack.mechines = getIndex();

            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit sendProcessInspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState())  // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void QFreeWork::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}
void QFreeWork::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");              // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) {  // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                      // 逐行读取文件
            QString line = in.readLine();          // 读取一行
            QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {
                QString sn = fields.at(0);   // 第一个字段是sn
                QString mac = fields.at(1);  // 第二个字段是mac
                if (sn == sn_to_search) {    // 检查是否是待检索的sn
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

        file.close();  // 关闭文件
    }
    if (!ui->isformmes->isChecked() && ui->macInput->text().isEmpty()) {
        ui->getMac->clear();
        showlog("找不到mac地址，清空当前输入的sn");
    }
}
void QFreeWork::getmacadress(const QByteArray& byte) {
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
        // at->sendMac(macAddress);//发送mac地址
        qDebug() << getIndex() << macAddress;
        bandingMacSn(macAddress, snbanding);
    }
    ui->snbanding->setFocus();
}
void QFreeWork::bandingMacSn(QString bandingmac, QString bandingsn) {
    if (bandingsn == "" || bandingmac == "")
        bandingresult = false;

    // 将网络路径转换为 QFile 能够处理的格式
    QString path;
    if (pack.factory == "xwd")
        path = "\\\\172.30.189.83\\sgpub\\LTC\\MAC\\mac_sn.txt";

    else
        path = "mac_sn.txt";

    // 在 Windows 上，使用 QDir::fromNativeSeparators 将路径中的反斜杠转换为正斜杠
    // path = QDir::fromNativeSeparators(path);
    QFile file(path);  // 创建一个文件对象

    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {  //
        QTextStream in(&file);                                // 创建一个文本流对象
        QStringList lines;                                    // 用于存储文件中的每一行数据
        bool found = false;                                   // 标记是否找到了相同的SN
        while (!in.atEnd()) {
            QString line = in.readLine();         // 逐行读取文件
            QStringList parts = line.split(",");  // 以逗号分隔每行数据
            if (parts.size() == 2 && parts[0].trimmed() == bandingsn) {
                // 如果找到了相同的SN，替换MAC地址
                lines << (bandingsn + "," + bandingmac);
                found = true;
            } else {
                // 否则，保留原有数据
                lines << line;
            }
        }
        if (!found) {
            // 如果没有找到相同的SN，则追加新的SN和MAC地址
            lines << (bandingsn + "," + bandingmac);
        }
        // 清空文件并写入新的数据
        file.resize(0);
        QTextStream out(&file);
        for (const QString& line : lines) {
            out << line << '\n';
        }
        file.close();  // 关闭文件
        showlog("保存mac_sn文件成功");
    } else {
        showlog("保存mac_sn文件失败");
    }

    // bandingMacSn_mes(bandingmac, bandingsn);
}

void QFreeWork::bandingMacSn_mes(QString bandingmac, QString bandingsn) {
    Q_UNUSED(bandingsn);
    pack.mechines = 1;  // 1脱1,1号上位机
    pack.sn = snbanding;
    pack.result = "PASS";
    pack.itemvalue = QString("|BTMAC:%1|").arg(bandingmac);
    pack.instruct_num = "076";
    if (ui->isusemes->checkState()) {
        emit send_end_testPass(pack);
    }

    if (bandingresult) {
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
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    ui->snbanding->clear();
    bandingresult = true;
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
        // 自由工站无 writesn/stringsn：写入 SN 用 expectedTailSnFromMes testFunction「写入SN码」）；读回比对用 deviceTailSnFromDevice + ui->getMac
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
    const QString stepName =
        stepNameIn.isEmpty() ? QStringLiteral("产品串口仪器复位应答") : stepNameIn;
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
        if (!isCurrentStep(stepName)) {
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
        if (!isCurrentStep(stepName)) {
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
    const QString stepName =
        stepNameIn.isEmpty() ? QStringLiteral("产品串口停止接收与PER") : stepNameIn;
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

    QString cmwErr;
    bool ranCmwBurst = false;
    if (!freeWorkInstrumentBleBrushCmwBurstIfEnabled(stepName, lastBrushInstrumentProfile_, &cmwErr, waitPacketMs, nullptr,
                                                     &ranCmwBurst)) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = cmwErr;
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：CMW GPRF——") + cmwErr);
        return;
    }

    const int delayBeforeStopMs = ranCmwBurst ? 0 : waitPacketMs;
    showlog(stepName +
            QStringLiteral("：写停止接收前延时 %1 ms（BrushInstrument/PacketPhaseWaitMs=%2；已实际打并联 CMW 突发则不再追加积包延时）")
                .arg(delayBeforeStopMs)
                .arg(waitPacketMs));

    QTimer::singleShot(delayBeforeStopMs, this, [this, stepName, stopAckTimeout]() {
        if (!isCurrentStep(stepName)) {
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
            if (!isCurrentStep(stepName)) {
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

void QFreeWork::loadWifiBleCmw100Config() {
    cmw100VisaConfig_.useVisa = true;
    cmw100VisaConfig_.visaAddress = SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed();
    cmw100VisaConfig_.timeoutMs = SETTINGS.value(QStringLiteral("BlePer/CmwTimeoutMs"), 5000).toInt();
    setVisaProtocolConfig(cmw100VisaConfig_);
}

bool QFreeWork::runCmwVisa(const std::function<bool(Qvisa*)>& action) {
    loadWifiBleCmw100Config();
    setVisaProtocolConfig(cmw100VisaConfig_);
    return runVisa(action);
}

bool QFreeWork::freeWorkCmwVisaWrite(const QString& cmd) {
    const bool trace = freeWorkShouldLogCmwVisaIo();
    if (trace) {
        showlog(QStringLiteral("[CMW-VISA] >> %1").arg(cmd));
    }
    const bool ok = runCmwVisa([&cmd](Qvisa* device) { return device->writeCommand(cmd); });
    if (trace) {
        showlog(QStringLiteral("[CMW-VISA] << (write ok=%1)").arg(ok ? QStringLiteral("yes") : QStringLiteral("no")));
    }
    return ok;
}

bool QFreeWork::freeWorkCmwVisaQuery(const QString& cmd, QString* response) {
    const bool trace = freeWorkShouldLogCmwVisaIo();
    if (trace) {
        showlog(QStringLiteral("[CMW-VISA] >> %1").arg(cmd));
    }
    QString stack;
    QString& ref = response ? *response : stack;
    const bool ok = runCmwVisa([&cmd, &ref](Qvisa* device) { return device->queryCommand(cmd, &ref); });
    if (trace) {
        showlog(QStringLiteral("[CMW-VISA] << %1").arg(ref));
    }
    return ok;
}

bool QFreeWork::freeWorkPrimeInstrumentCmwGprf(QString* errorMessage) {
    if (!SETTINGS.value(QStringLiteral("BlePer/CmwEnableFixedInit"), true).toBool()) {
        gInstrumentCmwGprfPrimed = true;
        return true;
    }
    if (gInstrumentCmwGprfPrimed) {
        return true;
    }
    freeWorkCmwVisaWrite(QStringLiteral("*CLS"));
    const int cycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                       SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt()).toInt();
    const QString repetition =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
    const double level = SETTINGS.value(QStringLiteral("BlePer/CmwTxPowerDbm"), -50.0).toDouble();
    QString opc;
    freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opc);
    freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:LIST OFF"));
    freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:BBMode ARB"));
    const QString waveform = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile"),"@WAVEFORM\WLAN_11ac_VHT_BW20_MCS0_LEN4096.wv").toString().trimmed();
    if (!waveform.isEmpty()) {
        showlog(QStringLiteral("CMW GPRF 加载 ARB 波形文件：%1").arg(waveform));
        freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE \"%1\"").arg(waveform));
        QString arbReadBack;
        if (freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &arbReadBack)) {
            showlog(QStringLiteral("CMW GPRF SOURce:GPRF:GEN:ARB:FILE? 仪侧当前波形路径：%1").arg(arbReadBack.trimmed()));
        }
    } else {
        showlog(QStringLiteral("CMW GPRF：BlePer/CmwWaveformFile 未配置（首次初始化仍继续，请确认仪上 ARB）"));
        // 未下发 FILE 时也允许读回仪内已驻留波形，便于核对当前播放文件。
        if (SETTINGS.value(QStringLiteral("BlePer/CmwQueryCurrentArbFile"), true).toBool()) {
            QString arbReadBack;
            if (freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &arbReadBack)) {
                showlog(QStringLiteral("CMW GPRF SOURce:GPRF:GEN:ARB:FILE?（未配置本机 BlePer/CmwWaveformFile）仪侧：%1")
                            .arg(arbReadBack.trimmed()));
            }
        }
    }
    freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(repetition));
    freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(qMax(1, cycles)));
    freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:RFSettings:LEVel %1").arg(level, 0, 'f', 1));
    // 对标 docs/cmw100rx.txt：不在此处 SOURce:GPRF:GEN:STATe ON（与 TRIG:...MANual:EXECute 的顺序在单次 burst 路径中完成）。
    QString err;
    if (SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterInit"), false).toBool() &&
        freeWorkCmwVisaQuery(QStringLiteral("SYSTem:ERRor?"), &err) && !err.startsWith(QLatin1Char('0'))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CMW100 GPRF初始化错误: %1").arg(err);
        }
        return false;
    }
    gInstrumentCmwGprfPrimed = true;
    return true;
}

bool QFreeWork::freeWorkWaitBleCmwArbComplete(const QString& scenarioLabel, QString* errorMessage, int* outElapsedMs) {
    const int cyclesSetting = SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                                              SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt()).toInt();
    const int targetCycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCompleteCycles"), qMax(0, cyclesSetting - 1)).toInt();
    const int pollIntervalMs =
        qMax(50, SETTINGS.value(QStringLiteral("BlePer/CmwArbPollIntervalMs"), 100).toInt());
    const int timeoutMs = qMax(500, SETTINGS.value(QStringLiteral("BlePer/CmwArbTimeoutMs"), 10000).toInt());
    const bool verbosePoll = SETTINGS.value(QStringLiteral("BlePer/CmwVerboseArbPollLog"), false).toBool();
    QElapsedTimer timer;
    timer.start();
    QString lastResponse;
    int lastCycles = 0;
    int prevCycles = -1;
    while (timer.elapsed() < timeoutMs) {
        QString response;
        if (!freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:ARB:SCOunt?"), &response)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100发包进度查询失败").arg(scenarioLabel);
            }
            if (outElapsedMs) {
                *outElapsedMs = static_cast<int>(timer.elapsed());
            }
            return false;
        }
        lastResponse = response;
        double countTime = 0.0;
        int cycles = 0;
        int samplesCurrent = 0;
        if (parseBlePerCmwArbScountFree(response, &countTime, &cycles, &samplesCurrent)) {
            lastCycles = cycles;
            if (verbosePoll || cycles != prevCycles) {
                prevCycles = cycles;
                showlog(QStringLiteral("CMW100发包进度 %1: time=%2s, cycles=%3, samples=%4")
                            .arg(scenarioLabel)
                            .arg(countTime, 0, 'f', 3)
                            .arg(cycles)
                            .arg(samplesCurrent));
            }
            if (targetCycles <= 0 || cycles >= targetCycles) {
                if (outElapsedMs) {
                    *outElapsedMs = static_cast<int>(timer.elapsed());
                }
                if (!verbosePoll) {
                    showlog(QStringLiteral("CMW100发包进度 %1：ARB 完成 time=%2s cycles=%3 samples=%4 耗时%5ms")
                                .arg(scenarioLabel)
                                .arg(countTime, 0, 'f', 3)
                                .arg(cycles)
                                .arg(samplesCurrent)
                                .arg(timer.elapsed()));
                }
                return true;
            }
        } else {
            showlog(QStringLiteral("CMW100发包进度 %1: 无法解析SCOunt返回 %2").arg(scenarioLabel, response));
        }
        waitWork(pollIntervalMs);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("%1 CMW100 ARB发包超时，最后返回:%2，cycles=%3")
                            .arg(scenarioLabel, lastResponse)
                            .arg(lastCycles);
    }
    if (outElapsedMs) {
        *outElapsedMs = static_cast<int>(timer.elapsed());
    }
    return false;
}

bool QFreeWork::freeWorkRunSingleCmwBurstAtMhz(int freqMhz, const QString& scenarioLabel, QString* errorMessage,
                                               int postTrigHoldMsOverride) {
    const QString wfPathLog = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (wfPathLog.isEmpty()) {
        showlog(QStringLiteral("[%1] CMW 单次 GPRF：BlePer/CmwWaveformFile 未配置").arg(scenarioLabel));
    } else {
        showlog(QStringLiteral("[%1] CMW 单次 GPRF ARB：BlePer/CmwWaveformFile=%2").arg(scenarioLabel).arg(wfPathLog));
    }

    const int cycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                       SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt()).toInt();
    const auto stopCmwGen = [this]() {
        if (!SETTINGS.value(QStringLiteral("BlePer/CmwStopAfterScenario"), true).toBool()) {
            return;
        }
        QString opc;
        freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opc);
        QString state;
        if (freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe?"), &state)) {
            showlog(QStringLiteral("CMW100 GPRF状态: %1").arg(state));
        }
    };
    // 单次频点复位：对齐 cmw100rx「先 SOURce:GPRF:GEN:STATe OFF」再下发切频/TRIG。
    if (SETTINGS.value(QStringLiteral("BlePer/CmwBurstStatOffFirst"), true).toBool()) {
        QString opcOff;
        freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opcOff);
    }
    freeWorkCmwVisaWrite(QStringLiteral("*CLS"));
    freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:RFSettings:FREQuency %1MHz").arg(freqMhz));
    if (!SETTINGS.value(QStringLiteral("BlePer/CmwUseGuiRfConfig"), true).toBool()) {
        const QString repetition =
            SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
        freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(repetition));
        freeWorkCmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(qMax(1, cycles)));
    }
    if (SETTINGS.value(QStringLiteral("BlePer/CmwQueryCurrentArbFile"), true).toBool()) {
        QString arbCur;
        if (freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &arbCur)) {
            showlog(QStringLiteral("[%1] SOURce:GPRF:GEN:ARB:FILE? 仪侧当前波形：%2")
                        .arg(scenarioLabel)
                        .arg(arbCur.trimmed()));
        }
    }
    // docs/cmw100rx.txt：先 TRIGger:...MANual:EXECute 再 SOURce:GPRF:GEN:STATe ON;*OPC?
    if (!freeWorkCmwVisaWrite(QStringLiteral("TRIGger:GPRF:GEN:ARB:MANual:EXECute"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 CMW100触发发包失败").arg(scenarioLabel);
        }
        stopCmwGen();
        return false;
    }
    QString opc;
    freeWorkCmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"), &opc);

    const int holdMs =
        postTrigHoldMsOverride >= 0 ? postTrigHoldMsOverride : SETTINGS.value(QStringLiteral("BlePer/CmwTriggerWaitMs"), 1000).toInt();
    const bool legacyScountOnly = SETTINGS.value(QStringLiteral("BlePer/CmwWaitArbScount"), false).toBool();
    const bool burstPollPad = SETTINGS.value(QStringLiteral("BlePer/CmwBurstPollArbScount"), true).toBool();

    if (legacyScountOnly) {
        if (postTrigHoldMsOverride >= 0) {
            showlog(QStringLiteral("%1：BlePer/CmwWaitArbScount=真，仅以 ARB SCOunt? 轮询待发完；积包 %2 ms 不用于 TRIG 后阻塞；PER 步仍可能在该窗口后发停止（已与 CMW 顺序解耦）")
                        .arg(scenarioLabel)
                        .arg(postTrigHoldMsOverride));
        }
        if (!freeWorkWaitBleCmwArbComplete(scenarioLabel, errorMessage, nullptr)) {
            stopCmwGen();
            return false;
        }
    } else if (burstPollPad) {
        int arbElapsedMs = 0;
        if (!freeWorkWaitBleCmwArbComplete(scenarioLabel, errorMessage, &arbElapsedMs)) {
            stopCmwGen();
            return false;
        }
        if (arbElapsedMs < holdMs) {
            const int padMs = holdMs - arbElapsedMs;
            if (postTrigHoldMsOverride >= 0) {
                showlog(QStringLiteral(
                             "%1：ARB:SCOunt? 已达设定周期后再补足积包 %2ms（总≥配置的积包毫秒 %3）")
                            .arg(scenarioLabel)
                            .arg(padMs)
                            .arg(holdMs));
            }
            waitWork(padMs);
        } else if (postTrigHoldMsOverride >= 0) {
            showlog(QStringLiteral("%1：ARB 轮询段已耗时 %2 ms，不小于积包毫秒 %3，跳过补足等待")
                        .arg(scenarioLabel)
                        .arg(arbElapsedMs)
                        .arg(holdMs));
        }
    } else {
        if (postTrigHoldMsOverride >= 0) {
            showlog(QStringLiteral("%1：BlePer/CmwBurstPollArbScount=false，STAT ON 后仅定时阻塞 %2ms（不轮询 SCOunt）")
                        .arg(scenarioLabel)
                        .arg(holdMs));
        }
        waitWork(holdMs);
    }
    if (SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterScenario"), false).toBool()) {
        QString err;
        if (freeWorkCmwVisaQuery(QStringLiteral("SYSTem:ERRor?"), &err) && !err.startsWith(QLatin1Char('0'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100错误: %2").arg(scenarioLabel, err);
            }
            stopCmwGen();
            return false;
        }
    }
    stopCmwGen();
    return true;
}

bool QFreeWork::freeWorkInstrumentBleBrushCmwBurstIfEnabled(const QString& scenarioLabel, int brushProfile,
                                                              QString* errorMessage, int alignedPostTrigHoldMs,
                                                              bool* outAlignedWaitDoneByCmw,
                                                              bool* ranCmwBurst) {
    if (outAlignedWaitDoneByCmw) {
        *outAlignedWaitDoneByCmw = false;
    }
    if (ranCmwBurst) {
        *ranCmwBurst = false;
    }
    // 并联 CMW Profile0～5 各占一步时，可在完成六步后关此项，避免 PER1～PER6 每步再打一发 GPRF。
    if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwOnStopPer"), true).toBool()) {
        return true;
    }
    if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool()) {
        return true;
    }
    loadWifiBleCmw100Config();
    if (cmw100VisaConfig_.visaAddress.isEmpty()) {
        showlog(QStringLiteral("%1：已启用并联射频但未配置 BlePer/CmwVisaAddress，跳过 CMW").arg(scenarioLabel));
        return true;
    }
    if (brushProfile < 0 || brushProfile > 5) {
        showlog(QStringLiteral("%1：无有效 brush profile（请先完成对应「开始接收」），跳过 CMW").arg(scenarioLabel));
        return true;
    }
    QString idn;
    if (!runCmwVisa([this, &idn](Qvisa* device) {
            if (!device->ensureConnected()) {
                return false;
            }
            const QString q = QStringLiteral("*IDN?");
            if (freeWorkShouldLogCmwVisaIo()) {
                showlog(QStringLiteral("[CMW-VISA] >> %1").arg(q));
            }
            const bool qok = device->queryCommand(q, &idn);
            if (freeWorkShouldLogCmwVisaIo()) {
                showlog(QStringLiteral("[CMW-VISA] << %1").arg(idn));
            }
            return qok;
        })) {
        if (errorMessage) {
            *errorMessage =
                QStringLiteral("CMW VISA连接失败（%1）").arg(cmw100VisaConfig_.visaAddress);
        }
        return false;
    }
    if (!idn.trimmed().isEmpty()) {
        showlog(QStringLiteral("并联CMW: %1").arg(idn.trimmed()));
    }
    if (!freeWorkPrimeInstrumentCmwGprf(errorMessage)) {
        return false;
    }
    const int mhz = brushProfileToBleCmwMHz(brushProfile);
    const QString label =
        QStringLiteral("%1 Profile%2@%3MHz").arg(scenarioLabel).arg(brushProfile).arg(mhz);
    const bool burstOk =
        freeWorkRunSingleCmwBurstAtMhz(mhz, label, errorMessage, alignedPostTrigHoldMs);
    if (burstOk && ranCmwBurst) {
        *ranCmwBurst = true;
    }
    if (burstOk && outAlignedWaitDoneByCmw && alignedPostTrigHoldMs >= 0
        && !SETTINGS.value(QStringLiteral("BlePer/CmwWaitArbScount"), false).toBool()) {
        *outAlignedWaitDoneByCmw = true;
    }
    return burstOk;
}

bool QFreeWork::runFreeInstrumentBleCmwBurstForBrushProfile(QString* detail, int brushProfile) {
    auto setDetail = [detail](const QString& s) {
        if (detail) {
            *detail = s;
        }
    };
    if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool()) {
        const QString msg = QStringLiteral("跳过：未勾选并联 CMW100（BleBrushCmwConcurrent）");
        showlog(QStringLiteral("并联CMW Profile%1：%2").arg(brushProfile).arg(msg));
        setDetail(msg);
        return true;
    }
    if (brushProfile < 0 || brushProfile > 5) {
        const QString msg = QStringLiteral("Profile无效：%1").arg(brushProfile);
        setDetail(msg);
        showlog(QStringLiteral("并联CMW失败：%1").arg(msg));
        return false;
    }
    loadWifiBleCmw100Config();
    if (cmw100VisaConfig_.visaAddress.isEmpty()) {
        const QString msg = QStringLiteral("跳过：未配置 BlePer/CmwVisaAddress");
        showlog(QStringLiteral("并联CMW Profile%1：%2").arg(brushProfile).arg(msg));
        setDetail(msg);
        return true;
    }
    QString idn;
    if (!runCmwVisa([this, &idn](Qvisa* device) {
            if (!device->ensureConnected()) {
                return false;
            }
            const QString q = QStringLiteral("*IDN?");
            if (freeWorkShouldLogCmwVisaIo()) {
                showlog(QStringLiteral("[CMW-VISA] >> %1").arg(q));
            }
            const bool qok = device->queryCommand(q, &idn);
            if (freeWorkShouldLogCmwVisaIo()) {
                showlog(QStringLiteral("[CMW-VISA] << %1").arg(idn));
            }
            return qok;
        })) {
        const QString err = QStringLiteral("CMW VISA连接失败（%1）").arg(cmw100VisaConfig_.visaAddress);
        setDetail(err);
        showlog(QStringLiteral("并联CMW Profile%1：%2").arg(brushProfile).arg(err));
        return false;
    }
    const QString wfPath = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (wfPath.isEmpty()) {
        showlog(QStringLiteral("并联CMW Profile%1：CMW ARB 波形未配置（BlePer/CmwWaveformFile 为空）").arg(brushProfile));
    } else {
        showlog(QStringLiteral("并联CMW Profile%1：CMW ARB 波形文件 %2").arg(brushProfile).arg(wfPath));
    }
    QString primeErr;
    if (!freeWorkPrimeInstrumentCmwGprf(&primeErr)) {
        setDetail(primeErr);
        showlog(QStringLiteral("并联CMW Profile%1 GPRF初始化失败：%2").arg(brushProfile).arg(primeErr));
        return false;
    }
    const int mhz = brushProfileToBleCmwMHz(brushProfile);
    const QString label = QStringLiteral("并联CMW播放 Profile%1@%2MHz").arg(brushProfile).arg(mhz);
    QString burstErr;
    if (!freeWorkRunSingleCmwBurstAtMhz(mhz, label, &burstErr)) {
        setDetail(burstErr);
        showlog(QStringLiteral("并联CMW Profile%1 失败：%2").arg(brushProfile).arg(burstErr));
        return false;
    }
    const QString okLine = QStringLiteral("OK Profile%1 %2MHz").arg(brushProfile).arg(mhz);
    setDetail(okLine);
    showlog(okLine);
    return true;
}

void QFreeWork::on_stopTest_clicked() {
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(100);
    clearProductInstrumentWatch();
    inovancePlcTcp_.disconnect();
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

int QFreeWork::resolvedPlcMBase() const {
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/MBase_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    const int base1 = SETTINGS.value(QStringLiteral("PLC/MBase"), 200).toInt();
    const int step = SETTINGS.value(QStringLiteral("PLC/MBaseStationStep"), 20).toInt();
    if (st <= 1) {
        return base1;
    }
    return base1 + step * (st - 1);
}

int QFreeWork::resolvedPlcSwitchForwardM() const {
    const int base = resolvedPlcMBase();
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/SwitchForwardM_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    const int global = SETTINGS.value(QStringLiteral("PLC/SwitchForwardM"), -1).toInt();
    if (global >= 0) {
        return global;
    }
    return base + SETTINGS.value(QStringLiteral("PLC/SwitchForwardOffset"), 12).toInt();
}

int QFreeWork::resolvedPlcSwitchPressM() const {
    const int base = resolvedPlcMBase();
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/SwitchPressM_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    const int global = SETTINGS.value(QStringLiteral("PLC/SwitchPressM"), -1).toInt();
    if (global >= 0) {
        return global;
    }
    return base + SETTINGS.value(QStringLiteral("PLC/SwitchPressOffset"), 7).toInt();
}

int QFreeWork::resolvedPlcConnectVerifyM() const {
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    return SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM"), resolvedPlcMBase()).toInt();
}

QString QFreeWork::resolvedPlcIpAddress() const {
    const int st = qMax(1, getIndex());
    return SETTINGS.value(QStringLiteral("PLC/IpAddress_Station%1").arg(st),
                          SETTINGS.value(QStringLiteral("PLC/IpAddress"), QStringLiteral("192.168.1.88")))
        .toString();
}

int QFreeWork::resolvedPlcPort() const {
    const int st = qMax(1, getIndex());
    return SETTINGS.value(QStringLiteral("PLC/Port_Station%1").arg(st), SETTINGS.value(QStringLiteral("PLC/Port"), 502))
        .toInt();
}

quint8 QFreeWork::resolvedPlcUnitId() const {
    const int st = qMax(1, getIndex());
    return quint8(SETTINGS.value(QStringLiteral("PLC/UnitId_Station%1").arg(st), SETTINGS.value(QStringLiteral("PLC/UnitId"), 1))
                      .toUInt());
}

int QFreeWork::resolvedPlcMCoilAddressOffset() const {
    const int st = qMax(1, getIndex());
    return SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station%1").arg(st),
                          SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset"), 0))
        .toInt();
}

int QFreeWork::resolvedPlcPositionReadyBase() const {
    const int st = qMax(1, getIndex());
    const int per = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase_Station%1").arg(st), -1).toInt();
    if (per >= 0) {
        return per;
    }
    const int g = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase"), -1).toInt();
    if (g >= 0) {
        return g;
    }
    const int step = SETTINGS.value(QStringLiteral("PLC/PositionReadyBaseStationStep"), 20).toInt();
    return 250 + step * (st - 1);
}

int QFreeWork::resolvedPlcStepDoneBase() const {
    const int st = qMax(1, getIndex());
    const int per = SETTINGS.value(QStringLiteral("PLC/StepDoneBase_Station%1").arg(st), -1).toInt();
    if (per >= 0) {
        return per;
    }
    const int g = SETTINGS.value(QStringLiteral("PLC/StepDoneBase"), -1).toInt();
    if (g >= 0) {
        return g;
    }
    const int step = SETTINGS.value(QStringLiteral("PLC/StepDoneBaseStationStep"), 20).toInt();
    return 260 + step * (st - 1);
}

int QFreeWork::resolvedPlcKeyDoneM() const {
    const int st = qMax(1, getIndex());
    const int per = SETTINGS.value(QStringLiteral("PLC/KeyDoneM_Station%1").arg(st), -1).toInt();
    if (per >= 0) {
        return per;
    }
    const int g = SETTINGS.value(QStringLiteral("PLC/KeyDoneM"), -1).toInt();
    if (g >= 0) {
        return g;
    }
    return resolvedPlcMBase() + SETTINGS.value(QStringLiteral("PLC/KeyDoneOffsetFromMBase"), 10).toInt();
}

void QFreeWork::syncPlcModbusTraceFromSettings() {
    const bool on = SETTINGS.value(QStringLiteral("PLC/ModbusTrace"), false).toBool()
                     || (qEnvironmentVariableIntValue("PLC_MODBUS_TRACE") != 0);
    // inovancePlcTcp_.setTraceEnabled(on);
}

void QFreeWork::maybeShowlogPlcSessionSummary(const QString& stepTag) {
    showlog(QStringLiteral("[PLC调试 %1] IP=%2 Port=%3 UnitId=%4 M偏移=%5 MBase=%6 PosReady基=%7 StepDone基=%8 "
                           "KeyDoneM=%9 ConnectVerifyM=%10 ConnectTimeoutMs=%11 RequestTimeoutMs=%12 "
                           "Trace=%13 验证读=%14 握手=%15 等KeyDone=%16 完成后释放=%17 KeyDone预复位=%18 KeyDone复位等待=%19 "
                           "CommandGapMs=%20 PosTimeoutMs=%21 PosPollMs=%22 KeyDoneTimeoutMs=%23 KeyDonePollMs=%24")
                .arg(stepTag)
                .arg(resolvedPlcIpAddress())
                .arg(resolvedPlcPort())
                .arg(resolvedPlcUnitId())
                .arg(resolvedPlcMCoilAddressOffset())
                .arg(resolvedPlcMBase())
                .arg(resolvedPlcPositionReadyBase())
                .arg(resolvedPlcStepDoneBase())
                .arg(resolvedPlcKeyDoneM())
                .arg(resolvedPlcConnectVerifyM())
                .arg(SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt())
                .arg(plcBoolText(1))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool()))
                .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool()))
                .arg(SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                    SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                         .toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                    SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt())
                         .toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                .arg(SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt()));
}

void QFreeWork::runPlcModbusConnectTest() {
    syncPlcModbusTraceFromSettings();
    maybeShowlogPlcSessionSummary(QStringLiteral("连接测试"));
    const QString host = resolvedPlcIpAddress();
    const int port = resolvedPlcPort();
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    const int reqMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
    const quint8 uid = resolvedPlcUnitId();
    const int offset = resolvedPlcMCoilAddressOffset();

    showlog(QStringLiteral("PLC_Modbus连接开始(工位%1): IP=%2 Port=%3 UnitId=%4 ConnectTimeoutMs=%5 RequestTimeoutMs=%6")
                .arg(getIndex())
                .arg(host)
                .arg(port)
                .arg(uid)
                .arg(connMs)
                .arg(reqMs));

    QString err;
    if (!inovancePlcTcp_.connectPlc(host, quint16(port), uid, connMs, &err)) {
        stepRuntime_.pass = false;
        const QString detail = QStringLiteral("%1；配置: IP=%2 Port=%3 UnitId=%4 ConnectTimeoutMs=%5 "
                                              "RequestTimeoutMs=%6 M偏移=%7 验证读=%8；请检查PLC IP/端口、网线、"
                                              "PLC Modbus TCP服务和防火墙")
                                   .arg(err)
                                   .arg(host)
                                   .arg(port)
                                   .arg(uid)
                                   .arg(connMs)
                                   .arg(reqMs)
                                   .arg(offset)
                                   .arg(SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()
                                            ? QStringLiteral("开启")
                                            : QStringLiteral("关闭"));
        stepRuntime_.testData = detail;
        showlog(QStringLiteral("PLC_Modbus连接失败(工位%1): %2").arg(getIndex()).arg(detail));
        return;
    }
    showlog(QStringLiteral("PLC_Modbus TCP已连接(工位%1): IP=%2 Port=%3 UnitId=%4")
                .arg(getIndex())
                .arg(host)
                .arg(port)
                .arg(uid));
    bool ok = true;
    if (SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()) {
        const int verifyM = resolvedPlcConnectVerifyM();
        showlog(QStringLiteral("PLC_Modbus连接验证读: M%1(addr=%2) Quantity=1 TimeoutMs=%3")
                    .arg(verifyM)
                    .arg(verifyM + offset)
                    .arg(reqMs));
        QVector<bool> bits;
        if (!inovancePlcTcp_.readMCoils(verifyM, 1, offset, uid, reqMs, &bits, &err)) {
            ok = false;
            stepRuntime_.testData = QStringLiteral("连接验证读失败: M%1(addr=%2) %3")
                                        .arg(verifyM)
                                        .arg(verifyM + offset)
                                        .arg(err);
            showlog(QStringLiteral("PLC 连接后读线圈失败(可关 PLC/ConnectVerifyRead): M%1(addr=%2) %3")
                        .arg(verifyM)
                        .arg(verifyM + offset)
                        .arg(err));
        } else {
            stepRuntime_.testData = QStringLiteral("已连 %1:%2")
                                        .arg(host)
                                        .arg(port);
            showlog(QStringLiteral("PLC_Modbus连接验证读通过: M%1(addr=%2)=%3")
                        .arg(verifyM)
                        .arg(verifyM + offset)
                        .arg(bits.value(0) ? 1 : 0));
        }
    } else {
        stepRuntime_.testData = QStringLiteral("已连 %1:%2 UnitId=%3，验证读关闭").arg(host).arg(port).arg(uid);
        showlog(QStringLiteral("PLC_Modbus连接验证读关闭"));
    }
    inovancePlcTcp_.disconnect();
    stepRuntime_.pass = ok;
    showlog(ok ? QStringLiteral("PLC_Modbus连接通过") : QStringLiteral("PLC_Modbus连接未通过"));
}

bool QFreeWork::plcReadCoil(int absoluteM, bool* value, QString* errorMessage) {
    const int reqMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
    const quint8 uid = resolvedPlcUnitId();
    const int offset = resolvedPlcMCoilAddressOffset();
    QVector<bool> bits;
    if (!inovancePlcTcp_.readMCoils(absoluteM, 1, offset, uid, reqMs, &bits, errorMessage)) {
        return false;
    }
    *value = bits.value(0);
    return true;
}

bool QFreeWork::plcWriteCoil(int absoluteM, bool value, QString* errorMessage) {
    const int reqMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
    const quint8 uid = resolvedPlcUnitId();
    const int offset = resolvedPlcMCoilAddressOffset();
    return inovancePlcTcp_.writeMCoil(absoluteM, value, offset, uid, reqMs, errorMessage);
}

bool QFreeWork::plcWaitCoilTrue(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage) {
    QElapsedTimer t;
    t.start();
    const int step = qMax(10, pollMs);
    while (t.elapsed() < timeoutMs) {
        // if (!isTestContinue) {
        //     if (errorMessage) {
        //         *errorMessage = QStringLiteral("测试已停止");
        //     }
        //     return false;
        // }
        bool v = false;
        if (!plcReadCoil(absoluteM, &v, errorMessage)) {
            return false;
        }
        if (v) {
            return true;
        }
        QThread::msleep(static_cast<unsigned long>(step));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待 M%1=1 超时 %2ms").arg(absoluteM).arg(timeoutMs);
    }
    return false;
}

bool QFreeWork::plcWaitCoilFalse(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage) {
    QElapsedTimer t;
    t.start();
    const int step = qMax(10, pollMs);
    while (t.elapsed() < timeoutMs) {
        if (!isTestContinue) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("测试已停止");
            }
            return false;
        }
        bool v = false;
        if (!plcReadCoil(absoluteM, &v, errorMessage)) {
            return false;
        }
        if (!v) {
            return true;
        }
        QThread::msleep(static_cast<unsigned long>(step));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待 M%1=0 超时 %2ms").arg(absoluteM).arg(timeoutMs);
    }
    return false;
}

bool QFreeWork::plcSendStepDone(QString* errorMessage) {
    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const int pulseMs = SETTINGS.value(QStringLiteral("PLC/StepDonePulseMs"), 0).toInt();
    const int m = resolvedPlcStepDoneBase();
    const int offset = resolvedPlcMCoilAddressOffset();
    showlog(QStringLiteral("PLC发送 StepDone: M%1(addr=%2)=1 GapMs=%3 PulseMs=%4")
                .arg(m)
                .arg(m + offset)
                .arg(gapMs)
                .arg(pulseMs));
    if (!plcWriteCoil(m, true, errorMessage)) {
        return false;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    if (pulseMs > 0) {
        QThread::msleep(static_cast<unsigned long>(pulseMs));
        showlog(QStringLiteral("PLC复位 StepDone 脉冲: M%1(addr=%2)=0").arg(m).arg(m + offset));
        if (!plcWriteCoil(m, false, errorMessage)) {
            return false;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }
    return true;
}

void QFreeWork::runPlcV3TouchKeyFull(int keyIndex0To6, bool finishStepRuntime) {
    syncPlcModbusTraceFromSettings();
    maybeShowlogPlcSessionSummary(QStringLiteral("V3键%1").arg(keyIndex0To6));
    const auto fail = [this](const QString& msg) {
        inovancePlcTcp_.disconnect();
        stepRuntime_.pass = false;
        stepRuntime_.testData = msg;
        stepRuntime_.done = true;
        showlog(msg);
    };
    const auto passOk = [this, finishStepRuntime](const QString& msg) {
        inovancePlcTcp_.disconnect();
        stepRuntime_.pass = true;
        if (finishStepRuntime) {
            stepRuntime_.testData = msg;
            stepRuntime_.done = true;
            plcKeyBlePlcOkSummary_.clear();
        } else if (stepRuntime_.done) {
            // 阻塞跑 PLC 时事件循环已收到协议并置 done，勿再清掉
            if (!msg.isEmpty()) {
                stepRuntime_.testData = stepRuntime_.testData.isEmpty()
                                            ? msg
                                            : QStringLiteral("%1；%2").arg(msg, stepRuntime_.testData);
            }
            plcKeyBlePlcOkSummary_.clear();
        } else {
            stepRuntime_.testData = msg;
            stepRuntime_.done = false;
            plcKeyBlePlcOkSummary_ = msg;
        }
        showlog(msg);
    };

    if (keyIndex0To6 < 0 || keyIndex0To6 > 6) {
        fail(QStringLiteral("V3 Touch keyIndex 非法: %1").arg(keyIndex0To6));
        return;
    }

    const QString host = resolvedPlcIpAddress();
    const int port = resolvedPlcPort();
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    QString err;
    showlog(QStringLiteral("PLC按键整步连接: 键Index=%1 工位=%2 IP=%3 Port=%4 UnitId=%5")
                .arg(keyIndex0To6)
                .arg(getIndex())
                .arg(host)
                .arg(port)
                .arg(resolvedPlcUnitId()));
    if (!inovancePlcTcp_.connectPlc(host, quint16(port), resolvedPlcUnitId(), connMs, &err)) {
        fail(QStringLiteral("PLC 连接失败: %1").arg(err));
        return;
    }

    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const bool useHandshake = SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool();
    const bool waitKeyDone = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool();
    const bool releaseAfter = SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool();
    const bool ensureKeyIdle = SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool();
    const bool waitKeyReset = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool();
    const int posSettle = SETTINGS.value(QStringLiteral("PLC/PositionSettleMs"), 500).toInt();
    const int actionSettle = SETTINGS.value(QStringLiteral("PLC/KeyActionSettleMs"),
                                             SETTINGS.value(QStringLiteral("KeyTest/ActionSettleMs"), 0).toInt())
                                 .toInt();
    const int releaseSettle = SETTINGS.value(QStringLiteral("PLC/KeyReleaseSettleMs"),
                                               SETTINGS.value(QStringLiteral("KeyTest/ReleaseSettleMs"), 120).toInt())
                                  .toInt();
    const int posTimeout = SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                               .toInt();
    const int posPoll = SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                       SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 1000).toInt())
                            .toInt();
    const int keyDoneTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt();
    const int keyDonePoll = SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt();
    const int keyResetTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneResetTimeoutMs"), 1500).toInt();
    const int offset = resolvedPlcMCoilAddressOffset();

    const int mBase = resolvedPlcMBase();
    const int keyM = mBase + keyIndex0To6;
    const int posReadyM = resolvedPlcPositionReadyBase() + keyIndex0To6;
    const int stepDoneM = resolvedPlcStepDoneBase();
    const int keyDoneM = resolvedPlcKeyDoneM();
    showlog(QStringLiteral("PLC按键整步开始: 键Index=%1 KeyM=M%2(addr=%3) PosReady=M%4(addr=%5) "
                           "StepDone=M%6(addr=%7) KeyDone=M%8(addr=%9) 握手=%10 等KeyDone=%11 完成后释放=%12")
                .arg(keyIndex0To6)
                .arg(keyM)
                .arg(keyM + offset)
                .arg(posReadyM)
                .arg(posReadyM + offset)
                .arg(stepDoneM)
                .arg(stepDoneM + offset)
                .arg(keyDoneM)
                .arg(keyDoneM + offset)
                .arg(plcBoolText(useHandshake))
                .arg(plcBoolText(waitKeyDone))
                .arg(plcBoolText(releaseAfter)));

    if (ensureKeyIdle) {
        bool kd = false;
        showlog(QStringLiteral("PLC预检 KeyDone 空闲: 读 M%1(addr=%2)").arg(keyDoneM).arg(keyDoneM + offset));
        if (!plcReadCoil(keyDoneM, &kd, &err)) {
            fail(QStringLiteral("读 KeyDone 失败: %1").arg(err));
            return;
        }
        if (kd && !plcWaitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return;
        }
    }

    if (useHandshake) {
        bool sd = false;
        showlog(QStringLiteral("PLC预检 StepDone: 读 M%1(addr=%2)").arg(stepDoneM).arg(stepDoneM + offset));
        if (!plcReadCoil(stepDoneM, &sd, &err)) {
            fail(QStringLiteral("读 StepDone 失败: %1").arg(err));
            return;
        }
        if (sd) {
            showlog(QStringLiteral("PLC复位残留 StepDone: M%1(addr=%2)=0").arg(stepDoneM).arg(stepDoneM + offset));
            if (!plcWriteCoil(stepDoneM, false, &err)) {
                fail(QStringLiteral("复位 StepDone 失败: %1").arg(err));
                return;
            }
            if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
        }
    }

    showlog(QStringLiteral("PLC按键下压: M%1(addr=%2)=1").arg(keyM).arg(keyM + offset));
    if (!plcWriteCoil(keyM, true, &err)) {
        fail(QStringLiteral("下压 M%1 失败: %2").arg(keyM).arg(err));
        return;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }

    if (useHandshake) {
        showlog(QStringLiteral("PLC等待位置到位哈哈: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                    .arg(posReadyM)
                    .arg(posReadyM + offset)
                    .arg(posTimeout)
                    .arg(posPoll));
        if (!plcWaitCoilTrue(posReadyM, posTimeout, posPoll, &err)) {
            fail(QStringLiteral("等待位置到位 M%1: %2").arg(posReadyM).arg(err));
            return;
        }
    }

    if (posSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(posSettle));
    }
    if (actionSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(actionSettle));
    }

    if (useHandshake) {
        if (!plcSendStepDone(&err)) {
            fail(QStringLiteral("发送 StepDone 失败: %1").arg(err));
            return;
        }
    }

    bool sawKeyDone = false;
    if (waitKeyDone) {
        showlog(QStringLiteral("PLC等待 KeyDone: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                    .arg(keyDoneM)
                    .arg(keyDoneM + offset)
                    .arg(keyDoneTimeout)
                    .arg(keyDonePoll));
        if (!plcWaitCoilTrue(keyDoneM, keyDoneTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone M%1: %2").arg(keyDoneM).arg(err));
            return;
        }
        sawKeyDone = true;
    }

    if (releaseAfter) {
        showlog(QStringLiteral("PLC按键抬起: M%1(addr=%2)=0").arg(keyM).arg(keyM + offset));
        if (!plcWriteCoil(keyM, false, &err)) {
            fail(QStringLiteral("抬起 M%1 失败: %2").arg(keyM).arg(err));
            return;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }

    if (releaseSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(releaseSettle));
    }

    if (sawKeyDone && waitKeyReset) {
        showlog(QStringLiteral("PLC等待 KeyDone 复位: M%1(addr=%2)=0 TimeoutMs=%3 PollMs=%4")
                    .arg(keyDoneM)
                    .arg(keyDoneM + offset)
                    .arg(keyResetTimeout)
                    .arg(keyDonePoll));
        if (!plcWaitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return;
        }
    }

    passOk(QStringLiteral("键%1 M%2 P%3 S%4 K%5")
               .arg(keyIndex0To6)
               .arg(keyM)
               .arg(posReadyM)
               .arg(stepDoneM)
               .arg(keyDoneM));
}

void QFreeWork::runPlcV3TouchSwitchFull(bool finishStepRuntime) {
    syncPlcModbusTraceFromSettings();
    maybeShowlogPlcSessionSummary(QStringLiteral("V3旋钮"));
    const auto fail = [this](const QString& msg) {
        inovancePlcTcp_.disconnect();
        stepRuntime_.pass = false;
        stepRuntime_.testData = msg;
        stepRuntime_.done = true;
        showlog(msg);
    };
    const auto passOk = [this, finishStepRuntime](const QString& msg) {
        inovancePlcTcp_.disconnect();
        stepRuntime_.pass = true;
        if (finishStepRuntime) {
            stepRuntime_.testData = msg;
            stepRuntime_.done = true;
            plcKeyBlePlcOkSummary_.clear();
        } else if (stepRuntime_.done) {
            if (!msg.isEmpty()) {
                stepRuntime_.testData = stepRuntime_.testData.isEmpty()
                                            ? msg
                                            : QStringLiteral("%1；%2").arg(msg, stepRuntime_.testData);
            }
            plcKeyBlePlcOkSummary_.clear();
        } else {
            stepRuntime_.testData = msg;
            stepRuntime_.done = false;
            plcKeyBlePlcOkSummary_ = msg;
        }
        showlog(msg);
    };

    const QString host = resolvedPlcIpAddress();
    const int port = resolvedPlcPort();
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    QString err;
    showlog(QStringLiteral("PLC旋钮整步连接: 工位=%1 IP=%2 Port=%3 UnitId=%4")
                .arg(getIndex())
                .arg(host)
                .arg(port)
                .arg(resolvedPlcUnitId()));
    if (!inovancePlcTcp_.connectPlc(host, quint16(port), resolvedPlcUnitId(), connMs, &err)) {
        fail(QStringLiteral("PLC 连接失败: %1").arg(err));
        return;
    }

    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const bool useHandshake = SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool();
    const bool waitKeyDone = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool();
    const bool releaseAfter = SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool();
    const bool ensureKeyIdle = SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool();
    const bool waitKeyReset = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool();
    const int posSettle = SETTINGS.value(QStringLiteral("PLC/PositionSettleMs"), 500).toInt();
    const int actionSettle = SETTINGS.value(QStringLiteral("PLC/KeyActionSettleMs"),
                                             SETTINGS.value(QStringLiteral("KeyTest/ActionSettleMs"), 0).toInt())
                                 .toInt();
    const int releaseSettle = SETTINGS.value(QStringLiteral("PLC/KeyReleaseSettleMs"),
                                               SETTINGS.value(QStringLiteral("KeyTest/ReleaseSettleMs"), 120).toInt())
                                  .toInt();
    const int posTimeout = SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                               .toInt();
    const int posPoll = SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                       SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt())
                            .toInt();
    const int keyDoneTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt();
    const int keyDonePoll = SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt();
    const int keyResetTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneResetTimeoutMs"), 1500).toInt();
    const int offset = resolvedPlcMCoilAddressOffset();

    const int forwardM = resolvedPlcSwitchForwardM();
    const int pressM = resolvedPlcSwitchPressM();
    const int posReadyM = resolvedPlcPositionReadyBase() + 7;
    const int stepDoneM = resolvedPlcStepDoneBase();
    const int keyDoneM = resolvedPlcKeyDoneM();
    showlog(QStringLiteral("PLC旋钮整步开始: Forward=M%1(addr=%2) Press=M%3(addr=%4) PosReady=M%5(addr=%6) "
                           "StepDone=M%7(addr=%8) KeyDone=M%9(addr=%10) 握手=%11 等KeyDone=%12 完成后释放=%13")
                .arg(forwardM)
                .arg(forwardM + offset)
                .arg(pressM)
                .arg(pressM + offset)
                .arg(posReadyM)
                .arg(posReadyM + offset)
                .arg(stepDoneM)
                .arg(stepDoneM + offset)
                .arg(keyDoneM)
                .arg(keyDoneM + offset)
                .arg(plcBoolText(useHandshake))
                .arg(plcBoolText(waitKeyDone))
                .arg(plcBoolText(releaseAfter)));

    if (ensureKeyIdle) {
        bool kd = false;
        showlog(QStringLiteral("PLC预检 KeyDone 空闲: 读 M%1(addr=%2)").arg(keyDoneM).arg(keyDoneM + offset));
        if (!plcReadCoil(keyDoneM, &kd, &err)) {
            fail(QStringLiteral("读 KeyDone 失败: %1").arg(err));
            return;
        }
        if (kd && !plcWaitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return;
        }
    }

    if (useHandshake) {
        bool sd = false;
        showlog(QStringLiteral("PLC预检 StepDone: 读 M%1(addr=%2)").arg(stepDoneM).arg(stepDoneM + offset));
        if (!plcReadCoil(stepDoneM, &sd, &err)) {
            fail(QStringLiteral("读 StepDone 失败: %1").arg(err));
            return;
        }
        if (sd) {
            showlog(QStringLiteral("PLC复位残留 StepDone: M%1(addr=%2)=0").arg(stepDoneM).arg(stepDoneM + offset));
            if (!plcWriteCoil(stepDoneM, false, &err)) {
                fail(QStringLiteral("复位 StepDone 失败: %1").arg(err));
                return;
            }
            if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
        }
    }

    showlog(QStringLiteral("PLC旋钮前推: M%1(addr=%2)=1").arg(forwardM).arg(forwardM + offset));
    if (!plcWriteCoil(forwardM, true, &err)) {
        fail(QStringLiteral("旋钮前推 M%1 失败: %2").arg(forwardM).arg(err));
        return;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    showlog(QStringLiteral("PLC旋钮按压: M%1(addr=%2)=1").arg(pressM).arg(pressM + offset));
    if (!plcWriteCoil(pressM, true, &err)) {
        fail(QStringLiteral("旋钮按压 M%1 失败: %2").arg(pressM).arg(err));
        return;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }

    if (useHandshake) {
        showlog(QStringLiteral("PLC等待位置到位: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                    .arg(posReadyM)
                    .arg(posReadyM + offset)
                    .arg(posTimeout)
                    .arg(posPoll));
        if (!plcWaitCoilTrue(posReadyM, posTimeout, posPoll, &err)) {
            fail(QStringLiteral("等待位置到位 M%1: %2").arg(posReadyM).arg(err));
            return;
        }
    }

    if (posSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(posSettle));
    }
    if (actionSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(actionSettle));
    }

    if (useHandshake) {
        if (!plcSendStepDone(&err)) {
            fail(QStringLiteral("发送 StepDone 失败: %1").arg(err));
            return;
        }
    }

    bool sawKeyDone = false;
    if (waitKeyDone) {
        showlog(QStringLiteral("PLC等待 KeyDone: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                    .arg(keyDoneM)
                    .arg(keyDoneM + offset)
                    .arg(keyDoneTimeout)
                    .arg(keyDonePoll));
        if (!plcWaitCoilTrue(keyDoneM, keyDoneTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone M%1: %2").arg(keyDoneM).arg(err));
            return;
        }
        sawKeyDone = true;
    }

    if (releaseAfter) {
        showlog(QStringLiteral("PLC旋钮释放按压: M%1(addr=%2)=0").arg(pressM).arg(pressM + offset));
        if (!plcWriteCoil(pressM, false, &err)) {
            fail(QStringLiteral("旋钮释放按压 M%1 失败: %2").arg(pressM).arg(err));
            return;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
        showlog(QStringLiteral("PLC旋钮收回前推: M%1(addr=%2)=0").arg(forwardM).arg(forwardM + offset));
        if (!plcWriteCoil(forwardM, false, &err)) {
            fail(QStringLiteral("旋钮收回前推 M%1 失败: %2").arg(forwardM).arg(err));
            return;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }

    if (releaseSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(releaseSettle));
    }

    if (sawKeyDone && waitKeyReset) {
        showlog(QStringLiteral("PLC等待 KeyDone 复位: M%1(addr=%2)=0 TimeoutMs=%3 PollMs=%4")
                    .arg(keyDoneM)
                    .arg(keyDoneM + offset)
                    .arg(keyResetTimeout)
                    .arg(keyDonePoll));
        if (!plcWaitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return;
        }
    }

    // passOk(QStringLiteral("旋钮整步 M%1+M%2 Pos%3 Step%4 KeyDone%5")
    //            .arg(forwardM)
    //            .arg(pressM)
    //            .arg(posReadyM)
    //            .arg(stepDoneM)
    //            .arg(keyDoneM));
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
    keyWaitPrompt_ = new QMessageBox(QMessageBox::Information, QStringLiteral("PLC旋钮右旋"),
                                     QStringLiteral("治具将自动完成旋钮动作（实际为右转），请确认设备上报右旋"),
                                     QMessageBox::NoButton, this);
    keyWaitPrompt_->setStandardButtons(QMessageBox::NoButton);
    {
        QPushButton* hiddenCloseButton = keyWaitPrompt_->addButton("", QMessageBox::RejectRole);
        hiddenCloseButton->hide();
    }
    keyWaitPrompt_->setAttribute(Qt::WA_DeleteOnClose);
    keyWaitPromptProgrammaticClose_ = false;
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
            plcKeyBlePlcOkSummary_.clear();
            TestResult = failValue;
            showlog(currentKeyTestName_ + "失败：用户关闭按键弹窗");
        }
        keyWaitPromptProgrammaticClose_ = false;
    });
    keyWaitPrompt_->show();
    showlog(QStringLiteral("PLC+V3旋钮右旋：已开始等待协议，将执行PLC旋钮整步"));

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
        closeKeyWaitPrompt();
        return;
    }

    armPlcBleKeyWaitTimeout();
    showlog(currentKeyTestName_ + QStringLiteral("：PLC旋钮整步完成，等待右旋上报"));
}