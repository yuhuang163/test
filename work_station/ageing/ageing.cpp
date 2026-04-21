#include "ageing.h"

#include <QGraphicsPixmapItem>

#include "ui_ageing.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

ageing::ageing(int index, QWidget* parent) : test_base(parent), ui(new Ui::ageing) {
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

    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();
    showlog("standbattary=" + QString::number(standbattary));
    showlog("machineNo=" + pack.machineNo);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);
    showlog("model=" + pack.model);
    testResultTableInit();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}

void ageing::refreshPeriphData(FacGetPeriphState data) {
    if (refresh_periph_times) {
        refresh_periph_times = 0;

        QString flashStatus = SETTINGS.value("PeripheralStatus/Flash_Status").toString();
        QString flashStateStr = QString::number(data.flash_state);

        // 现在可以将 QString 类型的状态用于你的条件判断
        bool checkFlash = SETTINGS.value("PeripheralStatus/FlashStatus_checkBox").toBool();

        if ((!checkFlash || compareVersions(flashStatus, flashStateStr))) {
            flash_state = 1;
        } else {
            flash_state = 2;
        }

        TestItem test;

        if (checkFlash) {
            test.testItem = "内存状态";
            test.testData = QString::number(data.flash_state);
            test.ask = flashStatus;
            testItems.append(test);
        }

        updateTestData(testItems);
    }
}
void ageing::getTestValue(const int mechines, const QString value) {
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

    // bandingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试
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
        protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
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
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
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

void ageing:: on_enterBurningMode_clicked() {
    if (at->getConnected()) {
        const QString currentText = ui->burningModeCombo->currentText();
        int mode = 0;
        if (currentText == "老化1") {
            mode = 1;
        } else if (currentText == "老化2") {
            mode = 2;
        } else if (currentText == "老化3") {
            mode = 3;
        } else if (currentText == "老化4") {
            mode = 4;
        }

        if (mode == 0) {
            showlog("未知老化模式，请先选择老化1-老化4");
            return;
        }

        QVariantMap m;
        m["mode"] = mode;
        m["seconds"] = ui->burningModetime->text();
        protocolManager.set(DeviceCmd::BurningMode, m);
        showlog("已发送老化");
    } else {
        showlog("请等待连接设备后再试");
    }
}

void ageing::on_exitBurningMode_clicked() {
    if (at->getConnected()) {
        QVariantMap m;
        m["mode"] = 1;
        m["switch"] = static_cast<int>(FacSwitch_CLOSE);
        protocolManager.set(DeviceCmd::BurningMode, m);
        // showlog("已退出老化模式");
    } else {
        // showlog("请等待连接设备后再试");
    }
}

void ageing::refreshSn(FacDevInfo data) {
    QString brushstringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    QString brushstringSubpid = QString::fromUtf8(data.dev_info[0].value_item.sub_pid);
    QString brushstringSkuid = QString::fromUtf8(data.dev_info[0].value_item.sku_id);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "brushstringsn" << brushstringsn;

    if (data.dev_info[0].which_value_item == FacDevInfoValue_sub_pid_tag) {
        ui->brush_subpid->setText("存储的subpid:" + brushstringSubpid);

        showlog("读取的subpid为" + brushstringSubpid);
        showlog("写入的subpid为" + stringsubpid);

        if (brushstringSubpid == stringsubpid) {
            TestItem test;
            test.testItem = "读取的suipid";
            test.testData = brushstringSubpid;
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);
            testResultTableUpdate(testItems);

            subpidCompareOk = 1;

        } else {
            TestItem test;
            test.testItem = "读取的suipid";
            test.testData = brushstringSubpid;
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);
            testResultTableUpdate(testItems);

            subpidCompareOk = 2;
        }
    }

    if (data.dev_info[0].which_value_item == FacDevInfoValue_tail_sn_tag) {
        ui->tail_sn->setText("存储的尾盖sn:" + brushstringsn);

        showlog("读取的sn为" + brushstringsn);
        showlog("写入的sn为" + stringsn);
        if (brushstringsn == stringsn) {
            TestItem test;
            test.testItem = "读取的sn";
            test.testData = brushstringsn;
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);
            testResultTableUpdate(testItems);

            snCompareOk = 1;

        } else {
            TestItem test;
            test.testItem = "读取的sn";
            test.testData = brushstringsn;
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);
            testResultTableUpdate(testItems);

            snCompareOk = 2;
        }
    }

    if (data.dev_info[0].which_value_item == FacDevInfoValue_sku_id_tag) {
        ui->sku_id->setText("存储的skuid:" + brushstringSkuid);

        showlog("读取的Skuid为" + brushstringSkuid);
        showlog("写入的Skuid为" + stringSkuid);
        if (brushstringSkuid == stringSkuid) {
            TestItem test;
            test.testItem = "读取的skuid";
            test.testData = brushstringSkuid;
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);
            testResultTableUpdate(testItems);
            skuCompareOk = 1;

        } else {
            TestItem test;
            test.testItem = "读取的skuid";
            test.testData = brushstringSkuid;
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);
            testResultTableUpdate(testItems);

            skuCompareOk = 2;
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
        showlog("SN为空");
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
        // ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);  // 转换为浮动数

        switch (state) {
            case STATE_IDLE:  // 复位一切
                is_battary_test = 0;

                showlog("开始测试");
                pb->reset_all_pb();
                at->resetConnected();
                ui->tail_sn->setText("存储的尾盖sn:");
                ui->brush_subpid->setText("存储的subpid:");
                snCompareOk = 0;
                result = "";
                TestTime.start();
                flash_state = 0;
                refresh_periph_times = 1;
                subpidCompareOk = 0;
                waitWork(1000);
                at->sendMac(ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());
                state = STATE_WATI_CONNECT;
                break;
            case STATE_WATI_CONNECT:
                if (at->getConnected()) {
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, writesn})); });
                    showlog("sn绑定保存内容为：" + stringsn);
                    state = STATE_WAIT_BANDING;
                }
                break;

            case STATE_WAIT_BANDING:
                if (canGoNext) {
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetSn, static_cast<int>(FacDevInfoType_TAIL_SN)); });
                    state = STATE_WAIT_CORRECT_BANDING;
                }

                break;

            case STATE_WAIT_CORRECT_BANDING:

                if (canGoNext) {
                    if (snCompareOk == 1) {
                        if (SETTINGS.value("SYSTEM/NeedWriteSubpid").toBool()) {
                            showlog("已发送subpid");
                            sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SUB_PID, writesubpid})); });
                            state = STATE_WAIT_BANDING_SUBPID;
                        } else {
                            // if (SETTINGS.value("SYSTEM/NeedWriteSkuid").toBool()) {
                            //     showlog("已发送Skuid");
                            //     sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SKUID, writeskuid})); });
                            //     state = STATE_WAIT_BANDING_SKUID;
                            // } else {
                            state = STATE_DISABLE_SLEEP_1;
                            // }
                        }

                        showlog("sn已比对成功");
                    } else if (snCompareOk == 2) {
                        showlog("sn已比对失败"); pack.error="SP03011";

                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }

                break;
            case STATE_WAIT_BANDING_SUBPID:  // 设置设备采集
                if (canGoNext) {
                    showlog("已绑定成功SUBPID");
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetSn, static_cast<int>(FacDevInfoType_SUB_PID)); });

                    state = STATE_WAIT_CORRECT_BANDING_SUBPID;
                }
                break;
            case STATE_WAIT_CORRECT_BANDING_SUBPID:  // 设置设备采集

                if (canGoNext) {
                    if (subpidCompareOk == 1) {
                        // if (SETTINGS.value("SYSTEM/NeedWriteSkuid").toBool()) {
                        //     showlog("已发送Skuid");
                        //     sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SKUID, writeskuid})); });
                        //     state = STATE_WAIT_BANDING_SKUID;
                        // } else {
                        state = STATE_DISABLE_SLEEP_1;
                        // }

                        showlog("subpid已比对成功");
                    } else if (subpidCompareOk == 2) {
                        showlog("subpid已比对失败");
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }
                break;

            case STATE_WAIT_BANDING_SKUID:  // 设置设备采集
                if (canGoNext) {
                    showlog("已绑定成功SKUID");
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetSn, static_cast<int>(FacDevInfoType_SKUID)); });

                    state = STATE_WAIT_CORRECT_BANDING_SKUID;
                }
                break;
            case STATE_WAIT_CORRECT_BANDING_SKUID:  // 设置设备采集

                if (canGoNext) {
                    if (skuCompareOk == 1) {
                        state = STATE_DISABLE_SLEEP_1;
                        showlog("subpid已比对成功");
                    } else if (skuCompareOk == 2) {
                        showlog("subpid已比对失败");
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }
                break;

            case STATE_DISABLE_SLEEP_1:
                if (pb->getState(Qpb::PbStateType::DisableSleep)) {
                    showlog("已进入禁止休眠模式");
 if (SETTINGS.value("Mes/Product_Name").toString() == "P20P")
                 {   protocolManager.get(DeviceCmd::Battery);
                    state = STATE_GETBATTERY;}
 else
                    state = STATE_CHECK_FLASH;

                } else {
                    waitWork(500);
                    protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
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
                        testResultTableUpdate(testItems);
                        protocolManager.get(DeviceCmd::PeriphState);
                        state = STATE_CHECK_FLASH;
                    }

                    if (is_battary_test == 2) {
                        showlog("当前电压低为" + QString::number(voltage));
                        pack.error="SP03010";
                        TestItem test;
                        test.testItem = "当前电压";
                        test.testData = QString::number(voltage);
                        test.testResult = "失败";
                        test.ask = "通过";
                        testItems.append(test);
                        testResultTableUpdate(testItems);

                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    waitWork(500);
                    showlog("正在重发获取电量信息");
                    protocolManager.get(DeviceCmd::Battery);
                }
                break;

            case STATE_CHECK_FLASH:
                if (flash_state == 1) {
                    showlog("已发送进入老化");
                    sendCommandWithRetry([&]() {
                        QVariantMap m;
                        m["mode"] = 1;
                        m["switch"] = static_cast<int>(FacSwitch_OPEN);
                        protocolManager.set(DeviceCmd::BurningMode, m);
                    });

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
                    protocolManager.get(DeviceCmd::PeriphState);
                    showlog("正在重新获取Flash资源状态");
                }
                break;

            case STATE_AGE:
                if (canGoNext) {
                    result = passValue;
                    state = STATE_SAVE_RESULT;
                }
                break;

            case STATE_SAVE_RESULT: {
                if (result == passValue) {
                    QString mesresult = "PASS";
                    pack.result = mesresult;
                    pack.itemvalue = QString("|AGE_TEST:PASS|");
                    pack.mechines = getIndex();
                    pack.sn = ui->getMac->text();

                    //  pack.itemvalue = "083";
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
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

                TestItem test;
                test.testItem = "老化测试";
                test.testData = "已进入";
                test.testResult = "通过";
                test.ask = "通过";
                testItems.append(test);
                testResultTableUpdate(testItems);

                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                emit send_end_test(getIndex());
                showlog("测试结束");
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                waitWork(150);
                on_disconnectButton_clicked();
                isTestContinue = false;
            } break;
            case STATE_PROCESS_INSPECTION: break;
        }
        QCoreApplication::processEvents();
    }
}

void ageing::on_pushButton_clicked() {
    ui->macInput->setText("3C:84:27:07:A8:D2");
    on_macInput_returnPressed();
}

void ageing::on_getMac_returnPressed() {
    testResultTableInit();
    writesn.clear();
    writesubpid.clear();
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
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
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->getMac->text().length()));
        showlog("要求格式为" + snPattern);
        ui->getMac->clear();
        ui->getMac->setFocus();
        return;
    }

    showlog("正在查询mac地址");
    writesn = ui->getMac->text().toUtf8();
    stringsn = ui->getMac->text();
    if (SETTINGS.value("SYSTEM/NeedWriteSubpid").toBool()) {
        if (writesn != last_sn) {
            last_sn = writesn;

        } else {
            showlog("sn与上一台机器重复,机子异常" + last_sn + writesn);
            return;
        }

        writesubpid = getValueBySN(ui->getMac->text()).toUtf8();

        if ("SUBPID_ERRO" == writesubpid) {
            QMessageBox::warning(nullptr, "Warning", "没匹配到subpid");
            return;
        }
        stringsubpid = writesubpid;
        show_product(writesubpid);
    }

    getMac(ui->getMac->text());  // 文件获取
    processInspection(ui->getMac->text());
    processGetMesTestValue();  // mes获取
}

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

void ageing::on_snInput_returnPressed() {
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

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
}

void ageing::on_stopTest_clicked() {
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}



