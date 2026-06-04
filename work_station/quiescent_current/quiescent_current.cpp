#include "quiescent_current.h"

#include "ui_quiescent_current.h"
#include <QMessageBox>
#include <QTimer>
#include <QVector>

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
}
quiescent_current::quiescent_current(int index, QWidget* parent) :
    test_base(parent), ui(new Ui::quiescent_current), basicInfoModel(new TestModel), peripheralModel(new TestModel) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = QC_VER;

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
        state = STATE_SLEEP_CURRENT_TEST;
        qDebug() << getIndex() << "计时结束，进入电流测试" << QDateTime::currentDateTime();
    });
    connect(usblogwaittime, &QTimer::timeout, [=]() {
        at->get(DongleCmd::GetGmac);
        showlog("正在定时器复位设备");
    });
    connect(visa, &Qvisa::programmablePowerCurrentRead, this, &quiescent_current::refreshProgrammablePowerCurrent);

    HighCurrent = SETTINGS.value("Current/HighstaticCurrent").toDouble();
    LowCurrent = SETTINGS.value("Current/LowstaticCurrent").toDouble();

    measure_wait_time = SETTINGS.value("Current/measure_wait_time").toInt();

    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);
    showlog("model=" + pack.model);

    showlog("machineNo=" + pack.machineNo);
    showlog("HighCurrent=" + QString::number(HighCurrent));
    showlog("LowCurrent=" + QString::number(LowCurrent));
    showlog("measure_wait_time=" + QString::number(measure_wait_time));
    applyCurrentProtocolConfig();

    if (pack.factory == "hq" || pack.factory == "jj") {
        ui->jigComNameCombo->setEnabled(false);
    }

    if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
        ui->productComNameCombo->setEnabled(true);
    }else {
        ui->productComNameCombo->setEnabled(false);
    }

    // if (QString(pack.product).compare("P20PS") == 0) {
    //     productBaudRate = 2000000;
    // } else {
    //     productBaudRate = 1000000;
    // }
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}

void quiescent_current::applyCurrentProtocolConfig() {
    Qusb::ProtocolConfig cfg;
    cfg.protocol = protocolTypeFromSetting("auto");
    cfg.luxshareMachineId = getIndex();
    cfg.scpiCurrentType = SETTINGS.value("Current/ScpiCurrentType", "CURR").toString();
    cfg.scpiCurrentMode = SETTINGS.value("Current/ScpiCurrentMode", "DC").toString();
    cfg.scpiRange = SETTINGS.value("Current/ScpiRange", "500e-3").toString();
    syncVisaPowerUiFromSettings();

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

    currentProtocolType = cfg.protocol;
    useProgrammablePower = SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool();

    usb->setProtocolConfig(cfg);

    showlog("静态电流协议=" + SETTINGS.value("Current/ProtocolType", "auto").toString() +
            " 实际生效协议=" + QString::number(static_cast<int>(currentProtocolType)));
    showlog("静态电流配置: machineId=" + QString::number(cfg.luxshareMachineId) +
            ", scpi=" + cfg.scpiCurrentType + ":" + cfg.scpiCurrentMode + " " + cfg.scpiRange +
            ", VISA配置=" + QString(SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool() ? "ON" : "OFF") +
            ", 地址=" + SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString());
    {
        showlog("程控电源开关=" + QString(useProgrammablePower ? "ON" : "OFF") +
                ", VISA=" + SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString() +
                ", V=" + QString::number(SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), 12.0).toDouble(), 'f', 3) +
                ", A=" + QString::number(SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), 2.5).toDouble(), 'f', 3));
    }
}

void quiescent_current::syncVisaPowerUiFromSettings() {
    ui->currentAmmeterUseVisaCheckBox->setChecked(
        SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), true).toBool());
    ui->currentAmmeterVisaAddressEdit->setText(
        SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("ASRL10::INSTR")).toString().trimmed());
}

bool quiescent_current::setProgrammablePowerOutput(bool enable) {
    if (!useProgrammablePower) {
        showlog(QStringLiteral("程控电源未启用，跳过输出%1")
                    .arg(enable ? QStringLiteral("打开") : QStringLiteral("关闭")));
        return true;
    }
    if (!visa) {
        return false;
    }
    const VisaCmd cmd = enable ? VisaCmd::PowerOutputOn : VisaCmd::PowerOutputOff;
    bool ok = visa->set(cmd);
    if (!ok) {
        showlog(QStringLiteral("程控电源输出%1失败，重连VISA后重试")
                    .arg(enable ? QStringLiteral("打开") : QStringLiteral("关闭")));
        resetVisaBackend();
        ok = visa->set(cmd);
    }
    showlog(QStringLiteral("程控电源输出%1=%2")
                .arg(enable ? QStringLiteral("打开") : QStringLiteral("关闭"))
                .arg(ok ? QStringLiteral("OK") : QStringLiteral("NG")));
    return ok;
}

void quiescent_current::refreshProgrammablePowerCurrent(double valueAmps, bool ok) {
    programmablePowerMeasuredCurrentA_ = valueAmps;
    programmablePowerCurrentReadOk_ = ok;
    if (ok) {
        measure_ammeter = valueAmps * 1000.0;
        showlog(QStringLiteral("程控电源电流回读：%1 mA").arg(measure_ammeter, 0, 'f', 4));
    } else {
        showlog(QStringLiteral("程控电源电流回读失败或无法解析"));
    }
}

void quiescent_current::disconnect_dongle() { on_disconnectButton_clicked(); }

void quiescent_current::refreshMusicState(ProtocolMusicStateData data) {
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

void quiescent_current::refreshChargeCurrentRead(ProtocolUInt32ValueData data) {
    if (state != STATE_SLEEP_CURRENT_TEST) {
        return;
    }

    const double currentMa = static_cast<double>(data.value);
    const double lowCharCurrent = SETTINGS.value("Current/LowCharCurrent").toDouble();
    const double highCharCurrent = SETTINGS.value("Current/HighCharCurrent").toDouble();
    const bool pass = currentMa >= lowCharCurrent && currentMa <= highCharCurrent;
    const QString resultText = pass ? passValue : failValue;

    showlog(QString("充电电流%1，当前=%2ma，范围=%3~%4ma")
                .arg(pass ? QStringLiteral("通过") : QStringLiteral("失败"))
                .arg(currentMa, 0, 'f', 0)
                .arg(lowCharCurrent)
                .arg(highCharCurrent));

    TestItem test;
    test.testItem = "充电电流(ma)";
    test.testData = QString::number(currentMa, 'f', 0);
    test.testResult = resultText;
    test.ask = QString("%1~%2").arg(lowCharCurrent).arg(highCharCurrent);
    testItems.append(test);
    testResultTableUpdate(testItems);

    pack.itemvalue += QStringLiteral("|CHARGE_CURRENT:%1:%2:%3::ma:%4")
                          .arg(currentMa, 0, 'f', 0)
                          .arg(highCharCurrent)
                          .arg(lowCharCurrent)
                          .arg(pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"));

    totalresult = pass ? passValue : failValue;
    state = STATE_SAVE_RESULT;
}

void quiescent_current::refreshBaseData(ProtocolBaseInfoData data) {
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

  
        QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();


        // bool isProductTest = SETTINGS.value("ProductInfo/ProductName_checkBox").toBool();
        // bool isHwTest = SETTINGS.value("ProductInfo/HardwareVersion_checkBox").toBool();
        bool isSoftwareTest = SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool();
        // bool isResourceTest = SETTINGS.value("ProductInfo/ResourceVersion_checkBox").toBool();
        // bool isMotorTest = SETTINGS.value("ProductInfo/MotorVersion_checkBox").toBool();
        // bool isAppProtocolTest = SETTINGS.value("ProductInfo/AppPB_checkBox").toBool();
        // bool isFactoryProtocolTest = SETTINGS.value("ProductInfo/FactoryPB_checkBox").toBool();
        // bool isAlgoTest = SETTINGS.value("ProductInfo/AlgorithmVersion_checkBox").toBool();
        // bool isPresureTest = SETTINGS.value("ProductInfo/PressureVersion_checkBox").toBool();
        // bool isFSensorTest = SETTINGS.value("ProductInfo/FSensorVersion_checkBox").toBool();
        // bool isImuTest = SETTINGS.value("ProductInfo/ImuID_checkBox").toBool();
        // bool isCameraTest = SETTINGS.value("ProductInfo/CameraID_checkBox").toBool();
        // bool isBleTest = SETTINGS.value("ProductInfo/BluetoothVersion_checkBox").toBool();

        if ((!isSoftwareTest || compareVersions(softwareVersion, data.soft_version))) {
            // (!isHwTest || compareVersions(hardwareVersion, data.hw_version)) &&
            // (!isPresureTest || compareVersions(pressureSenseVersion, data.presure_version)) &&
            // (!isFSensorTest || compareVersions(fsensorVersion, data.fsensor_version)) &&
            // (!isProductTest || compareVersions(productName, data.product_name)) &&
            // (!isAppProtocolTest || compareVersions(appProtocolVersion, QString::number(data.pb_phone_ver))) &&
            // (!isFactoryProtocolTest || compareVersions(factoryProtocolVersion, QString::number(data.pb_factory_ver))) &&
            // (!isAlgoTest || compareVersions(algorithmVersion, data.algo_version)) &&
            // (!isResourceTest || compareVersions(resourceVersion, data.res_version)) &&
            // (!isMotorTest || compareVersions(motorVersion, data.motor_version)) &&
            // (!isImuTest || compareVersions(imuId, QString::number(data.imu_id))) &&
            // (!isBleTest || compareVersions(bleVersion, data.ble_version)) &&
            // (!isCameraTest || compareVersions(camera_id, data.camera_version))) {
            pack.itemvalue += QStringLiteral("|softwareVersion:%1:::%2:").arg(pumpsoft_version).arg(softwareVersion);
            base_state = 1;
        } else {
            base_state = 2;
            pack.itemvalue += QStringLiteral("|softwareVersion:%1:::%2:").arg(pumpsoft_version).arg(softwareVersion);
        }

        TestItem test;

        // Check for BLE version
        // if (isBleTest) {
        //     test.testItem = "蓝牙版本";
        //     test.testData = data.ble_version;
        //     test.ask = bleVersion;
        //     testItems.append(test);
        // }

        // // Check for Motor version
        // if (isMotorTest) {
        //     test.testItem = "电机版本";
        //     test.testData = data.motor_version;
        //     test.ask = motorVersion;
        //     testItems.append(test);
        // }

        // // Check for Resource version
        // if (isResourceTest) {
        //     test.testItem = "资源版本";
        //     test.testData = data.res_version;
        //     test.ask = resourceVersion;
        //     testItems.append(test);
        // }

        // Check for Software version
        if (isSoftwareTest) {
            test.testItem = "软件版本";
            test.testData = data.soft_version;
            test.ask = softwareVersion;
            testItems.append(test);
        }

        // // Check for Product name
        // if (isProductTest) {
        //     test.testItem = "产品名字";
        //     test.testData = data.product_name;
        //     test.ask = productName;
        //     testItems.append(test);
        // }

        // // Check for Hardware version
        // if (isHwTest) {
        //     test.testItem = "硬件版本";
        //     test.testData = data.hw_version;
        //     test.ask = hardwareVersion;
        //     testItems.append(test);
        // }

        // // Check for Algorithm version
        // if (isAlgoTest) {
        //     test.testItem = "算法版本号";
        //     test.testData = data.algo_version;
        //     test.ask = algorithmVersion;
        //     testItems.append(test);
        // }

        // // Check for Pressure version
        // if (isPresureTest) {
        //     test.testItem = "压感版本";
        //     test.testData = data.presure_version;
        //     test.ask = pressureSenseVersion;
        //     testItems.append(test);
        // }

        // // Check for FSensor version
        // if (isFSensorTest) {
        //     test.testItem = "电机压感版本";
        //     test.testData = data.fsensor_version;
        //     test.ask = fsensorVersion;
        //     testItems.append(test);
        // }

        // // Check for IMU ID
        // if (isImuTest) {
        //     test.testItem = "六轴id";
        //     test.testData = QString::number(data.imu_id);
        //     test.ask = imuId;
        //     testItems.append(test);
        // }

        // // Check for APP Protocol version
        // if (isAppProtocolTest) {
        //     test.testItem = "APP的pb版本";
        //     test.testData = QString("%1").arg(data.pb_phone_ver);
        //     test.ask = appProtocolVersion;
        //     testItems.append(test);
        // }

        // // Check for Factory Protocol version
        // if (isFactoryProtocolTest) {
        //     test.testItem = "工厂的pb版本";
        //     test.testData = QString("%1").arg(data.pb_factory_ver);
        //     test.ask = factoryProtocolVersion;
        //     testItems.append(test);
        // }

        // // Check for Camera ID
        // if (isCameraTest) {
        //     test.testItem = "摄像头id";
        //     test.testData = data.camera_version;
        //     test.ask = camera_id;
        //     testItems.append(test);
        // }

        updateTestData(testItems);
    }
}

void quiescent_current::refreshPeriphData(ProtocolPeriphStateData data) {
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
            pack.itemvalue += QStringLiteral("|audio_state:%1:::%2:").arg(data.audio_state).arg(audioState);
        }

        if (SETTINGS.value("PeripheralStatus/FlashStatus_checkBox").toBool()) {
            test.testItem = "内存状态";
            test.testData = QString::number(data.flash_state);
            test.ask = flashStatus;
            testItems.append(test);
            pack.itemvalue += QStringLiteral("|flash_state:%1:::%2:").arg(data.flash_state).arg(flashStatus);
        }

        if (SETTINGS.value("PeripheralStatus/IMUStatus_checkBox").toBool()) {
            test.testItem = "imu状态";
            test.testData = QString::number(data.imu_state);
            test.ask = imuStatus;
            testItems.append(test);
            pack.itemvalue += QStringLiteral("|imu_state:%1:::%2:").arg(data.imu_state).arg(imuStatus);
        }

        if (SETTINGS.value("PeripheralStatus/MagneticStatus_checkBox").toBool()) {
            if (SETTINGS.value("SYSTEM/MagneticReuseMotorStatus").toBool())
                test.testItem = "马达状态";
            else
                test.testItem = "地磁状态";
            test.testData = QString::number(data.magnet_state);
            test.ask = magneticStatus;
            testItems.append(test);
            pack.itemvalue += QStringLiteral("|magnet_state:%1:::%2:").arg(data.magnet_state).arg(magneticStatus);
        }

        if (SETTINGS.value("PeripheralStatus/PressureStatus_checkBox").toBool()) {
            test.testItem = "压感状态";
            test.testData = QString::number(data.press_state);
            test.ask = pressureStatus;
            testItems.append(test);
            pack.itemvalue += QStringLiteral("|press_state:%1:::%2:").arg(data.press_state).arg(pressureStatus);
        }

        if (SETTINGS.value("PeripheralStatus/BatteryIcStatus_checkBox").toBool()) {
            test.testItem = "电池ic状态";
            test.testData = QString::number(data.battery_ic_state);
            test.ask = batteryIcStatus;
            testItems.append(test);
            pack.itemvalue += QStringLiteral("|battery_ic_state:%1:::%2:").arg(data.battery_ic_state).arg(batteryIcStatus);
        }

        if (SETTINGS.value("PeripheralStatus/TouchIcStatus_checkBox").toBool()) {
            test.testItem = "触摸ic状态";
            test.testData = QString::number(data.touch_ic_state);
            test.ask = touchIcStatus;
            testItems.append(test);
            pack.itemvalue += QStringLiteral("|touch_ic_state:%1:::%2:").arg(data.touch_ic_state).arg(touchIcStatus);
        }

        updateTestData(testItems);
    }
}

void quiescent_current::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到电流数据" << data;
    double normalValue = 0;
    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    if (currentProtocolType == Qusb::ProtocolType::LxModbus)
        normalValue = data.toDouble(&conversionOk) / 100;
    else if (currentProtocolType == Qusb::ProtocolType::HqModbus)
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

void quiescent_current::getTestValue(const int mechines, const QString value) {
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

        sn = snFromMes.toUtf8();
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

        sn = snFromMes.toUtf8();
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

    // bandingMacSn(mesmacAddress, ui->snInput->text());//获取测试数据不要绑定测试
}

quiescent_current::~quiescent_current() {
    qDebug() << getIndex() << "已进入析构";
    isTestContinue = 0;
    closeDongleSerialPort();
    closeUsbSerialPort();
    closeJigSerialPort();

    delete ui;
}

void quiescent_current::refreshSn(ProtocolSnData data) {

    tail_sn_string = data.value;
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

void quiescent_current::on_snInput_returnPressed() {
    clearDisplay();
    pack.itemvalue.clear();
    macAddress = "没有mac地址";
    logString = "";
    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    applyAdaptiveV3ProductBySn(ui->snInput);

    QRegularExpression snRegex(snPattern);
    if (!snRegex.match(ui->snInput->text()).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->snInput->text().length()));
        showlog("要求格式为" + snPattern);
        ui->snInput->clear();
        return;
    }

    emit send_startTest(getIndex());
    // stringsn = ui->snInput->text();
    // sn = ui->snInput->text().toUtf8();
    ui->snInput->setDisabled(1);

    // 新流程：SN校验后先解析MAC
    appendStationResult(testItems, "主板条码", "0.0000", passValue);
    testResultTableUpdate(testItems);
    // BYD：过程码换 SN（GetSfcKeyBySfc）——勾选「表单MES」或「启用MES」且工厂为 byd 时均下发
    qDebug() << "snInput->text():" << ui->snInput->text();
    processGetMesTestValue();
    // if (parsedMac.isEmpty()) {
    //     showlog("从SN解析MAC失败（预留规则待补）");
    //     on_stopTest_clicked();
    //     return;
    // }
    if (ui->isusemes->checkState()) {
        processInspection(ui->snInput->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
    // startFlowWithMac(parsedMac);
}
void quiescent_current::on_macInput_returnPressed() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    waitWork(WAITTIME);
    // 检查是否是mac格式
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        qDebug() << getIndex() << macAddress;
        // 主状态机流程
        isTestContinue = true;
        emit send_go_next_focus();
        state = STATE_IDLE;
    }
}

void quiescent_current::processGetMesTestValue() {
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

void quiescent_current::clearDisplay() {
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
void quiescent_current::refreshBleState(int state) {
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

void quiescent_current::refreshProductUartState(int state) {
    if (state)
        showlog("product串口连接成功");
    else {
        ui->productComNameCombo->setEnabled(true);
        showlog("product串口连接断开");
    }
}

void quiescent_current::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void quiescent_current::refreshJigUartState(int state) {
    if (state)
        showlog("治具串口连接成功");
    else {
        ui->jigComNameCombo->setEnabled(true);
        showlog("治具串口连接断开");
    }
}

void quiescent_current::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        showlog("万用表 USB 串口已断开");
    }
}
void quiescent_current::on_connectButton_clicked() {
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void quiescent_current::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}
void quiescent_current::on_usbconnectButton_clicked() {
    applyCurrentProtocolConfig();
    showlog(QStringLiteral("VISA 已切换为 VisaPower 配置，地址=%1")
                .arg(ui->currentAmmeterVisaAddressEdit->text().trimmed()));
}

void quiescent_current::on_usbdisconnectButton_clicked() {
    closeUsbSerialPort();
}

void quiescent_current::processInspection(QString stringsn) {
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



void quiescent_current::startFlowWithMac(const QString& mac) {
    const bool simulateFlow = SETTINGS.value("SYSTEM/DebugSimulateFlow", false).toBool();
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

void quiescent_current::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
            case STATE_IDLE: {  // 复位一切
                if (useProgrammablePower) {
                    applyCurrentProtocolConfig();
                    if (!SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool()
                        || SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString().trimmed().isEmpty()) {
                        showlog(QStringLiteral("程控电源 VISA 未启用或地址为空"));
                        pack.itemvalue = "power_visa_config=NG";
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                        break;
                    }
                    showlog(QStringLiteral("开始配置程控电源：address=%1")
                                .arg(SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString()));
                    bool powerCfgOk = true;
                    if (!visa) {
                        powerCfgOk = false;
                    } else {
                        powerCfgOk = visa->set(VisaCmd::ConfigurePowerSupply);
                    }
                    const bool outOk = setProgrammablePowerOutput(true);
                    showlog(QString("程控电源配置=%1, 输出打开=%2")
                                .arg(powerCfgOk ? "OK" : "NG")
                                .arg(outOk ? "OK" : "NG"));
                    if (!powerCfgOk || !outOk) {
                        pack.itemvalue = "power_on=NG";
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                        break;
                    }
                    if (outOk) {
                        auto* prompt = new QMessageBox(QMessageBox::Information,
                                                       QStringLiteral("操作提示"),
                                                       QStringLiteral("请按产品电源键开机，5秒后自动继续连接"),
                                                       QMessageBox::NoButton,
                                                       this);
                        prompt->setAttribute(Qt::WA_DeleteOnClose);
                        prompt->show();
                        QTimer::singleShot(5000, prompt, &QMessageBox::accept);
                    }
                }

                protocolManager.resetAllPb();
                periph_state = 0;
                base_state = 0;
                isovertime = 0;
                refresh_base_times = 1;
                refresh_periph_times = 1;
                totalresult = "";
                at->resetConnected();
                measure_ammeter = 0;
                waitWork(5000);
                at->set(DongleCmd::BleScanConnect, ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());
                showlog("已经发送mac地址");
                TestTime.start();
                state = STATE_SET_TEST_MODE;

                break;
            }

            case STATE_SET_TEST_MODE:
                if (at->getConnected()) {
                    showlog("设置工厂模式");
                    sendCommandWithRetry([&]() { 
                        protocolManager.set(DeviceCmd::FacMode, 1);
                    });
                    waitWork(300);
                    state = STATE_VERIFY_TEST_MODE;
                }
                break;

            case STATE_VERIFY_TEST_MODE:
                if (canGoNext) {
                    showlog("已进入工厂模式");
                    appendStationResult(testItems, "进入工厂模式", "0.0000", passValue);
                    testResultTableUpdate(testItems);
                    sendCommandWithRetry([&]() { 
                        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn})); 
                    });
                    state = STATE_BANDING;
                break;

            case STATE_BANDING: {
                if (canGoNext) {
                    if (snCompareOk == 1) {
                        state = STATE_WATI_GET_BASE_STATE;                        
                        showlog("sn已比对成功");
                        appendStationResult(testItems, "sn写入校验", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        pack.itemvalue += QStringLiteral("|SN_BANDING:%1:::%2:").arg(tail_sn_string).arg(stringsn);
                        sendCommandWithRetry([&]() { 
                            protocolManager.get(DeviceCmd::BaseInfo);
                        });
                        
                    } else if (snCompareOk == 2) {
                        showlog("sn已比对失败"); 
                        // pack.error="SP03011";
                        result = failValue;
                        pack.itemvalue += QStringLiteral("|SN_BANDING:%1:::%2:").arg(tail_sn_string).arg(stringsn);
                        state = STATE_SAVE_RESULT;
                    } else {
                    waitWork(500);
                    protocolManager.get(DeviceCmd::Sn, static_cast<int>(FacDevInfoType_TAIL_SN));
                    showlog("已发送sn绑定");
                    }
                }
                break;
            }

            case STATE_WATI_GET_BASE_STATE:
                if (base_state == 1)  // 基础信息正常
                {
                    waitWork(WAITTIME);
                    showlog("固件版本验证通过");
                    sendCommandWithRetry([&]() { 
                        protocolManager.get(DeviceCmd::PeriphState);
                    });
                    state = STATE_WATI_GET_PERIPHERAL_STATE;
                }
                else if (base_state == 2) {
                    waitWork(WAITTIME);
                    showlog("固件版本验证失败");
                    protocolManager.get(DeviceCmd::PeriphState);
                    totalresult = failValue;
                    state = STATE_SAVE_RESULT;
                }
                else {
                    waitWork(500);
                    protocolManager.get(DeviceCmd::BaseInfo);
                    showlog("正在重发获取固件版本");
                }
                break;

            case STATE_WATI_GET_PERIPHERAL_STATE:
                if (periph_state == 1)  // 设备信息正常
                {
                    showlog("外设状态正常");
                    const QString mesProductName = SETTINGS.value("MES/Product_Name").toString().trimmed();
                    if (mesProductName.compare(QStringLiteral("V3Pro"), Qt::CaseInsensitive) == 0) {
                        showlog(QStringLiteral("V3Pro产品，读取充电电流"));
                        sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::ChargeCurrentRead); });
                        state = STATE_SLEEP_CURRENT_TEST;
                    } else {
                        totalresult = passValue;
                        state = STATE_SAVE_RESULT;
                    }

                } else if (periph_state == 2)  // 设备信息异常
                {
                    showlog("外设状态异常");
                    totalresult = failValue;
                    state = STATE_SAVE_RESULT;
                }
                else {
                    waitWork(500);
                    protocolManager.get(DeviceCmd::PeriphState);
                    showlog("正在重发获取外设信息");
                }
                break;

            case STATE_SLEEP_CURRENT_TEST:
                if (sendRetryOver) {
                    sendRetryOver = false;
                    showlog(QStringLiteral("读取充电电流超时"));
                    appendStationResult(testItems, "充电电流(ma)", "读取超时", failValue);
                    testResultTableUpdate(testItems);
                    pack.itemvalue += QStringLiteral("|CHARGE_CURRENT:TIMEOUT::::ma:FAIL");
                    totalresult = failValue;
                    state = STATE_SAVE_RESULT;
                }
                break;



            case STATE_SAVE_RESULT:
                stringsn = "";
                setProgrammablePowerOutput(false);
                if (totalresult == passValue) {
                    pack.result = "PASS";
                    pack.sn = ui->snInput->text();
                    pack.instruct_num = "076";
                    pack.itemvalue += QStringLiteral("|STATIC_CURRENT_TEST:PASS");
                    if (ui->isusemes->checkState()) {
                        pack.elapseTime = static_cast<double>(TestTime.elapsed()) / 1000.0;
                        emit send_end_testPass(pack);
                        appendStationResult(testItems, "MES完成上报", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                    }

                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((totalresult == failValue)) {
                    pack.result = "FAIL";
                    pack.sn = ui->snInput->text();
                    pack.instruct_num = "076";
                    pack.itemvalue += QStringLiteral("|STATIC_CURRENT_TEST:FAIL");
                    if (ui->isusemes->checkState()) {
                        pack.elapseTime = static_cast<double>(TestTime.elapsed()) / 1000.0;
                        emit send_end_testPass(pack);
                        appendStationResult(testItems, "MES完成上报", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                    }

                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }

                showlog("测试结束，sn为：" + ui->snInput->text());
                ui->macInput->clear();
                ui->snInput->clear();

                isTestContinue = false;  // 结束

                if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                    SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                } else {
                    if (pack.factory == "xwd")
                        jig->set_cylinder_state(0, getIndex());
                }

                on_disconnectButton_clicked();
                on_usbdisconnectButton_clicked();
                ui->snInput->setDisabled(0);
                ui->macInput->setDisabled(1);
                ui->getMac->setDisabled(0);
                emit send_end_test(getIndex());

                state = STATE_IDLE;
                break;
        }
        //   QCoreApplication::processEvents();
    }
    }
}
void quiescent_current::on_pushButton_clicked() {
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

void quiescent_current::on_pushButton_3_clicked() {
    if (useProgrammablePower) {
        if (visa) {
            visa->get(VisaCmd::ReadCurrent);
        }
    } else {
        usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement);
    }

    // at->get(DongleCmd::GetGmac);
    // MesInit();
}
void quiescent_current::processReceivedData(const QByteArray& data) {
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

void quiescent_current::on_pushButton_4_clicked() {
    static int clickStep = 1;  // 用于跟踪当前运行的步骤
    pack.mechines = 1;
    pack.sn = "U03000077I1H00007D";

    pack.result = "PASS";
    pack.line = "1C3A04";
    pack.itemvalue = QString("|BTMAC:3C:84:27:07:A8:D2|") + QString("CURRENT:0.24|");

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

void quiescent_current::bandingMacSn(QString bandingmac, QString bandingsn) {
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

void quiescent_current::on_stopTest_clicked() {
    showlog("触发停止测试");
    setProgrammablePowerOutput(false);
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






void quiescent_current::on_visa_test_clicked()
{
    applyCurrentProtocolConfig();

    if (!SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool()
        || SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString().trimmed().isEmpty()) {
        showlog(QStringLiteral("程控电源 VISA 未启用或地址为空"));
        return;
    }
    if (!visa) {
        showlog(QStringLiteral("程控电源 ConfigurePowerSupply 失败"));
        return;
    }
    if (!visa->set(VisaCmd::ConfigurePowerSupply)) {
        showlog(QStringLiteral("程控电源 ConfigurePowerSupply 失败"));
        return;
    }

    const bool outOnOk = setProgrammablePowerOutput(true);
    if (!outOnOk) {
        showlog(QStringLiteral("程控电源输出打开失败"));
        return;
    }
    waitWork(SETTINGS.value(QStringLiteral("Current/PowerOnWaitMs"), 20000).toInt());
    const bool outOffOk = setProgrammablePowerOutput(false);

    showlog(QStringLiteral("程控电源已按配置上电：电压=%1V，限流=%2A，链路=%3")
                    .arg(SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), 12.0).toDouble(), 0, 'f', 2)
                    .arg(SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), 2.5).toDouble(), 0, 'f', 3)
                    .arg(QStringLiteral("VISA:%1")
                             .arg(SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString())));
    showlog(QStringLiteral("程控电源输出关闭=%1").arg(outOffOk ? QStringLiteral("OK") : QStringLiteral("NG")));
}

void quiescent_current::on_currentAmmeterVisaApplyButton_clicked()
{
    SETTINGS.setValue(QStringLiteral("VisaPower/ScpiUseVisa"), ui->currentAmmeterUseVisaCheckBox->isChecked());
    SETTINGS.setValue(QStringLiteral("VisaPower/VisaAddress"), ui->currentAmmeterVisaAddressEdit->text().trimmed());
    closeUsbSerialPort();
    applyCurrentProtocolConfig();
    showlog(QStringLiteral("已保存 VISA 配置（VisaPower），useVisa=%1，地址=%2")
                .arg(ui->currentAmmeterUseVisaCheckBox->isChecked() ? 1 : 0)
                .arg(ui->currentAmmeterVisaAddressEdit->text().trimmed()));
}

