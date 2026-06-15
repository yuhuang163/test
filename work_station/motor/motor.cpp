
#include "motor.h"

#include "common_utils.h"

#include <QSerialPort>

#include "qatmanager.h"
#include "qdebug.h"
#include "qpb.h"
#include "qprocess.h"
#include "qserialportinfo.h"
#include "ui_motor.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
void motor::on_pushButton_2_clicked() {
    // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    ui->macInput->setText("E1:51:07:34:0B:F8");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    ui->macInput->setText("e2:66:07:34:2d:f7");
    ui->macInput->setText("cb:32:09:30:95:f7");
    ui->macInput->setText("b4:56:5d:bf:57:9d");
    // ui->macInput->setText("74:4D:BD:95:80:7e");
    on_macInput_returnPressed();
}
motor::motor(int index, QWidget* parent) : test_base(parent), ui(new Ui::motor) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = MOTOR_VER;

    ui->setupUi(this);
    scanSerialPorts(); // 要搜索一下一开�?
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    updateMainStyle("Ubuntu.qss");
    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();

    showlog("standbattary=" + QString::number(standbattary));
    showlog("action=" + pack.action);
    showlog("line=" + pack.line);
    showlog("model=" + pack.model);
    showlog("action=" + pack.test_station);
    showlog("machineNo=" + pack.machineNo);

    testResultTableInit();
    ui->tabWidget->setCurrentIndex(0); // 设置当前页为第一�?
}

void motor::refreshMotorCaliMsg(QString msg) {
    showlog(msg);

    const QString folderPath = QStringLiteral("D:/测试结果");
    CommonUtils::ensureDirectory(folderPath);
    const QString filePath =
        CommonUtils::joinPath(folderPath, CommonUtils::formatDateIso() + QStringLiteral("_电机测试log.csv"));

    QFile file(filePath);

    if (!file.exists()) {
        // 文件不存在，打开文件并写入表�?
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            // 获取当前时间�?
            const QString timestamp = CommonUtils::dateTimeStamp();
            // 写入表头
            QStringList headers;
            headers << "sn"
                    << "上位机版�?
                    << "mac地址"
                    << "时间�?
                    << "测试日志";
            stream << headers.join(",") << "\n";
            file.close();
        } else {
            qDebug() << getIndex() << "Error creating file";
            return;
        }
    }

    // 文件已存在，以追加模式打开文件并写入数�?
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);
        // 获取当前时间�?
        const QString timestamp = CommonUtils::dateTimeStamp();
        // 写入数据
        QStringList rowData;
        rowData << stringsn << MOTOR_VER << macAddress << timestamp << msg;
        stream << rowData.join(",") << "\n";

        file.close();
        qDebug() << getIndex() << "Data appended to" << filePath;
    } else {
        qDebug() << getIndex() << "Error appending to file";
    }
}

void motor::control_motor_cmd(QString cmd) {
    // 创建一个QProcess对象
    QProcess process;

    // 设置要运行的可执行文件路�?
    QString program = "./DM2J/DM2J.exe";

    // 如果可执行文件有参数，可以在这里设置
    QStringList arguments;
    arguments << cmd; // 例如，添加参�?arg1"�?arg2"

    // 启动外部可执行文�?
    process.start(program, arguments);

    // 等待可执行文件运行完�?
    process.waitForFinished(-1);

    // 获取可执行文件的输出
    QByteArray output = process.readAllStandardOutput();
    QByteArray error = process.readAllStandardError();

    // 输出可执行文件的输出和错误信�?
    qDebug() << getIndex() << "Output:" << output;
    qDebug() << getIndex() << "Error:" << error;
}

motor::~motor() {
    delete ui;
}

void motor::refreshPbData(QString data) {
    showlog(data);
}
void motor::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void motor::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接�?font color='green'>成功</font>");
        showlog("蓝牙连接成功");
        protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        showlog("已发送禁止休�?);
    } else {
        ui->bleStatusLabel->setText("蓝牙连接�?font color='red'>失败</font>");
        qDebug() << "蓝牙连接断开";

        // showlog("蓝牙连接断开");
    }
}

void motor::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}

void motor::on_connectButton_clicked() {
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void motor::on_motor_cali_clicked() {
    static int caliStep = 1; // 用于跟踪当前运行的校准步�?

    switch (caliStep) {
    case 1:
        protocolManager.set(DeviceCmd::MotorCali, 1);
        showlog("Running set_motor_cali(1)");
        break;

    case 2:
        protocolManager.set(DeviceCmd::MotorCali, 2);
        showlog("Running set_motor_cali(2)");
        break;

    case 3:
        protocolManager.set(DeviceCmd::MotorCali, 3);
        showlog("Running set_motor_cali(3)");
        break;

    case 4:
        protocolManager.set(DeviceCmd::MotorDampingState, 1);
        showlog("Running set_motor_damping_state(1)");
        break;

    case 5:
        protocolManager.set(DeviceCmd::MotorDampingState, 0);
        showlog("Running set_motor_damping_state(0)");
        break;
    }

    caliStep++; // 增加步骤计数

    // 如果步骤计数超过了最大步骤数，重置为第一�?
    if (caliStep > 5) {
        caliStep = 1;
    }
}

void motor::refreshSn(ProtocolSnData data) {
    stringsn = data.value;
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->product_sn->setText("存储整机sn:" + stringsn);
}

void motor::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检�?);
            pack.sn = inputSnText;

            pack.mechines = getIndex();

            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";

            emit send_process_inspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState()) // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void motor::refreshBaseData(ProtocolBaseInfoData data) {
    // QString mac;

    // for (int var = data.ble_mac.size - 1; var >= 0; --var) {
    //     mac += QString("%1").arg(data.ble_mac.bytes[var], 2, 16, QChar('0'));
    //     if (var > 0) {
    //         mac += ":";
    //     }
    // }

    // showlog("当前连接设备的mac地址" + mac.toUpper());

    // if (macAddress.toUpper() != mac.toUpper()) {
    //     isTestContinue = false;
    //     showlog("停止运行");
    //     QMessageBox::warning(NULL, "严重警告", " 当前连接设备与获取的mac不同\t\r\n");
    // }

    // 读取软件版本字符�?
    QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();

    // 读取资源版本字符�?
    QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();

    // 读取电机版本字符�?
    QString motorVersion = SETTINGS.value("ProductInfo/Motor_Ver").toString();

    // 读取蓝牙版本字符�?
    QString bleVersion = SETTINGS.value("ProductInfo/Ble_Ver").toString();

    showlog("设备名字�? + QString(data.product_name));

    bool isMotorTest = SETTINGS.value("ProductInfo/MotorVersion_checkBox").toBool();
    bool isResourceTest = SETTINGS.value("ProductInfo/ResourceVersion_checkBox").toBool();
    bool isSoftwareTest = SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool();
    bool isBleTest = SETTINGS.value("ProductInfo/BluetoothVersion_checkBox").toBool();

    if ((!isSoftwareTest || compareVersions(softwareVersion, data.soft_version)) &&
        (!isResourceTest || compareVersions(resourceVersion, data.res_version)) &&
        (!isBleTest || compareVersions(bleVersion, data.ble_version)) &&
        (!isMotorTest || compareVersions(motorVersion, data.motor_version))) {
        showlog("软件版本正确" + data.soft_version);
        showlog("资源版本正确" + data.res_version);
        showlog("电机版本正确" + data.motor_version);
        showlog("蓝牙版本正确" + data.ble_version);
        showlog("软件版本正确");

    } else {
        ui->test_result->setText("FAIL");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                       "#FF0000; border-radius: 10px; padding: 10px; text-align: center;");

        showlog("版本错误");
        showlog("当前设备软件版本" + data.soft_version + "配置文件软件版本" + softwareVersion);
        showlog("当前设备资源版本" + data.res_version + "配置文件资源版本" + resourceVersion);
        showlog("当前设备电机版本" + data.motor_version + "配置文件电机版本" + motorVersion);
        showlog("当前设备蓝牙版本" + data.ble_version + "配置文件蓝牙要求" + bleVersion);

        isTestContinue = false;
        showlog("停止运行");

        ui->macInput->clear();

#ifdef NEW_MOTOR_MES
        ui->get_sn->clear();
        ui->get_sn->setFocus();
#else
        ui->getMac->clear();
        ui->getMac->setFocus();
#endif
    }
    TestItem test;

    // Check for BLE version
    if (isBleTest) {
        test.testItem = "蓝牙版本";
        test.testData = data.ble_version;
        test.ask = bleVersion;
        testItems.append(test);
    }

    // Check for Motor version
    if (isMotorTest) {
        test.testItem = "电机版本";
        test.testData = data.motor_version;
        test.ask = motorVersion;
        testItems.append(test);
    }

    // Check for Resource version
    if (isResourceTest) {
        test.testItem = "资源版本";
        test.testData = data.res_version;
        test.ask = resourceVersion;
        testItems.append(test);
    }

    // Check for Software version
    if (isSoftwareTest) {
        test.testItem = "软件版本";
        test.testData = data.soft_version;
        test.ask = softwareVersion;
        testItems.append(test);
    }

    updateTestData(testItems);
}
void motor::refreshBattaryData(ProtocolBatteryData adc) {
    QString chargeStateStr;
    switch (adc.chargeState) {
    case 1:
        chargeStateStr = "充电状态为�?span style='color:green'>电量充满</span>";
        break;
    case 2:
        chargeStateStr = "充电状态为�?span style='color:orange'>正在充电</span>";
        break;
    case 3:
        chargeStateStr = "充电状态为�?span style='color:red'>充电断开</span>";
        break;
    case 4:
        chargeStateStr = "充电状态为�?span style='color:red'>没有电池</span>";
        break;
    default:
        chargeStateStr = "充电状态为�?span style='color:red'>未知</span>";
        break;
    }
    ui->battary_state->setText(chargeStateStr);

    // 修改电量的显示样�?
    QString batteryPercentStr =
        "电量为：<span style='color:blue'>" + QString::number(adc.percent) + "%</span>";
    ui->battary_value->setText(batteryPercentStr);

    // 修改电压的显示样�?
    QString batteryVoltageStr = "电压为：<span style='color:purple'>" +
        QString::number(adc.voltageMv / 1000.0, 'f', 3) +
        "V</span>";
    ui->battary_voltage->setText(batteryVoltageStr);

    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);
    is_battary_test = 1;
    if (adc.voltageMv / 1000.0 > standbattary) {
        TestItem test;
        test.testItem = "电压测试";
        test.testData = QString::number(adc.voltageMv / 1000.0) + "V";
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("电压通过");
    } else {
        TestItem test;
        test.testItem = "电压测试";
        test.testData = "当前电压�? + QString::number(adc.voltageMv / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("电压太低" + QString::number(adc.voltageMv / 1000.0) + "V");
        result = failValue;
        state = STATE_SAVE_RESULT;
    }
}

void motor::canGoNextMechine(int x) {
    is_canGoNext = 1;
    qDebug() << getIndex() << "得到信息" << x;

    if (x == getIndex()) {
        result = failValue;
        showlog("停止运行");
        ui->test_result->setText("FAIL");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                       "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
        refreshMotorCaliMsg("电机测试失败");
        pack.error = "SP03006";
        state = STATE_SAVE_RESULT;
        showlog("电机测试失败");
    }
}

void motor::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
        case STATE_IDLE: // 复位一�?
            refreshMotorCaliMsg("开始测�?);
            // showlog("开始测�?);
            pb->reset_all_pb();

            at->resetConnected();
            is_canGoNext = 0;
            refreshBleState(0);
            ui->product_sn->setText("存储整机sn:");
            stringsn = "";
            result = passValue;
            TestTime.start();
            waitWork(1000);
            at->set(DongleCmd::BleScanConnect, ui->macInput->text()); // 发送mac地址
            showlog("MAC地址为：" + ui->macInput->text());
            is_battary_test = 0;

            state = STATE_WATI_CONNECT;
            break;
        case STATE_WATI_CONNECT:
            if (at->getConnected()) {
                sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::BaseInfo); });
                state = STATE_GETBASEDATA;
            }

            break;
        case STATE_GETBASEDATA:
            if (canGoNext) {
                sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetBattery); });
                state = STATE_WATI_CORRECT_BATTARY;
            }
            break;

        case STATE_WATI_CORRECT_BATTARY: // 设置禁止休眠
            if (is_battary_test) {
                showlog("已成功完成电量测�?);
                state = STATE_DISABLE_SLEEP_1;
            }

            break;

        case STATE_DISABLE_SLEEP_1:
            if (pb->getState(Qpb::PbStateType::DisableSleep)) {
                showlog("已进入禁止休眠模�?);
                protocolManager.set(DeviceCmd::MotorTestState, 0);
                waitWork(100);
                protocolManager.set(DeviceCmd::MotorCaliState, 1);
                waitWork(100);
                protocolManager.set(DeviceCmd::MotorDampingState, 0);
                waitWork(100);
                refreshMotorCaliMsg("解除阻尼");
                // showlog("解除阻尼");
                // #ifdef LXSET
                //                      waitWork(1000);
                //                      control_motor_cmd("/c:-90");
                //                         waitWork(1000);
                // #endif

                state = UNLOCK_DAMPING;

                // protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn}));
                // showlog("已发送sn绑定");
                // state = STATE_SN_CHECK;
            } else {
                waitWork(500);
                showlog("正在重发进入禁止休眠");
                protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
            }
            break;

            //         case STATE_SN_CHECK:
            //             if (pb->get_is_banding_ok()) {
            //                 showlog("sn已成功绑定保�?);
            //                 stringsn = ui->getMac->text();
            //                 showlog(stringsn);

            //             } else {
            //                 waitWork(500);
            //                 protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn}));
            //                 showlog("重发sn绑定");
            //             }
            //             break;

        case UNLOCK_DAMPING:

            if (pb->getState(Qpb::PbStateType::DampingState) == 1) {
                refreshMotorCaliMsg("正在进行霍尔校准");
                // showlog("正在进行霍尔校准");

                protocolManager.set(DeviceCmd::MotorCali, 1);
                waitWork(WAITTIME);
                state = MOTOR_CALI1;
            } else {
                protocolManager.set(DeviceCmd::MotorDampingState, 0);
                showlog("重发解除阻尼");
                waitWork(500);
            }

        case MOTOR_CALI1:
            if (pb->getState(Qpb::PbStateType::HallCali) == 1) {
                refreshMotorCaliMsg("霍尔校准完成");
                qDebug() << getIndex() << "发射霍尔校准完成";
                emit send_go_next_test(getIndex());

                state = MOTOR_WAIT_CALI1;
            }
            if (pb->getState(Qpb::PbStateType::HallCali) == 2) {
                refreshMotorCaliMsg("霍尔校准失败");
                pack.error = "SP03004";
                result = failValue;
                state = STATE_SAVE_RESULT;
            }
            break;
        case MOTOR_WAIT_CALI1:
            if (is_canGoNext) {
                is_canGoNext = 0;
                waitWork(WAITTIME);
                protocolManager.set(DeviceCmd::MotorCali, 2);
                waitWork(WAITTIME);
                showlog("正在进行零点校准");
                state = MOTOR_CALI2;
            }

            break;
        case MOTOR_CALI2:
            if (pb->getState(Qpb::PbStateType::ZeroCali) == 1) {
                refreshMotorCaliMsg("零点校准完成");
                // showlog("零点校准完成");
                if (pack.factory == "lx") {
                    qDebug() << getIndex() << "发射零点校准完成";
                    emit send_go_next_test(getIndex());
                    state = MOTOR_WAIT_CALI2;
                    break;
                }

                protocolManager.set(DeviceCmd::MotorCaliState, 0);
                state = STOP_MOTOR_CALI;
            }
            if (pb->getState(Qpb::PbStateType::ZeroCali) == 2) {
                pack.error = "SP03005";
                result = failValue;
                state = STATE_SAVE_RESULT;
            }
            break;

        case MOTOR_WAIT_CALI2:
            if (is_canGoNext) {
                is_canGoNext = 0;
                protocolManager.set(DeviceCmd::MotorCaliState, 0);
                state = STOP_MOTOR_CALI;
            }
            break;

        case STOP_MOTOR_CALI:
            qDebug() << "当前状态是" << state;
            if (pb->getState(Qpb::PbStateType::StopMotorCali)) {
                refreshMotorCaliMsg("正在进行电机测试");
                // showlog("正在进行电机测试");
                protocolManager.set(DeviceCmd::SevorMotorParam, QVariant::fromValue(SevorMotorParamPayload{14, 12, 5.2, 190}));
                //  protocolManager.set(DeviceCmd::MotorTestState, 1);

                state = MOTOR_TESTING;
            } else {
                protocolManager.set(DeviceCmd::MotorCaliState, 0);
                refreshMotorCaliMsg("重发关闭校准流程");
                waitWork(500);
            }
            break;
        case MOTOR_TESTING:
            if (pb->getState(Qpb::PbStateType::MotorTestState)) {

                qDebug() << getIndex() << "发射电机测试完成";
                emit send_go_next_test(getIndex());
                state = MOTOR_WAIT_TESTING;
            } else {
                protocolManager.set(DeviceCmd::SevorMotorParam, QVariant::fromValue(SevorMotorParamPayload{14, 12, 5.2, 190}));
                refreshMotorCaliMsg("重发电机测试");
                waitWork(500);
            }
            break;

        case MOTOR_WAIT_TESTING:
            if (is_canGoNext) {
                is_canGoNext = 0;
                state = STATE_SAVE_RESULT;
            }
            break;

        case STATE_SAVE_RESULT:

            if (result == passValue) {
                QString mesresult = "PASS";
                pack.result = mesresult;
                pack.itemvalue = QString("|MOTOR_TEST:PASS|");
                if (ui->isusemes->checkState()) {
                    emit send_end_test_pass(pack);
                }

                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
            } else if ((result == failValue)) {
                QString mesresult = "NG";
                pack.result = mesresult;
                pack.itemvalue = QString("|MOTOR_TEST:NG|");
                pack.sn = ui->getMac->text();

                if (ui->isusemes->checkState()) {
                    emit send_end_test_pass(pack);
                }

                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");
            }

            TestItem test;
            test.testItem = "电机校准";
            test.testData = "";
            test.testResult = result;
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            sn = "";
            stringsn = "";
            ui->macInput->clear();
            ui->getMac->clear();
            ui->getMac->setFocus();
            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);
            waitWork(WAITTIME);
            protocolManager.set(DeviceCmd::SevorMotorParam, QVariant::fromValue(SevorMotorParamPayload{0, 0, 0, 0}));
            waitWork(500);
            at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00"); // 发送mac地址
            waitWork(50);
            on_disconnectButton_clicked();
            // showlog("测试结束");
            refreshMotorCaliMsg("测试结束");
            isTestContinue = false;
            emit send_end_test(getIndex());

            state = STATE_IDLE;
            break;
        }
    }
}

void motor::startTest_task() {
    if (is_motor_test_continue) {
        switch (test_state) {
        case STATE_IDLE: // 复位一�?
            showlog("开始电机测�?);
            pb->reset_all_pb();
            at->resetConnected();
            refreshBleState(0);
            ui->product_sn->setText("存储整机sn:");
            stringsn = "";
            test_result = passValue;
            test_state = STATE_WATI_CONNECT;
            break;
        case STATE_WATI_CONNECT:
            if (at->getConnected()) {
                test_state = STATE_DISABLE_SLEEP_1;
            }
            break;

        case STATE_DISABLE_SLEEP_1:
            if (pb->getState(Qpb::PbStateType::DisableSleep)) {
                showlog("已进入禁止休眠模�?);
                waitWork(WAITTIME);
                protocolManager.set(DeviceCmd::MotorTestState, 0);
                waitWork(WAITTIME);
                protocolManager.set(DeviceCmd::MotorDampingState, 0);
                waitWork(WAITTIME);
                showlog("解除阻尼");
                QMessageBox::warning(NULL, "警告", " 请把电机置于�?位\t\r\n");
                protocolManager.get(DeviceCmd::Sn, static_cast<int>(FacDevInfoType_TAIL_SN));
                waitWork(WAITTIME);
                showlog("打开阻尼");
                protocolManager.set(DeviceCmd::MotorDampingState, 1);
                test_state = MOTOR_TESTING;
                showlog("正在查询SN");
            }
            break;

        case MOTOR_TESTING:
            if (pb->getState(Qpb::PbStateType::DampingState) == 1) {
                QMessageBox::StandardButton reply;
                reply =
                    QMessageBox::question(this, "电机测试", "设备是否成功复位", QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No) {
                    test_result = failValue;
                }
                test_state = STATE_SAVE_RESULT;
            }

            break;

        case STATE_SAVE_RESULT:

            if (test_result == passValue) {
                QString mesresult = "PASS";
                QString itemvalue = QString("|MOTOR_TEST:PASS|");
                pack.result = mesresult;

                pack.itemvalue = itemvalue;

                if (ui->isusemes->checkState()) {
                    emit send_end_test_pass(pack);
                }

                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
            } else if ((test_result == failValue)) {
                QString mesresult = "NG";
                QString itemvalue = QString("|MOTOR_TEST:NG|");
                pack.result = mesresult;

                pack.itemvalue = itemvalue;

                pack.sn = ui->getMac->text();

                if (ui->isusemes->checkState()) {
                    emit send_end_test_pass(pack);
                }
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");
            }
            TestItem test;
            test.testItem = "电机测试";
            test.testData = "";
            test.testResult = result;
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            sn = "";
            stringsn = "";
            ui->macInput->clear();
            ui->getMac->clear();
            ui->getMac->setFocus();
            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);
            waitWork(WAITTIME);
            at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00"); // 发送mac地址
            waitWork(50);
            on_disconnectButton_clicked();
            showlog("测试结束");
            is_motor_test_continue = false;
            test_state = STATE_IDLE;
            break;
        }
    }
}

void motor::getTestValue(const int mechines, const QString value) {
    // showlog(value);
    QString mesmacAddress;
    if (pack.factory == "hq") {
        // 定义正则表达式，匹配MAC地址的模�?
        QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");

        // 在数据中查找匹配的内�?
        QRegularExpressionMatch match = regex.match(value);

        // 检查是否有匹配�?
        if (match.hasMatch()) {
            // 提取MAC地址
            mesmacAddress = match.captured(1);
            qDebug() << getIndex() << "MAC地址�? << mesmacAddress;
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

        // �?�?�?�?�?0的位置插入冒�?
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
    } else if (pack.factory == "hz") {
        if (mechines != getIndex()) {
            return;
        }
        const QString snFromMes = value.trimmed();
        mesmacAddress = parseMacFromSn(snFromMes);
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            showlog(value);
            return;
        }
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        on_macInput_returnPressed();
    } else {
        if (mechines == getIndex()) {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }

    // bindingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试mac——sn
}

void motor::on_damping_open_clicked() {
    protocolManager.set(DeviceCmd::MotorDampingState, 1);
}

void motor::on_damping_close_clicked() {
    protocolManager.set(DeviceCmd::MotorDampingState, 0);
}

void motor::on_getMac_returnPressed() {
    testResultTableInit();

    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    applyAdaptiveV3ProductBySn(ui->getMac);
    // 检查是否是序列号格�?
    QRegularExpression snRegex(snPattern);

    // 使用正则表达式匹�?
    if (!snRegex.match(ui->getMac->text()).hasMatch()) {
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("序列号错�?);
        showlog("实际长度�? + QString::number(ui->getMac->text().length()));
        showlog("要求格式�? + snPattern);
        ui->getMac->clear();
        ui->getMac->setFocus();
        return;
    }

    showlog("正在查询mac地址");
    sn = ui->getMac->text().toUtf8();
    getMac(ui->getMac->text()); // 文件获取
    processInspection(ui->getMac->text());
    processGetMesTestValue(); // mes获取
}
void motor::processGetMesTestValue() {
    if (pack.factory == "hz") {
        pack.sn = ui->getMac->text();
        pack.mechines = getIndex();
        getTestValue(getIndex(), pack.sn.trimmed());
        return;
    }
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";

        emit send_mes_test_value(pack);
    }
}
void motor::on_macInput_returnPressed() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹�?
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        // QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        // qDebug() << getIndex()<< macAddress;

        if (ui->ismotortest->isChecked()) {
            is_motor_test_continue = true;
            isTestContinue = false;
        } else {
            is_motor_test_continue = false;
            isTestContinue = true;
        }

        emit send_go_next_focus();

        state = STATE_IDLE;
    }
}

// E2:33:07:34:8E:F5

void motor::on_end_cali_clicked() {
    sn = "";
    stringsn = "";
    showlog("测试结束");
    isTestContinue = false;
    state = STATE_IDLE;
}

void motor::on_stopTest_clicked() {
    // at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00");   // 发送mac地址
    // waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}
