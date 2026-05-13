#include "suction.h"

#include "qusb.h"
#include "ui_suction.h"
#include <QSerialPort>
#include <QStringList>
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
    if (value == "byd" || value == "byddam3158") {
        return Qusb::ProtocolType::Byd;
    }
    return Qusb::ProtocolType::Auto;
}

QVariant suctionProfileValue(const QString& factory, const QString& key, const QVariant& fallback) {
    const QString fac = factory.trimmed().toLower();
    if (fac.isEmpty()) {
        return SETTINGS.value(QStringLiteral("Suction/") + key, fallback);
    }
    return SETTINGS.value(QStringLiteral("Suction_") + fac + "/" + key,
                          SETTINGS.value(QStringLiteral("Suction/") + key, fallback));
}

bool sameSerialPortName(const QString& openPortName, const QString& comboPortName)
{
    const QString a = openPortName.trimmed();
    const QString b = comboPortName.trimmed();
    if (a.isEmpty() || b.isEmpty()) {
        return false;
    }
    if (a.compare(b, Qt::CaseInsensitive) == 0) {
        return true;
    }
    const QString normA = a.startsWith(QStringLiteral("\\\\.\\"), Qt::CaseInsensitive) ? a : (QStringLiteral("\\\\.\\") + a);
    const QString normB = b.startsWith(QStringLiteral("\\\\.\\"), Qt::CaseInsensitive) ? b : (QStringLiteral("\\\\.\\") + b);
    return normA.compare(normB, Qt::CaseInsensitive) == 0;
}
}
suction::suction(int index, QWidget* parent) :
    test_base(parent), ui(new Ui::suction), basicInfoModel(new TestModel), peripheralModel(new TestModel) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = SUCTION_VER;

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
        state = STATE_SUCTION_TEST;
        qDebug() << getIndex() << "计时结束，进入吸力测试" << QDateTime::currentDateTime();
    });
    connect(usblogwaittime, &QTimer::timeout, [=]() {
        at->ask_mac();
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
    // 程控电源 VISA 与 ini [VisaPower] 同步到界面（改后点「保存并应用」写回）
    ui->suctionPowerUseVisaCheckBox->setChecked(powerProtocolConfig_.scpiUseVisa);
    ui->suctionPowerVisaAddressEdit->setText(powerProtocolConfig_.scpiVisaAddress);

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
    powerSerialPort = new QSerialPort(this);
    powerUsb = new Qusb(powerSerialPort);
    connect(jig, SIGNAL(send_suction_data(QString)), this, SLOT(refreshAmmeterData(QString)));
    // 程控电源读 V/I：与 refreshAmmeterData 同模式，由 Qusb 专用信号进入本工站 refresh*
    connect(usb, &Qusb::programmablePowerVoltageRead, this, &suction::refreshProgrammablePowerVoltage);
    connect(usb, &Qusb::programmablePowerCurrentRead, this, &suction::refreshProgrammablePowerCurrent);
    connect(powerUsb, &Qusb::programmablePowerVoltageRead, this, &suction::refreshProgrammablePowerVoltage);
    connect(powerUsb, &Qusb::programmablePowerCurrentRead, this, &suction::refreshProgrammablePowerCurrent);
    // 独立电源串口：与 test_base 传感器 USB 相同，定时合并 readyRead 再 parseCmd
    connect(powerSerialPort, &QSerialPort::readyRead, this, &suction::onPowerSerialPortReadyRead);
    connect(powerSerialPortTimer, &QTimer::timeout, this, &suction::readPowerSerialPortData);
}

void suction::applySuctionProtocolConfig() {
    Qusb::ProtocolConfig cfg;
    cfg.protocol = protocolTypeFromSetting(SETTINGS.value("Suction/ProtocolType", SETTINGS.value("Current/ProtocolType", "auto")).toString());
    cfg.luxshareMachineId = SETTINGS.value("Suction/LxMachineId", SETTINGS.value("Current/LxMachineId", getIndex())).toInt();
    cfg.scpiUseVisa = SETTINGS.value("Suction/ScpiUseVisa", false).toBool();
    cfg.scpiVisaAddress = SETTINGS.value("Suction/VisaAddress", "GPIB0::7::INSTR").toString();
    cfg.scpiCurrentType = SETTINGS.value("Suction/ScpiCurrentType", SETTINGS.value("Current/ScpiCurrentType", "CURR")).toString();
    cfg.scpiCurrentMode = SETTINGS.value("Suction/ScpiCurrentMode", SETTINGS.value("Current/ScpiCurrentMode", "DC")).toString();
    cfg.scpiRange = SETTINGS.value("Suction/ScpiRange", SETTINGS.value("Current/ScpiRange", "500e-3")).toString();
    cfg.scpiPowerVoltageV = SETTINGS.value("Suction/PowerVoltageV", 12.0).toDouble();
    cfg.scpiPowerCurrentA = SETTINGS.value("Suction/PowerCurrentLimitA", 2.5).toDouble();
    cfg.scpiSetVoltageCmd = SETTINGS.value("Suction/ScpiSetVoltageCmd",
                                            SETTINGS.value("VisaPower/ScpiSetVoltageCmd", QStringLiteral("VOLT %1")))
                                .toString();
    cfg.scpiSetCurrentCmd = SETTINGS.value("Suction/ScpiSetCurrentCmd",
                                           SETTINGS.value("VisaPower/ScpiSetCurrentCmd", QStringLiteral("CURR %1")))
                                .toString();
    cfg.scpiOutputOnCmd = SETTINGS.value("Suction/ScpiOutputOnCmd",
                                           SETTINGS.value("VisaPower/ScpiOutputOnCmd", QStringLiteral("OUTP ON")))
                              .toString();
    cfg.scpiOutputOffCmd = SETTINGS.value("Suction/ScpiOutputOffCmd",
                                            SETTINGS.value("VisaPower/ScpiOutputOffCmd", QStringLiteral("OUTP OFF")))
                               .toString();
    cfg.scpiReadVoltageCmd = SETTINGS.value("Suction/ScpiReadVoltageCmd",
                                            SETTINGS.value("VisaPower/ScpiReadVoltageCmd", QStringLiteral("MEASure:VOLTage:DC?")))
                                 .toString();
    cfg.scpiReadCurrentCmd = SETTINGS.value("Suction/ScpiReadCurrentCmd",
                                            SETTINGS.value("VisaPower/ScpiReadCurrentCmd",
                                                           QStringLiteral("MEASure:CURRent:DC? 500e-3")))
                                 .toString();
    damRangeCode = SETTINGS.value("Suction/DamRangeCode", 0x000C).toInt();
    damRawMax = SETTINGS.value("Suction/DamRawMax", 65535.0).toDouble();
    damCurrentFullScale_mA = SETTINGS.value("Suction/DamCurrentFullScale_mA", 10.0).toDouble();
    damPressureAtMinCurrent_kPa = SETTINGS.value("Suction/DamPressureAt0mA_kPa", -100.0).toDouble();
    damPressureAtMaxCurrent_kPa = SETTINGS.value("Suction/DamPressureAtFullCurrent_kPa", 0.0).toDouble();
    damLeftChannel = suctionProfileValue(pack.factory, QStringLiteral("DamLeftChannel"), 1).toInt();
    damRightChannel = suctionProfileValue(pack.factory, QStringLiteral("DamRightChannel"), 2).toInt();
    suctionSampleDurationMs = SETTINGS.value("Suction/SampleDurationMs", 15000).toInt();
    suctionSampleIntervalMs = SETTINGS.value("Suction/SampleIntervalMs", 100).toInt();
    suctionPeakTargetKpa = SETTINGS.value("Suction/PeakTargetKpa", 36.0).toDouble();
    suctionPeakToleranceKpa = SETTINGS.value("Suction/PeakToleranceKpa", 2.6).toDouble();
    suctionPeakDiffMaxKpa = SETTINGS.value("Suction/PeakDiffMaxKpa", 2.6).toDouble();
    suctionExternalPowerEnabled = SETTINGS.value("Suction/ExternalPowerEnabled", false).toBool();
    suctionPowerOnWaitMs = SETTINGS.value("Suction/PowerOnWaitMs", 5000).toInt();

    if (cfg.protocol == Qusb::ProtocolType::Auto) {
        const QString factory = pack.factory.trimmed().toLower();
        if (factory == "hq") {
            cfg.protocol = Qusb::ProtocolType::HqModbus;

        } else if (factory == "lx" || factory == "jj") {
            cfg.protocol = Qusb::ProtocolType::LxModbus;
        } else if (factory == "byd") {
            // BYD 产线：吸力经 DAM-3158(A) 采集卡读传感器（Modbus）；与本口程控电源 SCPI 互斥，见 ini 说明
            cfg.protocol = Qusb::ProtocolType::Byd;
        } else {
            cfg.protocol = Qusb::ProtocolType::Scpi;
        }
    }

    suctionProtocolType = cfg.protocol;
    usb->setProtocolConfig(cfg);
    suctionUsbProtocolConfig_ = cfg;
    powerSharesUsbSerial_ = false;
    jig->setDam3158Channel(damLeftChannel - 1);
    jig->setDam3158RangeCode(static_cast<quint16>(damRangeCode));
    powerBackendInitialized = false;
    powerProtocolConfig_ = cfg;
    powerProtocolConfig_.protocol = Qusb::ProtocolType::Scpi;
    const Qusb::ProtocolConfig& wfp = Qusb::programmablePowerDefaultsWfp60h();
    // 程控电源 SCPI/VISA 仅读 [VisaPower]（全工站共用），不回退 [Suction]/ProgrammablePower
    powerProtocolConfig_.scpiUseVisa =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiUseVisa"), wfp.scpiUseVisa).toBool();
    powerProtocolConfig_.scpiVisaAddress =
        SETTINGS.value(QStringLiteral("VisaPower/VisaAddress"), wfp.scpiVisaAddress).toString();
    powerProtocolConfig_.scpiCurrentType =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiCurrentType"), cfg.scpiCurrentType).toString();
    powerProtocolConfig_.scpiCurrentMode =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiCurrentMode"), cfg.scpiCurrentMode).toString();
    powerProtocolConfig_.scpiRange = SETTINGS.value(QStringLiteral("VisaPower/ScpiRange"), cfg.scpiRange).toString();
    powerProtocolConfig_.scpiPowerVoltageV =
        SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), wfp.scpiPowerVoltageV).toDouble();
    powerProtocolConfig_.scpiPowerCurrentA =
        SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), wfp.scpiPowerCurrentA).toDouble();
    powerProtocolConfig_.scpiSetVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), wfp.scpiSetVoltageCmd).toString();
    powerProtocolConfig_.scpiSetCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), wfp.scpiSetCurrentCmd).toString();
    powerProtocolConfig_.scpiOutputOnCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOnCmd"), wfp.scpiOutputOnCmd).toString();
    powerProtocolConfig_.scpiOutputOffCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOffCmd"), wfp.scpiOutputOffCmd).toString();
    powerProtocolConfig_.scpiReadVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), wfp.scpiReadVoltageCmd).toString();
    powerProtocolConfig_.scpiReadCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), wfp.scpiReadCurrentCmd).toString();
    showlog("电源链路配置: useVisa=" + QString::number(powerProtocolConfig_.scpiUseVisa ? 1 : 0) +
            ", visa=" + powerProtocolConfig_.scpiVisaAddress);

    showlog("吸力测试协议=" + SETTINGS.value("Suction/ProtocolType", SETTINGS.value("Current/ProtocolType", "auto")).toString() +
            " 实际生效协议=" + QString::number(static_cast<int>(suctionProtocolType)));
    showlog("吸力测试配置: machineId=" + QString::number(cfg.luxshareMachineId) +
            ", scpi=" + cfg.scpiCurrentType + ":" + cfg.scpiCurrentMode + " " + cfg.scpiRange +
            ", damRangeCode=0x" + QString::number(damRangeCode, 16).toUpper() +
            ", damRawMax=" + QString::number(damRawMax, 'f', 1) +
            ", damCurrentFullScale_mA=" + QString::number(damCurrentFullScale_mA, 'f', 3) +
            ", damPressureAtMinCurrent_kPa=" + QString::number(damPressureAtMinCurrent_kPa, 'f', 3) +
            ", damPressureAtMaxCurrent_kPa=" + QString::number(damPressureAtMaxCurrent_kPa, 'f', 3) +
            ", leftCh=" + QString::number(damLeftChannel) +
            ", rightCh=" + QString::number(damRightChannel) +
            ", sampleMs=" + QString::number(suctionSampleDurationMs) +
            ", intervalMs=" + QString::number(suctionSampleIntervalMs) +
            ", peakTarget=" + QString::number(suctionPeakTargetKpa, 'f', 2) +
            ", peakTol=" + QString::number(suctionPeakToleranceKpa, 'f', 2) +
            ", diffMax=" + QString::number(suctionPeakDiffMaxKpa, 'f', 2));
}

bool suction::ensurePowerBackendReady() {
    if (powerBackendInitialized) {
        if (powerSharesUsbSerial_ && !usbSerialPort->isOpen()) {
            powerBackendInitialized = false;
        } else {
            return true;
        }
    }
    if (!powerUsb || !powerSerialPort) {
        return false;
    }
    powerUsb->setProtocolConfig(powerProtocolConfig_);
    if (powerProtocolConfig_.scpiUseVisa) {
        powerSharesUsbSerial_ = false;
        powerBackendInitialized = true;
        return true;
    }

    // 程控电源走串口时与界面「传感器/采集 USB」所选 COM 一致（未勾选 VISA 时）
    const QString portName = getUsbcomNameCombo() ? getUsbcomNameCombo()->currentText().trimmed() : QString();
    if (portName.isEmpty()) {
        showlog("电源串口未配置：请勾选「程控电源(VISA)」使用 VISA，或在「传感器/采集 USB」选 COM 并连接（与电源同口时可共用）");
        return false;
    }

    if (usbSerialPort->isOpen() && sameSerialPortName(usbSerialPort->portName(), portName)) {
        powerSharesUsbSerial_ = true;
        if (powerSerialPort->isOpen()) {
            powerSerialPort->close();
        }
        powerBackendInitialized = true;
        showlog(QStringLiteral("电源与传感器 USB 共用串口: %1").arg(usbSerialPort->portName()));
        return true;
    }

    powerSharesUsbSerial_ = false;
    if (powerSerialPort->isOpen()) {
        powerSerialPort->close();
    }
    powerSerialPort->setPortName(portName);
    powerSerialPort->setBaudRate(usbBaudRate);
    powerSerialPort->setDataBits(QSerialPort::Data8);
    powerSerialPort->setParity(QSerialPort::NoParity);
    powerSerialPort->setStopBits(QSerialPort::OneStop);
    powerSerialPort->setFlowControl(QSerialPort::NoFlowControl);
    if (!powerSerialPort->open(QIODevice::ReadWrite)) {
        showlog("打开电源串口失败: " + portName);
        return false;
    }
    showlog("电源串口已连接: " + portName);
    powerBackendInitialized = true;
    return true;
}

bool suction::dispatchPowerAction(Qusb::PowerAction action) {
    if (!suctionExternalPowerEnabled) {
        return true;
    }
    if (!ensurePowerBackendReady()) {
        return false;
    }
    if (powerSharesUsbSerial_) {
        const Qusb::ProtocolConfig saved = usb->protocolConfig();
        usb->setProtocolConfig(powerProtocolConfig_);
        const bool ok = usb->sendPowerInstruction(action);
        usb->setProtocolConfig(saved);
        return ok;
    }
    return powerUsb->sendPowerInstruction(action);
}

void suction::setExternalProgrammablePowerOutput(bool enable) {
    if (!suctionExternalPowerEnabled) {
        return;
    }
    if (suctionProtocolType == Qusb::ProtocolType::Scpi) {
        usb->setProgrammablePowerOutput(enable);
        return;
    }
    if (powerSharesUsbSerial_) {
        const Qusb::ProtocolConfig saved = usb->protocolConfig();
        usb->setProtocolConfig(powerProtocolConfig_);
        usb->setProgrammablePowerOutput(enable);
        usb->setProtocolConfig(saved);
        return;
    }
    if (powerUsb) {
        powerUsb->setProgrammablePowerOutput(enable);
    }
}

void suction::disconnect_dongle() { on_disconnectButton_clicked(); }

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

void suction::onPowerSerialPortReadyRead()
{
    if (!powerSerialPortTimer || !powerSerialPort) {
        return;
    }
    powerSerialPortTimer->start(10);
    powerSerialPortBuf.append(powerSerialPort->readAll());
}

void suction::readPowerSerialPortData()
{
    if (!powerSerialPortTimer) {
        return;
    }
    powerSerialPortTimer->stop();
    const QByteArray dataTemp = powerSerialPortBuf;
    powerSerialPortBuf.clear();
    if (powerUsb && !dataTemp.isEmpty()) {
        powerUsb->parseCmd(dataTemp);
    }
}

void suction::refreshProgrammablePowerVoltage(double valueVolts, bool ok)
{
    programmablePowerMeasuredVoltageV_ = valueVolts;
    programmablePowerVoltageReadOk_ = ok;
    qDebug() << getIndex() << "程控电源电压回读(V):" << valueVolts << "ok=" << ok;
    if (!ok) {
        showlog(QStringLiteral("程控电源电压回读失败或无法解析"));
    }
}

void suction::refreshProgrammablePowerCurrent(double valueAmps, bool ok)
{
    programmablePowerMeasuredCurrentA_ = valueAmps;
    programmablePowerCurrentReadOk_ = ok;
    qDebug() << getIndex() << "程控电源电流回读(A):" << valueAmps << "ok=" << ok;
    if (!ok) {
        showlog(QStringLiteral("程控电源电流回读失败或无法解析"));
    }
}

void suction::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到吸力数据" << data;
    double normalValue = 0;
    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    if (suctionProtocolType == Qusb::ProtocolType::Byd) {
        const QStringList rawList = data.split(',', Qt::SkipEmptyParts);
        if (rawList.size() >= 8) {
            const double denom = (damRawMax > 0.0) ? damRawMax : 65535.0;
            const double scaleDenom =
                (damCurrentFullScale_mA > 0.0) ? damCurrentFullScale_mA : 10.0;
            const int leftIdx = qBound(0, damLeftChannel - 1, 7);
            const int rightIdx = qBound(0, damRightChannel - 1, 7);
            damRawChannels_.resize(8);
            conversionOk = true;
            for (int i = 0; i < 8; ++i) {
                bool rawOk = false;
                const double raw = rawList.at(i).toDouble(&rawOk);
                if (!rawOk) {
                    conversionOk = false;
                    break;
                }
                damRawChannels_[i] = raw;
            }
            if (conversionOk) {
                auto rawToKpa = [&](double rawValue) -> double {
                    const double current_mA = (rawValue / denom) * damCurrentFullScale_mA;
                    return damPressureAtMinCurrent_kPa +
                           (current_mA / scaleDenom) *
                               (damPressureAtMaxCurrent_kPa - damPressureAtMinCurrent_kPa);
                };
                damLeftKpa_ = rawToKpa(damRawChannels_.at(leftIdx));
                damRightKpa_ = rawToKpa(damRawChannels_.at(rightIdx));
                normalValue = damLeftKpa_;
            }
        }
    }
    else
        normalValue = data.toDouble(&conversionOk) * 1000;

    if (conversionOk) {
        // 转换成功
        if (suctionProtocolType == Qusb::ProtocolType::Byd)
            qDebug() << getIndex() << "转换后的数值：左" << damLeftKpa_ << "kPa 右" << damRightKpa_ << "kPa";
        else
            qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
    } else {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

suction::~suction() {
    qDebug() << getIndex() << "已进入析构";
    isTestContinue = 0;
    if (powerSerialPortTimer) {
        powerSerialPortTimer->stop();
    }
    if (powerSerialPort) {
        disconnect(powerSerialPort, nullptr, this, nullptr);
    }
    if (powerSerialPortTimer) {
        disconnect(powerSerialPortTimer, nullptr, this, nullptr);
    }
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
    if (powerSerialPort && powerSerialPort->isOpen()) {
        powerSerialPort->close();
        qDebug() << getIndex() << "已关闭电源串口";
    }

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

void suction::getTestValue(const int mechines, const QString value) {
    if (mechines != getIndex()) {
        return;
    }
    bool isProcessCodePayload = false;
    const QString snFromMes =0; //test_base::snFromBydProcessCodeMesPayload(value, &isProcessCodePayload);
    if (!isProcessCodePayload || snFromMes.isEmpty()) {
        return;
    }
    showlog(QStringLiteral("MES：过程码已换 SN"));
    // 真 SN 以 MES 返回值为准，传入后续解析，不再从界面读取
    continueSnInputAfterSnValidated(snFromMes);
}

void suction::on_snInput_returnPressed() {
    clearDisplay();
    macAddress = "没有mac地址";
    logString = "";

    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;

    const QString entered = ui->snInput->text().trimmed();
    if (entered.isEmpty()) {
        showlog(QStringLiteral("过程码为空"));
        return;
    }

    const QString fac = pack.factory.trimmed();
    const bool bydLine = (fac.compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0);
    const QRegularExpression snRegex(snPattern);
    const bool looksLikeCompanySn = snRegex.match(entered).hasMatch();

    // BYD + 启用 MES：输入不像公司 SN 时视为过程码，经 getMesTestValue → bydmes::GetTestData 分支走 QuerySnByProcessCode
    if (bydLine && ui->isusemes->checkState() && !looksLikeCompanySn) {
        MesPacketData p = pack;
        p.factory = QStringLiteral("byd");
        p.mechines = getIndex();
        p.sn = entered;
        p.itemvalue = entered;
        p.instruct_num = QStringLiteral("079");
        showlog(QStringLiteral("MES：getMesTestValue → 按过程码请求 SN"));
        emit getMesTestValue(p);
        return;
    }

    continueSnInputAfterSnValidated(entered);
}

void suction::continueSnInputAfterSnValidated(const QString& snTextRaw) {
    const QString snText = snTextRaw.trimmed();
    if (snText.isEmpty()) {
        showlog(QStringLiteral("SN为空"));
        return;
    }
    // 界面与后续 pack.sn 等保持一致；判定与解析均以入参 snText 为准
    // ui->snInput->setText(snText);
    QRegularExpression snRegex(snPattern);
    if (!snRegex.match(snText).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(snText.length()));
        showlog("要求格式为" + snPattern);
        ui->snInput->clear();
        return;
    }

    const QString parsedMac = parseMacFromSn(snText);
    if (parsedMac.isEmpty()) {
        showlog("从SN解析MAC失败（预留规则待补）");
        on_stopTest_clicked();
        return;
    }

    stringsn = snText;
    sn = snText.toUtf8();
    ui->snInput->setDisabled(1);

    if (ui->isusemes->checkState()) {
        processInspection(snText);
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
    startFlowWithMac(parsedMac);
    emit send_startTest(getIndex());
}
void suction::on_macInput_returnPressed() {
    // 吸力工站改为按SN启动，MAC由SN自动解析，不允许手动输入。
    showlog("当前吸力工站不支持手动输入MAC，请扫描SN后回车启动测试");
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
    openUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(false);
    ui->usbconnectButton->setEnabled(false);
}

void suction::on_usbdisconnectButton_clicked() {
    closeUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(true);
    ui->usbconnectButton->setEnabled(true);
}

void suction::on_suctionPowerVisaApplyButton_clicked() {
    SETTINGS.setValue(QStringLiteral("VisaPower/ScpiUseVisa"), ui->suctionPowerUseVisaCheckBox->isChecked());
    SETTINGS.setValue(QStringLiteral("VisaPower/VisaAddress"), ui->suctionPowerVisaAddressEdit->text().trimmed());
    powerBackendInitialized = false;
    applySuctionProtocolConfig();
    ui->suctionPowerUseVisaCheckBox->setChecked(powerProtocolConfig_.scpiUseVisa);
    ui->suctionPowerVisaAddressEdit->setText(powerProtocolConfig_.scpiVisaAddress);
    showlog(QStringLiteral("已保存程控电源 VISA 配置（VisaPower），useVisa=%1，地址=%2")
                 .arg(powerProtocolConfig_.scpiUseVisa ? 1 : 0)
                 .arg(powerProtocolConfig_.scpiVisaAddress));
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

void suction::processInspection(QString stringsn) {
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

void suction::startFlowWithMac(const QString& mac) {
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
    if (!usbSerialPort->isOpen() && suctionExternalPowerEnabled) {
        on_usbconnectButton_clicked();
    }
    if (pack.factory == "byd" && !jigSerialPort->isOpen()) {
    openJigSerialPort();
    }
    // jig->set_cylinder_state(1, getIndex());
    // bandingMacSn(macAddress, stringsn);
    state = STATE_IDLE;
    isTestContinue = true;
}

void suction::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
            case STATE_IDLE:  // 复位一切

                // 同工站双设备：传感器走 usb(Byd)，电源走独立 power 后端（VISA 或独立串口）
                if (suctionExternalPowerEnabled) {
                    const bool powerCfgOk = dispatchPowerAction(Qusb::PowerAction::ConfigurePowerSupply);
                    if (!powerCfgOk) {
                        pack.itemvalue = "power_config=NG";
                        totalresult = failValue;
                        state = STATE_SAVE_RESULT;
                        break;
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
                    const int intervalMs = qMax(50, suctionSampleIntervalMs);
                    const int totalSamples = qMax(1, durationMs / intervalMs);
                    QVector<double> leftSamples;
                    QVector<double> rightSamples;
                    leftSamples.reserve(totalSamples);
                    rightSamples.reserve(totalSamples);

                    if (suctionProtocolType == Qusb::ProtocolType::Byd) {
                        showlog(QStringLiteral("双通道吸力(DAM-3158)：左=第%1路AI，右=第%2路AI（1~8，与接线一致）")
                                    .arg(damLeftChannel)
                                    .arg(damRightChannel));
                        if (damLeftChannel == damRightChannel) {
                            showlog(QStringLiteral("警告：左右吸力配置为同一路通道，请检查 DamLeftChannel / DamRightChannel"));
                        }
                    } else {
                        showlog(QStringLiteral("双通道吸力：本协议下按「先左后右」两次读数，左/右数据来自设备返回顺序"));
                    }

                    showlog(QString("━━━━━ 双传感器并行采集 (%1ms) ━━━━━").arg(durationMs));
                    const QDateTime start = QDateTime::currentDateTime();
                    const QDateTime end = start.addMSecs(durationMs);
                    showlog("开始时间: " + start.toString("hh:mm:ss.zzz"));
                    showlog("预计结束: " + end.toString("hh:mm:ss.zzz"));

                    for (int i = 0; i < totalSamples; ++i) {
                        if (suctionProtocolType == Qusb::ProtocolType::Byd) {
                            jig->getDam3158Measure();
                        } else {
                            usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement);
                        }
                        waitWork(30);
                        const double leftKpa = (suctionProtocolType == Qusb::ProtocolType::Byd) ? damLeftKpa_ : measure_ammeter;
                        leftSamples.append(leftKpa);

                        
                        const double rightKpa = (suctionProtocolType == Qusb::ProtocolType::Byd) ? damRightKpa_ : measure_ammeter;
                        rightSamples.append(rightKpa);

                        if (((i + 1) % 10) == 0) {
                            showlog(QString("[%1] 左: %2Kpa | 右: %3Kpa")
                                        .arg(i + 1)
                                        .arg(leftKpa, 0, 'f', 2)
                                        .arg(rightKpa, 0, 'f', 2));
                        }
                        waitWork(qMax(0, intervalMs - 60));
                    }

                    const double lowerBound = suctionPeakTargetKpa - suctionPeakToleranceKpa;
                    const double upperBound = suctionPeakTargetKpa + suctionPeakToleranceKpa;
                    const int minPeakDistanceSamples = qMax(1, 300 / qMax(1, intervalMs));
                    auto extractPeaks = [&](const QVector<double>& values) -> QVector<double> {
                        QVector<double> peaks;
                        if (values.size() < 3) {
                            return peaks;
                        }
                        int lastPeakIndex = -minPeakDistanceSamples;
                        for (int i = 1; i < values.size() - 1; ++i) {
                            const double prev = qAbs(values.at(i - 1));
                            const double curr = qAbs(values.at(i));
                            const double next = qAbs(values.at(i + 1));
                            if (curr >= prev && curr > next) {
                                if (!peaks.isEmpty() && (i - lastPeakIndex) < minPeakDistanceSamples) {
                                    peaks.last() = qMax(peaks.last(), curr);
                                } else {
                                    peaks.append(curr);
                                    lastPeakIndex = i;
                                }
                            }
                        }
                        return peaks;
                    };
                    auto allPeaksInRange = [&](const QVector<double>& peaks) -> bool {
                        if (peaks.isEmpty()) {
                            return false;
                        }
                        for (double p : peaks) {
                            if (p < lowerBound || p > upperBound) {
                                return false;
                            }
                        }
                        return true;
                    };
                    const QVector<double> leftPeaks = extractPeaks(leftSamples);
                    const QVector<double> rightPeaks = extractPeaks(rightSamples);
                    const bool leftPass = allPeaksInRange(leftPeaks);
                    const bool rightPass = allPeaksInRange(rightPeaks);
                    const int pairedCount = qMin(leftPeaks.size(), rightPeaks.size());
                    double sideDiff = 0.0;
                    bool diffPass = (pairedCount > 0);
                    for (int i = 0; i < pairedCount; ++i) {
                        const double d = qAbs(leftPeaks.at(i) - rightPeaks.at(i));
                        sideDiff = qMax(sideDiff, d);
                        if (d > suctionPeakDiffMaxKpa) {
                            diffPass = false;
                        }
                    }
                    double leftPeak = 0.0;
                    for (double p : leftPeaks) {
                        leftPeak = qMax(leftPeak, p);
                    }
                    double rightPeak = 0.0;
                    for (double p : rightPeaks) {
                        rightPeak = qMax(rightPeak, p);
                    }

                    showlog(QString("采集完成！循环: %1次, 左: %2点, 右: %3点")
                                .arg(totalSamples)
                                .arg(leftSamples.size())
                                .arg(rightSamples.size()));
                    showlog(QString("峰值提取: 左%1个, 右%2个").arg(leftPeaks.size()).arg(rightPeaks.size()));
                    if (leftPeaks.size() != rightPeaks.size()) {
                        showlog(QString("提示：左右峰数量不一致，按最小配对数%1做差值判定").arg(pairedCount));
                    }
                    showlog("━━━━━ 测试结果（仅吸力测试）━━━━━");
                    showlog("【左侧吸力测量】");
                    showlog(QString("峰值最大: %1Kpa, 所有峰值判定: %2")
                                .arg(leftPeak, 0, 'f', 2)
                                .arg(leftPass ? "通过" : "不通过"));
                    showlog("【右侧吸力测量】");
                    showlog(QString("峰值最大: %1Kpa, 所有峰值判定: %2")
                                .arg(rightPeak, 0, 'f', 2)
                                .arg(rightPass ? "通过" : "不通过"));
                    showlog(QString("左右峰值逐对差最大: %1Kpa, 判定: %2")
                                .arg(sideDiff, 0, 'f', 2)
                                .arg(diffPass ? "通过" : "不通过"));

                    TestItem leftTest;
                    leftTest.testItem = "左侧吸力所有峰值(kPa)";
                    leftTest.testData = QString("count=%1,max=%2")
                                            .arg(leftPeaks.size())
                                            .arg(leftPeak, 0, 'f', 2);
                    leftTest.ask = QString("%1±%2").arg(suctionPeakTargetKpa, 0, 'f', 2).arg(suctionPeakToleranceKpa, 0, 'f', 2);
                    leftTest.testResult = leftPass ? passValue : failValue;
                    testItems.append(leftTest);

                    TestItem rightTest;
                    rightTest.testItem = "右侧吸力所有峰值(kPa)";
                    rightTest.testData = QString("count=%1,max=%2")
                                                .arg(rightPeaks.size())
                                                .arg(rightPeak, 0, 'f', 2);
                    rightTest.ask = QString("%1±%2").arg(suctionPeakTargetKpa, 0, 'f', 2).arg(suctionPeakToleranceKpa, 0, 'f', 2);
                    rightTest.testResult = rightPass ? passValue : failValue;
                    testItems.append(rightTest);

                    TestItem diffTest;
                    diffTest.testItem = "两侧吸力峰值逐对差(kPa)";
                    diffTest.testData = QString::number(sideDiff, 'f', 2);
                    diffTest.ask = QString("<=%1").arg(suctionPeakDiffMaxKpa, 0, 'f', 2);
                    diffTest.testResult = diffPass ? passValue : failValue;
                    testItems.append(diffTest);
                    testResultTableUpdate(testItems);

                    totalresult = (leftPass && rightPass && diffPass) ? passValue : failValue;
                    if (totalresult == passValue) {
                        showlog("仅吸力测试通过");
                    } else {
                        pack.itemvalue = QString("suction_left_peaks=%1,suction_right_peaks=%2,suction_left_peak_max=%3,suction_right_peak_max=%4,suction_peak_pair_diff_max=%5")
                                                .arg(leftPeaks.size())
                                                .arg(rightPeaks.size())
                                                .arg(leftPeak, 0, 'f', 2)
                                                .arg(rightPeak, 0, 'f', 2)
                                                .arg(sideDiff, 0, 'f', 2);
                    }
                    state = STATE_SAVE_RESULT;
                    break;
            }

            case STATE_SAVE_RESULT:
                stringsn = "";
                setExternalProgrammablePowerOutput(false);
                if (totalresult == passValue) {
                    pack.result = "PASS";
                    pack.sn = stringsn;
                    pack.instruct_num = "076";
                    pack.itemvalue = pack.sn + "," + macAddress + ",SUCTION_RESULT*" + pack.result +
                                     QString("@SUCTION*0");
                    if (ui->isusemes->checkState()) {
                        // emit send_end_testPass(pack);
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
                    if (ui->isusemes->checkState()) {
                        // emit send_end_testPass(pack);
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

                isTestContinue = false;  // 结束

                if (SETTINGS.value("SYSTEM/SuctionMechine", SETTINGS.value("SYSTEM/CurrentMechine")).toInt() == 3 ||
                    SETTINGS.value("SYSTEM/SuctionMechine", SETTINGS.value("SYSTEM/CurrentMechine")).toInt() == 2) {
                } else {
                    if (pack.factory == "xwd")
                        jig->set_cylinder_state(0, getIndex());
                }

                on_disconnectButton_clicked();
                ui->snInput->setDisabled(0);
                ui->macInput->setDisabled(1);
                ui->getMac->setDisabled(0);
                // emit send_end_test(getIndex());

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
    at->sendMac(ui->macInput->text()); 
    sendCommandWithRetry([&]() { 
        protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn})); 
    });
    // sendjigData(STATE_CYLINDER_RESET);

    // bandingMacSn(macAddress, stringsn);
    //     save_brush_log("dataTemp");

    // 开发：已配置 VisaPower 且后端就绪时，对程控电源做一次电压/电流读（VISA 为同步，会进 refreshProgrammablePower*）
    applySuctionProtocolConfig();
    if (powerProtocolConfig_.scpiUseVisa && !powerProtocolConfig_.scpiVisaAddress.trimmed().isEmpty()) {
        if (!ensurePowerBackendReady()) {
            showlog(QStringLiteral("VISA 程控电源试读跳过：后端未就绪"));
        } else {
            bool vOk = false;
            bool iOk = false;
            if (powerSharesUsbSerial_) {
                const Qusb::ProtocolConfig saved = usb->protocolConfig();
                usb->setProtocolConfig(powerProtocolConfig_);
                vOk = usb->readProgrammablePowerVoltage();
                iOk = usb->readProgrammablePowerCurrent();
                usb->setProtocolConfig(saved);
            } else if (powerUsb) {
                powerUsb->setProtocolConfig(powerProtocolConfig_);
                vOk = powerUsb->readProgrammablePowerVoltage();
                iOk = powerUsb->readProgrammablePowerCurrent();
            }
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
}

void suction::on_pushButton_3_clicked() {
    usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement);

    // at->ask_mac();
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
            at->ask_mac();
            showlog("日志数据中未找到完整的MAC地址，正在重发请求获取mac地址");
        }
    } else {
        // qDebug() << getIndex()<< "日志数据中未找到mac地址关键字。";
    }
}

void suction::on_pushButton_4_clicked() {
    static int clickStep = 1;  // 用于跟踪当前运行的步骤
    pack.mechines = 1;
    pack.sn = "U03000077I1H00007D";

    pack.result = "PASS";
    pack.line = "1C3A04";
    pack.itemvalue = QString("|BTMAC:3C:84:27:07:A8:D2|") + QString("SUCTION:PASS|");

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

void suction::bandingMacSn(QString bandingmac, QString bandingsn) {
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

void suction::on_stopTest_clicked() {
    showlog("触发停止测试");
    setExternalProgrammablePowerOutput(false);
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
        p.sn = stringsn.trimmed();
    }
    p.instruct_num = QStringLiteral("079");
    showlog(QStringLiteral("MES：GetCustomData（bydmes::GetTestData）"));
    emit getMesTestValue(p);
}

void suction::on_start_formes_clicked() {
    if (!ui->isusemes->isChecked()) {
        showlog(QStringLiteral("请先勾选「MES」后再模拟站前检测"));
        return;
    }
    MesPacketData p = pack;
    p.factory = QStringLiteral("byd");
    p.mechines = getIndex();
    p.sn = stringsn.trimmed();
    if (p.sn.isEmpty()) {
        showlog(QStringLiteral("模拟站前：SN 为空，请在 SN 输入框填写后再试"));
        return;
    }
    p.instruct_num = QStringLiteral("079");
    showlog(QStringLiteral("模拟站前：sendProcessInspection（BYD Start）"));
    emit sendProcessInspection(p);
}

void suction::on_testdata_formes_clicked() {
    if (!ui->isusemes->isChecked()) {
        showlog(QStringLiteral("请先勾选「MES」后再模拟过站上报"));
        return;
    }
    MesPacketData p = pack;
    p.factory = QStringLiteral("byd");
    p.mechines = getIndex();
    p.sn = stringsn.trimmed();
    if (p.sn.isEmpty()) {
        showlog(QStringLiteral("模拟过站：SN 为空，请在 SN 输入框填写后再试"));
        return;
    }
    const QString mac = ui->macInput->text().trimmed().isEmpty() ? macAddress : ui->macInput->text().trimmed();
    p.result = QStringLiteral("PASS");
    p.error = QStringLiteral("NULL");
    p.instruct_num = QStringLiteral("076");
    p.itemvalue = p.sn + QLatin1Char(',') + mac + QStringLiteral(",SUCTION_RESULT*PASS@SUCTION*0");
    showlog(QStringLiteral("模拟过站：send_end_testPass（BYD TestDataCollect + Complete）"));
    emit send_end_testPass(p);
}


void suction::on_pushButton_2_clicked()
{
    // 手动：就绪 power 后端（powerUsb / VISA / 与传感器 USB 共用），再按 powerProtocolConfig_ 上电（同工站 STATE_IDLE 电源段 + 电压限流）
    if (!suctionExternalPowerEnabled) {
        showlog(QStringLiteral("未启用外接程控电源：请在 ini 中开启 Suction/ExternalPowerEnabled"));
        return;
    }
    applySuctionProtocolConfig();

    if (!ensurePowerBackendReady()) {
        showlog(QStringLiteral("程控电源后端未就绪：请配置 VISA（程控电源行）或连接传感器 USB/独立电源串口"));
        return;
    }
    if (!dispatchPowerAction(Qusb::PowerAction::ConfigurePowerSupply)) {
        showlog(QStringLiteral("程控电源 ConfigurePowerSupply 失败"));
        return;
    }

    const double v = powerProtocolConfig_.scpiPowerVoltageV;
    const double a = powerProtocolConfig_.scpiPowerCurrentA;
    bool cfgOk = false;
    if (powerSharesUsbSerial_) {
        const Qusb::ProtocolConfig saved = usb->protocolConfig();
        usb->setProtocolConfig(powerProtocolConfig_);
        cfgOk = usb->configureProgrammablePower(v, a);
        usb->setProtocolConfig(saved);
    } else {
        cfgOk = powerUsb->configureProgrammablePower(v, a);
    }
    if (!cfgOk) {
        showlog(QStringLiteral("程控电源电压/限流下发失败（检查 SCPI 与连接）"));
        return;
    }

    setExternalProgrammablePowerOutput(true);
    QString linkDesc;
    if (powerProtocolConfig_.scpiUseVisa) {
        linkDesc = QStringLiteral("VISA:%1").arg(powerProtocolConfig_.scpiVisaAddress);
    } else if (powerSharesUsbSerial_) {
        linkDesc = QStringLiteral("与传感器共用:%1").arg(usbSerialPort->isOpen() ? usbSerialPort->portName() : QString());
    } else {
        linkDesc = QStringLiteral("独立串口:%1").arg(powerSerialPort && powerSerialPort->isOpen() ? powerSerialPort->portName() : QString());
    }
    showlog(QStringLiteral("程控电源已按配置上电：电压=%1V，限流=%2A，链路=%3")
                 .arg(v, 0, 'f', 2)
                 .arg(a, 0, 'f', 3)
                 .arg(linkDesc));
}

