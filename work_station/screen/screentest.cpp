
#include "screentest.h"

#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_screentest.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif
void screentest::on_pushButton_clicked() {
    ui->macInput->setText("f4:12:fa:c5:51:c6");
    //    ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    ui->macInput->setText("3C:84:27:07:A8:D2");
    on_macInput_returnPressed();
}
screentest::screentest(int index, QWidget* parent) : ui(new Ui::screentest) {
    m_index = index;

    ui->setupUi(this);

    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();  // 要搜索一下一开始

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    showlog("action=" + pack.test_station);
    showlog("model=" + pack.model);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);

    showlog("machineNo=" + pack.machineNo);
}

void screentest::refreshLcdControl(FacLcdControl style) {
    qDebug() << getIndex() << style.result;
    is_lcd_control = 1;
}

void screentest::getDongleVer(QString data) { showlog("当前dongle的版本为：" + data); }
screentest::~screentest() { delete ui; }

void screentest::on_disconnectButton_clicked() {
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}
void screentest::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        pb->set_forbid_sleep(FacSwitch_OPEN);
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void screentest::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void screentest::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}
void screentest::on_macInput_returnPressed() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        ui->macLabel->setText("蓝牙mac: " + macAddress);
        qDebug() << getIndex() << macAddress;
        isScreenContinue = true;
        emit send_go_next_focus();
        state = STATE_IDLE;
    }
}

void screentest::solveMesSucess(const int mechines) {
    if (mechines == getIndex()) {
        showlog("mes操作成功");
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #00FF00; color: black; border: 2px solid "
                                     "#00FF00; border-radius: 10px; padding: 10px; text-align: center;");

        mes_set_ok = 1;
    }
}
void screentest::solveMesData(const int mechines, QString msg) {
    if (mechines == getIndex()) {
        showlog("MES:报错信息:" + msg);
        ui->macInput->setDisabled(0);
        ui->getMac->setDisabled(0);
        isScreenContinue = false;
        showlog("停止运行");

        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
        emit send_end_test(getIndex());

        ui->getMac->clear();
        ui->getMac->setFocus();
    }
}

void screentest::closeEvent(QCloseEvent*) {
    qDebug() << getIndex() << "开始关闭";

    isScreenContinue = false;
}
void screentest::refreshSn(FacDevInfo data) {
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("存储尾盖sn:" + stringsn);
}

void screentest::set_screen_color(int x) {
    if (at->getConnected()) {
        pb->set_screen_color(x);
        showlog("已设置屏幕颜色" + QString::number(x));

    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void screentest::canGoNextMechine(int x) {
    is_canGoNext = 1;

    qDebug() << getIndex() << "得到信息" << getIndex();

    if (x == getIndex()) {
        result = failValue;
        showlog("停止运行");
        ui->test_result->setText("FAIL");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                       "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

        state = STATE_SAVE_RESULT;
        showlog("屏幕测试失败");
    }
}

void screentest::processInspection(QString stringsn) {
    if (stringsn != "" || !ui->isusemes->checkState()) {
        if (ui->isformmes->checkState()) {
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

void screentest::startTask()  // 编写六轴校准的代码
{
    if (isScreenContinue) {
        ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state) {
            case STATE_IDLE:  // 复位一切
                showlog("开始测试");
                pb->reset_all_pb();
                at->resetConnected();
                refreshBleState(0);
                ui->tail_sn->setText("存储尾盖sn:");
                stringsn = "";
                result = passValue;
                is_canGoNext = 0;
                is_lcd_control = 0;
                TestTime.start();
                at->sendMac(ui->macInput->text());  // 发送mac地址

                state = STATE_WATI_CONNECT;
                break;
            case STATE_WATI_CONNECT:
                if (at->getConnected()) {
                    state = STATE_DISABLE_SLEEP_1;
                }
                break;

            case STATE_DISABLE_SLEEP_1:
                if (pb->getDisableSleep()) {
                    showlog("已进入禁止休眠模式");
                    set_screen_color(0);
                    showlog("--------------当前是红色");
                    emit send_go_next_test(getIndex());
                    state = STATE_COLOR1;
                }
                break;

            case STATE_COLOR1:
                if (is_lcd_control && is_canGoNext) {
                    is_canGoNext = 0;
                    is_lcd_control = 0;
                    set_screen_color(1);
                    showlog("--------------当前是绿色");
                    emit send_go_next_test(getIndex());

                    state = STATE_COLOR2;
                }
                break;
            case STATE_COLOR2:
                if (is_lcd_control && is_canGoNext) {
                    is_canGoNext = 0;
                    is_lcd_control = 0;
                    set_screen_color(2);
                    showlog("--------------当前是蓝色");

                    emit send_go_next_test(getIndex());

                    state = STATE_COLOR3;
                }
                break;
            case STATE_COLOR3:
                if (is_lcd_control && is_canGoNext) {
                    is_canGoNext = 0;
                    is_lcd_control = 0;
                    set_screen_color(3);
                    showlog("--------------当前是白色");

                    emit send_go_next_test(getIndex());

                    state = STATE_COLOR4;
                }
                break;
            case STATE_COLOR4:
                if (is_lcd_control && is_canGoNext) {
                    is_canGoNext = 0;
                    is_lcd_control = 0;
                    set_screen_color(4);
                    showlog("--------------当前是黑色");

                    emit send_go_next_test(getIndex());

                    state = STATE_COLOR5;
                }
                break;
            case STATE_COLOR5:
                if (is_lcd_control && is_canGoNext) {
                    is_canGoNext = 0;
                    is_lcd_control = 0;

                    state = STATE_SAVE_RESULT;
                }
                break;

            case STATE_SAVE_RESULT:

                if (result == passValue) {
                    QString mesresult = "PASS";

                    pack.result = mesresult;

                    pack.itemvalue = QString("|SCREEN_TEST:PASS|");

                    pack.sn = ui->getMac->text();

                    if (ui->isusemes->checkState()) {
                        send_end_testPass(pack);
                    }

                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((result == failValue)) {
                    QString mesresult = "NG";
                    pack.result = mesresult;

                    pack.itemvalue = QString("|SCREEN_TEST:NG|");

                    pack.sn = ui->getMac->text();

                    if (ui->isusemes->checkState()) {
                        send_end_testPass(pack);
                    }
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }
                TestItem test;
                test.testItem = "屏幕测试";
                test.testData = "";
                test.testResult = result;
                testItems.append(test);

                log->saveTestCsv(SCREEN_VER, ui->getMac->text(), ui->macInput->text(), testItems);

                stringsn = "";
                ui->getMac->clear();
                ui->macInput->clear();
                pb->set_dev_reset();
                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                waitWork(50);
                on_disconnectButton_clicked();
                showlog("测试结束");
                isScreenContinue = false;
                emit send_end_test(getIndex());

                state = STATE_IDLE;
                break;
        }

        //  QCoreApplication::processEvents();
    }
}

void screentest::on_lcdTestButton_clicked() {
    pb->set_dev_reset();
    // waitWork(WAITTIME);
    //
    // waitWork(WAITTIME);
    // pb->set_fac_mode(0);
    // waitWork(WAITTIME);
    // pb->set_fac_mode(1);
    // waitWork(WAITTIME);
    // pb->set_fac_mode(0);
    // waitWork(WAITTIME);
    // pb->set_fac_mode(1);
    // waitWork(WAITTIME);
}

void screentest::refreshMesState(int state) {
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}
void screentest::getTestValue(const int mechines, const QString value) {
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
void screentest::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");              // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) {  // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                      // 逐行读取文件
            QString line = in.readLine();          // 读取一行
            QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {             // 至少需要两个字段
                QString sn = fields.at(0);         // 第一个字段是sn
                QString mac = fields.at(1);        // 第二个字段是mac
                if (sn == sn_to_search) {          // 检查是否是待检索的sn
                    showlog("这是从文件获取的mac地址");
                    ui->macInput->setText(mac);
                    on_macInput_returnPressed();
                    qDebug() << getIndex() << "The corresponding mac is: " << mac;
                    break;
                }
            } else {
                showlog("存在没有逗号分开的" + QString::number(fields.count()) + line);
            }
        }
        file.close();  // 关闭文件
    }
}
void screentest::on_getMac_returnPressed() {
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);

    // 使用正则表达式匹配
    if (!snRegex.match(ui->getMac->text()).hasMatch()) {
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("序列号错误");
        ui->getMac->clear();
        return;
    }

    showlog("正在查询mac地址");
    getMac(ui->getMac->text());             // 文件获取
    processInspection(ui->getMac->text());  // 站前检测
    processGetMesTestValue();               // mes获取
}

void screentest::processGetMesTestValue() {
    if (ui->isusemes->checkState()) {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";

        emit getMesTestValue(pack);
    }
}

void screentest::on_stopTest_clicked() {
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}
