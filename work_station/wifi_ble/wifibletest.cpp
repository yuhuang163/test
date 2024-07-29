#include "wifibletest.h"
#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_wifibletest.h"



#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
// extern "C"   // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
// {
// #include "lib/nfc/dcrf32.h"
// }

void wifibletest::on_pushButton_clicked()
{
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    // // ui->macInput->setText("3c:84:27:06:f7:5e");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    // // ui->macInput->setText("3c:84:27:29:50:32");
   // ui->macInput->setText("B4:56:5D:BF:53:71");

    on_macInput_returnPressed();
    // // usb-> getlxMEASure();
    // // waitWork(1000);

    // showlog("正在获取牙刷电量");
    // ui->comNameCombo->setCurrentText("COM134");
    showlog("保存mac_sn文件成功");

}

wifibletest::wifibletest(int index, QWidget *parent) : ui(new Ui::wifibletest)
{
    m_index=index;
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();   // 要搜索一下一开始

    // connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    connect(waittime, &QTimer::timeout,
            [=]()
            {
                isovertime = 1;
                waittime->stop();
            });

    connect(comparewaittime, &QTimer::timeout,
            [=]()
            {
                iscompareovertime = 1;
                comparewaittime->stop();
            });

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    HighRssi = settings.value("WIFI/HighRssi").toDouble();
    LowRssi = settings.value("WIFI/LowRssi").toDouble();
    BleHighRssi = settings.value("BLE/HighRssi").toDouble();
    BleLowRssi = settings.value("BLE/LowRssi").toDouble();
    filter_name = settings.value("BLE/Filter_Name").toString();
    standbattary = settings.value("BATTARY/standbattary").toDouble();
    HighCurrent = settings.value("quiescentCurrent/HighCurrent").toDouble();
    LowCurrent = settings.value("quiescentCurrent/LowCurrent").toDouble();
    measure_wait_time = settings.value("Current/measure_wait_time").toInt();

    RssiTestTime = settings.value("BLE/RssiCount").toInt();
    // ui->wifiUserName->setText(settings.value("WIFI/Name", "请在配置文件中设置").toString());
    ui->wifiUserName->setText(
        settings.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(settings.value("WIFI/Password", "usmile123").toString());

    showlog("HighCurrent=" + QString::number(HighCurrent));
    showlog("LowCurrent=" + QString::number(LowCurrent));
    showlog("measure_wait_time=" + QString::number(measure_wait_time));

    showlog("machineNo=" + pack.machineNo);
    showlog("filter_name=" + filter_name);
    showlog("standbattary=" + QString::number(standbattary));
    showlog("model=" + pack.model);
    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);

    if (pack.factory == "lx" || pack.factory == "jj")
    {
        usbBaudRate = 9600;
    }
    else
    {
        usbBaudRate = 115200;
        ui->usbdisconnectButton->setDisabled(true);
        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }

    ui->tabWidget->setTabText(0, "信号测试");


}


wifibletest::~wifibletest()
{
    delete ui;
}

void wifibletest::refreshBaseData(FacGetDevBaseInfo data)
{

    showlog("当前产品名字为"+pack.product);
    if (QString(data.product_name).compare("U7P") == 0 ||
        QString(data.product_name).compare("U7") == 0)
    {
        showlog("开始写入nfc数据");
        if(QString(data.product_name).compare("U7P") == 0 )
        {
            nfcdataHeadText = "033BD2023668772001004800324F3130"+getValueBySN(ui->getMac->text()).toUtf8()+"810800272000141785911410";
            showlog("当前nfc写入的是U7P!");
        }
        if( QString(data.product_name).compare("U7") == 0){
            nfcdataHeadText = "033BD2023668772001004800324F3045"+getValueBySN(ui->getMac->text()).toUtf8()+"810800272000141785911410";
            showlog("当前nfc写入的是U7!");
        }

        on_nfc_write_read_clicked();


    }


    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    // 读取软件版本字符串
    QString softwareVersions = settings.value("ProductInfo/Software_Version").toString();
    qDebug() << "Read Software_Version:" << softwareVersions;
    QStringList softwareVersionList = softwareVersions.split('=');

    // 读取资源版本字符串
    QString resourceVersions = settings.value("ProductInfo/Resource_Version").toString();
    QStringList resourceVersionList = resourceVersions.split('=');

    // 读取老化状态字符串
    QString ageStates = settings.value("ProductInfo/Age_State").toString();
    QStringList ageStateList = ageStates.split('=');

    // 读取蓝牙版本字符串
    QString bleVersions = settings.value("ProductInfo/Ble_Ver").toString();
    QStringList bleVersionList = bleVersions.split('=');

    // 读取压感版本字符串
    QString pressVersions = settings.value("ProductInfo/Pressure_Sense_Version").toString();
    QStringList pressVersionList = pressVersions.split('=');

    // 读取电机版本字符串
    QString motorVersions = settings.value("ProductInfo/Motor_Ver").toString();
    QStringList motorVersionList = motorVersions.split('=');


    // 输出蓝牙状态列表
    for (const QString &version : bleVersionList) {
        qDebug() << "ble Version:" << version.trimmed();
    }

    // 输出软件版本列表
    for (const QString &version : softwareVersionList) {
        qDebug() << "Software Version:" << version.trimmed();
    }

    // 输出资源版本列表
    for (const QString &version : resourceVersionList) {
        qDebug() << "Resource Version:" << version.trimmed();
    }

    // 输出老化状态列表
    for (const QString &state : ageStateList) {
        qDebug() << "Age State:" << state.trimmed();
    }



    wifiMac.clear();
    for (int var = 0; var < data.wifi_mac.size; ++var)
    {
        wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
        if (var < data.wifi_mac.size - 1)
            wifiMac += ":";
    }
    qDebug() << getIndex() << "设备的 wifiMac:" << wifiMac;

    // 检查软件版本、资源版本和老化状态是否匹配
    if (softwareVersionList.contains(data.soft_version) &&
        resourceVersionList.contains(data.res_version) &&
         bleVersionList.contains(data.ble_version) &&

           pressVersionList.contains(data.presure_version) &&
           motorVersionList.contains(data.motor_version) &&

        ageStateList.contains(QString::number(data.ageing_state)))
    {
        showlog("软件版本正确" + QString::fromUtf8(data.soft_version));
        showlog("资源版本正确" + QString::fromUtf8(data.res_version));
        showlog("老化状态正确" + QString::number(data.ageing_state));
        showlog("蓝牙版本正确" + QString::fromUtf8(data.ble_version));

        showlog("压感状态正确" + QString::fromUtf8(data.presure_version));
        showlog("电机版本正确" + QString::fromUtf8(data.motor_version));

        showlog("软件版本正确");

    }
    else
    {

        TestResult = failValue;
        showlog("状态错误");
        showlog("当前设备软件版本" + QString::fromUtf8(data.soft_version) +
                                     "配置文件软件版本" + softwareVersions);
        showlog("当前设备资源版本" + QString::fromUtf8(data.res_version) +
                                     "配置文件资源版本" + resourceVersions);

        showlog("当前设备老化状态" + QString::number(data.ageing_state) + "配置文件老化要求" +
                ageStates);

        showlog("当前设备蓝牙版本" + QString::fromUtf8(data.ble_version) + "配置文件蓝牙要求" +
                                                         bleVersions);


        showlog("当前设备压感版本" + QString::fromUtf8(data.presure_version) + "配置文件压感要求" +
                                                           pressVersions);
        showlog("当前设备电机版本" + QString::fromUtf8(data.motor_version) + "配置文件电机要求" +
                                                           motorVersions);


        iswifibleContinue = false;
        showlog("停止运行");

        ui->macInput->clear();
        ui->getMac->clear();
        ui->getMac->setFocus();
    }
}

void wifibletest::refreshBattaryData(FacDevInfo adc)
{
    QString chargeStateStr;
    switch (adc.dev_info[0].value_item.battery.charge_state)
    {
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
    QString batteryPercentStr = "电量为：<span style='color:blue'>" +
                                QString::number(adc.dev_info[0].value_item.battery.percent) +
                                "%</span>";
    ui->battary_value->setText(batteryPercentStr);

    // 修改电压的显示样式
    QString batteryVoltageStr =
        "电压为：<span style='color:purple'>" +
        QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0, 'f', 3) + "V</span>";
    ui->battary_voltage->setText(batteryVoltageStr);

    voltage = adc.dev_info[0].value_item.battery.voltage / 1000.0;
    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);
    is_battary_test = 1;
    if (adc.dev_info[0].value_item.battery.charge_state == 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary)
    {
        TestItem charge;
        charge.testItem = "充电测试";
        charge.testData =
            "正在充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        charge.testResult = "通过";
        testItems.append(charge);

        charageresult = "通过";
        voltageresult = "通过";
        showlog("电量和充电测试通过");
    }
    if (adc.dev_info[0].value_item.battery.charge_state != 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary)
    {
        TestItem charge;
        charge.testItem = "充电测试";
        charge.testData =
            "不充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        charge.testResult = "失败";
        testItems.append(charge);
        showlog("充电状态不通过");
        charageresult = "失败";
        voltageresult = "通过";
        TestResult = failValue;
    }
    if (adc.dev_info[0].value_item.battery.charge_state == 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary)

    {
        TestItem charge;
        charge.testItem = "充电测试";
        charge.testData =
            "正在充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        charge.testResult = "失败";
        testItems.append(charge);
        showlog("电量测试不通过");
        voltageresult = "失败";
        charageresult = "通过";
        TestResult = failValue;
    }
    if (adc.dev_info[0].value_item.battery.charge_state != 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary)
    {
        TestItem charge;
        charge.testItem = "充电测试";
        charge.testData =
            "不充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        charge.testResult = "失败";
        testItems.append(charge);
        showlog("电量和充电测试都不通过");
        voltageresult = "失败";
        charageresult = "失败";
        TestResult = failValue;
    }
}

void wifibletest::refreshWifiState(int state)
{
    if (state)
    {
        // ui->WIFIStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        //  showlog("WIFI连接成功");
        wifistate = 1;
    }
    else
    {
        //  ui->WIFIStatusLabel->setText("WIFI连接：<font color='red'>失败</font>");
        //  showlog("WIFI连接断开");
        wifistate = 0;
    }
}

void wifibletest::refreshSn(FacDevInfo data)
{
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("芯片存储的尾盖sn:" + stringsn);

    if (stringsn == "")
    {
        QMessageBox::warning(NULL, "警告", " 该设备未绑定sn！\t\r\n");
    }
}
void wifibletest::refreshMesState(int state)
{
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}

void wifibletest::getDongleWifi(QString data)
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
void wifibletest::getDongleVer(QString data)
{
    showlog("当前dongle的版本为：" + data);
}

void wifibletest::refreshBleRssi(QString data)
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

void wifibletest::refreshBleState(int state)
{
    if (state)
    {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        pb->set_forbid_sleep(FacSwitch_OPEN);
        showlog("已发送禁止休眠");
    }
    else
    {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void wifibletest::refreshDongleUartState(int state)
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
void wifibletest::refreshUsbUartState(int state)
{
    if (state)
        showlog("usb串口连接成功");
    else
    {
        showlog("usb串口连接断开");

        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }
}
void wifibletest::refreshAmmeterData(QString data)
{
    qDebug() << getIndex() << "收到电流数据" << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk) / 100;

    if (conversionOk)
    {
        // 转换成功
        qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
        QString formattedValue = QString::number(normalValue, 'f', 4);
        qDebug() << getIndex() << "转换后的数值：" << formattedValue << "ma";
        // ui->log->appendPlainText(formattedValue+"ma");
        showlog(formattedValue + "ma");
    }
    else
    {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

void wifibletest::solveMesSucess(const int mechines)
{
    if (mechines == getIndex())
    {
        showlog("mes操作成功");
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");

        mes_set_ok = 1;
    }
}
void wifibletest::solveMesData(const int mechines, QString msg)
{
    if (mechines == getIndex())
    {
        showlog("MES:报错信息:" + msg);
        ui->macInput->setDisabled(0);
        ui->getMac->setDisabled(0);
        iswifibleContinue = false;
        showlog("停止运行");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

        bandingresult = false;
        emit send_end_test(getIndex());

        ui->getMac->clear();
        ui->getMac->setFocus();
    }
}

void wifibletest::closeEvent(QCloseEvent *event)
{
    qDebug() << getIndex() << "开始关闭";
    iswifibleContinue = false;
}

void wifibletest::getWifiMsg(QString data)
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
            refreshWifiState(1);
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
void wifibletest::initDate()
{
    ui->tail_sn->setText("芯片存储的尾盖sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;
    testItems.clear();
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
wifibletest::State wifibletest::getNextState(State currentState)
{
    return static_cast<State>((static_cast<int>(currentState) + 1) % 5);
}
void wifibletest::startTask()
{
    if (iswifibleContinue)
    {
        ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state)
        {
        case STATE_IDLE:   // 复位一切
            initDate();
            at->sendMac(macAddress);   // 开始连接
            showlog("开始测试");
            state = getNextState(state);
            break;
        case STATE_WATI_CONNECT:   // 设置禁止休眠
            if (at->getConnected())
            {
                showlog("蓝牙连接成功");
                sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
                state = getNextState(state);
            }
            break;

        case STATE_DISABLE_SLEEP_1:   // 设置设备采集
            if (canGoNext)
            {
                sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb));
                showlog("已进入禁止休眠");
                state = getNextState(state);
            }
            break;

        case STATE_WATI_BASE_INFO:
            if (canGoNext)
            {
                showlog("已检查牙刷状态");
                showlog("打开蓝牙日志");
                at->sendBLELOG(1);   // 日志开
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
                        TestItem bleRssi;
                        bleRssi.testItem = "蓝牙信号强度测试";
                        bleRssi.testData = BLE_RSSI;
                        bleRssi.testResult = "通过";
                        testItems.append(bleRssi);
                        showlog("蓝牙测试通过" + QString::number(intblerssi) +
                                                     "测试次数为" + QString::number(rssitestcount));

                        rssitestcount = 0;
                        at->sendBLELOG(0);   // 日志关
                        if (pack.product== "P20P" || pack.product== "U7"|| pack.product== "U7P")
                        {
                            sendCommandWithRetry(std::bind(&Qpb::get_battery, pb));
                            state = STATE_WATI_CORRECT_BATTARY;
                        }
                        else
                        {
                            on_connectwifi_clicked();
                            state = STATE_WATI_WIFI_CONNECT;
                        }
                    }
                }
                else
                {
                    rssitestcount = 0;
                    rssitestfailcount++;

                    if (rssitestfailcount >= RssiTestTime)   // 蓝牙信号不可以
                    {
                        TestItem bleRssi;

                        bleRssi.testItem = "蓝牙信号强度测试";
                        bleRssi.testData = BLE_RSSI;
                        bleRssi.testResult = "失败";
                        testItems.append(bleRssi);
                        TestResult = failValue;
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

                        if (pack.product== "P20P" || pack.product== "U7"|| pack.product== "U7P")
                        {
                            wifiresult = "通过";
                            sendCommandWithRetry(std::bind(&Qpb::get_battery, pb));
                            state = STATE_WATI_CORRECT_BATTARY;
                        }
                        else
                        {
                            on_connectwifi_clicked();
                            state = STATE_WATI_WIFI_CONNECT;
                        }
                    }
                }
            }
            waitWork(100);

            break;

        case STATE_WATI_WIFI_CONNECT:

            if (wifistate)   // wifi
            {
                showlog("wifi连接成功");
                state = STATE_WATI_GET_CORRECT_WIFIRSSI;
            }

            // waitWork(1500);//防止重发
            // if (!pb->get_is_wifi_set_ok())   // wifi
            // {
            //      showlog("正在重连wifi");
            //      on_connectwifi_clicked();
            // }

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
                        wifiresult = "通过";
                        TestItem wifiRssi;

                        wifiRssi.testItem = "WIFI信号强度";
                        wifiRssi.testData = WIFI_RSSI;
                        wifiRssi.testResult = "通过";
                        testItems.append(wifiRssi);
                        sendCommandWithRetry(std::bind(&Qpb::get_battery, pb));
                        state = STATE_WATI_CORRECT_BATTARY;
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
                        TestResult = failValue;
                        TestItem wifiRssi;

                        wifiRssi.testItem = "WIFI信号强度";
                        wifiRssi.testData = WIFI_RSSI;
                        wifiRssi.testResult = TestResult;
                        testItems.append(wifiRssi);
                        qDebug() << getIndex() << "wifi不合格信号强度" << intwifirssi;
                        showlog("wifi不合格信号强度" + WIFI_RSSI);
                        rssitestfailcount = 0;
                        sendCommandWithRetry(std::bind(&Qpb::get_battery, pb));
                        state = STATE_WATI_CORRECT_BATTARY;
                    }
                }
            }
            waitWork(100);

            break;

        case STATE_WATI_CORRECT_BATTARY:   // 设置禁止休眠
            if (is_battary_test)
            {
                showlog("已成功完成电量测试");
                if (pack.factory == "lx" || pack.factory == "jj")
                {
                    state = STATE_WATI_CORRECT_CURRENT;
                    qDebug() << getIndex() << "开始计时" << QDateTime::currentDateTime();
                    waittime->setInterval(measure_wait_time);
                    waittime->start();
                }
                else{
                    state = STATE_SAVE_RESULT;
                }
            }

            break;

        case STATE_WATI_CORRECT_CURRENT:   // 设置禁止休眠

            if (isovertime &&
                (measure_ammeter < LowCurrent || measure_ammeter > HighCurrent))   // 失败
            {
                waittime->stop();
                showlog("充电电流测试失败：超时");
                showlog("电流测量值为" + QString::number(measure_ammeter));
                currentresult = "失败";
                TestItem chargeAmmeter;

                chargeAmmeter.testItem = "充电电流";
                chargeAmmeter.testData = measure_ammeter;
                chargeAmmeter.testResult = currentresult;
                testItems.append(chargeAmmeter);
                TestResult = failValue;
                state = STATE_SAVE_RESULT;

                break;
            }
            if (measure_ammeter > LowCurrent && measure_ammeter < HighCurrent)   // 成功
            {
                showlog("充电电流正常");
                waittime->stop();
                currentresult = "通过";
                state = STATE_SAVE_RESULT;
                TestItem chargeAmmeter;
                chargeAmmeter.testItem = "充电电流";
                chargeAmmeter.testData = measure_ammeter;
                chargeAmmeter.testResult = currentresult;
                testItems.append(chargeAmmeter);
            }
            else
            {
                if (pack.factory == "lx" || pack.factory == "jj")
                {
                    usb->getlxMEASure(getIndex());
                    showlog("等待治具测量充电电流");
                    waitWork(1000);
                }
            }
            break;

        case STATE_SAVE_RESULT:

            if (TestResult == failValue)
            {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

                pack.itemvalue = QString("|CHARGE_CURRENT:%1").arg(measure_ammeter) +
                                 QString("|WIFI_TEST:%1|").arg(intwifirssi) +
                                 QString("BLE_TEST:%1").arg(intblerssi) +
                                 QString("|CHAR_TEST:%1|").arg(chargestate) +
                                 QString("VOL_TEST:%1|").arg(voltage);
                pack.result = "NG";

                pack.sn = ui->getMac->text();

                pack.instruct_num = "079";
                if (ui->isusemes->checkState())
                {
                    send_end_testPass(pack);
                }
            }
            else
            {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
                pack.result = "PASS";

                pack.itemvalue = QString("|CHARGE_CURRENT:%1").arg(measure_ammeter) +
                                 QString("|WIFI_TEST:%1|").arg(intwifirssi) +
                                 QString("BLE_TEST:%1").arg(intblerssi) +
                                 QString("|CHAR_TEST:%1|").arg(chargestate) +
                                 QString("VOL_TEST:%1|").arg(voltage);
                pack.sn = ui->getMac->text();

                pack.instruct_num = "079";
                if (ui->isusemes->checkState())
                {
                    send_end_testPass(pack);
                }
            }

            if (pack.product== "P20P" || pack.product== "U7"|| pack.product== "U7P")
            {
                at->sendMac("00:00:00:00:00:00");   // 发送mac地址
                waitWork(50);
                on_disconnectButton_clicked();
            }

            else if (getIndex() != 1)
            {
                at->sendMac("00:00:00:00:00:00");   // 发送mac地址
                waitWork(50);
                // on_disconnectButton_clicked();
            }

            showlog("测试结束");
            // log->saveRssiDataToCsv(measure_ammeter, intwifirssi, intblerssi, wifiresult,
            //                            bleresult, charageresult, voltageresult);

            log->saveTestCsv(SINGLE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
            state = STATE_IDLE;
            iswifibleContinue = false;
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




void wifibletest::on_disconnectwifi_clicked()
{
    if (at->getConnected())
    {
        pb->set_wifi_disconnect();
        showlog("已设置断开wifi");
    }
    else
    {
        showlog("请等待连接牙刷后再试");
    }
}
void wifibletest::on_connectwifi_clicked()
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

void wifibletest::on_macInput_returnPressed()
{
    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }

    if (pack.factory == "lx" || pack.factory == "jj")
    {
        if (!usbSerialPort->isOpen())
        {
            openUsbSerialPort();
        }
    }

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch())
    {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    }
    else
    {
        macAddress = ui->macInput->text();
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

        ui->start_wifible_test->setEnabled(false);
        // 主状态机流程
        iswifibleContinue = true;
        state = STATE_IDLE;

        emit send_go_next_focus();
    }
}

void wifibletest::on_pushButton_2_clicked()
{
    at->sendBLELOG(1);   // 日志开
}

void wifibletest::on_getMac_returnPressed()
{
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

       // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);

    // 使用正则表达式匹配
    if (!snRegex.match(ui->getMac->text()).hasMatch())
    {
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("序列号错误");
        ui->getMac->clear();
        return;
    }
    showlog("正在查询mac地址");
    getMac(ui->getMac->text());             // 文件获取
    processInspection(ui->getMac->text());   // 站前检测
    processGetMesTestValue();                 // mes获取
}

void wifibletest::processInspection(QString stringsn)
{
    if (stringsn != "" || !ui->isusemes->checkState())
    {
        if (ui->isusemes->checkState())
        {
            showlog("正在进行站前检测");
            pack.sn = stringsn;

            pack.mechines = getIndex();

            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit sendProcessInspection(pack);
        }
    }
    else
    {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState())   // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void wifibletest::processGetMesTestValue()
{
     if (ui->isformmes->checkState())
    {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}
void wifibletest::getMac(QString sn_to_search)
{
    QFile file("mac_sn.txt");   // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly))
    {   // 打开文件
        QTextStream in(&file);
        while (!in.atEnd())
        {                                           // 逐行读取文件
            QString line = in.readLine();           // 读取一行
            QStringList fields = line.split(",");   // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2)
            {
                QString sn = fields.at(0);    // 第一个字段是sn
                QString mac = fields.at(1);   // 第二个字段是mac
                if (sn == sn_to_search)
                {   // 检查是否是待检索的sn
                    {
                        ui->macInput->setText(mac);
                         on_macInput_returnPressed();
                        showlog("这是从文件获取的mac地址");
                        qDebug() << getIndex() << "The corresponding mac is: " << mac;
                    }

                    break;
                }
            }
        }
        file.close();   // 关闭文件
    }
}
void wifibletest::getmacadress(const QByteArray &byte)
{
    receivedData = "";
    receivedData = receivedData + QString::fromUtf8(byte);

    if (receivedData.length() >= 2 && receivedData.right(2) == "\r\n")
    {
        // 使用正则表达式提取设备信息
        QRegularExpression regex(
            "deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
        QRegularExpressionMatch match = regex.match(receivedData);
        QString deviceName, deviceAddress, deviceRssi;

        // qDebug() << getIndex()<< "数据长度" << receivedData;
        // 检查是否匹配成功
        if (match.hasMatch())
        {
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
void wifibletest::on_clear_scan_clicked()
{
    deviceMap.clear();
    ui->mac_combo->clear();
}
void wifibletest::updateComboBox()
{
    // 遍历设备信息，根据 rssi 的值进行过滤
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it)
    {
        QString deviceAddress = it.key();
        QString deviceName = it.value()["Name"];
        QString deviceRssi = it.value()["Rssi"];

        if (deviceName.contains(ui->name_range->text()) &&
            deviceRssi.toInt() > ui->rssi_range->text().toInt() && deviceAddress.length() == 17)

        {
            int index = ui->mac_combo->findText(deviceAddress);
            qDebug() << getIndex() << "信号强度:" << deviceRssi;
            if (index == -1)
            {
                ui->mac_combo->addItem(deviceAddress);
                qDebug() << getIndex() << "有新增" << deviceAddress;
            }
        }
    }
}
void wifibletest::on_mac_combo_textActivated(const QString &arg1)
{
    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch())
    {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    }
    else
    {
        macAddress = arg1;
        // at->sendMac(macAddress);//发送mac地址
        qDebug() << getIndex() << macAddress;
        bandingMacSn(macAddress, snbanding);
    }
    ui->snbanding->setFocus();
}
void wifibletest::bandingMacSn(QString bandingmac, QString bandingsn)
{
    if (bandingsn == "" || bandingmac == "")
        bandingresult = false;

    QString path = "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt";
    QFileInfo checkPath(path);
    if (checkPath.exists() && checkPath.isDir())
    {
        path = "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt";
        qDebug() << "The network path exists and is a directory.";
    }
    else
    {
        path = "mac_sn.txt";
        qDebug() << "The network path does not exist or is not a directory.";
    }

    // 在 Windows 上，使用 QDir::fromNativeSeparators 将路径中的反斜杠转换为正斜杠
    // path = QDir::fromNativeSeparators(path);
    QFile file(path);   // 创建一个文件对象

    if (file.open(QIODevice::ReadWrite | QIODevice::Text))
    {                            //
        QTextStream in(&file);   // 创建一个文本流对象
        QStringList lines;       // 用于存储文件中的每一行数据
        bool found = false;      // 标记是否找到了相同的SN
        while (!in.atEnd())
        {
            QString line = in.readLine();          // 逐行读取文件
            QStringList parts = line.split(",");   // 以逗号分隔每行数据
            if (parts.size() == 2 && parts[0].trimmed() == bandingsn)
            {
                // 如果找到了相同的SN，替换MAC地址
                lines << (bandingsn + "," + bandingmac);
                found = true;
            }
            else
            {
                // 否则，保留原有数据
                lines << line;
            }
        }
        if (!found)
        {
            // 如果没有找到相同的SN，则追加新的SN和MAC地址
            lines << (bandingsn + "," + bandingmac);
        }
        // 清空文件并写入新的数据
        file.resize(0);
        QTextStream out(&file);
        for (const QString &line : lines)
        {
            out << line << endl;
        }
        file.close();   // 关闭文件
        showlog("保存mac_sn文件成功");
    }
    else
    {
        showlog("保存mac_sn文件失败");
    }

    // QFile file("mac_sn.txt");   // 创建一个文件对象
    // if (file.open(QIODevice::ReadWrite | QIODevice::Append))
    // {                                                    //
    //     QTextStream out(&file);                          // 创建一个文本流对象
    //     out << bandingsn << "," << bandingmac << endl;   // 将sn和mac写入文件
    //     file.close();                                    // 关闭文件
    // }

    bandingMacSn_mes(bandingmac, bandingsn);
}

void wifibletest::bandingMacSn_mes(QString bandingmac, QString bandingsn)
{
    pack.mechines = 1;   // 1脱1,1号上位机
    pack.sn = snbanding;

    pack.result = "PASS";

    pack.itemvalue = QString("|BTMAC:%1|").arg(bandingmac);

    pack.instruct_num = "076";

    if (ui->isusemes->checkState())
    {
        send_end_testPass(pack);
    }

    if (bandingresult)
    {
        ui->banding_result->setText("绑定:PASS");
        ui->banding_result->setStyleSheet(
            "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
    }
    else
    {
        ui->banding_result->setText("绑定:NG");
        ui->banding_result->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
    ui->snbanding->setFocus();
}
void wifibletest::on_snbanding_returnPressed()
{
    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    ui->snbanding->clear();
    bandingresult = true;
}

void wifibletest::getTestValue(const int mechines, const QString value)
{
    // showlog(value);
    QString mesmacAddress;
    if (pack.factory == "hq")
    {
        // 定义正则表达式，匹配MAC地址的模式
        QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");

        // 在数据中查找匹配的内容
        QRegularExpressionMatch match = regex.match(value);

        // 检查是否有匹配项
        if (match.hasMatch())
        {
            // 提取MAC地址
            mesmacAddress = match.captured(1);
            qDebug() << getIndex() << "MAC地址：" << mesmacAddress;
            if (mechines == getIndex())
            {
                ui->macInput->setText(mesmacAddress);
                on_macInput_returnPressed();
            }
        }
        else
        {
            showlog("mes未找到匹配的MAC地址");
            showlog(value);
        }
    }
    // showlog(value);
    else if (pack.factory == "lx")
    {
        mesmacAddress = value;

        // 在2、4、6、8、10的位置插入冒号
        mesmacAddress.insert(2, ":");
        mesmacAddress.insert(5, ":");
        mesmacAddress.insert(8, ":");
        mesmacAddress.insert(11, ":");
        mesmacAddress.insert(14, ":");

        // 将小写字母转换成大写字母
        mesmacAddress = mesmacAddress.toUpper();
        if (mechines == getIndex())
        {
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }
    else
    {
        if (mechines == getIndex())
        {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }

    // bandingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试mac——sn
}
void wifibletest::on_clear_nfc_data_clicked()
{
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8] = {0x03, 0x00, 0xFE, 0x00};   // 写入数据缓冲区
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport=100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport= parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);

    if ((intptr_t)icdev <= 0)
    {
        showlog("Init Com Error!");
        return;
    }
    else
    {
        showlog("Init Com OK!");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0)
    {
        showlog("dc_card_n Error!");
        return;
    }
    else
    {
        showlog("dc_card_n Ok!");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char *)szSnr;
        showlog(QString::fromStdString(str1));
    }

    int ret = dc_write(icdev, 4, writedata);   // 将写入数据缓冲区中的数据写入设备

    if (ret != 0)
    {
        QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
        qDebug() << getIndex() << "errMsg: " << writedata << errMsg;
    }

    st = dc_read(icdev, 4, rdata);
    if (st != 0)
    {
        showlog("dc_read Error!");
        return;
    }
    else
    {
        memset(rdatahex, 0x00, sizeof(rdatahex));
        hex_a(rdata, rdatahex, 4);
        std::string str1 = (char *)rdatahex;
        showlog(QString::fromStdString(str1));
    }

    if ((intptr_t)icdev > 0)
    {
        st = dc_exit(icdev);
        if (st != 0)
        {
            showlog("dc_exit Error!");
            return;
        }
        else
        {
            showlog("dc_exit OK!");
            icdev = (HANDLE)-1;
        }
    }
    return;
}
QString wifibletest::generateDateCode()
{
    // 获取当前日期时间
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // 获取当前年份、月份和日期
    int year = currentDateTime.date().year() % 100;   // 获取后两位年份
    int month = currentDateTime.date().month();
    int day = currentDateTime.date().day();

    // 月份编码
    char monthCode;
    if (month >= 1 && month <= 9)
    {
        monthCode = '0' + month;
    }
    else
    {
        monthCode = 'A' + (month - 10);
    }

    // 日期编码
    char dayCode;
    if (day >= 1 && day <= 9)
    {
        dayCode = '0' + day;
    }
    else
    {
        dayCode = 'A' + (day - 10);
    }

    // 构造日期编码
    QString dateCode = QString::number(year) + monthCode + dayCode;
    return dateCode;
}
QString wifibletest::generateHexString(int productionNumber)
{
    // 构造生产数量字符串（4位数，不足位数前面补0）
    QString productionStr = QString("%1").arg(productionNumber, 4, 16, QChar('0'));

    // 构造十六进制字符串
    sku = "55040701";

    QString hexString = sku;           // 固定部分
    hexString += generateDateCode();   // 日期部分
    hexString += productionStr;        // 生产数量
    qDebug() << getIndex() << "nfc的序列号: " << hexString;

    return hexString;
}

QString wifibletest::getValueBySN(const QString &sn ) {

    QString truncatedSN = sn.left(8);
    showlog("truncatedSN:"+truncatedSN);

    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString value =  settings.value("SUBPID/"+truncatedSN, "SUBPID_ERRO").toString();
    showlog("匹配到的subpid："+value);


    if("SUBPID_ERRO"==value){
        QMessageBox::warning(nullptr, "Warning", "没匹配到subpid");

    }

    return value;
}
void wifibletest::on_nfc_write_read_clicked()
{
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8];   // 写入数据缓冲区
    ReadNfcData = "";
    // 033BD2023668772001004800324F30450081080027200014178591141035353034303730313233313131313131170102910B0101010A063C842707A8D1
    // 3C842707A8D1
    // 35353034303730313233313131313131
    // 5504070123111111    //55040701华为固定开头sku2311年月日1111数量
    QString hexString;

    static int productionNumber = ui->nfc_count->text().toInt();   // 记录生产数量
    // QString text = generateHexString(productionNumber++);//自己生
    QString text = ui->getMac->text();   // 外部给

    ui->nfc_count->setText(QString::number(productionNumber));

    for (int i = 0; i < text.length(); ++i)
    {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0'));
    }

      QString  nfcdataText = nfcdataHeadText+ hexString.toUpper() +
            "170102910B0101010A06" + macAddress.remove(":").toUpper();


    showlog("写入的nfc数据为"+nfcdataText);
    QByteArray dataBytes =
        QByteArray::fromHex(nfcdataText.toLatin1());   // 将十六进制字符串转换为字节数组
    int dataSize = dataBytes.size();                // 获取字节数组的大小
    qDebug() << getIndex() << "dataSize: " << dataSize << nfcdataText;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport=100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport= parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    if ((intptr_t)icdev <= 0)
    {
        showlog("初始化nfc接口失败!");
        TestResult = failValue;
        return;
    }
    else
    {
        showlog("初始化nfc接口成功");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0)
    {
        if (st == 1)
            showlog("nfc卡识别不到");
        if (st < 0)
            showlog("nfc卡查询失败");

        TestResult = failValue;
        return;
    }
    else
    {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char *)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 0; i < dataSize; i += 4)
    {                                               // 每次处理8个字节
        int bytesToWrite = qMin(8, dataSize - i);   // 计算本次需要写入的字节数，最多8个

        memcpy(writedata, dataBytes.constData() + i, bytesToWrite);   // 将数据复制到写入数据缓冲区

        int ret = dc_write(icdev, 4 + i / 4, writedata);   // 将写入数据缓冲区中的数据写入设备

        if (ret != 0)
        {
            QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
            showlog(errMsg);

            qDebug() << getIndex() << "errMsg: " << writedata << errMsg;
        }
    }
    showlog("nfc信息读取内容为：");
    for (int i = 4; i * 4 < dataSize; i += 4)
    {   // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0)
        {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            TestResult = failValue;
            return;
        }
        else
        {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char *)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16)
    {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0)
        {
            showlog("nfc信息读取失败");
            TestResult = failValue;
            //  showlog("dc_read Error!");
            return;
        }
        else
        {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char *)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            showlog(QString::fromStdString(str1));
        }
    }
    showlog("nfc信息读取结束");
    if (ReadNfcData == nfcdataText)
    {
        showlog("写入的与读取的比对通过");
    }
    else
    {
        showlog("写入的与读取的比对失败");
        TestResult = failValue;
    }
    if ((intptr_t)icdev > 0)
    {
        st = dc_exit(icdev);
        if (st != 0)
        {
            TestResult = failValue;
            showlog("nfc退出失败");
            return;
        }
        else
        {
            showlog("nfc退出成功!");
            icdev = (HANDLE)-1;
        }
    }
}

void wifibletest::on_connectButton_clicked()
{
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void wifibletest::on_disconnectButton_clicked()
{
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}

void wifibletest::on_stopTest_clicked()
{
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

void wifibletest::on_nfc_read_clicked()
{
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    ReadNfcData = "";
    int dataSize =62;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport=100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport= parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    if ((intptr_t)icdev <= 0)
    {
        showlog("初始化nfc接口失败!");
        TestResult = failValue;
        return;
    }
    else
    {
        showlog("初始化nfc接口成功");
    }

    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0)
    {
        if (st == 1)
            showlog("nfc卡识别不到");
        if (st < 0)
            showlog("nfc卡查询失败");

        TestResult = failValue;
        return;
    }
    else
    {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char *)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 4; i * 4 < dataSize; i += 4)
    {   // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0)
        {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            TestResult = failValue;
            return;
        }
        else
        {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char *)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
         //   showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16)
    {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0)
        {
            showlog("nfc信息读取失败");
            TestResult = failValue;
            //  showlog("dc_read Error!");
            return;
        }
        else
        {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char *)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
          //  showlog(QString::fromStdString(str1));
        }
    }
    showlog("nfc信息读取结束");
    showlog("nfc内容为："+ReadNfcData);

}

//写入
void wifibletest::on_nfc_encode_clicked()
{
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    int nfcport=100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport= parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    st = dc_swr_eeprom(icdev, 0, 8, (unsigned char *)"READER_A");
    if (st != 0)
    {
        showlog("nfc烧录器写入失败");
    }
    else
    {
        showlog("nfc烧录器写入成功");
    }

}

//读取
void wifibletest::on_nfc_decode_clicked()
{
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];
    int nfcport=100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport= parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    st = dc_srd_eeprom(icdev, 0, 8, buff_1);
    if (st != 0)
    {
        showlog("nfc烧录器读取失败");
    }
    else
    {
        showlog("nfc烧录器读取成功");
        // 将 buff_1 数组转换为 QString
        QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
        // 输出整个字符串
        qDebug() << "nfc设备为:" << buffStr;
    }

}


void wifibletest::on_nfcComFresh_clicked()
{
    updateHIDComboBox(getNfcComboBox());
}





void wifibletest::on_get_battery_clicked()
{
    if (at->getConnected())
    {
        pb->get_battery();
        showlog("正在获取牙刷电量");
    }
    else
    {
        showlog("请等待连接牙刷后再试");
    }
}

