#include "pcbaform.h"

#include <QMessageBox>
#include <QSerialPortInfo>

#include "qdebug.h"
#include "ui_pcbaform.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif
void PcbaForm::on_pushButton_clicked() {
    // ui->macInput->setText("f4:12:fa:c5:51:c6");  // 测试牙刷
    // //  ui->macInput->setText("74:4D:BD:95:7A:DA");//su牙刷
    // // ui->macInput->setText("74:4D:BD:95:7B:E6");//金鹏牙刷
    // // ui->macInput->setText("74:4D:BD:95:7B:F6");//金鹏牙刷
    // // ui->macInput->setText("74:4D:BD:95:7C:BE");//zhangmeng牙刷
    // //   ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    // ui->macInput->setText("3C:84:27:07:A8:D2");
    // // ui->macInput->setText("e2:66:07:34:2d:f7");
    // // ui->macInput->setText("3c:84:27:29:50:32");
    // ui->macInput->setText("b4:56:5d:bf:54:4e");
    // ui->macInput->setText("3C:84:27:07:A8:D2");

    // on_macInput_returnPressed();
    // at->sendMac(ui->macInput->text());  // 发送mac地址

    testData = "连接超时";
    TestItem test;
    test.testItem = "wifi连接";
    test.testData = testData;
    test.testResult = failValue;
    test.ask = "通过";
    testItems.append(test);

    testResultTableUpdate(testItems);

    erroContent << "|WIFI连接:超时";

    testData = "连接不行";
    test.testItem = "wif劳烦";
    test.testData = testData;
    test.testResult = failValue;
    test.ask = "通过";
    testItems.append(test);

    testResultTableUpdate(testItems);

    erroContent << "|WIFI测试:失败";
}
void PcbaForm::on_pushButton_2_clicked()  // 单机
{
    static int t = 1;
    if (t == 1) {
        pack.itemvalue = "";
        pack.error = erroContent.join("") + "|";  //加结束符
        pack.sn = ui->getMac->text();
        showlog("失败过站");
        if (ui->isusemes->checkState()) {
            send_end_testPass(pack);
        }
        t = 0;
    } else {
        pack.itemvalue = exportTableContent();
        pack.error = "";
        pack.sn = ui->getMac->text();
        showlog("正常过站");

        if (ui->isusemes->checkState()) {
            send_end_testPass(pack);
        }
        t = 1;
    }
    // at->sendBLELOG(1);
    // testItem = "六轴状态";
    // testData = "QString::number(data.imu_state)";

    // pb->set_forbid_sleep(FacSwitch_CLOSE);
    // pb->set_sleeep(FacSwitch_OPEN);
    // waitWork(100);
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(50);
    // on_disconnectButton_clicked();
    // // 3C:84:27:07:A8:D2

    // showlog("已发送休眠");

    // ui->ng_number->setText("NG:"+QString::number(ngnumber));
    // ui->ng_number->setStyleSheet("font-size: 15px; background-color: #FF0000; color: black;
    // border: 2px solid #FF0000; border-radius: 10px; padding: 1px; text-align: center; ");

    //  pb->set_wifi_disconnect();
    // pb->set_device_mode();//进入亮白
    // waitWork(500);
    // pb->set_brush_control(1);
    //  pb->set_ship_mode(1);

    //  pb->set_led_color(1);
    //  pb->get_battery();
    //  pb->set_sleeep(FacSwitch_OPEN);
    // qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<< " getIndex()"<<
    // getIndex();
    //     emit overtask(getIndex());

    // pb->get_periph_state();

    // pb->set_wifi_disconnect();
    // pb->set_fac_mode(1);
    // emit overtask(getIndex());

    //  connectwifi();
    // waitWork(WAITTIME);
    // static int t=1;
    // if(t==1){
    //  //  pb->set_device_mode();//进入亮白
    //       connectwifi();
    //     t=0;
    // }else{
    //    pb->set_wifi_disconnect();
    //     t=1;
    // }

    // pb->set_forbid_sleep(FacSwitch_CLOSE);
    // waitWork(WAITTIME);
    // pb->set_sleeep(FacSwitch_OPEN);
}

PcbaForm::PcbaForm(int index, QWidget* parent) : ui(new Ui::PcbaForm) {
    m_index = index;
    pack.mechines = getIndex();
    //  dongleBaudRate = 115200;
    dongleOutTime = 1;  // 太快会死锁
    upperComputerVer = PCBA_VER;

    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();  // 要搜索一下一开始
    testResultTableInit();

    ui->ng_number->setText("NG:" + QString::number(0));
    ui->ng_number->setStyleSheet("font-size: 15px; background-color: #ff557f; color: black; border: 2px solid #FF0000; "
                                 "border-radius: 10px; padding: 1px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 20px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    ui->pass_number->setText("PASS:" + QString::number(0));
    ui->pass_number->setStyleSheet("font-size: 15px; background-color: #00FF00; color: black; border: 2px solid "
                                   "#00FF00; border-radius: 10px; padding: 1px; text-align: center;");

    ui->mechine_number->setText(QString::number(getIndex()) + "号机");
    ui->mechine_number->setStyleSheet("font-size: 30px; background-color: yellow; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    connect(usblogwaittime, &QTimer::timeout, [=]() {
        if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
            at->ask_mac();
            showlog("正在定时器复位牙刷");
        }
    });

    connect(blewaittime, &QTimer::timeout, [=]() {
        isbleovertime = 1;
        blewaittime->stop();
    });

    connect(wifiwaittime, &QTimer::timeout, [=]() {
        iswifiovertime = 1;
        wifiwaittime->stop();
    });

    connect(wificonnectwaittime, &QTimer::timeout, [=]() {
        iswificonnectovertime = 1;
        wificonnectwaittime->stop();
    });

    connect(wificonnectwaittimehalf, &QTimer::timeout, [=]() {
        connectwifi();
        wificonnectwaittimehalf->stop();
    });

    connect(music_play_time, &QTimer::timeout, [=]() {
        is_music_play_over_time = 1;
        music_play_time->stop();
    });

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    // ui->wifiUserName->setText(SETTINGS.value("WIFI/Name", "请在配置文件中设置").toString());
    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "usmile123").toString());
    HighRssi = SETTINGS.value("WIFI/HighRssi").toInt();
    LowRssi = SETTINGS.value("WIFI/LowRssi").toInt();
    BleHighRssi = SETTINGS.value("BLE/HighRssi").toInt();
    BleLowRssi = SETTINGS.value("BLE/LowRssi").toInt();
    ble_wait_time = SETTINGS.value("BLE/BleWaitTime", "15000").toInt();
    wifi_wait_time = SETTINGS.value("WIFI/WifiWaitTime", "15000").toInt();
    wifi_connect_waittime = SETTINGS.value("BLE/WifiConnectWaitTime", "15000").toInt();
    HighCharCurrent = SETTINGS.value("Current/HighCharCurrent").toDouble();
    LowCharCurrent = SETTINGS.value("Current/LowCharCurrent").toDouble();
    HighworkCurrent = SETTINGS.value("Current/HighworkCurrent").toDouble();
    LowworkCurrent = SETTINGS.value("Current/LowworkCurrent").toDouble();

    HighmusicCurrent = SETTINGS.value("Current/HighmusicCurrent").toDouble();
    LowmusicCurrent = SETTINGS.value("Current/LowmusicCurrent").toDouble();

    HighshipCurrent = SETTINGS.value("Current/HighshipCurrent").toDouble();
    LowshipCurrent = SETTINGS.value("Current/LowshipCurrent").toDouble();

    HighstaticCurrent = SETTINGS.value("Current/HighstaticCurrent").toDouble();
    LowstaticCurrent = SETTINGS.value("Current/LowstaticCurrent").toDouble();
    music_state = SETTINGS.value("FIXTEST/MusicState").toInt();
    overVoltageLight = SETTINGS.value("FIXTEST/OverVoltageLight").toInt();
    button1 = SETTINGS.value("FIXTEST/Button1").toInt();
    button2 = SETTINGS.value("FIXTEST/Button2").toInt();
    RssiTestTime = SETTINGS.value("BLE/RssiCount").toInt();
    acc_x_up = SETTINGS.value("IMU/acc_x_up").toInt();
    acc_x_down = SETTINGS.value("IMU/acc_x_down").toInt();

    acc_y_up = SETTINGS.value("IMU/acc_y_up").toInt();
    acc_y_down = SETTINGS.value("IMU/acc_y_down").toInt();

    acc_z_up = SETTINGS.value("IMU/acc_z_up").toInt();
    acc_z_down = SETTINGS.value("IMU/acc_z_down").toInt();

    showlog("acc_z_up=" + QString::number(acc_z_up));
    showlog("acc_z_down=" + QString::number(acc_z_down));

    showlog("MusicState=" + QString::number(music_state));
    showlog("OverVoltageLight=" + QString::number(overVoltageLight));
    showlog("Button1=" + QString::number(button1));
    showlog("Button2=" + QString::number(button2));
    showlog("ble_wait_time=" + QString::number(ble_wait_time));
    showlog("wifi_wait_time=" + QString::number(wifi_wait_time));
    showlog("wifi_connect_waittime=" + QString::number(wifi_connect_waittime));
    showlog("wifiUserName=" + SETTINGS.value("WIFI/Name", "请在配置文件中设置").toString());
    showlog("wifiPassword=" + SETTINGS.value("WIFI/Password", "usmile123").toString());
    showlog("HighRssi=" + QString::number(HighRssi));
    showlog("LowRssi=" + QString::number(LowRssi));
    showlog("BleHighRssi=" + QString::number(BleHighRssi));
    showlog("BleLowRssi=" + QString::number(BleLowRssi));
    showlog("HighCharCurrent=" + QString::number(HighCharCurrent));
    showlog("LowCharCurrent=" + QString::number(LowCharCurrent));
    showlog("HighworkCurrent=" + QString::number(HighworkCurrent));
    showlog("LowworkCurrent=" + QString::number(LowworkCurrent));
    showlog("HighmusicCurrent=" + QString::number(HighmusicCurrent));
    showlog("LowmusicCurrent=" + QString::number(LowmusicCurrent));
    showlog("HighshipCurrent=" + QString::number(HighshipCurrent));
    showlog("LowshipCurrent=" + QString::number(LowshipCurrent));
    showlog("HighstaticCurrent=" + QString::number(HighstaticCurrent));
    showlog("LowstaticCurrent=" + QString::number(LowstaticCurrent));
    showlog("RssiTestTime=" + QString::number(RssiTestTime));
    showlog("machineNo=" + pack.machineNo);
    showlog("test_station=" + pack.test_station);

    if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool()) {
        ui->productDisconnectButton->setEnabled(1);
        ui->productConnectButton->setEnabled(1);
        ui->productComNameCombo->setEnabled(1);
    } else {
        ui->productDisconnectButton->setEnabled(0);
        ui->productConnectButton->setEnabled(0);
        ui->productComNameCombo->setEnabled(0);
    }

    if (pack.factory == "wks") {
        ui->getMac->setEnabled(1);
    } else {
        ui->getMac->setEnabled(0);
        ui->isusemes->setChecked(0);
        ui->isformmes->setChecked(0);
    }

    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool())
        ui->start_scan->setVisible(true);  // 隐藏按钮
    else
        ui->start_scan->setVisible(false);  // 隐藏按钮
}
void PcbaForm::getDongleVer(QString data) { showlog("当前dongle的版本为：" + data); }
void PcbaForm::getDongleWifi(QString data) {
    showlog("获取到了wifi名字" + data);

    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
}

void PcbaForm::processReceivedData(const QByteArray& data) {
    static QString logString = "";

    // 将接收到的数据添加到日志字符串中
    logString += data;

    logString.replace("\n", "").replace("\r", "");
    // showlog(logString);
    // 查找日志中最后一个 "Local Board" 条目的索引
    int lastIndex = logString.lastIndexOf("Local Board");

    if (lastIndex != -1) {
        // 从最后一个 "Local Board" 条目开始查找 MAC 地址
        int macIndex = logString.indexOf('=', lastIndex);

        if (macIndex != -1 &&
            logString.length() >= macIndex + 18) {  // 判断是否包含完整的 MAC 地址（17 个字符 + 1 个分隔符）
            // 提取 MAC 地址
            QString macAddress = logString.mid(macIndex + 1, 17).trimmed();  // MAC 地址长度为17，并去除首尾空格
            qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                     << "提取到的MAC地址：" << macAddress;

            // 去掉 MAC 地址中的冒号
            QString macWithoutColons = macAddress;
            macWithoutColons.remove(':');

            // 检查除冒号外的所有字符是否为 '0'
            bool isAllZero = true;
            for (QChar ch : macWithoutColons) {
                if (ch != '0') {
                    isAllZero = false;
                    break;
                }
            }

            // 如果 MAC 地址不全为 '0'，继续后续处理逻辑
            if (isAllZero) {
                // 后续处理逻辑
                showlog("日志数据中MAC地址全0,取消本数据");
                logString = "";

                return;
            }

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
            at->ask_mac();
            showlog("日志数据中未找到完整的MAC地址，正在重发请求获取mac地址");
        }
    } else {
        //  qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<<
        //  "日志数据中未找到mac地址关键字。";
    }
}

void PcbaForm::refreshAmmeterData(QString data) {
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "收到电流数据" << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk);

    if (conversionOk) {
        // 转换成功
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
        QString formattedValue = QString::number(normalValue, 'f', 8);
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "转换后的数值：" << formattedValue << "ma";
    } else {
        // 转换失败
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "无法将字符串转换为 double 类型";
    }
}

void PcbaForm::getWifiMsg(QString data) {
    // qDebug() << getIndex()<< "收到wifi数据为" << data;
    QStringList parts = data.split("-");
    int numPairs = parts.size() / 2;
    for (int i = 0; i < numPairs; ++i) {
        QString macAddress = parts[i * 2];
        QString rssi = "-" + parts[i * 2 + 1];
        wifiMac = wifiMac.toUpper();
        // qDebug() << getIndex() << "dongle的的wifiMac:" << macAddress;
        // qDebug() << getIndex() << "RSSI:" << rssi;
        // qDebug() << getIndex() << " 牙刷的wifiMac:" << wifiMac;
        if (macAddress == wifiMac) {
            ui->WIFI_RSSI->setText("WIFI的RSSI：" + rssi);
            // qDebug() << getIndex()<< getIndex() << " 比对成功";
            refreshWifiState(1);
            WIFI_RSSI = rssi;

            bool ok;
            WIFI_RSSI.toInt(&ok);

            if (!ok) {
                qDebug() << "转换WIFIrssi失败,内容为" + WIFI_RSSI + "内容结束" << data;
            } else {
                //  showlog("转换成功");
                intwifirssi = WIFI_RSSI.toInt(&ok);
            }
        }
    }
}
void PcbaForm::refreshUsbUartState(int state) {
    if (state)
        ui->usbuartstate->setText("usb串口连接：<font color='green'>成功</font>");
    else
        ui->usbuartstate->setText("usb串口连接：<font color='red'>断开</font>");
}
void PcbaForm::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void PcbaForm::refreshWifiState(int state) {
    if (state) {
        ui->WIFIStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        // showlog("WIFI连接成功");
    } else {
        ui->WIFIStatusLabel->setText("WIFI连接：<font color='red'>断开</font>");
        //  showlog("WIFI连接断开");
    }
}
void PcbaForm::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        showlog("蓝牙连接成功");
        pb->set_forbid_sleep(FacSwitch_OPEN);
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>断开</font>");
        qDebug() << "蓝牙连接断开";

        // showlog("蓝牙连接断开");
    }
}

void PcbaForm::refreshBleRssi(QString data) {
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
void PcbaForm::checkLedControlState(FacLedControl data) {
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "收到led控制状态:" << data.switch_state;
    isledcontrol = 1;
}

void PcbaForm::checkBrushControlState(FacBrushControl data) {
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "收到牙刷控制状态:" << data.value_item.brush_start;
    isbrushcontrol = 1;
}
void PcbaForm::checkbutton(FacButtonState data) {
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "获取到开关按键状态" << data.button_state[0].command_data.power_button.button_state_now;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "获取到模式按键状态" << data.button_state[1].command_data.mode_button.button_state_now;

    testData = QString::number(static_cast<int>(data.button_state[0].command_data.power_button.button_state_now));
    if (testData == "2") {
        TestItem test;
        test.testItem = "电源按键";
        test.testData = "2";
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        isbuttonTest = 1;
    }
}
PcbaForm::~PcbaForm() {
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "PcbaFormd的析构";
    delete ui;
}
void PcbaForm::closeEvent(QCloseEvent*) {
    isPcbaTestContinue = false;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "已经关闭" << isPcbaTestContinue;
}
void PcbaForm::refreshBaseData(FacGetDevBaseInfo data) {
    if (refresh_times) {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "refresh_times" << refresh_times;
        refresh_times = 0;

        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "algo_version" << data.algo_version;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "hw_version" << data.hw_version;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "presure_version" << data.presure_version;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "product_name" << data.product_name;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "pb_phone_ver" << data.pb_phone_ver;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "pb_factory_ver" << data.pb_factory_ver;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "soft_version" << QString("%1").arg(data.soft_version);
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "res_version" << data.res_version;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "imu_id" << data.imu_id;
        // qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
        //          << "imu_id" << data.imu_id;

        QString mac;
        mac.clear();
        for (int var = 0; var < data.ble_mac.size; ++var) {
            mac += QString::number(data.ble_mac.bytes[var], 16);
            if (var < data.ble_mac.size - 1) {
                mac += ":";
            }
        }

        // 反转MAC地址

        for (int i = mac.size(); i > 0; i -= 3) {
            bleMac += mac.mid(i - 2, 2);
            if (i > 2) {
                bleMac += ":";
            }
        }

        wifiMac.clear();
        for (int var = 0; var < data.wifi_mac.size; ++var) {
            wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
            if (var < data.wifi_mac.size - 1)
                wifiMac += ":";
        }
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "设备的 wifiMac:" << wifiMac;

        QString productName = SETTINGS.value("ProductInfo/Product_Name").toString();
        QString appProtocolVersion = SETTINGS.value("ProductInfo/App_Protocol_Version").toString();
        QString factoryProtocolVersion = SETTINGS.value("ProductInfo/Factory_Protocol_Version").toString();
        QString hardwareVersion = SETTINGS.value("ProductInfo/Hardware_Version").toString();
        QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
        QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();
        QString algorithmVersion = SETTINGS.value("ProductInfo/Algorithm_Version").toString();
        QString pressureSenseVersion = SETTINGS.value("ProductInfo/Pressure_Sense_Version").toString();
        QString imuId = SETTINGS.value("ProductInfo/IMU_ID").toString();
        QString ble_ver = SETTINGS.value("ProductInfo/Ble_Ver").toString();
        QString motorVersion = SETTINGS.value("ProductInfo/Motor_Ver").toString();
        QString camera_id = SETTINGS.value("ProductInfo/Camera_Id").toString();

        if (algorithmVersion.contains(data.algo_version) && hardwareVersion.contains(data.hw_version) &&
            pressureSenseVersion.contains(data.presure_version) && productName.contains(data.product_name) &&
            appProtocolVersion.contains(QString::number(data.pb_phone_ver)) &&
            factoryProtocolVersion.contains(QString::number(data.pb_factory_ver)) &&
            softwareVersion.contains(data.soft_version) && resourceVersion.contains(data.res_version) &&
            motorVersion.contains(data.motor_version) && imuId.contains(QString::number(data.imu_id)) &&
            ble_ver.contains(data.ble_version) && camera_id.contains(data.camera_version)) {
            base_state = 1;
        } else {
            base_state = 2;
        }

        TestItem test;

        test.testItem = "蓝牙版本";
        test.testData = QString::fromUtf8(data.ble_version);
        test.ask = ble_ver;
        testItems.append(test);

        test.testItem = "电机版本";
        test.testData = QString::fromUtf8(data.motor_version);
        test.ask = motorVersion;
        testItems.append(test);

        test.testItem = "资源版本";
        test.testData = QString::fromUtf8(data.res_version);
        test.ask = resourceVersion;
        testItems.append(test);
        test.testItem = "软件版本";
        test.testData = QString::fromUtf8(data.soft_version);
        test.ask = softwareVersion;
        testItems.append(test);

        test.testItem = "产品名字";
        test.testData = QString::fromUtf8(data.product_name);
        test.ask = productName;
        testItems.append(test);
        test.testItem = "硬件版本";
        test.testData = QString::fromUtf8(data.hw_version);
        test.ask = hardwareVersion;
        testItems.append(test);

        test.testItem = "算法版本号";
        test.testData = QString::fromUtf8(data.algo_version);
        test.ask = algorithmVersion;
        testItems.append(test);

        test.testItem = "压感版本";
        test.testData = QString::fromUtf8(data.presure_version);
        test.ask = pressureSenseVersion;
        testItems.append(test);

        test.testItem = "六轴id";
        test.testData = QString::number(data.imu_id);
        test.ask = imuId;
        testItems.append(test);

        test.testItem = "APP的pb版本";
        test.testData = QString("%1").arg(data.pb_phone_ver);
        test.ask = appProtocolVersion;
        testItems.append(test);

        test.testItem = "工厂的pb版本";
        test.testData = QString("%1").arg(data.pb_factory_ver);
        test.ask = factoryProtocolVersion;
        testItems.append(test);

        test.testItem = "摄像头id";
        test.testData = QString::fromUtf8(data.camera_version);
        test.ask = camera_id;
        testItems.append(test);

        updateTestData(testItems);
    }
}

void PcbaForm::refreshPeriphData(FacGetPeriphState data) {
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

    if (refresh_periph_state_times) {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "refresh_periph_state_times" << refresh_periph_state_times;
        refresh_periph_state_times = 0;

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
        if (flashStatus.contains(flashStateStr) && imuStatus.contains(imuStateStr) &&
            audioState.contains(audioStateStr) && pressureStatus.contains(pressStateStr) &&
            magneticStatus.contains(magnetStateStr)) {
            periph_state = 1;
        } else {
            periph_state = 2;
        }

        TestItem test;

        test.testItem = "功放状态";
        test.testData = QString::number(data.audio_state);
        test.ask = audioState;
        testItems.append(test);

        test.testItem = "内存状态";
        test.testData = QString::number(data.flash_state);
        test.ask = flashStatus;
        testItems.append(test);

        test.testItem = "六轴状态";
        test.testData = QString::number(data.imu_state);
        test.ask = imuStatus;
        testItems.append(test);

        if (SETTINGS.value("SYSTEM/MagneticReuseMotorStatus").toBool())
            test.testItem = "马达状态";
        else
            test.testItem = "地磁状态";
        test.testData = QString::number(data.magnet_state);
        test.ask = magneticStatus;
        testItems.append(test);

        test.testItem = "压感状态";
        test.testData = QString::number(data.press_state);
        test.ask = pressureStatus;
        testItems.append(test);

        updateTestData(testItems);
    }
}

void PcbaForm::on_stopTest_clicked() {
    usblogwaittime->stop();
    isPcbaTestContinue = false;
    on_disconnectButton_clicked();

    if (SETTINGS.value("SYSTEM/SerialPortMAC").toBool())
        on_productDisconnectButton_clicked();

    ui->macInput->setDisabled(0);
    if (pack.factory == "wks") {
        ui->getMac->setDisabled(0);
    }
}

void PcbaForm::connectwifi() {
    QString wifiName = SETTINGS.value(QString("WIFI/Name%1").arg(getIndex())).toString();
    QString wifiPassword = SETTINGS.value("WIFI/Password").toString();

    // QString wifiName = SETTINGS.value("WIFI/Name").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected()) {
        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void PcbaForm::clearDisplay() {
    ui->msgEdit->clear();
    testResultTableInit();
    ui->log->clear();
}

void PcbaForm::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void PcbaForm::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void PcbaForm::overTask() { on_stopTest_clicked(); }

void PcbaForm::on_connectButton_clicked() {
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void PcbaForm::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}

void PcbaForm::get_remain_data_sleep(const FixturePacketData packetData) {
    showlog("已收到治具请求休眠命令" + QString::number(packetData.machineNumber));

    if (packetData.machineNumber == getIndex())

        if (packetData.sleep == 1) {
            qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                     << "开始休眠" << getIndex();
            start_sleep = 1;
            showlog("已收到治具请求休眠命令");
        }
}

void PcbaForm::get_remain_data(const FixturePacketData packetData) {
    // 验证收到的数据是否属于当前 PCBA
    if (packetData.machineNumber != getIndex())
        return;

    logPacketData(packetData);  // 记录收到的数据

    QList<TestItem> testItems;

    // 根据产品类型进行特定测试
    if (SETTINGS.value("SYSTEM/TestShippingCurrent").toBool()) {
        processTestItem("船运电流", packetData.shipCurrent, LowshipCurrent, HighshipCurrent, testItems);
    }

    if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool()) {
        processTestItem("音频电流", packetData.musicCurrent, LowmusicCurrent, HighmusicCurrent, testItems);
    } else {
        processSimpleTestItem("音频测试", packetData.music_state, music_state, testItems);
    }

    // 处理过压灯和按键测试
    processSimpleTestItem("过压灯测试", packetData.overVoltageLight, overVoltageLight, testItems);
    processSimpleTestItem("按键1", packetData.button1, button1, testItems);
    processSimpleTestItem("按键2", packetData.button2, button2, testItems);

    // 处理静态电流和工作电流测试
    processTestItem("静态电流测试", packetData.staticCurrent, LowstaticCurrent, HighstaticCurrent, testItems);
    processTestItem("工作电流测试", packetData.workingCurrent, LowworkCurrent, HighworkCurrent, testItems);
    processTestItem("充电电流测试", packetData.chargingCurrent, LowCharCurrent, HighCharCurrent, testItems);

    // 保存测试结果并更新UI
    QVector<TestItem> vectorTestItems = QVector<TestItem>::fromList(testItems);  // 将 QList 转换为 QVector
    testResultTableUpdate(vectorTestItems);                                      // 传递 QVector

    // 根据测试结果更新UI状态
    updateTestResultUI();

    showlog("流程结束");
}

void PcbaForm::processTestItem(const QString& testItem, int currentValue, int lowValue, int highValue,
                               QList<TestItem>& testItems) {
    TestItem test;
    test.testItem = testItem;
    test.testData = QString::number(currentValue);
    test.ask = "通过";

    if (currentValue >= lowValue && currentValue <= highValue) {
        test.testResult = "通过";
    } else {
        test.testResult = "失败";
        totalresult = failValue;
        erroContent << QString("|治具测试:失败");
    }

    testItems.append(test);
}

void PcbaForm::processSimpleTestItem(const QString& testItem, int currentValue, int expectedValue,
                                     QList<TestItem>& testItems) {
    TestItem test;
    test.testItem = testItem;
    test.testData = QString::number(currentValue);
    test.ask = QString::number(expectedValue);

    if (currentValue != expectedValue) {
        test.testResult = "失败";
        totalresult = failValue;
        erroContent << QString("|治具测试:失败");
    } else {
        test.testResult = "通过";
    }

    testItems.append(test);
}

void PcbaForm::updateTestResultUI() {
    if (totalresult == failValue) {
        remain_ok = 2;
        ui->ng_number->setText("NG:" + QString::number(ngnumber));
        ui->ng_number->setStyleSheet("font-size: 15px; background-color: #ff557f; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 1px; text-align: center;");
        ui->test_result->setText("FAIL");
        ui->test_result->setStyleSheet("font-size: 40px; background-color: #FF0000; color: black; border: 2px solid "
                                       "#FF0000; border-radius: 10px; padding: 10px; text-align: center;");

        isPcbaTestContinue = false;  // 结束
        ui->getMac->clear();
        ui->macInput->setDisabled(0);
        if (pack.factory == "wks") {
            ui->getMac->setDisabled(0);
        }
        on_disconnectButton_clicked();

        emit send_end_test(getIndex());

    } else {
        remain_ok = 1;
    }

    showlog("remain_ok=" + QString::number(remain_ok));
}

void PcbaForm::logPacketData(const FixturePacketData& packetData) {
    showlog("机号:" + QString(packetData.machineNumber));
    showlog("静态电流值:" + QString::number(packetData.staticCurrent) + "ua");
    showlog("工作电流:" + QString::number(packetData.workingCurrent) + "ma");
    showlog("过压灯正常:" + QString(packetData.overVoltageLight ? "是" : "否"));
    showlog("按键1:" + QString(packetData.button1 ? "按下" : "松开"));
    showlog("按键2:" + QString(packetData.button2 ? "按下" : "松开"));
    showlog("充电电流:" + QString::number(packetData.chargingCurrent) + "ma");
    showlog("船运电流:" + QString::number(packetData.shipCurrent) + "ua");
    showlog("音频电流:" + QString::number(packetData.musicCurrent) + "ma");
    showlog("产品为:" + QString(pack.product));
}

void PcbaForm::getimuData(FacUploadNineAlex x) {
    for (int i = 0; i < x.data_count; i++) {
        // 处理 IMU 数据
        really_accx = processIMUData(x.data[i].acc_x);
        really_accy = processIMUData(x.data[i].acc_y);
        really_accz = processIMUData(x.data[i].acc_z);

        // 打印实际读取到的加速度数据
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "实际 acc_x:" << really_accx << "实际 acc_y:" << really_accy << "实际 acc_z:" << really_accz;
    }
    // 检查并记录 IMU 数据
    checkIMUData("acc_x", really_accx, acc_x_up, acc_x_down);
    checkIMUData("acc_y", really_accy, acc_y_up, acc_y_down);
    checkIMUData("acc_z", really_accz, acc_z_up, acc_z_down);
}

int32_t PcbaForm::processIMUData(uint16_t imuData) {
    if (imuData & 0x8000) {                                 // 判断最高位是否为 1，表示负数
        return static_cast<int32_t>(imuData | 0xFFFF0000);  // 扩展为 32 位的负数
    } else {
        return static_cast<int32_t>(imuData);  // 保持正数不变
    }
}

void PcbaForm::checkIMUData(const QString& axis, int32_t value, int32_t upper, int32_t lower) {
    if (value <= upper && value >= lower) {
        is_imu_correct_data = 1;
    } else {
        state = STATE_WATI_STOP_IMU_DATA;

        pb->set_imu_collect_param(FacSwitch_STOP);
        totalresult = failValue;

        TestItem test;
        test.testItem = "六轴数据";
        test.testData = QString("六轴数据异常: %1 超出范围. 实际值: %2， 范围: [%3， %4]")
                            .arg(axis)
                            .arg(value)
                            .arg(lower)
                            .arg(upper);
        test.testResult = failValue;
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        is_imu_correct_data = 2;

        showlog(QString("六轴数据异常: %1 超出范围. 实际值: %2，范围: [%3， %4]")
                    .arg(axis)
                    .arg(value)
                    .arg(lower)
                    .arg(upper));

        erroContent << "|六轴数据:异常";
    }
}

void PcbaForm::startTask() {
    // QMessageBox::StandardButton reply;
    uint32_t value = 0;
    if (isPcbaTestContinue) {
        ui->test_time->display(TestTime.elapsed() / 1000);
        //   qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<<
        //   "状态机"<<getIndex();
        switch (state) {
            case STATE_IDLE:  // 复位一切

                showlog("状态机" + QString::number(getIndex()));
                ui->test_result->setText("WAIT");
                ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                               "border-radius: 10px; padding: 10px; text-align: center; ");
                pb->reset_all_pb();
                refreshWifiState(0);
                erroContent.clear();

                periph_state = 0;
                rssitestfailcount = 0;
                intblerssi = 0;
                start_sleep = 0;
                TestTime.start();
                refresh_times = 1;
                intwifirssi = 0;
                refresh_periph_state_times = 1;
                base_state = 0;
                sendcount = 0;
                rssitestcount = 0;
                really_accx = 0;
                really_accy = 0;
                really_accz = 0;
                isbuttonTest = 0;
                isbrushcontrol = 0;
                remain_ok = 0;
                isbleovertime = 0;   // 是否开始发送校验结果
                iswifiovertime = 0;  // 是否开始发送校验结果
                is_imu_correct_data = 0;
                iswificonnectovertime = 0;
                is_music_play_over_time = 0;
                at->resetConnected();
                waitWork(500);                      // 用于esp32启动等待时间
                at->sendMac(ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());
                totalresult = passValue;
                state = STATE_WATI_CONNECT;

                break;

            case STATE_WATI_CONNECT:  // 设置禁止休眠
                if (at->getConnected()) {
                    state = STATE_WATI_DISABLE_SLEEP;
                }
                break;

            case STATE_WATI_DISABLE_SLEEP:

                if (pb->getDisableSleep()) {
                    showlog("已进入禁止休眠");
                    pb->set_fac_mode(1);
                    sendCommandWithRetry(std::bind(&Qpb::set_imu_collect_param, pb, FacSwitch_START));

                    state = STATE_WATI_CORRECT_IMU_DATA;
                } else {
                    waitWork(500);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                }
                break;

            case STATE_WATI_CORRECT_IMU_DATA:  // 获取准确inu数据
                if (is_imu_correct_data == 1) {
                    showlog("开始关闭六轴开关，当前状态为" + QString::number(pb->getis_imu_set_sta()));
                    showlog("六轴数据正常");
                    showlog("1、设备信息测试");

                    QString testData =
                        QString("acc_x: %1\nacc_y: %2\nacc_z: %3").arg(really_accx).arg(really_accy).arg(really_accz);
                    TestItem test;
                    test.testItem = "六轴数据";
                    test.testData = testData;
                    test.testResult = passValue;
                    test.ask = "通过";
                    testItems.append(test);
                    testResultTableUpdate(testItems);

                    sendCommandWithRetry(std::bind(&Qpb::set_imu_collect_param, pb, FacSwitch_STOP));
                    state = STATE_WATI_STOP_IMU_DATA;
                }

                break;

            case STATE_WATI_STOP_IMU_DATA:  // 设置设备采集
                if (canGoNext) {
                    showlog("成功关闭六轴数据");
                    sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb));

                    state = STATE_WATI_GET_BASE_STATE;
                }
                break;

            case STATE_WATI_GET_BASE_STATE:
                if (canGoNext) {
                    if (base_state == 1)  // 外设状态正常
                    {
                        showlog("基础信息验证通过");
                        waitWork(WAITTIME);
                        sendCommandWithRetry(std::bind(&Qpb::get_periph_state, pb));

                        showlog("2、设备外设测试");
                        state = STATE_WATI_GET_PERIPHERAL_STATE;
                    }
                    if (base_state == 2) {
                        waitWork(WAITTIME);
                        showlog("基础信息验证失败");
                        sendCommandWithRetry(std::bind(&Qpb::get_periph_state, pb));
                        showlog("2、设备外设测试");
                        totalresult = failValue;
                        erroContent << "|基础信息:异常";
                        state = STATE_WATI_GET_PERIPHERAL_STATE;
                    }
                }
                break;
            case STATE_WATI_GET_PERIPHERAL_STATE:
                if (canGoNext) {
                    if (periph_state == 1)  // 设备信息正常
                    {
                        showlog("外设状态正常");
                        // pb->get_button_state(1);
                        // showlog("8、按键测试");
                        // showlog("请按下按键");
                        state = STATE_WATI_CHARGE_CURRENT;
                    }
                    if (periph_state == 2)  // 设备信息异常
                    {
                        TestItem test;
                        test.testItem = "外设状态";
                        test.testData = QString::number(periph_state);
                        test.testResult = failValue;
                        test.ask = "通过";
                        testItems.append(test);
                        testResultTableUpdate(testItems);

                        totalresult = failValue;
                        erroContent << "|外设状态:异常";
                        showlog("外设状态异常");
                        state = STATE_WATI_CHARGE_CURRENT;
                    }
                }
                break;

            case STATE_WATI_CHARGE_CURRENT:

                if (SETTINGS.value("SYSTEM/SendMotorCalibration").toBool() &&
                    !SETTINGS.value("SYSTEM/TestWifiSignal").toBool()) {
                    bool ok = false;
                    value = ui->pcba_motor_cali_param->text().mid(0, 8).toUInt(&ok, 16);  // 将十六进制字符串转换为
                    if (ok) {
                        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                                 << "电机参数为" << value;  // 输出 38822029，即十六进制字符串 "003bdf6d" 对应的数值
                    } else {
                        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                                 << "Invalid input string";
                    }

                    sendCommandWithRetry(std::bind(&Qpb::set_motor_cali_result_param, pb, value));

                    state = STATE_WATI_GET_CORRECT_MOTOR;
                } else if (!SETTINGS.value("SYSTEM/TestWifiSignal").toBool() &&
                           !SETTINGS.value("SYSTEM/SendMotorCalibration").toBool()) {
                    showlog("5、蓝牙强度测试");
                    at->sendBLELOG(1);  // 日志开
                    rssitestcount = 0;
                    state = STATE_WATI_GET_CORRECT_BLERSSI;
                } else {
                    showlog("3、wifi测试");
                    connectwifi();
                    wificonnectwaittime->setInterval(wifi_connect_waittime);
                    wificonnectwaittime->start();
                    wificonnectwaittimehalf->setInterval(wifi_connect_waittime / 2);
                    wificonnectwaittimehalf->start();
                    state = STATE_WATI_CONNECT_WIFI;
                }

                break;

            case STATE_WATI_CONNECT_WIFI:
                if (ui->WIFIStatusLabel->text() == "WIFI连接：<font color='green'>成功</font>")  // wifi连接成功
                {
                    wificonnectwaittime->stop();
                    wificonnectwaittimehalf->stop();
                    showlog("WIFI已经连接成功");
                    testData = "连接成功";
                    TestItem test;
                    test.testItem = "wifi连接";
                    test.testData = testData;
                    test.testResult = passValue;
                    test.ask = "通过";
                    testItems.append(test);

                    testResultTableUpdate(testItems);

                    showlog("4、wifi强度测试");
                    wifiwaittime->setInterval(wifi_wait_time);
                    wifiwaittime->start();
                    state = STATE_WATI_GET_CORRECT_WIFIRSSI;
                    iswificonnectovertime = 0;
                }

                if (iswificonnectovertime) {
                    wificonnectwaittime->stop();
                    wificonnectwaittimehalf->stop();
                    showlog("WIFI连接超时");
                    testData = "连接超时";
                    TestItem test;
                    test.testItem = "wifi连接";
                    test.testData = testData;
                    test.testResult = failValue;
                    test.ask = "通过";
                    testItems.append(test);

                    testResultTableUpdate(testItems);

                    wifiwaittime->setInterval(wifi_wait_time);
                    wifiwaittime->start();
                    totalresult = failValue;

                    erroContent << "|WIFI连接:超时";
                    blewaittime->setInterval(ble_wait_time);
                    blewaittime->start();
                    at->sendBLELOG(1);  // 日志开
                    state = STATE_WATI_GET_CORRECT_BLERSSI;
                }

                break;

            case STATE_WATI_GET_CORRECT_WIFIRSSI:

                if (intwifirssi != 0) {
                    if (intwifirssi < HighRssi && intwifirssi > LowRssi)  // wifi信号可以
                    {
                        rssitestcount++;
                        showlog("WIFI测试" + QString::number(intwifirssi));

                        if (rssitestcount > RssiTestTime)  // wifi信号可以
                        {
                            QString wifiName = "usmile_finish";
                            QString wifiPassword = "12345678";
                            QByteArray wifiNameBytes = wifiName.toUtf8();
                            QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
                            pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
                            //   pb->set_wifi_disconnect();
                            testData = WIFI_RSSI;
                            TestItem test;
                            test.testItem = "wifi信号";
                            test.testData = testData;
                            test.testResult = passValue;
                            test.ask = "通过";
                            testItems.append(test);

                            testResultTableUpdate(testItems);

                            state = STATE_WATI_GET_CORRECT_BLERSSI;
                            showlog("5、蓝牙强度测试");
                            at->sendBLELOG(1);  // 日志开
                            rssitestcount = 0;
                        }
                    } else {
                        rssitestcount = 0;
                        rssitestfailcount++;
                        if (rssitestfailcount > RssiTestTime)  // wifi信号不可以
                        {
                            QString wifiName = "usmile_finish";
                            QString wifiPassword = "12345678";
                            QByteArray wifiNameBytes = wifiName.toUtf8();
                            QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
                            pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
                            //     pb->set_wifi_disconnect();
                            totalresult = failValue;

                            testData = WIFI_RSSI;

                            TestItem test;
                            test.testItem = "wifi信号";
                            test.testData = testData;
                            test.testResult = failValue;
                            test.ask = "通过";
                            testItems.append(test);

                            testResultTableUpdate(testItems);

                            qDebug() << getIndex() << "wifi不合格信号强度" << intwifirssi;
                            showlog("wifi不合格信号强度" + WIFI_RSSI);
                            rssitestfailcount = 0;
                            state = STATE_WATI_GET_CORRECT_BLERSSI;
                            showlog("5、蓝牙强度测试");
                            at->sendBLELOG(1);  // 日志开
                            erroContent << QString("|wifi信号:%1").arg(testData);
                        }
                    }
                }
                waitWork(50);

                break;

            case STATE_WATI_GET_CORRECT_MOTOR:
                if (canGoNext) {
                    pb->set_sevor_motor_param(14, 12, 5.2, 190);

                    showlog("已收到电机校准参数");
                    showlog("5、蓝牙强度测试");
                    at->sendBLELOG(1);  // 日志开
                    showlog("已发送蓝牙强度获取指令");

                    blewaittime->setInterval(ble_wait_time);
                    blewaittime->start();
                    state = STATE_WATI_GET_CORRECT_BLERSSI;
                }

                break;
            case STATE_WATI_GET_CORRECT_BLERSSI:

                if (intblerssi != 0) {
                    if (intblerssi < BleHighRssi && intblerssi > BleLowRssi)  // 蓝牙信号可以
                    {
                        showlog("蓝牙测试" + QString::number(intblerssi));
                        rssitestcount++;
                        if (rssitestcount >= RssiTestTime)  // 蓝牙信号可以
                        {
                            testData = BLE_RSSI;
                            TestItem test;
                            test.testItem = "蓝牙信号";
                            test.testData = testData;
                            test.testResult = passValue;
                            test.ask = "通过";
                            testItems.append(test);

                            testResultTableUpdate(testItems);

                            showlog("蓝牙测试通过" + QString::number(intblerssi) + "测试次数为" +
                                    QString::number(rssitestcount));
                            rssitestcount = 0;
                            at->sendBLELOG(0);  // 日志关
                            waitWork(100);      // 防止发送给治具粘包
                            emit overtask(getIndex());
                            showlog("6、等待治具测试");
                            if (SETTINGS.value("SYSTEM/LightTest").toBool()) {
                                state = STATE_WATI_LIGHT_TEST;
                            } else {
                                waitWork(WAITTIME);  // 播放音乐
                                QString musicFileName = "/data/audio/scan_f.bin";
                                // qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<<
                                // "文件名为"
                                // << musicFileName;
                                QByteArray musicFileNameBytes = musicFileName.toUtf8();
                                showlog("正在播放音乐");
                                pb->set_music(musicFileNameBytes);
                                music_play_time->setInterval(5000);
                                music_play_time->start();
                                state = STATE_WATI_WORKING_TEST;
                            }

                            isbleovertime = 0;
                            break;
                        }
                    } else {
                        rssitestcount = 0;
                        rssitestfailcount++;
                        showlog("重试测试蓝牙" + BLE_RSSI);
                        if (rssitestfailcount >= RssiTestTime)  // 蓝牙信号不可以
                        {
                            testData = BLE_RSSI;

                            TestItem test;
                            test.testItem = "蓝牙信号";
                            test.testData = testData;
                            test.testResult = failValue;
                            test.ask = "通过";
                            testItems.append(test);

                            testResultTableUpdate(testItems);

                            erroContent << QString("|蓝牙信号:%1").arg(testData);
                            totalresult = failValue;
                            qDebug() << getIndex() << "蓝牙不合格信号强度" << BLE_RSSI;
                            qDebug() << getIndex() << "范围为" << BleHighRssi << BleLowRssi;
                            showlog("蓝牙不合格信号强度intblerssi" + QString::number(intblerssi));

                            showlog("蓝牙不合格信号强度" + BLE_RSSI);
                            showlog("当前蓝牙范围为" + QString::number(BleHighRssi) + QString::number(BleLowRssi));

                            at->sendBLELOG(0);  // 日志关
                            rssitestfailcount = 0;

                            waitWork(100);  // 防止发送给治具粘包
                            emit overtask(getIndex());
                            showlog("6、等待治具测试");

                            if (SETTINGS.value("SYSTEM/LightTest").toBool()) {
                                state = STATE_WATI_LIGHT_TEST;
                            } else {
                                QString musicFileName = "/data/audio/scan_f.bin";
                                // qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<<
                                // "文件名为"
                                // << musicFileName;
                                QByteArray musicFileNameBytes = musicFileName.toUtf8();
                                showlog("正在播放音乐");
                                pb->set_music(musicFileNameBytes);
                                music_play_time->setInterval(5000);
                                music_play_time->start();
                                state = STATE_WATI_WORKING_TEST;
                            }
                        }
                    }
                }
                waitWork(50);
                break;

            case STATE_WATI_LIGHT_TEST:
                if (1) {
                    showlog("正在进行灯光测试");
                    pb->set_led_color(1, 1);
                    waitWork(100);
                    pb->set_led_color(0, 1);
                    waitWork(100);
                    pb->set_led_color(0, 2);
                    waitWork(100);
                    pb->set_led_color(1, 2);
                    is_music_play_over_time = 1;
                    state = STATE_WATI_WORKING_TEST;
                }

                break;

            case STATE_WATI_WORKING_TEST:
                if (is_music_play_over_time) {
                    is_music_play_over_time = 0;
                    showlog("播放结束");
                    if (SETTINGS.value("SYSTEM/ServoMotorStart").toBool()) {
                        pb->set_sevor_motor_param(14, 12, 5.2, 190);
                        showlog("已经发送电机测试指令");
                    } else {
                        if (SETTINGS.value("SYSTEM/uperMotor").toBool()) {
                            showlog("跑的是P30P");
                            pb->set_sevor_motor_param(3500, 14000, 10, 380);
                        } else {
                            showlog("跑的是q20");
                            pb->set_motor_param(270, 60);
                            pb->set_motor_state(1);
                        }
                        //   waitWork(1500);
                        //   pb->set_device_mode(3);
                        //       pb->set_brush_control(1);
                    }

                    // else {
                    //     pb->set_device_mode(4);  // 进入亮白
                    // }

                    showlog("已发送进入亮白模式");
                    state = STATE_WATI_WHITE_MODE;
                }

                break;

            case STATE_WATI_WHITE_MODE:

                if (pb->getisDevintowhitemode() || pb->get_is_motor_param_set() || pb->get_is_motor_test_state()) {
                    //  pb->reset_all_pb();
                    showlog("已进入亮白模式且发送指令给治具");
                    waitWork(100);
                    emit start_white_modle_command(getIndex());
                    waitWork(100);
                    emit start_white_modle_command(getIndex());
                    waitWork(100);
                    emit start_white_modle_command(getIndex());

                    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool()) {
                        QMessageBox::StandardButton reply;
                        reply =
                            QMessageBox::question(this, "电机测试", "电机正常吗？", QMessageBox::Yes | QMessageBox::No);
                        if (reply == QMessageBox::No) {
                            showlog("电机测试失败");
                            erroContent << QString("|电机测试:异常");
                            totalresult = failValue;
                        }
                    }
                    at->resetConnected();
                    showlog("等待治具请求休眠命令");
                    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool())
                        start_sleep = 1;
                    state = STATE_WATI_RETURN_TEST;
                } else {
                    waitWork(500);
                    if (SETTINGS.value("SYSTEM/ServoMotorStart").toBool()) {
                        pb->set_sevor_motor_param(14, 12, 5.2, 190);
                        showlog("已经发送电机测试指令");
                    } else {
                        if (SETTINGS.value("SYSTEM/uperMotor").toBool()) {
                            showlog("跑的是P30P");
                            pb->set_sevor_motor_param(3500, 14000, 10, 380);
                        } else {
                            showlog("跑的是q20");
                            pb->set_motor_param(270, 60);
                            pb->set_motor_state(1);
                        }
                    }
                }
                break;

            case STATE_WATI_RETURN_TEST:
                if (start_sleep) {
                    start_sleep = 0;
                    pb->set_forbid_sleep(FacSwitch_CLOSE);
                    showlog("已发送取消禁止休眠");
                    state = STATE_CLOSE_FORBID_SLEEP;
                }

                break;

            case STATE_CLOSE_FORBID_SLEEP:
                if (pb->get_is_close_forbid_sleep()) {
                    pb->set_sleeep(FacSwitch_OPEN);

                    waitWork(100);
                    at->sendMac("00:00:00:00:00:00");
                    waitWork(100);

                    showlog("已经发送请求牙刷休眠");
                    state = STATE_SLEEP_OPEN;

                } else {
                    waitWork(500);
                    pb->set_forbid_sleep(FacSwitch_CLOSE);
                    showlog("正在重发取消禁止休眠");
                }

                break;
            case STATE_SLEEP_OPEN:
                if (!at->getConnected()) {
                    showlog("开始等待治具测试结果");

                    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool())
                        remain_ok = 1;
                    state = STATE_WATI_REMAIN_TEST;
                } else {
                    pb->set_sleeep(FacSwitch_OPEN);
                    showlog("正在重发开始休眠");
                    at->sendMac("00:00:00:00:00:00");
                    waitWork(500);
                }

                break;

            case STATE_WATI_REMAIN_TEST: {
                if (remain_ok == 1)  // 设备信息正常
                {
                    showlog("治具测试内容获取成功");
                    state = STATE_SAVE_RESULT;
                }
                if (remain_ok == 2)  // 不正常
                {
                    showlog("治具测试内容获取成功");
                    showlog("测试失败");
                    totalresult = failValue;
                    erroContent << QString("|治具测试:失败");
                    state = STATE_SAVE_RESULT;
                } else {
                    waitWork(100);
                    if (sendcount < 3) {
                        emit start_sleep_command(getIndex());
                    }
                    sendcount++;
                }
            }

            break;

            case STATE_SAVE_RESULT: {
                if (totalresult == passValue) {
                    // QString currentDate =
                    // QDateTime::currentDateTime().toString("yyyy年MM月dd日hh时"); QString
                    // fileNamemacAddress = macAddress; QString folderPath =
                    // QString("LOG/%1/").arg(currentDate); QString fileName = folderPath +
                    // QString("%1_上位机号为%2_测试次数为%3.log").arg(fileNamemacAddress.remove(":")).arg(getIndex()).arg(currentTestNumber);
                    // deleteFile(fileName);

                    ui->pass_number->setText("PASS:" + QString::number(passnumber));
                    ui->pass_number->setStyleSheet(
                        "font-size: 15px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 1px; text-align: center;");

                    passnumber++;
                    pb->set_fac_result(1);
                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 40px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");

                    QString itemvalue;
                    itemvalue = exportTableContent();
                    pack.itemvalue = itemvalue;
                    pack.error = "";
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        send_end_testPass(pack);
                    }

                } else if ((totalresult == failValue)) {
                    pack.itemvalue = "";
                    pack.error = erroContent.join("") + "|";  //加结束符
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }

                    ui->ng_number->setText("NG:" + QString::number(ngnumber));
                    ui->ng_number->setStyleSheet(
                        "font-size: 15px; background-color: #ff557f; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 1px; text-align: center; ");
                    ngnumber++;

                    pb->set_fac_result(0);
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 40px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }

                showlog("保存完毕");
                showlog("测试结束");
                ui->macInput->clear();
                ui->getMac->clear();
                // ui->macInput->setFocus();
                ui->macInput->setDisabled(0);
                if (pack.factory == "wks") {
                    ui->getMac->setDisabled(0);
                }

                isPcbaTestContinue = false;  // 结束

                // pb->set_fac_mode(1);
                // waitWork(300);
                on_disconnectButton_clicked();

                emit send_end_test(getIndex());

                state = STATE_IDLE;
            }

            break;
            default: break;
        }
        // QCoreApplication::processEvents();
    }
}

void PcbaForm::on_macInput_returnPressed() {
    clearDisplay();

    // 检查是否是同一个mac地址进行的连续测试
    if (macAddress != lastMacAddress) {
        currentTestNumber = 1;
        lastMacAddress = macAddress;
    } else {
        currentTestNumber++;
    }

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    // if (!usbserialPort->isOpen()){
    //     openusbSerialPort();
    // }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        at->ask_mac();
        showlog("Mac地址错误，当前为:" + ui->macInput->text());
        //   QMessageBox::warning(nullptr, "Warning", "Mac地址错误:\n\r" + ui->macInput->text());

        // if(mac_retry_flag == false){
        //     mac_retry_flag = true;

        // } else {
        //     mac_retry_flag = false;
        // }
        return;
    } else {
        macAddress = ui->macInput->text();
        usblogwaittime->stop();
        firstconnectbrush = 0;
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        isPcbaTestContinue = true;
        state = STATE_IDLE;

        emit send_go_next_focus();
        ui->macInput->setDisabled(1);
        pack.mac = macAddress;

        // thread->start();
    }
}
void PcbaForm::startTest() {
    ui->test_time->display(0);
    ui->macInput->clear();
    usblogwaittime->setInterval(10000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    clearDisplay();
    on_connectButton_clicked();
    on_productConnectButton_clicked();
};
bool PcbaForm::deleteFile(const QString& filePath) {
    // 尝试删除文件
    QFile file(filePath);
    if (file.exists()) {
        if (file.remove()) {
            qDebug() << "文件删除成功:" << filePath;
            return true;
        } else {
            qDebug() << "删除文件失败:" << filePath;
            return false;
        }
    } else {
        qDebug() << "文件不存在:" << filePath;
        return false;
    }
}

void PcbaForm::writeToLogFile(const QByteArray& data, QString currentDate, QString macAddress, int machineNumber) {
    QString fileNamemacAddress = macAddress;

    // 构建文件夹路径，按照日期存储日志
    QString folderPath = QString("LOG/%1/").arg(currentDate);
    // 确保路径存在
    QDir().mkpath(folderPath);

    // 构建日志文件名，包括日期、mac地址、测试次数和机号
    QString fileName = folderPath + QString("%1_上位机号为%2_测试次数为%3.log")
                                        .arg(fileNamemacAddress.remove(":"))
                                        .arg(machineNumber)
                                        .arg(currentTestNumber);

    // 打开日志文件，只是创建并关闭
    QFile logFile(fileName);

    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        // qDebug() << "写入成功牙刷日志";
        // 写入数据
        QTextStream out(&logFile);
        out << data << endl;
        logFile.close();
    } else {
        qDebug() << "无法打开牙刷日志文件：" << fileName;
    }
}
void PcbaForm::on_getMac_returnPressed() {
    testResultTableInit();
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 20px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

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
        ui->getMac->setFocus();
        return;
    }

    emit send_go_next_focus();

    processInspection(ui->getMac->text());

    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool())
        on_start_scan_clicked();
}
void PcbaForm::processInspection(QString stringsn) {
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
        ui->mes_state->setStyleSheet("font-size: 20px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void PcbaForm::on_start_scan_clicked() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    ui->pick_device->clear();
    deviceMap.clear();

    showlog("已经触发");
}
void PcbaForm::updateComboBox() {
    // 遍历设备信息，根据 rssi 的值进行过滤
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        QString deviceAddress = it.key();
        QString deviceName = it.value()["Name"];
        QString deviceRssi = it.value()["Rssi"];

        // qDebug() << "设备地址：" << deviceName<<deviceAddress<<deviceRssi;
        if (deviceName.contains(ui->name_range->text()) && deviceRssi.toInt() > ui->rssi_range->text().toInt() &&
            deviceAddress.length() == 17) {
            int index = ui->pick_device->findText(deviceAddress);
            index = ui->pick_device->findText(deviceAddress);

            if (index == -1) {
                ui->pick_device->addItem(deviceAddress);
                if (ui->is_scan_connect->checkState()) {
                    ui->macInput->setText(deviceAddress);
                    qDebug() << deviceAddress;
                    on_macInput_returnPressed();
                }

                // qDebug() << "有新增" << deviceAddress;
            }
        }
    }
}

void PcbaForm::on_pick_device_textActivated(const QString& arg1) {
    ui->log->clear();
    ui->msgEdit->clear();
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        ui->macInput->setText(arg1);

        qDebug() << arg1;

        on_macInput_returnPressed();
    }
}
