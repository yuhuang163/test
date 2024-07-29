#include "pcbaform.h"
#include "qdebug.h"
#include "ui_pcbaform.h"
#include <QMessageBox>
#include <QSerialPortInfo>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
void PcbaForm::on_pushButton_clicked()
{
    ui->macInput->setText("f4:12:fa:c5:51:c6");   // 测试牙刷
    //  ui->macInput->setText("74:4D:BD:95:7A:DA");//su牙刷
    // ui->macInput->setText("74:4D:BD:95:7B:E6");//金鹏牙刷
    // ui->macInput->setText("74:4D:BD:95:7B:F6");//金鹏牙刷
    // ui->macInput->setText("74:4D:BD:95:7C:BE");//zhangmeng牙刷
    //   ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    ui->macInput->setText("3C:84:27:07:A8:D2");
    // ui->macInput->setText("e2:66:07:34:2d:f7");
    // ui->macInput->setText("3c:84:27:29:50:32");
    ui->macInput->setText("b4:56:5d:bf:54:4e");

    on_macInput_returnPressed();
    at->sendMac(ui->macInput->text());   // 发送mac地址
}

PcbaForm::PcbaForm(int index, QWidget *parent) : ui(new Ui::PcbaForm)
{    m_index=index;

    ui->setupUi(this);
    update_main_style("Ubuntu.qss");
    scanSerialPorts();   // 要搜索一下一开始
    borad_test_table_init();

    ui->ng_number->setText("NG:" + QString::number(0));
    ui->ng_number->setStyleSheet(
        "font-size: 16px; background-color: #ff557f; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 1px; text-align: center; ");

    ui->pass_number->setText("PASS:" + QString::number(0));
    ui->pass_number->setStyleSheet(
        "font-size: 16px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 1px; text-align: center;");

    ui->mechine_number->setText(QString::number(getIndex()) + "号机");
    ui->mechine_number->setStyleSheet(
        "font-size: 33px; background-color: yellow; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 66px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    connect(usblogwaittime, &QTimer::timeout,
            [=]()
            {
                if (pack.product == "P20P" || pack.product == "Q20")
                {
                    at->ask_mac();
                    showlog("正在定时器复位牙刷");
                }
            });

    connect(blewaittime, &QTimer::timeout,
            [=]()
            {
                isbleovertime = 1;
                blewaittime->stop();
            });

    connect(wifiwaittime, &QTimer::timeout,
            [=]()
            {
                iswifiovertime = 1;
                wifiwaittime->stop();
            });

    connect(wificonnectwaittime, &QTimer::timeout,
            [=]()
            {
                iswificonnectovertime = 1;
                wificonnectwaittime->stop();
            });

    connect(wificonnectwaittimehalf, &QTimer::timeout,
            [=]()
            {
                connectwifi();
                wificonnectwaittimehalf->stop();
            });

    connect(music_play_time, &QTimer::timeout,
            [=]()
            {
                is_music_play_over_time = 1;
                music_play_time->stop();
            });

    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    ui->wifiUserName->setText(
        settings.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    // ui->wifiUserName->setText(settings.value("WIFI/Name", "请在配置文件中设置").toString());
    ui->wifiPassword->setText(settings.value("WIFI/Password", "usmile123").toString());
    HighRssi = settings.value("WIFI/HighRssi").toInt();
    LowRssi = settings.value("WIFI/LowRssi").toInt();
    BleHighRssi = settings.value("BLE/HighRssi").toInt();
    BleLowRssi = settings.value("BLE/LowRssi").toInt();
    ble_wait_time = settings.value("BLE/BleWaitTime", "15000").toInt();
    wifi_wait_time = settings.value("WIFI/WifiWaitTime", "15000").toInt();
    wifi_connect_waittime = settings.value("BLE/WifiConnectWaitTime", "15000").toInt();
    HighCharCurrent = settings.value("Current/HighCharCurrent").toDouble();
    LowCharCurrent = settings.value("Current/LowCharCurrent").toDouble();
    HighworkCurrent = settings.value("Current/HighworkCurrent").toDouble();
    LowworkCurrent = settings.value("Current/LowworkCurrent").toDouble();

    HighmusicCurrent = settings.value("Current/HighmusicCurrent").toDouble();
    LowmusicCurrent = settings.value("Current/LowmusicCurrent").toDouble();

    HighstaticCurrent = settings.value("Current/HighstaticCurrent").toDouble();
    LowstaticCurrent = settings.value("Current/LowstaticCurrent").toDouble();
    music_state = settings.value("FIXTEST/MusicState").toInt();
    overVoltageLight = settings.value("FIXTEST/OverVoltageLight").toInt();
    button1 = settings.value("FIXTEST/Button1").toInt();
    button2 = settings.value("FIXTEST/Button2").toInt();
    RssiTestTime = settings.value("BLE/RssiCount").toInt();
    acc_x_up = settings.value("IMU/acc_x_up").toInt();
    acc_x_down = settings.value("IMU/acc_x_down").toInt();

    acc_y_up = settings.value("IMU/acc_y_up").toInt();
    acc_y_down = settings.value("IMU/acc_y_down").toInt();

    acc_z_up = settings.value("IMU/acc_z_up").toInt();
    acc_z_down = settings.value("IMU/acc_z_down").toInt();

    showlog("acc_z_up=" + QString::number(acc_z_up));
    showlog("acc_z_down=" + QString::number(acc_z_down));

    showlog("MusicState=" + QString::number(music_state));
    showlog("OverVoltageLight=" + QString::number(overVoltageLight));
    showlog("Button1=" + QString::number(button1));
    showlog("Button2=" + QString::number(button2));
    showlog("ble_wait_time=" + QString::number(ble_wait_time));
    showlog("wifi_wait_time=" + QString::number(wifi_wait_time));
    showlog("wifi_connect_waittime=" + QString::number(wifi_connect_waittime));
    showlog("wifiUserName=" + settings.value("WIFI/Name", "请在配置文件中设置").toString());
    showlog("wifiPassword=" + settings.value("WIFI/Password", "usmile123").toString());
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

    showlog("HighstaticCurrent=" + QString::number(HighstaticCurrent));
    showlog("LowstaticCurrent=" + QString::number(LowstaticCurrent));
    showlog("RssiTestTime=" + QString::number(RssiTestTime));

    if (pack.product == "P20P" || pack.product == "Q20")
    {
        ui->productDisconnectButton->setEnabled(1);
        ui->productConnectButton->setEnabled(1);
        ui->productComNameCombo->setEnabled(1);
    }
    else
    {
        ui->productDisconnectButton->setEnabled(0);
        ui->productConnectButton->setEnabled(0);
        ui->productComNameCombo->setEnabled(0);
    }
}
void PcbaForm::get_dongle_ver(QString data)
{
    showlog("当前dongle的版本为：" + data);
}
void PcbaForm::get_dongle_wifi(QString data)
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    showlog("获取到了wifi名字" + data);

    // 保存密码
    settings.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    settings.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(
        settings.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(settings.value("WIFI/Password", "123445566").toString());
}

void PcbaForm::processReceivedData(const QByteArray &data)
{
    static QString logString = "";

    // 将接收到的数据添加到日志字符串中
    logString += data;

    logString.replace("\n", "").replace("\r", "");
    // showlog(logString);
    // 查找日志中最后一个 "Local Board" 条目的索引
    int lastIndex = logString.lastIndexOf("Local Board");

    if (lastIndex != -1)
    {
        // 从最后一个 "Local Board" 条目开始查找 MAC 地址
        int macIndex = logString.indexOf('=', lastIndex);

        if (macIndex != -1 && logString.length() >= macIndex + 18)
        {   // 判断是否包含完整的 MAC 地址（17 个字符 + 1 个分隔符）
            // 提取 MAC 地址
            QString macAddress =
                logString.mid(macIndex + 1, 17).trimmed();   // MAC 地址长度为17，并去除首尾空格
            qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                     << "提取到的MAC地址：" << macAddress;

            // 清空日志字符串
            logString = "";

            // 在界面上设置 MAC 地址
            ui->macInput->setText(macAddress);

            if (firstconnectbrush)
            {
                on_macInput_returnPressed();
            }

            // 在这里可以将提取到的 MAC 地址用于后续处理
        }
        else
        {
            logString = "";
            at->ask_mac();
            showlog("日志数据中未找到完整的MAC地址，正在重发请求获取mac地址");
        }
    }
    else
    {
        //  qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<<
        //  "日志数据中未找到mac地址关键字。";
    }
}

void PcbaForm::refresh_ammeter_data(QString data)
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "收到电流数据"
             << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk);

    if (conversionOk)
    {
        // 转换成功
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
        QString formattedValue = QString::number(normalValue, 'f', 8);
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "转换后的数值：" << formattedValue << "ma";
    }
    else
    {
        // 转换失败
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "无法将字符串转换为 double 类型";
    }
}

void PcbaForm::get_wifi_msg(QString data)
{
    // qDebug() << getIndex()<< "收到wifi数据为" << data;
    QStringList parts = data.split("-");
    int numPairs = parts.size() / 2;
    for (int i = 0; i < numPairs; ++i)
    {
        QString macAddress = parts[i * 2];
        QString rssi = "-" + parts[i * 2 + 1];
        wifiMac = wifiMac.toUpper();
        // qDebug() << getIndex() << "dongle的的wifiMac:" << macAddress;
        // qDebug() << getIndex() << "RSSI:" << rssi;
        // qDebug() << getIndex() << " 牙刷的wifiMac:" << wifiMac;
        if (macAddress == wifiMac)
        {
            ui->WIFI_RSSI->setText("WIFI的RSSI：" + rssi);
            // qDebug() << getIndex()<< getIndex() << " 比对成功";
            refresh_wifi_state(1);
            WIFI_RSSI = rssi;
        }
    }

    bool ok;
    WIFI_RSSI.toInt(&ok);

    if (!ok)
    {
        qDebug() << "转换WIFIrssi失败,内容为" + WIFI_RSSI + "内容结束";
    }
    else
    {
        //  showlog("转换成功");
        intwifirssi = WIFI_RSSI.toInt(&ok);
    }
}
void PcbaForm::refresh_usb_uart_state(int state)
{
    if (state)
        ui->usbuartstate->setText("usb串口连接：<font color='green'>成功</font>");
    else
        ui->usbuartstate->setText("usb串口连接：<font color='red'>断开</font>");
}
void PcbaForm::refresh_dongle_uart_state(int state)
{
    if (state)
        showlog("dongle串口连接成功");
    else
    {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void PcbaForm::refresh_wifi_state(int state)
{
    if (state)
    {
        ui->WIFIStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        // showlog("WIFI连接成功");
    }
    else
    {
        ui->WIFIStatusLabel->setText("WIFI连接：<font color='red'>断开</font>");
        //  showlog("WIFI连接断开");
    }
}
void PcbaForm::refresh_ble_state(int state)
{
    if (state)
    {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        showlog("蓝牙连接成功");
        pb->set_forbid_sleep(FacSwitch_OPEN);
        showlog("已发送禁止休眠");
    }
    else
    {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>断开</font>");
        showlog("蓝牙连接断开");
    }
}

void PcbaForm::refresh_ble_rssi(QString data)
{
    // qDebug() << data;
    ui->BLE_RSSI->setText("BLE的RSSI:" + data);
    // showlog("zzzzz"+data);
    BLE_RSSI = data;
    bool ok;
    BLE_RSSI.toInt(&ok);

    if (!ok)
    {
        qDebug() << "转换蓝牙rssi失败,内容为" + BLE_RSSI + "内容结束";
    }
    else
    {
        // showlog("转换成功");
        intblerssi = BLE_RSSI.toInt(&ok);
    }
}
void PcbaForm::check_LED_CONTROL_state(FacLedControl data)
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "收到led控制状态:" << data.switch_state;
    isledcontrol = 1;
}

void PcbaForm::check_BrushControl_state(FacBrushControl data)
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "收到牙刷控制状态:" << data.value_item.brush_start;
    isbrushcontrol = 1;
}
void PcbaForm::checkbutton(FacButtonState data)
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "获取到开关按键状态"
             << data.button_state[0].command_data.power_button.button_state_now;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "获取到模式按键状态"
             << data.button_state[1].command_data.mode_button.button_state_now;

    // testItem = "模式按键"; testData =
    // QString::number(data.button_state[0].command_data.mode_button.button_state_now);
    // if(testData==1){
    //    pcba_test_data_update(testItem, testData, passValue);
    // }else{
    //    pcba_test_data_update(testItem, testData, failValue);
    // }

    testItem = "电源按键";
    testData = QString::number(
        static_cast<int>(data.button_state[0].command_data.power_button.button_state_now));
    if (testData == "2")
    {
        pcba_test_data_update(testItem, testData, passValue);
        isbuttonTest = 1;
    }
}
PcbaForm::~PcbaForm()
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
             << "PcbaFormd的析构";
    delete ui;
}
void PcbaForm::closeEvent(QCloseEvent *)
{
    isPcbaTestContinue = false;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "已经关闭"
             << isPcbaTestContinue;
}
void PcbaForm::refresh_base_data(FacGetDevBaseInfo data)
{
    if (refresh_times)
    {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "refresh_times" << refresh_times;
        refresh_times = 0;
        QSettings settings(SETTING_NAME, QSettings::IniFormat);
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "algo_version" << data.algo_version;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "hw_version"
                 << data.hw_version;
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
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "imu_id"
                 << data.imu_id;

        QString mac;
        mac.clear();
        for (int var = 0; var < data.ble_mac.size; ++var)
        {
            mac += QString::number(data.ble_mac.bytes[var], 16);
            if (var < data.ble_mac.size - 1)
            {
                mac += ":";
            }
        }

        // 反转MAC地址

        for (int i = mac.size(); i > 0; i -= 3)
        {
            bleMac += mac.mid(i - 2, 2);
            if (i > 2)
            {
                bleMac += ":";
            }
        }

        wifiMac.clear();
        for (int var = 0; var < data.wifi_mac.size; ++var)
        {
            wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
            if (var < data.wifi_mac.size - 1)
                wifiMac += ":";
        }
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "设备的 wifiMac:" << wifiMac;

        QString productName = settings.value("ProductInfo/Product_Name").toString();
        QString appProtocolVersion = settings.value("ProductInfo/App_Protocol_Version").toString();
        QString factoryProtocolVersion =
            settings.value("ProductInfo/Factory_Protocol_Version").toString();
        QString hardwareVersion = settings.value("ProductInfo/Hardware_Version").toString();
        QString softwareVersion = settings.value("ProductInfo/Software_Version").toString();
        QString resourceVersion = settings.value("ProductInfo/Resource_Version").toString();
        QString algorithmVersion = settings.value("ProductInfo/Algorithm_Version").toString();
        QString pressureSenseVersion =
            settings.value("ProductInfo/Pressure_Sense_Version").toString();
        QString imuId = settings.value("ProductInfo/IMU_ID").toString();
        QString ble_ver = settings.value("ProductInfo/Ble_Ver").toString();

        if (data.algo_version == algorithmVersion && data.hw_version == hardwareVersion &&
            data.ble_version == ble_ver && data.presure_version == pressureSenseVersion &&
            data.product_name == productName &&
            QString("%1").arg(data.pb_phone_ver) == appProtocolVersion &&
            QString("%1").arg(data.pb_factory_ver) == factoryProtocolVersion &&
            data.soft_version == softwareVersion && data.res_version == resourceVersion &&
            QString::number(data.imu_id) == imuId)

        {
            base_state = 1;
        }
        else
        {
            base_state = 2;
        }

        testItem = "产品名字";
        testData = data.product_name;
        if (testData == productName)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "软件版本";
        testData = data.soft_version;
        if (testData == softwareVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "资源版本";
        testData = data.res_version;
        if (testData == resourceVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "蓝牙版本";
        testData = data.ble_version;
        if (testData == ble_ver)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }

        testItem = "硬件版本";
        testData = data.hw_version;
        if (testData == hardwareVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }

        testItem = "算法版本号";
        testData = data.algo_version;
        if (testData == algorithmVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }

        testItem = "压感版本";
        testData = data.presure_version;
        if (testData == pressureSenseVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }

        testItem = "六轴id";
        testData = QString::number(data.imu_id);
        if (testData == imuId)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }

        testItem = "APP的pb版本";
        testData = QString("%1").arg(data.pb_phone_ver);
        if (testData == appProtocolVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "工厂的pb版本";
        testData = QString("%1").arg(data.pb_factory_ver);
        if (testData == factoryProtocolVersion)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
    }
}

void PcbaForm::refresh_periph_data(FacGetPeriphState data)
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "flash_state"
             << data.flash_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "imu_state"
             << data.imu_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "magnet_state"
             << data.magnet_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "press_state"
             << data.press_state;
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "audio_state"
             << data.audio_state;

    if (refresh_periph_state_times)
    {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "refresh_periph_state_times" << refresh_periph_state_times;
        refresh_periph_state_times = 0;

        QSettings settings(SETTING_NAME, QSettings::IniFormat);
        bool imuStatus = settings.value("PeripheralStatus/IMU_Status").toBool();
        bool flashStatus = settings.value("PeripheralStatus/Flash_Status").toBool();
        bool magneticStatus = settings.value("PeripheralStatus/Magnetic_Status").toBool();
        bool pressureStatus = settings.value("PeripheralStatus/Pressure_Status").toBool();
        bool audioState = settings.value("PeripheralStatus/Audio_Status").toBool();

        if (data.flash_state == flashStatus && data.imu_state == imuStatus &&
            data.audio_state == audioState && data.press_state == pressureStatus &&
            data.magnet_state == magneticStatus)
        {
            periph_state = 1;
        }
        else
        {
            periph_state = 2;
        }

        testItem = "功放状态";
        testData = QString::number(data.audio_state);
        if (data.audio_state == audioState)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "内存状态";
        testData = QString::number(data.flash_state);
        if (data.flash_state == flashStatus)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "六轴状态";
        testData = QString::number(data.imu_state);
        if (data.imu_state == imuStatus)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        if (pack.product == "P20P")
            testItem = "马达状态";
        else
            testItem = "地磁状态";

        testData = QString::number(data.magnet_state);
        if (data.magnet_state == magneticStatus)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
        testItem = "压感状态";
        testData = QString::number(data.press_state);
        if (data.press_state == pressureStatus)
        {
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            pcba_test_data_update(testItem, testData, failValue);
        }
    }
}

void PcbaForm::on_end_clicked()
{
    usblogwaittime->stop();
    isPcbaTestContinue = false;
    on_disconnectButton_clicked();

    if (pack.product == "P20P" || pack.product == "Q20")
        on_productDisconnectButton_clicked();

    ui->macInput->setDisabled(0);
}

void PcbaForm::connectwifi()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    QString wifiName = settings.value(QString("WIFI/Name%1").arg(getIndex())).toString();
    QString wifiPassword = settings.value("WIFI/Password").toString();

    // QString wifiName = settings.value("WIFI/Name").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected())
    {
        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
        showlog("已设置连接wifi");
    }
    else
    {
        showlog("请等待连接牙刷后再试");
    }
}

void PcbaForm::borad_test_table_init()
{
    // 初始化表格
    ui->show_borad_test_result->setColumnCount(3);
    ui->show_borad_test_result->setRowCount(0);   // 初始行数为0，因为还没有数据

    // 设置表格标题
    QStringList headers;
    headers << "项目" << "数据" << "结果";
    ui->show_borad_test_result->setHorizontalHeaderLabels(headers);
    // 设置表格自适应列宽
    ui->show_borad_test_result->horizontalHeader()->setSectionResizeMode(
        QHeaderView::ResizeToContents);
}

void PcbaForm::pcba_test_data_update(const QString &item, const QString &data,
                                     const QString &result)
{
    TestItem pcba;
    pcba.testItem = item;
    pcba.testData = data;
    pcba.testResult = result;
    testItems.append(pcba);

    // 获取当前时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    ui->show_borad_test_result->scrollToBottom();
    // 插入新的一行
    int row = ui->show_borad_test_result->rowCount();
    ui->show_borad_test_result->insertRow(row);

    // 设置每列的数据
    // ui->show_borad_test_result->setItem(row, 0, new QTableWidgetItem(timestamp));
    ui->show_borad_test_result->setItem(row, 0, new QTableWidgetItem(item));
    ui->show_borad_test_result->setItem(row, 1, new QTableWidgetItem(data));

    // 设置结果列的数据，假设结果是一个字符串
    QTableWidgetItem *resultItem = new QTableWidgetItem(result);

    // 设置失败状态的背景颜色为红色
    if (result == "失败")
    {
        resultItem->setBackground(QBrush(Qt::red));
    }
    if (result == "通过")
    {
        resultItem->setBackground(QBrush(Qt::green));
    }
    ui->show_borad_test_result->setItem(row, 2, resultItem);
}

void PcbaForm::band_sn_mac_to_csv(const QString &macAddress, const QString &sn)
{
    // 获取桌面路径
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    // 构建 "测试结果" 文件夹的完整路径
    QString folderPath = QDir(desktopPath).filePath("测试结果");

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists())
    {
        QDir().mkpath(folderPath);
    }

    // 构建完整的文件路径
    QString filePath = QDir(folderPath).filePath("绑定文件.csv");

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);
        // 检查是否是文件的开头，如果是，则写入表头
        if (file.pos() == 0)
        {
            QStringList headers;
            headers << "时间" << "mac地址" << "sn码";
            stream << headers.join(",") << "\n";
        }
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        // 写入数据
        QStringList rowData;
        rowData << timestamp;
        rowData << macAddress;
        rowData << sn;
        stream << rowData.join(",") << "\n";
        file.close();
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "文件保存到"
                 << filePath;
    }
    else
    {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "文件没关或者其他问题";
    }
}

void PcbaForm::clear_display()
{
    ui->msgEdit->clear();
    ui->show_borad_test_result->clear();
    borad_test_table_init();
    ui->log->clear();
}

void PcbaForm::on_productConnectButton_clicked()
{
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
}

void PcbaForm::on_productDisconnectButton_clicked()
{
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
}

void PcbaForm::over_task()
{
    on_end_clicked();
}
void PcbaForm::solveMesSucess(const int mechines)
{
    if (mechines == getIndex())
    {
        showlog("mes操作成功");
        // ui->mes_state->setText("MES");
        // ui->mes_state->setStyleSheet(
        //     "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00;
        //     border-radius: 10px; padding: 10px; text-align: center;");

        // mes_set_ok = 1;
    }
}
void PcbaForm::solveMesData(const int mechines, QString msg)
{
    if (mechines == getIndex())
    {
        showlog("MES:报错信息:" + msg);
        // is_motor_continue = false;
        // showlog("停止运行");
        // ui->mes_state->setStyleSheet(
        //     "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000;
        //     border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void PcbaForm::on_connectButton_clicked()
{
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void PcbaForm::on_disconnectButton_clicked()
{
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refresh_ble_state(0);
}

void PcbaForm::get_remain_data_sleep(const FixturePacketData packetData)
{
    showlog("已收到治具请求休眠命令" + QString::number(packetData.machineNumber));

    if (packetData.machineNumber == getIndex())

        if (packetData.sleep == 1)
        {
            qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                     << "开始休眠" << getIndex();
            start_sleep = 1;
            showlog("已收到治具请求休眠命令");
        }
}

void PcbaForm::get_remain_data(const FixturePacketData packetData)
{
    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log：" << "收到"
             << getIndex();
    //   showlog("已收到治具请求休眠命令" );
    if (packetData.machineNumber == getIndex())
    {
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "机号:" << packetData.machineNumber;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "静态电流值:" << packetData.staticCurrent << "ua";
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "工作电流:" << packetData.workingCurrent << "ma";
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "过压灯正常:" << packetData.overVoltageLight;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "按键1:" << packetData.button1;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "按键2:" << packetData.button2;
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "充电电流:" << packetData.chargingCurrent << "ma";

#ifdef NEW_MUSIC_CURRENT
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "音频电流:" << packetData.musicCurrent;

        if (packetData.musicCurrent < HighmusicCurrent && packetData.musicCurrent > LowmusicCurrent)

        {
            testItem = "音频电流";
            testData = QString::number(packetData.musicCurrent);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "音频电流";
            testData = QString::number(packetData.musicCurrent);
            pcba_test_data_update(testItem, testData, failValue);
        }

#else
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "音频情况:" << packetData.music_state;
        if (packetData.music_state == music_state)
        {
            testItem = "音频测试";
            testData = QString::number(packetData.music_state);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "音频测试";
            testData = QString::number(packetData.music_state);
            pcba_test_data_update(testItem, testData, failValue);
        }

#endif

        if (packetData.overVoltageLight == overVoltageLight)
        {
            if (pack.product == "P20P" || pack.product == "Q20")
                testItem = "灯光测试";
            else
                testItem = "过压灯测试";

            testData = QString::number(packetData.overVoltageLight);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            if (pack.product == "P20P" || pack.product == "Q20")

                testItem = "灯光测试";
            else
                testItem = "过压灯测试";

            testData = QString::number(packetData.overVoltageLight);
            pcba_test_data_update(testItem, testData, failValue);
        }

        if (packetData.button1 == button1)
        {
            testItem = "按键1";
            testData = QString::number(packetData.button1);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "按键1";
            testData = QString::number(packetData.button1);
            pcba_test_data_update(testItem, testData, failValue);
        }

        if (packetData.button2 == button2)
        {
            testItem = "按键2";
            testData = QString::number(packetData.button2);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "按键2";
            testData = QString::number(packetData.button2);
            pcba_test_data_update(testItem, testData, failValue);
        }

        if (packetData.staticCurrent < HighstaticCurrent &&
            packetData.staticCurrent > LowstaticCurrent)
        {
            testItem = "静态电流测试";
            testData = QString::number(packetData.staticCurrent);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "静态电流测试";
            testData = QString::number(packetData.staticCurrent);
            pcba_test_data_update(testItem, testData, failValue);
        }

        if (packetData.workingCurrent < HighworkCurrent &&
            packetData.workingCurrent > LowworkCurrent)
        {
            testItem = "工作电流测试";
            testData = QString::number(packetData.workingCurrent);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "工作电流测试";
            testData = QString::number(packetData.workingCurrent);
            pcba_test_data_update(testItem, testData, failValue);
        }

        if (packetData.chargingCurrent < HighCharCurrent &&
            packetData.chargingCurrent > LowCharCurrent)
        {
            testItem = "充电电流测试";
            testData = QString::number(packetData.chargingCurrent);
            pcba_test_data_update(testItem, testData, passValue);
        }
        else
        {
            totalresult = failValue;
            testItem = "充电电流测试";
            testData = QString::number(packetData.chargingCurrent);
            pcba_test_data_update(testItem, testData, failValue);
        }

        if (totalresult == failValue)
        {
            remain_ok = 2;
            ui->ng_number->setText("NG:" + QString::number(ngnumber));
            ui->ng_number->setStyleSheet(
                "font-size: 16px; background-color: #ff557f; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 1px; text-align: center; ");

            ui->test_result->setText("FAIL");
            ui->test_result->setStyleSheet(
                "font-size: 66px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

            state = STATE_SAVE_RESULT;
            // isPcbaTestContinue = false;
            showlog("流程结束");
        }
        else
            remain_ok = 1;

        showlog("remain_ok=" + QString::number(remain_ok));

        //    start_task();
    }
};
void PcbaForm::getimuData(FacUploadNineAlex x)
{
    for (int i = 0; i < x.data_count; i++)
    {
        // 处理 IMU 数据
        really_accx = processIMUData(x.data[i].acc_x);
        really_accy = processIMUData(x.data[i].acc_y);
        really_accz = processIMUData(x.data[i].acc_z);

        // 打印实际读取到的加速度数据
        qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                 << "实际 acc_x:" << really_accx << "实际 acc_y:" << really_accy
                 << "实际 acc_z:" << really_accz;
    }
    // 检查并记录 IMU 数据
    checkIMUData("acc_x", really_accx, acc_x_up, acc_x_down);
    checkIMUData("acc_y", really_accy, acc_y_up, acc_y_down);
    checkIMUData("acc_z", really_accz, acc_z_up, acc_z_down);
}

int32_t PcbaForm::processIMUData(uint16_t imuData)
{
    if (imuData & 0x8000)
    {   // 判断最高位是否为 1，表示负数
        return static_cast<int32_t>(imuData | 0xFFFF0000);   // 扩展为 32 位的负数
    }
    else
    {
        return static_cast<int32_t>(imuData);   // 保持正数不变
    }
}

void PcbaForm::checkIMUData(const QString &axis, int32_t value, int32_t upper, int32_t lower)
{
    if (value <= upper && value >= lower)
    {
        is_imu_correct_data = 1;
    }
    else
    {
        state = STATE_WATI_STOP_IMU_DATA;

        pb->set_imu_collect_param(FacSwitch_STOP);
        totalresult = failValue;
        testItem = "六轴数据";
        testData = QString("六轴数据异常: %1 超出范围. 实际值: %2, 范围: [%3, %4]")
                       .arg(axis)
                       .arg(value)
                       .arg(lower)
                       .arg(upper);
        pcba_test_data_update(testItem, testData, failValue);
        is_imu_correct_data = 2;
        showlog(QString("六轴数据异常: %1 超出范围. 实际值: %2, 范围: [%3, %4]")
                    .arg(axis)
                    .arg(value)
                    .arg(lower)
                    .arg(upper));
    }
}


void PcbaForm::start_task()
{
    // QMessageBox::StandardButton reply;
    uint32_t value = 0;
    if (isPcbaTestContinue)
    {
        ui->test_time->display(TestTime.elapsed() / 1000);
        //   qDebug() <<"pcba号："<<getIndex()<<"mac地址："<<macAddress<<"log："<<
        //   "状态机"<<getIndex();
        switch (state)
        {
        case STATE_IDLE:   // 复位一切
            showlog("状态机" + QString::number(getIndex()));
            ui->test_result->setText("WAIT");
            ui->test_result->setStyleSheet(
                "font-size: 66px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
            pb->reset_all_pb();
            refresh_wifi_state(0);
            testItems.clear();
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
            isbleovertime = 0;    // 是否开始发送校验结果
            iswifiovertime = 0;   // 是否开始发送校验结果
            is_imu_correct_data = 0;
            iswificonnectovertime = 0;
            is_music_play_over_time = 0;
            totalresult = "";
            at->resetConnected();
            waitWork(500);                       // 用于esp32启动等待时间
            at->sendMac(ui->macInput->text());   // 发送mac地址
            showlog(ui->macInput->text());

            totalresult = passValue;
            state = STATE_WATI_CONNECT;

            break;

        case STATE_WATI_CONNECT:   // 设置禁止休眠
            if (at->getConnected())
            {
                state = STATE_WATI_DISABLE_SLEEP;
            }
            break;

        case STATE_WATI_DISABLE_SLEEP:

            if (pb->getDisableSleep())
            {
                showlog("已进入禁止休眠");
                pb->set_fac_mode(1);
                waitWork(WAITTIME);
                pb->set_imu_collect_param(FacSwitch_START);
                state = STATE_WATI_CORRECT_IMU_DATA;
            }
            else
            {
                waitWork(500);
                pb->set_forbid_sleep(FacSwitch_OPEN);
            }
            break;

        case STATE_WATI_CORRECT_IMU_DATA:   // 获取准确inu数据
            if (is_imu_correct_data == 1)
            {
                showlog("开始关闭六轴开关，当前状态为" + QString::number(pb->getis_imu_set_sta()));
                showlog("六轴数据正常");
                showlog("1、设备信息测试");

                testItem = "六轴数据";
                QString testData = QString("acc_x: %1\nacc_y: %2\nacc_z: %3")
                                       .arg(really_accx)
                                       .arg(really_accy)
                                       .arg(really_accz);
                pcba_test_data_update(testItem, testData, passValue);
                sendCommandWithRetry(std::bind(&Qpb::set_imu_collect_param, pb, FacSwitch_STOP));
                state = STATE_WATI_STOP_IMU_DATA;
            }
            else
            {
                waitWork(500);
                pb->set_imu_collect_param(FacSwitch_START);
                qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                         << "重发六轴参数开";
            }

            break;

        case STATE_WATI_STOP_IMU_DATA:   // 设置设备采集
            if (canGoNext)
            {
                showlog("成功关闭六轴数据");
                sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb));

                state = STATE_WATI_GET_BASE_STATE;
            }
            break;

        case STATE_WATI_GET_BASE_STATE:
            if (canGoNext)
            {
                if (base_state == 1)   // 外设状态正常
                {
                    showlog("基础信息验证通过");
                    waitWork(WAITTIME);
                    sendCommandWithRetry(std::bind(&Qpb::get_periph_state, pb));

                    showlog("2、设备外设测试");
                    state = STATE_WATI_GET_PERIPHERAL_STATE;
                }
                if (base_state == 2)
                {
                    waitWork(WAITTIME);
                    showlog("基础信息验证失败");
                    sendCommandWithRetry(std::bind(&Qpb::get_periph_state, pb));
                    showlog("2、设备外设测试");
                    totalresult = failValue;
                    state = STATE_WATI_GET_PERIPHERAL_STATE;
                }
            }
            break;
        case STATE_WATI_GET_PERIPHERAL_STATE:
            if (canGoNext)
            {
                if (periph_state == 1)   // 设备信息正常
                {
                    showlog("外设状态正常");
                    // pb->get_button_state(1);
                    // showlog("8、按键测试");
                    // showlog("请按下按键");
                    state = STATE_WATI_CHARGE_CURRENT;
                }
                if (periph_state == 2)   // 设备信息异常
                {
                    totalresult = failValue;
                    showlog("外设状态异常");
                    state = STATE_WATI_CHARGE_CURRENT;
                }
            }
            break;

        case STATE_WATI_CHARGE_CURRENT:

            if (pack.product == "P20P" || pack.product == "U7P" || pack.product == "U7")
            {
                bool ok = false;
                value = ui->pcba_motor_cali_param->text().mid(0, 8).toUInt(
                    &ok, 16);   // 将十六进制字符串转换为
                if (ok)
                {
                    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                             << "电机参数为"
                             << value;   // 输出 38822029，即十六进制字符串 "003bdf6d" 对应的数值
                }
                else
                {
                    qDebug() << "pcba号：" << getIndex() << "mac地址：" << macAddress << "log："
                             << "Invalid input string";
                }

                sendCommandWithRetry(std::bind(&Qpb::set_motor_cali_result_param, pb, value));

                state = STATE_WATI_GET_CORRECT_MOTOR;
            }
            else
            {
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
            if (ui->WIFIStatusLabel->text() ==
                "WIFI连接：<font color='green'>成功</font>")   // wifi连接成功
            {
                wificonnectwaittime->stop();
                wificonnectwaittimehalf->stop();
                showlog("WIFI已经连接成功");
                testItem = "wifi连接";
                testData = "连接成功";
                pcba_test_data_update(testItem, testData, passValue);
                showlog("4、wifi强度测试");
                wifiwaittime->setInterval(wifi_wait_time);
                wifiwaittime->start();
                state = STATE_WATI_GET_CORRECT_WIFIRSSI;
                iswificonnectovertime = 0;
            }

            if (iswificonnectovertime)
            {
                wificonnectwaittime->stop();
                wificonnectwaittimehalf->stop();
                showlog("WIFI连接超时");
                testItem = "wifi连接";
                testData = "连接超时";
                pcba_test_data_update(testItem, testData, failValue);
                wifiwaittime->setInterval(wifi_wait_time);
                wifiwaittime->start();
                totalresult = failValue;
                blewaittime->setInterval(ble_wait_time);
                blewaittime->start();
                at->sendBLELOG(1);   // 日志开
                state = STATE_WATI_GET_CORRECT_BLERSSI;
            }

            break;

        case STATE_WATI_GET_CORRECT_WIFIRSSI:

            if (intwifirssi != 0)
            {
                if (intwifirssi < HighRssi && intwifirssi > LowRssi)   // wifi信号可以
                {
                    rssitestcount++;
                    showlog("WIFI测试" + QString::number(intwifirssi));

                    if (rssitestcount > RssiTestTime)   // wifi信号可以
                    {
                        QString wifiName = "usmile_finish";
                        QString wifiPassword = "12345678";
                        QByteArray wifiNameBytes = wifiName.toUtf8();
                        QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
                        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
                        //   pb->set_wifi_disconnect();
                        testItem = "wifi信号";
                        testData = WIFI_RSSI;
                        pcba_test_data_update(testItem, testData, passValue);
                        state = STATE_WATI_GET_CORRECT_BLERSSI;
                        showlog("5、蓝牙强度测试");
                        at->sendBLELOG(1);   // 日志开
                        rssitestcount = 0;
                    }
                }
                else
                {
                    rssitestcount = 0;
                    rssitestfailcount++;
                    if (rssitestfailcount > RssiTestTime)   // wifi信号不可以
                    {
                        QString wifiName = "usmile_finish";
                        QString wifiPassword = "12345678";
                        QByteArray wifiNameBytes = wifiName.toUtf8();
                        QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
                        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
                        //     pb->set_wifi_disconnect();
                        totalresult = failValue;

                        testItem = "wifi信号";
                        testData = WIFI_RSSI;
                        pcba_test_data_update(testItem, testData, failValue);

                        qDebug() << getIndex() << "wifi不合格信号强度" << intwifirssi;
                        showlog("wifi不合格信号强度" + WIFI_RSSI);
                        rssitestfailcount = 0;
                        state = STATE_WATI_GET_CORRECT_BLERSSI;
                        showlog("5、蓝牙强度测试");
                        at->sendBLELOG(1);   // 日志开
                    }
                }
            }
            waitWork(50);

            break;

        case STATE_WATI_GET_CORRECT_MOTOR:
            if (canGoNext)
            {
                showlog("已收到电机校准参数");
                showlog("5、蓝牙强度测试");
                at->sendBLELOG(1);   // 日志开
                showlog("已发送蓝牙强度获取指令");

                blewaittime->setInterval(ble_wait_time);
                blewaittime->start();
                state = STATE_WATI_GET_CORRECT_BLERSSI;
            }

            break;
        case STATE_WATI_GET_CORRECT_BLERSSI:

            if (intblerssi != 0)
            {
                if (intblerssi < BleHighRssi && intblerssi > BleLowRssi)   // 蓝牙信号可以
                {
                    showlog("蓝牙测试" + QString::number(intblerssi));
                    rssitestcount++;
                    if (rssitestcount >= RssiTestTime)   // 蓝牙信号可以
                    {
                        testItem = "蓝牙信号";
                        testData = BLE_RSSI;
                        pcba_test_data_update(testItem, testData, passValue);

                        showlog("蓝牙测试通过" + QString::number(intblerssi) +
                                                     "测试次数为" + QString::number(rssitestcount));
                        rssitestcount = 0;
                        at->sendBLELOG(0);   // 日志关
                        waitWork(100);       // 防止发送给治具粘包
                        emit overtask(getIndex());
                        showlog("6、等待治具测试");
                        if (pack.product == "P20P")
                        {
                            state = STATE_WATI_LIGHT_TEST;
                        }
                        else
                        {
                            waitWork(WAITTIME);   // 播放音乐
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
                }
                else
                {
                    rssitestcount = 0;
                    rssitestfailcount++;
                    showlog("重试测试蓝牙" + BLE_RSSI);
                    if (rssitestfailcount >= RssiTestTime)   // 蓝牙信号不可以
                    {
                        testItem = "蓝牙信号";
                        testData = BLE_RSSI;
                        pcba_test_data_update(testItem, testData, failValue);

                        totalresult = failValue;
                        qDebug() << getIndex() << "蓝牙不合格信号强度" << BLE_RSSI;
                        qDebug() << getIndex() << "范围为" << BleHighRssi << BleLowRssi;
                        showlog("蓝牙不合格信号强度intblerssi" +
                                                     QString::number(intblerssi));

                        showlog("蓝牙不合格信号强度" + BLE_RSSI);
                        showlog("当前蓝牙范围为" +
                                                     QString::number(BleHighRssi) +
                                                     QString::number(BleLowRssi));

                        at->sendBLELOG(0);   // 日志关
                        rssitestfailcount = 0;

                        waitWork(100);   // 防止发送给治具粘包
                        emit overtask(getIndex());
                        showlog("6、等待治具测试");

                        if (pack.product == "P20P")
                        {
                            state = STATE_WATI_LIGHT_TEST;
                        }
                        else
                        {
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
            if (1)
            {
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
            if (is_music_play_over_time)
            {
                is_music_play_over_time = 0;
                showlog("播放结束");
                if (pack.product == "Q20")
                {
                    showlog("跑的是q20");
                    pb->set_motor_param(270, 60);
                    pb->set_motor_state(1);
                    //   waitWork(1500);
                    //   pb->set_device_mode(3);
                    //       pb->set_brush_control(1);
                }
                else if (pack.product == "P20P")
                {
                    pb->set_sevor_motor_param(14, 12, 5.2, 190);
                    showlog("已经发送电机测试指令");
                }
                else
                {
                    pb->set_device_mode(4);   // 进入亮白
                }

                showlog("已发送进入亮白模式");
                state = STATE_WATI_WHITE_MODE;
            }

            break;

        case STATE_WATI_WHITE_MODE:

            if (pb->getisDevintowhitemode() || pb->get_is_motor_param_set() ||
                pb->get_is_motor_test_state())
            {
                //  pb->reset_all_pb();
                showlog("已进入亮白模式且发送指令给治具");
                waitWork(100);
                emit start_white_modle_command(getIndex());
                waitWork(100);
                emit start_white_modle_command(getIndex());
                waitWork(100);
                emit start_white_modle_command(getIndex());

                at->resetConnected();
                showlog("等待治具请求休眠命令");
                state = STATE_WATI_RETURN_TEST;
            }
            else
            {
                waitWork(500);
                if (pack.product == "Q20")
                {
                    pb->set_motor_param(270, 60);
                    pb->set_motor_state(1);
                    // waitWork(1500);
                    // pb->set_brush_control(1);
                }
                else if (pack.product == "P20P")
                {
                    pb->set_sevor_motor_param(14, 12, 5.2, 190);
                    showlog("已经发送电机测试指令");
                }
                else
                {
                    pb->set_device_mode(4);   // 进入亮白
                }
            }
            break;

        case STATE_WATI_RETURN_TEST:
            if (start_sleep)
            {
                start_sleep = 0;
                pb->set_forbid_sleep(FacSwitch_OPEN);
                showlog("已发送取消禁止休眠");
                state = STATE_CLOSE_FORBID_SLEEP;
            }

            break;

        case STATE_CLOSE_FORBID_SLEEP:
            if (pb->get_is_close_forbid_sleep())
            {
                // if (pack.product == "P20P")
                // {
                //     at->sendMac("00:00:00:00:00:00");   // 发送mac地址
                //     showlog("已经断开蓝牙方式休眠");
                //     state = STATE_SLEEP_OPEN;
                // }
                // else
                // {
                pb->set_sleeep(FacSwitch_OPEN);
                waitWork(100);
                at->sendMac("00:00:00:00:00:00");
                waitWork(100);
                showlog("已经发送请求牙刷休眠");
                state = STATE_SLEEP_OPEN;
                // }
            }
            else
            {
                waitWork(500);
                pb->set_forbid_sleep(FacSwitch_CLOSE);
                showlog("正在重发取消禁止休眠");
            }

            break;
        case STATE_SLEEP_OPEN:
            if (!at->getConnected())
            {
                showlog("开始等待治具测试结果");
                state = STATE_WATI_REMAIN_TEST;
            }
            else
            {
                pb->set_sleeep(FacSwitch_OPEN);
                showlog("正在重发开始休眠");
                at->sendMac("00:00:00:00:00:00");
                waitWork(500);
            }

            break;

        case STATE_WATI_REMAIN_TEST:
        {
            if (remain_ok == 1)   // 设备信息正常
            {
                showlog("治具测试内容获取成功");
                state = STATE_SAVE_RESULT;
            }
            if (remain_ok == 2)   // 不正常
            {
                showlog("治具测试内容获取成功");
                showlog("测试失败");
                totalresult = failValue;
                state = STATE_SAVE_RESULT;
            }
            else
            {
                waitWork(100);
                if (sendcount < 3)
                {
                    emit start_sleep_command(getIndex());
                }
                sendcount++;
            }
        }

        break;

        case STATE_SAVE_RESULT:
        {
            if (totalresult == passValue)
            {
                // QString currentDate =
                // QDateTime::currentDateTime().toString("yyyy年MM月dd日hh时"); QString
                // fileNamemacAddress = macAddress; QString folderPath =
                // QString("LOG/%1/").arg(currentDate); QString fileName = folderPath +
                // QString("%1_上位机号为%2_测试次数为%3.log").arg(fileNamemacAddress.remove(":")).arg(getIndex()).arg(currentTestNumber);
                // deleteFile(fileName);

                ui->pass_number->setText("PASS:" + QString::number(passnumber));
                ui->pass_number->setStyleSheet(
                    "font-size: 16px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 1px; text-align: center;");

                passnumber++;
                pb->set_fac_result(1);
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 66px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
            }
            else if ((totalresult == failValue))
            {
                ui->ng_number->setText("NG:" + QString::number(ngnumber));
                ui->ng_number->setStyleSheet(
                    "font-size: 16px; background-color: #ff557f; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 1px; text-align: center; ");
                ngnumber++;

                pb->set_fac_result(0);
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 66px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
            }

            log->saveTestCsv(PCBA_VER, "", ui->macInput->text(), testItems);
            showlog("保存完毕");
            showlog("测试结束");
            ui->macInput->clear();
            // ui->macInput->setFocus();
            ui->macInput->setDisabled(0);

            isPcbaTestContinue = false;   // 结束

            // pb->set_fac_mode(1);
            // waitWork(300);
            on_disconnectButton_clicked();

            emit endTest(getIndex());

            state = STATE_IDLE;
        }

        break;
        default:

            break;
        }
        // QCoreApplication::processEvents();
    }
}

void PcbaForm::on_pushButton_2_clicked()   // 单机
{
    at->sendBLELOG(1);
    // testItem = "六轴状态";
    // testData = "QString::number(data.imu_state)";

    // pcba_test_data_update(testItem, testData, passValue);

    // pb->set_forbid_sleep(FacSwitch_CLOSE);
    // pb->set_sleeep(FacSwitch_OPEN);
    // waitWork(100);
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(50);
    // on_disconnectButton_clicked();
    // // 3C:84:27:07:A8:D2

    // showlog("已设置休眠");

    // ui->ng_number->setText("NG:"+QString::number(ngnumber));
    // ui->ng_number->setStyleSheet("font-size: 16px; background-color: #FF0000; color: black;
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

    // static int t=1;
    // if(t==1){
    //   pb->set_fac_result(1);
    //   t=0;
    // }else{
    //   pb->set_fac_result(0);
    //   t=1;
    // }

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

void PcbaForm::on_macInput_returnPressed()
{
    clear_display();

    // 检查是否是同一个mac地址进行的连续测试
    if (macAddress != lastMacAddress)
    {
        currentTestNumber = 1;
        lastMacAddress = macAddress;
    }
    else
    {
        currentTestNumber++;
    }

    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }

    // if (!usbserialPort->isOpen()){
    //     openusbSerialPort();
    // }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch())
    {
        at->ask_mac();
        showlog("Mac地址错误，当前为:" + ui->macInput->text());
        //   QMessageBox::warning(nullptr, "Warning", "Mac地址错误:\n\r" + ui->macInput->text());

        // if(mac_retry_flag == false){
        //     mac_retry_flag = true;

        // } else {
        //     mac_retry_flag = false;
        // }
        return;
    }
    else
    {
        // // 执行 USB 断开操作
        // if (pack.product == "P20P" || pack.product == "Q20")
        // {
        //     on_productDisconnectButton_clicked();
        // }

        macAddress = ui->macInput->text();
        usblogwaittime->stop();
        firstconnectbrush = 0;
        ui->macLabel->setText("蓝牙mac: " + macAddress);
        // QSettings settings(SETTING_NAME,QSettings::IniFormat);
        // if(settings.value("User/formColumn").toInt()*settings.value("User/formRow").toInt()==1)
        // start_task();

        isPcbaTestContinue = true;
        state = STATE_IDLE;
        emit goNextFocus();
        ui->macInput->setDisabled(1);

        // thread->start();
    }
}
void PcbaForm::start_test()
{
    ui->test_time->display(0);
    ui->macInput->clear();
    usblogwaittime->setInterval(5000);
    usblogwaittime->start();
    firstconnectbrush = 1;
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 66px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    clear_display();
    on_connectButton_clicked();
    on_productConnectButton_clicked();
};
bool PcbaForm::deleteFile(const QString &filePath)
{
    // 尝试删除文件
    QFile file(filePath);
    if (file.exists())
    {
        if (file.remove())
        {
            qDebug() << "文件删除成功:" << filePath;
            return true;
        }
        else
        {
            qDebug() << "删除文件失败:" << filePath;
            return false;
        }
    }
    else
    {
        qDebug() << "文件不存在:" << filePath;
        return false;
    }
}

void PcbaForm::writeToLogFile(const QByteArray &data, QString currentDate, QString macAddress,
                              int machineNumber)
{
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

    if (logFile.open(QIODevice::Append | QIODevice::Text))
    {
        // qDebug() << "写入成功牙刷日志";
        // 写入数据
        QTextStream out(&logFile);
        out << data << endl;
        logFile.close();
    }
    else
    {
        qDebug() << "无法打开牙刷日志文件：" << fileName;
    }
}
