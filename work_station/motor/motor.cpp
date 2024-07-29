
#include "motor.h"
#include "qat.h"
#include "qdebug.h"
#include "qpb.h"
#include "qprocess.h"
#include "qserialportinfo.h"
#include "ui_motor.h"
#include <QSerialPort>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

motor::motor(int index, QWidget *parent) : ui(new Ui::motor)
{    m_index=index;

    ui->setupUi(this);
    scanSerialPorts();   // 要搜索一下一开始
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

   
    update_main_style("Ubuntu.qss");
    standbattary = settings.value("BATTARY/standbattary").toDouble();

    showlog("standbattary=" + QString::number(standbattary));
    showlog("action=" + pack.action);
    showlog("line=" + pack.line);
    showlog("model=" + pack.model);
    showlog("action=" + pack.test_station);
    showlog("machineNo=" + pack.machineNo);

    if (pack.product == "U7" || pack.product == "U7P")
        ui->nfc_sn->setDisabled(1);
    else
        ui->nfc_sn->setDisabled(0);
}

void motor::refresh_motor_cali_msg(QString msg)
{
    showlog(msg);
    // qDebug() << getIndex()<< "收到数据: " << getIndex();

    // 构建 "测试结果" 文件夹的完整路径，这里选择保存到D盘
    QString folderPath = "D:/测试结果";

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists())
    {
        QDir().mkpath(folderPath);
    }

    // 获取当前日期
    QDate currentDate = QDate::currentDate();

    // 构建年、月、日的文件夹结构
    QString yearFolder = QString::number(currentDate.year());
    QString monthFolder = currentDate.toString("MM");
    QString dayFolder = currentDate.toString("dd");

    // 构建完整的文件路径，加上日期
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_电机测试log.csv";
    QString filePath = QDir(folderPath).filePath(fileName);

    QFile file(filePath);

    if (!file.exists())
    {
        // 文件不存在，打开文件并写入表头
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream stream(&file);
            // 获取当前时间戳
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 写入表头
            QStringList headers;
            headers << "sn" << "上位机版本" << "mac地址" << "时间戳" << "测试日志";
            stream << headers.join(",") << "\n";
            file.close();
        }
        else
        {
            qDebug() << getIndex() << "Error creating file";
            return;
        }
    }

    // 文件已存在，以追加模式打开文件并写入数据
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);
        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        // 写入数据
        QStringList rowData;
        rowData << stringsn << MOTOR_VER << macAddress << timestamp << msg;
        stream << rowData.join(",") << "\n";

        file.close();
        qDebug() << getIndex() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << getIndex() << "Error appending to file";
    }
}

void motor::control_motor_cmd(QString cmd)
{
    // 创建一个QProcess对象
    QProcess process;

    // 设置要运行的可执行文件路径
    QString program = "./DM2J/DM2J.exe";

    // 如果可执行文件有参数，可以在这里设置
    QStringList arguments;
    arguments << cmd;   // 例如，添加参数"arg1"和"arg2"

    // 启动外部可执行文件
    process.start(program, arguments);

    // 等待可执行文件运行完成
    process.waitForFinished(-1);

    // 获取可执行文件的输出
    QByteArray output = process.readAllStandardOutput();
    QByteArray error = process.readAllStandardError();

    // 输出可执行文件的输出和错误信息
    qDebug() << getIndex() << "Output:" << output;
    qDebug() << getIndex() << "Error:" << error;
}

motor::~motor()
{
    delete ui;
}

void motor::refresh_pb_data(QString data)
{
    showlog(data);
}
void motor::refresh_dongle_uart_state(int state)
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

void motor::refresh_ble_state(int state)
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
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void motor::on_disconnectButton_clicked()
{
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refresh_ble_state(0);
}

void motor::on_connectButton_clicked()
{
    openDongleSerialPort();
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
}

void motor::on_pushButton_2_clicked()
{
    // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    ui->macInput->setText("E1:51:07:34:0B:F8");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    ui->macInput->setText("e2:66:07:34:2d:f7");
    ui->macInput->setText("cb:32:09:30:95:f7");
    ui->macInput->setText("c9:76:09:30:95:f6");
    // ui->macInput->setText("74:4D:BD:95:80:7e");
    on_macInput_returnPressed();
}

void motor::on_motor_cali_clicked()
{
    static int caliStep = 1;   // 用于跟踪当前运行的校准步骤

    switch (caliStep)
    {
    case 1:
        pb->set_motor_cali(1);
        showlog("Running set_motor_cali(1)");
        break;

    case 2:
        pb->set_motor_cali(2);
        showlog("Running set_motor_cali(2)");
        break;

    case 3:
        pb->set_motor_cali(3);
        showlog("Running set_motor_cali(3)");
        break;

    case 4:
        pb->set_motor_damping_state(1);
        showlog("Running set_motor_damping_state(1)");
        break;

    case 5:
        pb->set_motor_damping_state(0);
        showlog("Running set_motor_damping_state(0)");
        break;
    }

    caliStep++;   // 增加步骤计数

    // 如果步骤计数超过了最大步骤数，重置为第一步
    if (caliStep > 5)
    {
        caliStep = 1;
    }
}

void motor::solveMesSucess(const int mechines)
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
void motor::solveMesData(const int mechines, QString msg)
{
    if (mechines == getIndex())
    {
        showlog("MES:报错信息:" + msg);
        ui->macInput->setDisabled(0);
        ui->get_mac->setDisabled(0);
        is_motor_continue = false;
        showlog("停止运行");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
        emit endTest(getIndex());

        ui->get_mac->clear();
        ui->get_mac->setFocus();
    }
}
void motor::get_dongle_ver(QString data)
{
    showlog("当前dongle的版本为：" + data);
}
void motor::refresh_sn(FacDevInfo data)
{
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("存储尾盖sn:" + stringsn);
}


void motor::processInspection(QString stringsn)
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

void motor::refresh_base_data(FacGetDevBaseInfo data)
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    // 读取软件版本字符串
    QString softwareVersions = settings.value("ProductInfo/Software_Version").toString();
    QStringList softwareVersionList = softwareVersions.split('=');

    // 读取资源版本字符串
    QString resourceVersions = settings.value("ProductInfo/Resource_Version").toString();
    QStringList resourceVersionList = resourceVersions.split('=');

    // 读取电机版本字符串
    QString motorVersions = settings.value("ProductInfo/Motor_Ver").toString();
    QStringList motorVersionList = motorVersions.split('=');

    // 读取蓝牙版本字符串
    QString bleVersions = settings.value("ProductInfo/Ble_Ver").toString();
    QStringList bleVersionList = bleVersions.split('=');


    // 输出蓝牙状态列表
    for (const QString &version : bleVersionList) {
        qDebug() << "ble Version:" << version.trimmed();
    }


    // 输出电机版本列表
    for (const QString &version : motorVersionList)
    {
        qDebug() << "motor Version:" << version.trimmed();
    }

    // 输出软件版本列表
    for (const QString &version : softwareVersionList)
    {
        qDebug() << "Software Version:" << version.trimmed();
    }

    // 输出资源版本列表
    for (const QString &version : resourceVersionList)
    {
        qDebug() << "Resource Version:" << version.trimmed();
    }

    showlog("设备名字为" + QString(data.product_name));

    if (softwareVersionList.contains(data.soft_version) &&
        resourceVersionList.contains(data.res_version)&&
          bleVersionList.contains(data.ble_version) &&
        motorVersionList.contains(data.motor_version))

    {


        showlog("软件版本正确" + QString::fromUtf8(data.soft_version));
        showlog("资源版本正确" + QString::fromUtf8(data.res_version));
        showlog("电机版本正确" + QString::fromUtf8(data.motor_version));
        showlog("蓝牙版本正确" + QString::fromUtf8(data.ble_version));
        showlog("软件版本正确");


    }
    else
    {
        ui->test_result->setText("FAIL");
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center;");

        showlog("版本错误");
        showlog("当前设备软件版本" + QString::fromUtf8(data.soft_version) +
                                     "配置文件软件版本" + softwareVersions);
        showlog("当前设备资源版本" + QString::fromUtf8(data.res_version) +
                                     "配置文件资源版本" + resourceVersions);
        showlog("当前设备电机版本" + QString::fromUtf8(data.motor_version) +
                                     "配置文件电机版本" + motorVersions);
        showlog("当前设备蓝牙版本" + QString::fromUtf8(data.ble_version) + "配置文件蓝牙要求" +
                                                         bleVersions);

        is_motor_continue = false;
        showlog("停止运行");

        ui->macInput->clear();

#ifdef NEW_MOTOR_MES
        ui->get_sn->clear();
        ui->get_sn->setFocus();
#else
        ui->get_mac->clear();
        ui->get_mac->setFocus();
#endif
    }
}
void motor::refresh_battary_data(FacDevInfo adc)
{
    QString chargeStateStr;
    switch (adc.dev_info[0].value_item.battery.charge_state)
    {
    case 1:
        chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>";
        break;
    case 2:
        chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>";
        break;
    case 3:
        chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>";
        break;
    case 4:
        chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>";
        break;
    default:
        chargeStateStr = "充电状态为：<span style='color:red'>未知</span>";
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

    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);
    is_battary_test = 1;
    if (adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary)
    {
        TestItem charge;
        charge.testItem = "充电测试";
        charge.testData =
            "正在充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        charge.testResult = "通过";
        testItems.append(charge);
        showlog("电量通过");
    }else{
        TestItem charge;
        charge.testItem = "充电测试";
        charge.testData =
            "当前电压为" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
         charge.testResult = "失败";
        testItems.append(charge);
        showlog("电压太低"+ QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V");
        result = failValue;
        state = STATE_SAVE_RESULT;

    }


}
void motor::start_task()
{
    if (is_motor_continue)
    {
        ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state)
        {
        case STATE_IDLE:   // 复位一切
            refresh_motor_cali_msg("开始测试");
            // showlog("开始测试");
            pb->reset_all_pb();
            testItems.clear();
            at->resetConnected();
            refresh_ble_state(0);
            ui->tail_sn->setText("存储尾盖sn:");
            stringsn = "";
            result = passValue;
            TestTime.start();
            at->sendMac(ui->macInput->text());   // 发送mac地址
            is_battary_test = 0;

            state = STATE_WATI_CONNECT;
            break;
        case STATE_WATI_CONNECT:
            if (at->getConnected())
            {
                sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb));
                state = STATE_GETBASEDATA;
            }


            break;
        case STATE_GETBASEDATA:
            if (canGoNext)
            {
                sendCommandWithRetry(std::bind(&Qpb::get_battery, pb));
                state = STATE_WATI_CORRECT_BATTARY;

            }
            break;


        case STATE_WATI_CORRECT_BATTARY:   // 设置禁止休眠
            if (is_battary_test)
            {
                showlog("已成功完成电量测试");
                state = STATE_DISABLE_SLEEP_1;
            }

            break;

        case STATE_DISABLE_SLEEP_1:
            if (pb->getDisableSleep())
            {
                showlog("已进入禁止休眠模式");
                pb->set_sn(FacDevInfoType_TAIL_SN, sn);
                showlog("已发送sn绑定");
                state = STATE_SN_CHECK;
            } else
            {
                waitWork(500);
                showlog("正在重发进入禁止休眠");
                pb->set_forbid_sleep(FacSwitch_OPEN);
            }
            break;

        case STATE_SN_CHECK:
            if (pb->get_is_banding_ok())
            {
                showlog("sn已成功绑定保存");

                stringsn = ui->get_mac->text();
                showlog(stringsn);
                waitWork(WAITTIME);
                pb->set_motor_test_state(0);
                waitWork(WAITTIME);
                pb->set_motor_cali_state(1);
                waitWork(WAITTIME);
                pb->set_motor_damping_state(0);
                waitWork(WAITTIME);
                refresh_motor_cali_msg("解除阻尼");
                // showlog("解除阻尼");

                // #ifdef LXSET
                //                      waitWork(1000);
                //                      control_motor_cmd("/c:-90");
                //                         waitWork(1000);

                // #else
                //                      QMessageBox::warning(NULL, "警告", "
                //                      请把刷头置于非0位\t\r\n");
                // #endif

                state = UNLOCK_DAMPING;
            }
            else
            {
                waitWork(500);
                pb->set_sn(FacDevInfoType_TAIL_SN, sn);
                showlog("重发sn绑定");
            }
            break;

        case UNLOCK_DAMPING:

            if (pb->get_is_damping_state() == 1)
            {
                refresh_motor_cali_msg("正在进行霍尔校准");
                // showlog("正在进行霍尔校准");

                pb->set_motor_cali(1);
                waitWork(WAITTIME);
                state = MOTOR_CALI1;
            }
            else
            {
                pb->set_motor_damping_state(0);
                showlog("重发解除阻尼");
                waitWork(500);
            }

        case MOTOR_CALI1:
            if (pb->getisHallCali() == 1)
            {
                refresh_motor_cali_msg("霍尔校准完成");

                // showlog("霍尔校准完成");

                // #ifdef    LXSET
                //                    waitWork(WAITTIME);
                //                 control_motor_cmd("/c:zero");
                //                     waitWork(1000);
                // #else
                //                 QMessageBox::warning(NULL, "警告", " 请把刷头置于0位\t\r\n");
                // #endif
                QMessageBox::warning(NULL, "警告", " 请把刷头置于0位\t\r\n");
                waitWork(WAITTIME);
                pb->set_motor_cali(2);
                waitWork(WAITTIME);
                showlog("正在进行零点校准");
                state = MOTOR_CALI2;
            }
            if (pb->getisHallCali() == 2)
            {
                refresh_motor_cali_msg("霍尔校准失败");

                result = failValue;
                state = STATE_SAVE_RESULT;
            }
            break;
        case MOTOR_CALI2:
            if (pb->getisZeroCali() == 1)
            {
                refresh_motor_cali_msg("零点校准完成");
                // showlog("零点校准完成");
                if (pack.factory == "lx")
                    QMessageBox::warning(NULL, "警告", " 校准完成,请取出电机\t\r\n");

                pb->set_motor_cali_state(0);

                state = STOP_MOTOR_CALI;
            }
            if (pb->getisZeroCali() == 2)
            {
                result = failValue;
                state = STATE_SAVE_RESULT;
            }
            break;
        case STOP_MOTOR_CALI:
            if (pb->get_is_stop_motor_cali())
            {
                refresh_motor_cali_msg("正在进行电机测试");
                // showlog("正在进行电机测试");
                pb->set_sevor_motor_param(14, 12, 5.2, 190);
                //  pb->set_motor_test_state(1);

                state = MOTOR_TESTING;
            }
            else
            {
                pb->set_motor_cali_state(0);
                refresh_motor_cali_msg("重发关闭校准流程");
                waitWork(500);
            }

        case MOTOR_TESTING:
            if (pb->get_is_motor_test_state())
            {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this, "电机测试", "电机正常吗？",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    refresh_motor_cali_msg("电机测试失败");
                    result = failValue;
                }
                state = STATE_SAVE_RESULT;
            }
            else
            {
                pb->set_sevor_motor_param(14, 12, 5.2, 190);
                refresh_motor_cali_msg("重发电机测试");
                waitWork(500);
            }
            break;

        case STATE_SAVE_RESULT:

            if (result == passValue)
            {
                QString mesresult = "PASS";
                pack.result = mesresult;

                pack.itemvalue = QString("|MOTOR_TEST:PASS|");

                if (ui->isusemes->checkState())
                {
                    sendTestPass(pack);
                }

                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
            }
            else if ((result == failValue))
            {
                QString mesresult = "NG";
                pack.result = mesresult;

                pack.itemvalue = QString("|MOTOR_TEST:NG|");

                pack.sn = ui->get_mac->text();

                if (ui->isusemes->checkState())
                {
                    sendTestPass(pack);
                }

                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
            }

            TestItem test;
            test.testItem = "电机校准";
            test.testData = "";
            test.testResult = result;
            testItems.append(test);

            log->saveTestCsv(MOTOR_VER, ui->get_mac->text(), ui->macInput->text(), testItems);

            sn = "";
            stringsn = "";
            ui->macInput->clear();
            ui->get_mac->clear();
            ui->get_mac->setFocus();
            ui->macInput->setDisabled(0);
            ui->get_mac->setDisabled(0);
            // ui->macInput->setFocus();
            waitWork(WAITTIME);
            // pb->set_motor_test_state(0);
            pb->set_sevor_motor_param(0, 0, 0, 0);
            waitWork(500);
            at->sendMac("00:00:00:00:00:00");   // 发送mac地址
            waitWork(50);
            on_disconnectButton_clicked();
            // showlog("测试结束");
            refresh_motor_cali_msg("测试结束");
            is_motor_continue = false;
            emit endTest(getIndex());

            state = STATE_IDLE;
            break;
        }
    }
}

void motor::start_test_task()
{
    if (is_motor_test_continue)
    {
        switch (test_state)
        {
        case STATE_IDLE:   // 复位一切
            showlog("开始电机测试");
            pb->reset_all_pb();
            at->resetConnected();
            refresh_ble_state(0);
            ui->tail_sn->setText("存储尾盖sn:");
            stringsn = "";
            test_result = passValue;
            test_state = STATE_WATI_CONNECT;
            break;
        case STATE_WATI_CONNECT:
            if (at->getConnected())
            {
                test_state = STATE_DISABLE_SLEEP_1;
            }
            break;

        case STATE_DISABLE_SLEEP_1:
            if (pb->getDisableSleep())
            {
                showlog("已进入禁止休眠模式");
                waitWork(WAITTIME);
                pb->set_motor_test_state(0);
                waitWork(WAITTIME);
                pb->set_motor_damping_state(0);
                waitWork(WAITTIME);
                showlog("解除阻尼");
                QMessageBox::warning(NULL, "警告", " 请把刷头置于非0位\t\r\n");
                pb->get_sn(FacDevInfoType_TAIL_SN);
                waitWork(WAITTIME);
                showlog("打开阻尼");
                pb->set_motor_damping_state(1);
                test_state = MOTOR_TESTING;
                showlog("正在查询SN");
            }
            break;

        case MOTOR_TESTING:
            if (pb->get_is_damping_state() == 1)
            {
                QMessageBox::StandardButton reply;
                reply = QMessageBox::question(this, "电机测试", "牙刷是否成功复位",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    test_result = failValue;
                }
                test_state = STATE_SAVE_RESULT;
            }

            break;

        case STATE_SAVE_RESULT:

            if (test_result == passValue)
            {
                QString mesresult = "PASS";
                QString itemvalue = QString("|MOTOR_TEST:PASS|");
                pack.result = mesresult;

                pack.itemvalue = itemvalue;

                if (ui->isusemes->checkState())
                {
                    sendTestPass(pack);
                }

                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
            }
            else if ((test_result == failValue))
            {
                QString mesresult = "NG";
                QString itemvalue = QString("|MOTOR_TEST:NG|");
                pack.result = mesresult;

                pack.itemvalue = itemvalue;

                pack.sn = ui->get_mac->text();

                if (ui->isusemes->checkState())
                {
                    sendTestPass(pack);
                }
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
            }
            TestItem test;
            test.testItem = "电机测试";
            test.testData = "";
            test.testResult = result;
            testItems.append(test);
            log->saveTestCsv(MOTOR_VER, ui->get_mac->text(), ui->macInput->text(), testItems);

            sn = "";
            stringsn = "";
            ui->macInput->clear();
            ui->get_mac->clear();
            ui->get_mac->setFocus();
            ui->macInput->setDisabled(0);
            ui->get_mac->setDisabled(0);
            waitWork(WAITTIME);
            at->sendMac("00:00:00:00:00:00");   // 发送mac地址
            waitWork(50);
            on_disconnectButton_clicked();
            showlog("测试结束");
            is_motor_test_continue = false;
            test_state = STATE_IDLE;
            break;
        }
    }
}

void motor::refreshMesState(int state)
{
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}
void motor::getTestValue(const int mechines, const QString value)
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
            // on_macInput_returnPressed();
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

    // banding_mac_sn(mesmacAddress, ui->get_mac->text());//获取测试数据不要绑定测试mac——sn
}

void motor::on_damping_open_clicked()
{
    pb->set_motor_damping_state(1);
}

void motor::on_damping_close_clicked()
{
    pb->set_motor_damping_state(0);
}

void motor::on_get_mac_returnPressed()
{
    ui->log->clear();
    ui->msgEdit->clear();
    ui->get_mac->setDisabled(1);
    ui->macInput->setDisabled(1);
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);

    // 使用正则表达式匹配
    if (!snRegex.match(ui->get_mac->text()).hasMatch())
    {
        ui->get_mac->setDisabled(0);
        ui->macInput->setDisabled(0);
        showlog("序列号错误");
        ui->get_mac->clear();
        return;
    }

    if (pack.product == "U7" || pack.product == "U7P")
        ui->nfc_sn->setFocus();

    showlog("正在查询mac地址");
    sn = ui->get_mac->text().toUtf8();
    get_mac(ui->get_mac->text());   // 文件获取
    processInspection(ui->get_mac->text());
    processGetMesTestValue();   // mes获取
}
void motor::processGetMesTestValue()
{
    if (ui->isformmes->checkState())
    {
        pack.sn = ui->get_mac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";

        emit getMesTestValue(pack);
    }
}
void motor::on_macInput_returnPressed()
{
    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    waitWork(WAITTIME);
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
        // qDebug() << getIndex()<< macAddress;

        if (ui->ismotortest->isChecked())
        {
            is_motor_test_continue = true;
            is_motor_continue = false;
        }
        else
        {
            is_motor_test_continue = false;
            is_motor_continue = true;
        }

        emit goNextFocus();

        state = STATE_IDLE;
    }
}
void motor::get_mac(QString sn_to_search)
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

// E2:33:07:34:8E:F5

void motor::on_end_cali_clicked()
{
    sn = "";
    stringsn = "";
    ui->macInput->clear();
    ui->get_mac->clear();
    ui->get_mac->setFocus();
    showlog("测试结束");
    is_motor_continue = false;
    state = STATE_IDLE;
}

void motor::on_stopTest_clicked()
{
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    waitWork(100);
    ui->macInput->setDisabled(0);
    ui->get_mac->setDisabled(0);

    ui->macInput->clear();
    ui->get_mac->clear();
    ui->get_mac->setFocus();
    on_disconnectButton_clicked();
}
