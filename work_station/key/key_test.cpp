#include "key_test.h"

#include "ui_key_test.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QMessageBox>
#include <QStringList>
#include <QThread>
#include <QVector>
#include <QtGlobal>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {
Qusb::ProtocolType protocolTypeFromSetting(const QString& type)
{
    const QString value = type.trimmed().toLower();
    if (value == "scpi") {
        return Qusb::ProtocolType::Scpi;
    }
    if (value == "hq" || value == "hqmodbus") {
        return Qusb::ProtocolType::HqModbus;
    }
    if (value == "lx" || value == "lxmodbus") {
        return Qusb::ProtocolType::LxModbus;
    }
    return Qusb::ProtocolType::Auto;
}

QString plcBoolText(bool v)
{
    return v ? QStringLiteral("开") : QStringLiteral("关");
}
}
key_test::key_test(int index, QWidget* parent) :
    test_base(parent), ui(new Ui::key_test), basicInfoModel(new TestModel), peripheralModel(new TestModel) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = KEY_VER;

    ui->setupUi(this);
    // setAttribute(Qt::WA_DeleteOnClose);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();  // 要搜索一下一开始

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    testResultTableInit();

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
        qDebug() << getIndex() << "结束计时" << QDateTime::currentDateTime();
    });
    connect(ble_waittime, &QTimer::timeout, [=]() {
        ble_waittime->stop();
        state = STATE_KEY_TEST;
        qDebug() << getIndex() << "计时结束，进入按键测试" << QDateTime::currentDateTime();
    });
    connect(usblogwaittime, &QTimer::timeout, [=]() {
        at->get(DongleCmd::GetGmac);
        showlog("正在定时器复位设备");
    });

    key_test_wait_time = SETTINGS.value("Key/TestWaitTime", 15000).toInt();

    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);
    showlog("model=" + pack.model);

    showlog("machineNo=" + pack.machineNo);
    showlog("key_test_wait_time=" + QString::number(key_test_wait_time));
    applyKeyProtocolConfig();

    if (pack.factory == "hq" || pack.factory == "jj") {
        ui->jigComNameCombo->setEnabled(false);
        ui->jigConnectButton->setEnabled(false);
        ui->jigDisconnectButton->setEnabled(false);
    }

    if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        ui->productDisconnectButton->setEnabled(true);
    }else {
        ui->productComNameCombo->setEnabled(false);
        ui->productConnectButton->setEnabled(false);
        ui->productDisconnectButton->setEnabled(false);
    }

    // if (QString(pack.product).compare("P20PS") == 0) {
    //     productBaudRate = 2000000;
    // } else {
    //     productBaudRate = 1000000;
    // }
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}

void key_test::applyKeyProtocolConfig() {
    Qusb::ProtocolConfig cfg;
    cfg.protocol = protocolTypeFromSetting("auto");
    cfg.luxshareMachineId = getIndex();
    cfg.scpiCurrentType = SETTINGS.value("Current/ScpiCurrentType", "CURR").toString();
    cfg.scpiCurrentMode = SETTINGS.value("Current/ScpiCurrentMode", "DC").toString();
    cfg.scpiRange = SETTINGS.value("Current/ScpiRange", "500e-3").toString();

    if (cfg.protocol == Qusb::ProtocolType::Auto) {
        const QString factory = pack.factory.trimmed().toLower();
        if (factory == "hq") {
            cfg.protocol = Qusb::ProtocolType::HqModbus;
        } else if (factory == "lx" || factory == "jj") {
            cfg.protocol = Qusb::ProtocolType::LxModbus;
        } else {
            cfg.protocol = Qusb::ProtocolType::Scpi;
        }
    }

    keyProtocolType = cfg.protocol;
    usb->setProtocolConfig(cfg);

    showlog("按键测试协议=" + SETTINGS.value("Current/ProtocolType", "auto").toString() +
            " 实际生效协议=" + QString::number(static_cast<int>(keyProtocolType)));
    showlog("按键测试配置: machineId=" + QString::number(cfg.luxshareMachineId) +
            ", scpi=" + cfg.scpiCurrentType + ":" + cfg.scpiCurrentMode + " " + cfg.scpiRange);
}

void key_test::closeKeyWaitPromptProgrammatically() {
    if (keyWaitPrompt == nullptr) {
        return;
    }
    keyWaitPromptProgrammaticClose = true;
    // close()/delete 是异步的，先清空成员指针，避免下一个状态机判断仍认为弹窗存在
    QMessageBox* prompt = keyWaitPrompt;
    keyWaitPrompt = nullptr;
    prompt->close();
}

void key_test::onKeyWaitPromptDestroyed() {
    keyWaitPrompt = nullptr;
    // 弹窗已销毁，必须复位，否则后续 STATE_WAIT_GET_KEY_* 会因 !keyWaitPromptShown 而永远不建框
    keyWaitPromptShown = false;

    if (keyWaitPromptProgrammaticClose) {
        keyWaitPromptProgrammaticClose = false;
        return;
    }
    if (!isTestContinue) {
        return;
    }

    // 需求：点击弹窗右上角 X 直接停止测试，且不上传 MES。
    // 因此不进入 STATE_SAVE_RESULT，不触发 send_end_testPass。
    showlog(QStringLiteral("用户关闭按键提示，测试已中止，不上传MES"));
    state = STATE_IDLE;
    on_stopTest_clicked();
}

bool key_test::usePlcClickerForKeyTest() const {
    return SETTINGS.value(QStringLiteral("KeyTest/UsePlcClicker"), true).toBool();
}

void key_test::failCurrentPlcKeyStep(const QString& testName, const QString& reason) {
    ++plcKeyWaitSeq;
    refresh_key_times = 0;
    plcKeyActionStarted = false;
    totalresult = failValue;
    appendStationResult(testItems, testName, reason, failValue);
    testResultTableUpdate(testItems);
    showlog(testName + QStringLiteral("失败：") + reason);
    state = STATE_SAVE_RESULT;
}

QString key_test::currentKeyStepName() const {
    switch (state) {
        case STATE_WAIT_GET_KEY_POWER_STATE: return QStringLiteral("电源键测试");
        case STATE_WAIT_GET_KEY_MODE_STATE: return QStringLiteral("Mode键测试");
        case STATE_WAIT_GET_KEY_PROGRAM_STATE: return QStringLiteral("Program键测试");
        case STATE_WAIT_GET_KEY_SPEED_STATE: return QStringLiteral("Speed键测试");
        case STATE_WAIT_GET_KEY_STARTPAUSE_STATE: return QStringLiteral("Start/Pause键测试");
        case STATE_WAIT_GET_KEY_LEFT_STATE: return QStringLiteral("左键测试");
        case STATE_WAIT_GET_KEY_RIGHT_STATE: return QStringLiteral("右键测试");
        default: return QString();
    }
}

QString key_test::currentExpectedKeyId() const {
    switch (state) {
        case STATE_WAIT_GET_KEY_POWER_STATE:
            return SETTINGS.value("ProductInfo/KeyIdPower_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdPower").toString()
                       : QString();
        case STATE_WAIT_GET_KEY_MODE_STATE:
            return SETTINGS.value("ProductInfo/KeyIdMode_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdMode").toString()
                       : QString();
        case STATE_WAIT_GET_KEY_PROGRAM_STATE:
            return SETTINGS.value("ProductInfo/KeyIdProgram_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdProgram").toString()
                       : QString();
        case STATE_WAIT_GET_KEY_SPEED_STATE:
            return SETTINGS.value("ProductInfo/KeyIdSpeed_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdSpeed").toString()
                       : QString();
        case STATE_WAIT_GET_KEY_STARTPAUSE_STATE:
            return SETTINGS.value("ProductInfo/KeyIdStartPause_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdStartPause").toString()
                       : QString();
        case STATE_WAIT_GET_KEY_LEFT_STATE:
            return SETTINGS.value("ProductInfo/KeyIdLeft_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdLeft").toString()
                       : QString();
        case STATE_WAIT_GET_KEY_RIGHT_STATE:
            return SETTINGS.value("ProductInfo/KeyIdRight_checkBox").toBool()
                       ? SETTINGS.value("ProductInfo/KeyIdRight").toString()
                       : QString();
        default: return QString();
    }
}

void key_test::setCurrentKeyResult(int result) {
    switch (state) {
        case STATE_WAIT_GET_KEY_POWER_STATE: KeyPowerState = result; break;
        case STATE_WAIT_GET_KEY_MODE_STATE: KeyModeState = result; break;
        case STATE_WAIT_GET_KEY_PROGRAM_STATE: KeyProgramState = result; break;
        case STATE_WAIT_GET_KEY_SPEED_STATE: KeySpeedState = result; break;
        case STATE_WAIT_GET_KEY_STARTPAUSE_STATE: KeyStartPauseState = result; break;
        case STATE_WAIT_GET_KEY_LEFT_STATE: KeyLeftState = result; break;
        case STATE_WAIT_GET_KEY_RIGHT_STATE: KeyRightState = result; break;
        default: break;
    }
}

QString key_test::currentKeyFailureDetail() const {
    return keyErrorDetail.isEmpty() ? QStringLiteral("按键错误") : keyErrorDetail;
}

void key_test::recordCurrentKeyButtonResult(int keyButtonId) {
    const QString stepName = currentKeyStepName();
    const QString expectedKeyId = currentExpectedKeyId();
    if (stepName.isEmpty() || expectedKeyId.trimmed().isEmpty()) {
        showlog(QStringLiteral("收到按键ID=%1，但当前步骤未配置期望按键，已忽略").arg(keyButtonId));
        return;
    }

    refresh_key_times = 0;
    ++plcKeyWaitSeq;
    plcKeyActionStarted = false;

    const QString actualKeyId = QString::number(keyButtonId);
    if (compareVersions(expectedKeyId, actualKeyId)) {
        keyErrorDetail.clear();
        setCurrentKeyResult(1);
        showlog(QStringLiteral("%1收到期望按键ID=%2").arg(stepName, actualKeyId));
        return;
    }

    keyErrorDetail = QStringLiteral("实际按键ID:%1，期望按键ID:%2").arg(actualKeyId, expectedKeyId);
    setCurrentKeyResult(2);
    showlog(QStringLiteral("%1错按：%2").arg(stepName, keyErrorDetail));
}

void key_test::refreshKeySignalRead(ProtocolUInt32ValueData data) {
    // PLC 下压期间同步读取 KeyCap，pollKeyCapDuringPress 会等待这个槽写回结果。
    if (plcKeyCapSyncReadPending_) {
        plcKeyCapSyncReadPending_ = false;
        plcKeyCapSyncReadOk_ = true;
        plcKeyCapSyncReadValue_ = data.value;
        plcKeyCapSyncReadAuxId_ = data.auxId;
    }
}

void key_test::resetPlcKeyCapSyncReadState() {
    plcKeyCapSyncReadPending_ = false;
    plcKeyCapSyncReadOk_ = false;
    plcKeyCapSyncReadValue_ = 0;
    plcKeyCapSyncReadAuxId_ = -1;
}

bool key_test::pollKeyCapDuringPress(QString* errOut, QString* outSummary) {
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
                *errOut = QStringLiteral("第%1/%2次读电容超时(%3ms)").arg(i + 1).arg(readCount).arg(singleTimeoutMs);
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
        showlog(currentKeyStepName() + QStringLiteral("：第%1/%2次读电容 KK=%3 值=%4")
                    .arg(i + 1)
                    .arg(readCount)
                    .arg(kk)
                    .arg(plcKeyCapSyncReadValue_));

        if (i + 1 < readCount && intervalMs > 0) {
            QThread::msleep(static_cast<unsigned long>(intervalMs));
        }
    }

    const QString expectedKeyId = QString::number(configuredKeyId);
    const bool idOk = compareVersions(expectedKeyId, QString::number(kk + 1));
    const bool capOk = (bestCap >= lowKeyCap_) && (bestCap <= highKeyCap_);
    const QString capAsk = QStringLiteral("[%1,%2]").arg(lowKeyCap_).arg(highKeyCap_);
    const QString summary = QStringLiteral("KK:%1 采样[%2] 最大:%3 ID:%4 期望ID:%5")
                                .arg(kk)
                                .arg(sampleTexts.join(QLatin1Char(',')))
                                .arg(bestCap)
                                .arg(kk + 1)
                                .arg(expectedKeyId);
    if (outSummary) {
        *outSummary = summary;
    }

    if (!idOk) {
        if (errOut) {
            *errOut = QStringLiteral("按键ID与配置不符，KK=%1 期望ID=%2").arg(kk).arg(expectedKeyId);
        }
        return false;
    }
    if (!capOk) {
        if (errOut) {
            *errOut = QStringLiteral("电容卡控失败，最大=%1 允许%2").arg(bestCap).arg(capAsk);
        }
        return false;
    }

    showlog(currentKeyStepName() + QStringLiteral("卡控通过，KK=%1 最大电容=%2").arg(kk).arg(bestCap));
    return true;
}

void key_test::startPlcClickerAndWaitKey(const QString& testName, int keyIndex0To6) {
    if (!usePlcClickerForKeyTest()) {
        failCurrentPlcKeyStep(testName, QStringLiteral("KeyTest/UsePlcClicker 未启用"));
        return;
    }
    if (plcKeyActionStarted) {
        return;
    }
    plcKeyActionStarted = true;
    keyErrorDetail.clear();
    keyPassDetail.clear();

    const QString expectedKeyId = currentExpectedKeyId().trimmed();
    bool keyIdOk = false;
    const int configuredKeyId = expectedKeyId.toInt(&keyIdOk);
    if (!keyIdOk || configuredKeyId <= 0) {
        plcKeyActionStarted = false;
        keyErrorDetail = QStringLiteral("按键ID配置无效:%1").arg(expectedKeyId);
        setCurrentKeyResult(2);
        return;
    }
    currentKeyConfiguredId_ = configuredKeyId;
    currentKeyCapRequestKk_ = configuredKeyId - 1;
    lowKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/Low"), 1).toUInt();
    highKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/High"), 65535).toUInt();
    resetPlcKeyCapSyncReadState();
    plcKeyCapPassSummary_.clear();

    QString summary;
    showlog(testName + QStringLiteral("：启动外设点击器并读取电容 KK=%1（配置ID=%2 减1），卡控[%3,%4]")
                           .arg(currentKeyCapRequestKk_)
                           .arg(configuredKeyId)
                           .arg(lowKeyCap_)
                           .arg(highKeyCap_));
    if (!runPlcV3TouchKeyFull(keyIndex0To6, &summary)) {
        plcKeyActionStarted = false;
        keyErrorDetail = summary;
        setCurrentKeyResult(2);
        return;
    }
    showlog(testName + QStringLiteral("：外设点击器完成，%1").arg(summary));
    plcKeyActionStarted = false;
    keyErrorDetail.clear();
    keyPassDetail = summary;
    setCurrentKeyResult(1);
}

int key_test::resolvedPlcMBase() const {
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/MBase_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    const int base1 = SETTINGS.value(QStringLiteral("PLC/MBase"), 200).toInt();
    const int step = SETTINGS.value(QStringLiteral("PLC/MBaseStationStep"), 20).toInt();
    return st <= 1 ? base1 : base1 + step * (st - 1);
}

int key_test::resolvedPlcConnectVerifyM() const {
    const int st = qMax(1, getIndex());
    const int perStation = SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM_Station%1").arg(st), -1).toInt();
    if (perStation >= 0) {
        return perStation;
    }
    return SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM"), resolvedPlcMBase()).toInt();
}          

QString key_test::resolvedPlcIpAddress() const {
    const int st = qMax(1, getIndex());
    return SETTINGS.value(QStringLiteral("PLC/IpAddress_Station%1").arg(st),
                          SETTINGS.value(QStringLiteral("PLC/IpAddress"), QStringLiteral("192.168.1.88"))).toString();
}

int key_test::resolvedPlcPort() const {
    const int st = qMax(1, getIndex());
    return SETTINGS.value(QStringLiteral("PLC/Port_Station%1").arg(st), SETTINGS.value(QStringLiteral("PLC/Port"), 502)).toInt();
}

quint8 key_test::resolvedPlcUnitId() const {
    const int st = qMax(1, getIndex());
    return quint8(SETTINGS.value(QStringLiteral("PLC/UnitId_Station%1").arg(st), SETTINGS.value(QStringLiteral("PLC/UnitId"), 1)).toUInt());
}

int key_test::resolvedPlcMCoilAddressOffset() const {
    const int st = qMax(1, getIndex());
    return SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station%1").arg(st),
                          SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset"), 0)).toInt();
}

int key_test::resolvedPlcPositionReadyBase() const {
    const int st = qMax(1, getIndex());
    const int per = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase_Station%1").arg(st), -1).toInt();
    if (per >= 0) {
        return per;
    }
    const int global = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase"), -1).toInt();
    if (global >= 0) {
        return global;
    }
    const int step = SETTINGS.value(QStringLiteral("PLC/PositionReadyBaseStationStep"), 20).toInt();
    return 250 + step * (st - 1);
}

int key_test::resolvedPlcStepDoneBase() const {
    const int st = qMax(1, getIndex());
    const int per = SETTINGS.value(QStringLiteral("PLC/StepDoneBase_Station%1").arg(st), -1).toInt();
    if (per >= 0) {
        return per;
    }
    const int global = SETTINGS.value(QStringLiteral("PLC/StepDoneBase"), -1).toInt();
    if (global >= 0) {
        return global;
    }
    const int step = SETTINGS.value(QStringLiteral("PLC/StepDoneBaseStationStep"), 20).toInt();
    return 260 + step * (st - 1);
}

int key_test::resolvedPlcKeyDoneM() const {
    const int st = qMax(1, getIndex());
    const int per = SETTINGS.value(QStringLiteral("PLC/KeyDoneM_Station%1").arg(st), -1).toInt();
    if (per >= 0) {
        return per;
    }
    const int global = SETTINGS.value(QStringLiteral("PLC/KeyDoneM"), -1).toInt();
    if (global >= 0) {
        return global;
    }
    return resolvedPlcMBase() + SETTINGS.value(QStringLiteral("PLC/KeyDoneOffsetFromMBase"), 10).toInt();
}

bool key_test::plcReadCoil(int absoluteM, bool* value, QString* errorMessage) {
    const int reqMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
    QVector<bool> bits;
    if (!inovancePlcTcp_.readMCoils(absoluteM, 1, resolvedPlcMCoilAddressOffset(), resolvedPlcUnitId(), reqMs, &bits, errorMessage)) {
        return false;
    }
    *value = bits.value(0);
    return true;
}

bool key_test::plcWriteCoil(int absoluteM, bool value, QString* errorMessage) {
    const int reqMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
    return inovancePlcTcp_.writeMCoil(absoluteM, value, resolvedPlcMCoilAddressOffset(), resolvedPlcUnitId(), reqMs, errorMessage);
}

bool key_test::plcWaitCoilTrue(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage) {
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

bool key_test::plcWaitCoilFalse(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage) {
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

bool key_test::plcSendStepDone(QString* errorMessage) {
    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const int pulseMs = SETTINGS.value(QStringLiteral("PLC/StepDonePulseMs"), 0).toInt();
    const int m = resolvedPlcStepDoneBase();
    if (!plcWriteCoil(m, true, errorMessage)) {
        return false;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    if (pulseMs > 0) {
        QThread::msleep(static_cast<unsigned long>(pulseMs));
        if (!plcWriteCoil(m, false, errorMessage)) {
            return false;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }
    return true;
}

bool key_test::runPlcV3TouchKeyFull(int keyIndex0To6, QString* summary) {
    // inovancePlcTcp_.setTraceEnabled(SETTINGS.value(QStringLiteral("PLC/ModbusTrace"), false).toBool()
    //                                 || (qEnvironmentVariableIntValue("PLC_MODBUS_TRACE") != 0));
    if (keyIndex0To6 < 0 || keyIndex0To6 > 6) {
        if (summary) {
            *summary = QStringLiteral("V3 Touch keyIndex 非法: %1").arg(keyIndex0To6);
        }
        return false;
    }

    const QString host = resolvedPlcIpAddress();
    const int port = resolvedPlcPort();
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    QString err;
    showlog(QStringLiteral("PLC按键整步连接: 键Index=%1 工位=%2 IP=%3 Port=%4 UnitId=%5")
                .arg(keyIndex0To6).arg(getIndex()).arg(host).arg(port).arg(resolvedPlcUnitId()));
    if (!inovancePlcTcp_.connectPlc(host, quint16(port), resolvedPlcUnitId(), connMs, &err)) {
        if (summary) {
            *summary = QStringLiteral("PLC 连接失败: %1").arg(err);
        }
        return false;
    }

    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const bool useHandshake = SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool();
    const bool waitKeyDone = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool();
    const bool releaseAfter = SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool();
    const bool ensureKeyIdle = SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool();
    const bool waitKeyReset = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool();
    const int posSettle = SETTINGS.value(QStringLiteral("PLC/PositionSettleMs"), 500).toInt();
    const int actionSettle = SETTINGS.value(QStringLiteral("PLC/KeyActionSettleMs"),
                                             SETTINGS.value(QStringLiteral("KeyTest/ActionSettleMs"), 0).toInt()).toInt();
    const int releaseSettle = SETTINGS.value(QStringLiteral("PLC/KeyReleaseSettleMs"),
                                               SETTINGS.value(QStringLiteral("KeyTest/ReleaseSettleMs"), 120).toInt()).toInt();
    const int posTimeout = SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt()).toInt();
    const int posPoll = SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                       SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt()).toInt();
    const int keyDoneTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt();
    const int keyDonePoll = SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt();
    const int keyResetTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneResetTimeoutMs"), 1500).toInt();
    const int offset = resolvedPlcMCoilAddressOffset();
    const int keyM = resolvedPlcMBase() + keyIndex0To6;
    const int posReadyM = resolvedPlcPositionReadyBase() + keyIndex0To6;
    const int stepDoneM = resolvedPlcStepDoneBase();
    const int keyDoneM = resolvedPlcKeyDoneM();
    showlog(QStringLiteral("PLC按键整步开始: KeyM=M%1(addr=%2) PosReady=M%3(addr=%4) StepDone=M%5(addr=%6) KeyDone=M%7(addr=%8) 握手=%9 等KeyDone=%10")
                .arg(keyM).arg(keyM + offset).arg(posReadyM).arg(posReadyM + offset)
                .arg(stepDoneM).arg(stepDoneM + offset).arg(keyDoneM).arg(keyDoneM + offset)
                .arg(plcBoolText(useHandshake)).arg(plcBoolText(waitKeyDone)));

    const auto fail = [&](const QString& msg) {
        if (inovancePlcTcp_.isConnected()) {
            QString releaseErr;
            if (!plcWriteCoil(keyM, false, &releaseErr)) {
                showlog(QStringLiteral("按键测试失败后释放 M%1 失败: %2").arg(keyM).arg(releaseErr));
            } else if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
        }
        inovancePlcTcp_.disconnect();
        if (summary) {
            *summary = msg;
        }
        return false;
    };

    if (ensureKeyIdle) {
        bool kd = false;
        if (!plcReadCoil(keyDoneM, &kd, &err)) {
            return fail(QStringLiteral("读 KeyDone 失败: %1").arg(err));
        }
        if (kd && !plcWaitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            return fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
        }
    }
    if (useHandshake) {
        bool sd = false;
        if (!plcReadCoil(stepDoneM, &sd, &err)) {
            return fail(QStringLiteral("读 StepDone 失败: %1").arg(err));
        }
        if (sd && !plcWriteCoil(stepDoneM, false, &err)) {
            return fail(QStringLiteral("复位 StepDone 失败: %1").arg(err));
        }
    }
    if (!plcWriteCoil(keyM, true, &err)) {
        return fail(QStringLiteral("下压 M%1 失败: %2").arg(keyM).arg(err));
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    if (useHandshake && !plcWaitCoilTrue(posReadyM, posTimeout, posPoll, &err)) {
        return fail(QStringLiteral("等待位置到位 M%1: %2").arg(posReadyM).arg(err));
    }
    if (posSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(posSettle));
    }
    if (actionSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(actionSettle));
    }

    QString capSummary;
    QString capErr;
    // 与自由工站一致：PLC 下压稳定后、StepDone/抬起前读取 KeyCap，确保治具仍保持按压状态。
    if (!pollKeyCapDuringPress(&capErr, &capSummary)) {
        return fail(capErr.isEmpty() ? QStringLiteral("按下期间读取按键电容失败") : capErr);
    }
    plcKeyCapPassSummary_ = capSummary;

    if (useHandshake && !plcSendStepDone(&err)) {
        return fail(QStringLiteral("发送 StepDone 失败: %1").arg(err));
    }
    bool sawKeyDone = false;
    if (waitKeyDone) {
        if (!plcWaitCoilTrue(keyDoneM, keyDoneTimeout, keyDonePoll, &err)) {
            return fail(QStringLiteral("等待 KeyDone M%1: %2").arg(keyDoneM).arg(err));
        }
        sawKeyDone = true;
    }
    if (releaseAfter) {
        if (!plcWriteCoil(keyM, false, &err)) {
            return fail(QStringLiteral("抬起 M%1 失败: %2").arg(keyM).arg(err));
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }
    if (releaseSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(releaseSettle));
    }
    if (sawKeyDone && waitKeyReset && !plcWaitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
        return fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
    }
    inovancePlcTcp_.disconnect();
    if (summary) {
        *summary = QStringLiteral("键%1 整步 M%2 Pos%3 Step%4 KeyDone%5 电容:%6")
                       .arg(keyIndex0To6)
                       .arg(keyM)
                       .arg(posReadyM)
                       .arg(stepDoneM)
                       .arg(keyDoneM)
                       .arg(plcKeyCapPassSummary_);
    }
    plcKeyCapPassSummary_.clear();
    return true;
}

void key_test::disconnect_dongle() { on_disconnectButton_clicked(); }

void key_test::refreshMusicState(ProtocolMusicStateData data) {
    bool isMusicStateTest = SETTINGS.value("Music/MusicState_checkBox").toBool();
    showlog("当前曲目为：" + QString::number(data.musicState));

    if (!isMusicStateTest || SETTINGS.value("Music/MusicState").toInt() == data.musicState) {
        TestItem test;
        test.testItem = "曲目测试";
        test.testData = QString::number(data.musicState);
        test.testResult = passValue;
        test.ask = QString::number(SETTINGS.value("Music/MusicState").toInt());
        testItems.append(test);

        testResultTableUpdate(testItems);

    } else {
        TestItem test;
        test.testItem = "曲目测试";
        test.testData = QString::number(data.musicState);
        test.testResult = failValue;
        test.ask = QString::number(SETTINGS.value("Music/MusicState").toInt());
        testItems.append(test);

        testResultTableUpdate(testItems);

        totalresult = failValue;
        state = STATE_SAVE_RESULT;
        showlog("曲目测试不良");
    }
}


void key_test::checkbutton(ProtocolButtonStateData x) {
    if (refresh_key_times) {
        if (!keyButtonDebounceTimer.isValid()) {
            keyButtonDebounceTimer.start();
        }
        const qint64 nowMs = keyButtonDebounceTimer.elapsed();
        constexpr int debounceMs = 300;
        if (x.keyButtonId == lastKeyButtonId && nowMs - lastKeyButtonMs < debounceMs) {
            showlog(QStringLiteral("忽略重复按键上报 keyId=%1").arg(x.keyButtonId));
            return;
        }
        lastKeyButtonId = x.keyButtonId;
        lastKeyButtonMs = nowMs;

        showlog("获取到按键上报");
        recordCurrentKeyButtonResult(x.keyButtonId);
        updateTestData(testItems);
    }
}



void key_test::refreshBaseData(ProtocolBaseInfoData data) {
    if (refresh_base_times) {
        qDebug() << getIndex() << "refresh_times" << refresh_base_times;
        refresh_base_times = 0;
        showlog("收到固件信号");
        pumpsoft_version = data.soft_version;
        qDebug() << getIndex() << "algo_version" << data.algo_version;
        qDebug() << getIndex() << "hw_version" << data.hw_version;
        qDebug() << getIndex() << "presure_version" << data.presure_version;
        qDebug() << getIndex() << "product_name" << data.product_name;
        qDebug() << getIndex() << "pb_phone_ver" << data.pb_phone_ver;
        qDebug() << getIndex() << "pb_factory_ver" << data.pb_factory_ver;
        qDebug() << getIndex() << "soft_version" << QString("%1").arg(data.soft_version);
        qDebug() << getIndex() << "res_version" << data.res_version;

        QString productName = SETTINGS.value("ProductInfo/Product_Name").toString();
        QString appProtocolVersion = SETTINGS.value("ProductInfo/App_Protocol_Version").toString();
        QString factoryProtocolVersion = SETTINGS.value("ProductInfo/Factory_Protocol_Version").toString();
        QString hardwareVersion = SETTINGS.value("ProductInfo/Hardware_Version").toString();
        QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
        QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();
        QString motorVersion = SETTINGS.value("ProductInfo/Motor_Ver").toString();
        QString algorithmVersion = SETTINGS.value("ProductInfo/Algorithm_Version").toString();
        QString pressureSenseVersion = SETTINGS.value("ProductInfo/Pressure_Sense_Version").toString();
        QString fsensorVersion = SETTINGS.value("ProductInfo/FSensor_Version").toString();
        QString imuId = SETTINGS.value("ProductInfo/IMU_ID").toString();
        QString bleVersion = SETTINGS.value("ProductInfo/Ble_Ver").toString();
        QString camera_id = SETTINGS.value("ProductInfo/Camera_Id").toString();

        bool isProductTest = SETTINGS.value("ProductInfo/ProductName_checkBox").toBool();
        bool isHwTest = SETTINGS.value("ProductInfo/HardwareVersion_checkBox").toBool();
        bool isSoftwareTest = SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool();
        bool isResourceTest = SETTINGS.value("ProductInfo/ResourceVersion_checkBox").toBool();
        bool isMotorTest = SETTINGS.value("ProductInfo/MotorVersion_checkBox").toBool();
        bool isAppProtocolTest = SETTINGS.value("ProductInfo/AppPB_checkBox").toBool();
        bool isFactoryProtocolTest = SETTINGS.value("ProductInfo/FactoryPB_checkBox").toBool();
        bool isAlgoTest = SETTINGS.value("ProductInfo/AlgorithmVersion_checkBox").toBool();
        bool isPresureTest = SETTINGS.value("ProductInfo/PressureVersion_checkBox").toBool();
        bool isFSensorTest = SETTINGS.value("ProductInfo/FSensorVersion_checkBox").toBool();
        bool isImuTest = SETTINGS.value("ProductInfo/ImuID_checkBox").toBool();
        bool isCameraTest = SETTINGS.value("ProductInfo/CameraID_checkBox").toBool();
        bool isBleTest = SETTINGS.value("ProductInfo/BluetoothVersion_checkBox").toBool();

        if ((!isAlgoTest || compareVersions(algorithmVersion, data.algo_version)) &&
            (!isHwTest || compareVersions(hardwareVersion, data.hw_version)) &&
            (!isPresureTest || compareVersions(pressureSenseVersion, data.presure_version)) &&
            (!isFSensorTest || compareVersions(fsensorVersion, data.fsensor_version)) &&
            (!isProductTest || compareVersions(productName, data.product_name)) &&
            (!isAppProtocolTest || compareVersions(appProtocolVersion, QString::number(data.pb_phone_ver))) &&
            (!isFactoryProtocolTest || compareVersions(factoryProtocolVersion, QString::number(data.pb_factory_ver))) &&
            (!isSoftwareTest || compareVersions(softwareVersion, data.soft_version)) &&
            (!isResourceTest || compareVersions(resourceVersion, data.res_version)) &&
            (!isMotorTest || compareVersions(motorVersion, data.motor_version)) &&
            (!isImuTest || compareVersions(imuId, QString::number(data.imu_id))) &&
            (!isBleTest || compareVersions(bleVersion, data.ble_version)) &&
            (!isCameraTest || compareVersions(camera_id, data.camera_version))) {
            base_state = 1;
        } else {
            base_state = 2;
        }

        TestItem test;

        // Check for BLE version
        if (isBleTest) {
            test.testItem = "蓝牙版本";
            test.testData = data.ble_version;
            test.ask = bleVersion;
            testItems.append(test);
        }

        // Check for Motor version
        if (isMotorTest) {
            test.testItem = "电机版本";
            test.testData = data.motor_version;
            test.ask = motorVersion;
            testItems.append(test);
        }

        // Check for Resource version
        if (isResourceTest) {
            test.testItem = "资源版本";
            test.testData = data.res_version;
            test.ask = resourceVersion;
            testItems.append(test);
        }

        // Check for Software version
        if (isSoftwareTest) {
            test.testItem = "软件版本";
            test.testData = data.soft_version;
            test.ask = softwareVersion;
            testItems.append(test);
        }

        // Check for Product name
        if (isProductTest) {
            test.testItem = "产品名字";
            test.testData = data.product_name;
            test.ask = productName;
            testItems.append(test);
        }

        // Check for Hardware version
        if (isHwTest) {
            test.testItem = "硬件版本";
            test.testData = data.hw_version;
            test.ask = hardwareVersion;
            testItems.append(test);
        }

        // Check for Algorithm version
        if (isAlgoTest) {
            test.testItem = "算法版本号";
            test.testData = data.algo_version;
            test.ask = algorithmVersion;
            testItems.append(test);
        }

        // Check for Pressure version
        if (isPresureTest) {
            test.testItem = "压感版本";
            test.testData = data.presure_version;
            test.ask = pressureSenseVersion;
            testItems.append(test);
        }

        // Check for FSensor version
        if (isFSensorTest) {
            test.testItem = "电机压感版本";
            test.testData = data.fsensor_version;
            test.ask = fsensorVersion;
            testItems.append(test);
        }

        // Check for IMU ID
        if (isImuTest) {
            test.testItem = "六轴id";
            test.testData = QString::number(data.imu_id);
            test.ask = imuId;
            testItems.append(test);
        }

        // Check for APP Protocol version
        if (isAppProtocolTest) {
            test.testItem = "APP的pb版本";
            test.testData = QString("%1").arg(data.pb_phone_ver);
            test.ask = appProtocolVersion;
            testItems.append(test);
        }

        // Check for Factory Protocol version
        if (isFactoryProtocolTest) {
            test.testItem = "工厂的pb版本";
            test.testData = QString("%1").arg(data.pb_factory_ver);
            test.ask = factoryProtocolVersion;
            testItems.append(test);
        }

        // Check for Camera ID
        if (isCameraTest) {
            test.testItem = "摄像头id";
            test.testData = data.camera_version;
            test.ask = camera_id;
            testItems.append(test);
        }

        updateTestData(testItems);
    }
}

void key_test::refreshPeriphData(ProtocolPeriphStateData data) {
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "flash_state" << data.flash_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "imu_state" << data.imu_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "magnet_state" << data.magnet_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "press_state" << data.press_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "audio_state" << data.audio_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
    << "battery_ic_state" << data.battery_ic_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
    << "touch_ic_state" << data.touch_ic_state;

    if (refresh_periph_times) {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "refresh_periph_times" << refresh_periph_times;
        refresh_periph_times = 0;

        QString imuStatus = SETTINGS.value("PeripheralStatus/IMU_Status").toString();
        QString flashStatus = SETTINGS.value("PeripheralStatus/Flash_Status").toString();
        QString magneticStatus = SETTINGS.value("PeripheralStatus/Magnetic_Status").toString();
        QString pressureStatus = SETTINGS.value("PeripheralStatus/Pressure_Status").toString();
        QString audioState = SETTINGS.value("PeripheralStatus/Audio_Status").toString();
        QString batteryIcStatus = SETTINGS.value("PeripheralStatus/BatteryIc_Status").toString();
        QString touchIcStatus = SETTINGS.value("PeripheralStatus/TouchIc_Status").toString();

        // 将布尔值转换为 QString
        QString flashStateStr = QString::number(data.flash_state);
        QString imuStateStr = QString::number(data.imu_state);
        QString audioStateStr = QString::number(data.audio_state);
        QString pressStateStr = QString::number(data.press_state);
        QString magnetStateStr = QString::number(data.magnet_state);
        QString batteryIcStateStr = QString::number(data.battery_ic_state);
        QString touchIcStateStr = QString::number(data.touch_ic_state);

        // 现在可以将 QString 类型的状态用于你的条件判断
        bool checkFlash = SETTINGS.value("PeripheralStatus/FlashStatus_checkBox").toBool();
        bool checkIMU = SETTINGS.value("PeripheralStatus/IMUStatus_checkBox").toBool();
        bool checkAudio = SETTINGS.value("PeripheralStatus/AudioStatus_checkBox").toBool();
        bool checkPressure = SETTINGS.value("PeripheralStatus/PressureStatus_checkBox").toBool();
        bool checkMagnetic = SETTINGS.value("PeripheralStatus/MagneticStatus_checkBox").toBool();
        bool checkBatteryIc = SETTINGS.value("PeripheralStatus/BatteryIcStatus_checkBox").toBool();
        bool checkTouchIc = SETTINGS.value("PeripheralStatus/TouchIcStatus_checkBox").toBool();

        if ((!checkFlash || compareVersions(flashStatus, flashStateStr)) &&
            (!checkIMU || compareVersions(imuStatus, imuStateStr)) &&
            (!checkAudio || compareVersions(audioState, audioStateStr)) &&
            (!checkPressure || compareVersions(pressureStatus, pressStateStr)) &&
            (!checkMagnetic || compareVersions(magneticStatus, magnetStateStr)) &&
            (!checkBatteryIc || compareVersions(batteryIcStatus, batteryIcStateStr)) &&
            (!checkTouchIc || compareVersions(touchIcStatus, touchIcStateStr))) {
            periph_state = 1;
        } else {
            periph_state = 2;
        }

        TestItem test;

        if (SETTINGS.value("PeripheralStatus/AudioStatus_checkBox").toBool()) {
            test.testItem = "功放状态";
            test.testData = QString::number(data.audio_state);
            test.ask = audioState;
            testItems.append(test);
        }

        if (SETTINGS.value("PeripheralStatus/FlashStatus_checkBox").toBool()) {
            test.testItem = "内存状态";
            test.testData = QString::number(data.flash_state);
            test.ask = flashStatus;
            testItems.append(test);
        }

        if (SETTINGS.value("PeripheralStatus/IMUStatus_checkBox").toBool()) {
            test.testItem = "imu状态";
            test.testData = QString::number(data.imu_state);
            test.ask = imuStatus;
            testItems.append(test);
        }

        if (SETTINGS.value("PeripheralStatus/MagneticStatus_checkBox").toBool()) {
            if (SETTINGS.value("SYSTEM/MagneticReuseMotorStatus").toBool())
                test.testItem = "马达状态";
            else
                test.testItem = "地磁状态";
            test.testData = QString::number(data.magnet_state);
            test.ask = magneticStatus;
            testItems.append(test);
        }

        if (SETTINGS.value("PeripheralStatus/PressureStatus_checkBox").toBool()) {
            test.testItem = "压感状态";
            test.testData = QString::number(data.press_state);
            test.ask = pressureStatus;
            testItems.append(test);
        }

        if (SETTINGS.value("PeripheralStatus/BatteryIcStatus_checkBox").toBool()) {
            test.testItem = "电池ic状态";
            test.testData = QString::number(data.battery_ic_state);
            test.ask = batteryIcStatus;
            testItems.append(test);
        }

        if (SETTINGS.value("PeripheralStatus/TouchIcStatus_checkBox").toBool()) {
            test.testItem = "触摸ic状态";
            test.testData = QString::number(data.touch_ic_state);
            test.ask = touchIcStatus;
            testItems.append(test);
        }

        updateTestData(testItems);
    }
}

void key_test::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到按键数据" << data;
    double normalValue = 0;
    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    if (keyProtocolType == Qusb::ProtocolType::LxModbus)
        normalValue = data.toDouble(&conversionOk) / 100;
    else if (keyProtocolType == Qusb::ProtocolType::HqModbus)
        normalValue = data.toDouble(&conversionOk) / 10000;
    else
        normalValue = data.toDouble(&conversionOk) * 1000;

    if (conversionOk) {
        // 转换成功
        qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;

        showlog("转换后的数值：" + QString::number(normalValue, 'f', 8) + "ma");
    } else {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

key_test::~key_test() {
    qDebug() << getIndex() << "已进入析构";
    isTestContinue = 0;
    closeDongleSerialPort();
    closeUsbSerialPort();
    closeJigSerialPort();

    delete ui;
}

void key_test::refreshSn(ProtocolSnData data) {

    QString tail_sn_string = data.value;
    ui->product_sn->setText("整机sn:" + tail_sn_string);
    showlog("读取的sn为" + tail_sn_string);
    showlog("写入的sn为" + stringsn);
    if (tail_sn_string == stringsn) {
        TestItem test;
        test.testItem = "读取的sn";
        test.testData = tail_sn_string;
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);
        testResultTableUpdate(testItems);

        snCompareOk = 1;

    } else {
        TestItem test;
        test.testItem = "读取的sn";
        test.testData = tail_sn_string;
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);
        testResultTableUpdate(testItems);
        snCompareOk = 2;
    }
    }

void key_test::on_snInput_returnPressed() {
    clearDisplay();
    macAddress = "没有mac地址";
    logString = "";
    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    applyAdaptiveV3ProductBySn(ui->snInput);
    // 与按键测试工站保持一致：仅使用公司SN规则校验（字母数字且长度>12）

     // 检查是否是序列号格式
     QRegularExpression snRegex(snPattern);
     // 使用正则表达式匹配
     if (!snRegex.match(ui->snInput->text()).hasMatch()) {
         ui->snInput->setDisabled(0);
         ui->macInput->setDisabled(0);
         showlog("序列号错误");
         showlog("实际长度为" + QString::number(ui->snInput->text().length()));
         showlog("要求格式为" + snPattern);
         ui->snInput->clear();
         ui->snInput->setFocus();
         return;
     }
    showlog("正在查询mac地址");
    emit send_startTest(getIndex());
    appendStationResult(testItems, "主板条码", "0.0000", passValue);
    testResultTableUpdate(testItems);
    // 获取比亚迪mes的sn校验规则
    processGetMesTestValue();
    // MES站前检测，成功再开始测试
    if (ui->isusemes->checkState()) {
        processInspection(ui->snInput->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
}

void key_test::processGetMesTestValue() {
    if (pack.factory == "hz") {
        pack.sn = ui->snInput->text();
        pack.mechines = getIndex();
        getTestValue(getIndex(), pack.sn.trimmed());
        return;
    }
    if (ui->isformmes->checkState()) {
        pack.sn = ui->snInput->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}

void key_test::getTestValue(const int mechines, const QString value) {
    // showlog(value);
    QString mesmacAddress;
    if (pack.factory == "hq") {
        // 定义正则表达式，匹配MAC地址的模式
        static const QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");

        // 在数据中查找匹配的内容
        QRegularExpressionMatch match = regex.match(value);

        // 检查是否有匹配项
        if (match.hasMatch()) {
            // 提取MAC地址
            mesmacAddress = match.captured(1);
            qDebug() << getIndex() << "MAC地址:" << mesmacAddress;
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

        stringsn = snFromMes;
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        on_macInput_returnPressed();
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

        stringsn = snFromMes;
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

    // bandingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试
}

void key_test::on_macInput_returnPressed() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    

    // 检查是否是mac格式
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: "
                                       "10px; padding: 10px; text-align: center; ");

        qDebug() << getIndex() << macAddress;
        // 主状态机流程
        isTestContinue = true;
        emit send_go_next_focus();
        state = STATE_IDLE;
    }
}

void key_test::clearDisplay() {
    ui->msgEdit->clear();
    testResultTableInit();
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->product_sn->clear();
    ui->product_sn->setText("整机sn:");
    ui->log->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    ui->macInput->clear();
}
void key_test::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        showlog("蓝牙连接成功");
        protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        qDebug() << "蓝牙连接断开";

        // showlog("蓝牙连接断开");
    }
}

void key_test::refreshProductUartState(int state) {
    if (state)
        showlog("product串口连接成功");
    else {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        showlog("product串口连接断开");
    }
}

void key_test::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void key_test::refreshJigUartState(int state) {
    if (state)
        showlog("治具串口连接成功");
    else {
        ui->jigComNameCombo->setEnabled(true);
        ui->jigConnectButton->setEnabled(true);
        showlog("治具串口连接断开");
    }
}

void key_test::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        ui->usbcomNameCombo->setEnabled(true);
        ui->usbconnectButton->setEnabled(true);
        showlog("usb串口连接断开");
    }
}
void key_test::on_connectButton_clicked() {
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void key_test::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}
void key_test::on_usbconnectButton_clicked() {
    openUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(false);
    ui->usbconnectButton->setEnabled(false);
}

void key_test::on_usbdisconnectButton_clicked() {
    closeUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(true);
    ui->usbconnectButton->setEnabled(true);
}

void key_test::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void key_test::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void key_test::on_jigConnectButton_clicked() {
    openJigSerialPort();
    ui->jigComNameCombo->setEnabled(false);
    ui->jigConnectButton->setEnabled(false);
}

void key_test::on_jigDisconnectButton_clicked() {
    closeJigSerialPort();
    ui->jigComNameCombo->setEnabled(true);
    ui->jigConnectButton->setEnabled(true);
}

void key_test::processInspection(QString stringsn) {
    if (stringsn != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = stringsn;
            pack.mechines = getIndex();
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


void key_test::startFlowWithMac(const QString& mac) {
    usblogwaittime->stop();
    firstconnectbrush = 0;
    ui->macInput->setDisabled(1);
    ui->macInput->setText(mac);
    macAddress = mac;
    last_macAddress = macAddress;
    ui->macLabel->setText("蓝牙mac: " + macAddress);

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // if (!usbSerialPort->isOpen()) {
    //     on_usbconnectButton_clicked();
    // }
    // if (pack.factory == "xwd" && !jigSerialPort->isOpen()) {
    //     openJigSerialPort();
    // }
    // jig->set_cylinder_state(1, getIndex());
    // bandingMacSn(macAddress, stringsn);
    state = STATE_IDLE;
    isTestContinue = true;
}



void key_test::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
            case STATE_IDLE:  // 复位一切

                // usb->sendPowerInstruction(Qusb::PowerAction::ConfigurePowerSupply);

                protocolManager.resetAllPb();
                isovertime = 0;
                KeyPowerState = 0;
                KeyStartPauseState = 0; 
                KeyModeState = 0;
                KeySpeedState = 0;
                KeyProgramState = 0;
                KeyLeftState = 0;
                KeyRightState = 0;
                keyErrorDetail.clear();
                keyPassDetail.clear();
                lastKeyButtonId = -1;
                lastKeyButtonMs = 0;
                keyButtonDebounceTimer.invalidate();
                closeKeyWaitPromptProgrammatically();
                keyWaitPromptShown = false;
                plcKeyActionStarted = false;
                ++plcKeyWaitSeq;
                factoryModeLogged = false;
                totalresult = "";
                pack.itemvalue.clear();
                at->resetConnected();
                measure_ammeter = 0;
                waitWork(1000);
                at->set(DongleCmd::BleScanConnect, ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());
                showlog("已经发送mac地址");
                TestTime.start();
                state = STATE_SET_TEST_MODE;

                break;

            case STATE_SET_TEST_MODE:
                if (at->getConnected()) {
                    showlog("设置工厂模式");
                    sendCommandWithRetry([&]() { 
                        protocolManager.set(DeviceCmd::FacMode, 1);
                    });
                    waitWork(300);
                    state = STATE_WAIT_GET_KEY_POWER_STATE;
                }
                break;

            // 等待获取电源键状态
            case STATE_WAIT_GET_KEY_POWER_STATE:
                if (canGoNext) {
                    if (!factoryModeLogged) {
                        showlog("已进入工厂模式");
                        appendStationResult(testItems, "进入工厂模式", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        factoryModeLogged = true;
                    }
                    if (KeyPowerState != 0) {
                        closeKeyWaitPromptProgrammatically();
                        if (KeyPowerState == 1) {
                            appendStationResult(testItems, "电源键测试",
                                                keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                                passValue);
                            testResultTableUpdate(testItems);
                            keyWaitPromptShown = false;
                            showlog("电源按钮测试通过");
                            state = STATE_WAIT_GET_KEY_MODE_STATE;
                        } else {
                            appendStationResult(testItems, "电源键测试", currentKeyFailureDetail(), failValue);
                            testResultTableUpdate(testItems);
                            keyWaitPromptShown = false;
                            totalresult = failValue;
                            state = STATE_SAVE_RESULT;
                        }
                    } else {
                        startPlcClickerAndWaitKey(QStringLiteral("电源键测试"), 6);
                    }
                }
                break;
            // 等待获取模式键状态
            case STATE_WAIT_GET_KEY_MODE_STATE:
                if (KeyModeState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyModeState == 1) {
                        showlog("Mode键短按");
                        appendStationResult(testItems, "Mode键测试",
                                            keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                            passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("Mode键测试通过");
                        state = STATE_WAIT_GET_KEY_PROGRAM_STATE;
                    } else {
                        appendStationResult(testItems, "Mode键测试", currentKeyFailureDetail(), failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    startPlcClickerAndWaitKey(QStringLiteral("Mode键测试"), 0);
                }
                break; 
            // 等待获取程序键状态
            case STATE_WAIT_GET_KEY_PROGRAM_STATE:
                if (KeyProgramState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyProgramState == 1) {
                        showlog("Program键短按");
                        appendStationResult(testItems, "Program键测试",
                                            keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                            passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("Program键测试通过");
                        state = STATE_WAIT_GET_KEY_SPEED_STATE;
                    } else {
                        appendStationResult(testItems, "Program键测试", currentKeyFailureDetail(), failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    startPlcClickerAndWaitKey(QStringLiteral("Program键测试"), 1);
                }
                break;

            
            // 等待获取速度键状态
            case STATE_WAIT_GET_KEY_SPEED_STATE:
                if (KeySpeedState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeySpeedState == 1) {
                        showlog("Speed键短按");
                        appendStationResult(testItems, "Speed键测试",
                                            keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                            passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("Speed键测试通过");
                        state = STATE_WAIT_GET_KEY_STARTPAUSE_STATE;
                    } else {
                        appendStationResult(testItems, "Speed键测试", currentKeyFailureDetail(), failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    startPlcClickerAndWaitKey(QStringLiteral("Speed键测试"), 2);
                }
                break;
            // 等待获取开始/暂停键状态
            case STATE_WAIT_GET_KEY_STARTPAUSE_STATE:
                if (KeyStartPauseState != 0) {
                    showlog("Start/Pause开始/暂停键状态：" + QString::number(KeyStartPauseState));
                    closeKeyWaitPromptProgrammatically();
                    if (KeyStartPauseState == 1) {
                        showlog("Start/Pause开始/暂停键短按");
                        appendStationResult(testItems, "Start/Pause键测试",
                                            keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                            passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("Start/Pause键测试通过");
                        state = STATE_WAIT_GET_KEY_LEFT_STATE;
                    } else {
                        appendStationResult(testItems, "开始/暂停键测试", currentKeyFailureDetail(), failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    startPlcClickerAndWaitKey(QStringLiteral("Start/Pause键测试"), 4);
                }
                break;
           
            // 等待获取左键状态
            case STATE_WAIT_GET_KEY_LEFT_STATE:
                if (KeyLeftState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyLeftState == 1) {
                        showlog("左键短按");
                        appendStationResult(testItems, "左键测试",
                                            keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                            passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("左键测试通过");
                        state = STATE_WAIT_GET_KEY_RIGHT_STATE;
                    } else {
                        appendStationResult(testItems, "左键测试", currentKeyFailureDetail(), failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    startPlcClickerAndWaitKey(QStringLiteral("左键测试"), 5);
                }
                break;
            // 等待获取右键状态
            case STATE_WAIT_GET_KEY_RIGHT_STATE:
                if (KeyRightState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyRightState == 1) {
                        showlog("右键短按");
                        appendStationResult(testItems, "右键测试",
                                            keyPassDetail.isEmpty() ? QStringLiteral("0.0000") : keyPassDetail,
                                            passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("右键测试通过");
                        totalresult = passValue;
                        state = STATE_SAVE_RESULT;
                    } else {
                        appendStationResult(testItems, "右键测试", currentKeyFailureDetail(), failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    startPlcClickerAndWaitKey(QStringLiteral("右键测试"), 3);
                }
                break;
            case STATE_SAVE_RESULT: {
                const QString endedSn = ui->snInput->text();
                stringsn = "";
                if (totalresult == passValue) {
                    pack.result = "PASS";
                    pack.sn = endedSn;
                    pack.mechines = getIndex();
                    pack.instruct_num = "079";
                    pack.itemvalue = pack.sn + "," + macAddress + ",KEY_TEST_RESULT*" + pack.result +
                                     QString("@KEY_TEST*0");
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                        showlog("Running send_end_testPass");
                        appendStationResult(testItems, "MES完成上报", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                    }

                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((totalresult == failValue)) {
                    pack.result = "NG";
                    pack.sn = endedSn;
                    pack.mechines = getIndex();
                    pack.instruct_num = "079";
                    pack.itemvalue = pack.sn + "," + macAddress + ",KEY_TEST_RESULT*" + pack.result +
                                     QString("@KEY_TEST*0");
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                        showlog("Running send_end_testPass");
                        appendStationResult(testItems, "MES完成上报", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                    }

                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }

                showlog("测试结束，sn为：" + endedSn);
                ui->macInput->clear();
                ui->snInput->clear();

                isTestContinue = false;  // 结束

                // if (SETTINGS.value("SYSTEM/KeyMechine").toInt() == 3 ||
                //     SETTINGS.value("SYSTEM/KeyMechine").toInt() == 2) {
                // } else {
                //     if (pack.factory == "xwd")
                //         jig->set_cylinder_state(0, getIndex());
                // }

                on_disconnectButton_clicked();
                on_usbdisconnectButton_clicked();
                ui->snInput->setDisabled(0);
                ui->macInput->setDisabled(1);
                ui->getMac->setDisabled(0);
                emit send_end_test(getIndex());

                state = STATE_IDLE;
                break;
            }
        }
        //   QCoreApplication::processEvents();
    }
}

void key_test::on_pushButton_clicked() {
    // 开发测试入口：改为模拟SN扫码触发，MAC自动解析。
    // ui->snInput->setText("U03000077I1H00007D");
    // on_snInput_returnPressed();
    at->set(DongleCmd::BleScanConnect, ui->macInput->text()); 
    sendCommandWithRetry([&]() { 
        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn})); 
    });
    // sendjigData(STATE_CYLINDER_RESET);

    // bandingMacSn(macAddress, stringsn);
    //     save_brush_log("dataTemp");
}

void key_test::on_pushButton_3_clicked() {
    usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement);

    // at->get(DongleCmd::GetGmac);
    // MesInit();
}
void key_test::processReceivedData(const QByteArray& data) {
    // 将接收到的数据添加到日志字符串中
    logString += data;

    // 查找日志中最后一个 "Local Board" 条目的索引
    int lastIndex = logString.lastIndexOf("Local Board");

    if (lastIndex != -1) {
        // 从最后一个 "Local Board" 条目开始查找 MAC 地址
        int macIndex = logString.indexOf('=', lastIndex);

        if (macIndex != -1 &&
            logString.length() >= macIndex + 18) {  // 判断是否包含完整的 MAC 地址（17 个字符 + 1 个分隔符）
            // 提取 MAC 地址
            QString macAddress = logString.mid(macIndex + 1, 17).trimmed();  // MAC 地址长度为17，并去除首尾空格
            qDebug() << "提取到的MAC地址：" << macAddress;

            // 清空日志字符串
            logString = "";

            // 在界面上设置 MAC 地址
            ui->macInput->setText(macAddress);

            // 执行 USB 断开操作
            // on_productDisconnectButton_clicked();

            if (firstconnectbrush) {
                on_macInput_returnPressed();
            }
            // 在这里可以将提取到的 MAC 地址用于后续处理
        } else {
            logString = "";
            at->get(DongleCmd::GetGmac);
            showlog("日志数据中未找到完整的MAC地址，正在重发请求获取mac地址");
        }
    } else {
        // qDebug() << getIndex()<< "日志数据中未找到mac地址关键字。";
    }
}

void key_test::on_pushButton_4_clicked() {
    static int clickStep = 1;  // 用于跟踪当前运行的步骤
    pack.mechines = 1;
    pack.sn = "U03000077I1H00007D";

    pack.result = "PASS";
    pack.line = "1C3A04";
    pack.itemvalue = QString("|BTMAC:3C:84:27:07:A8:D2|") + QString("KEY:PASS|");

    switch (clickStep) {
        case 1: showlog("Running processInspection"); break;

        case 2:
            emit send_end_testPass(pack);
            showlog("Running send_end_testPass");
            break;
    }

    clickStep++;  // 增加步骤计数

    // 如果步骤计数超过了最大步骤数，重置为第一步
    if (clickStep > 2) {
        clickStep = 1;
    }
}

void key_test::bandingMacSn(QString bandingmac, QString bandingsn) {
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
}

void key_test::on_stopTest_clicked() {
    showlog("触发停止测试");
    ++plcKeyWaitSeq;
    plcKeyActionStarted = false;
    refresh_key_times = 0;
    inovancePlcTcp_.disconnect();
    usblogwaittime->stop();
    ui->macInput->clear();
    ui->snInput->clear();
    ui->snInput->setFocus();
    isTestContinue = false;  // 结束
    if (pack.factory != "jj") {
        jig->set_cylinder_state(0, getIndex());
    }
    ui->snInput->setDisabled(0);
    ui->macInput->setDisabled(1);
    ui->getMac->setDisabled(0);
}





