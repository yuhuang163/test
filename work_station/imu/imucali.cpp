#include "imucali.h"

#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_imucali.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
void imucali::on_pushButton_clicked() {
    // emit send_end_test(getIndex());

    // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    ui->macInput->setText("3C:84:27:07:A8:D2");
    // ui->macInput->setText("74:4D:BD:95:7F:36");
    // ui->macInput->setText("e2:66:07:34:2d:f7");
    // ui->macInput->setText("74:4D:BD:95:80:7e");
    // ui->macInput->setText("b4:56:5d:bf:57:9d");//E4:08:09:30:7E:FB
    // ui->macInput->setText("b4:56:5d:bf:54:4e");  // wd设备
    // ui->macInput->setText("ca:5b:09:30:ca:fb");
    // ui->macInput->setText("b4:56:5d:bf:57:9d");
    on_macInput_returnPressed();
}
imucali::imucali(int index, QWidget* parent) :
    test_base(parent), ui(new Ui::imucali), qimuc(new imu_calibrate), nqimuc(new new_imu_calibrate) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = IMU_VER;
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");

    scanSerialPorts();  // 要搜索一下一开始

    connect(nqimuc, SIGNAL(send_imu_cali_position(int)), this, SLOT(refresh_imu_cali_position(int)));
    connect(nqimuc, SIGNAL(send_imu_cali_msg(QString)), this, SLOT(refreshImuCaliMsg(QString)));

    connect(qimuc, SIGNAL(send_imu_cali_msg(QString)), this, SLOT(refreshImuCaliMsg(QString)));
    connect(qimuc, SIGNAL(send_imu_cali_reslt_msg(QString)), this, SLOT(refresh_imu_cali_reslt_msg(QString)));
    connect(qimuc, SIGNAL(send_imu_data_to_csv(QString, QString)), this,
            SLOT(refresh_imu_data_to_csv(QString, QString)));

    labelList.append(ui->p1);
    labelList.append(ui->p2);
    labelList.append(ui->p3);
    labelList.append(ui->p4);
    labelList.append(ui->p5);
    labelList.append(ui->p6);
    labelList.append(ui->p7);
    labelList.append(ui->p8);
    labelList.append(ui->p9);
    labelList.append(ui->p10);
    labelList.append(ui->p11);
    labelList.append(ui->p12);

    ui->tabWidget->setTabText(0, "IMU校准");

    for (int la = 0; la < 12; la++) {
        QLabel* label = labelList.at(la);
        label->setStyleSheet(
            " background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    }
    ui->mechine_number->setText(QString::number(getIndex()) + "号机");
    ui->mechine_number->setStyleSheet("font-size: 20px; background-color: yellow; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 20px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 20px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
    });

    connect(comparewaittime, &QTimer::timeout, [=]() {
        iscompareovertime = 1;
        comparewaittime->stop();
    });
    // 到位就停止定时器，位置刷新就开始定时器，陪跑策略，并且fail后死亡这个机
    connect(caliwaittime, &QTimer::timeout, [=]() {
        showlog("六轴校准小位置超时当前超时时间为" + QString::number(imu_cali_wait_time));
        qDebug() << getIndex() << "正在跳过这个小位置";
        refresh_imu_cali_position(-1);
        is_out_counts++;
        if (is_out_counts == 2) {
            showlog("跳过位置达到上限2个");
            isovertime = 1;
            emit send_kill_test(getIndex());
        }
    });

    imu_wait_time = SETTINGS.value("IMU/IMU_Wait_Time", "15000").toInt();
    ImuCompareData = SETTINGS.value("IMU/ImuCompareData", "-8000").toInt();
    imu_cali_wait_time = SETTINGS.value("IMU/imu_cali_wait_time", "15000").toInt();
    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();

    showlog("standbattary=" + QString::number(standbattary));

    showlog("model=" + pack.model);
    showlog("test_station=" + pack.test_station);
    showlog("action=" + pack.action);
    showlog("line=" + pack.line);

    showlog("machineNo=" + pack.machineNo);
    showlog("IMU_Wait_Time=" + QString::number(imu_wait_time));
    showlog("ImuCompareData=" + QString::number(ImuCompareData));
    showlog("imu_cali_wait_time=" + QString::number(imu_cali_wait_time));

    QFont font("Arial", 10);
    ui->gyro_x->setFont(font);
    ui->gyro_y->setFont(font);
    ui->gyro_z->setFont(font);
    ui->acc_x->setFont(font);
    ui->acc_y->setFont(font);
    ui->acc_z->setFont(font);

    testResultTableInit();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}
void imucali::refreshImuCaliResult(ProtocolImuCalibResultData x) {
    showlog("以下为收到设备的imu校准数据");
    showlog(QString("gyro_x: %1").arg(x.gyroX));
    showlog(QString("gyro_y: %1").arg(x.gyroY));
    showlog(QString("gyro_z: %1").arg(x.gyroZ));
    showlog(QString("bx: %1").arg(x.bx));
    showlog(QString("by: %1").arg(x.by));
    showlog(QString("bz: %1").arg(x.bz));
    showlog(QString("kx: %1").arg(x.kx));
    showlog(QString("ky: %1").arg(x.ky));
    showlog(QString("kz: %1").arg(x.kz));
    showlog(QString("syx: %1").arg(x.syx));
    showlog(QString("szx: %1").arg(x.szx));
    showlog(QString("szy: %1").arg(x.szy));
    showlog(QString("imu校准结果: %1").arg(x.result));

    const bool isP20POrQ20 = (QString(product_name).compare("P20P") == 0 || QString(product_name).compare("Q20") == 0);
    bool isMatch = false;
    if (isP20POrQ20) {
        isMatch = (nqimuc->calData.kx == x.kx && nqimuc->calData.ky == x.ky && nqimuc->calData.kz == x.kz &&
                   nqimuc->calData.syx == x.syx && nqimuc->calData.szx == x.szx && nqimuc->calData.szy == x.szy &&
                   nqimuc->calData.bx == x.bx && nqimuc->calData.by == x.by && nqimuc->calData.bz == x.bz);
    } else {
        isMatch = (nqimuc->calData.gyro_offset[0] == x.gyroX && nqimuc->calData.gyro_offset[1] == x.gyroY &&
                   nqimuc->calData.gyro_offset[2] == x.gyroZ && nqimuc->calData.kx == x.kx &&
                   nqimuc->calData.ky == x.ky && nqimuc->calData.kz == x.kz && nqimuc->calData.syx == x.syx &&
                   nqimuc->calData.szx == x.szx && nqimuc->calData.szy == x.szy && nqimuc->calData.bx == x.bx &&
                   nqimuc->calData.by == x.by && nqimuc->calData.bz == x.bz);
    }

    if (isMatch) {
        imudata_check = 1;
        return;
    }

    imudata_check = 2;
    if (nqimuc->calData.gyro_offset[0] != x.gyroX)
        qDebug() << getIndex() << "gyro_offset[0] 不匹配：" << nqimuc->calData.gyro_offset[0] << " != " << x.gyroX;
    if (nqimuc->calData.gyro_offset[1] != x.gyroY)
        qDebug() << getIndex() << "gyro_offset[1] 不匹配：" << nqimuc->calData.gyro_offset[1] << " != " << x.gyroY;
    if (nqimuc->calData.gyro_offset[2] != x.gyroZ)
        qDebug() << getIndex() << "gyro_offset[2] 不匹配：" << nqimuc->calData.gyro_offset[2] << " != " << x.gyroZ;
    if (nqimuc->calData.kx != x.kx)
        qDebug() << getIndex() << "kx 不匹配：" << nqimuc->calData.kx << " != " << x.kx;
    if (nqimuc->calData.ky != x.ky)
        qDebug() << getIndex() << "ky 不匹配：" << nqimuc->calData.ky << " != " << x.ky;
    if (nqimuc->calData.kz != x.kz)
        qDebug() << getIndex() << "kz 不匹配：" << nqimuc->calData.kz << " != " << x.kz;
    if (nqimuc->calData.syx != x.syx)
        qDebug() << getIndex() << "syx 不匹配：" << nqimuc->calData.syx << " != " << x.syx;
    if (nqimuc->calData.szx != x.szx)
        qDebug() << getIndex() << "szx 不匹配：" << nqimuc->calData.szx << " != " << x.szx;
    if (nqimuc->calData.szy != x.szy)
        qDebug() << getIndex() << "szy 不匹配：" << nqimuc->calData.szy << " != " << x.szy;
    if (nqimuc->calData.bx != x.bx)
        qDebug() << getIndex() << "bx 不匹配：" << nqimuc->calData.bx << " != " << x.bx;
    if (nqimuc->calData.by != x.by)
        qDebug() << getIndex() << "by 不匹配：" << nqimuc->calData.by << " != " << x.by;
    if (nqimuc->calData.bz != x.bz)
        qDebug() << getIndex() << "bz 不匹配：" << nqimuc->calData.bz << " != " << x.bz;
}

void imucali::getTestValue(const int mechines, const QString value) {
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
            // on_macInput_returnPressed();
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

imucali::~imucali() { delete ui; }

void imucali::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
    // showlog("蓝牙连接断开");
}
void imucali::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        showlog("蓝牙连接成功");

    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        showlog("蓝牙连接断开");
        // if (isTestContinue == true)
        //     on_macInput_returnPressed();
    }
}

void imucali::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void imucali::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void imucali::refreshSn(ProtocolSnData data) {
    stringsn = data.value;
    qDebug() << getIndex() << "dev_info" << data.value;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("芯片存储的尾盖sn:" + stringsn);
}

void imucali::set_fix_result(int state) {
    if (state) {
        is_fix_set_ok = 1;
    } else {
        showlog("治具角度错误");
        is_fix_set_ok = 0;
    }
}
void imucali::get_fix_action(int state) {
    if (state && is_start_ium_cali) {
        caliwaittime->stop();
        caliwaittime->start(imu_cali_wait_time);
        showlog("开始小位置超时定时器");
    }
}

void imucali::print_fixture_log(QString data) { showlog(data); }

void imucali::refresh_imu_cali_reslt_msg(QString msg) { showlog(msg); }
void imucali::refreshImuCaliMsg(QString msg) {
    showlog(msg);
    // qDebug() << getIndex()<< "收到数据: " << getIndex();

    // 构建 "测试结果" 文件夹的完整路径，这里选择保存到D盘
    QString folderPath = "D:/测试结果";

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists()) {
        QDir().mkpath(folderPath);
    }

    // 获取当前日期
    QDate currentDate = QDate::currentDate();

    // 构建年、月、日的文件夹结构
    QString yearFolder = QString::number(currentDate.year());
    QString monthFolder = currentDate.toString("MM");
    QString dayFolder = currentDate.toString("dd");

    // 构建完整的文件路径，加上日期
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_六轴测试log.csv";
    QString filePath = QDir(folderPath).filePath(fileName);

    QFile file(filePath);

    if (!file.exists()) {
        // 文件不存在，打开文件并写入表头
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            // 获取当前时间戳
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 写入表头
            QStringList headers;
            headers << "sn"
                    << "上位机版本"
                    << "mac地址"
                    << "时间戳"
                    << "测试日志";
            stream << headers.join(",") << "\n";
            file.close();
        } else {
            qDebug() << getIndex() << "Error creating file";
            return;
        }
    }

    // 文件已存在，以追加模式打开文件并写入数据
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);
        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        // 写入数据
        QStringList rowData;
        rowData << stringsn << upperComputerVer << macAddress << timestamp << msg;
        stream << rowData.join(",") << "\n";

        file.close();
        // qDebug() << getIndex()<< "Data appended to" << filePath;
    } else {
        qDebug() << getIndex() << "Error appending to file";
    }
}

void imucali::refresh_imu_cali_position(int position) {
    qDebug() << getIndex() << "收到的position为" << position;

    if (position != -1) {
        showlog("位置：" + QString::number(position + 1) + "已经标定好了");
    }

    int index = position + 1;
    if (index >= 0 && index <= 12) {
        caliwaittime->stop();
        showlog("停止小位置超时定时器");
        if (index != 0) {
            QLabel* label = labelList.at(index - 1);
            label->setStyleSheet(  // border: 2px solid #00FF00;
                " background-color: #00FF00; color: black;  border-radius: 10px; padding: 10px; text-align: center;");
        }
        if (index == 0) {
            index = upPositionIndex + 1;
        }
        upPositionIndex = index;

        if (index == 1 || index == 2 || index == 3) {
            count123++;
            if (index == 1)
                emit fixture_left(getIndex());
            if (index == 2)
                emit fixture_down(getIndex());
            if (index == 3)
                emit fixture_up(getIndex());

            if (count123 >= 3) {
                qDebug() << getIndex() << "发射stage1_ok";
                showlog("六轴校准水平阶段完成");
                emit stage1_ok(getIndex());
                count123 = 0;
            }
        } else if (index == 4 || index == 5 || index == 6 || index == 7) {
            if (index == 4)
                emit fixture_left(getIndex());
            if (index == 5)
                emit fixture_down(getIndex());
            if (index == 6)
                emit fixture_right(getIndex());
            // if (index == 7)
            //     emit fixture_up(getIndex());

            count4567++;

            if (count4567 >= 4) {
                qDebug() << getIndex() << "发射stage2_ok";
                showlog("六轴校准40度阶段完成");
                emit stage2_ok(getIndex());
                count4567 = 0;
            }
        } else if (index == 8 || index == 9 || index == 10 || index == 11) {
            if (index == 11)
                emit fixture_down(getIndex());
            if (index == 10)
                emit fixture_left(getIndex());
            if (index == 9)
                emit fixture_up(getIndex());
            // if (index == 8)
            //     emit fixture_down(getIndex());

            count89++;
            showlog("我需要的数值" + QString::number(nqimuc->calib_datasets->size) + QString::number(FACTORY_POSE_NUM));
            if (count89 >= 3 && (nqimuc->calib_datasets->size >= FACTORY_POSE_NUM) &&
                (nqimuc->extra_calib_datasets->size >= EXTRA_POSE_NUM)) {
                qDebug() << getIndex() << "发射stage3_ok";
                showlog("六轴校准反着40度阶段完成");
                emit stage3_ok(getIndex());
                count89 = 0;
            }
        }
    }
}

void imucali::refresh_imu_data_to_csv(QString imutime, QString msg) {
    // showlog(msg);
    // qDebug() << getIndex()<< "收到数据: " << getIndex();
    // 构建 "测试结果" 文件夹的完整路径，这里选择保存到D盘
    QString folderPath = "D:/测试结果/六轴数据";

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists()) {
        QDir().mkpath(folderPath);
    }

    // 获取当前日期
    QDate currentDate = QDate::currentDate();

    // 构建年、月、日的文件夹结构
    QString yearFolder = QString::number(currentDate.year());
    QString monthFolder = currentDate.toString("MM");
    QString dayFolder = currentDate.toString("dd");

    // 构建完整的文件路径，加上日期
    QString fileName = QString::number(getIndex()) + ui->getMac->text() + ".csv";
    QString filePath = QDir(folderPath).filePath(fileName);

    QFile file(filePath);

    if (!file.exists()) {
        // 文件不存在，打开文件并写入表头
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            // 获取当前时间戳
            QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            // 写入表头
            QStringList headers;

            headers << "mac地址"
                    << "时间戳"
                    << "gyro[0]"
                    << "gyro[1]"
                    << "gyro[2]"
                    << "acc[0]"
                    << "acc[1]"
                    << "acc[2]"

                    << "normol"
                    << "acc_dif[0]"
                    << "acc_dif[1]"
                    << "acc_dif[2]"
                    << "gyro_dif[0]"
                    << "gyro_dif[1]"
                    << "gyro_dif[2]"
                    << "acc_var[0]"
                    << "acc_var[1]"
                    << "acc_var[2]"
                    << "gyro_var[0]"
                    << "gyro_var[1]"
                    << "gyro_var[2]"
                    << "gyrobias1"
                    << "gyrobias2"
                    << "gyrobias3";
            stream << headers.join(",") << "\n";
            file.close();
        } else {
            qDebug() << getIndex() << "Error creating file";
            return;
        }
    }

    // 文件已存在，以追加模式打开文件并写入数据
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);
        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        // 写入数据
        QStringList rowData;
        rowData << macAddress << imutime << msg;
        stream << rowData.join(",") << "\n";

        file.close();
        // qDebug() << getIndex()<< "Data appended to" << filePath;
    } else {
        qDebug() << getIndex() << "Error appending to file";
    }
}

void imucali::getimuData(ProtocolImuSampleData x) {
    int ret = 0;
    int old_ret = 0;
    int32_t imudata_result = 0;
    ui->log->appendPlainText("收到六轴数据");
    // qDebug() << getIndex()<< "收到数据: " << getIndex();
    if (is_start_ium_cali) {
        //  showlog("收到的个数=" + QString::number(x.data_count));
        const int sampleCount = qMin(x.accelValues.size(), x.gyroValues.size()) / 3;
        for (int i = 0; i < sampleCount; i++) {
            const int base = i * 3;
            orgData.acc[0] = x.accelValues[base];
            orgData.acc[1] = x.accelValues[base + 1];
            orgData.acc[2] = x.accelValues[base + 2];
            orgData.gyro[0] = x.gyroValues[base];
            orgData.gyro[1] = x.gyroValues[base + 1];
            orgData.gyro[2] = x.gyroValues[base + 2];

            refresh_imu_data_to_csv(QString::number(x.timeStamp),
                                    QString::number(orgData.gyro[0]) + "," + QString::number(orgData.gyro[1]) + "," +
                                        QString::number(orgData.gyro[2]) + "," + QString::number(orgData.acc[0]) + "," +
                                        QString::number(orgData.acc[1]) + "," + QString::number(orgData.acc[2]));

            //  showlog("时间为=" + QString::number(x.data[i].timestamp));
            nqimuc->imu_time = QString::number(x.timeStamp);
            qimuc->imu_time = QString::number(x.timeStamp);

            if (orgData.gyro[0] & 0x8000) {  // 判断最高位是否为 1，表示负数测试一下
                imudata_result = static_cast<int32_t>(orgData.gyro[0] | 0xFFFF0000);  // 扩展为 32 位的负数
            } else {
                imudata_result = static_cast<int32_t>(orgData.gyro[0]);  // 保持正数不变
            }

            // qDebug()<< "Converted back result: " << imudata_result;
            if (imudata_result == -32768) {
                showlog("数据异常正在重新唤醒");
                waitWork(WAITTIME);
                protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_STOP));
                waitWork(WAITTIME);
                protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
                waitWork(1000);
                break;
            }

            ui->gyro_x->setText("gyro_x=" + QString::number(orgData.gyro[0]));
            ui->gyro_y->setText("gyro_y=" + QString::number(orgData.gyro[1]));
            ui->gyro_z->setText("gyro_z=" + QString::number(orgData.gyro[2]));
            ui->acc_x->setText("acc_x=" + QString::number(orgData.acc[0]));
            ui->acc_y->setText("acc_y=" + QString::number(orgData.acc[1]));
            ui->acc_z->setText("acc_z=" + QString::number(orgData.acc[2]));

            // showlog("gyro_x="+QString::number(orgData.gyro[0]));
            // showlog("gyro_y="+QString::number(orgData.gyro[1]));
            // showlog("gyro_z="+QString::number(orgData.gyro[2]));
            // showlog("acc_x="+QString::number(orgData.acc[0]));
            // showlog("acc_y="+QString::number(orgData.acc[1]));
            // showlog("acc_z="+QString::number(orgData.acc[2]));

            if (isNeedNewCali) {
                nqimuc->sensorhub_timer_callback(&orgData);
                ret = nqimuc->acccalib_sensors_task();
            } else {
                nqimuc->sensorhub_timer_callback(&orgData);

                if (is_new_cali)
                    ret = nqimuc->acccalib_sensors_task();
                if (ret == 0)
                    is_new_cali = 0;

                if (is_old_cali)
                    old_ret = qimuc->imu_calib_proc(&orgData);

                if (old_ret == 0)
                    is_old_cali = 0;
            }

            if (ret == 1)  // 标定失败
            {
                caliwaittime->stop();
                showlog("停止小位置超时定时器");
                showlog("加速度校准失败");
                is_start_ium_cali = 0;
                result = failValue;
                state = STATE_SAVE_RESULT;

                showlog("校准值kx=" + QString::number(nqimuc->calData.kx));
                showlog("校准值ky=" + QString::number(nqimuc->calData.ky));
                showlog("校准值kz=" + QString::number(nqimuc->calData.kz));
                showlog("校准值syx=" + QString::number(nqimuc->calData.syx));
                showlog("校准值szx=" + QString::number(nqimuc->calData.szx));
                showlog("校准值szy=" + QString::number(nqimuc->calData.szy));
                showlog("校准值bx=" + QString::number(nqimuc->calData.bx));
                showlog("校准值by=" + QString::number(nqimuc->calData.by));
                showlog("校准值bz=" + QString::number(nqimuc->calData.bz));

                showlog("校准值x=" + QString::number(qimuc->calData.gyro_offset[0]));
                showlog("校准值y=" + QString::number(qimuc->calData.gyro_offset[1]));
                showlog("校准值z=" + QString::number(qimuc->calData.gyro_offset[2]));

                break;
            }

            // waitWork(10);
            // showlog("imu校准返回值："+QString::number(ret));

            // qDebug() << getIndex()<< "imu校准结果" << ret;

            if (ret == 0) {
                caliwaittime->stop();
                showlog("停止小位置定时器");
            }
            if (ret == 0 && old_ret == 0) {
                isimuCaliOk = 1;
                is_start_ium_cali = 0;
                break;
            }
        }
    }

    if (is_start_ium_test) {
        const int sampleCount = qMin(x.accelValues.size(), x.gyroValues.size()) / 3;
        for (int i = 0; i < sampleCount; i++) {
            const int base = i * 3;
            orgData.acc[0] = x.accelValues[base];
            orgData.acc[1] = x.accelValues[base + 1];
            orgData.acc[2] = x.accelValues[base + 2];
            orgData.gyro[0] = x.gyroValues[base];
            orgData.gyro[1] = x.gyroValues[base + 1];
            orgData.gyro[2] = x.gyroValues[base + 2];

            ui->gyro_x->setText("gyro_x=" + QString::number(orgData.gyro[0]));
            ui->gyro_y->setText("gyro_y=" + QString::number(orgData.gyro[1]));
            ui->gyro_z->setText("gyro_z=" + QString::number(orgData.gyro[2]));
            ui->acc_x->setText("acc_x=" + QString::number(orgData.acc[0]));
            ui->acc_y->setText("acc_y=" + QString::number(orgData.acc[1]));
            ui->acc_z->setText("acc_z=" + QString::number(orgData.acc[2]));

            int32_t result = 0;

            if (orgData.acc[2] & 0x8000) {  // 判断最高位是否为 1，表示负数
                result = static_cast<int32_t>(orgData.acc[2] | 0xFFFF0000);  // 扩展为 32 位的负数
            } else {
                result = static_cast<int32_t>(orgData.acc[2]);  // 保持正数不变
            }

            qDebug() << getIndex() << "Converted back result: " << result;
            if (result < ImuCompareData) {
                showlog("六轴测试通过" + QString::number(orgData.acc[2]));
                is_imu_test_ok = 1;
                is_start_ium_test = 0;
                break;
            }
        }
    }
}
void imucali::refreshBaseData(ProtocolBaseInfoData data) {
    showlog("设备名字为" + QString(data.product_name));

    product_name = QString(data.product_name);

    if (QString(data.product_name).compare("P20PS") == 0 || QString(data.product_name).compare("P20P") == 0 ||
        QString(data.product_name).compare("Q20") == 0) {
        if (data.imu_id == 144) {  //无陀螺仪
            nqimuc->LSB = 1;
            isNeedNewCali = 1;

        } else if (data.imu_id == 250) {  //博士的imu无陀螺仪
            nqimuc->LSB = 0.25;
            isNeedNewCali = 1;
        }

    } else {
        nqimuc->LSB = 4;
        isNeedNewCali = 1;
        if (QString(data.product_name).compare("U7") == 0 || QString(data.product_name).compare("U7P") == 0 ||
            QString(data.product_name).compare("Y25SE") == 0) {
            if (QString(data.product_name).compare("Y25SE") == 0) {
                isNeedNewCali = 0;
                showlog("当前姿态为Y25SE");
            }
            nqimuc->imu_static_state = 0;
            showlog("当前姿态为q20");
        } else {
            nqimuc->imu_static_state = 1;
            showlog("当前姿态为y20");
        }
    }
    showlog("LSB改为" + QString::number(nqimuc->LSB));
}

void imucali::processInspection(QString stringsn) {
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
        ui->mes_state->setStyleSheet("font-size: 20px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}
void imucali::refreshBattaryData(ProtocolBatteryData adc) {
    QString chargeStateStr;
    switch (adc.chargeState) {
        case 1: chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>"; break;
        case 2: chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>"; break;
        case 3: chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>"; break;
        case 4: chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>"; break;
        default: chargeStateStr = "充电状态为：<span style='color:red'>未知</span>"; break;
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

    if (adc.voltageMv / 1000.0 > standbattary) {
        is_battary_test = 1;  // 正常
    }
    if (adc.voltageMv / 1000.0 <= standbattary)
        is_battary_test = 2;  // 低电量
}
void imucali::dataInit() {
    upPositionIndex = 0;  // 上一个位置的索引
    ui->test_time->display(0);
    information = "";

    isovertime = 0;
    is_old_cali = 1;
    is_battary_test = 0;
    is_new_cali = 1;
    is_out_counts = 0;
    is_imu_test_ok = 0;
    is_fix_set_ok = 0;  // 是否治具设置ok
    iscompareovertime = 0;
    isimuCaliOk = 0;  // 是否校准完成
    is_start_ium_test = 0;
    dongleOutTime = 50;  // 调小会奔溃
    is_start_ium_cali = 0;
    isStartSendCaliResult = 0;  // 是否开始发送校验结果
    imudata_check = 0;
    count123 = 0;
    count4567 = 0;
    count89 = 0;
    nqimuc->calData.gyro_offset[0] = 0;
    nqimuc->calData.gyro_offset[1] = 0;
    nqimuc->calData.gyro_offset[2] = 0;
    nqimuc->calData.kx = 0.0f;
    nqimuc->calData.ky = 0.0f;
    nqimuc->calData.kz = 0.0f;
    nqimuc->calData.syx = 0.0f;
    nqimuc->calData.szx = 0.0f;
    nqimuc->calData.szy = 0.0f;
    nqimuc->calData.bx = 0.0f;
    nqimuc->calData.by = 0.0f;
    nqimuc->calData.bz = 0.0f;

    pb->reset_all_pb();
    at->resetConnected();
    //  at->sendMac(ui->macInput->text());//发送mac地址
    result = passValue;

    ui->tail_sn->setText("芯片存储的尾盖sn:");

    for (int la = 0; la < 12; la++) {
        QLabel* label = labelList.at(la);
        label->setStyleSheet(
            " background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    }
    stringsn = "";
    ui->macLabel->setText("蓝牙mac: " + macAddress);

    ui->gyro_x->setText("gyro_x=");
    ui->gyro_y->setText("gyro_y=");
    ui->gyro_z->setText("gyro_z=");
    ui->acc_x->setText("acc_x=");
    ui->acc_y->setText("acc_y=");
    ui->acc_z->setText("acc_z=");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 20px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
}

void imucali::useMes() {
    turn = !turn;
    if (turn) {
        ui->isusemes->setCheckState(Qt::Checked);
    } else {
        ui->isusemes->setCheckState(Qt::Unchecked);
    }
}
void imucali::endTask() {
    dataInit();
    on_stopTest_clicked();
    caliwaittime->stop();
    waittime->stop();
    comparewaittime->stop();
    on_disconnectButton_clicked();
    emit send_end_test(getIndex());
}
void imucali::startTest() { on_macInput_returnPressed(); };

void imucali::startTask()  // 编写六轴校准的代码
{
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
            case STATE_IDLE:  // 复位一切
                showlog("开始测试");
                dataInit();
                waitWork(1000);
                at->sendMac(ui->macInput->text());  // 开始连接
                showlog("MAC地址为：" + ui->macInput->text());

                TestTime.start();
                state = STATE_WATI_CONNECT;
                break;

            case STATE_WATI_CONNECT:  // 设置禁止休眠
                if (at->getConnected()) {
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN)); });
                    state = STATE_DISABLE_SLEEP_1;
                }
                break;

            case STATE_DISABLE_SLEEP_1:  // 设置设备采集
                if (canGoNext) {
                    showlog("已进入禁止休眠");

                    if (SETTINGS.value("SYSTEM/DisableSerialPortRx").toBool()) {
                        sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 0); });
                        state = STATE_CLOSE_UART;
                    } else {
                        sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::BaseInfo); });
                        state = STATE_GETBASEDATA;
                    }
                }
                break;

            case STATE_CLOSE_UART:

                if (canGoNext) {
                    showlog("已经关闭串口接收功能");
                    TestItem test;
                    test.testItem = "串口接收";
                    test.testData = "关闭";
                    test.testResult = "通过";
                    test.ask = "关闭";
                    testItems.append(test);
                    testResultTableUpdate(testItems);
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::BaseInfo); });
                    state = STATE_GETBASEDATA;
                }
                break;

            case STATE_GETBASEDATA:
                if (canGoNext) {
                    showlog("开始获取出厂电压");
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::Battery); });
                    state = STATE_GETBATTERY;
                }
                break;

            case STATE_GETBATTERY:
                //等于0就卡着
                if (is_battary_test != 0) {
                    if (is_battary_test == 1) {
                        showlog("出厂电压正常");
                        TestItem test;
                        test.testItem = "出厂电压";
                        test.testData = QString::number(voltage);
                        test.testResult = "通过";
                        test.ask = QString::number(standbattary);
                        testItems.append(test);

                        testResultTableUpdate(testItems);
                    }

                    if (is_battary_test == 2) {
                        pack.error = "SP03020";
                        showlog("出厂电压异常为" + QString::number(voltage));
                        result = failValue;
                        TestItem test;
                        test.testItem = "出厂电压";
                        test.testData = QString::number(voltage);
                        test.testResult = "失败";
                        test.ask = QString::number(standbattary);
                        testItems.append(test);

                        testResultTableUpdate(testItems);
                    }
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ImuCollect, static_cast<int>(FacSwitch_START)); });

                    state = STATE_CAIL;
                }
                break;

            case STATE_CAIL:  // 开始校准
                if (canGoNext) {
                    showlog("已发送imu采集参数");

                    qimuc->imu_calib_init();
                    nqimuc->acccalib_sensors_init();

                    // emit fixture_up(getIndex());

                    is_start_ium_cali = 1;
                    qDebug() << getIndex() << "开始校准";
                    showlog("等待校准");
                    waittime->start(imu_wait_time);
                    caliwaittime->start(imu_cali_wait_time);
                    state = STATE_SEND_CAIL_RESULT;
                }
                break;

            case STATE_SEND_CAIL_RESULT:  // 开始发送校准值
                if (isimuCaliOk) {
                    waittime->stop();
                    showlog("IMU校准成功");
                    nqimuc->calData.gyro_offset[0] = qimuc->calData.gyro_offset[0];
                    nqimuc->calData.gyro_offset[1] = qimuc->calData.gyro_offset[1];
                    nqimuc->calData.gyro_offset[2] = qimuc->calData.gyro_offset[2];
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ImuCollect, static_cast<int>(FacSwitch_STOP)); });
                    state = STATE_END_CALI_DATA;
                    break;
                }
                if (isovertime) {
                    caliwaittime->stop();
                    is_start_ium_cali = 0;
                    refreshImuCaliMsg("六轴校准失败");
                    isovertime = 0;
                    showlog("六轴校准失败：超时");
                    waitWork(WAITTIME);
                    protocolManager.set(DeviceCmd::ImuCollect, static_cast<int>(FacSwitch_STOP));
                    waitWork(WAITTIME);
                    state = STATE_SAVE_RESULT;
                    result = failValue;
                }

                break;

            case STATE_END_CALI_DATA:

                if (canGoNext) {
                    showlog("开始设置六轴校准结果");
                    sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::NewImuCaliResult, QVariant::fromValue(nqimuc->calData)); });
                    state = STATE_SENDOK;
                }
                break;

            case STATE_SENDOK:

                if (canGoNext) {
                    showlog("已获取到imu校准结果发送回应");
                    sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetImuCaliResult); });
                    showlog("开始获取imu校准结果");
                    state = STATE_CHECKOK;
                }
                break;

            case STATE_CHECKOK:

                if (canGoNext) {
                    if (imudata_check == 1) {
                        state = STATE_SAVE_RESULT;
                    } else if (imudata_check == 2) {
                        result = failValue;
                        showlog("获取的设备校准值错误");
                        state = STATE_SAVE_RESULT;
                    }
                }

                break;

            case STATE_SAVE_RESULT:  //保存结果用的
                if (result == failValue) {
                    TestItem test;
                    test.testItem = "IMU校准测试";
                    test.testData = qimuc->imureason + nqimuc->imureason;
                    test.testResult = "失败";
                    test.ask = "通过";
                    testItems.append(test);

                    testResultTableUpdate(testItems);
                    pack.error = "SP03021";
                    showlog("六轴校准结束");
                    emit endcali(getIndex());
                    state = STATE_END;
                    break;
                }
                if (result == passValue) {
                    TestItem test;
                    // 使用 QString 连接所有内容
                    QString data;
                    data.append("校准值kx=" + QString::number(nqimuc->calData.kx) + "\n");
                    data.append("校准值ky=" + QString::number(nqimuc->calData.ky) + "\n");
                    data.append("校准值kz=" + QString::number(nqimuc->calData.kz) + "\n");
                    data.append("校准值syx=" + QString::number(nqimuc->calData.syx) + "\n");
                    data.append("校准值szx=" + QString::number(nqimuc->calData.szx) + "\n");
                    data.append("校准值szy=" + QString::number(nqimuc->calData.szy) + "\n");
                    data.append("校准值bx=" + QString::number(nqimuc->calData.bx) + "\n");
                    data.append("校准值by=" + QString::number(nqimuc->calData.by) + "\n");
                    data.append("校准值bz=" + QString::number(nqimuc->calData.bz) + "\n");
                    data.append("校准值x=" + QString::number(qimuc->calData.gyro_offset[0]) + "\n");
                    data.append("校准值y=" + QString::number(qimuc->calData.gyro_offset[1]) + "\n");
                    data.append("校准值z=" + QString::number(qimuc->calData.gyro_offset[2]) + "\n");

                    // 将连接好的字符串赋值给 test.testData
                    test.testItem = "IMU校准测试";
                    test.testData = data;
                    test.testResult = "通过";
                    test.ask = "通过";
                    testItems.append(test);

                    testResultTableUpdate(testItems);

                    showlog(data);

                    showlog("六轴校准结束");
                    emit endcali(getIndex());

                    if ((ui->isusemes->checkState() || pack.factory == "无mes厂") && result == passValue) {
                        protocolManager.set(DeviceCmd::FacMode, 0);
                        waitWork(100);
                        sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ShipMode, 1); });
                        showlog("已发送进入船运模式");
                        qDebug() << ui->getMac->text() << macAddress << getIndex() << "已发送进入船运模式";
                        state = STATE_SHIP_MODE_CHECK;

                    } else {
                        state = STATE_END;
                    }
                }

                break;

            case STATE_SHIP_MODE_CHECK:
                if (SETTINGS.value("SYSTEM/ShipModeResponse").toBool()) {
                    if (!at->getConnected() && canGoNext) {
                        showlog("检测到蓝牙已经断开且收到设备回应收到船运退出指令");
                        showlog("说明已经成功进入船运模式");
                        TestItem test;
                        test.testItem = "船运测试";
                        test.testData = "收到回应";
                        test.testResult = "通过";
                        test.ask = "通过";
                        testItems.append(test);
                        testResultTableUpdate(testItems);
                        state = STATE_END;
                    }

                } else {
                    if (!at->getConnected()) {
                        showlog("检测到蓝牙已经断开");
                        showlog("说明已经成功进入船运模式");
                        TestItem test;
                        test.testItem = "船运测试";
                        test.testData = "收到回应";
                        test.testResult = "通过";
                        test.ask = "通过";
                        testItems.append(test);
                        testResultTableUpdate(testItems);
                        state = STATE_END;
                    }
                }
                if (sendRetryOver) {
                    TestItem test;
                    test.testItem = "船运测试";
                    test.testData = "等待超时";
                    test.testResult = "失败";
                    test.ask = "通过";
                    pack.error = "SP03022";
                    testItems.append(test);
                    testResultTableUpdate(testItems);

                    result = failValue;
                    state = STATE_END;
                }

                break;

            case STATE_END:
                if (result == passValue) {
                    QString mesresult = "PASS";
                    QString itemvalue;

                    itemvalue =
                        QString("|outvoltage:%1").arg(voltage) +
                        QString("|gyro_offsetx:%1").arg(nqimuc->calData.gyro_offset[0]) +
                        QString("|gyro_offsety:%1").arg(nqimuc->calData.gyro_offset[1]) +
                        QString("|gyro_offsetz:%1").arg(nqimuc->calData.gyro_offset[2]) +
                        QString("|kx:%1").arg(nqimuc->calData.kx) + QString("|ky:%1").arg(nqimuc->calData.ky) +
                        QString("|kz:%1").arg(nqimuc->calData.kz) + QString("|syx:%1").arg(nqimuc->calData.syx) +
                        QString("|szx:%1").arg(nqimuc->calData.szx) + QString("|szy:%1").arg(nqimuc->calData.szy) +
                        QString("|bx:%1").arg(nqimuc->calData.bx) + QString("|by:%1").arg(nqimuc->calData.by) +
                        QString("|bz:%1|").arg(nqimuc->calData.bz) + QString("IMU_TEST_RESULT:PASS|");

                    pack.result = mesresult;
                    pack.itemvalue = itemvalue;
                    pack.instruct_num = "084";
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }

                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 20px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if (result == failValue) {
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 20px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                    QString mesresult = "NG";
                    QString itemvalue = QString("|outvoltage:%1").arg(voltage) + QString("|IMU_CALI_RESULT:NG|") +
                                        QString("IMU_TEST_RESULT:NG|");
                    pack.result = mesresult;
                    pack.itemvalue = itemvalue;
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }
                }

                testResultTableUpdate(testItems);

                waitWork(WAITTIME);
                protocolManager.set(DeviceCmd::ImuCollect, static_cast<int>(FacSwitch_STOP));
                waitWork(100);
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                stringsn = "";
                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                ui->macInput->clear();
                ui->getMac->clear();

                // emit fixture_left(getIndex());
                // emit fixture_down(getIndex());
                // emit fixture_right(getIndex());
                // emit fixture_up(getIndex());
                emit send_end_test(getIndex());

                showlog("流程结束");
                caliwaittime->stop();
                isTestContinue = false;  // 结束
                waitWork(100);
                on_disconnectButton_clicked();
                state = STATE_IDLE;

                break;

            default: break;
        }
    }
}

void imucali::on_pushButton_2_clicked() {
    at->sendBLEDEVICELOG(0);
    // for (int la = 0; la < 12; la++)
    // {
    //     QLabel *label = labelList.at(la);

    //     label->setStyleSheet(   // border: 2px solid #00FF00;
    //         " background-color: #00FF00; color: black;  border-radius: 10px; padding: 10px;
    //         text-align: center;");

    //     // QPalette palette = label->palette();
    //     // palette.setColor(QPalette::Window, Qt::gray);
    //     // label->setAutoFillBackground(true);
    //     // label->setPalette(palette);
    // }
}

void imucali::on_macInput_returnPressed() {
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

        // 主状态机流程
        isTestContinue = true;
        state = STATE_IDLE;
        if (pack.factory != "lx") {
            emit send_go_next_focus();
        }
    }
}

void imucali::on_getMac_returnPressed() {
    testResultTableInit();
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 20px; background-color: #808080; color: black;  border-radius: 10px; "
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
    if (pack.factory == "lx") {
        emit send_go_next_focus();
    }
    dataInit();
    showlog("正在查询mac地址");
    getMac(ui->getMac->text());  // 文件获取
    processInspection(ui->getMac->text());

    processGetMesTestValue();  // mes获取
}

void imucali::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";

        emit getMesTestValue(pack);
    }
}

void imucali::on_stopTest_clicked() {
    emit send_end_test(getIndex());

    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);
    isTestContinue = false;
    showlog("停止运行");
    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}




