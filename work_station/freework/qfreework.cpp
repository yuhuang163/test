#include "qfreework.h"

#include <algorithm>
#include <QSet>
#include "ui_qfreework.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {
QString orderGroupName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    return key.isEmpty() ? "TestOrder_default" : QString("TestOrder_%1").arg(key);
}
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
    } else {
        usbBaudRate = 115200;
        ui->usbdisconnectButton->setDisabled(true);
        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }

    createTestFunctions();
    refreshOrderedTestIndexes();
    testResultTableInit();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}
void QFreeWork::refreshOrderedTestIndexes() {
    const QString stationName = SETTINGS.value("TestOrderMeta/SelectedStationName").toString().trimmed();
    const QString tabName = stationName.isEmpty() ? "自由工站" : stationName;
    ui->tabWidget->setTabText(0, tabName);
    qDebug() << "[FreeWork] refresh tab, SelectedStationName =" << stationName << ", tabName =" << tabName;

    orderedTestIndexes_.clear();
    QSet<int> validIds;
    for (const auto& testFunction : testFunctions) {
        validIds.insert(testFunction.id);
    }
    const QVector<int> indexes = loadIndexesFromConfig();
    for (int index : indexes) {
        if (validIds.contains(index) && !orderedTestIndexes_.contains(index)) {
            orderedTestIndexes_.append(index);
        }
    }
    if (orderedTestIndexes_.isEmpty()) {
        for (const auto& testFunction : testFunctions) {
            orderedTestIndexes_.append(testFunction.id);
        }
    }
}

QVector<int> QFreeWork::loadIndexesFromConfig() {
    QVector<int> indexes;
    const QString selectedStation = SETTINGS.value("TestOrderMeta/SelectedStation").toString().trimmed();
    qDebug() << "[FreeWork] loadIndexesFromConfig, SelectedStationName =" << selectedStation;

    auto loadFromGroup = [&indexes](const QString& groupPath) {
        SETTINGS.beginGroup(groupPath);
        QStringList keys = SETTINGS.childKeys();
        std::sort(keys.begin(), keys.end(), [](const QString& left, const QString& right) { return left.toInt() < right.toInt(); });
        for (const QString& key : keys) {
            indexes.append(SETTINGS.value(key).toInt());
        }
        SETTINGS.endGroup();
    };

    if (!selectedStation.isEmpty()) {
        loadFromGroup(orderGroupName(selectedStation));
    }
    if (indexes.isEmpty() && !selectedStation.isEmpty()) {
        loadFromGroup(QString("TestOrder/%1").arg(selectedStation));
    }
    if (indexes.isEmpty()) {
        loadFromGroup("TestOrder");
    }
    if (indexes.isEmpty()) {
        loadFromGroup(orderGroupName("default"));
    }
    if (indexes.isEmpty()) {
        loadFromGroup("TestOrder/default");
    }

    qDebug() << "[FreeWork] loaded indexes, station =" << selectedStation << ", indexes =" << indexes;
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
            // 每次开始测试都重新读取配置，避免设置页调整后本页仍使用旧队列。
            refreshOrderedTestIndexes();
            waitWork(1000);
            at->sendDcon(macAddress);  // 开始连接
            showlog("MAC地址为：" + ui->macInput->text());
            teststate++;
        }
        if (at->getConnected()) {
            for (; teststate < orderedTestIndexes_.count();) {
                const int functionId = orderedTestIndexes_.at(teststate);
                auto it = std::find_if(testFunctions.begin(), testFunctions.end(),
                                       [functionId](const NamedFunction& item) { return item.id == functionId; });
                if (it == testFunctions.end()) {
                    ++teststate;
                    stepRuntime_.reset();
                    break;
                }
                const NamedFunction& currentFunction = *it;
                const QString functionName = currentFunction.name;

                // 阶段1：首次进入当前步骤，仅触发一次动作（通常是发协议命令）
                if (!stepRuntime_.started) {
                    if (!canGoNext) {
                        break;
                    }
                    stepRuntime_.started = true;
                    stepRuntime_.functionId = functionId;
                    stepRuntime_.done = !currentFunction.needCaseDone;
                    stepRuntime_.pass = true;
                    stepRuntime_.testData = "-";
                    stepRuntime_.ask = "通过";
                    showlog("开始测试内容：" + functionName);
                    executeFunctionByName(functionName);  //执行操作
                    qDebug() << "程序在跑" << teststate << orderedTestIndexes_.count();
                    break;
                }

                // 阶段2：等待通信链路允许继续
                if (!canGoNext) {
                    break;
                }

                // 发送重试超时：将当前步骤判失败并放行，避免卡住不判定。
                if (sendRetryOver) {
                    sendRetryOver = false;
                    stepRuntime_.done = true;
                    stepRuntime_.pass = false;
                    TestResult = failValue;
                    showlog("步骤超时未收到响应，判定失败：" + functionName);
                }

                // 需要业务判定的步骤必须等异步回调将 done 置位后再推进，
                // 防止“有回包即通过”。
                if (currentFunction.needCaseDone && !stepRuntime_.done) {
                    break;
                }

                if (!stepRuntime_.pass) {
                    TestResult = failValue;
                }

                // static const QSet<QString> skipTableFunctions = {"获取外围设备状态", "获取基本信息"};
                // if (!skipTableFunctions.contains(functionName)) {
                    TestItem test;
                    test.testItem = functionName;
                    test.testData = stepRuntime_.testData;
                    test.testResult = stepRuntime_.pass ? "通过" : "失败";
                    test.ask = stepRuntime_.ask;
                    testItems.append(test);
                    testResultTableUpdate(testItems);
                // }

                ++teststate;
                stepRuntime_.reset();

                break;
            }
        }

        if (teststate == orderedTestIndexes_.count() && teststate != 0) {
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
            stepRuntime_.reset();
            ui->macInput->clear();
            ui->snInput->clear();
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

void QFreeWork::getDongleWifi(QString data) {
    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
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
void QFreeWork::initDate() {
    ui->product_sn->setText("芯片存储的整机sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;

    rssitestfailcount = 0;
    wifistate = 0;
    measure_ammeter = 0;
    dongleOutTime = 10;
    canGoNext = 1;
    stepRuntime_.reset();
    isovertime = 0;
    BT_RSSI = "";
    BLE_RSSI = "";
    WIFI_RSSI = "";
    softwareVersionForReport_.clear();
    softwareVersionPassForReport_ = true;
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
    deviceTailSnFromDevice = "";
    TestTime.start();
}

void QFreeWork::on_pushButton_clicked() {
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    // // ui->macInput->setText("3c:84:27:06:f7:5e");
    // ui->macInput->setText("3C:84:27:07:A8:D2");
    // // // ui->macInput->setText("3c:84:27:29:50:32");
    // ui->macInput->setText("b4:56:5d:bf:57:9d");

    // on_macInput_returnPressed();
    // // usb-> getlxMEASure();
    // // waitWork(1000);

    // showlog("正在获取设备电量");
    // ui->comNameCombo->setCurrentText("COM134");

    debugUpdateTupleMacStatus();
    applyTupleByMac();
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
    expectedTailSnFromUi = ui->getMac->text().toUtf8();
    showlog("正在查询mac地址");
    getMac(ui->getMac->text());             // 文件获取
    processInspection(ui->getMac->text());  // 站前检测
    // processGetMesTestValue();               // mes获取

    const QString parsedMac = parseMacFromSn(ui->getMac->text());
    if (parsedMac.isEmpty()) {
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("SN解析MAC失败");
        ui->getMac->setFocus();
        return;
    }

    ui->macInput->setText(parsedMac);
    showlog("SN解析MAC成功: " + parsedMac);

on_macInput_returnPressed();
}

void QFreeWork::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = inputSnText;

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




