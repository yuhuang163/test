#include "ageing.h"

#include "ui_ageing.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

ageing::ageing(int index, QWidget* parent) : ui(new Ui::ageing) {
    m_index = index;
    pack.mechines = getIndex();
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    upperComputerVer = AGE_VER;

    scanSerialPorts();  // 要搜索一下一开始

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    // mes失败停止。

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    settings.setValue("Window/Size", this->size());
    standbattary = settings.value("BATTARY/standbattary").toDouble();

    showlog("standbattary=" + QString::number(standbattary));
    showlog("machineNo=" + pack.machineNo);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);
    showlog("model=" + pack.model);
    testResultTableInit();
}
void ageing::refreshMesState(int state) {
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}

void ageing::refreshPeriphData(FacGetPeriphState data) {
    if (refresh_periph_times) {
        refresh_periph_times = 0;
        QSettings settings(SETTING_NAME, QSettings::IniFormat);
        QString flashStatus = settings.value("PeripheralStatus/Flash_Status").toString();

        if (data.flash_state == flashStatus.toInt() || flashStatus == "null") {
            flash_state = 1;
        } else {
            flash_state = 2;
        }

        TestItem test;
        test.testItem = "内存状态";
        test.testData = QString::number(data.flash_state);
        test.ask = flashStatus;
        testItems.append(test);

        log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
        updateTestData(testItems);
        testItems.clear();
    }
}
void ageing::getTestValue(const int mechines, const QString value) {
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

    // bandingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试mac——sn
}
ageing::~ageing() { delete ui; }
void ageing::refreshBattaryData(FacDevInfo adc) {
    QString chargeStateStr;
    switch (adc.dev_info[0].value_item.battery.charge_state) {
        case 1: chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>"; break;
        case 2: chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>"; break;
        case 3: chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>"; break;
        case 4: chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>"; break;
        default: chargeStateStr = "充电状态为：<span style='color:red'>未知</span>"; break;
    }
    ui->battary_state->setText(chargeStateStr);

    // 修改电量的显示样式
    QString batteryPercentStr =
        "电量为：<span style='color:blue'>" + QString::number(adc.dev_info[0].value_item.battery.percent) + "%</span>";
    ui->battary_value->setText(batteryPercentStr);

    // 修改电压的显示样式
    QString batteryVoltageStr = "电压为：<span style='color:purple'>" +
                                QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0, 'f', 3) +
                                "V</span>";
    ui->battary_voltage->setText(batteryVoltageStr);

    voltage = adc.dev_info[0].value_item.battery.voltage / 1000.0;
    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);

    if (adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary) {
        is_battary_test = 1;  // 正常
    }
    if (adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary)
        is_battary_test = 2;  // 低电量
}
void ageing::refreshBleState(int state) {
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

void ageing::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}
void ageing::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}

void ageing::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void ageing::on_macInput_returnPressed() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    waitWork(WAITTIME);
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
        // 主状态机流程
        isTestContinue = true;
        emit send_go_next_focus();

        state = STATE_IDLE;
    }
}

void ageing::on_enterBurningMode_clicked() {
    qDebug() << getIndex() << "串口问题";

    if (at->getConnected()) {
        qDebug() << getIndex() << "串口问题";
        if (ui->burningModeCombo->currentText() == "老化1")
            pb->set_burning_mode(1, FacSwitch_OPEN);
        if (ui->burningModeCombo->currentText() == "老化2")
            pb->set_burning_mode(2, FacSwitch_OPEN);
        if (ui->burningModeCombo->currentText() == "老化3")
            pb->set_burning_mode(3, FacSwitch_OPEN);
        if (ui->burningModeCombo->currentText() == "老化4")
            pb->set_burning_mode(4, FacSwitch_OPEN);
        showlog("已发送老化");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void ageing::on_exitBurningMode_clicked() {
    if (at->getConnected()) {
        pb->set_burning_mode(1, FacSwitch_CLOSE);
        // showlog("已退出老化模式");
    } else {
        // showlog("请等待连接牙刷后再试");
    }
}

void ageing::getDongleVer(QString data) { showlog("当前dongle的版本为：" + data); }

void ageing::closeEvent(QCloseEvent*) {
    qDebug() << getIndex() << "开始关闭";
    isTestContinue = false;
}
void ageing::refreshSn(FacDevInfo data) {
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);

    stringSubpid = QString::fromUtf8(data.dev_info[0].value_item.sub_pid);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("芯片存储的尾盖sn:" + stringsn);

    if (data.dev_info[0].which_value_item == FacDevInfoValue_sub_pid_tag) {
        showlog("读取的subpid为" + stringSubpid);
        showlog("写入的subpid为" + subpid);

        if (subpid == stringSubpid) {
            TestItem test;
            test.testItem = "suipid测试";
            test.testData = stringSubpid;
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);
            log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
            testResultTableUpdate(testItems);
            testItems.clear();
            subpidCompareOk = 1;

        } else {
            TestItem test;
            test.testItem = "suipid测试";
            test.testData = stringSubpid;
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);
            log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
            testResultTableUpdate(testItems);
            testItems.clear();
            subpidCompareOk = 2;
        }
    }

    if (data.dev_info[0].which_value_item == FacDevInfoValue_tail_sn_tag) {
        showlog("读取的sn为" + stringsn);
        showlog("写入的sn为" + sn);
        if (stringsn == sn) {
            TestItem test;
            test.testItem = "sn测试";
            test.testData = stringsn;
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);
            log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
            testResultTableUpdate(testItems);
            testItems.clear();
            snCompareOk = 1;

        } else {
            TestItem test;
            test.testItem = "sn测试";
            test.testData = stringsn;
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);
            log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
            testResultTableUpdate(testItems);
            testItems.clear();
            snCompareOk = 2;
        }
    }
}

void ageing::processInspection(QString stringsn) {
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

void ageing::startTask() {
    if (isTestContinue) {
        ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state) {
            case STATE_IDLE:  // 复位一切
                is_battary_test = 0;
                testItems.clear();
                showlog("开始测试");
                pb->reset_all_pb();
                at->resetConnected();
                ui->tail_sn->setText("芯片存储的尾盖sn:");
                stringsn = "";
                snCompareOk = 0;
                result = "";
                TestTime.start();
                flash_state = 0;
                refresh_periph_times = 1;
                subpidCompareOk = 0;
                at->sendMac(ui->macInput->text());  // 发送mac地址
                state = STATE_WATI_CONNECT;
                break;
            case STATE_WATI_CONNECT:
                if (at->getConnected()) {
                    sendCommandWithRetry(std::bind(&Qpb::set_sn, pb, FacDevInfoType_TAIL_SN, sn));
                    state = STATE_WAIT_BANDING;
                }
                break;

            case STATE_WAIT_BANDING:

                if (canGoNext) {
                    stringsn = ui->getMac->text();
                    sendCommandWithRetry(std::bind(&Qpb::get_sn, pb, FacDevInfoType_TAIL_SN));
                    showlog("sn已成功绑定保存" + stringsn);
                    state = STATE_WAIT_CORRECT_BANDING;
                }

                break;

            case STATE_WAIT_CORRECT_BANDING:

                if (canGoNext) {
                    if (snCompareOk == 1) {
                        if (pack.product == "U7" || pack.product == "U7P") {
                            showlog("已发送subpid");
                            sendCommandWithRetry(std::bind(&Qpb::set_sn, pb, FacDevInfoType_SUB_PID, subpid));
                            state = STATE_WAIT_BANDING_SUBPID;
                        } else {
                            state = STATE_DISABLE_SLEEP_1;
                        }

                        showlog("sn已比对成功");
                    } else if (snCompareOk == 2) {
                        showlog("sn已比对失败");
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }

                break;
            case STATE_WAIT_BANDING_SUBPID:  // 设置设备采集
                if (canGoNext) {
                    showlog("已绑定成功SUBPID");
                    sendCommandWithRetry(std::bind(&Qpb::get_sn, pb, FacDevInfoType_SUB_PID));

                    state = STATE_WAIT_CORRECT_BANDING_SUBPID;
                }
                break;
            case STATE_WAIT_CORRECT_BANDING_SUBPID:  // 设置设备采集

                if (canGoNext) {
                    if (subpidCompareOk == 1) {
                        state = STATE_DISABLE_SLEEP_1;
                        showlog("subpid已比对成功");
                    } else if (subpidCompareOk == 2) {
                        showlog("subpid已比对失败");
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }
                break;
            case STATE_DISABLE_SLEEP_1:
                if (pb->getDisableSleep()) {
                    showlog("已进入禁止休眠模式");
                    pb->get_battery();
                    state = STATE_GETBATTERY;
                } else {
                    waitWork(500);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    showlog("已重发禁止休眠");
                }
                break;
            case STATE_GETBATTERY:
                if (is_battary_test != 0) {
                    if (is_battary_test == 1) {
                        showlog("电量正常" + QString::number(voltage));
                        TestItem test;
                        test.testItem = "当前电压";
                        test.testData = QString::number(voltage);
                        test.testResult = "通过";

                        test.ask = "通过";
                        testItems.append(test);
                        log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
                        testResultTableUpdate(testItems);
                        testItems.clear();
                        pb->get_periph_state();
                        state = STATE_CHECK_FLASH;
                    }

                    if (is_battary_test == 2) {
                        showlog("当前电压低为" + QString::number(voltage));
                        TestItem test;
                        test.testItem = "当前电压";
                        test.testData = QString::number(voltage);
                        test.testResult = "失败";
                        test.ask = "通过";
                        testItems.append(test);
                        log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
                        testResultTableUpdate(testItems);
                        testItems.clear();
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    waitWork(500);
                    showlog("正在重发获取电量信息");
                    pb->get_battery();
                }
                break;

            case STATE_CHECK_FLASH:
                if (flash_state == 1) {
                    showlog("已发送进入老化");
                    sendCommandWithRetry(std::bind(&Qpb::set_burning_mode, pb, 1, FacSwitch_OPEN));

                    ui->flash_state->setText("Flash State:<font color='green'>正常</font>");
                    showlog("Flash资源正常");
                    state = STATE_AGE;

                } else if (flash_state == 2) {
                    ui->flash_state->setText("Flash State:<font color='red'>异常</font>");
                    showlog("Flash资源异常");
                    result = failValue;
                    state = STATE_SAVE_RESULT;
                } else {
                    waitWork(500);
                    pb->get_periph_state();
                    showlog("正在重新获取Flash资源状态");
                }
                break;

            case STATE_AGE:
                if (canGoNext) {
                    result = passValue;
                    state = STATE_SAVE_RESULT;
                }
                break;

            case STATE_SAVE_RESULT:

                if (result == passValue) {
                    QString mesresult = "PASS";
                    pack.result = mesresult;
                    pack.itemvalue = QString("|AGE_TEST:PASS|");
                    pack.mechines = getIndex();
                    pack.sn = ui->getMac->text();

                    //  pack.itemvalue = "083";
                    if (ui->isusemes->checkState()) {
                        send_end_testPass(pack);
                    }

                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((result == failValue)) {
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }

                ui->getMac->clear();
                ui->macInput->clear();
                stringsn = "";
                TestItem test;
                test.testItem = "老化测试";
                test.testData = "已进入";
                test.testResult = result;
                test.ask = "通过";
                testItems.append(test);
                log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
                testResultTableUpdate(testItems);
                testItems.clear();
                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                log->saveTestCsv(AGE_VER, ui->getMac->text(), ui->macInput->text(), testItems);
                emit send_end_test(getIndex());
                showlog("测试结束");
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                waitWork(150);
                on_disconnectButton_clicked();
                isTestContinue = false;
                break;
        }
        QCoreApplication::processEvents();
    }
}

void ageing::on_pushButton_clicked()

{
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // //    ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    ui->macInput->setText("3C:84:27:07:A8:D2");
    on_macInput_returnPressed();
    //     static int clickStep = 1;   // 用于跟踪当前运行的步骤
    //     switch (clickStep)
    //     {
    //     case 1:
    //         if (ui->isusemes->checkState())
    //         {
    //             pack.sn = "01023120007402L";
    //
    //              pack.mechines = getIndex();
    //             pack.sn = stringsn;
    //
    //
    //
    // if (pack.factory == "hq" )
    //             emit processInspection(pack);

    // else if (pack.factory == "lx" )
    //             emit processInspection(pack);
    // #else
    //             emit processInspection(pack);
    // #endif
    //             showlog("正在进行站前检测");
    //         }

    //         break;

    //     case 2:
    //         if (ui->isusemes->checkState())
    //         {
    //             QString mesresult = "FAIL";
    //             QString itemvalue = QString("|AGE_TEST:PASS|");
    //             pack.result = mesresult;
    //
    //             pack.itemvalue = itemvalue;
    //
    // if (pack.factory == "hq" )
    //             emit send_TestPass(pack);
    // else if (pack.factory == "lx" )
    //
    //
    //             pack.itemvalue = "AGE_TEST=FAIL";
    //             emit send_TestPass(pack);
    // #else
    //             emit send_TestPass(pack);
    // #endif
    //         }

    //         break;
    //     }

    //     clickStep++;   // 增加步骤计数

    //     // 如果步骤计数超过了最大步骤数，重置为第一步
    //     if (clickStep > 2)
    //     {
    //         clickStep = 1;
    //     }
}
QString ageing::getValueBySN(const QString& sn) {
    QString truncatedSN = sn.left(8);
    showlog("truncatedSN:" + truncatedSN);

    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString value = settings.value("SUBPID/" + truncatedSN, "SUBPID_ERRO").toString();
    showlog("匹配到的subpid：" + value);

    return value;
}
void ageing::on_getMac_returnPressed() {
    testResultTableInit();

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
    sn = ui->getMac->text().toUtf8();

    if (pack.product == "U7" || pack.product == "U7P") {
        subpid = getValueBySN(ui->getMac->text()).toUtf8();
        if ("SUBPID_ERRO" == subpid) {
            QMessageBox::warning(nullptr, "Warning", "没匹配到subpid");
            return;
        }
        show_product(subpid);
    }

    getMac(ui->getMac->text());  // 文件获取
    processInspection(ui->getMac->text());
    processGetMesTestValue();  // mes获取
}
#include <QGraphicsPixmapItem>

void ageing::show_product(QString name) {
    QString imagePath = "产品照片/" + name + ".png";
    showlog("显示照片 " + imagePath);

    // 检查文件是否存在
    QFile file(imagePath);
    if (!file.exists()) {
        showlog("图片文件不存在: " + imagePath);
        return;
    }

    // 加载图片
    QPixmap pixmap(imagePath);
    if (pixmap.isNull()) {
        showlog("加载图片失败: " + imagePath);
        return;
    }

    // 获取 QGraphicsView 的尺寸
    QSize viewSize = ui->graphicsView->size();
    qDebug() << getIndex() << "viewSize" << viewSize;

    // 创建 QGraphicsScene 并设置到 QGraphicsView
    QGraphicsScene* scene = new QGraphicsScene(ui->graphicsView);
    ui->graphicsView->setScene(scene);

    // 按 QGraphicsView 的尺寸缩放图片，同时保持纵横比
    QPixmap scaledPixmap = pixmap.scaled(viewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // 创建 QGraphicsPixmapItem 并将缩放后的 QPixmap 设置到它
    QGraphicsPixmapItem* pixmapItem = new QGraphicsPixmapItem(scaledPixmap);

    // 将 QGraphicsPixmapItem 添加到场景中
    scene->addItem(pixmapItem);

    // 调整 QGraphicsView 的视图设置
    ui->graphicsView->setSceneRect(scaledPixmap.rect());
    ui->graphicsView->fitInView(scaledPixmap.rect(), Qt::KeepAspectRatio);  // 确保视图中的图像按比例适应
}
void ageing::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}

void ageing::getMac(QString sn_to_search) {
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
// E2:63:07:34:B9:F8
// E3:1B:07:34:C6:F2

void ageing::on_snInput_returnPressed() {
    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->snInput->text()).hasMatch()) {
        showlog("序列号错误");
        ui->snInput->clear();
        return;
    }

    // 编写SN绑定的代码
    // 先判断蓝牙是否连接上，如果没连接上，不能绑定
    stringsn = ui->snInput->text();
    //  sn = ui->snInput->text().toUtf8();

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // ui->macInput->setFocus();
}

void ageing::on_stopTest_clicked() {
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}
