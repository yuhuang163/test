#include "suction.h"

#include "modbus_types.h"
#include "scpi_types.h"
#include "ui_suction.h"
#include <algorithm>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSerialPort>
#include <QStringList>
#include <QTimer>
#include <QVector>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {
UsbProtocolRoute protocolTypeFromSetting(const QString& type) {
    const QString value = type.trimmed().toLower();
    if (value == "scpi") {
        return UsbProtocolRoute::Scpi;
    }
    if (value == "hq" || value == "hqmodbus") {
        return UsbProtocolRoute::HqModbus;
    }
    if (value == "lx" || value == "lxmodbus") {
        return UsbProtocolRoute::LxModbus;
    }
    return UsbProtocolRoute::Auto;
}
} // namespace
suction::suction(int index, QWidget* parent) : test_base(parent), ui(new Ui::suction), basicInfoModel(new TestModel), peripheralModel(new TestModel) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = SUCTION_VER;

    ui->setupUi(this);
    // setAttribute(Qt::WA_DeleteOnClose);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts(); // 要搜索一下一开始

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
        state = STATE_SUCTION_TEST;
        qDebug() << getIndex() << "计时结束，进入吸力测试" << QDateTime::currentDateTime();
    });
    connect(usblogwaittime, &QTimer::timeout, [=]() {
        at->get(DongleCmd::GetGmac);
        showlog("正在定时器复位设备");
    });

    HighSuction = SETTINGS.value("Suction/HighSuction", SETTINGS.value("Current/HighstaticCurrent")).toDouble();
    LowSuction = SETTINGS.value("Suction/LowSuction", SETTINGS.value("Current/LowstaticCurrent")).toDouble();

    suction_wait_time = SETTINGS.value("Suction/wait_time", SETTINGS.value("Current/measure_wait_time", 15000)).toInt();

    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);
    showlog("model=" + pack.model);

    showlog("machineNo=" + pack.machineNo);
    showlog("HighSuction=" + QString::number(HighSuction));
    showlog("LowSuction=" + QString::number(LowSuction));
    showlog("suction_wait_time=" + QString::number(suction_wait_time));
    applySuctionProtocolConfig();
    if (suctionUsePicoSensor) {
        const QString picoPort = SETTINGS.value(QStringLiteral("Suction/PicoPort"), QStringLiteral("COM34")).toString().trimmed();
        if (!picoPort.isEmpty()) {
            if (ui->usbcomNameCombo->findText(picoPort) < 0) {
                ui->usbcomNameCombo->addItem(picoPort);
            }
            ui->usbcomNameCombo->setCurrentText(picoPort);
        }
    }
    // 程控电源：VisaPower/* 由 scpiVisaManager_ 加载。
    {
        ui->suctionPowerUseVisaCheckBox->setChecked(
            SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool());
        ui->suctionPowerVisaAddressEdit->setText(
            SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("GPIB0::7::INSTR")).toString());
    }

    if (pack.factory == "hq" || pack.factory == "jj") {
        ui->jigComNameCombo->setEnabled(false);
        ui->jigConnectButton->setEnabled(false);
        ui->jigDisconnectButton->setEnabled(false);
    }

    if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        ui->productDisconnectButton->setEnabled(true);
    } else {
        ui->productComNameCombo->setEnabled(false);
        ui->productConnectButton->setEnabled(false);
        ui->productDisconnectButton->setEnabled(false);
    }

    // if (QString(pack.product).compare("P20PS") == 0) {
    //     productBaudRate = 2000000;
    // } else {
    //     productBaudRate = 1000000;
    // }
    ui->tabWidget->setCurrentIndex(0); // 设置当前页为第一页
    connect(scpiVisaManager(), &QScpiManager::programmablePowerVoltageRead, this, &suction::refreshProgrammablePowerVoltage);
    connect(scpiVisaManager(), &QScpiManager::programmablePowerCurrentRead, this, &suction::refreshProgrammablePowerCurrent);
}

void suction::applySuctionProtocolConfig() {
    UsbProtocolRoute protocol = protocolTypeFromSetting("auto");
    int luxshareMachineId = getIndex();
    QString scpiCurrentType = SETTINGS.value("Suction/ScpiCurrentType", SETTINGS.value("Current/ScpiCurrentType", "CURR")).toString();
    QString scpiCurrentMode = SETTINGS.value("Suction/ScpiCurrentMode", SETTINGS.value("Current/ScpiCurrentMode", "DC")).toString();
    QString scpiRange = SETTINGS.value("Suction/ScpiRange", SETTINGS.value("Current/ScpiRange", "500e-3")).toString();
    suctionSampleDurationMs = SETTINGS.value("Suction/SampleDurationMs", 10000).toInt();
    suctionSampleIntervalMs = SETTINGS.value("Suction/SampleIntervalMs", 20).toInt();
    suctionPeakTargetKpa = SETTINGS.value("Suction/PeakTargetKpa", -36.0).toDouble();
    suctionPeakToleranceKpa = SETTINGS.value("Suction/PeakToleranceKpa", 2.6).toDouble();
    suctionPeakDiffMaxKpa = SETTINGS.value("Suction/PeakDiffMaxKpa", 2.6).toDouble();
    // 吸力工站外接程控电源统一由 VisaPower/ScpiUseVisa 控制，避免与 Suction/ExternalPowerEnabled 双开关冲突。
    suctionExternalPowerEnabled = SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool();
    suctionPowerOnWaitMs = SETTINGS.value("Suction/PowerOnWaitMs", 5000).toInt();
    suctionUsePicoSensor = SETTINGS.value(QStringLiteral("Suction/UsePicoSensor"), true).toBool();
    if (suctionUsePicoSensor) {
        protocol = UsbProtocolRoute::Scpi;
        usbBaudRate = SETTINGS.value(QStringLiteral("Suction/PicoBaudRate"), 19200).toInt();
    }

    if (!suctionUsePicoSensor && protocol == UsbProtocolRoute::Auto) {
        const QString factory = pack.factory.trimmed().toLower();
        if (factory == "hq") {
            protocol = UsbProtocolRoute::HqModbus;
        } else if (factory == "lx" || factory == "jj") {
            protocol = UsbProtocolRoute::LxModbus;
        } else {
            protocol = UsbProtocolRoute::Scpi;
        }
    }

    suctionProtocolType = protocol;
    // 配置 scpiUsbManager_ 和 modbusManager 路由
    UsbLinkConfig linkCfg;
    linkCfg.protocol = protocol;
    linkCfg.luxshareMachineId = luxshareMachineId;
    suctionUsbProtocolConfig_ = linkCfg;
    // 根据协议路由同步设备路由
    switch (protocol) {
    case UsbProtocolRoute::Scpi:
    case UsbProtocolRoute::Auto:
        scpiUsbManager_.setDeviceRoute(ScpiDeviceRoute::HuilingWfp60h);
        break;
    default:
        scpiUsbManager_.setDeviceRoute(ScpiDeviceRoute::None);
        break;
    }
    switch (protocol) {
    case UsbProtocolRoute::HqModbus:
        modbusManager.setDeviceRoute(ModbusDeviceRoute::HqAmmeterRtu);
        break;
    case UsbProtocolRoute::LxModbus:
        modbusManager.setDeviceRoute(ModbusDeviceRoute::LxAmmeterRtu);
        break;
    default:
        break;
    }
    modbusManager.setLuxshareMachineId(luxshareMachineId);
    {
        showlog(QStringLiteral("程控电源配置: useVisa=%1, address=%2")
                    .arg(SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool() ? 1 : 0)
                    .arg(SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("GPIB0::7::INSTR"))
                             .toString()));
    }

    showlog("吸力测试协议=" + SETTINGS.value("Suction/ProtocolType", SETTINGS.value("Current/ProtocolType", "auto")).toString() +
            " 实际生效协议=" + QString::number(static_cast<int>(suctionProtocolType)));
    showlog("吸力测试配置: machineId=" + QString::number(linkCfg.luxshareMachineId) +
            ", scpi=" + scpiCurrentType + ":" + scpiCurrentMode + " " + scpiRange +
            ", pico=" + QString(suctionUsePicoSensor ? "ON" : "OFF") +
            ", picoBaud=" + QString::number(usbBaudRate) +
            ", sampleMs=" + QString::number(suctionSampleDurationMs) +
            ", intervalMs=" + QString::number(suctionSampleIntervalMs) +
            ", peakTarget=" + QString::number(suctionPeakTargetKpa, 'f', 2) +
            ", peakTol=" + QString::number(suctionPeakToleranceKpa, 'f', 2) +
            ", diffMax=" + QString::number(suctionPeakDiffMaxKpa, 'f', 2));
}

bool suction::setExternalProgrammablePowerOutput(bool enable) {
    bool ok = true;
    if (suctionExternalPowerEnabled) {
        ok = execVisaHuiling(HuilingScpiCmd::ProgrammablePowerOutput, enable);
        if (!ok) {
            showlog(QStringLiteral("程控电源输出%1失败，重连VISA后重试")
                        .arg(enable ? QStringLiteral("打开") : QStringLiteral("关闭")));
            resetVisaBackend();
            ok = execVisaHuiling(HuilingScpiCmd::ProgrammablePowerOutput, enable);
        }
    }
    showlog(QStringLiteral("程控电源输出%1=%2")
                .arg(enable ? QStringLiteral("打开") : QStringLiteral("关闭"))
                .arg(ok ? QStringLiteral("OK") : QStringLiteral("NG")));
    return ok;
}

void suction::refreshProgrammablePowerVoltage(double valueVolts, bool ok) {
    programmablePowerMeasuredVoltageV_ = valueVolts;
    programmablePowerVoltageReadOk_ = ok;
    qDebug() << getIndex() << "程控电源电压回读(V):" << valueVolts << "ok=" << ok;
    if (!ok) {
        showlog(QStringLiteral("程控电源电压回读失败或无法解析"));
    }
}

void suction::refreshProgrammablePowerCurrent(double valueAmps, bool ok) {
    programmablePowerMeasuredCurrentA_ = valueAmps;
    programmablePowerCurrentReadOk_ = ok;
    qDebug() << getIndex() << "程控电源电流回读(A):" << valueAmps << "ok=" << ok;
    if (!ok) {
        showlog(QStringLiteral("程控电源电流回读失败或无法解析"));
    }
}

void suction::disconnect_dongle() {
    on_disconnectButton_clicked();
}

void suction::refreshMusicState(ProtocolMusicStateData data) {
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

void suction::refreshBaseData(ProtocolBaseInfoData data) {
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

void suction::refreshPeriphData(ProtocolPeriphStateData data) {
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

bool suction::sendPicoAtCommand(const QString& cmd) {
    if (!suctionUsePicoSensor || !usbSerialPort || !usbSerialPort->isOpen()) {
        return false;
    }
    QString line = cmd.trimmed();
    if (line.isEmpty()) {
        return false;
    }
    if (!line.endsWith(QLatin1String("\r\n"))) {
        line += QLatin1String("\r\n");
    }
    const qint64 n = usbSerialPort->write(line.toUtf8());
    if (n <= 0) {
        showlog(QStringLiteral("Pico指令发送失败: %1").arg(cmd.trimmed()));
        return false;
    }
    if (!usbSerialPort->waitForBytesWritten(1000)) {
        showlog(QStringLiteral("Pico指令写入超时: %1，错误=%2")
                    .arg(cmd.trimmed())
                    .arg(usbSerialPort->errorString()));
        return false;
    }
    showlog(QStringLiteral("Pico TX: %1").arg(cmd.trimmed()));
    return true;
}

void suction::sendPicoSysModeStart() {
    sendPicoAtCommand(QStringLiteral("AT+SYSMODE=1"));
}

void suction::sendPicoSysModeStop() {
    sendPicoAtCommand(QStringLiteral("AT+SYSMODE=0"));
}

void suction::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到吸力数据" << data;
    double normalValue = 0;
    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    if (suctionUsePicoSensor) {
        QString payload = data.trimmed();
        const int dollarIndex = payload.indexOf(QLatin1Char('$'));
        if (dollarIndex < 0) {
            qDebug() << getIndex() << "忽略Pico非数据帧:" << payload;
            return;
        }
        payload = payload.mid(dollarIndex + 1);
        const int semicolonIndex = payload.indexOf(QLatin1Char(';'));
        if (semicolonIndex < 0) {
            qDebug() << getIndex() << "忽略Pico未结束数据帧:" << data;
            return;
        }
        payload = payload.left(semicolonIndex);
        const QRegularExpression numberRegex(QStringLiteral("-?\\d+(?:\\.\\d+)?"));
        QRegularExpressionMatchIterator it = numberRegex.globalMatch(payload);
        QVector<double> values;
        while (it.hasNext()) {
            bool ok = false;
            const double value = it.next().captured(0).toDouble(&ok);
            if (ok) {
                values.append(value);
            }
        }
        if (values.size() >= 2) {
            const double leftValue = values.at(0);
            const double rightValue = values.at(1);
            conversionOk = true;
            damLeftKpa_ = leftValue;
            damRightKpa_ = rightValue;
            normalValue = damLeftKpa_;
        }
    } else {
        normalValue = data.toDouble(&conversionOk) * 1000;
    }

    if (conversionOk) {
        if (suctionUsePicoSensor) {
            qDebug() << getIndex() << "Pico吸力：左" << damLeftKpa_ << "kPa 右" << damRightKpa_ << "kPa";
        } else {
            qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        }
        measure_ammeter = normalValue;
    } else {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

suction::~suction() {
    qDebug() << getIndex() << "已进入析构";
    isTestContinue = 0;
    closeDongleSerialPort();
    closeUsbSerialPort();
    closeJigSerialPort();
    resetVisaBackend();

    delete ui;
}

void suction::refreshSn(ProtocolSnData data) {

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

void suction::on_snInput_returnPressed() {
    clearDisplay();
    macAddress = "没有mac地址";
    logString = "";

    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    applyAdaptiveV3ProductBySn(ui->snInput);

    const QString entered = ui->snInput->text().trimmed();
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
    emit send_start_test(getIndex());
    appendStationResult(testItems, "主板条码", "0.0000", passValue);
    testResultTableUpdate(testItems);
    // 获取比亚迪mes的sn校验规则
    processGetMesTestValue();
    // BYD + 启用 MES：输入不像公司 SN 时视为过程码，经 send_mes_test_value → bydmes::GetTestData 分支走 QuerySnByProcessCode
    // MES站前检测，成功再开始测试
    if (ui->isusemes->checkState()) {
        processInspection(ui->snInput->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
}

void suction::processGetMesTestValue() {
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
        emit send_mes_test_value(pack);
    }
}
void suction::getTestValue(const int mechines, const QString value) {
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
}

void suction::on_macInput_returnPressed() {
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

void suction::clearDisplay() {
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
void suction::refreshBleState(int state) {
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

void suction::refreshProductUartState(int state) {
    if (state)
        showlog("product串口连接成功");
    else {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        showlog("product串口连接断开");
    }
}

void suction::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void suction::refreshJigUartState(int state) {
    if (state)
        showlog("治具串口连接成功");
    else {
        ui->jigComNameCombo->setEnabled(true);
        ui->jigConnectButton->setEnabled(true);
        showlog("治具串口连接断开");
    }
}

void suction::refreshUsbUartState(int state) {
    if (state)
        showlog("传感器/采集 USB 串口连接成功");
    else {
        ui->usbcomNameCombo->setEnabled(true);
        ui->usbconnectButton->setEnabled(true);
        showlog("传感器/采集 USB 串口已断开");
    }
}
void suction::on_connectButton_clicked() {
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void suction::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}
void suction::on_usbconnectButton_clicked() {
    if (suctionUsePicoSensor) {
        usbBaudRate = SETTINGS.value(QStringLiteral("Suction/PicoBaudRate"), 19200).toInt();
        const QString picoPort = SETTINGS.value(QStringLiteral("Suction/PicoPort"), QStringLiteral("COM34")).toString().trimmed();
        if (!picoPort.isEmpty() && ui->usbcomNameCombo->currentText().trimmed().isEmpty()) {
            if (ui->usbcomNameCombo->findText(picoPort) < 0) {
                ui->usbcomNameCombo->addItem(picoPort);
            }
            ui->usbcomNameCombo->setCurrentText(picoPort);
        }
        picoRxBuffer_.clear();
        SerialChannel::OpenParams params;
        params.portName = ui->usbcomNameCombo->currentText().trimmed();
        params.baudRate = usbBaudRate;
        params.readDebounceMs = 10;
        params.flowControl = QSerialPort::HardwareControl;
        params.rtsDtrMode = SerialChannel::RtsDtrMode::DtrOnly;
        if (usbSerialChannel_->open(params)) {
            emit send_usb_serialPort_state(1);
            showlog(QStringLiteral("Pico吸力板串口已连接：%1，波特率=%2，流控=RTS/CTS")
                        .arg(ui->usbcomNameCombo->currentText())
                        .arg(usbBaudRate));
        } else {
            showlog(QStringLiteral("Pico吸力板串口打开失败：%1").arg(ui->usbcomNameCombo->currentText()));
            return;
        }
        ui->usbcomNameCombo->setEnabled(false);
        ui->usbconnectButton->setEnabled(false);
        return;
    }
    openUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(false);
    ui->usbconnectButton->setEnabled(false);
}

void suction::onUsbSerialFrame(const QByteArray& dataTemp) {
    if (!suctionUsePicoSensor) {
        test_base::onUsbSerialFrame(dataTemp);
        return;
    }

    if (dataTemp.isEmpty())
        return;

    picoRxBuffer_ += QString::fromUtf8(dataTemp);
    qDebug() << getIndex() << "Pico raw:" << QString::fromUtf8(dataTemp).trimmed();

    while (true) {
        int delimiter = -1;
        const int semicolonIndex = picoRxBuffer_.indexOf(QLatin1Char(';'));
        const int lfIndex = picoRxBuffer_.indexOf(QLatin1Char('\n'));
        if (semicolonIndex >= 0 && lfIndex >= 0) {
            delimiter = qMin(semicolonIndex, lfIndex);
        } else {
            delimiter = qMax(semicolonIndex, lfIndex);
        }
        if (delimiter < 0) {
            break;
        }

        QString frame = picoRxBuffer_.left(delimiter + 1).trimmed();
        picoRxBuffer_.remove(0, delimiter + 1);
        if (frame.isEmpty()) {
            continue;
        }
        refreshAmmeterData(frame);
    }

    if (picoRxBuffer_.size() > 4096) {
        picoRxBuffer_ = picoRxBuffer_.right(1024);
    }
}

void suction::on_usbdisconnectButton_clicked() {
    if (suctionUsePicoSensor)
        sendPicoSysModeStop();
    closeUsbSerialPort();
    picoRxBuffer_.clear();
    ui->usbcomNameCombo->setEnabled(true);
    ui->usbconnectButton->setEnabled(true);
}

void suction::on_suctionPowerVisaApplyButton_clicked() {
    SETTINGS.setValue(QStringLiteral("VisaPower/ScpiUseVisa"), ui->suctionPowerUseVisaCheckBox->isChecked());
    SETTINGS.setValue(QStringLiteral("VisaPower/VisaAddress"), ui->suctionPowerVisaAddressEdit->text().trimmed());
    resetVisaBackend();
    applySuctionProtocolConfig();
    scpiVisaManager()->loadHuilingVisaFromSettings();
    const bool useVisa = SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), false).toBool();
    const QString visaAddress =
        SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("GPIB0::7::INSTR")).toString().trimmed();
    ui->suctionPowerUseVisaCheckBox->setChecked(useVisa);
    ui->suctionPowerVisaAddressEdit->setText(visaAddress);
    showlog(QStringLiteral("已保存程控电源 VISA 配置（VisaPower），useVisa=%1，地址=%2")
                .arg(useVisa ? 1 : 0)
                .arg(visaAddress));
}

void suction::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void suction::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void suction::on_jigConnectButton_clicked() {
    openJigSerialPort();
    ui->jigComNameCombo->setEnabled(false);
    ui->jigConnectButton->setEnabled(false);
}

void suction::on_jigDisconnectButton_clicked() {
    closeJigSerialPort();
    ui->jigComNameCombo->setEnabled(true);
    ui->jigConnectButton->setEnabled(true);
}

void suction::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {

            showlog("正在进行站前检测");
            pack.sn = inputSnText;
            pack.mechines = getIndex();
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

void suction::startFlowWithMac(const QString& mac) {
    const bool simulateFlow = SETTINGS.value("SYSTEM/DebugSimulateFlow", false).toBool();
    usblogwaittime->stop();
    firstconnectbrush = 0;
    // ui->macInput->setDisabled(1);
    ui->macInput->setText(mac);
    macAddress = mac;
    last_macAddress = macAddress;
    ui->macLabel->setText("蓝牙mac: " + macAddress);

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    if (!usbSerialPort->isOpen() && (suctionUsePicoSensor || suctionExternalPowerEnabled)) {
        on_usbconnectButton_clicked();
    }
    if (pack.factory == "byd" && !jigSerialPort->isOpen()) {
        openJigSerialPort();
    }
    // jig->set_cylinder_state(1, getIndex());
    // bindingMacSn(macAddress, stringsn);
    state = STATE_IDLE;
    isTestContinue = true;
}

void suction::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
        case STATE_IDLE: { // 复位一切
            // 同工站双设备：传感器走 Pico/SCPI，电源走基类通用 VISA 对象。
            bool powerCfgOk = true;
            if (suctionExternalPowerEnabled) {
                powerCfgOk = execVisaHuiling(HuilingScpiCmd::ConfigureProgrammablePower);
            }
            const bool powerOutOk = setExternalProgrammablePowerOutput(true);
            showlog(QString("程控电源配置=%1, 输出打开=%2")
                        .arg(powerCfgOk ? "OK" : "NG")
                        .arg(powerOutOk ? "OK" : "NG"));
            if (!powerCfgOk || !powerOutOk) {
                pack.itemvalue = "power_on=NG";
                totalresult = failValue;
                state = STATE_SAVE_RESULT;
                break;
            }
            if (suctionExternalPowerEnabled && powerOutOk) {
                auto* prompt = new QMessageBox(QMessageBox::Information,
                                               QStringLiteral("操作提示"),
                                               QStringLiteral("请按产品电源键开机，5秒后自动继续连接"),
                                               QMessageBox::NoButton,
                                               this);
                prompt->setAttribute(Qt::WA_DeleteOnClose);
                prompt->show();
                QTimer::singleShot(5000, prompt, &QMessageBox::accept);
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
            waitWork(1000);
            at->set(DongleCmd::BleScanConnect, ui->macInput->text()); // 发送mac地址
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
                    QVariantMap m;
                    m["enter"] = 1;
                    protocolManager.set(DeviceCmd::SuctionMode, m);
                });
                state = STATE_VERIFY_SUCTION_MODE;
            }
            break;

        case STATE_VERIFY_SUCTION_MODE:
            if (canGoNext) {
                if (sendRetryOver) {
                    showlog("进入吸力模式失败（设备无响应或超时）");
                    appendStationResult(testItems, "进入吸力模式", "0.0000", failValue);
                    testResultTableUpdate(testItems);
                    totalresult = failValue;
                    pack.itemvalue = "suction_mode_timeout=NG";
                    state = STATE_SAVE_RESULT;
                } else {
                    showlog("已进入吸力模式");
                    appendStationResult(testItems, "进入吸力模式", "0.0000", passValue);
                    testResultTableUpdate(testItems);
                    state = STATE_SUCTION_TEST;
                }
            }
            break;

        case STATE_SUCTION_TEST: {
            const int durationMs = qMax(1000, suctionSampleDurationMs);
            const int intervalMs = qMax(20, suctionSampleIntervalMs);
            const int totalSamples = qMax(1, durationMs / intervalMs);
            QVector<double> leftSamples;
            QVector<double> rightSamples;
            leftSamples.reserve(totalSamples);
            rightSamples.reserve(totalSamples);

            if (suctionUsePicoSensor) {
                showlog(QStringLiteral("双通道吸力(Pico)：第1列=左吸力，第2列=右吸力，帧格式 $X1 X2 X3 X4;"));
                if (!usbSerialPort->isOpen()) {
                    showlog(QStringLiteral("Pico吸力板串口未连接，正在自动连接"));
                    on_usbconnectButton_clicked();
                }
                if (!usbSerialPort->isOpen()) {
                    showlog(QStringLiteral("Pico吸力板串口未连接，无法采集吸力数据"));
                    totalresult = failValue;
                    pack.itemvalue = "pico_suction_not_connected=NG";
                    state = STATE_SAVE_RESULT;
                    break;
                }
                picoRxBuffer_.clear();
                usbSerialPort->clear(QSerialPort::Input);
                sendPicoSysModeStart();
                waitWork(100);
                if (picoRxBuffer_.isEmpty())
                    showlog(QStringLiteral("Pico已发送SYSMODE=1，100ms内暂未收到数据"));
            } else {
                showlog(QStringLiteral("双通道吸力：本协议下按「先左后右」两次读数，左/右数据来自设备返回顺序"));
            }

            showlog(QString("━━━━━ 双传感器并行采集 (%1ms) ━━━━━").arg(durationMs));
            const QDateTime start = QDateTime::currentDateTime();
            const QDateTime end = start.addMSecs(durationMs);
            showlog("开始时间: " + start.toString("hh:mm:ss.zzz"));
            showlog("预计结束: " + end.toString("hh:mm:ss.zzz"));

            bool collectionStopped = false;
            for (int i = 0; i < totalSamples; ++i) {
                if (!isTestContinue) {
                    collectionStopped = true;
                    break;
                }
                if (suctionUsePicoSensor) {
                    // Pico 板持续主动上报；经 SerialChannel 防抖后由 onUsbSerialFrame 解析并更新 damLeftKpa_/damRightKpa_。
                    if (usbSerialPort->bytesAvailable() > 0) {
                        onUsbSerialFrame(usbSerialPort->readAll());
                    }
                } else {
                    execAmmeterMeasure();
                }
                const int readDelayMs = suctionUsePicoSensor ? 0 : 30;
                if (readDelayMs > 0) {
                    waitWork(readDelayMs);
                }
                const double leftKpa = suctionUsePicoSensor ? damLeftKpa_ : measure_ammeter;
                leftSamples.append(leftKpa);

                const double rightKpa = suctionUsePicoSensor ? damRightKpa_ : measure_ammeter;
                rightSamples.append(rightKpa);

                showlog(QString("[吸力原始点%1] 左: %2Kpa | 右: %3Kpa")
                            .arg(i + 1)
                            .arg(leftKpa, 0, 'f', 2)
                            .arg(rightKpa, 0, 'f', 2));
                if (((i + 1) % 10) == 0) {
                    showlog(QString("[%1] 左: %2Kpa | 右: %3Kpa")
                                .arg(i + 1)
                                .arg(leftKpa, 0, 'f', 2)
                                .arg(rightKpa, 0, 'f', 2));
                }
                waitWork(qMax(0, intervalMs - readDelayMs));
            }
            if (collectionStopped) {
                sendPicoSysModeStop();
                showlog(QStringLiteral("吸力采集已停止"));
                break;
            }

            const double lowerBound = suctionPeakTargetKpa - suctionPeakToleranceKpa;
            const double upperBound = suctionPeakTargetKpa + suctionPeakToleranceKpa;
            const bool hasSamples = !leftSamples.isEmpty() && !rightSamples.isEmpty();
            const double leftPeak = hasSamples ? *std::min_element(leftSamples.cbegin(), leftSamples.cend()) : 0.0;
            const double rightPeak = hasSamples ? *std::min_element(rightSamples.cbegin(), rightSamples.cend()) : 0.0;
            const bool leftPass = hasSamples && leftPeak >= lowerBound && leftPeak <= upperBound;
            const bool rightPass = hasSamples && rightPeak >= lowerBound && rightPeak <= upperBound;
            const double sideDiff = hasSamples ? qAbs(leftPeak - rightPeak) : 0.0;
            const bool diffPass = hasSamples && sideDiff <= suctionPeakDiffMaxKpa;

            showlog(QString("采集完成！循环: %1次, 左: %2点, 右: %3点")
                        .arg(totalSamples)
                        .arg(leftSamples.size())
                        .arg(rightSamples.size()));
            showlog(QString("峰值提取: 10秒采样内取左右最小值，左=%1Kpa，右=%2Kpa")
                        .arg(leftPeak, 0, 'f', 2)
                        .arg(rightPeak, 0, 'f', 2));
            showlog("━━━━━ 测试结果（仅吸力测试）━━━━━");
            showlog("【左侧吸力测量】");
            showlog(QString("峰值: %1Kpa, 判定: %2")
                        .arg(leftPeak, 0, 'f', 2)
                        .arg(leftPass ? "通过" : "不通过"));
            if (!leftPass) {
                showlog(QString("左侧峰值不通过（允许范围: %1~%2Kpa）")
                            .arg(lowerBound, 0, 'f', 2)
                            .arg(upperBound, 0, 'f', 2));
            }
            showlog("【右侧吸力测量】");
            showlog(QString("峰值: %1Kpa, 判定: %2")
                        .arg(rightPeak, 0, 'f', 2)
                        .arg(rightPass ? "通过" : "不通过"));
            if (!rightPass) {
                showlog(QString("右侧峰值不通过（允许范围: %1~%2Kpa）")
                            .arg(lowerBound, 0, 'f', 2)
                            .arg(upperBound, 0, 'f', 2));
            }
            showlog(QString("左右峰值差: %1Kpa, 判定: %2")
                        .arg(sideDiff, 0, 'f', 2)
                        .arg(diffPass ? "通过" : "不通过"));

            TestItem leftTest;
            leftTest.testItem = "左侧吸力峰值(kPa)";
            leftTest.testData = QString::number(leftPeak, 'f', 2);
            leftTest.ask = QString("%1±%2").arg(suctionPeakTargetKpa, 0, 'f', 2).arg(suctionPeakToleranceKpa, 0, 'f', 2);
            leftTest.testResult = leftPass ? passValue : failValue;
            testItems.append(leftTest);
            reportBydSfcKey("左侧吸力峰值", leftPeak, 1);

            TestItem rightTest;
            rightTest.testItem = "右侧吸力峰值(kPa)";
            rightTest.testData = QString::number(rightPeak, 'f', 2);
            rightTest.ask = QString("%1±%2").arg(suctionPeakTargetKpa, 0, 'f', 2).arg(suctionPeakToleranceKpa, 0, 'f', 2);
            rightTest.testResult = rightPass ? passValue : failValue;
            testItems.append(rightTest);
            reportBydSfcKey("右侧吸力峰值", rightPeak, 1);

            TestItem diffTest;
            diffTest.testItem = "左右吸力峰值差(kPa)";
            diffTest.testData = QString::number(sideDiff, 'f', 2);
            diffTest.ask = QString("<=%1").arg(suctionPeakDiffMaxKpa, 0, 'f', 2);
            diffTest.testResult = diffPass ? passValue : failValue;
            testItems.append(diffTest);
            testResultTableUpdate(testItems);
            reportBydSfcKey("左右吸力峰值差", sideDiff, 1);

            totalresult = (leftPass && rightPass && diffPass) ? passValue : failValue;
            if (totalresult == passValue) {
                showlog("仅吸力测试通过");
            } else {
                pack.itemvalue = QString("suction_left_peak=%1,suction_right_peak=%2,suction_peak_diff=%3")
                                     .arg(leftPeak, 0, 'f', 2)
                                     .arg(rightPeak, 0, 'f', 2)
                                     .arg(sideDiff, 0, 'f', 2);
            }
            QVariantMap m;
            m["enter"] = 0;
            protocolManager.set(DeviceCmd::SuctionMode, m);
            state = STATE_SAVE_RESULT;
            break;
        }

        case STATE_SAVE_RESULT:
            sendPicoSysModeStop();
            stringsn = "";
            setExternalProgrammablePowerOutput(false);
            if (totalresult == passValue) {
                pack.result = "PASS";
                pack.sn = stringsn;
                pack.instruct_num = "076";
                pack.itemvalue = pack.sn + "," + macAddress + ",SUCTION_RESULT*" + pack.result +
                    QString("@SUCTION*0");
                finishTestRecord(pack, ui->isusemes->checkState());
                if (ui->isusemes->checkState()) {
                    appendStationResult(testItems, "MES完成上报", "0.0000", passValue);
                    testResultTableUpdate(testItems);
                }

                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
            } else if ((totalresult == failValue)) {
                pack.result = "NG";
                pack.sn = stringsn;
                pack.instruct_num = "076";
                if (pack.itemvalue.isEmpty()) {
                    pack.itemvalue = pack.sn + "," + macAddress + ",SUCTION_RESULT*" + pack.result +
                        QString("@SUCTION*0");
                }
                finishTestRecord(pack, ui->isusemes->checkState());
                if (ui->isusemes->checkState()) {
                    appendStationResult(testItems, "MES完成上报", "0.0000", failValue);
                    testResultTableUpdate(testItems);
                }

                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");
            }

            showlog("测试结束，sn为：" + stringsn);
            ui->macInput->clear();
            ui->snInput->clear();

            isTestContinue = false; // 结束

            if (SETTINGS.value("SYSTEM/SuctionMechine", SETTINGS.value("SYSTEM/CurrentMechine")).toInt() == 3 ||
                SETTINGS.value("SYSTEM/SuctionMechine", SETTINGS.value("SYSTEM/CurrentMechine")).toInt() == 2) {
            } else {
                if (pack.factory == "xwd")
                    jig->set_cylinder_state(0, getIndex());
            }

            on_disconnectButton_clicked();
            ui->snInput->setDisabled(0);
            // ui->macInput->setDisabled(1);
            ui->getMac->setDisabled(0);
            emit send_end_test(getIndex());

            state = STATE_IDLE;
            break;
        }
        //   QCoreApplication::processEvents();
    }
}

void suction::on_pushButton_clicked() {
    // 开发测试入口：改为模拟SN扫码触发，MAC自动解析。
    // ui->snInput->setText("U03000077I1H00007D");
    // on_snInput_returnPressed();
    at->set(DongleCmd::BleScanConnect, ui->macInput->text());
    sendCommandWithRetry([&]() {
        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn}));
    });
    // sendjigData(STATE_CYLINDER_RESET);

    // bindingMacSn(macAddress, stringsn);
    //     save_brush_log("dataTemp");

    // 开发：已配置 VisaPower 时，对程控电源做一次电压/电流读（VISA 为同步，会进 refreshProgrammablePower*）
    applySuctionProtocolConfig();
    if (suctionExternalPowerEnabled && !SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QString()).toString().trimmed().isEmpty()) {
        const bool vOk = execVisaHuiling(HuilingScpiCmd::ReadProgrammableVoltage);
        const bool iOk = execVisaHuiling(HuilingScpiCmd::ReadProgrammableCurrent);
        showlog(QStringLiteral("VISA 程控电源试读：电压=%1 电流=%2")
                    .arg(vOk ? QStringLiteral("OK") : QStringLiteral("NG"))
                    .arg(iOk ? QStringLiteral("OK") : QStringLiteral("NG")));
        if (vOk && iOk) {
            showlog(QStringLiteral("  解析结果：电压=%1 V (ok=%3)，电流=%2 A (ok=%4)")
                        .arg(programmablePowerMeasuredVoltageV_, 0, 'f', 4)
                        .arg(programmablePowerMeasuredCurrentA_, 0, 'f', 6)
                        .arg(programmablePowerVoltageReadOk_ ? 1 : 0)
                        .arg(programmablePowerCurrentReadOk_ ? 1 : 0));
        }
    }
}

void suction::on_pushButton_3_clicked() {
    execAmmeterMeasure();

    // at->get(DongleCmd::GetGmac);
    // MesInit();
}
void suction::processReceivedData(const QByteArray& data) {
    // 将接收到的数据添加到日志字符串中
    logString += data;

    // 查找日志中最后一个 "Local Board" 条目的索引
    int lastIndex = logString.lastIndexOf("Local Board");

    if (lastIndex != -1) {
        // 从最后一个 "Local Board" 条目开始查找 MAC 地址
        int macIndex = logString.indexOf('=', lastIndex);

        if (macIndex != -1 &&
            logString.length() >= macIndex + 18) { // 判断是否包含完整的 MAC 地址（17 个字符 + 1 个分隔符）
            // 提取 MAC 地址
            QString macAddress = logString.mid(macIndex + 1, 17).trimmed(); // MAC 地址长度为17，并去除首尾空格
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

void suction::on_pushButton_4_clicked() {
    static int clickStep = 1; // 用于跟踪当前运行的步骤
    pack.mechines = 1;
    pack.sn = "U03000077I1H00007D";

    pack.result = "PASS";
    pack.line = "1C3A04";
    pack.itemvalue = QString("|BTMAC:3C:84:27:07:A8:D2|") + QString("SUCTION:PASS|");

    switch (clickStep) {
    case 1:
        showlog("Running processInspection");
        break;

    case 2:
        finishTestRecord(pack, true);
        showlog("Running send_end_test_pass");
        break;
    }

    clickStep++; // 增加步骤计数

    // 如果步骤计数超过了最大步骤数，重置为第一步
    if (clickStep > 2) {
        clickStep = 1;
    }
}

void suction::bindingMacSn(QString bindingMac, QString bindingSn) {
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
}

void suction::on_stopTest_clicked() {
    showlog("触发停止测试");
    sendPicoSysModeStop();
    setExternalProgrammablePowerOutput(false);
    usblogwaittime->stop();
    ui->macInput->clear();
    ui->snInput->clear();
    ui->snInput->setFocus();
    isTestContinue = false; // 结束
    if (pack.factory != "jj") {
        jig->set_cylinder_state(0, getIndex());
    }
    ui->snInput->setDisabled(0);
    // ui->macInput->setDisabled(1);
    ui->getMac->setDisabled(0);
}

void suction::reportBydSfcKey(const QString& dataName,
                              const QVariant& dataValue,
                              int qty) {
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
    p.instruct_num = dataName.trimmed();
    p.itemvalue = valueText;
    p.testCount = qty;
    p.iskeydata = 1;

    if (p.sn.isEmpty() || p.itemvalue.isEmpty()) {
        showlog(QStringLiteral("关键数据上报失败：SFC 或 DATA_VALUE 为空"));
        return;
    }
    showlog(QStringLiteral("MES：AddSfcKey 上报 %1=%2").arg(p.instruct_num, p.itemvalue));
    emit send_mes_test_value(p);
}

void suction::on_snruler_formes_clicked() {
    if (!ui->isusemes->isChecked()) {
        showlog(QStringLiteral("请先勾选「MES」后再请求 GetCustomData"));
        return;
    }
    MesPacketData p = pack;
    p.factory = QStringLiteral("byd");
    p.mechines = getIndex();
    p.sn.clear();
    p.itemvalue.clear();
    if (p.sn.trimmed().isEmpty()) {
        p.sn = ui->snInput->text().trimmed();
    }
    p.instruct_num = QStringLiteral("079");
    showlog(QStringLiteral("MES：GetCustomData（bydmes::GetTestData）"));
    emit send_mes_test_value(p);
}

void suction::on_start_formes_clicked() {
    if (!ui->isusemes->isChecked()) {
        showlog(QStringLiteral("请先勾选「MES」后再模拟站前检测"));
        return;
    }
    MesPacketData p = pack;
    p.factory = QStringLiteral("byd");
    p.mechines = getIndex();
    p.sn = ui->snInput->text().trimmed();
    if (p.sn.isEmpty()) {
        showlog(QStringLiteral("模拟站前：SN 为空，请在 SN 输入框填写后再试"));
        return;
    }
    p.instruct_num = QStringLiteral("079");
    showlog(QStringLiteral("模拟站前：send_process_inspection（BYD Start）"));
    emit send_process_inspection(p);
}

void suction::on_testdata_formes_clicked() {
    if (!ui->isusemes->isChecked()) {
        showlog(QStringLiteral("请先勾选「MES」后再模拟过站上报"));
        return;
    }
    MesPacketData p = pack;
    p.factory = QStringLiteral("byd");
    p.mechines = getIndex();
    p.sn = ui->snInput->text().trimmed();
    if (p.sn.isEmpty()) {
        showlog(QStringLiteral("模拟过站：SN 为空，请在 SN 输入框填写后再试"));
        return;
    }
    const QString mac = ui->macInput->text().trimmed().isEmpty() ? macAddress : ui->macInput->text().trimmed();
    p.result = QStringLiteral("PASS");
    p.error = QStringLiteral("NULL");
    p.instruct_num = QStringLiteral("076");
    p.itemvalue = p.sn + QLatin1Char(',') + mac + QStringLiteral(",SUCTION_RESULT*PASS@SUCTION*0");
    showlog(QStringLiteral("模拟过站：send_end_test_pass（BYD TestDataCollect + Complete）"));
    finishTestRecord(p, true);
}

void suction::on_pushButton_2_clicked() {
    // 手动：使用基类通用 VISA 对象上电（同工站 STATE_IDLE 电源段 + 电压限流）
    if (!suctionExternalPowerEnabled) {
        showlog(QStringLiteral("未启用外接程控电源：请在 ini 中开启 VisaPower/ScpiUseVisa"));
        return;
    }
    applySuctionProtocolConfig();

    const double powerVoltageV = SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), 12.0).toDouble();
    const double powerCurrentA = SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), 2.5).toDouble();
    const QString visaAddress =
        SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), QStringLiteral("GPIB0::7::INSTR")).toString().trimmed();
    if (!execVisaHuiling(HuilingScpiCmd::ConfigureProgrammablePower)) {
        showlog(QStringLiteral("程控电源 ConfigureProgrammablePower 失败"));
        return;
    }

    setExternalProgrammablePowerOutput(true);
    waitWork(20000);
    setExternalProgrammablePowerOutput(false);

    showlog(QStringLiteral("程控电源已按配置上电：电压=%1V，限流=%2A，链路=%3")
                .arg(powerVoltageV, 0, 'f', 2)
                .arg(powerCurrentA, 0, 'f', 3)
                .arg(QStringLiteral("VISA:%1").arg(visaAddress)));
}
