#include "quiescent_current.h"

#include "ui_quiescent_current.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif
quiescent_current::quiescent_current(int index, QWidget* parent) :
    ui(new Ui::quiescent_current), basicInfoModel(new TestModel), peripheralModel(new TestModel) {
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
        state = STATE_SLEEP_OPEN;
        ble_waittime->stop();
        qDebug() << getIndex() << "取消禁止休眠失败，直接测试静态电流" << QDateTime::currentDateTime();
    });
    connect(usblogwaittime, &QTimer::timeout, [=]() {
        at->ask_mac();
        showlog("正在定时器复位牙刷");
    });

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

    if (QString(pack.product).compare("P20PS") == 0) {
        productBaudRate = 2000000;
    } else {
        productBaudRate = 1000000;
    }
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}

void quiescent_current::disconnect_dongle() { on_disconnectButton_clicked(); }
void quiescent_current::refreshMusicState(FacDevInfo data) {
    bool isMusicStateTest = SETTINGS.value("Music/MusicState_checkBox").toBool();
    showlog("当前曲目为：" + QString::number(data.dev_info[0].value_item.music_state));

    if (!isMusicStateTest || SETTINGS.value("Music/MusicState").toInt() == data.dev_info[0].value_item.music_state) {
        TestItem test;
        test.testItem = "曲目测试";
        test.testData = QString::number(data.dev_info[0].value_item.music_state);
        test.testResult = passValue;
        test.ask = QString::number(SETTINGS.value("Music/MusicState").toInt());
        testItems.append(test);

        testResultTableUpdate(testItems);

    } else {
        TestItem test;
        test.testItem = "曲目测试";
        test.testData = QString::number(data.dev_info[0].value_item.music_state);
        test.testResult = failValue;
        test.ask = QString::number(SETTINGS.value("Music/MusicState").toInt());
        testItems.append(test);

        testResultTableUpdate(testItems);

        totalresult = failValue;
        state = STATE_SAVE_RESULT;
        showlog("曲目测试不良");
    }
}
void quiescent_current::refreshBaseData(FacGetDevBaseInfo data) {
    if (refresh_base_times) {
        qDebug() << getIndex() << "refresh_times" << refresh_base_times;
        refresh_base_times = 0;

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
        bool isImuTest = SETTINGS.value("ProductInfo/ImuID_checkBox").toBool();
        bool isCameraTest = SETTINGS.value("ProductInfo/CameraID_checkBox").toBool();
        bool isBleTest = SETTINGS.value("ProductInfo/BluetoothVersion_checkBox").toBool();

        if ((!isAlgoTest || algorithmVersion.contains(data.algo_version)) &&
            (!isHwTest || hardwareVersion.contains(data.hw_version)) &&
            (!isPresureTest || pressureSenseVersion.contains(data.presure_version)) &&
            (!isProductTest || productName.contains(data.product_name)) &&
            (!isAppProtocolTest || appProtocolVersion.contains(QString::number(data.pb_phone_ver))) &&
            (!isFactoryProtocolTest || factoryProtocolVersion.contains(QString::number(data.pb_factory_ver))) &&
            (!isSoftwareTest || softwareVersion.contains(data.soft_version)) &&
            (!isResourceTest || resourceVersion.contains(data.res_version)) &&
            (!isMotorTest || motorVersion.contains(data.motor_version)) &&
            (!isImuTest || imuId.contains(QString::number(data.imu_id))) &&
            (!isBleTest || bleVersion.contains(data.ble_version)) &&
            (!isCameraTest || camera_id.contains(data.camera_version))) {
            base_state = 1;
        } else {
            base_state = 2;
        }

        TestItem test;

        // Check for BLE version
        if (isBleTest) {
            test.testItem = "蓝牙版本";
            test.testData = QString::fromUtf8(data.ble_version);
            test.ask = bleVersion;
            testItems.append(test);
        }

        // Check for Motor version
        if (isMotorTest) {
            test.testItem = "电机版本";
            test.testData = QString::fromUtf8(data.motor_version);
            test.ask = motorVersion;
            testItems.append(test);
        }

        // Check for Resource version
        if (isResourceTest) {
            test.testItem = "资源版本";
            test.testData = QString::fromUtf8(data.res_version);
            test.ask = resourceVersion;
            testItems.append(test);
        }

        // Check for Software version
        if (isSoftwareTest) {
            test.testItem = "软件版本";
            test.testData = QString::fromUtf8(data.soft_version);
            test.ask = softwareVersion;
            testItems.append(test);
        }

        // Check for Product name
        if (isProductTest) {
            test.testItem = "产品名字";
            test.testData = QString::fromUtf8(data.product_name);
            test.ask = productName;
            testItems.append(test);
        }

        // Check for Hardware version
        if (isHwTest) {
            test.testItem = "硬件版本";
            test.testData = QString::fromUtf8(data.hw_version);
            test.ask = hardwareVersion;
            testItems.append(test);
        }

        // Check for Algorithm version
        if (isAlgoTest) {
            test.testItem = "算法版本号";
            test.testData = QString::fromUtf8(data.algo_version);
            test.ask = algorithmVersion;
            testItems.append(test);
        }

        // Check for Pressure version
        if (isPresureTest) {
            test.testItem = "压感版本";
            test.testData = QString::fromUtf8(data.presure_version);
            test.ask = pressureSenseVersion;
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
            test.testData = QString::fromUtf8(data.camera_version);
            test.ask = camera_id;
            testItems.append(test);
        }

        updateTestData(testItems);
    }
}

void quiescent_current::refreshPeriphData(FacGetPeriphState data) {
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

    QString imuStatus = SETTINGS.value("PeripheralStatus/IMU_Status").toString();
    QString flashStatus = SETTINGS.value("PeripheralStatus/Flash_Status").toString();
    QString magneticStatus = SETTINGS.value("PeripheralStatus/Magnetic_Status").toString();
    QString pressureStatus = SETTINGS.value("PeripheralStatus/Pressure_Status").toString();
    QString audioState = SETTINGS.value("PeripheralStatus/Audio_Status").toString();

    // 将布尔值转换为 QString
    QString flashStateStr = QString::number(data.flash_state);
    QString imuStateStr = QString::number(data.imu_state);
    QString audioStateStr = QString::number(data.audio_state);
    QString pressStateStr = QString::number(data.press_state);
    QString magnetStateStr = QString::number(data.magnet_state);

    // 现在可以将 QString 类型的状态用于你的条件判断
    bool checkFlash = SETTINGS.value("PeripheralStatus/FlashStatus_checkBox").toBool();
    bool checkIMU = SETTINGS.value("PeripheralStatus/IMUStatus_checkBox").toBool();
    bool checkAudio = SETTINGS.value("PeripheralStatus/AudioStatus_checkBox").toBool();
    bool checkPressure = SETTINGS.value("PeripheralStatus/PressureStatus_checkBox").toBool();
    bool checkMagnetic = SETTINGS.value("PeripheralStatus/MagneticStatus_checkBox").toBool();

    if ((!checkFlash || flashStatus.contains(flashStateStr)) && (!checkIMU || imuStatus.contains(imuStateStr)) &&
        (!checkAudio || audioState.contains(audioStateStr)) &&
        (!checkPressure || pressureStatus.contains(pressStateStr)) &&
        (!checkMagnetic || magneticStatus.contains(magnetStateStr))) {
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

    updateTestData(testItems);
}

void quiescent_current::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到电流数据" << data;
    double normalValue = 0;
    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    if (pack.factory == "jj")
        normalValue = data.toDouble(&conversionOk) / 100;
    else
        normalValue = data.toDouble(&conversionOk) * 1000;

    if (conversionOk) {
        // 转换成功
        qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
        QString formattedValue = QString::number(normalValue, 'f', 8);
        qDebug() << getIndex() << "转换后的数值：" << formattedValue << "ma";
        // ui->log->appendPlainText(formattedValue+"ma");
        showlog(formattedValue + "ma");
    } else {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

quiescent_current::~quiescent_current() {
    qDebug() << getIndex() << "已进入析构";
    isquiescent_currentContinue = 0;
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

void quiescent_current::refreshSn(FacDevInfo data) {
    // qDebug() << getIndex()<< "qss not load"<<  data.dev_info[0].value_item.board_sn;
    // qDebug() << getIndex()<< "qss not load"<<  data.dev_info[0].value_item.tail_sn;
    // QString board_sn_string = QString::fromUtf8(data.dev_info[0].value_item.board_sn);
    QString tail_sn_string = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    // ui->board_sn->setText("板子sn:"+board_sn_string);
    ui->tail_sn->setText("尾盖sn:" + tail_sn_string);

    ui->snInput->clear();
    ui->macInput->clear();
}

void quiescent_current::on_snInput_returnPressed() {
    clearDisplay();
    macAddress = "没有mac地址";
    logString = "";
    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->snInput->text()).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->getMac->text().length()));
        showlog("要求格式为" + snPattern);
        ui->snInput->clear();
        return;
    }

    emit send_startTest(getIndex());
    ui->macInput->setFocus();

    ui->snInput->setDisabled(1);
    stringsn = ui->snInput->text();
    sn = ui->snInput->text().toUtf8();

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    if (!usbSerialPort->isOpen()) {
        on_usbconnectButton_clicked();
    }

    emit send_go_next_focus();
    processInspection(ui->snInput->text());
    if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
        if (!productSerialPort->isOpen()) {
            openProductSerialPort();
        } else {
            at->ask_mac();
        }
    }

    if (pack.factory == "xwd" && !jigSerialPort->isOpen()) {
        openJigSerialPort();
    }
    jig->set_cylinder_state(1, getIndex());
}
void quiescent_current::on_macInput_returnPressed() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    if (!usbSerialPort->isOpen()) {
        on_usbconnectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        usblogwaittime->stop();
        firstconnectbrush = 0;
        ui->macInput->setDisabled(1);
        macAddress = ui->macInput->text();
        if (1)  //(macAddress != last_macAddress)
        {
            last_macAddress = macAddress;
            ui->macLabel->setText("蓝牙mac: " + macAddress);
            state = STATE_IDLE;
            bandingMacSn(macAddress, stringsn);
        } else {
            state = STATE_SAVE_RESULT;
            totalresult = failValue;
            showlog("mac地址重复,请交叉复测...如复测也报错，机子异常");
        }
        isquiescent_currentContinue = true;
    }
}

void quiescent_current::clearDisplay() {
    ui->msgEdit->clear();
    testResultTableInit();
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->tail_sn->clear();
    ui->tail_sn->setText("尾盖sn:");
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
        pb->set_forbid_sleep(FacSwitch_OPEN);
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
        ui->productConnectButton->setEnabled(true);
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
        ui->jigConnectButton->setEnabled(true);
        showlog("治具串口连接断开");
    }
}

void quiescent_current::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        ui->usbcomNameCombo->setEnabled(true);
        ui->usbconnectButton->setEnabled(true);
        showlog("usb串口连接断开");
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
    openUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(false);
    ui->usbconnectButton->setEnabled(false);
}

void quiescent_current::on_usbdisconnectButton_clicked() {
    closeUsbSerialPort();
    ui->usbcomNameCombo->setEnabled(true);
    ui->usbconnectButton->setEnabled(true);
}

void quiescent_current::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void quiescent_current::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void quiescent_current::on_jigConnectButton_clicked() {
    openJigSerialPort();
    ui->jigComNameCombo->setEnabled(false);
    ui->jigConnectButton->setEnabled(false);
}

void quiescent_current::on_jigDisconnectButton_clicked() {
    closeJigSerialPort();
    ui->jigComNameCombo->setEnabled(true);
    ui->jigConnectButton->setEnabled(true);
}

void quiescent_current::processInspection(QString stringsn) {
    if (stringsn != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = stringsn;
            pack.mechines = getIndex();
            pack.is_hq_send_mac = 0;
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

void quiescent_current::startTask() {
    if (isquiescent_currentContinue) {
        ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state) {
            case STATE_IDLE:  // 复位一切

                if (pack.factory == "hq")
                    usb->sethqMEASure();
                else
                    usb->sendCONF("CURR", "DC", "500e-3");  // 500ma静态电流

                pb->reset_all_pb();
                periph_state = 0;
                base_state = 0;
                isovertime = 0;
                refresh_base_times = 1;
                refresh_periph_times = 1;
                totalresult = "";
                at->resetConnected();
                measure_ammeter = 0;
                at->sendMac(ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());
                showlog("已经发送mac地址");
                TestTime.start();
                state = STATE_WATI_CONNECT;

                break;

            case STATE_WATI_CONNECT:  // 设置禁止休眠
                if (at->getConnected()) {
                    qDebug() << getIndex() << "蓝牙状态" << at->getConnected();
                    waitWork(WAITTIME);
                    showlog("蓝牙连接成功");
                    pb->set_sn(FacDevInfoType_TAIL_SN, sn);
                    state = STATE_BANDING;
                }
                break;

            case STATE_BANDING:
                if (pb->get_is_banding_ok()) {
                    showlog("sn已成功绑定保存");
                    state = STATE_WATI_DISABLE_SLEEP;
                } else {
                    waitWork(500);
                    pb->set_sn(FacDevInfoType_TAIL_SN, sn);
                    showlog("已发送sn绑定");
                }

                break;

            case STATE_WATI_DISABLE_SLEEP:

                if (pb->getDisableSleep()) {
                    showlog("已进入禁止休眠");
                    pb->get_now_music_info();
                    pb->get_base_info();
                    state = STATE_WATI_GET_BASE_STATE;
                } else {
                    waitWork(500);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    showlog("已发送禁止休眠");
                }
                break;

            case STATE_WATI_GET_BASE_STATE:

                if (base_state == 1)  // 基础信息正常
                {
                    waitWork(WAITTIME);
                    showlog("基础信息验证通过");
                    pb->get_periph_state();

                    state = STATE_WATI_GET_PERIPHERAL_STATE;
                }
                if (base_state == 2) {
                    waitWork(WAITTIME);
                    showlog("基础信息验证失败");
                    pb->get_periph_state();
                    QString mesresult = "NG";
                    QString itemvalue = QString("|版本信息:错误|");
                    pack.result = mesresult;

                    pack.itemvalue = itemvalue;

                    pack.sn = ui->snInput->text();

                    pack.instruct_num = "076";
                    pack.itemvalue = pack.sn + "," + macAddress + "," + "STATIC_CURRENT_RESULT*" + pack.result +
                                     QString("@STATIC_CURRENT*0");

                    pack.itemvalue = "base_state=NG";
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }
                    at->sendMac("00:00:00:00:00:00");  // 发送mac地址

                    totalresult = failValue;
                    state = STATE_SAVE_RESULT;
                }

                else {
                    waitWork(500);
                    pb->get_base_info();
                    showlog("正在重发获取基本信息");
                }
                break;

            case STATE_WATI_GET_PERIPHERAL_STATE:
                if (periph_state == 1)  // 设备信息正常
                {
                    showlog("外设状态正常");
                    showlog("正在发送取消静止休眠");
                    pb->set_forbid_sleep(FacSwitch_CLOSE);
                    qDebug() << getIndex() << "禁止休眠开始计时" << QDateTime::currentDateTime();
                    ble_waittime->setInterval(disconnect_wait_time);
                    ble_waittime->start();
                    state = STATE_CLOSE_FORBID_SLEEP;
                }
                if (periph_state == 2)  // 设备信息异常
                {
                    showlog("外设状态异常");
                    QString mesresult = "NG";
                    QString itemvalue = QString("|设备信息:错误");
                    pack.result = mesresult;
                    pack.itemvalue = itemvalue;
                    pack.sn = ui->snInput->text();
                    pack.itemvalue = "periph_state=NG";
                    pack.instruct_num = "076";
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }

                    at->sendMac("00:00:00:00:00:00");  // 发送mac地址

                    totalresult = failValue;
                    state = STATE_SAVE_RESULT;
                }
                if (periph_state == 0) {
                    waitWork(500);
                    pb->get_periph_state();
                    showlog("正在重发获取外设信息");
                }
                break;

            case STATE_CLOSE_FORBID_SLEEP:
                if (pb->get_is_close_forbid_sleep()) {
                    ble_waittime->stop();
                    waittime->stop();
                    waitWork(100);

                    sendCommandWithRetry(std::bind(&Qpb::set_sleeep, pb, FacSwitch_START));

                    state = STATE_SLEEP_OPEN;
                } else {
                    waitWork(500);
                    pb->set_forbid_sleep(FacSwitch_CLOSE);
                    showlog("正在重发取消禁止休眠");
                }

                break;
            case STATE_SLEEP_OPEN:
                if (!at->getConnected() || canGoNext) {
                    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                    waitWork(2000);
                    qDebug() << getIndex() << "开始计时" << QDateTime::currentDateTime();
                    waittime->setInterval(measure_wait_time);
                    waittime->start();
                    state = STATE_SLEEP_CURRENT_TEST;
                }

                break;
            case STATE_SLEEP_CURRENT_TEST:  // 开启休眠模式测试，系统记录休眠模式电流
                if (isovertime && (measure_ammeter < LowCurrent || measure_ammeter > HighCurrent)) {
                    waittime->stop();
                    QString mesresult = "NG";
                    //   QString mesmacAddress = macAddress.replace(":", "").toUpper();

                    QString itemvalue =
                        QString("|BTMAC:%1").arg(macAddress) + QString("|CURRENT:%1|").arg(measure_ammeter);
                    pack.result = mesresult;
                    pack.itemvalue = itemvalue;
                    pack.instruct_num = "076";
                    pack.sn = ui->snInput->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }
                    showlog("静态电流测试失败：超时");
                    showlog("电流测量值为" + QString::number(measure_ammeter));
                    testData = QString::number(measure_ammeter);

                    TestItem test;
                    test.testItem = "静态电流(ma)";
                    test.testData = testData;
                    test.testResult = failValue;
                    test.ask = "通过";
                    testItems.append(test);

                    testResultTableUpdate(testItems);

                    state = STATE_SAVE_RESULT;
                    totalresult = failValue;
                    break;
                }
                if (measure_ammeter > LowCurrent && measure_ammeter < HighCurrent) {
                    QString mesresult = "PASS";
                    //  QString mesmacAddress = macAddress.replace(":", "").toUpper();

                    QString itemvalue =
                        QString("|BTMAC:%1").arg(macAddress) + QString("|CURRENT:%1|").arg(measure_ammeter);
                    pack.result = mesresult;

                    pack.itemvalue = itemvalue;

                    pack.sn = ui->snInput->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }
                    showlog("静态电流正常");
                    testData = QString::number(measure_ammeter);

                    TestItem test;
                    test.testItem = "静态电流(ma)";
                    test.testData = testData;
                    test.testResult = passValue;
                    test.ask = "通过";
                    testItems.append(test);

                    testResultTableUpdate(testItems);

                    waittime->stop();
                    totalresult = passValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    if (pack.factory == "hq") {
                        usb->gethqMEASure();
                        waitWork(1000);
                    }

                    else if (pack.factory == "lx") {
                        usb->getlxMEASure(1);
                        waitWork(1000);
                    } else {
                        usb->getMEASure("");
                        waitWork(100);
                    }
                }
                break;

            case STATE_SAVE_RESULT:
                stringsn = "";
                if (totalresult == passValue) {
                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((totalresult == failValue)) {
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }

                showlog("测试结束");
                ui->macInput->clear();
                ui->snInput->clear();

                isquiescent_currentContinue = false;  // 结束

                if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                    SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                } else {
                    if (pack.factory == "xwd")
                        jig->set_cylinder_state(0, getIndex());
                }

                on_disconnectButton_clicked();
                on_usbdisconnectButton_clicked();
                ui->snInput->setDisabled(0);
                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                emit send_end_test(getIndex());

                state = STATE_IDLE;
                break;
        }
        //   QCoreApplication::processEvents();
    }
}

void quiescent_current::closeEvent(QCloseEvent* event) { isquiescent_currentContinue = false; }

void quiescent_current::on_pushButton_clicked() {
    ui->macInput->setText("f4:12:fa:c5:51:c6");
    // ui->macInput->setText("74:4d:bd:95:7c:be");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    //  ui->macInput->setText("3c:84:27:29:50:32");
    on_macInput_returnPressed();
    // ui->macInput->returnPressed();
    // sendjigData(STATE_CYLINDER_RESET);

    // bandingMacSn(macAddress, stringsn);
    //     save_brush_log("dataTemp");
}

void quiescent_current::on_pushButton_3_clicked() {
    if (pack.factory == "hq")
        usb->gethqMEASure();
    else
        usb->getMEASure("");

    // at->ask_mac();
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
            out << line << endl;
        }
        file.close();  // 关闭文件
        showlog("保存mac_sn文件成功");
    } else {
        showlog("保存mac_sn文件失败");
    }

    // if (pack.factory == "hq" )

    //     QFile
    //     file("ftp:\\101003931@172.19.250.89/MD1%B9%A4%B3%CC%B6%FE%BF%C6/PE/P20%BE%B2%CC%AC%B5%E7%C1%F7%B1%A3%B4%E6MAC/mac_sn.txt");
    //     // 创建一个文件对象 QString ftpUrl =
    //     "ftp://101003931@172.19.250.89/MD1%B9%A4%B3%CC%B6%FE%BF%C6/PE/P20saveMAC/mac_sn.txt"; //
    //     FTP 服务器上的文件路径 QString data = bandingsn + "," + bandingmac;         //
    //     要上传的数据

    //     QUrl ftpQUrl(ftpUrl);
    //     QNetworkRequest request(ftpQUrl);

    //     request.setSslConfiguration(QSslConfiguration::defaultConfiguration());

    //     QNetworkAccessManager nam;
    //     QObject::connect(&nam, &QNetworkAccessManager::authenticationRequired, [](QNetworkReply
    //     *reply, QAuthenticator *authenticator) {
    //         authenticator->setUser("101003931");  // 添加 FTP 服务器的用户名
    //         authenticator->setPassword("mcl45609");  // 添加 FTP 服务器的密码
    //     });

    //     QNetworkReply *reply = nam.put(request, data.toUtf8());

    //     QObject::connect(reply, &QNetworkReply::finished, [&]() {

    //     });

    //     // 创建事件循环
    //     QEventLoop loop;

    //     // 连接请求完成信号和事件循环退出
    //     QObject::connect(reply, &QNetworkReply::finished,
    //                      [&loop, reply, ui = this->ui]()
    //                      {
    //         if (reply->error() == QNetworkReply::NoError) {
    //             qDebug() << "Data uploaded successfully";
    //         } else {
    //             qDebug() << "Error: " << reply->errorString();
    //         }
    //         reply->deleteLater();
    //                          // 退出事件循环
    //                          loop.quit();
    //                      });

    //     // 开始事件循环，等待请求完成
    //     loop.exec();
    // #else
    // QFile file("mac_sn.txt");   // 创建一个文件对象
    // if (file.open(QIODevice::ReadWrite | QIODevice::Append))
    // {                                                    //
    //     QTextStream out(&file);                          // 创建一个文本流对象
    //     out << bandingsn << "," << bandingmac << endl;   // 将sn和mac写入文件
    //     file.close();                                    // 关闭文件
    // }else{

    //     showlog("保存mac_sn文件失败");
    // }
    // #endif
}

void quiescent_current::on_stopTest_clicked() {
    usblogwaittime->stop();
    ui->macInput->clear();
    ui->snInput->clear();
    ui->snInput->setFocus();
    isquiescent_currentContinue = false;  // 结束
    if (pack.factory != "jj") {
        jig->set_cylinder_state(0, getIndex());
    }
    ui->snInput->setDisabled(0);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);
}
