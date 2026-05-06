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

bool QFreeWork::completeCurrentStep(const QString& functionName, bool pass, const QString& testData,
                                    const QString& failLog, const QString& passLog) {
    if (!isCurrentStep(functionName)) {
        return false;
    }

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = testData;
    if (!pass) {
        TestResult = failValue;
        if (!failLog.isEmpty()) {
            showlog(failLog);
        }
    } else if (!passLog.isEmpty()) {
        showlog(passLog);
    }
    return true;
}

void QFreeWork::refreshBaseData(ProtocolBaseInfoData data) {
    QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
    QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();
    QString Age_State = SETTINGS.value("ProductInfo/Age_State").toString();
    product = data.product_name;
    wifiMac.clear();
    for (int var = 0; var < data.wifi_mac.size; ++var) {
        wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
        if (var < data.wifi_mac.size - 1)
            wifiMac += ":";
    }
    qDebug() << getIndex() << "设备的 wifiMac:" << wifiMac;

    if (data.soft_version == softwareVersion && data.res_version == resourceVersion &&
        QString::number(data.ageing_state) == Age_State) {
        showlog("软件版本正确" + data.soft_version);
        showlog("资源版本正确" + data.res_version);
        showlog("老化状态正确" + QString::number(data.ageing_state));
    } else {
        TestResult = failValue;
        showlog("状态错误");
        showlog("当前设备软件版本" + data.soft_version + "配置文件版本" + softwareVersion);
        showlog("当前设备资源版本" + data.res_version + "配置文件版本" + resourceVersion);
        showlog("当前设备老化状态" + QString::number(data.ageing_state) + "配置文件老化要求" + Age_State);

        // isTestContinue = false;
        // showlog("停止运行");

        // ui->macInput->clear();
        // ui->getMac->clear();
        // ui->getMac->setFocus();
    }
}

void QFreeWork::refreshBattaryData(ProtocolBatteryData adc) {
    // QString chargeStateStr;
    // switch (adc.chargeState) {
    //     case 1:
    //         chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>";
    //         chargestate = "CHARGE_FULL";
    //         break;
    //     case 2:
    //         chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>";
    //         chargestate = "CHARGING";
    //         break;
    //     case 3:
    //         chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>";
    //         chargestate = "UNCHARGED";
    //         break;
    //     case 4:
    //         chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>";
    //         chargestate = "NO_BATTER";
    //         break;
    //     default:
    //         chargeStateStr = "充电状态为：<span style='color:red'>未知</span>";
    //         chargestate = "UNKOWN_STATE";
    //         break;
    // }
    // ui->battary_state->setText(chargeStateStr);

    // // 修改电量的显示样式
    // QString batteryPercentStr =
    //     "电量为：<span style='color:blue'>" + QString::number(adc.percent) + "%</span>";
    // ui->battary_value->setText(batteryPercentStr);

    // // 修改电压的显示样式
    // QString batteryVoltageStr = "电压为：<span style='color:purple'>" +
    //                             QString::number(adc.voltageMv / 1000.0, 'f', 3) +
    //                             "V</span>";
    // ui->battary_voltage->setText(batteryVoltageStr);

    // voltage = adc.voltageMv / 1000.0;
    // // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // // chargestate = match.captured(2);
    // is_battary_test = 1;
    // if (adc.chargeState == 2 && adc.voltageMv / 1000.0 > standbattary) {
    //     TestItem test;
    //     test.testItem = "充电测试";
    //     test.testData = "正在充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //     test.testResult = "通过";
    //     test.ask = "通过";
    //     testItems.append(test);

    //     testResultTableUpdate(testItems);

    //     charageresult = "通过";
    //     voltageresult = "通过";
    //     showlog("电量和充电测试通过");
    // }
    // if (adc.chargeState != 2 && adc.voltageMv / 1000.0 > standbattary) {
    //     TestItem test;
    //     test.testItem = "充电测试";
    //     test.testData = "不充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //     test.testResult = "失败";
    //     test.ask = "通过";
    //     testItems.append(test);

    //     testResultTableUpdate(testItems);

    //     showlog("充电状态不通过");
    //     charageresult = "失败";
    //     voltageresult = "通过";
    //     TestResult = failValue;
    // }
    // if (adc.chargeState == 2 && adc.voltageMv / 1000.0 <= standbattary)

    // {
    //     TestItem test;
    //     test.testItem = "充电测试";
    //     test.testData = "正在充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //     test.testResult = "失败";
    //     test.ask = "通过";
    //     testItems.append(test);

    //     testResultTableUpdate(testItems);

    //     showlog("电量测试不通过");
    //     voltageresult = "失败";
    //     charageresult = "通过";
    //     TestResult = failValue;
    // }
    // if (adc.chargeState != 2 && adc.voltageMv / 1000.0 <= standbattary) {
    //     TestItem test;
    //     test.testItem = "充电测试";
    //     test.testData = "不充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //     test.testResult = "失败";
    //     test.ask = "通过";
    //     testItems.append(test);

    //     testResultTableUpdate(testItems);

    //     showlog("电量和充电测试都不通过");
    //     voltageresult = "失败";
    //     charageresult = "失败";
    //     TestResult = failValue;
    // }

    // 电量测试为异步判定：在电池回调里回填当前步骤的 done/pass。
    if (stepRuntime_.started && stepRuntime_.functionId >= 0) {
        auto it = std::find_if(testFunctions.begin(), testFunctions.end(),
                               [this](const NamedFunction& item) { return item.id == stepRuntime_.functionId; });
        if (it == testFunctions.end() || it->name != "获取电量信息") {
            return;
        }
        // bool cardControlPass = pb->getState(Qpb::PbStateType::DisableSleep);
        // if (!cardControlPass) {
        //     showlog("电量测试卡控状态未满足：DisableSleep 未就绪");
        // }

        stepRuntime_.done = true;
        stepRuntime_.pass = (adc.percent >= standbattary);
        stepRuntime_.testData = QString("电量:%1%").arg(adc.percent);
        if (!stepRuntime_.pass) {
            TestResult = failValue;
        }
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

    // “获取整机SN码”步骤采用异步判定：设备返回SN必须与UI输入SN一致才通过。
    if (stepRuntime_.started && stepRuntime_.functionId >= 0) {
        auto it = std::find_if(testFunctions.begin(), testFunctions.end(),
                               [this](const NamedFunction& item) { return item.id == stepRuntime_.functionId; });
        if (it != testFunctions.end() && it->name == "获取整机SN码") {
            stepRuntime_.done = true;
            stepRuntime_.pass = (!expectedTailSnFromUiText.isEmpty() && deviceTailSnFromDevice == expectedTailSnFromUiText);
            stepRuntime_.testData = deviceTailSnFromDevice;
            if (!stepRuntime_.pass) {
                TestResult = failValue;
                showlog("整机SN校验失败，设备SN=" + deviceTailSnFromDevice + "，输入SN=" + expectedTailSnFromUiText);
            } else {
                showlog("整机SN校验通过");
            }
        }
    }

    // if (deviceTailSnFromDevice == "")
    // {
    //     QMessageBox::warning(NULL, "警告", " 该设备未绑定sn！\t\r\n");
    // }
}

void QFreeWork::refreshPeriphData(ProtocolPeriphStateData data) {
    // “获取外围设备状态”步骤采用异步判定：按设置项勾选和期望值判定通过。
    if (!(stepRuntime_.started && stepRuntime_.functionId >= 0)) {
        return;
    }
    auto it = std::find_if(testFunctions.begin(), testFunctions.end(),
                           [this](const NamedFunction& item) { return item.id == stepRuntime_.functionId; });
    if (it == testFunctions.end() || it->name != "获取外围设备状态") {
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
    auto appendPeriphItem = [&](const QString& name, const QString& value, const QString& expect, bool needCompare) {
        TestItem item;
        item.testItem = name;
        item.testData = value;
        item.ask = needCompare ? expect : "-";
        item.testResult = (!needCompare || compareVersions(expect, value)) ? "通过" : "失败";
        if (item.testResult == "失败") {
            pass = false;
        }
        periphTestItems.append(item);
    };

    appendPeriphItem("压感0状态", press0StateStr, press0Status, checkPress0);
    appendPeriphItem("压感1状态", press1StateStr, press1Status, checkPress1);
    appendPeriphItem("电池IC状态", batteryStateStr, batteryIcStatus, checkBatteryIc);
    appendPeriphItem("触摸IC状态", touchStateStr, touchIcStatus, checkTouchIc);
    appendPeriphItem("LED IC状态", ledStateStr, ledIcStatus, checkLedIc);
    appendPeriphItem("PD IC状态", pdStateStr, pdIcStatus, checkPdIc);
    testResultTableUpdate(periphTestItems);

    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = "详见外设分项";
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

void QFreeWork::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到电流数据" << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk) / 100;

    if (!conversionOk) {
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
        completeCurrentStep("读取治具电流测量值", false, "电流解析失败", "电流卡控失败：无法解析电流数据");
        return;
    }

    qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
    measure_ammeter = normalValue;
    const QString formattedValue = QString::number(normalValue, 'f', 4);
    qDebug() << getIndex() << "转换后的数值：" << formattedValue << "ma";
    showlog(formattedValue + "ma");

    const bool pass = (measure_ammeter >= LowCurrent && measure_ammeter <= HighCurrent);
    const QString failLog = QString("电流卡控失败，测量值=%1ma，范围=[%2,%3]ma")
                                .arg(formattedValue, QString::number(LowCurrent), QString::number(HighCurrent));
    completeCurrentStep("读取治具电流测量值", pass, formattedValue + "ma", failLog, "电流卡控通过");
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
