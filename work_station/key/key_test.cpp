#include "key_test.h"

#include "ui_key_test.h"
#include <QMessageBox>
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
        at->ask_mac();
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
    }

    else {
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
    cfg.protocol = protocolTypeFromSetting(SETTINGS.value("Current/ProtocolType", "auto").toString());
    cfg.luxshareMachineId = SETTINGS.value("Current/LxMachineId", getIndex()).toInt();
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
        refresh_key_times = 0;
        showlog("获取到按键上报");
        TestItem test;
        bool isKeyIdPowerTest = SETTINGS.value("ProductInfo/KeyIdPower_checkBox").toBool();
        bool isKeyIdStartPauseTest = SETTINGS.value("ProductInfo/KeyIdStartPause_checkBox").toBool();
        bool isKeyIdModeTest = SETTINGS.value("ProductInfo/KeyIdMode_checkBox").toBool();
        bool isKeyIdSpeedTest = SETTINGS.value("ProductInfo/KeyIdSpeed_checkBox").toBool();
        bool isKeyIdProgramTest = SETTINGS.value("ProductInfo/KeyIdProgram_checkBox").toBool();
        bool isKeyIdLeftTest = SETTINGS.value("ProductInfo/KeyIdLeft_checkBox").toBool();
        bool isKeyIdRightTest = SETTINGS.value("ProductInfo/KeyIdRight_checkBox").toBool();
        bool isKeyIdLeftRotateTest = SETTINGS.value("ProductInfo/KeyIdLeftRotate_checkBox").toBool();
        bool isKeyIdRightRotateTest = SETTINGS.value("ProductInfo/KeyIdRightRotate_checkBox").toBool();

        QString keyIdPower = SETTINGS.value("ProductInfo/KeyIdPower").toString();
        QString keyIdStartPause = SETTINGS.value("ProductInfo/KeyIdStartPause").toString();
        QString keyIdMode = SETTINGS.value("ProductInfo/KeyIdMode").toString();
        QString keyIdSpeed = SETTINGS.value("ProductInfo/KeyIdSpeed").toString();
        QString keyIdProgram = SETTINGS.value("ProductInfo/KeyIdProgram").toString();
        QString keyIdLeft = SETTINGS.value("ProductInfo/KeyIdLeft").toString();
        QString keyIdRight = SETTINGS.value("ProductInfo/KeyIdRight").toString();
        QString keyIdLeftRotate = SETTINGS.value("ProductInfo/KeyIdLeftRotate").toString();
        QString keyIdRightRotate = SETTINGS.value("ProductInfo/KeyIdRightRotate").toString();


        if (isKeyIdPowerTest && state == STATE_WAIT_GET_KEY_POWER_STATE) {
            if (compareVersions(keyIdPower, QString::number(x.keyButtonId))) {
                KeyPowerState = 1;
            } else {
                KeyPowerState = 2;
            }
        }
        if (isKeyIdStartPauseTest && state == STATE_WAIT_GET_KEY_STARTPAUSE_STATE) {
            if (compareVersions(keyIdStartPause, QString::number(x.keyButtonId))) {
                KeyStartPauseState = 1;
            } else {
                KeyStartPauseState = 2;
            }
        }
        if (isKeyIdModeTest && state == STATE_WAIT_GET_KEY_MODE_STATE) {
            if (compareVersions(keyIdMode, QString::number(x.keyButtonId))) {
                KeyModeState = 1;
            } else {
                KeyModeState = 2;
            }   
        }
        if (isKeyIdSpeedTest && state == STATE_WAIT_GET_KEY_SPEED_STATE) {
            if (compareVersions(keyIdSpeed, QString::number(x.keyButtonId))) {
                KeySpeedState = 1;
            } else {
                KeySpeedState = 2;
            }
        }
        if (isKeyIdProgramTest && state == STATE_WAIT_GET_KEY_PROGRAM_STATE) {
            if (compareVersions(keyIdProgram, QString::number(x.keyButtonId))) {
                KeyProgramState = 1;
            } else {
                KeyProgramState = 2;
            }
        }
        if (isKeyIdLeftTest && state == STATE_WAIT_GET_KEY_LEFT_STATE) {
            if (compareVersions(keyIdLeft, QString::number(x.keyButtonId))) {
                KeyLeftState = 1;
            } else {
                KeyLeftState = 2;
            }
        }
        if (isKeyIdRightTest && state == STATE_WAIT_GET_KEY_RIGHT_STATE) {
            if (compareVersions(keyIdRight, QString::number(x.keyButtonId))) {
                KeyRightState = 1;
            } else {
                KeyRightState = 2;
            }
        }
        if (isKeyIdLeftRotateTest && state == STATE_WAIT_GET_KEY_LEFTROTATE_STATE) {
            if (compareVersions(keyIdLeftRotate, QString::number(x.keyButtonId))) {
                KeyLeftRotateState = 1;
            } else {
                KeyLeftRotateState = 2;
            }
        }
        if (isKeyIdRightRotateTest && state == STATE_WAIT_GET_KEY_RIGHTROTATE_STATE) {
            if (compareVersions(keyIdRightRotate, QString::number(x.keyButtonId))) {
                KeyRightRotateState = 1;
            } else {
                KeyRightRotateState = 2;
            }
        }
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
    if (dongleSerialPort->isOpen()) {
        disconnect(dongleSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        dongleSerialPort->close();
        qDebug() << getIndex() << "已关闭dongle串口";
    }
    if (usbSerialPort->isOpen()) {
        disconnect(usbSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        usbSerialPort->close();
        qDebug() << getIndex() << "已关闭usb串口";
    }
    if (jigSerialPort->isOpen()) {
        disconnect(jigSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        jigSerialPort->close();
        qDebug() << getIndex() << "已关闭jig串口";
    }

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
    waitingMesInspection = false;
    modeCheckPassed = false;
    workCurrentStats = CurrentStats();
    chargeCurrentStats = CurrentStats();
    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    // 与按键测试工站保持一致：仅使用公司SN规则校验（字母数字且长度>12）
    if (!validateCompanySnRule(ui->snInput->text())) {
        showlog("SN未通过公司规则校验");
        ui->snInput->clear();
        return;
    }

    emit send_startTest(getIndex());
    stringsn = ui->snInput->text();
    sn = ui->snInput->text().toUtf8();
    ui->snInput->setDisabled(1);

    // 新流程：SN校验后先解析MAC

    const QString parsedMac = parseMacFromSn(ui->snInput->text());
    if (parsedMac.isEmpty()) {
        showlog("从SN解析MAC失败（预留规则待补）");
        on_stopTest_clicked();
        return;
    }
    // MES站前检测，成功再开始测试
    // if (ui->isusemes->checkState()) {
    //     waitingMesInspection = true;
    //     processInspection(ui->snInput->text());
    //     return;
    // }
    // processInspection(ui->snInput->text());
    startFlowWithMac(parsedMac);
}
void key_test::on_macInput_returnPressed() {
    // 按键测试工站改为按SN启动，MAC由SN自动解析，不允许手动输入。
    showlog("当前工站不支持手动输入MAC，请扫描SN后回车启动测试");
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
    const bool simulateFlow = SETTINGS.value("SYSTEM/DebugSimulateFlow", false).toBool();
    if (stringsn != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            if (simulateFlow) {
                // 联调模拟：MES站前检查直接通过
                showlog("MES站前检测(模拟通过)");
                ui->mes_state->setText("MES PASS(模拟)");
                ui->mes_state->setStyleSheet("font-size: 33px; background-color: #00FF00; color: black; border-radius: 10px; "
                                             "padding: 10px; text-align: center;");
                waitingMesInspection = false;

                const QString parsedMac = parseMacFromSn(stringsn);
                startFlowWithMac(parsedMac);
                return;
            }
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



bool key_test::validateCompanySnRule(const QString& snValue) {
    // 参考 prod_test_for_trae 的 get_mac_from_scan 规则：
    // 1) 去空白后必须为字母数字
    // 2) 总长度需大于 12（可包含型号/SKU + 12 位 MAC）
    const QString normalized = QString(snValue).remove(QRegularExpression("\\s+")).trimmed().toUpper();
    static const QRegularExpression alnumRegex("^[0-9A-Z]+$");
    if (!alnumRegex.match(normalized).hasMatch()) {
        showlog("SN规则校验失败：仅允许字母数字");
        return false;
    }
    if (normalized.length() <= 12) {
        showlog("SN规则校验失败：长度不足，需大于12位");
        return false;
    }
    return true;
}

QString key_test::parseMacFromSn(const QString& snValue) {
    // 参考 prod_test_for_trae/core/common/sn_mac_parser.py 的 get_mac_from_scan:
    // - 默认从第4位后取12位MAC
    // - 若 Mes/model(视作SKU码)出现在型号后、合理偏移内，则从SKU结束位置取12位
    QString normalized = QString(snValue).remove(QRegularExpression("\\s+")).trimmed().toUpper();
    static const QRegularExpression alnumRegex("^[0-9A-Z]+$");
    if (!alnumRegex.match(normalized).hasMatch() || normalized.length() <= 12) {
        return QString();
    }

    const int modelLen = 4;
    const int middleLen = 7;
    const int macLen = 12;
    int startIdx = modelLen;

    QString skuCode = SETTINGS.value("Mes/model").toString().trimmed().toUpper();
    if (!skuCode.isEmpty()) {
        int searchPos = normalized.indexOf(skuCode);
        while (searchPos >= 0) {
            if (searchPos >= modelLen && searchPos <= (modelLen + middleLen - skuCode.length())) {
                startIdx = searchPos + skuCode.length();
                break;
            }
            searchPos = normalized.indexOf(skuCode, searchPos + 1);
        }
    }

    if (startIdx + macLen > normalized.length()) {
        return QString();
    }
    const QString rawMac = normalized.mid(startIdx, macLen);
    static const QRegularExpression hexRegex("^[0-9A-F]{12}$");
    if (!hexRegex.match(rawMac).hasMatch()) {
        return QString();
    }

    QString mac;
    for (int i = 0; i < macLen; i += 2) {
        if (!mac.isEmpty()) {
            mac += ":";
        }
        mac += rawMac.mid(i, 2);
    }
    return mac;
}

void key_test::startFlowWithMac(const QString& mac) {
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

bool key_test::verifyTestModeState() {
    // TODO: 预留测试模式回读校验逻辑
    return true;
}

bool key_test::controlProgrammablePowerForCharge(bool enable) {
    // TODO: 预留程控电源控制逻辑
    showlog(enable ? "程控电源: 开启供电（占位）" : "程控电源: 关闭供电（占位）");
    return true;
}

key_test::CurrentStats key_test::collectCurrentStats(const QString& itemName, int sampleCount,
                                                                       int sampleIntervalMs) {
    CurrentStats stats;
    if (sampleCount <= 0) {
        return stats;
    }

    QVector<double> samples;
    samples.reserve(sampleCount);
    for (int i = 0; i < sampleCount; ++i) {
        if (pack.factory == "hq") {
            usb->gethqMEASure();
        } else if (pack.factory == "lx") {
            usb->getlxMEASure(getIndex());
        } else {
            usb->getMEASure("");
        }
        waitWork(sampleIntervalMs);
        samples.append(measure_ammeter);
    }

    if (samples.isEmpty()) {
        return stats;
    }
    stats.minValue = samples.first();
    stats.maxValue = samples.first();
    double sum = 0.0;
    for (double value : samples) {
        stats.minValue = qMin(stats.minValue, value);
        stats.maxValue = qMax(stats.maxValue, value);
        sum += value;
    }
    stats.avgValue = sum / samples.size();
    stats.fluctuation = stats.maxValue - stats.minValue;
    stats.valid = true;

    showlog(QString("%1统计 -> 最小:%2mA 最大:%3mA 平均:%4mA 波动:%5mA")
                .arg(itemName)
                .arg(stats.minValue, 0, 'f', 4)
                .arg(stats.maxValue, 0, 'f', 4)
                .arg(stats.avgValue, 0, 'f', 4)
                .arg(stats.fluctuation, 0, 'f', 4));
    return stats;
}

bool key_test::evaluateCurrentStats(const QString& itemName, const CurrentStats& stats, double low, double high) {
    if (!stats.valid) {
        showlog(itemName + "采样无效");
        return false;
    }
    return stats.avgValue >= low && stats.avgValue <= high;
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
                KeyLeftRotateState = 0;
                KeyRightRotateState = 0;
                closeKeyWaitPromptProgrammatically();
                keyWaitPromptShown = false;
                factoryModeLogged = false;
                totalresult = "";
                at->resetConnected();
                measure_ammeter = 0;
                waitWork(1000);
                at->sendMac(ui->macInput->text());  // 发送mac地址
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
                            appendStationResult(testItems, "电源键测试", "0.0000", passValue);
                            testResultTableUpdate(testItems);
                            keyWaitPromptShown = false;
                            showlog("电源按钮测试通过");
                            state = STATE_WAIT_GET_KEY_STARTPAUSE_STATE;
                        } else {
                            appendStationResult(testItems, "电源键测试", "0.0000", failValue);
                            testResultTableUpdate(testItems);
                            keyWaitPromptShown = false;
                            totalresult = failValue;
                            state = STATE_SAVE_RESULT;
                        }
                    } else {
                        if (keyWaitPrompt == nullptr) {
                            keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                            "按键测试",
                                                            "请短按下电源按钮",
                                                            QMessageBox::NoButton,
                                                            this);
                            keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                            connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                            keyWaitPrompt->show();
                        }
                        refresh_key_times = 1;
                    }
                }
                break;

            // 等待获取开始/暂停键状态
            case STATE_WAIT_GET_KEY_STARTPAUSE_STATE:
                if (KeyStartPauseState != 0) {
                    showlog("开始/暂停键状态：" + QString::number(KeyStartPauseState));
                    closeKeyWaitPromptProgrammatically();
                    if (KeyStartPauseState == 1) {
                        showlog("开始/暂停键短按");
                        appendStationResult(testItems, "开始/暂停键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("开始/暂停键测试通过");
                        state = STATE_WAIT_GET_KEY_MODE_STATE;
                    } else {
                        appendStationResult(testItems, "开始/暂停键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                        "按键测试",  
                                                        "请短按下开始/暂停按钮",
                                                        QMessageBox::NoButton,
                                                        this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;
            // 等待获取模式键状态
            case STATE_WAIT_GET_KEY_MODE_STATE:
                if (KeyModeState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyModeState == 1) {
                        showlog("模式键短按");
                        appendStationResult(testItems, "模式键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("模式键测试通过");
                        state = STATE_WAIT_GET_KEY_SPEED_STATE;
                    } else {
                        appendStationResult(testItems, "模式键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下模式按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;
            // 等待获取速度键状态
            case STATE_WAIT_GET_KEY_SPEED_STATE:
                if (KeySpeedState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeySpeedState == 1) {
                        showlog("速度键短按");
                        appendStationResult(testItems, "速度键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("速度键测试通过");
                        state = STATE_WAIT_GET_KEY_PROGRAM_STATE;
                    } else {
                        appendStationResult(testItems, "速度键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下速度按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;

            // 等待获取程序键状态
            case STATE_WAIT_GET_KEY_PROGRAM_STATE:
                if (KeyProgramState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyProgramState == 1) {
                        showlog("程序键短按");
                        appendStationResult(testItems, "程序键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("程序键测试通过");
                        state = STATE_WAIT_GET_KEY_LEFT_STATE;
                    } else {
                        appendStationResult(testItems, "程序键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下程序按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;
            // 等待获取左键状态
            case STATE_WAIT_GET_KEY_LEFT_STATE:
                if (KeyLeftState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyLeftState == 1) {
                        showlog("左键短按");
                        appendStationResult(testItems, "左键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("左键测试通过");
                        state = STATE_WAIT_GET_KEY_RIGHT_STATE;
                    } else {
                        appendStationResult(testItems, "左键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下左按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;
            // 等待获取右键状态
            case STATE_WAIT_GET_KEY_RIGHT_STATE:
                if (KeyRightState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyRightState == 1) {
                        showlog("右键短按");
                        appendStationResult(testItems, "右键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("右键测试通过");
                        state = STATE_WAIT_GET_KEY_LEFTROTATE_STATE;
                    } else {
                        appendStationResult(testItems, "右键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下右按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;
            // 等待获取左旋键状态
            case STATE_WAIT_GET_KEY_LEFTROTATE_STATE:
                if (KeyLeftRotateState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyLeftRotateState == 1) {
                        showlog("左旋键短按");
                        appendStationResult(testItems, "左旋键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("左旋键测试通过");
                        state = STATE_WAIT_GET_KEY_RIGHTROTATE_STATE;
                    } else {
                        appendStationResult(testItems, "左旋键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下左旋按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
                }
                break;

            // 等待获取右旋键状态
            case STATE_WAIT_GET_KEY_RIGHTROTATE_STATE:
                if (KeyRightRotateState != 0) {
                    closeKeyWaitPromptProgrammatically();
                    if (KeyRightRotateState == 1) {
                        showlog("右旋键短按");
                        appendStationResult(testItems, "右旋键测试", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        showlog("右旋键测试通过");
                        totalresult = passValue;
                        state = STATE_SAVE_RESULT;
                    } else {
                        appendStationResult(testItems, "右旋键测试", "0.0000", failValue);
                        testResultTableUpdate(testItems);
                        keyWaitPromptShown = false;
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    if (keyWaitPrompt == nullptr) {
                        keyWaitPrompt = new QMessageBox(QMessageBox::Information,
                                                         "按键测试",
                                                         "请短按下右旋按钮",
                                                         QMessageBox::NoButton,
                                                         this);
                        keyWaitPrompt->setAttribute(Qt::WA_DeleteOnClose);
                        connect(keyWaitPrompt, &QObject::destroyed, this, &key_test::onKeyWaitPromptDestroyed);
                        keyWaitPrompt->show();
                    }

                    refresh_key_times = 1;
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
                    if (pack.itemvalue.isEmpty()) {
                        pack.itemvalue = pack.sn + "," + macAddress + ",KEY_TEST_RESULT*" + pack.result +
                                         QString("@KEY_TEST*0");
                    }
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
    at->sendMac(ui->macInput->text()); 
    sendCommandWithRetry([&]() { 
        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn})); 
    });
    // sendjigData(STATE_CYLINDER_RESET);

    // bandingMacSn(macAddress, stringsn);
    //     save_brush_log("dataTemp");
}

void key_test::on_pushButton_3_clicked() {
    usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement);

    // at->ask_mac();
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
                startFlowWithMac(macAddress);
            }
            // 在这里可以将提取到的 MAC 地址用于后续处理
        } else {
            logString = "";
            at->ask_mac();
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





