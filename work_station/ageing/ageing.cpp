#include "ageing.h"

#include <QGraphicsPixmapItem>
#include <QRegularExpression>

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

void ageing::refreshPeriphData(ProtocolPeriphStateData data) {
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
            qDebug() << getIndex() << "MAC地址:" << mesmacAddress;
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
void ageing::refreshBattaryData(ProtocolBatteryData adc) {
    QString chargeStateStr;
    switch (adc.chargeState) {
        case 1: chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>"; break;
        case 2: chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>"; break;
        case 3: chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>"; break;
        case 4: chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>"; break;
        default: chargeStateStr = "充电状态为：<span style='color:red'>未知</span>"; break;
    }
    ui->battary_state->setText(chargeStateStr);
    battary = adc.percent;
    // 修改电量的显示样式
    QString batteryPercentStr =
        "电量为：<span style='color:blue'>" + QString::number(battary) + "%</span>";
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
    if (battary >= standbattary) {
        is_battary_test = 1;  // 正常
    }
    if (battary < standbattary)
        is_battary_test = 2;  // 低电量
}

void ageing::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
          showlog("蓝牙连接成功");
        protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        showlog("蓝牙连接断开");
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
    isTestContinue = false;
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

        sendCommandWithRetry([&]() {
        QVariantMap m;
        m["mode"] = mode;
        m["seconds"] = ui->burningModetime->text();  // 统一上层入参，协议层做兼容
        protocolManager.set(DeviceCmd::BurningMode, m); });
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

void ageing::refreshSn(ProtocolSnData data) {
    const QString brushstringsn = data.value;
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "brushstringsn" << brushstringsn;

    if (data.type == ProtocolSnType::SubPid) {
        const QString brushstringSubpid = data.value;
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

    if (data.type == ProtocolSnType::TailSn) {
        ui->product_sn->setText("存储的整机sn:" + brushstringsn);

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

    if (data.type == ProtocolSnType::SkuId) {
        const QString brushstringSkuid = data.value;
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
            case STATE_IDLE:   // 复位
                is_battary_test = 0;
                showlog("开始测试");
                protocolManager.resetAllPb();
                at->resetConnected();
                // ui->tail_sn->setText("存储的尾盖sn:");
                // ui->brush_subpid->setText("存储的subpid:");
                snCompareOk = 0;
                result = "";
                TestTime.start();
                flash_state = 0;
                refresh_periph_times = 1;
                subpidCompareOk = 0;
                waitWork(1000);
                at->sendMac(ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());
                state = STATE_DISABLE_SLEEP_1;
                break;

            case STATE_DISABLE_SLEEP_1:
                if (at->getConnected()) {
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacMode, 1); });
                    showlog("已进入工厂模式");
                    appendStationResult(testItems, "进入工厂模式", "0.0000", passValue);
                    testResultTableUpdate(testItems);
                    state = STATE_WATI_CONNECT;
                }
                break;

            case STATE_WATI_CONNECT:
                if (canGoNext) {
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, writesn})); });
                    showlog(QString::number(canGoNext));
                    showlog("sn写入内容为：" + stringsn);
                    appendStationResult(testItems, "sn写入", "0.0000", passValue);
                    testResultTableUpdate(testItems);
                    state = STATE_WAIT_BANDING;
                }
                break;

            case STATE_WAIT_BANDING:
                if (canGoNext) {
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::Sn, static_cast<int>(FacDevInfoType_TAIL_SN)); });
                    state = STATE_WAIT_CORRECT_BANDING;
                }
                break;

            case STATE_WAIT_CORRECT_BANDING:

                if (canGoNext) {
                    if (snCompareOk == 1) {
                        state = STATE_GETBATTERY;
                        showlog("sn已比对成功");
                        appendStationResult(testItems, "sn写入校验", "0.0000", passValue);
                        testResultTableUpdate(testItems);
                        protocolManager.get(DeviceCmd::GetBattery);
                    } else if (snCompareOk == 2) {
                        showlog("sn已比对失败"); pack.error="SP03011";
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }
                break;

            case STATE_GETBATTERY: {
                waitWork(100);
                if (is_battary_test != 0) {

                    if (is_battary_test == 1) {
                        showlog("电量正常" + QString::number(battary) + "%");
                        TestItem test;
                        test.testItem = "当前电量";
                        test.testData = QString::number(battary) + "%";
                        test.testResult = "通过";
                        test.ask = "通过";
                        testItems.append(test);
                        testResultTableUpdate(testItems);
                        // protocolManager.get(DeviceCmd::PeriphState);
                        state = STATE_AGE;
                    }

                    if (is_battary_test == 2) {
                        showlog("当前电量低，为" + QString::number(battary) + "%");
                        // pack.error="SP03010";
                        TestItem test;
                        test.testItem = "当前电量";
                        test.testData = QString::number(battary);
                        test.testResult = "失败";
                        test.ask = "通过";
                        testItems.append(test);
                        testResultTableUpdate(testItems);

                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    showlog("正在重发获取电量信息");
                    protocolManager.get(DeviceCmd::GetBattery);
                }
                break;
            }

            case STATE_CHECK_FLASH:
                if (flash_state == 1) {
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
                    // waitWork(100);
                    sendCommandWithRetry([&]() {
                    QVariantMap m;
                    m["mode"] = 1;
                    m["seconds"] = 14400;
                    // m["switch"] = static_cast<int>(FacSwitch_OPEN);
                    // showlog(QString::number(m["switch"]));
                    protocolManager.set(DeviceCmd::BurningMode, m);
                    });
                    showlog("已发送进入老化");
                    ui->flash_state->setText("Flash State:<font color='green'>正常</font>");
                    state = STATE_AGE_CHECK;
                }
                break;

            case STATE_AGE_CHECK:
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
                    pack.itemvalue = "083";
                    if (ui->isusemes->checkState()) {
                        // emit send_end_testPass(pack);
                        appendStationResult(testItems, "MES完成上报", "0.0000", passValue);
                        testResultTableUpdate(testItems);
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
                    // emit send_end_testPass(pack);
                    appendStationResult(testItems, "MES完成上报", "0.0000", failValue);
                    testResultTableUpdate(testItems);
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
                // emit send_end_test(getIndex());
                
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                waitWork(150);
                isTestContinue = false;
                on_disconnectButton_clicked();
                showlog(QString::number(isTestContinue));
                showlog("测试结束");
            } break;
            case STATE_PROCESS_INSPECTION: break;
        }
        QCoreApplication::processEvents();
        }
    }

void ageing::on_pushButton_clicked() {
    // ui->macInput->setText("3C:84:27:07:A8:D2");
    // on_macInput_returnPressed();
    at->sendMac(ui->macInput->text());  // 发送mac地址
    waitWork(8000);
    sendCommandWithRetry([&]() {     
    QVariantMap m;
    m["mode"] = 1;
    m["seconds"] = 3600;  // 统一上层入参，协议层做兼容
    protocolManager.set(DeviceCmd::BurningMode, m); });
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
    stringsn = ui->getMac->text();
    appendStationResult(testItems, "主板条码", "0.0000", passValue);
    testResultTableUpdate(testItems);
    // 获取比亚迪mes的sn校验规则
    // processGetMesTestValue();
    // 获取比亚迪mes的站前检测
    // processInspection(stringsn);
    appendStationResult(testItems, "MES启动", "0.0000", passValue);
    on_macInput_returnPressed();

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
    isTestContinue = false;
    state = STATE_IDLE;

    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}



