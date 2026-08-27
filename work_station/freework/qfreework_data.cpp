#include "qfreework.h"

#include <algorithm>

#include <QEvent>
#include <QMouseEvent>

#include "common_utils.h"
#include "qproduct.h"
#include "qprotocol_types.h"
#include "test_case.h"
#include "agreement/mes_protocol/device/byd_mes/bydmes.h"
#include "ui_qfreework.h"

namespace {

enum class TuplePositionKind {
    Unknown = -1,
    Left = 0,
    Right = 1,
    Single = 2,
    Unspecified = 3,
};

TuplePositionKind parseTuplePositionKind(const QString& raw) {
    const QString position = raw.trimmed();
    if (position.isEmpty()) {
        return TuplePositionKind::Unspecified;
    }
    const QChar first = position.at(0).toUpper();
    // 兼容输入：1/L/左、2/R/右、3/S/单、F/未指定；上传统一写成 1/2/3/F
    if (position == QStringLiteral("1") || first == QLatin1Char('L') || position.contains(QStringLiteral("左"))) {
        return TuplePositionKind::Left;
    }
    if (position == QStringLiteral("2") || first == QLatin1Char('R') || position.contains(QStringLiteral("右"))) {
        return TuplePositionKind::Right;
    }
    if (position == QStringLiteral("3") || first == QLatin1Char('S') || position.contains(QStringLiteral("单"))) {
        return TuplePositionKind::Single;
    }
    if (first == QLatin1Char('F') || position.contains(QStringLiteral("未指定"))) {
        return TuplePositionKind::Unspecified;
    }
    return TuplePositionKind::Unknown;
}

QString tuplePositionKindText(TuplePositionKind kind) {
    switch (kind) {
    case TuplePositionKind::Left:
        return QStringLiteral("左");
    case TuplePositionKind::Right:
        return QStringLiteral("右");
    case TuplePositionKind::Single:
        return QStringLiteral("单只");
    case TuplePositionKind::Unspecified:
        return QStringLiteral("未指定");
    default:
        return QStringLiteral("未知");
    }
}

QString tuplePositionCode(TuplePositionKind kind) {
    // 云端 applyTupleByMac 仅认 1/2/3/F（不再传 L/R/S）
    switch (kind) {
    case TuplePositionKind::Left:
        return QStringLiteral("1");
    case TuplePositionKind::Right:
        return QStringLiteral("2");
    case TuplePositionKind::Single:
        return QStringLiteral("3");
    case TuplePositionKind::Unspecified:
        return QStringLiteral("F");
    default:
        return QString();
    }
}

/** 与 Qaiot device_side_id 对齐：未指定按 Independent(2) */
int deviceSideIdFromKind(TuplePositionKind kind) {
    switch (kind) {
    case TuplePositionKind::Left:
        return 0;
    case TuplePositionKind::Right:
        return 1;
    case TuplePositionKind::Single:
    case TuplePositionKind::Unspecified:
        return 2;
    default:
        return 2;
    }
}

bool caseNeedsDeviceSideParam(const TestCaseDefinition& def) {
    if (def.send.deviceCmd == QStringLiteral("ApplyTupleByMac"))
        return true;
    if (def.send.channel != TestCaseSendChannel::Product)
        return false;
    if (def.send.productProtocol != TestCaseProductProtocol::Qaiot)
        return false;
    const QString& cmd = def.send.deviceCmd;
    return cmd == QStringLiteral("Sn") || cmd == QStringLiteral("WriteKey") || cmd == QStringLiteral("MacWrite")
           || cmd == QStringLiteral("MacRead") || cmd == QStringLiteral("TupleRead");
}

constexpr char kTuplePosInactiveStyle[] =
    "font-size: 18px; background-color: #808080; color: black; border-radius: 6px; padding: 4px 12px;";
constexpr char kTuplePosActiveStyle[] =
    "font-size: 18px; background-color: #00FF00; color: black; border: 2px solid black; border-radius: 6px; "
    "padding: 4px 12px;";

/** 整机 SN 第 9～11 位（1 起算，共 3 位）须与申请三元组时上传的 sku 一致（如 …BBBBPH9…） */
bool wholeMachineSnEmbeddedSkuMatches(const QString& wholeSn, const QString& sku, QString* detailOut) {
    const QString sn = wholeSn.trimmed();
    const QString expect = sku.trimmed();
    if (expect.isEmpty()) {
        if (detailOut)
            *detailOut = QStringLiteral("SKU为空");
        return false;
    }
    if (sn.size() < 11) {
        if (detailOut)
            *detailOut = QStringLiteral("SN长度不足11位");
        return false;
    }
    const QString embedded = sn.mid(8, 3);
    if (embedded.compare(expect, Qt::CaseInsensitive) != 0) {
        if (detailOut) {
            *detailOut =
                QStringLiteral("SN第9-11位=%1与SKU=%2不一致").arg(embedded, expect);
        }
        return false;
    }
    return true;
}

} // namespace

void QFreeWork::resetTuplePositionHighlight() {
    ui->label_tuplePosLeft->setStyleSheet(kTuplePosInactiveStyle);
    ui->label_tuplePosRight->setStyleSheet(kTuplePosInactiveStyle);
    ui->label_tuplePosSingle->setStyleSheet(kTuplePosInactiveStyle);
    ui->label_tuplePosUnspecified->setStyleSheet(kTuplePosInactiveStyle);
}

void QFreeWork::updateTuplePositionUiVisible() {
    bool show = false;
    auto scanList = [this, &show](const QStringList& names) {
        for (const QString& caseName : names) {
            TestCaseDefinition def;
            if (!TestCaseRunner::loadCaseForStation(activeFlowStationKey_, caseName, def))
                continue;
            if (caseNeedsDeviceSideParam(def)) {
                show = true;
                break;
            }
        }
    };
    scanList(orderedTestCaseNames_);
    if (!show)
        scanList(orderedFailCaseNames_);
    ui->label_tuplePositionCaption->setVisible(show);
    ui->label_tuplePosLeft->setVisible(show);
    ui->label_tuplePosRight->setVisible(show);
    ui->label_tuplePosSingle->setVisible(show);
    ui->label_tuplePosUnspecified->setVisible(show);
    if (!show)
        resetTuplePositionHighlight();
}

void QFreeWork::setupTuplePositionClickable() {
    const QList<QLabel*> labels = {ui->label_tuplePosLeft, ui->label_tuplePosRight, ui->label_tuplePosSingle,
                                   ui->label_tuplePosUnspecified};
    for (QLabel* label : labels) {
        label->setCursor(Qt::PointingHandCursor);
        label->setToolTip(QStringLiteral("点击设置当前工站左右位（写入 flow.ini 并同步相关步骤）"));
        label->installEventFilter(this);
    }
}

void QFreeWork::loadAndApplyStationDeviceSide() {
    const TestCaseDeviceSideConfig cfg = TestCaseStore::loadStationDeviceSideConfig(activeFlowStationKey_);
    QString position = cfg.position.trimmed();
    if (position.isEmpty() && cfg.sideId >= 0 && cfg.sideId <= 2) {
        if (cfg.sideId == 0)
            position = QStringLiteral("1");
        else if (cfg.sideId == 1)
            position = QStringLiteral("2");
        else
            position = QStringLiteral("3");
    }
    if (position.isEmpty())
        position = SETTINGS.value(QStringLiteral("Tuple/Position")).toString().trimmed();
    if (position.isEmpty())
        return;
    SETTINGS.setValue(QStringLiteral("Tuple/Position"), tuplePositionCode(parseTuplePositionKind(position)));
    updateTuplePositionHighlight(position);
}

int QFreeWork::syncDeviceSideToStationSteps(int sideId, const QString& positionCode) {
    if (activeFlowStationKey_.isEmpty() || sideId < 0 || sideId > 2)
        return 0;

    QStringList names = orderedTestCaseNames_;
    for (const QString& n : orderedFailCaseNames_) {
        if (!names.contains(n))
            names.append(n);
    }

    int updated = 0;
    for (const QString& caseName : names) {
        TestCaseDefinition def;
        if (!TestCaseRunner::loadCaseForStation(activeFlowStationKey_, caseName, def))
            continue;
        if (!caseNeedsDeviceSideParam(def))
            continue;

        if (def.send.deviceCmd == QStringLiteral("ApplyTupleByMac")) {
            QVariantMap map;
            if (def.send.param.canConvert<QVariantMap>())
                map = def.send.param.toMap();
            else if (def.send.param.type() == QVariant::String) {
                const QString mac = def.send.param.toString().trimmed();
                if (!mac.isEmpty())
                    map.insert(QStringLiteral("mac"), mac);
            }
            map.insert(QStringLiteral("position"), positionCode);
            def.send.param = map;
        } else if (def.send.param.canConvert<DeviceSnPayload>()) {
            DeviceSnPayload payload = def.send.param.value<DeviceSnPayload>();
            payload.sideId = sideId;
            def.send.param = QVariant::fromValue(payload);
        } else {
            QVariantMap map;
            if (def.send.param.canConvert<QVariantMap>())
                map = def.send.param.toMap();
            else if (def.send.param.type() == QVariant::String) {
                const QString text = def.send.param.toString().trimmed();
                if (!text.isEmpty()) {
                    if (def.send.deviceCmd == QStringLiteral("MacWrite") || def.send.deviceCmd == QStringLiteral("MacRead"))
                        map.insert(QStringLiteral("value"), text);
                    else
                        map.insert(QStringLiteral("value"), text);
                }
            }
            map.insert(QStringLiteral("side"), sideId);
            def.send.param = map;
        }

        if (TestCaseStore::saveCaseForStation(activeFlowStationKey_, def))
            ++updated;
    }
    return updated;
}

void QFreeWork::applyTuplePositionSelection(const QString& positionCode) {
    const TuplePositionKind kind = parseTuplePositionKind(positionCode);
    if (kind == TuplePositionKind::Unknown)
        return;

    const QString code = tuplePositionCode(kind);
    const int sideId = deviceSideIdFromKind(kind);

    TestCaseDeviceSideConfig cfg;
    cfg.position = code;
    cfg.sideId = sideId;
    if (!TestCaseStore::saveStationDeviceSideConfig(activeFlowStationKey_, cfg)) {
        showlog(QStringLiteral("三元组位置：保存工站配置失败"));
        return;
    }

    SETTINGS.setValue(QStringLiteral("Tuple/Position"), code);
    updateTuplePositionHighlight(code);

    const int n = syncDeviceSideToStationSteps(sideId, code);
    showlog(QStringLiteral("三元组位置已设为%1（%2），device_side_id=%3，已更新%4个步骤")
                .arg(tuplePositionKindText(kind), code)
                .arg(sideId)
                .arg(n));
}

bool QFreeWork::eventFilter(QObject* watched, QEvent* event) {
    if (event && event->type() == QEvent::MouseButtonRelease) {
        const auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton) {
            QString code;
            if (watched == ui->label_tuplePosLeft)
                code = QStringLiteral("L");
            else if (watched == ui->label_tuplePosRight)
                code = QStringLiteral("R");
            else if (watched == ui->label_tuplePosSingle)
                code = QStringLiteral("S");
            else if (watched == ui->label_tuplePosUnspecified)
                code = QStringLiteral("F");
            if (!code.isEmpty()) {
                applyTuplePositionSelection(code);
                return true;
            }
            if (watched == ui->label_screenInspectShot || watched == ui->label_screenInspectRef) {
                if (!screenInspectCapture_.isNull())
                    showScreenInspectViewer();
                return true;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void QFreeWork::applyStationSerialUiConfig() {
    const TestCaseSerialUiConfig cfg = TestCaseStore::loadStationSerialUiConfig(activeFlowStationKey_);
    auto centeredLabel = [](const QString& text) {
        return QStringLiteral("<html><head/><body><p align=\"center\">%1</p></body></html>")
            .arg(text.toHtmlEscaped());
    };

    ui->label_8->setText(centeredLabel(cfg.jigLabel));
    ui->label_8->setVisible(cfg.jigVisible);
    ui->jigComNameCombo->setVisible(cfg.jigVisible);
    ui->jigConnectButton->setVisible(cfg.jigVisible);
    ui->jigDisconnectButton->setVisible(cfg.jigVisible);

    ui->label_productInst->setText(centeredLabel(cfg.productLabel));
    ui->label_productInst->setVisible(cfg.productVisible);
    ui->productComNameCombo->setVisible(cfg.productVisible);
    ui->productConnectButton->setVisible(cfg.productVisible);
    ui->productDisconnectButton->setVisible(cfg.productVisible);

    ui->label_7->setText(centeredLabel(cfg.usbLabel));
    ui->label_7->setVisible(cfg.usbVisible);
    ui->usbcomNameCombo->setVisible(cfg.usbVisible);
    ui->usbconnectButton->setVisible(cfg.usbVisible);
    ui->usbdisconnectButton->setVisible(cfg.usbVisible);
}

void QFreeWork::updateTuplePositionHighlight(const QString& position) {
    resetTuplePositionHighlight();
    QLabel* target = nullptr;
    switch (parseTuplePositionKind(position)) {
    case TuplePositionKind::Left:
        target = ui->label_tuplePosLeft;
        break;
    case TuplePositionKind::Right:
        target = ui->label_tuplePosRight;
        break;
    case TuplePositionKind::Single:
        target = ui->label_tuplePosSingle;
        break;
    case TuplePositionKind::Unspecified:
        target = ui->label_tuplePosUnspecified;
        break;
    default:
        break;
    }
    if (target) {
        target->setStyleSheet(kTuplePosActiveStyle);
    }
}

// 协议 / 治具 / dongle 回包：解析与条件判定（仍为 QFreeWork 成员，仅拆到本翻译单元）

void QFreeWork::onProductInstrumentStopReceiveAckForPer(int recvPkts) {
    if (productInstrumentStopWaitStepName_.isEmpty()) {
        qDebug() << "[FreeWork][StopRxAck] 忽略：未登记等待步骤 recvPkts=" << recvPkts << "工位=" << getIndex();
        return;
    }
    const QString stepName = productInstrumentStopWaitStepName_;
    if (!isCurrentInstrumentStep(stepName)) {
        qDebug() << "[FreeWork][StopRxAck] 忽略：非当前步骤 期待=" << stepName << "recvPkts=" << recvPkts
                 << "工位=" << getIndex();
        return;
    }
    if (stepRuntime_.done) {
        qDebug() << "[FreeWork][StopRxAck] 忽略：本步已结束 step=" << stepName << "recvPkts=" << recvPkts << "工位=" << getIndex();
        return;
    }
    productInstrumentStopWaitStepName_.clear();
    finishCommandRetryWait(true, QString());
    const int sendCount = SETTINGS.value(QStringLiteral("BrushInstrument/InstrumentSendPacketCount"), 1000).toInt();
    const double maxPer = SETTINGS.value(QStringLiteral("BrushInstrument/MaxPer"), 0.05).toDouble();
    const double per = Qproduct::computePer(sendCount, recvPkts);
    const bool pass = (recvPkts >= 0) && (per <= maxPer);
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = QStringLiteral("仪器发包数=%1 收包=%2 PER=%3 门限<=%4")
                                .arg(sendCount)
                                .arg(recvPkts)
                                .arg(per, 0, 'f', 4)
                                .arg(maxPer, 0, 'f', 4);
    if (!pass) {
        TestResult = failValue;
    }
    showlog(stepName + (pass ? QStringLiteral("通过 ") : QStringLiteral("失败 ")) + stepRuntime_.testData);
}

bool QFreeWork::isCurrentStep(const QString& functionName) const {
    if (!stepRuntime_.started) {
        return false;
    }
    return isActiveTestCaseStep(functionName);
}

bool QFreeWork::isCurrentInstrumentStep(const QString& stepName) const {
    if (isCurrentStep(stepName))
        return true;
    return isActiveTestCaseStep(stepName);
}

void QFreeWork::appendPeriphItem(QVector<TestItem>& periphTestItems, bool& pass, const QString& name,
                                 const QString& value, const QString& expect, bool needCompare) {
    if (!needCompare) {
        return;
    }
    TestItem item;
    item.testItem = name;
    item.testData = value;
    item.ask = expect;
    item.testResult = compareVersions(expect, value) ? "通过" : "失败";
    if (item.testResult == "失败") {
        pass = false;
    }
    periphTestItems.append(item);
}
void QFreeWork::refreshBaseData(ProtocolBaseInfoData data) {
    const QString productName = SETTINGS.value("ProductInfo/Product_Name").toString();
    QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
    QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();
    QString Age_State = SETTINGS.value("ProductInfo/Age_State").toString();
    const bool isProductTest = SETTINGS.value("ProductInfo/ProductName_checkBox").toBool();
    const bool isSoftwareTest = SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool();
    const bool isResourceTest = SETTINGS.value("ProductInfo/ResourceVersion_checkBox").toBool();
    const bool isAgingStatusTest = SETTINGS.value("ProductInfo/AgingStatus_checkBox").toBool();

    wifiMac.clear();
    for (int var = 0; var < data.wifi_mac.size; ++var) {
        wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
        if (var < data.wifi_mac.size - 1)
            wifiMac += ":";
    }
    qDebug() << getIndex() << "设备的 wifiMac:" << wifiMac;

    if (testCaseStepActive_ && activeTestCase_.gate.reportType == QStringLiteral("ProtocolBaseInfoData")) {
        softwareVersionForReport_ = data.soft_version;
        const QString actualSoftwareVersion = data.soft_version.trimmed();
        QString expectedSoftwareVersion = activeTestCase_.gate.expected.trimmed();
        if (expectedSoftwareVersion.isEmpty() && !activeTestCase_.gate.expectedSettingsKey.isEmpty()) {
            expectedSoftwareVersion =
                SETTINGS.value(activeTestCase_.gate.expectedSettingsKey).toString().trimmed();
        }
        softwareVersionPassForReport_ =
            expectedSoftwareVersion.isEmpty() || compareVersions(expectedSoftwareVersion, actualSoftwareVersion);
    }

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(data)))
        return;

    if (isCurrentStep(QStringLiteral("读取版本号")) || isCurrentStep(QStringLiteral("获取基本信息"))) {
        // 旧路径：无 case Gate 时走设置页 ProductInfo 勾选/期望（见下方）
    } else if (testCaseStepActive_ && !activeTestCase_.gate.enabled
               && activeTestCase_.send.channel == TestCaseSendChannel::Product
               && activeTestCase_.send.action == TestCaseSendAction::Get) {
        // 无 Gate 的读名称/版本：有业务上报即过步，并把实测写入表格
        const QString cmd = activeTestCase_.send.deviceCmd;
        QString testData;
        if (cmd == QStringLiteral("DeviceInfo") || cmd == QStringLiteral("BaseInfo"))
            testData = data.product_name.trimmed();
        else if (cmd == QStringLiteral("SoftVersionRead"))
            testData = data.soft_version.trimmed();
        if (cmd == QStringLiteral("DeviceInfo") || cmd == QStringLiteral("BaseInfo")
            || cmd == QStringLiteral("SoftVersionRead")) {
            if (testData.isEmpty())
                testData = QStringLiteral("ok");
            markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
            showlog(QStringLiteral("读取完成：%1").arg(testData));
            return;
        }
        return;
    } else {
        return;
    }

    QVector<TestItem> baseItems;
    baseItems.reserve(4);
    bool pass = true;
    softwareVersionForReport_ = data.soft_version;
    const QString actualSoftwareVersion = data.soft_version.trimmed();
    const QString expectedSoftwareVersion = softwareVersion.trimmed();
    const QStringList expectedSoftwareVersions = expectedSoftwareVersion.split("=", QString::SkipEmptyParts);
    softwareVersionPassForReport_ = !isSoftwareTest || expectedSoftwareVersions.contains(actualSoftwareVersion) ||
        expectedSoftwareVersion == actualSoftwareVersion;
    qDebug() << "[Tuple] software version report, actual =" << actualSoftwareVersion
             << "expected =" << expectedSoftwareVersion
             << "pass =" << softwareVersionPassForReport_;
    appendPeriphItem(baseItems, pass, "产品名称", data.product_name, productName, isProductTest);
    appendPeriphItem(baseItems, pass, "软件版本", data.soft_version, softwareVersion, isSoftwareTest);
    appendPeriphItem(baseItems, pass, "资源版本", data.res_version, resourceVersion, isResourceTest);
    appendPeriphItem(baseItems, pass, "老化状态", QString::number(data.ageing_state), Age_State, isAgingStatusTest);
    testResultTableUpdate(baseItems);

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = "-";
    if (!pass) {
        TestResult = failValue;
        showlog(QString("基本信息校验失败：soft=%1(%2) res=%3(%4) age=%5(%6)")
                    .arg(data.soft_version, softwareVersion, data.res_version, resourceVersion,
                         QString::number(data.ageing_state), Age_State));
    } else {
        showlog("基本信息校验通过");
    }
}

void QFreeWork::refreshBattaryData(ProtocolBatteryData adc) {

    // 电量测试为异步判定：在电池回调里显式回填当前步骤。
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(adc)))
        return;

    // 无 Gate 的 GetBattery：仍把实测写入结果表「数据」列
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("GetBattery")
        && activeTestCase_.send.action == TestCaseSendAction::Get) {
        QString field = QStringLiteral("percent");
        if (activeTestCase_.send.param.canConvert<QVariantMap>()) {
            const QVariantMap map = activeTestCase_.send.param.toMap();
            if (map.contains(QStringLiteral("field")))
                field = map.value(QStringLiteral("field")).toString().trimmed();
        }
        QString testData;
        if (field == QLatin1String("voltage") || field == QLatin1String("voltageMv"))
            testData = QString::number(adc.voltageMv) + QStringLiteral(" mV");
        else if (field == QLatin1String("current") || field == QLatin1String("currentMa"))
            testData = QString::number(adc.currentMa) + QStringLiteral(" mA");
        else if (field == QLatin1String("temperature") || field == QLatin1String("temperatureC")
                 || field == QLatin1String("temp"))
            testData = QString::number(adc.temperatureC) + QStringLiteral(" °C");
        else
            testData = QString::number(adc.percent) + QStringLiteral(" %");
        markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("读取完成"));
        showlog(QStringLiteral("读取电池完成：%1").arg(testData));
        return;
    }

    if (!isCurrentStep("获取电量信息")) {
        return;
    }
    const bool pass = (adc.percent >= standbattary);
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    // MES：testData 仅 ASCII 数值；界面要求列带 %
    stepRuntime_.testData = QString::number(adc.percent);
    stepRuntime_.ask = QStringLiteral("[%1,100] %").arg(static_cast<int>(standbattary));
    if (!pass) {
        TestResult = failValue;
        showlog(QString("电量卡控失败，当前%1%，要求≥%2%").arg(adc.percent).arg(standbattary));
    }
}

void QFreeWork::refreshWifiState(int state) {
    wifistate = state ? 1 : 0;
}

void QFreeWork::refreshSn(ProtocolSnData data) {
    deviceTailSnFromDevice = data.value.trimmed();
    const QString expectedWholeMachineSn = resolvedPcbaSnText();
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "deviceTailSnFromDevice" << deviceTailSnFromDevice;
    ui->product_sn->setText("芯片存储的整机sn:" + deviceTailSnFromDevice);

    // “获取整机SN码”步骤采用异步判定：设备返回 SN 必须与 UI 输入一致才通过。
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolSnData"), QVariant::fromValue(data)))
        return;

    if (!isCurrentStep("获取整机SN码")) {
        return;
    }
    QVector<TestItem> snItems;
    snItems.reserve(1);
    bool snPass = !expectedWholeMachineSn.isEmpty();
    appendPeriphItem(snItems, snPass, "整机SN码", deviceTailSnFromDevice, expectedWholeMachineSn, true);
    stepRuntime_.done = true;
    stepRuntime_.pass = snPass;
    stepRuntime_.testData = deviceTailSnFromDevice;
    if (!snPass) {
        TestResult = failValue;
        showlog("整机SN校验失败，设备SN=" + deviceTailSnFromDevice + "，期望整机SN=" + expectedWholeMachineSn);
    } else {
        showlog("整机SN校验通过");
    }
}

void QFreeWork::refreshMacData(ProtocolMacData data) {
    const QString mac = data.mac.trimmed();
    if (mac.isEmpty())
        return;

    macAddress = mac;
    if (ui && ui->macLabel)
        ui->macLabel->setText(QStringLiteral("蓝牙mac: ") + mac);

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolMacData"), QVariant::fromValue(data)))
        return;

    // 无 Gate 的 MacRead：有回包即过步
    if (testCaseStepActive_ && !activeTestCase_.gate.enabled
        && activeTestCase_.send.channel == TestCaseSendChannel::Product
        && activeTestCase_.send.action == TestCaseSendAction::Get
        && activeTestCase_.send.deviceCmd == QStringLiteral("MacRead")) {
        markActiveTestCaseStepDone(true, mac, QStringLiteral("通过"));
        showlog(QStringLiteral("读取 MAC 完成：%1").arg(mac));
    }
}

void QFreeWork::refreshAiotImuCali(ProtocolAiotImuCaliData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotImuCaliData"), QVariant::fromValue(data)))
        return;

    const QString text = QStringLiteral("kx=%1 ky=%2 kz=%3 syx=%4 szx=%5 szy=%6 bx=%7 by=%8 bz=%9")
                             .arg(data.kx, 0, 'g', 8)
                             .arg(data.ky, 0, 'g', 8)
                             .arg(data.kz, 0, 'g', 8)
                             .arg(data.syx, 0, 'g', 8)
                             .arg(data.szx, 0, 'g', 8)
                             .arg(data.szy, 0, 'g', 8)
                             .arg(data.bx, 0, 'g', 8)
                             .arg(data.by, 0, 'g', 8)
                             .arg(data.bz, 0, 'g', 8);
    if (testCaseStepActive_
        && (activeTestCase_.send.deviceCmd == QStringLiteral("PeriphState")
            || activeTestCase_.send.deviceCmd == QStringLiteral("GetImuCaliResult")
            || activeTestCase_.send.deviceCmd == QStringLiteral("LightSensorInfo"))
        && activeTestCase_.send.action == TestCaseSendAction::Get) {
        markActiveTestCaseStepDone(true, text, QStringLiteral("通过"));
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("读取完成"));
        showlog(QStringLiteral("读取IMU校准完成：%1").arg(text));
        return;
    }
    showlog(QStringLiteral("IMU校准：%1").arg(text));
}

void QFreeWork::refreshAiotFsensorCali(ProtocolAiotFsensorCaliData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotFsensorCaliData"), QVariant::fromValue(data)))
        return;

    const QString text = data.calibrated ? QStringLiteral("1(已校准)") : QStringLiteral("0(未校准)");
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("PeriphState")
        && activeTestCase_.send.action == TestCaseSendAction::Get) {
        markActiveTestCaseStepDone(true, text, QStringLiteral("通过"));
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("读取完成"));
        showlog(QStringLiteral("读取电容/力传感校准完成：%1").arg(text));
        return;
    }
    showlog(QStringLiteral("电容/力传感校准：%1").arg(text));
}

void QFreeWork::refreshAiotExceptionThreshold(ProtocolAiotExceptionThresholdData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotExceptionThresholdData"), QVariant::fromValue(data)))
        return;

    QStringList parts;
    for (const ProtocolAiotExceptionThresholdItem& item : data.items) {
        if (item.type == 0x05)
            parts << QStringLiteral("0x%1=%2~%3")
                         .arg(item.type, 2, 16, QChar('0'))
                         .arg(item.value)
                         .arg(item.valueHigh);
        else
            parts << QStringLiteral("0x%1=%2").arg(item.type, 2, 16, QChar('0')).arg(item.value);
    }
    const QString text = parts.isEmpty() ? QStringLiteral("-") : parts.join(QStringLiteral("; "));
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("ExceptionThresholdRead")
        && activeTestCase_.send.action == TestCaseSendAction::Get) {
        markActiveTestCaseStepDone(true, text, QStringLiteral("通过"));
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("读取完成"));
        showlog(QStringLiteral("读取异常阈值完成：%1").arg(text));
        return;
    }
    showlog(QStringLiteral("异常阈值：%1").arg(text));
}

void QFreeWork::refreshAiotPumpParam(ProtocolAiotPumpParamData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotPumpParamData"), QVariant::fromValue(data)))
        return;

    const QString cmd = activeTestCase_.send.deviceCmd;
    const bool valveStep = (cmd == QStringLiteral("ValveParamRead"));
    const QString text =
        valveStep
            ? QStringLiteral("阀使能=%1 关闭=%2 PWM=%3")
                  .arg(data.valveEnableTime)
                  .arg(data.valveDisableTime)
                  .arg(data.valvePwm)
            : QStringLiteral("循环=%1 泵时长=%2 间隔=%3 PWM=%4")
                  .arg(data.circleNum)
                  .arg(data.durationTime)
                  .arg(data.intervalTime)
                  .arg(data.pumpPwm);
    if (testCaseStepActive_
        && (cmd == QStringLiteral("PumpParamRead") || cmd == QStringLiteral("ValveParamRead"))
        && activeTestCase_.send.action == TestCaseSendAction::Get) {
        markActiveTestCaseStepDone(true, text, QStringLiteral("通过"));
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("读取完成"));
        showlog(QStringLiteral("读取%1参数完成：%2")
                    .arg(valveStep ? QStringLiteral("阀") : QStringLiteral("泵"), text));
        return;
    }
    showlog(QStringLiteral("泵/阀参数：循环=%1 泵=%2/%3/%4 阀=%5/%6/%7")
                .arg(data.circleNum)
                .arg(data.durationTime)
                .arg(data.intervalTime)
                .arg(data.pumpPwm)
                .arg(data.valveEnableTime)
                .arg(data.valveDisableTime)
                .arg(data.valvePwm));
}

void QFreeWork::refreshAiotHeatTest(ProtocolAiotHeatTestData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotHeatTestData"), QVariant::fromValue(data)))
        return;

    const QString text =
        data.hasDuration
            ? QStringLiteral("enable=%1 strength=%2 duration=%3")
                  .arg(data.enable)
                  .arg(data.driveStrength)
                  .arg(data.durationTime)
            : QStringLiteral("enable=%1 strength=%2").arg(data.enable).arg(data.driveStrength);
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("HeatTestWrite")
        && activeTestCase_.send.action == TestCaseSendAction::Set) {
        // 有 Gate 时已由 evaluate 收尾；无 Gate 时 Set 默认已 mark，此处补数据列
        if (stepRuntime_.testData == QLatin1String("-") || stepRuntime_.testData.trimmed().isEmpty()
            || stepRuntime_.testData == QLatin1String("ok"))
            stepRuntime_.testData = text;
        showlog(QStringLiteral("加热测试回包：%1").arg(text));
        return;
    }
    showlog(QStringLiteral("加热测试：%1").arg(text));
}

void QFreeWork::refreshAiotVibrationTest(ProtocolAiotVibrationTestData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotVibrationTestData"), QVariant::fromValue(data)))
        return;

    const QString text = QStringLiteral("enable=%1 strength=%2 freq=%3 duration=%4")
                             .arg(data.enable)
                             .arg(data.driveStrength)
                             .arg(data.freq)
                             .arg(data.durationTime);
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("VibrationTestWrite")
        && activeTestCase_.send.action == TestCaseSendAction::Set) {
        if (stepRuntime_.testData == QLatin1String("-") || stepRuntime_.testData.trimmed().isEmpty()
            || stepRuntime_.testData == QLatin1String("ok"))
            stepRuntime_.testData = text;
        showlog(QStringLiteral("振动测试回包：%1").arg(text));
        return;
    }
    showlog(QStringLiteral("振动测试：%1").arg(text));
}

void QFreeWork::refreshAiotCycleReportConfig(ProtocolAiotCycleReportConfigData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotCycleReportConfigData"), QVariant::fromValue(data)))
        return;

    QStringList parts;
    for (const ProtocolAiotCycleReportConfigItem& it : data.items)
        parts << QStringLiteral("type=0x%1@%2ms").arg(it.dataType, 2, 16, QChar('0')).arg(it.intervalTime);
    const QString text = QStringLiteral("enable=%1 [%2]")
                             .arg(data.enable)
                             .arg(parts.isEmpty() ? QStringLiteral("-") : parts.join(QStringLiteral(", ")));
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("CycleReportWrite")
        && activeTestCase_.send.action == TestCaseSendAction::Set) {
        if (stepRuntime_.testData == QLatin1String("-") || stepRuntime_.testData.trimmed().isEmpty()
            || stepRuntime_.testData == QLatin1String("ok"))
            stepRuntime_.testData = text;
        showlog(QStringLiteral("循环上报配置回包：%1").arg(text));
        return;
    }
    showlog(QStringLiteral("循环上报配置：%1").arg(text));
}

void QFreeWork::refreshAiotCycleReport(ProtocolAiotCycleReportData data) {
    // 等待被动上报：按 Gate 比对；未匹配则继续等（勿直接失败）
    if (testCaseStepActive_ && !activeTestCase_.hook.enabled && activeTestCase_.gate.enabled
        && activeTestCase_.gate.reportType == QStringLiteral("ProtocolAiotCycleReportData")) {
        if (data.items.isEmpty())
            return;
        QVector<TestCaseGate> gates = TestCaseStore::activeGatesForEvaluation(activeTestCase_);
        if (gates.isEmpty())
            return;
        bool pass = false;
        QString detail;
        if (gates.size() > 1)
            GateRegistry::evaluateAll(gates, QStringLiteral("ProtocolAiotCycleReportData"), QVariant::fromValue(data),
                                     pass, detail);
        else
            GateRegistry::evaluate(gates.first(), QStringLiteral("ProtocolAiotCycleReportData"),
                                   QVariant::fromValue(data), pass, detail);
        if (!pass) {
            showlog(QStringLiteral("循环上报未匹配期望，继续等待：%1").arg(detail));
            return;
        }
        const GateStepDisplay display =
            GateRegistry::formatStepDisplay(gates.first(), gates, QStringLiteral("ProtocolAiotCycleReportData"),
                                            QVariant::fromValue(data), gates.size() > 1);
        markActiveTestCaseStepDone(true, display.testData.isEmpty() ? detail : display.testData, display.ask);
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("循环上报卡控通过"));
        showlog(QStringLiteral("循环上报卡控通过：%1").arg(detail));
        return;
    }

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolAiotCycleReportData"), QVariant::fromValue(data)))
        return;

    QStringList parts;
    for (const ProtocolAiotCycleReportItem& it : data.items) {
        parts << QStringLiteral("type=0x%1 rawLen=%2").arg(it.dataType, 2, 16, QChar('0')).arg(it.raw.size());
    }
    showlog(QStringLiteral("循环上报数据：%1")
                .arg(parts.isEmpty() ? QStringLiteral("(空)") : parts.join(QStringLiteral("; "))));
}

void QFreeWork::refreshPeriphData(ProtocolPeriphStateData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolPeriphStateData"), QVariant::fromValue(data)))
        return;

    // “获取外围设备状态”步骤采用异步判定：按设置项勾选和期望值判定通过。
    if (!isCurrentStep("获取外围设备状态")) {
        return;
    }

    const QString press0Status = SETTINGS.value("PeripheralStatus/Press0_Status").toString();
    const QString press1Status = SETTINGS.value("PeripheralStatus/Press1_Status").toString();
    const QString batteryIcStatus = SETTINGS.value("PeripheralStatus/BatteryIc_Status").toString();
    const QString touchIcStatus = SETTINGS.value("PeripheralStatus/TouchIc_Status").toString();
    const QString ledIcStatus = SETTINGS.value("PeripheralStatus/LedIc_Status").toString();
    const QString pdIcStatus = SETTINGS.value("PeripheralStatus/PdIc_Status").toString();

    // freework 外设分项使用独立勾选开关，避免复用旧的外围配置导致误判。
    const bool checkPress0 = SETTINGS.value("FreeWorkPeripheral/Press0_checkBox").toBool();
    const bool checkPress1 = SETTINGS.value("FreeWorkPeripheral/Press1_checkBox").toBool();
    const bool checkBatteryIc = SETTINGS.value("FreeWorkPeripheral/BatteryIC_checkBox").toBool();
    const bool checkTouchIc = SETTINGS.value("FreeWorkPeripheral/TouchIC_checkBox").toBool();
    const bool checkLedIc = SETTINGS.value("FreeWorkPeripheral/LedIC_checkBox").toBool();
    const bool checkPdIc = SETTINGS.value("FreeWorkPeripheral/PdIC_checkBox").toBool();

    const QString press0StateStr = QString::number(data.press0_state);
    const QString press1StateStr = QString::number(data.press1_state);
    const QString batteryStateStr = QString::number(data.battery_ic_state);
    const QString touchStateStr = QString::number(data.touch_ic_state);
    const QString ledStateStr = QString::number(data.led_ic_state);
    const QString pdStateStr = QString::number(data.pd_ic_state);

    QVector<TestItem> periphTestItems;
    periphTestItems.reserve(6);
    bool pass = true;
    appendPeriphItem(periphTestItems, pass, "压感0状态", press0StateStr, press0Status, checkPress0);
    appendPeriphItem(periphTestItems, pass, "压感1状态", press1StateStr, press1Status, checkPress1);
    appendPeriphItem(periphTestItems, pass, "电池IC状态", batteryStateStr, batteryIcStatus, checkBatteryIc);
    appendPeriphItem(periphTestItems, pass, "触摸IC状态", touchStateStr, touchIcStatus, checkTouchIc);
    appendPeriphItem(periphTestItems, pass, "LED IC状态", ledStateStr, ledIcStatus, checkLedIc);
    appendPeriphItem(periphTestItems, pass, "PD IC状态", pdStateStr, pdIcStatus, checkPdIc);
    testResultTableUpdate(periphTestItems);

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = "-";
    if (!pass) {
        TestResult = failValue;
        showlog(QString("外围状态校验失败：press0=%1 press1=%2 battery=%3 touch=%4 led=%5 pd=%6")
                    .arg(press0StateStr, press1StateStr, batteryStateStr, touchStateStr, ledStateStr, pdStateStr));
    } else {
        showlog("外围状态校验通过");
    }
}

void QFreeWork::refreshBleRssi(QString data) {
    // qDebug() << data;
    ui->BLE_RSSI->setText("BLE的RSSI:" + data);
    // showlog("zzzzz"+data);
    BLE_RSSI = data;
    bool ok;
    BLE_RSSI.toInt(&ok);

    if (!ok) {
        qDebug() << "转换蓝牙rssi失败,内容为" + BLE_RSSI + "内容结束";
    } else {
        // showlog("转换成功");
        intblerssi = BLE_RSSI.toInt(&ok);
    }
}

void QFreeWork::refreshRssiRead(ProtocolRssiData data) {
    const int rssi = data.dbm;
    const QString value = QString::number(rssi);

    if (testCaseStepActive_) {
        const QString caseName = activeTestCase_.meta.name.trimmed();
        const QString mesTag = activeTestCase_.meta.mesTag.trimmed();
        const bool isBtCase = caseName == QStringLiteral("获取BT RSSI") || mesTag == QStringLiteral("BT_RSSI");
        const bool isBleCase = caseName == QStringLiteral("获取BLE RSSI") || mesTag == QStringLiteral("BLE_RSSI");
        if (isBtCase) {
            ui->WIFI_RSSI->setText(QStringLiteral("BT的RSSI：") + value);
            BT_RSSI = value;
            intblerssi = rssi;
        } else if (isBleCase) {
            ui->BLE_RSSI->setText(QStringLiteral("BLE的RSSI:") + value);
            BLE_RSSI = value;
            intblerssi = rssi;
        }
        if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolRssiData"), QVariant::fromValue(data)))
            return;
        return;
    }

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolRssiData"), QVariant::fromValue(data)))
        return;

    const bool isBtStep = isCurrentStep(QStringLiteral("获取BT RSSI"));
    const bool isBleStep = isCurrentStep(QStringLiteral("获取BLE RSSI"));
    if (!isBtStep && !isBleStep)
        return;

    const QString itemName = isBtStep ? QStringLiteral("BT RSSI") : QStringLiteral("BLE RSSI");
    const QString ask = QStringLiteral("[%1,%2] dBm").arg(BleLowRssi).arg(BleHighRssi);
    const bool pass = (rssi > BleLowRssi && rssi < BleHighRssi);

    if (isBtStep) {
        ui->WIFI_RSSI->setText(QStringLiteral("BT的RSSI：") + value);
        BT_RSSI = value;
    } else {
        ui->BLE_RSSI->setText(QStringLiteral("BLE的RSSI:") + value);
        BLE_RSSI = value;
    }

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = value + QStringLiteral(" dBm");
    stepRuntime_.ask = ask;
    if (!pass) {
        TestResult = failValue;
        showlog(QStringLiteral("%1卡控失败，当前=%2，范围=%3").arg(itemName, value, ask));
    } else {
        showlog(QStringLiteral("%1卡控通过，当前=%2").arg(itemName, value));
    }
}

void QFreeWork::refreshKeySignalRead(ProtocolKeyCapData data) {
    // 治具下压期间同步轮询：由 pollKeyCapDuringPress 等待本槽结束
    if (plcKeyCapSyncReadPending_) {
        plcKeyCapSyncReadPending_ = false;
        plcKeyCapSyncReadOk_ = true;
        plcKeyCapSyncReadValue_ = data.capacitance;
        plcKeyCapSyncReadAuxId_ = data.keyId;
        return;
    }
}

void QFreeWork::refreshChargeCurrentRead(ProtocolChargeCurrentData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolChargeCurrentData"), QVariant::fromValue(data)))
        return;

    if (!isCurrentStep("读取充电电流")) {
        return;
    }

    const double currentMa = static_cast<double>(data.currentMa);
    const QString value = QString::number(currentMa, 'f', 0) + QStringLiteral(" mA");
    const QString ask =
        QStringLiteral("[%1,%2] mA").arg(QString::number(LowCurrent), QString::number(HighCurrent));
    const bool pass = (currentMa >= LowCurrent && currentMa <= HighCurrent);

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = value;
    stepRuntime_.ask = ask;
    if (!pass) {
        TestResult = failValue;
        showlog(QString("充电电流卡控失败，当前=%1，范围=%2").arg(value, ask));
    } else {
        showlog(QString("充电电流卡控通过，当前=%1").arg(value));
    }
}

bool QFreeWork::failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason) {
    if (!tupleData_.success) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("云端三元组未获取成功");
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：云端三元组未获取成功"));
        return true;
    }
    if (!fieldOk) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = emptyReason;
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：") + emptyReason);
        return true;
    }
    return false;
}

void QFreeWork::reportBydSfcKey(const QString& dataName, const QVariant& dataValue, int qty) {
    if (!ui->isusemes->isChecked()) {
        showlog(QStringLiteral("请先勾选「MES」后再上报关键数据"));
        return;
    }
    QString valueText;
    if (dataValue.canConvert<double>() && dataValue.type() != QVariant::String) {
        valueText = QString::number(dataValue.toDouble(), 'f', 2);
    } else {
        valueText = dataValue.toString().trimmed();
    }

    MesPacketData p = pack;
    p.factory = QStringLiteral("byd");
    p.mechines = getIndex();
    // AddSfcKey 的 SFC 与 Complete 一致：开局过程码
    p.sn = mesProcessCode_.trimmed();
    if (p.sn.isEmpty())
        p.sn = pack.sn.trimmed();
    if (p.sn.isEmpty()) {
        p.sn = resolvedPcbaSnText();
    }
    p.instruct_num = dataName.trimmed();
    p.itemvalue = valueText;
    p.testCount = qty;
    p.iskeydata = 1;

    if (p.sn.isEmpty() || p.itemvalue.isEmpty()) {
        showlog(QStringLiteral("关键数据上报失败：SFC 或 DATA_VALUE 为空（%1）").arg(p.instruct_num));
        return;
    }
    showlog(QStringLiteral("MES：AddSfcKey 上报 %1=%2").arg(p.instruct_num, p.itemvalue));
    emit send_mes_test_value(p);
}

void QFreeWork::fetchMesRootSku() {
    pack.sku.clear();
    MesPacketData p = pack;
    p.mechines = getIndex();
    p.iskeydata = 2;
    p.instruct_num = QStringLiteral("ROOTSKU");
    pack.iskeydata = 2;
    showlog(QStringLiteral("MES：GetCustomData 获取 ROOTSKU（与「是否过站」开关无关）"));
    emit send_mes_test_value(p);
    pack.iskeydata = 0;
    if (!isTestContinue)
        return;
    if (pack.sku.trimmed().isEmpty()) {
        markActiveTestCaseStepDone(false, QStringLiteral("ROOTSKU为空"), QStringLiteral("失败"));
        showlog(QStringLiteral("获取 MES SKU 失败：未从 GetCustomData 解析到 ROOTSKU"));
        return;
    }
    showlog(QStringLiteral("获取 MES SKU 成功：ROOTSKU=%1").arg(pack.sku));
    markActiveTestCaseStepDone(true, pack.sku, QStringLiteral("通过"));
}

void QFreeWork::refreshBydMesResourceDisplay() {
    if (!ui || !ui->label_bydMesResource || !ui->label_bydMesResourceCaption) {
        return;
    }
    const bool isByd = pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0;
    ui->label_bydMesResourceCaption->setVisible(isByd);
    ui->label_bydMesResource->setVisible(isByd);
    if (!isByd) {
        return;
    }
    const QString resource = bydmes::externalSettingsValue(QStringLiteral("Resource"));
    if (resource.isEmpty()) {
        ui->label_bydMesResource->setText(QStringLiteral("未配置"));

        return;
    }
    ui->label_bydMesResource->setText(resource);

}

void QFreeWork::reportBydBluetoothMesKeyMaterials() {
    if (!ui->isusemes->isChecked()) {
        return;
    }
    if (pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) != 0) {
        return;
    }
    if (!tupleData_.success) {
        return;
    }

    QString macVal = tupleData_.mac.trimmed();
    if (macVal.isEmpty()) {
        macVal = macAddress;
        macVal.remove(QLatin1Char(':'));
        macVal.remove(QLatin1Char('-'));
        macVal = macVal.trimmed().toUpper();
    }

    const QString snVal = resolvedWholeMachineSnText().isEmpty() ? resolvedPcbaSnText() : resolvedWholeMachineSnText();
    reportBydSfcKey(QStringLiteral("SN"), snVal, 1);
    reportBydSfcKey(QStringLiteral("deviceName"), tupleData_.deviceName, 1);
    reportBydSfcKey(QStringLiteral("deviceSecret"), tupleData_.deviceSecret, 1);
    reportBydSfcKey(QStringLiteral("mac"), macVal, 1);
    reportBydSfcKey(QStringLiteral("productKey"), tupleData_.productKey, -1);
}

void QFreeWork::applyTupleByMac() {
    tupleData_ = TupleApplyResult{};
    QString sku;
    QString position;
    QString macFromParam;

    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("ApplyTupleByMac")) {
        const QVariant resolved = resolveTestCaseSendParamTree(activeTestCase_.send.param);
        if (resolved.canConvert<QVariantMap>()) {
            const QVariantMap m = resolved.toMap();
            sku = m.value(QStringLiteral("sku")).toString().trimmed();
            position = m.value(QStringLiteral("position")).toString().trimmed();
            macFromParam = m.value(QStringLiteral("mac")).toString().trimmed();
            if (macFromParam.isEmpty())
                macFromParam = m.value(QStringLiteral("string")).toString().trimmed();
        } else if (resolved.userType() == QMetaType::QString) {
            macFromParam = resolved.toString().trimmed();
        }
    }
    // pack.sku 有值（前置「获取 MES SKU」）则优先；否则步骤 Param_sku / 设置页 Tuple/Sku
    if (!pack.sku.trimmed().isEmpty())
        sku = pack.sku.trimmed();
    else if (sku.isEmpty())
        sku = SETTINGS.value(QStringLiteral("Tuple/Sku"), QString()).toString().trimmed();
    // 步骤未写 position 时跟主界面 / SETTINGS（由「三元组位置」点击写入）
    if (position.isEmpty())
        position = SETTINGS.value(QStringLiteral("Tuple/Position")).toString().trimmed();
    if (position.isEmpty())
        position = QStringLiteral("1");

    // 上传只允许 1/2/3/F；L/R/S/中文等先归一，非法值直接失败
    const TuplePositionKind posKind = parseTuplePositionKind(position);
    if (posKind == TuplePositionKind::Unknown) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("position非法:");
        stepRuntime_.testData += position;
        TestResult = failValue;
        showlog(QStringLiteral("三元组获取失败：position 仅允许 1/2/3/F（当前=%1）").arg(position));
        return;
    }
    position = tuplePositionCode(posKind);

    updateTuplePositionHighlight(position);
    showlog(QStringLiteral("三元组获取：sku=%1（%2） 位置=%3（%4）")
                .arg(sku,
                     pack.sku.trimmed().isEmpty() ? QStringLiteral("步骤Param_sku")
                                                  : QStringLiteral("MES ROOTSKU"),
                     position, tuplePositionKindText(posKind)));

    QString tupleMac = macFromParam;
    if (tupleMac.isEmpty())
        tupleMac = ui->macInput->text();
    tupleMac.remove(QLatin1Char(':'));
    tupleMac.remove(QLatin1Char('-'));
    tupleMac.remove(QLatin1Char(' '));
    tupleMac = tupleMac.trimmed().toUpper();

    stepRuntime_.done = true;
    stepRuntime_.ask = "获取成功";
    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("ApplyTupleByMac") && sku.isEmpty()) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("SKU未配置");
        TestResult = failValue;
        showlog(QStringLiteral("三元组获取失败：请先执行「获取 MES SKU」或在步骤 Param_sku 填写 SKU"));
        return;
    }
    if (tupleMac.isEmpty() || tupleMac == QStringLiteral("没有MAC地址")) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = "MAC为空";
        TestResult = failValue;
        showlog("三元组获取失败：MAC为空");
        return;
    }
    if (!QTupleService::hasSharedSession()) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("未登录");
        TestResult = failValue;
        showlog(QStringLiteral("三元组获取失败：请先执行「三元组云端登录」步骤"));
        return;
    }

    QTupleService service;
    QVariantMap applyMap;
    applyMap[QStringLiteral("mac")] = tupleMac;
    applyMap[QStringLiteral("sku")] = sku;
    applyMap[QStringLiteral("position")] = position;
    service.get(TupleCmd::ApplyTupleByMac, applyMap);
    tupleData_ = service.lastApplyResult();
    stepRuntime_.done = true;
    stepRuntime_.ask = QStringLiteral("获取成功");
    if (!tupleData_.success) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = tupleData_.error;
        TestResult = failValue;
        showlog(QStringLiteral("三元组获取失败：") + tupleData_.error);
        return;
    }
    const QString wholeSn = tupleData_.sn.trimmed();
    if (wholeSn.isEmpty()) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("整机SN为空");
        TestResult = failValue;
        showlog(QStringLiteral("三元组获取失败：云端返回整机SN为空"));
        return;
    }
    QString skuCheckDetail;
    if (!wholeMachineSnEmbeddedSkuMatches(wholeSn, sku, &skuCheckDetail)) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = skuCheckDetail;
        TestResult = failValue;
        showlog(QStringLiteral("三元组获取失败：整机SN SKU位校验未通过，%1（SN=%2，上传SKU=%3）")
                    .arg(skuCheckDetail, wholeSn, sku));
        return;
    }
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("SN:%1 productKey:%2 deviceName:%3 deviceSecret:%4")
                                .arg(wholeSn, tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);
    showlog(QStringLiteral("三元组获取成功：sn=%1 productKey=%2 deviceName=%3 deviceSecret=%4 mac=%5")
                .arg(wholeSn, tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret, tupleData_.mac));
    showlog(QStringLiteral("整机SN SKU位校验通过：第9-11位=%1").arg(wholeSn.mid(8, 3)));
    setWholeMachineSn(wholeSn);
    showlog(QStringLiteral("已替换界面SN：%1").arg(resolvedPcbaSnText()));
    // 蓝牙测试关键物料：与 MES「蓝牙测试」工站 SFC 生命周期表一致，各发一条 AddSfcKey（QTY=1）
    reportBydBluetoothMesKeyMaterials();
}

void QFreeWork::debugUpdateTupleMacStatus(const TestCaseDefinition& def) {
    stepRuntime_.done = true;
    const QVariantMap resolved = resolveTestCaseSendParamTree(def.send.param).toMap();
    QString tupleMac = resolved.value(QStringLiteral("mac")).toString().trimmed();
    tupleMac.remove(QLatin1Char(':'));
    tupleMac.remove(QLatin1Char('-'));
    tupleMac.remove(QLatin1Char(' '));
    tupleMac = tupleMac.trimmed().toUpper();

    if (tupleMac.isEmpty() || tupleMac == QStringLiteral("没有MAC地址") || tupleMac == QStringLiteral("没有mac地址")) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("MAC为空");
        TestResult = failValue;
        showlog(QStringLiteral("上报烧录状态失败：MAC为空"));
        return;
    }
    if (!QTupleService::hasSharedSession()) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog(QStringLiteral("上报烧录状态失败：请先执行「三元组云端登录」"));
        return;
    }

    const int status = resolved.value(QStringLiteral("status"), 2).toInt();
    const QString sn = resolved.value(QStringLiteral("sn")).toString().trimmed();

    QTupleService service;
    QVariantMap statusMap;
    statusMap[QStringLiteral("mac")] = tupleMac;
    statusMap[QStringLiteral("status")] = status;
    if (!sn.isEmpty())
        statusMap[QStringLiteral("sn")] = sn;
    service.set(TupleCmd::DebugUpdateMacStatus, statusMap);
    if (!service.lastError().isEmpty()) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog(QStringLiteral("上报烧录状态失败：") + service.lastError());
        return;
    }
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("%1 / status=%2").arg(tupleMac).arg(status);
    showlog(QStringLiteral("上报烧录状态成功：mac=%1 status=%2").arg(tupleMac).arg(status));
}

void QFreeWork::reportTupleWriteRecord() {
    stepRuntime_.done = true;
    const QString productSn = resolvedWholeMachineSnText().isEmpty() ? resolvedPcbaSnText() : resolvedWholeMachineSnText();
    stepRuntime_.testData = productSn;

    if (!QTupleService::hasSharedSession()) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog(QStringLiteral("检验数据上报失败：请先执行「三元组云端登录」"));
        return;
    }

    // M8 烧录工站等无「拉三元组」步骤：按 SN + 已写 MAC 上报 /api/inspection/report
    if (!tupleData_.success) {
        QString mac = ui->macInput->text().trimmed();
        mac.remove(QLatin1Char(':'));
        mac.remove(QLatin1Char('-'));
        mac.remove(QLatin1Char(' '));
        mac = mac.trimmed().toUpper();
        if (productSn.isEmpty() || mac.isEmpty() || mac == QStringLiteral("没有mac地址")) {
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("SN或MAC为空");
            TestResult = failValue;
            showlog(QStringLiteral("检验数据上报失败：SN 或 MAC 为空"));
            return;
        }
        QTupleService service;
        QVariantMap reportMap;
        reportMap[QStringLiteral("productSn")] = productSn;
        reportMap[QStringLiteral("burnMac")] = mac;
        reportMap[QStringLiteral("pass")] = TestResult != failValue;
        service.set(TupleCmd::ReportWriteRecord, reportMap);
        if (!service.lastError().isEmpty()) {
            stepRuntime_.pass = false;
            TestResult = failValue;
            showlog(QStringLiteral("检验数据上报失败：") + service.lastError());
            return;
        }
        stepRuntime_.pass = true;
        stepRuntime_.testData = QStringLiteral("%1 / %2").arg(productSn, mac);
        showlog(QStringLiteral("检验数据上报成功（写入MAC）"));
        return;
    }

    QTupleService service;
    const bool btRssiPass = BT_RSSI.toInt() > BleLowRssi && BT_RSSI.toInt() < BleHighRssi;
    const bool bleRssiPass = BLE_RSSI.toInt() > BleLowRssi && BLE_RSSI.toInt() < BleHighRssi;
    QVariantMap reportMap;
    reportMap[QStringLiteral("productKey")] = tupleData_.productKey;
    reportMap[QStringLiteral("deviceName")] = tupleData_.deviceName;
    reportMap[QStringLiteral("deviceSecret")] = tupleData_.deviceSecret;
    reportMap[QStringLiteral("sn")] = tupleData_.sn;
    reportMap[QStringLiteral("productSn")] = productSn;
    reportMap[QStringLiteral("result")] = TestResult == failValue ? QStringLiteral("NG") : QStringLiteral("OK");
    reportMap[QStringLiteral("btRssi")] = BT_RSSI;
    reportMap[QStringLiteral("btRssiPass")] = btRssiPass;
    reportMap[QStringLiteral("bleRssi")] = BLE_RSSI;
    reportMap[QStringLiteral("bleRssiPass")] = bleRssiPass;
    reportMap[QStringLiteral("softwareVersion")] = softwareVersionForReport_;
    reportMap[QStringLiteral("softwareVersionPass")] = softwareVersionPassForReport_;
    service.set(TupleCmd::ReportWriteRecord, reportMap);
    if (!service.lastError().isEmpty()) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog("三元组写入记录上报失败：" + service.lastError());
        return;
    }
    stepRuntime_.pass = true;
    showlog("三元组写入记录上报成功");
}

void QFreeWork::executeCloudTupleCase(const TestCaseDefinition& def) {
    TupleCmd cmd;
    if (!TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, cmd)) {
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    switch (cmd) {
    case TupleCmd::ApplyTupleByMac:
        applyTupleByMac();
        break;
    case TupleCmd::ReportWriteRecord:
        reportTupleWriteRecord();
        break;
    case TupleCmd::DebugUpdateMacStatus:
        debugUpdateTupleMacStatus(def);
        break;
    case TupleCmd::Login: {
        stepRuntime_.done = true;
        QTupleService service;
        QString loginError;
        const QVariantMap m = def.send.param.toMap();
        QString userName = m.value(QStringLiteral("userName")).toString().trimmed();
        QString password = m.value(QStringLiteral("password")).toString().trimmed();
        if (userName.isEmpty() || password.isEmpty()) {
            stepRuntime_.pass = false;
            loginError = QStringLiteral("请在用例 ini 配置 Param/userName、Param/password（及 Param/baseUrl）");
        } else {
            QVariantMap loginMap;
            loginMap[QStringLiteral("userName")] = userName;
            loginMap[QStringLiteral("password")] = password;
            const QString baseUrl = m.value(QStringLiteral("baseUrl")).toString().trimmed();
            if (!baseUrl.isEmpty())
                loginMap[QStringLiteral("baseUrl")] = baseUrl;
            service.set(TupleCmd::Login, loginMap);
            stepRuntime_.pass = service.lastError().isEmpty();
            if (!stepRuntime_.pass)
                loginError = service.lastError();
        }
        stepRuntime_.testData = stepRuntime_.pass ? QStringLiteral("登录成功") : loginError;
        if (!stepRuntime_.pass) {
            TestResult = failValue;
            showlog(QStringLiteral("三元组云端登录失败：") + loginError);
        } else {
            showlog(QStringLiteral("三元组云端登录成功"));
        }
        break;
    }
    }
}

bool QFreeWork::tryCompleteActiveTestCaseTupleCompare(const ProtocolTupleData& data) {
    if (!testCaseStepActive_)
        return false;
    if (activeTestCase_.send.channel != TestCaseSendChannel::Product || activeTestCase_.send.action != TestCaseSendAction::Get || activeTestCase_.send.deviceCmd != QStringLiteral("TupleRead"))
        return false;
    // 启用 Gate 时走单字段卡控，不在这里整包比对
    if (activeTestCase_.gate.enabled)
        return false;

    // Param_dataType / Param_type，或步骤名含 productID/deviceId/deviceSecret → 只比对应字段
    int onlyType = 0;
    if (activeTestCase_.send.param.canConvert<QVariantMap>()) {
        const QVariantMap map = activeTestCase_.send.param.toMap();
        if (map.contains(QStringLiteral("dataType")))
            onlyType = map.value(QStringLiteral("dataType")).toInt();
        else if (map.contains(QStringLiteral("type")))
            onlyType = map.value(QStringLiteral("type")).toInt();
    }
    if (onlyType == 0) {
        const QString label = activeTestCase_.meta.displayName.trimmed().isEmpty()
                                  ? (activeTestCase_.meta.name.trimmed().isEmpty() ? activeTestCase_.meta.mesTag
                                                                                  : activeTestCase_.meta.name)
                                  : activeTestCase_.meta.displayName;
        // 兼容旧步骤名 productKey/deviceName
        if (label.contains(QStringLiteral("productID"), Qt::CaseInsensitive)
            || label.contains(QStringLiteral("productKey"), Qt::CaseInsensitive))
            onlyType = 2;
        else if (label.contains(QStringLiteral("deviceId"), Qt::CaseInsensitive)
                 || label.contains(QStringLiteral("deviceName"), Qt::CaseInsensitive))
            onlyType = 3;
        else if (label.contains(QStringLiteral("deviceSecret"), Qt::CaseInsensitive))
            onlyType = 4;
    }

    if (!tupleData_.success) {
        markActiveTestCaseStepDone(false, QStringLiteral("云端三元组未获取成功"), QStringLiteral("-"));
        TestResult = failValue;
        showlog(QStringLiteral("设备三元组比较失败：云端三元组未获取成功"));
        return true;
    }

    bool pass = false;
    QString testData;
    QString ask;
    if (onlyType == 2) {
        testData = data.productId.trimmed();
        ask = tupleData_.productKey.trimmed();
        pass = testData == ask;
        markActiveTestCaseStepDone(pass, testData, ask);
        if (!pass) {
            TestResult = failValue;
            showlog(QStringLiteral("productID比较失败，设备=%1，云端=%2").arg(testData, ask));
        } else {
            showlog(QStringLiteral("productID比较通过：%1").arg(testData));
        }
        return true;
    }
    if (onlyType == 3) {
        testData = data.deviceId.trimmed();
        ask = tupleData_.deviceName.trimmed();
        pass = testData == ask;
        markActiveTestCaseStepDone(pass, testData, ask);
        if (!pass) {
            TestResult = failValue;
            showlog(QStringLiteral("deviceId比较失败，设备=%1，云端=%2").arg(testData, ask));
        } else {
            showlog(QStringLiteral("deviceId比较通过：%1").arg(testData));
        }
        return true;
    }
    if (onlyType == 4) {
        testData = data.key.trimmed();
        ask = tupleData_.deviceSecret.trimmed();
        pass = CommonUtils::matchTupleDeviceSecret(data.key, data.keyCipherHex, tupleData_.deviceSecret);
        markActiveTestCaseStepDone(pass, testData, ask);
        if (!pass) {
            TestResult = failValue;
            showlog(QStringLiteral("deviceSecret比较失败，设备=%1，云端=%2").arg(testData, ask));
        } else {
            showlog(QStringLiteral("deviceSecret比较通过"));
        }
        return true;
    }

    const QString allTestData =
        QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3").arg(data.productId, data.deviceId, data.key);
    const QString allAsk = QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3")
                               .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);
    const bool productKeyPass = data.productId.trimmed() == tupleData_.productKey.trimmed();
    const bool deviceNamePass = data.deviceId.trimmed() == tupleData_.deviceName.trimmed();
    const bool deviceSecretPass =
        CommonUtils::matchTupleDeviceSecret(data.key, data.keyCipherHex, tupleData_.deviceSecret);
    pass = productKeyPass && deviceNamePass && deviceSecretPass;

    markActiveTestCaseStepDone(pass, allTestData, allAsk);
    if (!pass) {
        TestResult = failValue;
        showlog(QStringLiteral("设备三元组比较失败，设备 productKey=%1 deviceName=%2 deviceSecret=%3，云端 productKey=%4 deviceName=%5 deviceSecret=%6")
                    .arg(data.productId, data.deviceId, data.key, tupleData_.productKey, tupleData_.deviceName,
                         tupleData_.deviceSecret));
    } else {
        showlog(QStringLiteral("设备三元组比较通过"));
    }
    return true;
}

void QFreeWork::refreshTupleData(ProtocolTupleData data) {
    if (tryCompleteActiveTestCaseTupleCompare(data))
        return;

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolTupleData"), QVariant::fromValue(data)))
        return;

    if (!isCurrentStep("读取设备三元组并比较")) {
        return;
    }

    stepRuntime_.done = true;
    const bool productKeyPass = data.productId.trimmed() == tupleData_.productKey.trimmed();
    const bool deviceNamePass = data.deviceId.trimmed() == tupleData_.deviceName.trimmed();
    const bool deviceSecretPass =
        CommonUtils::matchTupleDeviceSecret(data.key, data.keyCipherHex, tupleData_.deviceSecret);
    const bool pass = tupleData_.success && productKeyPass && deviceNamePass && deviceSecretPass;

    stepRuntime_.pass = pass;
    stepRuntime_.testData =
        QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3").arg(data.productId, data.deviceId, data.key);
    stepRuntime_.ask = QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3")
                           .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);
    if (!pass) {
        TestResult = failValue;
        showlog(QStringLiteral("设备三元组比较失败，设备 productKey=%1 deviceName=%2，云端 productKey=%3 deviceName=%4")
                    .arg(data.productId, data.deviceId, tupleData_.productKey, tupleData_.deviceName));
    } else {
        showlog(QStringLiteral("设备三元组比较通过"));
    }
}

void QFreeWork::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到电流数据" << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk) / 100;

    if (!conversionOk) {
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
        if (!isCurrentStep("读取治具电流测量值")) {
            return;
        }
        if (currentSampleAnyMatchActive_) {
            showlog(QStringLiteral("治具电流采样：本次解析失败，继续"));
            return;
        }
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = "电流解析失败";
        TestResult = failValue;
        showlog("电流卡控失败：无法解析电流数据");
        return;
    }

    qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
    measure_ammeter = normalValue;
    const QString formattedValue = QString::number(normalValue, 'f', 4);
    qDebug() << getIndex() << "转换后的数值：" << formattedValue << "ma";
    showlog(formattedValue + "ma");

    if (!isCurrentStep("读取治具电流测量值")) {
        return;
    }

    // 连续采样：Gate 开启时交给 ProtocolMeasureData 软判定；否则用 SETTINGS 电流上下限
    if (currentSampleAnyMatchActive_) {
        if (activeTestCase_.gate.enabled
            && activeTestCase_.gate.reportType == QLatin1String("ProtocolMeasureData")) {
            return;
        }
        ++currentSampleCount_;
        currentSampleLastValueText_ = formattedValue + QStringLiteral("ma");
        const QString ask =
            QStringLiteral("[%1,%2]ma").arg(QString::number(LowCurrent), QString::number(HighCurrent));
        const bool pass = (measure_ammeter >= LowCurrent && measure_ammeter <= HighCurrent);
        showlog(QStringLiteral("治具电流采样#%1：%2 → %3")
                    .arg(currentSampleCount_)
                    .arg(currentSampleLastValueText_)
                    .arg(pass ? QStringLiteral("卡控通过") : QStringLiteral("未达标(继续)")));
        if (pass) {
            currentSampleAnyMatchActive_ = false;
            markActiveTestCaseStepDone(true, currentSampleLastValueText_, ask);
            showlog(QStringLiteral("治具电流卡控通过（连续采样任一合格），当前=%1，范围=%2")
                        .arg(currentSampleLastValueText_, ask));
        }
        return;
    }

    const bool pass = (measure_ammeter >= LowCurrent && measure_ammeter <= HighCurrent);
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = formattedValue + "ma";
    if (!pass) {
        TestResult = failValue;
        showlog(QString("电流卡控失败，测量值=%1ma，范围=[%2,%3]ma")
                    .arg(formattedValue, QString::number(LowCurrent), QString::number(HighCurrent)));
    } else {
        showlog("电流卡控通过");
    }
}

void QFreeWork::refreshDongleSuctionData(ProtocolDongleSuctionData data) {
    data.ch1Kpa += suctionOffsetKpa_;
    data.ch2Kpa += suctionOffsetKpa_;
    data.ch3Kpa += suctionOffsetKpa_;
    dongleSuctionLastCh1Kpa_ = data.ch1Kpa;
    dongleSuctionLastCh2Kpa_ = data.ch2Kpa;
    dongleSuctionLastCh3Kpa_ = data.ch3Kpa;
    if (dongleSuctionSampleActive_) {
        dongleSuctionSampleTimeSec_.append(
            dongleSuctionSampleTimer_.isValid() ? dongleSuctionSampleTimer_.elapsed() / 1000.0 : 0.0);
        dongleSuctionCh1Samples_.append(data.ch1Kpa);
        dongleSuctionCh2Samples_.append(data.ch2Kpa);
        dongleSuctionCh3Samples_.append(data.ch3Kpa);
    }
    if (dongleSuctionReadEnabled_)
        appendSuctionChartSample(data.ch1Kpa, data.ch2Kpa);
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolDongleSuctionData"), QVariant::fromValue(data)))
        return;
}

void QFreeWork::refreshWifiMsg(QString data) {
    // qDebug() << getIndex()<< "收到wifi数据为" << data;
    QStringList parts = data.split("-");
    int numPairs = parts.size() / 2;
    for (int i = 0; i < numPairs; ++i) {
        QString macAddress = parts[i * 2];
        QString rssi = "-" + parts[i * 2 + 1];
        wifiMac = wifiMac.toUpper();
        // qDebug() << getIndex() << "dongle的的wifiMac:" << macAddress;
        // qDebug() << getIndex() << "RSSI:" << rssi;
        // qDebug() << getIndex() << " 设备的wifiMac:" << wifiMac;
        if (macAddress == wifiMac) {
            ui->WIFI_RSSI->setText("WIFI的RSSI：" + rssi);
            // qDebug() << getIndex()<< getIndex() << " 比对成功";
            refreshWifiState(1);
            WIFI_RSSI = rssi;
            bool ok;
            WIFI_RSSI.toInt(&ok);

            if (!ok) {
                qDebug() << "转换WIFIrssi失败,内容为" + WIFI_RSSI + "内容结束";
            } else {
                //  showlog("转换成功");
                intwifirssi = WIFI_RSSI.toInt(&ok);
            }
        }
    }
}
void QFreeWork::refreshRootBatteryTemp(quint8 temp) {
    ProtocolBatteryTempData data;
    data.type = temp;
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolBatteryTempData"), QVariant::fromValue(data)))
        return;

    // Gate 关闭时：仅在本步为 RootBatteryTempQuery 时回填（与电量等 Get 步一致）
    if (!testCaseStepActive_ || activeTestCase_.send.deviceCmd != QStringLiteral("RootBatteryTempQuery"))
        return;

    const QString testData = QString::number(temp) + QString::fromUtf8(" ℃");
    markActiveTestCaseStepDone(true, testData, QString::fromUtf8("[0,255] ℃"));
    showlog(QStringLiteral("电池温度：%1°C").arg(temp));
}

void QFreeWork::refreshRootHeatTemp(quint8 temp) {
    ProtocolHeatTempData data;
    data.type = temp;
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolHeatTempData"), QVariant::fromValue(data)))
        return;

    // Gate 关闭时：仅在本步为 RootHeatTempQuery 时回填
    if (!testCaseStepActive_ || activeTestCase_.send.deviceCmd != QStringLiteral("RootHeatTempQuery"))
        return;

    const QString testData = QString::number(temp) + QString::fromUtf8(" ℃");
    markActiveTestCaseStepDone(true, testData, QString::fromUtf8("[0,255] ℃"));
    showlog(QStringLiteral("加热温度：%1°C").arg(temp));
}

void QFreeWork::refreshResultCode(ProtocolResultData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolResultData"), QVariant::fromValue(data)))
        return;
    showlog(QStringLiteral("协议结果码：%1").arg(data.result));
}

void QFreeWork::refreshPhotosensitiveData(ProtocolPhotosensitiveData data) {
    if (lightSensorCollecting_) {
        if (!data.samples.isEmpty())
            lightSensorSamples_.append(data.samples);
        else
            lightSensorSamples_.append(data.lightSensor);
    }
    evaluateActiveTestCaseGate(QStringLiteral("ProtocolPhotosensitiveData"), QVariant::fromValue(data));
}

void QFreeWork::refreshLightCalibData(ProtocolLightCalibData data) {
    lightCalibReadValid_ = true;
    lightCalibReadValue_ = static_cast<int>(static_cast<int32_t>(data.calibValue));
    evaluateActiveTestCaseGate(QStringLiteral("ProtocolLightCalibData"), QVariant::fromValue(data));
}

void QFreeWork::refreshFlangeStatus(ProtocolTypeData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolFlangeData"), QVariant::fromValue(data)))
        return;

    // Gate 关闭时：仅在本步为 RootFlangeQuery 时回填
    if (!testCaseStepActive_ || activeTestCase_.send.deviceCmd != QStringLiteral("RootFlangeQuery"))
        return;

    const QString testData = QString::number(data.type);
    markActiveTestCaseStepDone(true, testData, QString());
    showlog(QStringLiteral("法兰状态：0x%1").arg(data.type, 2, 16, QChar('0')));
}

void QFreeWork::refreshPumpStallCurrent(ProtocolPumpStallCurrentData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolPumpStallCurrentData"), QVariant::fromValue(data)))
        return;

    // Gate 关闭时：仅在本步为 RootPumpStallCurrentQuery 时回填
    if (!testCaseStepActive_ || activeTestCase_.send.deviceCmd != QStringLiteral("RootPumpStallCurrentQuery"))
        return;

    const QString testData = QString::number(data.adcValue);
    markActiveTestCaseStepDone(true, testData, QString());
    showlog(QStringLiteral("泵堵电流 ADC：%1").arg(data.adcValue));
}

void QFreeWork::refreshRootAgingHistory(ProtocolRootAgingHistoryData data) {
    // Qroot 0x9C / Qaiot CID=0x01 老化模式共用；先打全量字段便于对照卡控
    QString summary;
    if (data.status >= 0 || data.finishedFlag >= 0) {
        summary = QStringLiteral("使能=%1 完成=%2 电池最高温=%3℃ 法兰最高温=%4℃ 堵转次数=%5 泵阀堵转阈值=%6 电流=[%7,%8,%9,%10,%11]")
                      .arg(data.status < 0 ? 0 : data.status)
                      .arg(data.finishedFlag < 0 ? 0 : data.finishedFlag)
                      .arg(data.batteryMaxTempC)
                      .arg(data.flangeMaxTempC)
                      .arg(data.stallCount)
                      .arg(data.stallThreshold)
                      .arg(data.stallCurrents[0])
                      .arg(data.stallCurrents[1])
                      .arg(data.stallCurrents[2])
                      .arg(data.stallCurrents[3])
                      .arg(data.stallCurrents[4]);
    } else {
        summary = QStringLiteral("次数=%1 电池最高温=%2℃ 法兰最高温=%3℃ 堵转次数=%4 泵阀堵转阈值=%5 电流=[%6,%7,%8,%9,%10]")
                      .arg(data.agingCount)
                      .arg(data.batteryMaxTempC)
                      .arg(data.flangeMaxTempC)
                      .arg(data.stallCount)
                      .arg(data.stallThreshold)
                      .arg(data.stallCurrents[0])
                      .arg(data.stallCurrents[1])
                      .arg(data.stallCurrents[2])
                      .arg(data.stallCurrents[3])
                      .arg(data.stallCurrents[4]);
    }
    showlog(QStringLiteral("老化历史：%1").arg(summary));

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolRootAgingHistoryData"), QVariant::fromValue(data)))
        return;

    const QString cmd = activeTestCase_.send.deviceCmd;
    if (!testCaseStepActive_
        || (cmd != QStringLiteral("RootAgingHistoryQuery") && cmd != QStringLiteral("AgingStatusRead")))
        return;

    markActiveTestCaseStepDone(true, summary, QString());
}

void QFreeWork::refreshTypeStatus(ProtocolTypeData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(data)))
        return;
}

void QFreeWork::refreshButton(ProtocolButtonStateData data) {
    // 步骤 Gate 等待 qroot 0x9A 按键上报：按 keyButtonId 与 Gate/Expected 比对（错键继续等，不结束步骤）
    if (testCaseStepActive_ && !activeTestCase_.hook.enabled && activeTestCase_.gate.enabled
        && activeTestCase_.gate.reportType == QStringLiteral("ProtocolButtonStateData")) {
        if (data.keyButtonId == 0)
            return;
        QVector<TestCaseGate> gates = TestCaseStore::activeGatesForEvaluation(activeTestCase_);
        if (gates.isEmpty())
            return;
        bool pass = false;
        QString detail;
        if (gates.size() > 1)
            GateRegistry::evaluateAll(gates, QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(data), pass,
                                     detail);
        else
            GateRegistry::evaluate(gates.first(), QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(data),
                                   pass, detail);
        if (!pass) {
            showlog(QStringLiteral("按键上报未匹配期望，继续等待：%1").arg(detail));
            return;
        }
        const GateStepDisplay display =
            GateRegistry::formatStepDisplay(gates.first(), gates, QStringLiteral("ProtocolButtonStateData"),
                                            QVariant::fromValue(data), gates.size() > 1);
        markActiveTestCaseStepDone(true, display.testData.isEmpty() ? detail : display.testData, display.ask);
        if (commandRetryTimer)
            finishCommandRetryWait(true, QStringLiteral("按键卡控通过"));
        showlog(QStringLiteral("按键卡控通过：%1").arg(detail));
        return;
    }

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

void QFreeWork::onUsbInstrumentReport(const ProtocolReport& report) {
    if (report.reportType == QLatin1String("ProtocolMeasureData")) {
        if (report.payload.canConvert<ProtocolMeasureData>()) {
            ProtocolMeasureData data = report.payload.value<ProtocolMeasureData>();
            if (data.type == QLatin1String("Current")) {
                // 回读为 A 时统一成 mA（设置页电流卡控多为 mA）
                if (data.unit == QLatin1String("A")
                    && (data.deviceName == QLatin1String("VISA_Power") || data.deviceName == QLatin1String("USB_Power")
                        || data.deviceName == QLatin1String("ASD9026A"))) {
                    data.value *= 1000.0;
                    data.valueText = QString::number(data.value, 'f', 4);
                    data.unit = QStringLiteral("mA");
                } else if (data.unit == QLatin1String("uA") || data.unit == QLatin1String("µA")) {
                    data.value /= 1000.0;
                    data.valueText = QString::number(data.value, 'f', 4);
                    data.unit = QStringLiteral("mA");
                }
                measure_ammeter = data.value;
                showlog(QStringLiteral("上报电流值: %1 %2")
                            .arg(data.value, 0, 'f', 4)
                            .arg(data.unit.isEmpty() ? QStringLiteral("mA") : data.unit));
            } else if (data.type == QLatin1String("Voltage")) {
                if (data.unit == QLatin1String("mV")) {
                    data.value /= 1000.0;
                    data.valueText = QString::number(data.value, 'f', 4);
                    data.unit = QStringLiteral("V");
                } else if (data.unit == QLatin1String("uV") || data.unit == QLatin1String("µV")) {
                    data.value /= 1000000.0;
                    data.valueText = QString::number(data.value, 'f', 4);
                    data.unit = QStringLiteral("V");
                }
                showlog(QStringLiteral("上报电压值: %1 %2")
                            .arg(data.value, 0, 'f', 4)
                            .arg(data.unit.isEmpty() ? QStringLiteral("V") : data.unit));
            }

            // 连续采样读电流：不合格不立刻结束，仅合格时收尾
            if (currentSampleAnyMatchActive_ && data.type == QLatin1String("Current") && testCaseStepActive_
                && activeTestCase_.gate.enabled
                && activeTestCase_.gate.reportType == QLatin1String("ProtocolMeasureData")) {
                if (!data.isOk) {
                    showlog(QStringLiteral("读电流采样：本次无效，继续"));
                    test_base::onUsbInstrumentReport(
                        ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(data)));
                    return;
                }
                ++currentSampleCount_;
                currentSampleLastValueText_ = QStringLiteral("%1 %2")
                                                  .arg(data.value, 0, 'f', 4)
                                                  .arg(data.unit.isEmpty() ? QStringLiteral("mA") : data.unit);

                QVector<TestCaseGate> gatesForEval = TestCaseStore::activeGatesForEvaluation(activeTestCase_);
                if (gatesForEval.isEmpty()) {
                    currentSampleAnyMatchActive_ = false;
                    markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
                    showlog(QStringLiteral("卡控失败：未启用任何判定项"));
                    test_base::onUsbInstrumentReport(
                        ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(data)));
                    return;
                }
                bool pass = false;
                QString detail;
                if (gatesForEval.size() > 1)
                    GateRegistry::evaluateAll(gatesForEval, QStringLiteral("ProtocolMeasureData"),
                                              QVariant::fromValue(data), pass, detail);
                else
                    GateRegistry::evaluate(gatesForEval.first(), QStringLiteral("ProtocolMeasureData"),
                                           QVariant::fromValue(data), pass, detail);
                showlog(QStringLiteral("读电流采样#%1：%2 → %3")
                            .arg(currentSampleCount_)
                            .arg(currentSampleLastValueText_)
                            .arg(pass ? QStringLiteral("卡控通过") : QStringLiteral("未达标(继续)")));
                if (pass) {
                    currentSampleAnyMatchActive_ = false;
                    GateStepDisplay display = GateRegistry::formatStepDisplay(
                        gatesForEval.first(), gatesForEval, QStringLiteral("ProtocolMeasureData"),
                        QVariant::fromValue(data), gatesForEval.size() > 1);
                    if (display.testData.isEmpty())
                        display.testData = detail;
                    markActiveTestCaseStepDone(true, display.testData, display.ask);
                    showlog(QStringLiteral("卡控通过（连续采样任一合格）：%1").arg(detail));
                }
                test_base::onUsbInstrumentReport(
                    ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(data)));
                return;
            }

            if (!data.isOk && testCaseStepActive_ && activeTestCase_.gate.enabled &&
                activeTestCase_.gate.reportType == QLatin1String("ProtocolMeasureData")) {
                markActiveTestCaseStepDone(false, QStringLiteral("读取失败"), QStringLiteral("失败"));
                if (commandRetryTimer)
                    finishCommandRetryWait(false, QStringLiteral("读取失败"));
            } else {
                evaluateActiveTestCaseGate(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(data));
            }
            test_base::onUsbInstrumentReport(
                ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(data)));
            return;
        }
    }
    test_base::onUsbInstrumentReport(report);
}
