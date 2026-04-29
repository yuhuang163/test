#include "qfreework.h"

#include "ui_qfreework.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
extern "C"  // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}
QFreeWork::QFreeWork(int index, QWidget* parent) : test_base(parent), ui(new Ui::QFreeWork) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = FREE_VER;
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();  // 要搜索一下一开始
    // connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
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
        connect(usb, SIGNAL(send_ammeter_data(QString)), this, SLOT(refreshAmmeterData(QString)));
    } else {
        usbBaudRate = 115200;
        ui->usbdisconnectButton->setDisabled(true);
        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }

    ui->tabWidget->setTabText(0, "自由工站");
    createTestFunctions();
    refreshOrderedTestIndexes();
    testResultTableInit();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}
void QFreeWork::refreshOrderedTestIndexes() {
    orderedTestIndexes_.clear();
    const QVector<int> indexes = loadIndexesFromConfig();
    for (int index : indexes) {
        if (index >= 0 && index < static_cast<int>(testFunctions.size()) && !orderedTestIndexes_.contains(index)) {
            orderedTestIndexes_.append(index);
        }
    }
    if (orderedTestIndexes_.isEmpty()) {
        for (int i = 0; i < static_cast<int>(testFunctions.size()); ++i) {
            orderedTestIndexes_.append(i);
        }
    }
}

QVector<int> QFreeWork::loadIndexesFromConfig() {
    QVector<int> indexes;

    SETTINGS.beginGroup("TestOrder");         // 打开 "TestOrder" 分组
    QStringList keys = SETTINGS.childKeys();  // 获取分组中的所有键

    for (const QString& key : keys) {
        indexes.append(SETTINGS.value(key).toInt());  // 读取每个键的值并转换为整数
    }
    SETTINGS.endGroup();

    qDebug() << "测试顺序已从配置文件中加载:" << indexes;
    return indexes;
}
// 获取下一个状态的函数
QFreeWork::State QFreeWork::getNextState(State currentState) {
    return static_cast<State>((static_cast<int>(currentState) + 1) % 5);
}

void QFreeWork::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        if (teststate == -1) {
            showlog("开始测试");
            initDate();
            waitWork(1000);
            at->sendMac(macAddress);  // 开始连接
            showlog("MAC地址为：" + ui->macInput->text());
            teststate++;
        }
        if (at->getConnected()) {
            for (; teststate < orderedTestIndexes_.count();) {
                // qDebug() << "程序在跑" << teststate;
                if (canGoNext) {
                    const int functionIndex = orderedTestIndexes_.at(teststate);
                    const QString functionName = testFunctions.at(functionIndex).name;
                    showlog("开始测试内容：" + functionName);
                    executeFunctionByName(functionName);  //执行操作
                    qDebug() << "程序在跑" << teststate << orderedTestIndexes_.count();

                    if (teststate >= 1) {
                        const int prevFunctionIndex = orderedTestIndexes_.at(teststate - 1);
                        TestItem test;
                        test.testItem = testFunctions.at(prevFunctionIndex).name;
                        test.testData = "";
                        test.testResult = "通过";
                        test.ask = "通过";
                        testItems.append(test);

                        testResultTableUpdate(testItems);
                    }
                    ++teststate;
                }

                break;
            }
        }

        if (teststate == orderedTestIndexes_.count() && teststate != 0) {
            const int lastFunctionIndex = orderedTestIndexes_.at(teststate - 1);
            TestItem test;
            test.testItem = testFunctions.at(lastFunctionIndex).name;
            test.testData = "";
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            if (TestResult == failValue) {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");
                pack.itemvalue = QString("|FREE_TEST:PASS|");
                pack.result = "NG";
                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                if (ui->isusemes->checkState()) {
                    emit send_end_testPass(pack);
                }
            } else {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
                pack.result = "PASS";
                pack.itemvalue = QString("|FREE_TEST:FAIL|");
                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                if (ui->isusemes->checkState()) {
                    emit send_end_testPass(pack);
                }
            }

            qDebug() << "测试结束";
            teststate = -1;
            ui->macInput->clear();
            ui->snInput->clear();
            ui->nfc_sn->clear();
            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);
            waitWork(50);
            on_disconnectButton_clicked();
            emit send_end_test(getIndex());
            ui->getMac->clear();
            isTestContinue = false;
        }
    }
}

QFreeWork::~QFreeWork() { delete ui; }

void QFreeWork::refreshBaseData(ProtocolBaseInfoData data) {
    QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
    QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();
    QString Age_State = SETTINGS.value("ProductInfo/Age_State").toString();
    product = data.product_name;
    if (QString(data.product_name).compare("U7P") == 0 || QString(data.product_name).compare("U7") == 0) {
        // sku = "55040701";
        showlog("开始写入nfc数据");

        on_nfc_write_read_clicked();
    } else {
    }
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
    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);
    is_battary_test = 1;
    if (adc.chargeState == 2 && adc.voltageMv / 1000.0 > standbattary) {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "正在充电" + QString::number(adc.voltageMv / 1000.0) + "V";
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        charageresult = "通过";
        voltageresult = "通过";
        showlog("电量和充电测试通过");
    }
    if (adc.chargeState != 2 && adc.voltageMv / 1000.0 > standbattary) {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "不充电" + QString::number(adc.voltageMv / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("充电状态不通过");
        charageresult = "失败";
        voltageresult = "通过";
        TestResult = failValue;
    }
    if (adc.chargeState == 2 && adc.voltageMv / 1000.0 <= standbattary)

    {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "正在充电" + QString::number(adc.voltageMv / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("电量测试不通过");
        voltageresult = "失败";
        charageresult = "通过";
        TestResult = failValue;
    }
    if (adc.chargeState != 2 && adc.voltageMv / 1000.0 <= standbattary) {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "不充电" + QString::number(adc.voltageMv / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("电量和充电测试都不通过");
        voltageresult = "失败";
        charageresult = "失败";
        TestResult = failValue;
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
    stringsn = data.value;
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->product_sn->setText("芯片存储的整机sn:" + stringsn);

    // if (stringsn == "")
    // {
    //     QMessageBox::warning(NULL, "警告", " 该设备未绑定sn！\t\r\n");
    // }
}

void QFreeWork::getDongleWifi(QString data) {
    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
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

void QFreeWork::refreshBleState(int state) {
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

void QFreeWork::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}
void QFreeWork::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        showlog("usb串口连接断开");

        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }
}
void QFreeWork::refreshAmmeterData(QString data) {
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
void QFreeWork::initDate() {
    ui->product_sn->setText("芯片存储的整机sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;

    rssitestfailcount = 0;
    wifistate = 0;
    measure_ammeter = 0;
    dongleOutTime = 10;
    canGoNext = 1;
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

void QFreeWork::on_pushButton_clicked() {
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    // // ui->macInput->setText("3c:84:27:06:f7:5e");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    // // ui->macInput->setText("3c:84:27:29:50:32");
    ui->macInput->setText("b4:56:5d:bf:57:9d");

    on_macInput_returnPressed();
    // // usb-> getlxMEASure();
    // // waitWork(1000);

    // showlog("正在获取设备电量");
    // ui->comNameCombo->setCurrentText("COM134");
}

void QFreeWork::on_get_battery_clicked() {
    if (at->getConnected()) {
        protocolManager.get(DeviceCmd::GetBattery);
        showlog("正在获取设备电量");
    } else {
        showlog("请等待连接设备后再试");
    }
}

void QFreeWork::on_disconnectwifi_clicked() {
    if (at->getConnected()) {
        protocolManager.set(DeviceCmd::WifiDisconnect);
        showlog("已发送断开wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}
void QFreeWork::on_connectwifi_clicked() {
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

void QFreeWork::on_macInput_returnPressed() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    if (pack.factory == "lx" || pack.factory == "jj") {
        if (!usbSerialPort->isOpen()) {
            openUsbSerialPort();
        }
    }

    // 检查是否是mac格式
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        if (ui->just_banding->checkState()) {
            bandingMacSn(macAddress, ui->getMac->text());  //获取测试数据不要绑定测试mac——sn
            ui->test_result->setText("PASS");
            ui->test_result->setStyleSheet(
                "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                "border-radius: 10px; padding: 10px; text-align: center;");
            ui->getMac->clear();
            ui->getMac->setFocus();

            return;
        }

        ui->macLabel->setText("蓝牙mac: " + macAddress);

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: "
                                       "10px; padding: 10px; text-align: center; ");

        ui->start_wifible_test->setEnabled(false);
        // 主状态机流程
        isTestContinue = true;
        teststate = -1;

        emit send_go_next_focus();
        ui->getMac->setDisabled(1);
        ui->macInput->setDisabled(1);
    }
}

void QFreeWork::on_pushButton_2_clicked() {
    at->sendBLELOG(1);  // 日志开
}

void QFreeWork::on_getMac_returnPressed() {
    testResultTableInit();

    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->getMac->text()).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->getMac->text().length()));
        showlog("要求格式为" + snPattern);
        ui->getMac->clear();
        return;
    }
    sn = ui->getMac->text().toUtf8();
    showlog("正在查询mac地址");
    getMac(ui->getMac->text());             // 文件获取
    processInspection(ui->getMac->text());  // 站前检测
    processGetMesTestValue();               // mes获取
}

void QFreeWork::processInspection(QString stringsn) {
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

void QFreeWork::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}
void QFreeWork::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");              // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) {  // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                      // 逐行读取文件
            QString line = in.readLine();          // 读取一行
            QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {
                QString sn = fields.at(0);   // 第一个字段是sn
                QString mac = fields.at(1);  // 第二个字段是mac
                if (sn == sn_to_search) {    // 检查是否是待检索的sn
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

        file.close();  // 关闭文件
    }
    if (!ui->isformmes->isChecked() && ui->macInput->text().isEmpty()) {
        ui->getMac->clear();
        showlog("找不到mac地址，清空当前输入的sn");
    }
}
void QFreeWork::getmacadress(const QByteArray& byte) {
    receivedData = "";
    receivedData = receivedData + QString::fromUtf8(byte);

    if (receivedData.length() >= 2 && receivedData.right(2) == "\r\n") {
        // 使用正则表达式提取设备信息
        static const QRegularExpression regex("deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
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
void QFreeWork::on_clear_scan_clicked() {
    deviceMap.clear();
    ui->mac_combo->clear();
}
void QFreeWork::updateComboBox() {
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
void QFreeWork::on_mac_combo_textActivated(const QString& arg1) {
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
        // at->sendMac(macAddress);//发送mac地址
        qDebug() << getIndex() << macAddress;
        bandingMacSn(macAddress, snbanding);
    }
    ui->snbanding->setFocus();
}
void QFreeWork::bandingMacSn(QString bandingmac, QString bandingsn) {
    if (bandingsn == "" || bandingmac == "")
        bandingresult = false;

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

    // bandingMacSn_mes(bandingmac, bandingsn);
}

void QFreeWork::bandingMacSn_mes(QString bandingmac, QString bandingsn) {
    Q_UNUSED(bandingsn);
    pack.mechines = 1;  // 1脱1,1号上位机
    pack.sn = snbanding;
    pack.result = "PASS";
    pack.itemvalue = QString("|BTMAC:%1|").arg(bandingmac);
    pack.instruct_num = "076";
    if (ui->isusemes->checkState()) {
        emit send_end_testPass(pack);
    }

    if (bandingresult) {
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
void QFreeWork::on_snbanding_returnPressed() {
    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    ui->snbanding->clear();
    bandingresult = true;
}

void QFreeWork::getTestValue(const int mechines, const QString value) {
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
    } else {
        if (mechines == getIndex()) {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }
}
void QFreeWork::on_clear_nfc_data_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8] = {0x03, 0x00, 0xFE, 0x00};  // 写入数据缓冲区
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    icdev = dc_init(100, 115200);
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

    int ret = dc_write(icdev, 4, writedata);  // 将写入数据缓冲区中的数据写入设备

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
QString QFreeWork::generateDateCode() {
    // 获取当前日期时间
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // 获取当前年份、月份和日期
    int year = currentDateTime.date().year() % 100;  // 获取后两位年份
    int month = currentDateTime.date().month();
    int day = currentDateTime.date().day();

    // 月份编码
    char monthCode;
    if (month >= 1 && month <= 9) {
        monthCode = '0' + month;
    } else {
        monthCode = 'A' + (month - 10);
    }

    // 日期编码
    char dayCode;
    if (day >= 1 && day <= 9) {
        dayCode = '0' + day;
    } else {
        dayCode = 'A' + (day - 10);
    }

    // 构造日期编码
    QString dateCode = QString::number(year) + monthCode + dayCode;
    return dateCode;
}

void QFreeWork::on_nfc_write_read_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8];  // 写入数据缓冲区
    ReadNfcData = "";
    // 033BD2023668772001004800324F30450081080027200014178591141035353034303730313233313131313131170102910B0101010A063C842707A8D1
    // 3C842707A8D1
    // 35353034303730313233313131313131
    // 5504070123111111    //55040701华为固定开头sku2311年月日1111数量
    QString hexString;

    static int productionNumber = ui->nfc_count->text().toInt();  // 记录生产数量

    QString text = ui->nfc_sn->text();  // 外部给
    ui->nfc_count->setText(QString::number(productionNumber));

    for (int i = 0; i < text.length(); ++i) {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0'));
    }

    QString dataText = "033BD2023668772001004800324F304500810800272000141785911410" + hexString +
                       "170102910B0101010A06" + macAddress.remove(":").toUpper();

    QByteArray dataBytes = QByteArray::fromHex(dataText.toLatin1());  // 将十六进制字符串转换为字节数组
    int dataSize = dataBytes.size();                                  // 获取字节数组的大小
    qDebug() << getIndex() << "dataSize: " << dataSize << dataText;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    icdev = dc_init(100, 115200);
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

    for (int i = 0; i < dataSize; i += 4) {        // 每次处理8个字节
        int bytesToWrite = qMin(8, dataSize - i);  // 计算本次需要写入的字节数，最多8个

        memcpy(writedata, dataBytes.constData() + i, bytesToWrite);  // 将数据复制到写入数据缓冲区

        int ret = dc_write(icdev, 4 + i / 4, writedata);  // 将写入数据缓冲区中的数据写入设备

        if (ret != 0) {
            QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
            showlog(errMsg);

            qDebug() << getIndex() << "errMsg: " << writedata << errMsg;
        }
    }
    showlog("nfc信息读取内容为：");
    for (int i = 4; i * 4 < dataSize; i += 4) {  // 每次处理16个字节
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
            showlog(QString::fromStdString(str1));
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
            showlog(QString::fromStdString(str1));
        }
    }
    showlog("nfc信息读取结束");
}
void QFreeWork::on_nfc_sn_returnPressed() { ui->getMac->setFocus(); }

void QFreeWork::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void QFreeWork::on_disconnectButton_clicked() {
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}

void QFreeWork::on_stopTest_clicked() {
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

void QFreeWork::on_save_config_clicked() {
    showlog("测试顺序配置已迁移到设置页面，请在 qsetting 的测试配置页调整并保存。");
}




