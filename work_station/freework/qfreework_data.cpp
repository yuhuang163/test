#include "qfreework.h"

#include <algorithm>

#include "qproduct.h"
#include "test_case.h"

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

    if (!isCurrentStep(QStringLiteral("读取版本号")) && !isCurrentStep(QStringLiteral("获取基本信息"))) {
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

    if (!isCurrentStep("获取电量信息")) {
        return;
    }
    const bool pass = (adc.percent >= standbattary);
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    // MES 与 RSSI 等一致：testData 仅 ASCII 数值，避免 itemvalue 出现「FAIL;电量:0%」
    stepRuntime_.testData = QString::number(adc.percent);
    stepRuntime_.ask = QStringLiteral("[%1,100]").arg(static_cast<int>(standbattary));
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
    const QString expectedTailSnFromMesText = resolvedExpectedTailSnText();
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
    bool snPass = !expectedTailSnFromMesText.isEmpty();
    appendPeriphItem(snItems, snPass, "整机SN码", deviceTailSnFromDevice, expectedTailSnFromMesText, true);
    stepRuntime_.done = true;
    stepRuntime_.pass = snPass;
    stepRuntime_.testData = deviceTailSnFromDevice;
    if (!snPass) {
        TestResult = failValue;
        showlog("整机SN校验失败，设备SN=" + deviceTailSnFromDevice + "，输入SN=" + expectedTailSnFromMesText);
    } else {
        showlog("整机SN校验通过");
    }
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
    const QString ask = QStringLiteral("[%1,%2]").arg(BleLowRssi).arg(BleHighRssi);
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
    stepRuntime_.testData = value;
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
    const QString value = QString::number(currentMa, 'f', 0) + "ma";
    const QString ask = QString("[%1,%2]ma").arg(QString::number(LowCurrent), QString::number(HighCurrent));
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
    p.sn = pack.sn.trimmed();
    if (p.sn.isEmpty()) {
        p.sn = ui->getMac->text().trimmed();
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

    const QString snVal = tupleData_.sn.trimmed().isEmpty() ? ui->getMac->text().trimmed() : tupleData_.sn.trimmed();
    reportBydSfcKey(QStringLiteral("SN"), snVal, 1);
    reportBydSfcKey(QStringLiteral("deviceName"), tupleData_.deviceName, 1);
    reportBydSfcKey(QStringLiteral("deviceSecret"), tupleData_.deviceSecret, 1);
    reportBydSfcKey(QStringLiteral("mac"), macVal, 1);
    reportBydSfcKey(QStringLiteral("productKey"), tupleData_.productKey, -1);
}

void QFreeWork::applyTupleByMac() {
    tupleData_ = TupleApplyResult{};
    QString sku;
    QString position = QStringLiteral("L");
    QString macFromParam;

    if (testCaseStepActive_ && activeTestCase_.send.deviceCmd == QStringLiteral("ApplyTupleByMac")) {
        const QVariant resolved = resolveTestCaseSendParamTree(activeTestCase_.send.param);
        if (resolved.canConvert<QVariantMap>()) {
            const QVariantMap m = resolved.toMap();
            sku = m.value(QStringLiteral("sku")).toString().trimmed();
            position = m.value(QStringLiteral("position"), QStringLiteral("L")).toString().trimmed();
            macFromParam = m.value(QStringLiteral("mac")).toString().trimmed();
            if (macFromParam.isEmpty())
                macFromParam = m.value(QStringLiteral("string")).toString().trimmed();
        } else if (resolved.userType() == QMetaType::QString) {
            macFromParam = resolved.toString().trimmed();
        }
    }
    if (sku.isEmpty())
        sku = SETTINGS.value(QStringLiteral("Tuple/Sku"), QString()).toString().trimmed();
    if (position.isEmpty())
        position = SETTINGS.value(QStringLiteral("Tuple/Position"), QStringLiteral("L")).toString().trimmed();

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
        showlog(QStringLiteral("三元组获取失败：请在用例 ini 配置 Param/sku（及 Param/position）"));
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
    stepRuntime_.pass = tupleData_.success;
    stepRuntime_.testData = tupleData_.success
        ? QString("productKey:%1 deviceName:%2 deviceSecret:%3")
              .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret)
        : tupleData_.error;
    if (!tupleData_.success) {
        TestResult = failValue;
        showlog("三元组获取失败：" + tupleData_.error);
        return;
    }
    showlog(QStringLiteral("三元组获取成功：sn=%1 productKey=%2 deviceName=%3 deviceSecret=%4 mac=%5")
                .arg(tupleData_.sn, tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret, tupleData_.mac));
    // 蓝牙测试关键物料：与 MES「蓝牙测试」工站 SFC 生命周期表一致，各发一条 AddSfcKey（QTY=1）
    reportBydBluetoothMesKeyMaterials();
}

void QFreeWork::debugUpdateTupleMacStatus() {
    QString tupleMac = ui->macInput->text();
    tupleMac.remove(":");
    tupleMac.remove("-");
    tupleMac.remove(" ");
    tupleMac = tupleMac.trimmed().toUpper();

    if (tupleMac.isEmpty() || tupleMac == "没有MAC地址") {
        showlog("调试更新MAC状态失败：MAC为空");
        return;
    }
    if (!QTupleService::hasSharedSession()) {
        showlog(QStringLiteral("调试更新MAC状态失败：请先执行「三元组云端登录」"));
        return;
    }

    QTupleService service;
    QVariantMap statusMap;
    statusMap[QStringLiteral("mac")] = tupleMac;
    statusMap[QStringLiteral("status")] = 2;
    service.set(TupleCmd::DebugUpdateMacStatus, statusMap);
    if (!service.lastError().isEmpty()) {
        showlog("调试更新MAC状态失败：" + service.lastError());
        return;
    }
    showlog("调试更新MAC状态成功：mac=" + tupleMac + " status=1");
}

void QFreeWork::reportTupleWriteRecord() {
    stepRuntime_.done = true;
    const QString productSn = tupleData_.sn.trimmed();
    stepRuntime_.testData = productSn;
    if (!tupleData_.success) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = "无有效三元组";
        TestResult = failValue;
        showlog("三元组写入记录上报失败：未获取到有效三元组");
        return;
    }

    if (!QTupleService::hasSharedSession()) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog(QStringLiteral("三元组写入记录上报失败：请先执行「三元组云端登录」"));
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
        debugUpdateTupleMacStatus();
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

    const QString testData =
        QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3").arg(data.productId, data.deviceId, data.key);
    const QString ask = QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3")
                            .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);

    if (!tupleData_.success) {
        markActiveTestCaseStepDone(false, QStringLiteral("云端三元组未获取成功"), ask);
        TestResult = failValue;
        showlog(QStringLiteral("设备三元组比较失败：云端三元组未获取成功"));
        return true;
    }

    const bool productKeyPass = data.productId.trimmed() == tupleData_.productKey.trimmed();
    const bool deviceNamePass = data.deviceId.trimmed() == tupleData_.deviceName.trimmed();
    const bool deviceSecretPass = data.key.trimmed() == tupleData_.deviceSecret.trimmed();
    const bool pass = productKeyPass && deviceNamePass && deviceSecretPass;

    markActiveTestCaseStepDone(pass, testData, ask);
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
    const bool deviceSecretPass = data.key.trimmed() == tupleData_.deviceSecret.trimmed();
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
    dongleSuctionLastLeftKpa_ = data.leftKpa;
    dongleSuctionLastRightKpa_ = data.rightKpa;
    qDebug() << getIndex() << "Dongle吸力：左" << data.leftKpa << "kPa 右" << data.rightKpa << "kPa";
    if (dongleSuctionSampleActive_) {
        dongleSuctionLeftSamples_.append(data.leftKpa);
        dongleSuctionRightSamples_.append(data.rightKpa);
    }
    if (dongleSuctionReadEnabled_)
        appendSuctionChartSample(data.leftKpa, data.rightKpa);
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

    const QString testData = QString::number(temp);
    markActiveTestCaseStepDone(true, testData, QStringLiteral("[0,255]"));
    showlog(QStringLiteral("电池温度：%1°C").arg(temp));
}

void QFreeWork::refreshResultCode(ProtocolResultData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolResultData"), QVariant::fromValue(data)))
        return;
    showlog(QStringLiteral("协议结果码：%1").arg(data.result));
}

void QFreeWork::refreshTypeStatus(ProtocolTypeData data) {
    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(data)))
        return;
}

void QFreeWork::refreshButton(ProtocolButtonStateData data) {
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
                // 程控电源 VISA/USB 回读为 A，设置页工作电流卡控为 mA
                if (data.unit == QLatin1String("A") &&
                    (data.deviceName == QLatin1String("VISA_Power") ||
                     data.deviceName == QLatin1String("USB_Power"))) {
                    data.value *= 1000.0;
                    data.valueText = QString::number(data.value, 'f', 4);
                    data.unit = QStringLiteral("mA");
                }
                measure_ammeter = data.value;
                showlog(QStringLiteral("上报电流值: %1 mA").arg(data.value, 0, 'f', 4));
            } else if (data.type == QLatin1String("Voltage")) {
                showlog(QStringLiteral("上报电压值: %1 V").arg(data.value, 0, 'f', 4));
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
