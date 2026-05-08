#include "qfreework.h"

#include <algorithm>

// 协议 / 治具 / dongle 回包：解析与条件判定（仍为 QFreeWork 成员，仅拆到本翻译单元）

bool QFreeWork::isCurrentStep(const QString& functionName) const {
    if (!stepRuntime_.started || stepRuntime_.functionId < 0) {
        return false;
    }
    auto it = std::find_if(testFunctions.cbegin(), testFunctions.cend(),
                           [this](const NamedFunction& item) { return item.id == stepRuntime_.functionId; });
    return it != testFunctions.cend() && it->name == functionName;
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

    product = data.product_name;
    wifiMac.clear();
    for (int var = 0; var < data.wifi_mac.size; ++var) {
        wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
        if (var < data.wifi_mac.size - 1)
            wifiMac += ":";
    }
    qDebug() << getIndex() << "设备的 wifiMac:" << wifiMac;

    if (!isCurrentStep("获取基本信息")) {
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
    stepRuntime_.testData = "详细如上";
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
    if (!isCurrentStep("获取电量信息")) {
        return;
    }
    const bool pass = (adc.percent >= standbattary);
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = QString("电量:%1%").arg(adc.percent);
    if (!pass) {
        TestResult = failValue;
        showlog(QString("电量卡控失败，当前%1%，要求≥%2%").arg(adc.percent).arg(standbattary));
    }
}

void QFreeWork::refreshWifiState(int state) {
    if (state) {
        // ui->WIFIStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        //  showlog("WIFI连接成功");
        wifistate = 1;
    } else {
        //  ui->WIFIStatusLabel->setText("WIFI连接：<font color='red'>失败</font>");
        //  showlog("WIFI连接断开");
        wifistate = 0;
    }
}

void QFreeWork::refreshSn(ProtocolSnData data) {
    deviceTailSnFromDevice = data.value.trimmed();
    const QString expectedTailSnFromUiText = ui->getMac->text().trimmed();
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "deviceTailSnFromDevice" << deviceTailSnFromDevice;
    ui->product_sn->setText("芯片存储的整机sn:" + deviceTailSnFromDevice);

    // “获取整机SN码”步骤采用异步判定：设备返回 SN 必须与 UI 输入一致才通过。
    if (!isCurrentStep("获取整机SN码")) {
        return;
    }
    QVector<TestItem> snItems;
    snItems.reserve(1);
    bool snPass = !expectedTailSnFromUiText.isEmpty();
    appendPeriphItem(snItems, snPass, "整机SN码", deviceTailSnFromDevice, expectedTailSnFromUiText, true);
    stepRuntime_.done = true;
    stepRuntime_.pass = snPass;
    stepRuntime_.testData = deviceTailSnFromDevice;
    if (!snPass) {
        TestResult = failValue;
        showlog("整机SN校验失败，设备SN=" + deviceTailSnFromDevice + "，输入SN=" + expectedTailSnFromUiText);
    } else {
        showlog("整机SN校验通过");
    }

    // if (deviceTailSnFromDevice == "")
    // {
    //     QMessageBox::warning(NULL, "警告", " 该设备未绑定sn！\t\r\n");
    // }
}

void QFreeWork::refreshPeriphData(ProtocolPeriphStateData data) {
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
    stepRuntime_.testData = "详细如上";
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
    const bool isBtStep = isCurrentStep("获取BT RSSI");
    const bool isBleStep = isCurrentStep("获取BLE RSSI");
    if (!isBtStep && !isBleStep) {
        return;
    }

    const QString itemName = isBtStep ? "BT RSSI" : "BLE RSSI";
    const QString value = QString::number(rssi);
    const QString ask = QString("[%1,%2]").arg(BleLowRssi).arg(BleHighRssi);
    const bool pass = (rssi > BleLowRssi && rssi < BleHighRssi);

    if (isBtStep) {
        ui->WIFI_RSSI->setText("BT的RSSI：" + value);
        BT_RSSI = value;
    } else {
        ui->BLE_RSSI->setText("BLE的RSSI:" + value);
        BLE_RSSI = value;
    }

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = value;
    stepRuntime_.ask = ask;
    if (!pass) {
        TestResult = failValue;
        showlog(QString("%1卡控失败，当前=%2，范围=%3").arg(itemName, value, ask));
    } else {
        showlog(QString("%1卡控通过，当前=%2").arg(itemName, value));
    }
}

void QFreeWork::refreshChargeCurrentRead(ProtocolUInt32ValueData data) {
    if (!isCurrentStep("读取充电电流")) {
        return;
    }

    const double currentMa = static_cast<double>(data.value);
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

void QFreeWork::applyTupleByMac() {
    const QString userName = SETTINGS.value("Tuple/AuthUser").toString();
    const QString password = SETTINGS.value("Tuple/AuthPassword").toString();
    const QString sku = SETTINGS.value("Tuple/Sku", "").toString();
    const QString position = SETTINGS.value("Tuple/Position", "L").toString();
    QString tupleMac = ui->macInput->text();
    tupleMac.remove(":");
    tupleMac.remove("-");
    tupleMac.remove(" ");
    tupleMac = tupleMac.trimmed().toUpper();

    stepRuntime_.done = true;
    stepRuntime_.ask = "获取成功";
    if (tupleMac.isEmpty() || tupleMac == "没有MAC地址") {
        stepRuntime_.pass = false;
        stepRuntime_.testData = "MAC为空";
        TestResult = failValue;
        showlog("三元组获取失败：MAC为空");
        return;
    }
    if (userName.isEmpty() || password.isEmpty()) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = "账号未配置";
        TestResult = failValue;
        showlog("三元组获取失败：Tuple/AuthUser 或 Tuple/AuthPassword 未配置");
        return;
    }

    QTupleService service;
    QString error;
    if (!service.login(userName, password, &error)) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = "登录失败";
        TestResult = failValue;
        showlog("三元组登录失败：" + error);
        return;
    }

    tupleData_ = service.applyTupleByMac(tupleMac, sku, position);
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
    showlog("三元组获取成功：productKey=" + tupleData_.productKey + " deviceName=" + tupleData_.deviceName);
}

void QFreeWork::debugUpdateTupleMacStatus() {
    const QString userName = SETTINGS.value("Tuple/AuthUser").toString();
    const QString password = SETTINGS.value("Tuple/AuthPassword").toString();
    QString tupleMac = ui->macInput->text();
    tupleMac.remove(":");
    tupleMac.remove("-");
    tupleMac.remove(" ");
    tupleMac = tupleMac.trimmed().toUpper();

    if (tupleMac.isEmpty() || tupleMac == "没有MAC地址") {
        showlog("调试更新MAC状态失败：MAC为空");
        return;
    }
    if (userName.isEmpty() || password.isEmpty()) {
        showlog("调试更新MAC状态失败：Tuple/AuthUser 或 Tuple/AuthPassword 未配置");
        return;
    }

    QTupleService service;
    QString error;
    if (!service.login(userName, password, &error)) {
        showlog("调试更新MAC状态登录失败：" + error);
        return;
    }
    if (!service.debugUpdateMacStatus(tupleMac, 2, &error)) {
        showlog("调试更新MAC状态失败：" + error);
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

    QTupleService service;
    QString error;
    if (!service.login(SETTINGS.value("Tuple/AuthUser").toString(), SETTINGS.value("Tuple/AuthPassword").toString(), &error)) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog("三元组写入记录上报登录失败：" + error);
        return;
    }
    const bool btRssiPass = BT_RSSI.toInt() > BleLowRssi && BT_RSSI.toInt() < BleHighRssi;
    const bool bleRssiPass = BLE_RSSI.toInt() > BleLowRssi && BLE_RSSI.toInt() < BleHighRssi;
    if (!service.reportWriteRecord(tupleData_, productSn, TestResult == failValue ? "NG" : "OK",
                                   BT_RSSI, btRssiPass, BLE_RSSI, bleRssiPass,
                                   softwareVersionForReport_, softwareVersionPassForReport_, &error)) {
        stepRuntime_.pass = false;
        TestResult = failValue;
        showlog("三元组写入记录上报失败：" + error);
        return;
    }
    stepRuntime_.pass = true;
    showlog("三元组写入记录上报成功");
}

void QFreeWork::refreshTupleData(ProtocolTupleData data) {
    if (!isCurrentStep("读取设备三元组并比较")) {
        return;
    }

    stepRuntime_.done = true;
    const bool productKeyPass = data.productId == tupleData_.productKey;
    const bool deviceNamePass = data.deviceId == tupleData_.deviceName;
    const bool deviceSecretPass = data.key == tupleData_.deviceSecret;
    const bool pass = tupleData_.success && productKeyPass && deviceNamePass && deviceSecretPass;

    stepRuntime_.pass = pass;
    stepRuntime_.testData = QString("productKey:%1 deviceName:%2 deviceSecret:%3").arg(data.productId, data.deviceId, data.key);
    stepRuntime_.ask = QString("productKey:%1 deviceName:%2 deviceSecret:%3")
                           .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);
    if (!pass) {
        TestResult = failValue;
        showlog(QString("设备三元组比较失败，设备 productKey=%1 deviceName=%2，云端 productKey=%3 deviceName=%4")
                    .arg(data.productId, data.deviceId, tupleData_.productKey, tupleData_.deviceName));
    } else {
        showlog("设备三元组比较通过");
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

void QFreeWork::getWifiMsg(QString data) {
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
