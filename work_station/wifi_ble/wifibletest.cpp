#include "wifibletest.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QRegExp>
#include <QSerialPort>
#include <QThread>
#include "common_utils.h"
#include "qproduct.h"
#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_wifibletest.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QString cmwGprfArbFileWriteCommand(const QString& rawPath) {
    const QString p = rawPath.trimmed();
    if (p.startsWith(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE"), Qt::CaseInsensitive)) {
        return p;
    }
    if (p.size() >= 2 && p.front() == QLatin1Char('\'') && p.back() == QLatin1Char('\'')) {
        return QStringLiteral("SOURce:GPRF:GEN:ARB:FILE %1").arg(p);
    }
    QString inner = p;
    if (inner.size() >= 2 && inner.front() == QLatin1Char('"') && inner.back() == QLatin1Char('"')) {
        inner = inner.mid(1, inner.size() - 2).trimmed();
    }
    return QStringLiteral("SOURce:GPRF:GEN:ARB:FILE '%1'").arg(inner);
}

} // namespace

void wifibletest::on_pushButton_clicked() {
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    // // ui->macInput->setText("3c:84:27:06:f7:5e");
    // // ui->macInput->setText("3c:84:27:29:50:32");
    // ui->macInput->setText("B4:56:5D:BF:53:71");
    ui->macInput->setText("b4:56:5d:bf:57:9d");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    ui->macInput->setText("3c:84:27:29:50:32");
    on_macInput_returnPressed();
    // // usb-> getlxMEASure();
    // // waitWork(1000);
}

wifibletest::wifibletest(int index, QWidget* parent) : test_base(parent), ui(new Ui::wifibletest) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = SINGLE_VER;

    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
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

    // if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
    //     ui->productComNameCombo->setEnabled(true);
    //     ui->productConnectButton->setEnabled(true);
    //     ui->productDisconnectButton->setEnabled(true);
    // } else {
    //     ui->productComNameCombo->setEnabled(false);
    //     ui->productConnectButton->setEnabled(false);
    //     ui->productDisconnectButton->setEnabled(false);
    // }

    ui->tabWidget->setTabText(0, "信号测试");
    // on_nfcComFresh_clicked();
    testResultTableInit();

    ui->tabWidget->setCurrentIndex(0); // 设置当前页为第一页

    // qDebug() << "比较结果" << compareVersions("101=102", "102");
}

wifibletest::~wifibletest() {
    closeBlePerUart();
    resetVisaBackend();
    delete ui;
}

bool wifibletest::isWifiBleTupleEnabled() const {
    return SETTINGS.value(QStringLiteral("Tuple/WifiBleEnable"), true).toBool();
}

void wifibletest::resetTupleBurnRuntime() {
    tupleData_ = TupleApplyResult{};
    tupleStepStarted_ = false;
    tupleReadDone_ = false;
    tupleReadPass_ = false;
    tupleReadData_.clear();
    tupleReadAsk_.clear();
    tupleMesItemvalue_.clear();
}

void wifibletest::appendTupleMesSegment(const QString& key, const QString& value) {
    const QString k = key.trimmed();
    if (k.isEmpty()) {
        return;
    }
    QString v = value;
    v.replace(QLatin1Char('|'), QStringLiteral("｜"));
    tupleMesItemvalue_ += QStringLiteral("|%1:%2").arg(k, v);
}

void wifibletest::appendTupleTestResult(const QString& item, const QString& data, const QString& result, const QString& ask) {
    TestItem test;
    test.testItem = item;
    test.testData = data;
    test.testResult = result;
    test.ask = ask;
    testItems.append(test);
    testResultTableUpdate(testItems);
}

void wifibletest::finishTupleFailure(const QString& item, const QString& data, const QString& ask) {
    TestResult = failValue;
    appendTupleMesSegment(item, QStringLiteral("FAIL;%1").arg(data));
    appendTupleTestResult(item, data, failValue, ask);
    state = STATE_SAVE_RESULT;
    tupleStepStarted_ = false;
}

bool wifibletest::applyTupleByMacForWifiBle() {
    tupleData_ = TupleApplyResult{};
    const QString userName = SETTINGS.value(QStringLiteral("Tuple/AuthUser")).toString();
    const QString password = SETTINGS.value(QStringLiteral("Tuple/AuthPassword")).toString();
    const QString sku = SETTINGS.value(QStringLiteral("Tuple/Sku"), QString()).toString();
    const QString position = SETTINGS.value(QStringLiteral("Tuple/Position"), QStringLiteral("L")).toString();
    QString tupleMac = ui->macInput->text();
    tupleMac.remove(QStringLiteral(":"));
    tupleMac.remove(QStringLiteral("-"));
    tupleMac.remove(QStringLiteral(" "));
    tupleMac = tupleMac.trimmed().toUpper();

    if (tupleMac.isEmpty() || tupleMac == QStringLiteral("没有MAC地址")) {
        showlog(QStringLiteral("三元组获取失败：MAC为空"));
        return false;
    }
    if (userName.isEmpty() || password.isEmpty()) {
        showlog(QStringLiteral("三元组获取失败：Tuple/AuthUser 或 Tuple/AuthPassword 未配置"));
        return false;
    }

    QTupleService service;
    QVariantMap loginMap;
    loginMap[QStringLiteral("userName")] = userName;
    loginMap[QStringLiteral("password")] = password;
    service.set(TupleCmd::Login, loginMap);
    if (!service.lastError().isEmpty()) {
        showlog(QStringLiteral("三元组登录失败：") + service.lastError());
        return false;
    }

    QVariantMap applyMap;
    applyMap[QStringLiteral("mac")] = tupleMac;
    applyMap[QStringLiteral("sku")] = sku;
    applyMap[QStringLiteral("position")] = position;
    service.get(TupleCmd::ApplyTupleByMac, applyMap);
    tupleData_ = service.lastApplyResult();
    if (!tupleData_.success) {
        showlog(QStringLiteral("三元组获取失败：") + tupleData_.error);
        return false;
    }
    showlog(QStringLiteral("三元组获取成功：productKey=%1 deviceName=%2 deviceSecret=%3")
                .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret));
    return true;
}

bool wifibletest::failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason) {
    if (!tupleData_.success) {
        finishTupleFailure(stepName, QStringLiteral("云端三元组未获取成功"));
        showlog(stepName + QStringLiteral("失败：云端三元组未获取成功"));
        return true;
    }
    if (!fieldOk) {
        finishTupleFailure(stepName, emptyReason);
        showlog(stepName + QStringLiteral("失败：") + emptyReason);
        return true;
    }
    return false;
}

void wifibletest::startTupleWriteProductKey() {
    if (failTupleWriteIfNoValidField(QStringLiteral("写入productKey"), !tupleData_.productKey.isEmpty(), QStringLiteral("productKey为空"))) {
        return;
    }
    sendCommandWithRetry([&]() {
        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SKUID, tupleData_.productKey.toUtf8()}));
    });
}

void wifibletest::startTupleWriteDeviceName() {
    if (failTupleWriteIfNoValidField(QStringLiteral("写入deviceName"), !tupleData_.deviceName.isEmpty(), QStringLiteral("deviceName为空"))) {
        return;
    }
    sendCommandWithRetry([&]() {
        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SUB_PID, tupleData_.deviceName.toUtf8()}));
    });
}

void wifibletest::startTupleWriteDeviceSecret() {
    if (failTupleWriteIfNoValidField(QStringLiteral("写入deviceSecret"), !tupleData_.deviceSecret.isEmpty(), QStringLiteral("deviceSecret为空"))) {
        return;
    }
    sendCommandWithRetry([&]() {
        QVariantMap map;
        map[QStringLiteral("value")] = tupleData_.deviceSecret.toUtf8();
        protocolManager.set(DeviceCmd::WriteKey, map);
    });
}

void wifibletest::startTupleReadCompare() {
    tupleReadDone_ = false;
    tupleReadPass_ = false;
    tupleReadData_.clear();
    tupleReadAsk_ = QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3")
                        .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);
    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::TupleRead); });
}

bool wifibletest::reportTupleWriteRecordForWifiBle() {
    const QString productSn = tupleData_.sn.trimmed();
    if (!tupleData_.success) {
        showlog(QStringLiteral("三元组写入记录上报失败：未获取到有效三元组"));
        return false;
    }

    QTupleService service;
    QVariantMap loginMap;
    loginMap[QStringLiteral("userName")] = SETTINGS.value(QStringLiteral("Tuple/AuthUser")).toString();
    loginMap[QStringLiteral("password")] = SETTINGS.value(QStringLiteral("Tuple/AuthPassword")).toString();
    service.set(TupleCmd::Login, loginMap);
    if (!service.lastError().isEmpty()) {
        showlog(QStringLiteral("三元组写入记录上报登录失败：") + service.lastError());
        return false;
    }
    const bool btRssiPass = intblerssi > BleLowRssi && intblerssi < BleHighRssi;
    const bool bleRssiPass = intblerssi > BleLowRssi && intblerssi < BleHighRssi;
    QVariantMap reportMap;
    reportMap[QStringLiteral("productKey")] = tupleData_.productKey;
    reportMap[QStringLiteral("deviceName")] = tupleData_.deviceName;
    reportMap[QStringLiteral("deviceSecret")] = tupleData_.deviceSecret;
    reportMap[QStringLiteral("sn")] = tupleData_.sn;
    reportMap[QStringLiteral("productSn")] = productSn;
    reportMap[QStringLiteral("result")] = TestResult == failValue ? QStringLiteral("NG") : QStringLiteral("OK");
    reportMap[QStringLiteral("btRssi")] = BLE_RSSI;
    reportMap[QStringLiteral("btRssiPass")] = btRssiPass;
    reportMap[QStringLiteral("bleRssi")] = BLE_RSSI;
    reportMap[QStringLiteral("bleRssiPass")] = bleRssiPass;
    reportMap[QStringLiteral("softwareVersion")] = SETTINGS.value(QStringLiteral("ProductInfo/Software_Version")).toString();
    reportMap[QStringLiteral("softwareVersionPass")] = true;
    service.set(TupleCmd::ReportWriteRecord, reportMap);
    if (!service.lastError().isEmpty()) {
        showlog(QStringLiteral("三元组写入记录上报失败：") + service.lastError());
        return false;
    }
    showlog(QStringLiteral("三元组写入记录上报成功"));
    return true;
}

void wifibletest::reportBydSfcKey(const QString& dataName, const QVariant& dataValue, int qty) {
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
    p.testCount = qMax(1, qty);
    p.iskeydata = 1;

    if (p.sn.isEmpty() || p.itemvalue.isEmpty()) {
        showlog(QStringLiteral("关键数据上报失败：SFC 或 DATA_VALUE 为空"));
        return;
    }
    showlog(QStringLiteral("MES：AddSfcKey 上报 %1=%2").arg(p.instruct_num, p.itemvalue));
    emit send_mes_test_value(p);
}

void wifibletest::refreshTupleData(ProtocolTupleData data) {
    if (state != STATE_TUPLE_READ_COMPARE) {
        return;
    }
    tupleReadDone_ = true;
    const bool productKeyPass = data.productId == tupleData_.productKey;
    const bool deviceNamePass = data.deviceId == tupleData_.deviceName;
    const bool deviceSecretPass =
        CommonUtils::matchTupleDeviceSecret(data.key, data.keyCipherHex, tupleData_.deviceSecret);
    tupleReadPass_ = tupleData_.success && productKeyPass && deviceNamePass && deviceSecretPass;
    tupleReadData_ = QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3").arg(data.productId, data.deviceId, data.key);
    tupleReadAsk_ = QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3")
                        .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret);

    if (!tupleReadPass_) {
        showlog(QStringLiteral("设备三元组比较失败，设备 productKey=%1 deviceName=%2，云端 productKey=%3 deviceName=%4")
                    .arg(data.productId, data.deviceId, tupleData_.productKey, tupleData_.deviceName));
    } else {
        showlog(QStringLiteral("设备三元组比较通过"));
    }
}

void wifibletest::refreshBaseData(ProtocolBaseInfoData data) {
    if (refresh_base_times) {
        refresh_base_times = 0;
        showlog("当前产品名字为" + QString(data.product_name));
        if (QString(data.product_name).compare("U7P") == 0 || QString(data.product_name).compare("U7") == 0) {
            showlog("开始写入nfc数据");
            QString value = getValueBySN(ui->getMac->text()).toUtf8();

            if ("SUBPID_ERRO" == value) {
                TestResult = failValue;
                isTestContinue = false;
                on_stopTest_clicked();
                TestItem test;
                test.testItem = "subpid";
                test.testData = value;
                test.testResult = "失败";
                test.ask = "通过";
                testItems.append(test);

                testResultTableUpdate(testItems);

                showlog("停止运行");
                showlog("没匹配到subpid");
                return;
            }
            TestItem test;
            test.testItem = "subpid";
            test.testData = value;
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            if (QString(data.product_name).compare("U7P") == 0) {
                nfcdataHeadText = "033BD2023668772001004800324F3130" + value + "810800272000141785911410";
                showlog("当前nfc写入的是U7P!");
            }
            if (QString(data.product_name).compare("U7") == 0) {
                nfcdataHeadText = "033BD2023668772001004800324F3045" + value + "810800272000141785911410";
                showlog("当前nfc写入的是U7!");
            }

            on_nfc_write_read_clicked();
        }

        // 读取软件版本字符串
        QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();

        // 读取资源版本字符串
        QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();

        // 读取老化状态字符串
        QString ageState = SETTINGS.value("ProductInfo/Age_State").toString();

        // 读取蓝牙版本字符串
        QString bleVersion = SETTINGS.value("ProductInfo/Ble_Ver").toString();

        // 读取压感版本字符串
        QString pressureSenseVersion = SETTINGS.value("ProductInfo/Pressure_Sense_Version").toString();
        // 读取电机压感版本字符串
        QString fsensorVersion = SETTINGS.value("ProductInfo/FSensor_Version").toString();
        // 读取电机版本字符串
        QString motorVersion = SETTINGS.value("ProductInfo/Motor_Ver").toString();

        wifiMac.clear();
        for (int var = 0; var < data.wifi_mac.size; ++var) {
            wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
            if (var < data.wifi_mac.size - 1)
                wifiMac += ":";
        }
        qDebug() << getIndex() << "设备的 wifiMac:" << wifiMac;

        if (!pressureSenseVersion.contains(data.presure_version) && allow_retry) {
            showlog("当前设备压感版本" + data.presure_version + "配置文件压感要求" +
                    pressureSenseVersion);
            allow_retry = 0;
            showlog("压感版本错误尝试重新获取一次");
            protocolManager.get(DeviceCmd::BaseInfo);
            refresh_base_times = 1;
            return;
        }

        // bool isProductTest = SETTINGS.value("ProductInfo/ProductName_checkBox").toBool();
        // bool isHwTest = SETTINGS.value("ProductInfo/HardwareVersion_checkBox").toBool();
        bool isSoftwareTest = SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool();
        bool isResourceTest = SETTINGS.value("ProductInfo/ResourceVersion_checkBox").toBool();
        bool isMotorTest = SETTINGS.value("ProductInfo/MotorVersion_checkBox").toBool();
        // bool isAppProtocolTest = SETTINGS.value("ProductInfo/AppPB_checkBox").toBool();
        // bool isFactoryProtocolTest = SETTINGS.value("ProductInfo/FactoryPB_checkBox").toBool();
        // bool isAlgoTest = SETTINGS.value("ProductInfo/AlgorithmVersion_checkBox").toBool();
        bool isPresureTest = SETTINGS.value("ProductInfo/PressureVersion_checkBox").toBool();
        bool isFSensorTest = SETTINGS.value("ProductInfo/FSensorVersion_checkBox").toBool();
        bool isBleTest = SETTINGS.value("ProductInfo/BluetoothVersion_checkBox").toBool();
        bool isAgeStatet = SETTINGS.value("ProductInfo/AgingStatus_checkBox").toBool();

        // 检查软件版本、资源版本和老化状态是否匹配
        if ((!isSoftwareTest || compareVersions(softwareVersion, data.soft_version)) &&
            (!isResourceTest || compareVersions(resourceVersion, data.res_version)) &&
            (!isBleTest || compareVersions(bleVersion, data.ble_version)) &&
            (!isPresureTest || compareVersions(pressureSenseVersion, data.presure_version)) &&
            (!isFSensorTest || compareVersions(fsensorVersion, data.fsensor_version)) &&
            (!isMotorTest || compareVersions(motorVersion, data.motor_version)) &&
            (!isAgeStatet || compareVersions(ageState, QString::number(data.ageing_state)))) {
            showlog("软件版本正确" + data.soft_version);
            showlog("资源版本正确" + data.res_version);
            showlog("老化状态正确" + QString::number(data.ageing_state));
            showlog("蓝牙版本正确" + data.ble_version);
            showlog("压感状态正确" + data.presure_version);
            showlog("电机压感状态正确" + data.fsensor_version);
            showlog("电机版本正确" + data.motor_version);
            showlog("软件版本正确");

        } else {
            pack.error = "SP03015";
            showlog("版本错误");
            showlog("当前设备软件版本" + data.soft_version + "配置文件软件版本" + softwareVersion);
            showlog("当前设备资源版本" + data.res_version + "配置文件资源版本" + resourceVersion);
            showlog("当前设备老化状态" + QString::number(data.ageing_state) + "配置文件老化要求" + ageState);
            showlog("当前设备蓝牙版本" + data.ble_version + "配置文件蓝牙要求" + bleVersion);
            showlog("当前设备压感版本" + data.presure_version + "配置文件压感要求" +
                    pressureSenseVersion);
            showlog("当前设备电机压感版本" + data.fsensor_version + "配置文件电机压感要求" +
                    fsensorVersion);
            showlog("当前设备电机版本" + data.motor_version + "配置文件电机要求" + motorVersion);

            TestResult = failValue;
            isTestContinue = false;
            showlog("停止运行");
            // at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00");  // 发送mac地址
            waitWork(100);
            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);

            ui->macInput->clear();
            ui->getMac->clear();
            // ui->getMac->setFocus();
            on_disconnectButton_clicked();

            ui->test_result->setText("FAIL");
            ui->test_result->setStyleSheet(
                "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                "border-radius: 10px; padding: 10px; text-align: center; ");
        }

        TestItem test;

        if (isAgeStatet) {
            test.testItem = "老化状态";
            test.testData = QString::number(data.ageing_state);
            test.ask = ageState;
            testItems.append(test);
        }

        // Check for Pressure version
        if (isPresureTest) {
            test.testItem = "按键压感版本";
            test.testData = data.presure_version;
            test.ask = pressureSenseVersion;
            testItems.append(test);
        }

        if (isFSensorTest) {
            test.testItem = "轴压感版本";
            test.testData = data.fsensor_version;
            test.ask = fsensorVersion;
            testItems.append(test);
        }
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

        updateTestData(testItems);
    }
}

void wifibletest::refreshBattaryData(ProtocolBatteryData adc) {
    QString chargeStateStr;
    switch (adc.chargeState) {
    case 1:
        chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>";
        chargestate = "CHARGE_FULL";
        break;
    case 2:
        chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>";
        chargestate = "CHARGING";
        break;
    case 3:
        chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>";
        chargestate = "UNCHARGED";
        break;
    case 4:
        chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>";
        chargestate = "NO_BATTER";
        break;
    default:
        chargeStateStr = "充电状态为：<span style='color:red'>未知</span>";
        chargestate = "UNKOWN_STATE";
        break;
    }
    ui->battary_state->setText(chargeStateStr);
    // 修改电量的显示样式
    QString batteryPercentStr =
        "电量为：<span style='color:blue'>" + QString::number(adc.percent) + "%</span>";
    ui->battary_value->setText(batteryPercentStr);
    // 修改电压的显示样式
    QString batteryVoltageStr = "电压为：<span style='color:purple'>" +
        QString::number(adc.voltageMv / 1000.0, 'f', 3) +
        "V</span>";
    ui->battary_voltage->setText(batteryVoltageStr);

    voltage = adc.voltageMv / 1000.0;
    battary = adc.percent;
    if (adc.percent >= standbattary) {
        is_battary_test = 1; // 正常
    }
    if (adc.percent < standbattary)
        is_battary_test = 2; // 低电量
    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);
    // is_battary_test = 1;
    //     if (adc.chargeState == 2 && adc.voltageMv / 1000.0 > standbattary) {
    //         TestItem test;
    //         test.testItem = "充电测试";
    //         test.testData = "正在充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //         test.testResult = "通过";
    //         test.ask = "通过";
    //         testItems.append(test);

    //         testResultTableUpdate(testItems);

    //         charageresult = "通过";
    //         voltageresult = "通过";
    //         showlog("电量和充电测试通过");
    //     }
    //     if (adc.chargeState != 2 && adc.voltageMv / 1000.0 > standbattary) {
    //         TestItem test;
    //         test.testItem = "充电测试";
    //         test.testData = "充电断开" + QString::number(adc.voltageMv / 1000.0) + "V";
    //         test.testResult = "失败";
    //         test.ask = "通过";
    //         testItems.append(test);

    //         pack.error = "SP03016";

    //         testResultTableUpdate(testItems);

    //         showlog("充电状态不通过");
    //         charageresult = "失败";
    //         voltageresult = "通过";
    //         TestResult = failValue;
    //     }
    //     if (adc.chargeState == 2 && adc.voltageMv / 1000.0 <= standbattary) {
    //         TestItem test;
    //         test.testItem = "充电测试";
    //         test.testData = "正在充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //         test.testResult = "失败";
    //         test.ask = "通过";
    //         testItems.append(test);

    //         testResultTableUpdate(testItems);
    //         pack.error = "SP03013";
    //         showlog("电量测试不通过");
    //         voltageresult = "失败";
    //         charageresult = "通过";
    //         TestResult = failValue;
    //     }
    //     if (adc.chargeState != 2 && adc.voltageMv / 1000.0 <= standbattary) {
    //         TestItem test;
    //         test.testItem = "充电测试";
    //         test.testData = "不充电" + QString::number(adc.voltageMv / 1000.0) + "V";
    //         test.testResult = "失败";
    //         test.ask = "通过";
    //         testItems.append(test);
    //         pack.error = "SP03013";
    //         testResultTableUpdate(testItems);

    //         showlog("电量和充电测试都不通过");
    //         voltageresult = "失败";
    //         charageresult = "失败";
    //         TestResult = failValue;
    //     }
}

void wifibletest::refreshWifiState(int state) {
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

void wifibletest::refreshSn(ProtocolSnData data) {
    stringsn = data.value;
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->product_sn->setText("芯片存储的整机sn:" + stringsn);

    if (stringsn == "") {
        QMessageBox::warning(NULL, "警告", " 该设备未绑定sn！\t\r\n");
    }
}

void wifibletest::refreshDongleWifi(QString data) {
    showlog("获取到了wifi名字" + data);

    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
}

void wifibletest::refreshBleRssi(QString data) {
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

void wifibletest::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void wifibletest::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}
void wifibletest::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        showlog("usb串口连接断开");

        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }
}

void wifibletest::refreshProductUartState(int state) {
    if (state)
        showlog("product串口连接成功");
    else {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        showlog("product串口连接断开");
    }
}

void wifibletest::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到电流数据" << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk) / 100;

    if (conversionOk) {
        // 转换成功
        qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
        QString formattedValue = QString::number(normalValue, 'f', 4);
        qDebug() << getIndex() << "转换后的数值：" << formattedValue << "ma";
        // ui->log->appendPlainText(formattedValue+"ma");
        showlog(formattedValue + "ma");
    } else {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

void wifibletest::refreshWifiMsg(QString data) {
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
void wifibletest::initData() {
    refresh_base_times = 1;
    refresh_periph_times = 1;
    allow_retry = 1;
    measure_ammeter_counts = 0;
    ui->product_sn->setText("芯片存储的整机sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;

    rssitestfailcount = 0;
    wifistate = 0;
    measure_ammeter = 0;
    isovertime = 0;
    BLE_RSSI = "";
    WIFI_RSSI = "";
    is_battary_test = 0;
    charageresult = "未测";
    voltageresult = "未测";
    currentresult = "未测";
    blePerRxDone = false;
    blePerRxPass = true;
    blePerRxMesValue = QStringLiteral("未测");
    blePerRxScenarios_.clear();
    blePerRxScenarioIndex_ = 0;
    resetTupleBurnRuntime();
    pb->reset_all_pb();
    at->resetConnected();
    TestResult = passValue;
    wifiresult = "未测";
    bleresult = "未测";
    ui->battary_state->setText("充电状态为:");
    ui->battary_value->setText("电量为:");
    ui->battary_voltage->setText("电压为:");
    stringsn = "";
    TestTime.start();
}
// 获取下一个状态的函数
wifibletest::State wifibletest::getNextState(State currentState) {
    return static_cast<State>((static_cast<int>(currentState) + 1) % (STATE_SAVE_RESULT + 1));
}

QList<wifibletest::BlePerScenario> wifibletest::parseBlePerScenarioList() const {
    QList<BlePerScenario> list;
    const QString raw = SETTINGS.value(QStringLiteral("BlePer/ScenarioList"),
                                       QStringLiteral("2480:1M,2402:1M,2440:2M,2402:2M,2440:1M,2480:2M"))
                            .toString();
    const QStringList parts = raw.split(QRegExp("[,;\\r\\n]"), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        const QString token = part.trimmed();
        if (token.isEmpty()) {
            continue;
        }
        const QStringList fields = token.split(QLatin1Char(':'), Qt::SkipEmptyParts);
        if (fields.isEmpty()) {
            continue;
        }
        QString freqToken = fields.at(0).trimmed().toUpper();
        const QString phyToken = (fields.size() > 1 ? fields.at(1).trimmed().toUpper() : QStringLiteral("1M"));
        int freqMHz = 0;
        if (freqToken == QStringLiteral("LOW")) {
            freqMHz = 2402;
        } else if (freqToken == QStringLiteral("MID")) {
            freqMHz = 2440;
        } else if (freqToken == QStringLiteral("HIGH")) {
            freqMHz = 2480;
        } else {
            bool ok = false;
            freqMHz = freqToken.toInt(&ok);
            if (!ok) {
                continue;
            }
        }
        BlePerScenario s;
        s.frequencyMHz = freqMHz;
        s.channelIndex = frequencyToBlePerChannel(freqMHz);
        if (s.channelIndex < 0) {
            continue;
        }
        s.phyCode = (phyToken == QStringLiteral("2M")) ? 0x02 : 0x01;
        s.phyName = (s.phyCode == 0x02) ? QStringLiteral("2M") : QStringLiteral("1M");
        s.label = QStringLiteral("%1_%2").arg(freqMHz).arg(s.phyName);
        list.append(s);
    }
    return list;
}

int wifibletest::frequencyToBlePerChannel(int freqMHz) const {
    if (freqMHz == 2402) {
        return 0;
    }
    if (freqMHz == 2440) {
        return 19;
    }
    if (freqMHz == 2480) {
        return 39;
    }
    if (freqMHz >= 2402 && freqMHz <= 2480) {
        return freqMHz - 2402;
    }
    return -1;
}

QByteArray wifibletest::buildBlePerRxStartCommand(int channelIndex, int phyCode) const {
    QByteArray cmd;
    cmd.append(char(0x01));
    cmd.append(char(0x33));
    cmd.append(char(0x20));
    cmd.append(char(0x03));
    cmd.append(char(channelIndex & 0xFF));
    cmd.append(char(phyCode & 0xFF));
    cmd.append(char(0x00));
    return cmd;
}

QByteArray wifibletest::hexStringToBytes(const QString& hex) const {
    QString clean;
    for (const QChar& c : hex) {
        if ((c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
            (c >= QLatin1Char('a') && c <= QLatin1Char('f')) ||
            (c >= QLatin1Char('A') && c <= QLatin1Char('F'))) {
            clean.append(c);
        }
    }
    if (clean.isEmpty() || (clean.size() % 2) != 0) {
        return {};
    }
    return QByteArray::fromHex(clean.toLatin1());
}

QString wifibletest::bytesToCompactHex(const QByteArray& data) const {
    return QString::fromLatin1(data.toHex().toUpper());
}

bool wifibletest::containsHexFragment(const QByteArray& response, const QString& fragment) const {
    return bytesToCompactHex(response).contains(fragment.trimmed().remove(QLatin1Char(' ')).toUpper());
}

bool wifibletest::openBlePerUart(QString* errorMessage) {
    if (blePerUart && blePerUart->isOpen()) {
        return true;
    }
    if (blePerUart == nullptr) {
        blePerUart = new QSerialPort(this);
    }
    const QString portName = SETTINGS.value(QStringLiteral("BlePer/UartPort"),
                                            ui->productComNameCombo ? ui->productComNameCombo->currentText() : QString())
                                 .toString()
                                 .trimmed();
    if (portName.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("BlePer/UartPort 未配置，且产品串口下拉框为空");
        }
        return false;
    }
    if (blePerUart->isOpen()) {
        blePerUart->close();
    }
    blePerUart->setPortName(portName);
    blePerUart->setBaudRate(SETTINGS.value(QStringLiteral("BlePer/UartBaudRate"), productBaudRate).toInt());
    blePerUart->setDataBits(QSerialPort::Data8);
    blePerUart->setParity(QSerialPort::NoParity);
    blePerUart->setStopBits(QSerialPort::OneStop);
    blePerUart->setFlowControl(QSerialPort::NoFlowControl);
    blePerUart->setReadBufferSize(4096);
    if (!blePerUart->open(QIODevice::ReadWrite)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("BLE PER HCI串口打开失败: %1").arg(blePerUart->errorString());
        }
        return false;
    }
    blePerUart->clear(QSerialPort::AllDirections);
    showlog(QStringLiteral("BLE PER HCI串口已打开: %1").arg(portName));
    return true;
}

void wifibletest::closeBlePerUart() {
    if (blePerUart) {
        if (blePerUart->isOpen()) {
            blePerUart->close();
        }
        delete blePerUart;
        blePerUart = nullptr;
    }
}

QByteArray wifibletest::sendBlePerUartCommandHex(const QString& hex, const QString& stageName, QString* errorMessage) {
    if (!blePerUart || !blePerUart->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("BLE PER HCI串口未打开");
        }
        return {};
    }
    const QByteArray cmd = hexStringToBytes(hex);
    if (cmd.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("非法HCI HEX: %1").arg(hex);
        }
        return {};
    }
    blePerUart->clear(QSerialPort::AllDirections);
    showlog(QStringLiteral("BLE_PER_UART_TX %1: %2").arg(stageName, bytesToCompactHex(cmd)));
    if (blePerUart->write(cmd) != cmd.size() || !blePerUart->waitForBytesWritten(200)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 写入失败: %2").arg(stageName, blePerUart->errorString());
        }
        return {};
    }

    QByteArray resp;
    const int timeoutMs = SETTINGS.value(QStringLiteral("BlePer/UartReadTimeoutMs"), 1000).toInt();
    const int quietMs = SETTINGS.value(QStringLiteral("BlePer/UartQuietMs"), 30).toInt();
    QElapsedTimer totalTimer;
    QElapsedTimer quietTimer;
    totalTimer.start();
    quietTimer.invalidate();
    while (totalTimer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents();
        if (blePerUart->waitForReadyRead(20)) {
            const QByteArray chunk = blePerUart->readAll();
            if (!chunk.isEmpty()) {
                resp.append(chunk);
                quietTimer.restart();
            }
        } else if (!resp.isEmpty() && quietTimer.isValid() && quietTimer.elapsed() >= quietMs) {
            break;
        }
    }
    showlog(QStringLiteral("BLE_PER_UART_RX %1: %2").arg(stageName, bytesToCompactHex(resp)));
    return resp;
}

int wifibletest::parseBlePerRxCount(const QByteArray& response, bool* ok) const {
    if (ok) {
        *ok = false;
    }
    if (response.size() < 2) {
        return -1;
    }
    const int low = static_cast<quint8>(response.at(response.size() - 2));
    const int high = static_cast<quint8>(response.at(response.size() - 1));
    if (ok) {
        *ok = true;
    }
    return low | (high << 8);
}

void wifibletest::loadWifiBleCmw100Config() {
    if (scpiVisaManager()) {
        scpiVisaManager()->loadCmwVisaFromSettings();
    }
}

bool wifibletest::cmwVisaWrite(const QString& cmd) {
    loadWifiBleCmw100Config();
    return scpiVisaManager() && scpiVisaManager()->exec(CmwScpiCmd::WriteLine, cmd);
}

bool wifibletest::cmwVisaQuery(const QString& cmd, QString* response) {
    loadWifiBleCmw100Config();
    if (!scpiVisaManager() || !scpiVisaManager()->exec(CmwScpiCmd::QueryLine, cmd)) {
        return false;
    }
    if (response) {
        *response = scpiVisaManager()->lastQueryResponse();
    }
    return true;
}

bool wifibletest::prepareBlePerCmw(QString* errorMessage) {
    loadWifiBleCmw100Config();
    if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed().isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("BlePer/CmwVisaAddress 未配置");
        }
        return false;
    }
    QString idn;
    const bool connected = scpiVisaManager() && scpiVisaManager()->exec(CmwScpiCmd::QueryLine, QStringLiteral("*IDN?"));
    if (connected) {
        idn = scpiVisaManager()->lastQueryResponse();
    }
    if (connected && !idn.trimmed().isEmpty()) {
        showlog(QStringLiteral("CMW100: %1").arg(idn.trimmed()));
    }
    if (!connected && errorMessage) {
        *errorMessage = QStringLiteral("CMW100 VISA连接失败: %1")
                            .arg(SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed());
    }
    return connected;
}

bool wifibletest::initializeBlePerCmwGprf(QString* errorMessage) {
    cmwVisaWrite(QStringLiteral("*CLS"));
    if (!SETTINGS.value(QStringLiteral("BlePer/CmwEnableFixedInit"), true).toBool()) {
        return true;
    }
    const int cycles = SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                                      SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
                           .toInt();
    const QString repetition = SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
    const double level = SETTINGS.value(QStringLiteral("BlePer/CmwTxPowerDbm"), -50.0).toDouble();
    QString opc;
    cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opc);
    cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:LIST OFF"));
    cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:BBMode ARB"));
    const QString waveform = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (!waveform.isEmpty()) {
        cmwVisaWrite(cmwGprfArbFileWriteCommand(waveform));
        QString ignored;
        cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &ignored);
    }
    cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(repetition));
    cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(qMax(1, cycles)));
    cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:RFSettings:LEVel %1").arg(level, 0, 'f', 1));
    cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"), &opc);
    QString err;
    if (SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterInit"), false).toBool() &&
        cmwVisaQuery(QStringLiteral("SYSTem:ERRor?"), &err) && !err.startsWith(QLatin1Char('0'))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CMW100 GPRF初始化错误: %1").arg(err);
        }
        return false;
    }
    return true;
}

bool wifibletest::parseBlePerCmwArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent) const {
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

bool wifibletest::waitBlePerCmwArbComplete(const BlePerScenario& scenario, QString* errorMessage) {
    const int cyclesSetting = SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                                             SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
                                  .toInt();
    const int targetCycles = SETTINGS.value(QStringLiteral("BlePer/CmwArbCompleteCycles"),
                                            qMax(0, cyclesSetting - 1))
                                 .toInt();
    const int pollIntervalMs = qMax(50, SETTINGS.value(QStringLiteral("BlePer/CmwArbPollIntervalMs"), 200).toInt());
    const int timeoutMs = qMax(500, SETTINGS.value(QStringLiteral("BlePer/CmwArbTimeoutMs"), 10000).toInt());
    QElapsedTimer timer;
    timer.start();
    QString lastResponse;
    int lastCycles = 0;
    while (timer.elapsed() < timeoutMs) {
        QString response;
        if (!cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:ARB:SCOunt?"), &response)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100发包进度查询失败").arg(scenario.label);
            }
            return false;
        }
        lastResponse = response;
        double countTime = 0.0;
        int cycles = 0;
        int samplesCurrent = 0;
        if (parseBlePerCmwArbScount(response, &countTime, &cycles, &samplesCurrent)) {
            lastCycles = cycles;
            showlog(QStringLiteral("CMW100发包进度 %1: time=%2s, cycles=%3, samples=%4")
                        .arg(scenario.label)
                        .arg(countTime, 0, 'f', 3)
                        .arg(cycles)
                        .arg(samplesCurrent));
            if (targetCycles <= 0 || cycles >= targetCycles) {
                return true;
            }
        } else {
            showlog(QStringLiteral("CMW100发包进度 %1: 无法解析SCOunt返回 %2").arg(scenario.label, response));
        }
        waitWork(pollIntervalMs);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("%1 CMW100 ARB发包超时，最后返回:%2，cycles=%3")
                            .arg(scenario.label, lastResponse)
                            .arg(lastCycles);
    }
    return false;
}

bool wifibletest::runBlePerCmwScenario(const BlePerScenario& scenario, QString* errorMessage) {
    const int cycles = SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                                      SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
                           .toInt();
    const auto stopCmwGen = [this]() {
        if (!SETTINGS.value(QStringLiteral("BlePer/CmwStopAfterScenario"), true).toBool()) {
            return;
        }
        QString opc;
        cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opc);
        QString state;
        if (cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe?"), &state)) {
            showlog(QStringLiteral("CMW100 GPRF状态: %1").arg(state));
        }
    };
    cmwVisaWrite(QStringLiteral("*CLS"));
    cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:RFSettings:FREQuency %1MHz").arg(scenario.frequencyMHz));
    if (!SETTINGS.value(QStringLiteral("BlePer/CmwUseGuiRfConfig"), true).toBool()) {
        const QString repetition = SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
        cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(repetition));
        cmwVisaWrite(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(qMax(1, cycles)));
    }
    // 产品串口仪器替代 DUT HCI 通讯；CMW100 仍按对标文档负责切频与 GPRF ARB 发包。
    QString opc;
    cmwVisaQuery(QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"), &opc);
    if (!cmwVisaWrite(QStringLiteral("TRIGger:GPRF:GEN:ARB:MANual:EXECute"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 CMW100触发发包失败").arg(scenario.label);
        }
        stopCmwGen();
        return false;
    }
    if (SETTINGS.value(QStringLiteral("BlePer/CmwWaitArbScount"), false).toBool()) {
        if (!waitBlePerCmwArbComplete(scenario, errorMessage)) {
            stopCmwGen();
            return false;
        }
    } else {
        waitWork(SETTINGS.value(QStringLiteral("BlePer/CmwTriggerWaitMs"), 1000).toInt());
    }
    if (SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterScenario"), false).toBool()) {
        QString err;
        if (cmwVisaQuery(QStringLiteral("SYSTem:ERRor?"), &err) && !err.startsWith(QLatin1Char('0'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100错误: %2").arg(scenario.label, err);
            }
            stopCmwGen();
            return false;
        }
    }
    stopCmwGen();
    return true;
}

bool wifibletest::runBlePerScenarioAttempt(const BlePerScenario& scenario, double* perOut, int* rxCountOut, QString* errorMessage) {
    const bool verify = SETTINGS.value(QStringLiteral("BlePer/VerifyHciResponse"), true).toBool();
    QString err;
    const QByteArray resetResp = sendBlePerUartCommandHex(
        SETTINGS.value(QStringLiteral("BlePer/HciResetHex"), QStringLiteral("01030C00")).toString(),
        QStringLiteral("HCI Reset"), &err);
    if (!err.isEmpty() || (verify && !containsHexFragment(resetResp, QStringLiteral("030C00")))) {
        if (errorMessage) {
            *errorMessage = err.isEmpty() ? QStringLiteral("%1 Reset应答异常: %2").arg(scenario.label, bytesToCompactHex(resetResp)) : err;
        }
        return false;
    }

    const QByteArray startCmd = buildBlePerRxStartCommand(scenario.channelIndex, scenario.phyCode);
    const QByteArray startResp = sendBlePerUartCommandHex(bytesToCompactHex(startCmd), QStringLiteral("Start RX ") + scenario.label, &err);
    bool rxStarted = err.isEmpty() && (!verify || containsHexFragment(startResp, QStringLiteral("332000")));
    if (!rxStarted) {
        if (errorMessage) {
            *errorMessage = err.isEmpty() ? QStringLiteral("%1 Start RX应答异常: %2").arg(scenario.label, bytesToCompactHex(startResp)) : err;
        }
        return false;
    }

    if (!runBlePerCmwScenario(scenario, errorMessage)) {
        sendBlePerUartCommandHex(SETTINGS.value(QStringLiteral("BlePer/HciTestEndHex"), QStringLiteral("011F2000")).toString(),
                                 QStringLiteral("Test End ") + scenario.label, nullptr);
        return false;
    }

    const QByteArray endResp = sendBlePerUartCommandHex(
        SETTINGS.value(QStringLiteral("BlePer/HciTestEndHex"), QStringLiteral("011F2000")).toString(),
        QStringLiteral("Test End ") + scenario.label, &err);
    if (!err.isEmpty() || (verify && !containsHexFragment(endResp, QStringLiteral("1F2000")))) {
        if (errorMessage) {
            *errorMessage = err.isEmpty() ? QStringLiteral("%1 Test End应答异常: %2").arg(scenario.label, bytesToCompactHex(endResp)) : err;
        }
        return false;
    }
    bool ok = false;
    const int rxCount = parseBlePerRxCount(endResp, &ok);
    const int txCount = SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt();
    if (!ok || txCount <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 RxCount解析失败或TxCount无效").arg(scenario.label);
        }
        return false;
    }
    const double per = (static_cast<double>(txCount - rxCount) / txCount) * 100.0;
    if (perOut) {
        *perOut = per;
    }
    if (rxCountOut) {
        *rxCountOut = rxCount;
    }
    return true;
}

bool wifibletest::ensureProductSerialForBlePer(const QString& stepName) {
    if (!product) {
        showlog(stepName + QStringLiteral("失败：Qproduct未初始化"));
        return false;
    }
    QComboBox* const prodCombo = getProductcomNameCombo();
    if (!prodCombo || prodCombo->currentText().trimmed().isEmpty()) {
        showlog(stepName + QStringLiteral("失败：请先选择产品串口仪器COM口"));
        return false;
    }
    if (!productSerialPort || !productSerialPort->isOpen()) {
        showlog(stepName + QStringLiteral("：正在打开产品串口仪器…"));
        on_productConnectButton_clicked();
        if (!productSerialPort || !productSerialPort->isOpen()) {
            showlog(stepName + QStringLiteral("失败：产品串口打开失败"));
            return false;
        }
    }
    product->clearProductSerialRxAccum();
    return true;
}

QByteArray wifibletest::brushInstrumentStartCmdForProfile(int profile) const {
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

bool wifibletest::waitBrushInstrumentResponse(const QByteArray& prefix, int timeoutMs, QString* errorMessage) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (!isTestContinue) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("测试已停止");
            }
            return false;
        }
        if (product && Qproduct::responseContainsPrefix(product->productSerialRxAccum(), prefix)) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(10);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待应答超时 %1ms").arg(timeoutMs);
    }
    return false;
}

int wifibletest::blePerProfileForScenario(const BlePerScenario& scenario) const {
    if (scenario.frequencyMHz == 2440 && scenario.phyCode == 0x01) {
        return 1;
    }
    if (scenario.frequencyMHz == 2480 && scenario.phyCode == 0x01) {
        return 2;
    }
    if (scenario.frequencyMHz == 2402 && scenario.phyCode == 0x02) {
        return 3;
    }
    if (scenario.frequencyMHz == 2440 && scenario.phyCode == 0x02) {
        return 4;
    }
    if (scenario.frequencyMHz == 2480 && scenario.phyCode == 0x02) {
        return 5;
    }
    return 0;
}

bool wifibletest::prepareBlePerRxStateMachine() {
    if (blePerRxDone) {
        return blePerRxPass;
    }
    blePerRxDone = true;
    blePerRxPass = true;
    blePerRxMesValue = QStringLiteral("PASS");
    blePerRxScenarioIndex_ = 0;
    blePerRxScenarios_ = parseBlePerScenarioList();
    if (blePerRxScenarios_.isEmpty()) {
        recordBlePerStepFailure(QStringLiteral("BLE PER RX 初始化"), QStringLiteral("场景为空，请检查 BlePer/ScenarioList"));
        return false;
    }

    QString errorMessage;
    if (!prepareBlePerCmw(&errorMessage) || !initializeBlePerCmwGprf(&errorMessage)) {
        recordBlePerStepFailure(QStringLiteral("BLE PER RX 初始化"), QStringLiteral("CMW100初始化失败: ") + errorMessage);
        return false;
    }
    showlog(QStringLiteral("BLE PER RX 初始化完成，场景数=%1").arg(blePerRxScenarios_.size()));
    return true;
}

bool wifibletest::runBlePerInstrumentResetStep() {
    if (blePerRxScenarioIndex_ < 0 || blePerRxScenarioIndex_ >= blePerRxScenarios_.size()) {
        return false;
    }
    const BlePerScenario scenario = blePerRxScenarios_.at(blePerRxScenarioIndex_);
    const QString stepName = QStringLiteral("BLE PER RX 仪器复位 ") + scenario.label;
    if (!ensureProductSerialForBlePer(stepName)) {
        recordBlePerStepFailure(stepName, QStringLiteral("产品串口仪器未就绪"));
        return false;
    }
    if (!SETTINGS.value(QStringLiteral("BrushInstrument/ResetBeforeEachRx"), true).toBool()) {
        showlog(stepName + QStringLiteral("跳过"));
        return true;
    }

    product->clearProductSerialRxAccum();
    QString err;
    const int ackTimeout = SETTINGS.value(QStringLiteral("BrushInstrument/StartAckTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("BrushInstrument/StopAckTimeoutMs"), 5000).toInt())
                               .toInt();
    if (!product->writeRaw(Qproduct::cmdReset(), &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("写复位失败: ") + err);
        return false;
    }
    if (!waitBrushInstrumentResponse(Qproduct::ackReset(), ackTimeout, &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("复位应答失败: ") + err);
        return false;
    }
    showlog(stepName + QStringLiteral("通过"));
    return true;
}

bool wifibletest::runBlePerStartRxStep() {
    if (blePerRxScenarioIndex_ < 0 || blePerRxScenarioIndex_ >= blePerRxScenarios_.size()) {
        return false;
    }
    const BlePerScenario scenario = blePerRxScenarios_.at(blePerRxScenarioIndex_);
    const QString stepName = QStringLiteral("BLE PER RX 开始接收 ") + scenario.label;
    if (!ensureProductSerialForBlePer(stepName)) {
        recordBlePerStepFailure(stepName, QStringLiteral("产品串口仪器未就绪"));
        return false;
    }

    product->clearProductSerialRxAccum();
    QString err;
    const int ackTimeout = SETTINGS.value(QStringLiteral("BrushInstrument/StartAckTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("BrushInstrument/StopAckTimeoutMs"), 5000).toInt())
                               .toInt();
    if (!product->writeRaw(brushInstrumentStartCmdForProfile(blePerProfileForScenario(scenario)), &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("写开始接收失败: ") + err);
        return false;
    }
    if (!waitBrushInstrumentResponse(Qproduct::ackStartReceive(), ackTimeout, &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("开始接收应答失败: ") + err);
        return false;
    }
    showlog(stepName + QStringLiteral("通过"));
    return true;
}

bool wifibletest::runBlePerCmwTxStep() {
    if (blePerRxScenarioIndex_ < 0 || blePerRxScenarioIndex_ >= blePerRxScenarios_.size()) {
        return false;
    }
    const BlePerScenario scenario = blePerRxScenarios_.at(blePerRxScenarioIndex_);
    const QString stepName = QStringLiteral("BLE PER RX CMW发包 ") + scenario.label;
    QString err;
    product->clearProductSerialRxAccum();
    if (!runBlePerCmwScenario(scenario, &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("CMW100发包失败: ") + err);
        product->writeRaw(Qproduct::cmdStopReceive(), nullptr);
        return false;
    }
    showlog(stepName + QStringLiteral("通过"));
    return true;
}

bool wifibletest::runBlePerStopRxPerStep() {
    if (blePerRxScenarioIndex_ < 0 || blePerRxScenarioIndex_ >= blePerRxScenarios_.size()) {
        return false;
    }
    const BlePerScenario scenario = blePerRxScenarios_.at(blePerRxScenarioIndex_);
    const QString stepName = QStringLiteral("BLE PER RX 停止接收与PER ") + scenario.label;
    QString err;
    if (!product->writeRaw(Qproduct::cmdStopReceive(), &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("写停止接收失败: ") + err);
        return false;
    }
    const int stopAckTimeout = SETTINGS.value(QStringLiteral("BrushInstrument/StopAckTimeoutMs"), 5000).toInt();
    if (!waitBrushInstrumentResponse(Qproduct::prefixStopReceive(), stopAckTimeout, &err)) {
        recordBlePerStepFailure(stepName, QStringLiteral("停止接收应答失败: ") + err);
        return false;
    }

    const int sendCount = SETTINGS.value(QStringLiteral("BrushInstrument/InstrumentSendPacketCount"), 1000).toInt();
    const double maxPer = SETTINGS.value(QStringLiteral("BrushInstrument/MaxPer"), 0.05).toDouble();
    const int recvPkts = Qproduct::parseStopReceivePacketCountLe(product->productSerialRxAccum());
    const double per = Qproduct::computePer(sendCount, recvPkts);
    const bool pass = (recvPkts >= 0) && (per <= maxPer);

    TestItem test;
    test.testItem = stepName;
    test.testData = QStringLiteral("仪器发包数=%1 收包=%2 PER=%3")
                        .arg(sendCount)
                        .arg(recvPkts)
                        .arg(per, 0, 'f', 4);
    test.testResult = pass ? passValue : failValue;
    test.ask = QStringLiteral("<=%1").arg(maxPer, 0, 'f', 4);
    testItems.append(test);
    testResultTableUpdate(testItems);
    showlog(stepName + (pass ? QStringLiteral("通过 ") : QStringLiteral("失败 ")) + test.testData);
    if (!pass) {
        blePerRxPass = false;
        blePerRxMesValue = QStringLiteral("FAIL");
    }
    return pass;
}

void wifibletest::recordBlePerStepFailure(const QString& stepName, const QString& reason) {
    blePerRxPass = false;
    blePerRxMesValue = QStringLiteral("FAIL");
    TestItem test;
    test.testItem = stepName;
    test.testData = reason;
    test.testResult = failValue;
    test.ask = QStringLiteral("通过");
    testItems.append(test);
    testResultTableUpdate(testItems);
    showlog(stepName + QStringLiteral("失败：") + reason);
}

void wifibletest::advanceBlePerScenarioOrContinue() {
    ++blePerRxScenarioIndex_;
    if (blePerRxScenarioIndex_ < blePerRxScenarios_.size()) {
        state = STATE_BLE_PER_INSTRUMENT_RESET;
        return;
    }
    continueAfterBlePerRx();
}

void wifibletest::continueAfterBlePerRx() {
    if (!blePerRxPass) {
        TestResult = failValue;
        pack.error = QStringLiteral("BLE_PER_RX_NG");
    }
    if (SETTINGS.value("SYSTEM/TestWifiSignal").toBool()) {
        on_connectwifi_clicked();
        state = STATE_WATI_WIFI_CONNECT;
    } else {
        sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetBattery); });
        state = STATE_WATI_CORRECT_BATTARY;
    }
}

void wifibletest::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
        case STATE_IDLE: // 复位一切
            initData();
            waitWork(1000);                                 //给开机时间
            at->set(DongleCmd::BleScanConnect, macAddress); // 开始连接
            showlog("MAC地址为：" + ui->macInput->text());
            showlog("开始测试");
            state = getNextState(state);

            break;
        case STATE_WATI_CONNECT: // 设置禁止休眠
            if (at->getConnected()) {
                showlog("蓝牙连接成功");
                sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacMode, 1); });
                state = getNextState(state);
            }
            break;
            // case STATE_DISABLE_SLEEP_1:  // 设置设备采集
            //     if (canGoNext) {
            //         sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacMode, 1); });
            //         showlog("已进入禁止休眠");
            //         state = getNextState(state);

            //     break;

        case STATE_FAC_MODE: // 设置设备采集
            if (canGoNext) {
                sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::BaseInfo); });
                showlog("已进入工厂模式");
                state = getNextState(state);
            }
            break;

        case STATE_WATI_BASE_INFO:
            if (canGoNext) {
                showlog("已检查设备状态");
                showlog("打开蓝牙日志");
                at->set(DongleCmd::BleLog, 1); // 日志开
                state = STATE_WATI_GET_CORRECT_BLERSSI;
            }

            break;

        case STATE_WATI_GET_CORRECT_BLERSSI:
            if (intblerssi != 0) {
                if (intblerssi < BleHighRssi && intblerssi > BleLowRssi) // 蓝牙信号可以
                {
                    showlog("蓝牙测试" + QString::number(intblerssi));
                    rssitestcount++;
                    if (rssitestcount >= RssiTestTime) // 蓝牙信号可以
                    {
                        TestItem test;
                        test.testItem = "蓝牙信号强度测试";
                        test.testData = BLE_RSSI;
                        test.testResult = "通过";
                        test.ask = "通过";
                        testItems.append(test);

                        testResultTableUpdate(testItems);

                        showlog("蓝牙测试通过" + QString::number(intblerssi) + "测试次数为" +
                                QString::number(rssitestcount));

                        rssitestcount = 0;
                        at->set(DongleCmd::BleLog, 0); // 日志关
                        if (SETTINGS.value(QStringLiteral("BlePer/EnableRxTest"), true).toBool()) {
                            state = STATE_BLE_PER_INIT;
                        } else {
                            continueAfterBlePerRx();
                        }
                    }
                } else {
                    rssitestcount = 0;
                    rssitestfailcount++;

                    if (rssitestfailcount >= RssiTestTime) // 蓝牙信号不可以
                    {
                        TestItem test;
                        pack.error = "SP03016";
                        test.testItem = "蓝牙信号强度测试";
                        test.testData = BLE_RSSI;
                        test.testResult = "失败";
                        test.ask = "通过";
                        testItems.append(test);

                        testResultTableUpdate(testItems);

                        TestResult = failValue;
                        qDebug() << getIndex() << "蓝牙不合格信号强度" << BLE_RSSI;
                        qDebug() << getIndex() << "范围为" << BleHighRssi << BleLowRssi;
                        showlog("蓝牙不合格信号强度intblerssi" + QString::number(intblerssi));

                        showlog("蓝牙不合格信号强度" + BLE_RSSI);
                        showlog("当前蓝牙范围为" + QString::number(BleHighRssi) + QString::number(BleLowRssi));

                        at->set(DongleCmd::BleLog, 0); // 日志关
                        rssitestfailcount = 0;

                        if (SETTINGS.value("SYSTEM/TestWifiSignal").toBool()) {
                            on_connectwifi_clicked();
                            state = STATE_WATI_WIFI_CONNECT;

                        } else {
                            wifiresult = "通过";
                            sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetBattery); });
                            state = STATE_WATI_CORRECT_BATTARY;
                        }
                    }
                }
            }
            waitWork(100);

            break;

        case STATE_BLE_PER_INIT:
            if (prepareBlePerRxStateMachine()) {
                state = STATE_BLE_PER_INSTRUMENT_RESET;
            } else {
                continueAfterBlePerRx();
            }
            break;

        case STATE_BLE_PER_INSTRUMENT_RESET:
            if (runBlePerInstrumentResetStep()) {
                state = STATE_BLE_PER_START_RX;
            } else {
                advanceBlePerScenarioOrContinue();
            }
            break;

        case STATE_BLE_PER_START_RX:
            if (runBlePerStartRxStep()) {
                state = STATE_BLE_PER_CMW_TX;
            } else {
                advanceBlePerScenarioOrContinue();
            }
            break;

        case STATE_BLE_PER_CMW_TX:
            if (runBlePerCmwTxStep()) {
                state = STATE_BLE_PER_STOP_RX_PER;
            } else {
                advanceBlePerScenarioOrContinue();
            }
            break;

        case STATE_BLE_PER_STOP_RX_PER:
            runBlePerStopRxPerStep();
            advanceBlePerScenarioOrContinue();
            break;

        case STATE_WATI_WIFI_CONNECT:

            if (wifistate) // wifi
            {
                showlog("wifi连接成功");
                state = STATE_WATI_GET_CORRECT_WIFIRSSI;
            }

            break;

        case STATE_WATI_GET_CORRECT_WIFIRSSI:

            if (intwifirssi != 0) {
                if (intwifirssi < HighRssi && intwifirssi > LowRssi) // wifi信号可以
                {
                    rssitestcount++;
                    showlog("WIFI测试" + QString::number(intwifirssi));

                    if (rssitestcount > RssiTestTime) // wifi信号可以
                    {
                        QString wifiName = "usmile_finish";
                        QString wifiPassword = "12345678";
                        QByteArray wifiNameBytes = wifiName.toUtf8();
                        QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
                        protocolManager.set(DeviceCmd::WifiConnect, QVariant::fromValue(WifiConnectPayload{wifiNameBytes, wifiPasswordBytes}));
                        //   protocolManager.set(DeviceCmd::WifiDisconnect);
                        wifiresult = "通过";
                        TestItem test;

                        test.testItem = "WIFI信号强度";
                        test.testData = WIFI_RSSI;
                        test.testResult = "通过";
                        test.ask = "通过";
                        testItems.append(test);

                        testResultTableUpdate(testItems);

                        sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetBattery); });
                        state = STATE_WATI_CORRECT_BATTARY;
                        rssitestcount = 0;
                    }
                } else {
                    rssitestcount = 0;
                    rssitestfailcount++;
                    if (rssitestfailcount > RssiTestTime) // wifi信号不可以
                    {
                        QString wifiName = "usmile_finish";
                        QString wifiPassword = "12345678";
                        QByteArray wifiNameBytes = wifiName.toUtf8();
                        QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
                        protocolManager.set(DeviceCmd::WifiConnect, QVariant::fromValue(WifiConnectPayload{wifiNameBytes, wifiPasswordBytes}));
                        //     protocolManager.set(DeviceCmd::WifiDisconnect);
                        TestResult = failValue;
                        TestItem test;

                        test.testItem = "WIFI信号强度";
                        test.testData = WIFI_RSSI;
                        test.testResult = TestResult;
                        test.ask = "通过";
                        testItems.append(test);

                        testResultTableUpdate(testItems);

                        qDebug() << getIndex() << "wifi不合格信号强度" << intwifirssi;
                        showlog("wifi不合格信号强度" + WIFI_RSSI);
                        rssitestfailcount = 0;
                        sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetBattery); });
                        state = STATE_WATI_CORRECT_BATTARY;
                    }
                }
            }
            waitWork(100);

            break;

        case STATE_WATI_CORRECT_BATTARY: // 设置禁止休眠
            if (is_battary_test != 0) {
                if (is_battary_test == 1) {
                    showlog("电量正常" + QString::number(battary) + "%");
                    TestItem test;
                    test.testItem = "当前电量";
                    test.testData = QString::number(battary) + "%";
                    test.testResult = "通过";
                    test.ask = "通过";
                    testItems.append(test);
                    testResultTableUpdate(testItems);
                    // protocolManager.get(DeviceCmd::PeriphState);
                    if (isWifiBleTupleEnabled()) {
                        resetTupleBurnRuntime();
                        showlog(QStringLiteral("电量校验通过，开始申请三元组并烧录校验"));
                        state = STATE_TUPLE_APPLY;
                    } else {
                        showlog(QStringLiteral("三元组烧录未启用，跳过"));
                        state = STATE_SAVE_RESULT;
                    }
                }

                if (is_battary_test == 2) {
                    showlog("当前电量低，为" + QString::number(battary) + "%");
                    // pack.error="SP03010";
                    TestItem test;
                    test.testItem = "当前电量";
                    test.testData = QString::number(battary);
                    test.testResult = "失败";
                    test.ask = "通过";
                    testItems.append(test);
                    testResultTableUpdate(testItems);

                    result = failValue;
                    state = STATE_SAVE_RESULT;
                }
            } else {
                showlog("正在重发获取电量信息");
                protocolManager.get(DeviceCmd::GetBattery);
            }
            break;

        case STATE_TUPLE_APPLY:
            if (!tupleStepStarted_) {
                tupleStepStarted_ = true;
                const bool ok = applyTupleByMacForWifiBle();
                appendTupleMesSegment(QStringLiteral("CLOUD_PRODUCT_KEY"), ok ? tupleData_.productKey : QStringLiteral("FAIL"));
                appendTupleMesSegment(QStringLiteral("CLOUD_DEVICE_NAME"), ok ? tupleData_.deviceName : QStringLiteral("FAIL"));
                appendTupleMesSegment(QStringLiteral("CLOUD_DEVICE_SECRET"), ok ? tupleData_.deviceSecret : QStringLiteral("FAIL"));
                appendTupleTestResult(QStringLiteral("获取云端三元组"),
                                      ok ? QStringLiteral("productKey:%1 deviceName:%2 deviceSecret:%3")
                                               .arg(tupleData_.productKey, tupleData_.deviceName, tupleData_.deviceSecret)
                                         : tupleData_.error,
                                      ok ? passValue : failValue);
                if (!ok) {
                    TestResult = failValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    state = STATE_TUPLE_WRITE_PRODUCT_KEY;
                }
                tupleStepStarted_ = false;
            }
            break;

        case STATE_TUPLE_WRITE_PRODUCT_KEY:
            if (!tupleStepStarted_) {
                tupleStepStarted_ = true;
                startTupleWriteProductKey();
            } else if (canGoNext) {
                const bool ok = !sendRetryOver;
                appendTupleMesSegment(QStringLiteral("WRITE_PRODUCT_KEY"), ok ? tupleData_.productKey : QStringLiteral("FAIL"));
                appendTupleTestResult(QStringLiteral("写入productKey"), ok ? tupleData_.productKey : QStringLiteral("超时"), ok ? passValue : failValue);
                if (!ok) {
                    TestResult = failValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    state = STATE_TUPLE_WRITE_DEVICE_NAME;
                }
                tupleStepStarted_ = false;
            }
            break;

        case STATE_TUPLE_WRITE_DEVICE_NAME:
            if (!tupleStepStarted_) {
                tupleStepStarted_ = true;
                startTupleWriteDeviceName();
            } else if (canGoNext) {
                const bool ok = !sendRetryOver;
                appendTupleMesSegment(QStringLiteral("WRITE_DEVICE_NAME"), ok ? tupleData_.deviceName : QStringLiteral("FAIL"));
                appendTupleTestResult(QStringLiteral("写入deviceName"), ok ? tupleData_.deviceName : QStringLiteral("超时"), ok ? passValue : failValue);
                if (!ok) {
                    TestResult = failValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    state = STATE_TUPLE_WRITE_DEVICE_SECRET;
                }
                tupleStepStarted_ = false;
            }
            break;

        case STATE_TUPLE_WRITE_DEVICE_SECRET:
            if (!tupleStepStarted_) {
                tupleStepStarted_ = true;
                startTupleWriteDeviceSecret();
            } else if (canGoNext) {
                const bool ok = !sendRetryOver;
                appendTupleMesSegment(QStringLiteral("WRITE_DEVICE_SECRET"), ok ? tupleData_.deviceSecret : QStringLiteral("FAIL"));
                appendTupleTestResult(QStringLiteral("写入deviceSecret"), ok ? tupleData_.deviceSecret : QStringLiteral("超时"), ok ? passValue : failValue);
                if (!ok) {
                    TestResult = failValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    state = STATE_TUPLE_READ_COMPARE;
                }
                tupleStepStarted_ = false;
            }
            break;

        case STATE_TUPLE_READ_COMPARE:
            if (!tupleStepStarted_) {
                tupleStepStarted_ = true;
                startTupleReadCompare();
            } else if (tupleReadDone_) {
                appendTupleMesSegment(QStringLiteral("READ_PRODUCT_KEY"), tupleReadPass_ ? tupleData_.productKey : QStringLiteral("FAIL"));
                appendTupleMesSegment(QStringLiteral("READ_DEVICE_NAME"), tupleReadPass_ ? tupleData_.deviceName : QStringLiteral("FAIL"));
                appendTupleMesSegment(QStringLiteral("READ_DEVICE_SECRET"), tupleReadPass_ ? tupleData_.deviceSecret : QStringLiteral("FAIL"));
                appendTupleTestResult(QStringLiteral("读取设备三元组并比较"), tupleReadData_, tupleReadPass_ ? passValue : failValue, tupleReadAsk_);
                if (!tupleReadPass_) {
                    TestResult = failValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    state = STATE_TUPLE_REPORT_WRITE;
                }
                tupleStepStarted_ = false;
            } else if (sendRetryOver) {
                finishTupleFailure(QStringLiteral("读取设备三元组并比较"), QStringLiteral("读取超时"), tupleReadAsk_);
            }
            break;

        case STATE_TUPLE_REPORT_WRITE:
            if (!tupleStepStarted_) {
                tupleStepStarted_ = true;
                const bool ok = reportTupleWriteRecordForWifiBle();
                appendTupleMesSegment(QStringLiteral("TUPLE_WRITE_REPORT"), ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"));
                appendTupleTestResult(QStringLiteral("上报三元组写入记录"), ok ? QStringLiteral("成功") : QStringLiteral("失败"), ok ? passValue : failValue);
                if (!ok) {
                    TestResult = failValue;
                }
                state = STATE_SAVE_RESULT;
                tupleStepStarted_ = false;
            }
            break;

        case STATE_WATI_CORRECT_CURRENT: // 设置禁止休眠

            break;

        case STATE_SAVE_RESULT:

            if (TestResult == failValue) {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");

                pack.itemvalue = QString("|CHARGE_CURRENT:%1").arg(measure_ammeter) +
                    QString("|WIFI_TEST:%1").arg(intwifirssi) +
                    QString("|BLE_TEST:%1").arg(intblerssi) +
                    QString("|BLE_PER_RX:%1").arg(blePerRxMesValue) +
                    QString("|CHAR_TEST:%1").arg(chargestate) + QString("VOL_TEST:%1").arg(voltage) +
                    tupleMesItemvalue_;
                pack.result = "NG";

                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                finishTestRecord(pack, ui->isusemes->checkState());
            } else {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
                pack.result = "PASS";

                pack.itemvalue = QString("|CHARGE_CURRENT:%1").arg(measure_ammeter) +
                    QString("|WIFI_TEST:%1").arg(intwifirssi) +
                    QString("|BLE_TEST:%1").arg(intblerssi) +
                    QString("|BLE_PER_RX:%1").arg(blePerRxMesValue) +
                    QString("|CHAR_TEST:%1|").arg(chargestate) + QString("VOL_TEST:%1|").arg(voltage) +
                    tupleMesItemvalue_;
                pack.sn = ui->getMac->text();

                pack.instruct_num = "079";
                finishTestRecord(pack, ui->isusemes->checkState());
            }

            at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00"); // 发送mac地址
            waitWork(50);
            on_disconnectButton_clicked();

            showlog("测试结束");
            // log->saveRssiDataToCsv(measure_ammeter, intwifirssi, intblerssi, wifiresult,
            //                            bleresult, charageresult, voltageresult);

            state = STATE_IDLE;
            isTestContinue = false;
            ui->macInput->clear();
            ui->snInput->clear();

            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);
            emit send_end_test(getIndex());

            ui->getMac->clear();
            break;
        }
    }

    //  QCoreApplication::processEvents();
}

void wifibletest::on_disconnectwifi_clicked() {
    if (at->getConnected()) {
        protocolManager.set(DeviceCmd::WifiDisconnect);
        showlog("已发送断开wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}
void wifibletest::on_connectwifi_clicked() {
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

void wifibletest::on_macInput_returnPressed() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
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

        ui->start_wifible_test->setEnabled(false);
        // 主状态机流程
        isTestContinue = true;
        state = STATE_IDLE;

        emit send_go_next_focus();
    }
}

void wifibletest::on_pushButton_2_clicked() {
    at->set(DongleCmd::BleLog, 1); // 日志开
}

void wifibletest::on_getMac_returnPressed() {
    testResultTableInit();
    pack.itemvalue.clear();
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
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
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->getMac->text().length()));
        showlog("要求格式为" + snPattern);
        ui->getMac->clear();
        return;
    }
    showlog("正在查询mac地址");
    // const QString parsedMac = parseMacFromSn(ui->getMac->text());
    // if (parsedMac.isEmpty()) {
    //     ui->getMac->setDisabled(0);
    //     ui->macInput->setDisabled(0);
    //     showlog("SN解析MAC失败");
    //     ui->getMac->setFocus();
    //     return;
    // }

    emit send_start_test(getIndex());
    appendStationResult(testItems, "主板条码", "0.0000", passValue);
    testResultTableUpdate(testItems);
    processGetMesTestValue();

    if (ui->isusemes->checkState()) {
        processInspection(ui->getMac->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
        testResultTableUpdate(testItems);
    }
    // on_macInput_returnPressed();
}

void wifibletest::processInspection(QString inputSnText) {
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

void wifibletest::processGetMesTestValue() {
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

void wifibletest::getMacAddress(const QByteArray& byte) {
    receivedData = "";
    receivedData = receivedData + QString::fromUtf8(byte);

    if (receivedData.length() >= 2 && receivedData.right(2) == "\r\n") {
        // 使用正则表达式提取设备信息
        QRegularExpression regex("deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
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
void wifibletest::on_clear_scan_clicked() {
    deviceMap.clear();
    ui->mac_combo->clear();
}
void wifibletest::updateComboBox() {
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
void wifibletest::on_mac_combo_textActivated(const QString& arg1) {
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
        bindingMacSnMes(macAddress, snBinding);
    }
    ui->snbanding->setFocus();
}
void wifibletest::bindingMacSn(QString bindingMac, QString bindingSn) {
    if (bindingSn == "" || bindingMac == "")
        bindingResult = false;
    QString path;
    if (SETTINGS.value("Mes/FACTORY").toString() == "xwd") {
        path = SETTINGS.value("MAC_SN/FilePath", "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt").toString();
        QFileInfo checkPath(path);
        if (checkPath.exists() && checkPath.isDir()) {
            qDebug() << "The network path exists and is a directory.";
        } else {
            path = "mac_sn.txt";
            qDebug() << "The network path does not exist or is not a directory.";
        }
    } else {
        path = "mac_sn.txt";
    }
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

    // QFile file("mac_sn.txt");   // 创建一个文件对象
    // if (file.open(QIODevice::ReadWrite | QIODevice::Append))
    // {                                                    //
    //     QTextStream out(&file);                          // 创建一个文本流对象
    //     out << bindingSn << "," << bindingMac << '\n';   // 将sn和mac写入文件
    //     file.close();                                    // 关闭文件
    // }
}

void wifibletest::bindingMacSnMes(QString bindingMac, QString bindingSn) {
    Q_UNUSED(bindingSn);
    pack.mechines = 1; // 1脱1,1号上位机
    pack.sn = snBinding;

    pack.result = "PASS";

    pack.itemvalue = QString("|BTMAC:%1|").arg(bindingMac);

    pack.instruct_num = "076";

    finishTestRecord(pack, ui->isusemes->checkState());

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
void wifibletest::on_snbanding_returnPressed() {
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

void wifibletest::getTestValue(const int mechines, const QString value) {
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

    // bindingMacSn(mesmacAddress, ui->getMac->text());  //获取测试数据不要绑定测试mac——sn
}
void wifibletest::on_clear_nfc_data_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8] = {0x03, 0x00, 0xFE, 0x00}; // 写入数据缓冲区
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = 100;
    // QStringList parts = ui->NfcComboBox->currentText().split(":");
    // if (parts.size() == 2) {
    //     nfcport = parts[0].toInt();
    // }
    nfcport = findNfcDevicePort(ui->NfcComboBox->currentText());

    showlog("nfc接口为：" + QString::number(nfcport));
    icdev = dc_init(nfcport, 115200);

    if ((intptr_t)icdev <= 0) {
        showlog("Init Com Error!");
        return;
    } else {
        showlog("Init Com OK!");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0) {
        showlog("dc_card_n Error!");

        return;
    } else {
        showlog("dc_card_n Ok!");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog(QString::fromStdString(str1));
    }

    int ret = dc_write(icdev, 4, writedata); // 将写入数据缓冲区中的数据写入设备

    if (ret != 0) {
        QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
        qDebug() << getIndex() << "errMsg: " << writedata << errMsg;
    }

    st = dc_read(icdev, 4, rdata);
    if (st != 0) {
        showlog("dc_read Error!");

        return;
    } else {
        memset(rdatahex, 0x00, sizeof(rdatahex));
        hex_a(rdata, rdatahex, 4);
        std::string str1 = (char*)rdatahex;
        showlog(QString::fromStdString(str1));
    }

    if ((intptr_t)icdev > 0) {
        st = dc_exit(icdev);
        if (st != 0) {
            showlog("dc_exit Error!");
            return;
        } else {
            showlog("dc_exit OK!");
            icdev = (HANDLE)-1;
        }
    }
    return;
}

void wifibletest::on_nfc_write_read_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8]; // 写入数据缓冲区
    ReadNfcData = "";
    // 033BD2023668772001004800324F30450081080027200014178591141035353034303730313233313131313131170102910B0101010A063C842707A8D1
    // 3C842707A8D1
    // 35353034303730313233313131313131
    // 5504070123111111    //55040701华为固定开头sku2311年月日1111数量
    QString hexString;

    static int productionNumber = ui->nfc_count->text().toInt(); // 记录生产数量

    QString text = ui->getMac->text(); // 外部给

    ui->nfc_count->setText(QString::number(productionNumber));

    for (int i = 0; i < text.length(); ++i) {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0'));
    }

    QString nfcdataText =
        nfcdataHeadText + hexString.toUpper() + "170102910B0101010A06" + macAddress.remove(":").toUpper();

    showlog("写入的nfc数据为" + nfcdataText);
    QByteArray dataBytes = QByteArray::fromHex(nfcdataText.toLatin1()); // 将十六进制字符串转换为字节数组
    int dataSize = dataBytes.size();                                    // 获取字节数组的大小
    qDebug() << getIndex() << "dataSize: " << dataSize << nfcdataText;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = 100;
    // QStringList parts = ui->NfcComboBox->currentText().split(":");
    // if (parts.size() == 2) {
    //     nfcport = parts[0].toInt();
    // }
    nfcport = findNfcDevicePort(ui->NfcComboBox->currentText());

    showlog("nfc接口为：" + QString::number(nfcport));
    icdev = dc_init(nfcport, 115200);
    if ((intptr_t)icdev <= 0) {
        showlog("初始化nfc接口失败!");
        TestResult = failValue;

        return;
    } else {
        showlog("初始化nfc接口成功");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0) {
        if (st == 1) {
            showlog("nfc卡识别不到");
            TestItem test;
            test.testItem = "nfc测试";
            test.testData = "nfc卡识别不到";
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);
        }
        if (st < 0)
            showlog("nfc卡查询失败");

        TestResult = failValue;

        return;
    } else {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));

        TestItem test;
        test.testItem = "nfc测试";
        test.testData = "nfc的序列号为" + QString::fromStdString(str1);
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);
    }

    for (int i = 0; i < dataSize; i += 4) {       // 每次处理8个字节
        int bytesToWrite = qMin(8, dataSize - i); // 计算本次需要写入的字节数，最多8个

        memcpy(writedata, dataBytes.constData() + i, bytesToWrite); // 将数据复制到写入数据缓冲区

        int ret = dc_write(icdev, 4 + i / 4, writedata); // 将写入数据缓冲区中的数据写入设备

        if (ret != 0) {
            QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
            showlog(errMsg);

            qDebug() << getIndex() << "errMsg: " << writedata << errMsg;

            TestItem test;
            test.testItem = "nfc测试";
            test.testData = "写入错误: " + errMsg;
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);
        }
    }
    showlog("nfc信息读取内容为：");
    for (int i = 4; i * 4 < dataSize; i += 4) { // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            TestResult = failValue;

            TestItem test;
            test.testItem = "nfc测试";
            test.testData = "nfc信息读取失败";
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            showlog("nfc信息读取失败");
            TestResult = failValue;
            TestItem test;
            test.testItem = "nfc测试";
            test.testData = "nfc信息读取失败";
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            showlog(QString::fromStdString(str1));
        }
    }
    showlog("nfc信息读取结束");
    if (ReadNfcData == nfcdataText) {
        showlog("写入的与读取的比对通过");
        TestItem test;
        test.testItem = "nfc测试";
        test.testData = "读写比对";
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

    } else {
        showlog("写入的与读取的比对失败");
        TestResult = failValue;
        TestItem test;
        test.testItem = "nfc测试";
        test.testData = "读写比对";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);
    }
}

void wifibletest::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void wifibletest::on_disconnectButton_clicked() {
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}

void wifibletest::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void wifibletest::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void wifibletest::on_stopTest_clicked() {
    // at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00");  // 发送mac地址
    waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

void wifibletest::on_nfc_read_clicked() {
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    ReadNfcData = "";
    int dataSize = 61;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = 100;
    // QStringList parts = ui->NfcComboBox->currentText().split(":");
    // if (parts.size() == 2) {
    //     nfcport = parts[0].toInt();
    // }
    nfcport = findNfcDevicePort(ui->NfcComboBox->currentText());

    showlog("nfc接口为：" + QString::number(nfcport));
    icdev = dc_init(nfcport, 115200);
    qDebug() << getIndex() << nfcport;

    if ((intptr_t)icdev <= 0) {
        showlog("初始化nfc接口失败!");
        TestResult = failValue;
        return;
    } else {
        showlog("初始化nfc接口成功");
    }

    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0) {
        if (st == 1)
            showlog("nfc卡识别不到");
        if (st < 0)
            showlog("nfc卡查询失败");
        TestResult = failValue;

        return;
    } else {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 4; i * 4 < dataSize; i += 4) { // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            TestResult = failValue;

            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //   showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            showlog("nfc信息读取失败");
            TestResult = failValue;
            //  showlog("dc_read Error!");

            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //  showlog(QString::fromStdString(str1));
        }
    }

    showlog("nfc信息读取结束");
    showlog("nfc内容为：" + ReadNfcData);
}

int wifibletest::findNfcDevicePort(QString name) {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];
    int k = 100;
    qDebug() << "查询的设备名字为:" << name;
    for (int i = 0; i < 10; i++) {
        k = k + i;
        icdev = dc_init(k, 115200);
        st = dc_srd_eeprom(icdev, 0, 8, buff_1);
        if (st != 0) {
            qDebug() << "nfc烧录器读取失败";
        } else {
            qDebug() << "nfc烧录器读取成功";
            QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
            if (buffStr == name) {
                showlog("找到设备端口号" + QString::number(k));
                return k;
            }
        }
    }
    showlog("没找到设备端口号");
    return 100;
}
//写入
void wifibletest::on_nfc_encode_clicked() {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    int nfcport = 100;
    // QStringList parts = ui->NfcComboBox->currentText().split(":");
    // if (parts.size() == 2) {
    //     nfcport = parts[0].toInt();
    // }
    nfcport = findNfcDevicePort(ui->NfcComboBox->currentText());

    showlog("nfc接口为：" + QString::number(nfcport));
    icdev = dc_init(nfcport, 115200);
    st = dc_swr_eeprom(icdev, 0, 8, (unsigned char*)"READER_A");
    if (st != 0) {
        showlog("nfc烧录器写入失败");
    } else {
        showlog("nfc烧录器写入成功");
    }
}

//读取
void wifibletest::on_nfc_decode_clicked() {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];
    int nfcport = 100;
    // QStringList parts = ui->NfcComboBox->currentText().split(":");
    // if (parts.size() == 2) {
    //     nfcport = parts[0].toInt();
    // }

    nfcport = findNfcDevicePort(ui->NfcComboBox->currentText());

    showlog("nfc接口为：" + QString::number(nfcport));
    icdev = dc_init(nfcport, 115200);
    st = dc_srd_eeprom(icdev, 0, 8, buff_1);
    if (st != 0) {
        showlog("nfc烧录器读取失败");
    } else {
        showlog("nfc烧录器读取成功");
        // 将 buff_1 数组转换为 QString
        QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
        // 输出整个字符串
        qDebug() << "nfc设备为:" << buffStr;
    }
}

void wifibletest::on_nfcComFresh_clicked() {
    updateHIDComboBox(getNfcComboBox());
}

void wifibletest::on_get_battery_clicked() {
    if (at->getConnected()) {
        protocolManager.get(DeviceCmd::GetBattery);
        showlog("正在获取设备电量");
    } else {
        showlog("请等待连接设备后再试");
    }
}

void wifibletest::on_usbconnectButton_clicked() {
    loadWifiBleCmw100Config();
    if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed().isEmpty()) {
        showlog(QStringLiteral("CMW100 VISA地址未配置：请配置 BlePer/CmwVisaAddress"));
        return;
    }

    QString cmd = SETTINGS.value(QStringLiteral("BlePer/CmwDebugQueryCmd"), QStringLiteral("SOURCe:GPRF:GEN:STAT OFF;*OPC?")).toString().trimmed();
    if (cmd.isEmpty()) {
        cmd = QStringLiteral("*IDN?");
    }

    loadWifiBleCmw100Config();
    QString response;
    const bool ok = scpiVisaManager() && scpiVisaManager()->exec(CmwScpiCmd::QueryLine, cmd);
    if (ok) {
        response = scpiVisaManager()->lastQueryResponse();
    }
    if (!ok) {
        showlog(QStringLiteral("CMW100 指令交互失败: %1").arg(cmd));
        return;
    }

    showlog(QStringLiteral("CMW100 VISA已连接: %1")
                .arg(scpiVisaManager() ? scpiVisaManager()->visaConfig().visaAddress : QString()));
    showlog(QStringLiteral("CMW100 指令 %1 返回: %2").arg(cmd, response));
}
