#include "mainwindow.h"

#include <QRandomGenerator>
#include <QtConcurrent>

#include "md5.h"
#include "productlicense.h"
#include "qeventloop.h"
#include "ui_mainwindow.h"
// f4:12:fa:c5:51:c6
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
extern "C"  // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}
QByteArray allPackets;

void MainWindow::on_pushButton_clicked() {
    // int* p = nullptr;
    // *p = 42;  // 触发崩溃

    // 创建一个按钮
    QPushButton* button = nullptr;

    // 尝试访问空指针
    button->setText("Click Me");
    // ui->macInput->setText("B4:56:5D:BF:53:66");
    // ui->macInput->setText("B4:56:5D:BF:53:71");   // wd牙刷
    // ui->macInput->setText("b4:56:5d:bf:54:4e");   // wd牙刷
    // // ui->macInput->setText("f8:8f:c8:57:72:df");
    // ui->macInput->setText("b4:56:5d:bf:57:9d");
    // ui->macInput->setText("3C:84:27:07:A8:D2");

    // on_macInput_returnPressed();
    // // pb->set_device_mode();//进入亮白
    // emit send_camera_respone(FacErrorCode_NO_ERROR);
    // printSquareData(reinterpret_cast<uint8_t*>(pictureByteArray.data()),
    // pictureByteArray.size());
    // 创建包含 GBK 编码的字节数组
    // QByteArray dataTemp = QByteArray::fromRawData("\xaa\xaa\xaa", 3);  // GBK 编码的字节 0xAA

    // // 使用 GBK 编码器进行解码
    // QTextCodec* codec = QTextCodec::codecForName("gbk");
    // QString utf8String = codec->toUnicode(dataTemp);
    // QString decodedString = QString::fromUtf8(QByteArray::fromPercentEncoding(dataTemp));
    // QString localString = QString::fromLocal8Bit(dataTemp);
    // // 输出结果
    // qDebug() << "Converted utf8String:" << utf8String;
    // qDebug() << "Converted decodedString:" << decodedString;
    // qDebug() << "Converted localString:" << localString;

    // at->sendOTADATA(1);
    // showlog("已发送OTA数据通道开启");
    // waitWork(1000);
    // QByteArray data;
    // for (int i = 0; i < 500; ++i) {
    //     data.append(i % 256);  // 示例数据：0x00 ~ 0xFF 循环
    // }
    // dongleSerialPort->write(data);
}
void MainWindow::on_pushButton_3_clicked() {
    // printOtaDeviceKeys();
    // pb->set_i_am_app();
    // RotasFileStatusReq RotasFiledata;
    // RotasFiledata.fileType = RotasUpdateFile_BLE_FIRMWARE;

    // pb->set_start_ota_app(RotasFiledata);
    // waitWork(1000);
    // at->sendOTADATA(1);

    // pb->set_servo_motor_info();
}

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent), dongleSerialPort(new QSerialPort(this)), pb(new Qpb(dongleSerialPort)),
    at(new Qat(dongleSerialPort)), qimuc(new imu_calibrate), basicInfoModel(new TestModel),
    nqimuc(new new_imu_calibrate), peripheralModel(new TestModel), ui(new Ui::MainWindow), executor(pb) {
    ui->setupUi(this);
    // tts = new QTextToSpeech(this);
    // if (!tts) {
    //     qDebug() << "Failed to initialize QTextToSpeech.";
    //     return;
    // }
    // setAttribute(Qt::WA_QuitOnClose,  true); //关闭此窗口，会立即执行析构函数
    updateMainStyle("Ubuntu.qss");
    ui->wifi_test_result->setText("WIFI:WAIT");
    ui->wifi_test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                        "border-radius: 10px; padding: 10px; text-align: center; ");
    ui->ble_test_result->setText("BLE:WAIT");
    ui->ble_test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                       "border-radius: 10px; padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");
    // this->setCentralWidget(ui->tabWidget);
    ui->local_ota_result->setText("OTA");
    ui->local_ota_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                        "border-radius: 10px; padding: 10px; text-align: center; ");
    ui->bleotaresult->setText("WAIT");
    ui->bleotaresult->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                    "border-radius: 10px; padding: 10px; text-align: center; ");

    scanSerialPorts();                  // 要搜索一下一开始
    scanSerialPortsTimer->start(1000);  // 每秒刷新一次
    connect(scanSerialPortsTimer, SIGNAL(timeout()), this, SLOT(scanSerialPorts()));
    connect(scanSerialPortsTimer, SIGNAL(timeout()), this, SLOT(scanIpPorts()));

    QButtonGroup* buttonGroup = new QButtonGroup();
    buttonGroup->addButton(ui->is_wifiota_press);
    buttonGroup->addButton(ui->is_bleota_press);

    // 让 QCheckBox 互斥（默认为非互斥，需要手动启用）
    buttonGroup->setExclusive(true);

    initBasicInfo();
    initPeriphState();
    OTAGroup->addButton(ui->product_ver, 0);  // 分组1、序号0
    OTAGroup->addButton(ui->test_ver, 1);     // 分组1、序号1
    OTAGroup->addButton(ui->csv_ver, 2);
    OTAGroup->addButton(ui->america_ver, 3);
    OTAGroup->addButton(ui->america_product_ver, 4);

    WifiStatusLabel = new QLabel("wifi连接：<font color='red'>失败</font>");
    bleStatusLabel = new QLabel("蓝牙连接：<font color='red'>失败</font>");
    uartStatusLabel = new QLabel("串口连接：<font color='red'>失败</font>");
    macLabel = new QLabel("蓝牙mac:                        ");
    board_sn = new QLabel("板子sn:                        ");
    tail_sn = new QLabel("尾盖sn:                        ");
    sub_pid = new QLabel("sub_pid:        ");
    sku_id = new QLabel("sku_id:        ");
    // 添加用户友好的字符串到下拉框
    // ui->battery_type_combox->addItem("两节电池", FacBatteryType_TWO_BATTERY);
    // ui->battery_type_combox->addItem("单节电池",FacBatteryType_ONE_BATTERY);

    otaSourceSet(1);  // 一开机锁住
    otaFwSet(1);      // 一开机锁住

    QAction* updata = ui->menubar->addAction("软件更新");

    connect(updata, &QAction::triggered, [=]() { checkAndUpdateFile(); });
    // 设置菜单栏样式
    ui->menubar->setStyleSheet("QMenuBar { "
                               "    background-color: white; "  // 设置菜单栏背景颜色为白色
                               "    color: black; "             // 设置菜单栏文字颜色为黑色
                               "}"
                               "QMenuBar::item { "
                               "    background-color: white; "  // 设置菜单项背景颜色为白色
                               "    color: black; "             // 设置菜单项文字颜色为黑色
                               "}"
                               "QMenuBar::item:selected { "
                               "    background-color: lightgray; "  // 设置选中菜单项的背景颜色
                               "    color: black; "                 // 设置选中菜单项的文字颜色
                               "}");

    ui->statusbar->addPermanentWidget(board_sn);
    ui->statusbar->addPermanentWidget(tail_sn);
    ui->statusbar->addPermanentWidget(sub_pid);
    ui->statusbar->addPermanentWidget(sku_id);

    ui->statusbar->addPermanentWidget(macLabel);
    ui->statusbar->addPermanentWidget(bleStatusLabel);
    ui->statusbar->addPermanentWidget(WifiStatusLabel);
    ui->statusbar->addPermanentWidget(uartStatusLabel);
    ui->statusbar->addPermanentWidget(new QLabel(DEBUG_VER + QString(__DATE__) + " " + QString(__TIME__)));
    otaResults << "手柄收到指令,开始OTA..."
               << "GENERAL"
               << "低电量"
               << "已经在OTA中"
               << "文件过大"
               << "MD5错误"
               << "文件不支持"
               << "FLASH错误"
               << "No Memory"
               << "TRANS_TIMEOUT"
               << "TRANS_OVER_RANGE"
               << " 下载成功"
               << "下载失败"
               << "没有配网"
               << "网络连接失败"
               << "获取文件URL失败，可能是长度超了"
               << "文件下载校验失败"
               << "文件下载安装失败"
               << "找不到网络"
               << "密码错误"
               << "正在下载文件中"
               << "WIFI已禁用"
               << "资源更新中"
               << "存储空间不足";

    connect(pb, SIGNAL(send_ota_flow_control(int)), this, SLOT(setBleOtaState(int)));
    connect(at, SIGNAL(send_dongle_wifi(QString)), this, SLOT(getDongleWifi(QString)));
    connect(at, SIGNAL(send_dongle_ver(QString)), this, SLOT(getDongleVer(QString)));

    connect(pb, SIGNAL(send_press_cali_data(FacPreSensorCalibResult)), this,
            SLOT(getPresscalidata(FacPreSensorCalibResult)));
    connect(nqimuc, SIGNAL(send_imu_cali_msg(QString)), this, SLOT(refreshImuCaliMsg(QString)));
    connect(pb, SIGNAL(send_pb_date(QString)), this, SLOT(refreshPbData(QString)));
    connect(this, SIGNAL(send_thread_date(QString)), this, SLOT(refreshPbData(QString)));
    connect(pb, &Qpb::send_pb_info, [&](QString s) {
        appendAndSaveWifiOtaLog(" ");
        appendAndSaveWifiOtaLog(QDateTime::currentDateTime().toString(Qt::ISODate) + s);

        ui->bleOtaMsg->appendPlainText(" ");
        ui->bleOtaMsg->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate) + s);
    });

    connect(pb, &Qpb::send_ota_result, [&](int r) {
        appendAndSaveWifiOtaLog(" ");
        appendAndSaveWifiOtaLog(QDateTime::currentDateTime().toString(Qt::ISODate) + "OTA 结果 : " + otaResults[r]);

        ui->bleOtaMsg->appendPlainText(" ");
        ui->bleOtaMsg->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate) +
                                       "OTA 结果 : " + otaResults[r]);

        if (r == 11) {
            appendAndSaveWifiOtaLog(QString("升级次数:%1").arg(wifiotaSuctimes++) +
                                    QString("，wifiota耗时:%1 s").arg(totalwifiOtaTime.elapsed() / 1000));
            ui->wifiota_suc_times->setText(QString("成功升级次数:%1").arg(wifiotaSuctimes));
            ui->bleotaresult->setText("PASS");
            ui->bleotaresult->setStyleSheet("font-size: 33px; background-color: #00FF00; color: "
                                            "black; border: 2px solid #00FF00; border-radius: "
                                            "10px; padding: 10px; text-align: center;");
            on_disconnectButton_clicked();
            if (ui->is_bleota_press->checkState()) {
                on_bleotamacInput_returnPressed();
            } else if (ui->is_wifiota_press->checkState()) {
                on_wifiOtaMacInput_returnPressed();

            } else {
                if (ui->is_clear_mac->checkState()) {
                    ui->bleotamacInput->clear();
                    ui->bleotamacInput->setFocus();
                }
            }

        } else if (r == 12) {
            appendAndSaveWifiOtaLog(QString("升级次数:%1").arg(wifiotaFaiTimes++) +
                                    QString("，wifiota耗时:%1 s").arg(totalwifiOtaTime.elapsed() / 1000));
            ui->wifiota_fai_times->setText(QString("失败升级次数:%1").arg(wifiotaFaiTimes));
            on_stopBleOta_clicked();
            on_disconnectButton_clicked();
            ui->bleotaresult->setText("FAIL");
            ui->bleotaresult->setStyleSheet("font-size: 33px; background-color: #FF0000; color: "
                                            "black; border: 2px solid #FF0000; border-radius: "
                                            "10px; padding: 10px; text-align: center; ");
            if (ui->is_bleota_press->checkState()) {
                on_bleotamacInput_returnPressed();
            } else if (ui->is_wifiota_press->checkState()) {
                on_wifiOtaMacInput_returnPressed();

            } else {
                if (ui->is_clear_mac->checkState()) {
                    ui->bleotamacInput->clear();
                    ui->bleotamacInput->setFocus();
                }
            }
        }
    });
    connect(this, SIGNAL(sendPicture_speed(int)), ui->picture_speed, SLOT(setValue(int)));
    connect(this, SIGNAL(sendBelOtaSpeed(int)), ui->bel_fw_ota_speed, SLOT(setValue(int)));
    connect(this, SIGNAL(sendBelSourceOtaSpeed(int)), ui->bel_source_ota_speed, SLOT(setValue(int)));

    connect(pb, SIGNAL(send_ota_progress(int)), ui->bel_ota_receive_speed, SLOT(setValue(int)));

    connect(pb, SIGNAL(send_get_picture_send_over(FacPictureDataAck)), this,
            SLOT(getPictureSendOver(FacPictureDataAck)));

    connect(pb, SIGNAL(send_press_data(FacUploadPresSensor)), this, SLOT(getPressSensorData(FacUploadPresSensor)));
    connect(pb, SIGNAL(send_imu_data(FacUploadNineAlex)), this, SLOT(getimuData(FacUploadNineAlex)));
    connect(pb, SIGNAL(send_battary(FacDevInfo)), this, SLOT(refreshBattaryData(FacDevInfo)));
    connect(pb, SIGNAL(send_wifi_State(FacDevInfo)), this, SLOT(updateWifi(FacDevInfo)));
    connect(pb, SIGNAL(send_sn_data(FacDevInfo)), this, SLOT(refreshSn(FacDevInfo)));
    connect(pb, SIGNAL(send_music_state(FacDevInfo)), this, SLOT(refreshMusicState(FacDevInfo)));

    connect(this->dongleSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleDongleSerialPortError(QSerialPort::SerialPortError)));
    connect(at, SIGNAL(sendWifiMsg(QString)), this, SLOT(getWifiMsg(QString)));
    connect(at, SIGNAL(sendWifiIp(QString)), this, SLOT(getWifiIp(QString)));

    connect(pb, SIGNAL(send_FactroyCmd_INTERNET_OTA(FacInternetOta)), this, SLOT(updateLocalOtaResult(FacInternetOta)));

    connect(this, SIGNAL(send_fault_data_packet(int, const QVector<int>&)), pb,
            SLOT(set_camera_fault_data_packet(int, const QVector<int>&)));

    connect(pb, SIGNAL(send_motor_cali_msg(QString)), this, SLOT(refreshMotorCaliMsg(QString)));

    connect(pb, SIGNAL(send_FactroyCmd_WIFI_DEMAND(FacWifiDemand)), this, SLOT(refreshWifiDemand(FacWifiDemand)));

    connect(pb, SIGNAL(send_IMU_CALIB_result(FacImuCalibResult)), this, SLOT(refreshImuCaliResult(FacImuCalibResult)));

    connect(pb, SIGNAL(send_servo_motor_info_msg(FacMotorCalibResult)), this,
            SLOT(getServoMotorInfoMsg(FacMotorCalibResult)));

    connect(this, SIGNAL(send_dongle_serialPort_state(int)), this, SLOT(refreshDongleUartState(int)));
    connect(ui->is_ota_1, SIGNAL(stateChanged(int)), this, SLOT(otaSourceSet(int)));
    connect(ui->is_ota_2, SIGNAL(stateChanged(int)), this, SLOT(otaFwSet(int)));
    connect(ui->show_rssi, SIGNAL(stateChanged(int)), at, SLOT(sendBLELOG(int)));
    connect(ui->show_bursh_log, SIGNAL(stateChanged(int)), pb, SLOT(set_uart_receive(int)));
    connect(ui->press_sensor_temp__switch, SIGNAL(stateChanged(int)), pb, SLOT(set_press_sensor_temp(int)));

    connect(this, &MainWindow::send_image_processed, this, &MainWindow::updateImageOnMainThread);

    connect(at, SIGNAL(send_ble_state(int)), this, SLOT(refreshBleState(int)));
    connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
    connect(at, SIGNAL(send_wifi_rssi(QString)), this, SLOT(refreshWifiRssi(QString)));
    // connect(at, SIGNAL(send_WIFI_state(int)), this,
    // SLOT(refreshWifiState(int)));

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
    });
    connect(dongleSerialPort, &QSerialPort::readyRead, this, [=]() {
        dongleSerialPortTimer->start(5);                          // 设置100毫秒的延时
        dongleSerialPortBuf.append(dongleSerialPort->readAll());  // 将读到的数据放入缓冲区
    });

    // 连接信号和槽
    // QObject::connect(cameratimer, &QTimer::timeout, this,
    // &MainWindow::solve_frame);
    // 启动定时器
    // cameratimer->start(1000);

    QFont font("Arial", 10);
    ui->gyro_x->setFont(font);
    ui->gyro_y->setFont(font);
    ui->gyro_z->setFont(font);
    ui->acc_x->setFont(font);
    ui->acc_y->setFont(font);
    ui->acc_z->setFont(font);
    ui->getMac->setFocus();

    // U7
    ui->nfc_combo->addItem("U7P");
    // U7P
    ui->nfc_combo->addItem("U7");

    ui->music_combo->addItem("/data/audio/scan_f.bin");
    ui->music_combo->addItem("/data/audio/white.bin");
    for (int i = 0; i <= 15; ++i) {
        QString item = QString("/data/audio/sound%1.bin").arg(i);
        ui->music_combo->addItem(item);
    }
    // 201固件  202资源  303音频 304主题 305音乐文件
    QStringList items = {"固件", "资源", "音频", "主题", "音乐文件"};

    for (auto& item : items) {
        ui->version_type_1->addItem(item);
        ui->version_type_2->addItem(item);
    }
    ui->version_type_1->setCurrentIndex(1);

    HighRssi = SETTINGS.value("WIFI/HighRssi").toDouble();
    LowRssi = SETTINGS.value("WIFI/LowRssi").toDouble();
    BleHighRssi = SETTINGS.value("BLE/HighRssi").toDouble();
    BleLowRssi = SETTINGS.value("BLE/LowRssi").toDouble();
    RssiTestTime = SETTINGS.value("BLE/RssiCount").toInt();

    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();
    QRegExp regExp("[0-9]{1,},[0-9]{1,},[0-9]{1,},[0-9]{1,},[0-9]{1,},[0-9]{1,}");
    QValidator* validator = new QRegExpValidator(regExp, this);
    ui->brushTime->setValidator(validator);
    ui->pressureTime->setValidator(validator);
    ui->horizalBrushTime->setValidator(validator);
    ui->dateTimeBrushSet->setDateTime(QDateTime::currentDateTime());
    ui->startDateTime->setDateTime(QDateTime::currentDateTime());
    ui->endDateTime->setDateTime(QDateTime::currentDateTime());
    // comNameCombo = new QComboBox(ui->toolBar);
    // ui->toolBar->addWidget(comNameCombo);
    // comNameCombo->clear();
    // comNameCombo->setMinimumWidth(50);
    // foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    // {
    //     comNameCombo->addItem(info.portName());
    //  }

    ui->progressBar->hide();
    ui->tabWidget->setTabVisible(8, SETTINGS.value("SYSTEM/ShowLocalOTAFunc").toBool());

    if (!SETTINGS.value("SYSTEM/ShowUpperComputerOTAFunc").toInt())
        updata->setVisible(false);

    ui->high_speed_tp->installEventFilter(this);
    ui->high_speed_tp->setAcceptDrops(true);

    ui->active_picture->installEventFilter(this);
    ui->active_picture->setAcceptDrops(true);

    if (SETTINGS.value("Mes/FACTORY").toString() == "hq")
        ui->statusbar->addPermanentWidget(new QLabel("华勤"));
    else if (SETTINGS.value("Mes/FACTORY").toString() == "lx")
        ui->statusbar->addPermanentWidget(new QLabel("立讯"));
    else if (SETTINGS.value("Mes/FACTORY").toString() == "jj")
        ui->statusbar->addPermanentWidget(new QLabel("金进"));
    else
        ui->statusbar->addPermanentWidget(new QLabel("欣旺达"));

    viewercamrea = new ImageViewer("image.png", this);
    ui->verticalLayout_31->addWidget(viewercamrea);  // 将 ImageViewer 添加到布局中
    viewercamrea->show();                            // 显示 ImageViewer
    viewercamrea->raise();
    dongleRingBuf = new RingBuf(&p_dongleRingBuffer, dongle_ring_buffer, 1, sizeof(dongle_ring_buffer));
    cameraRingBuf = new RingBuf(&p_cameraRingBuffer, camera_ring_buf, 1, sizeof(camera_ring_buf));
    recoverCustom();
    //  qDebug() << "Thread
    //  video_frame_data_struct..."<<sizeof(video_frame_data_struct);

    // //  在需要的地方调用 QtConcurrent::run 来异步执行函数
    // QFuture<void> future = QtConcurrent::run([this]() {

    //     while (1) {
    //         // 在 Lambda 表达式中调用 solve_frame 函数
    //         solve_frame();
    //         //qDebug() << "Thread running...";
    //         QThread::msleep(10); // 等待1秒
    //     }

    // });

    // 启动后台线程
    future = QtConcurrent::run([this]() {
        while (running.load()) {
            solve_frame();
            // QCoreApplication::processEvents();

            QThread::msleep(5);  // 等待10毫秒
        }
    });
    running.store(true);

    //上位机ota使用的
    updatamanager = new QNetworkAccessManager(this);
    connect(updatamanager, &QNetworkAccessManager::authenticationRequired, this, &MainWindow::provideAuthentication);

    pb->APP_VERSION = DEBUG_VER;

    QAction* setting = ui->menubar->addAction("功能设置");
    connect(setting, &QAction::triggered, [=]() { setting_ui(); });
    if (!SETTINGS.value("SYSTEM/setting").toInt()) {
        setting->setVisible(false);
    }

    aimanager = new QNetworkAccessManager(this);
    connect(aimanager, &QNetworkAccessManager::finished, this, &MainWindow::onRequestFinished);
    ui->wifiotaprogress->setMaximum(100);
    connect(pb, &Qpb::send_ota_progress, [&](int s) { ui->wifiotaprogress->setValue(s); });
    connect(pb, SIGNAL(send_button_state(FacButtonState)), this, SLOT(checkbutton(FacButtonState)));
}

void MainWindow::checkbutton(FacButtonState x) {
    qDebug() << "上报的按键个数" << x.button_state_count;
    showlog("电源按键" + QString::number(x.button_state[0].command_data.power_button.button_state_now));
    showlog("模式按键" + QString::number(x.button_state[1].command_data.mode_button.button_state_now));
}
void MainWindow::setting_ui() {
    if (qsetting_ui == NULL) {
        qsetting_ui = new qsetting;
    }
    qsetting_ui->raise();
    qsetting_ui->show();
    qsetting_ui->activateWindow();
}
void MainWindow::on_add_data_clicked() {
    int ring_size = cameraRingBuf->usmile_ring_buffer_items_count_get(&p_cameraRingBuffer);
    if (36040 > ring_size) {
        std::vector<uint8_t> p_data(36040 - ring_size,
                                    0);  // 动态分配大小并初始化为0
        write_camera_data(p_data.data(),
                          p_data.size());  // 使用向量的数据和大小
                                           //  solve_picture_frame();
    }
}

void MainWindow::on_init_ui_data_clicked() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/trigger_function"));  // 拼接 "/trigger_function" 到 ESP32 的 IP 地址

    // 发送 GET 请求
    QNetworkReply* reply = manager->get(request);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Request succeeded";
            showlog("文件系统初始化完毕");
        } else {
            qDebug() << "Request failed:" << reply->errorString();
            showlog("文件系统初始化失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_get_battery_level_clicked() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/battery_level"));  // 拼接 "/battery_level" 到 ESP32 的 IP 地址

    // 发送 GET 请求
    QNetworkReply* reply = manager->get(request);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            QString batteryLevel = reply->readAll();
            qDebug() << "Battery level request succeeded";
            showlog("电池电量: " + batteryLevel + "%");
        } else {
            qDebug() << "Battery level request failed:" << reply->errorString();
            showlog("获取电池电量失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::updateImageOnMainThread() {
    processTheDatagram(pictureByteArray);  // 显示图片
}
MainWindow::~MainWindow() {
    is_motor_continue = false;
    isimuCaliContinue = false;
    isrssiContinue = false;
    isWifiOtaContinue = false;
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent*) {
    qDebug() << "关闭上位机";
    on_stopBleOta_clicked();
    running.store(false);
    // 等待线程结束
    future.waitForFinished();
    saveCustom();
    isrssiContinue = false;
    isWifiOtaContinue = false;
    if (qsetting_ui != NULL)
        qsetting_ui->close();
}

void MainWindow::handleDongleSerialPortError(QSerialPort::SerialPortError error) {
    qDebug() << "DongleSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        closeDongleSerialPort();
    }
}

void MainWindow::openDongleSerialPort() {
    if (dongleSerialPort->isOpen()) {
        disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
                   &MainWindow::readDongleSerialPortData);  // timeout执行真正的读取操作
        dongleSerialPort->close();
    }

    // 设置串口名
    dongleSerialPort->setPortName(ui->comNameCombo->currentText());
    // 设置波特率
    dongleSerialPort->setBaudRate(921600);
    // 设置数据位
    dongleSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    dongleSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    dongleSerialPort->setStopBits(QSerialPort::OneStop);
    dongleSerialPort->setReadBufferSize(4096);

    // 设置流控制
    dongleSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (dongleSerialPort->open(QIODevice::ReadWrite)) {
        if (ui->is_reset_dongle->checkState()) {
            // 启用RTS信号
            dongleSerialPort->setRequestToSend(true);
            // 启用DTR信号
            dongleSerialPort->setDataTerminalReady(true);
            // 启用RTS信号
            dongleSerialPort->setRequestToSend(false);
            // 启用DTR信号
            dongleSerialPort->setDataTerminalReady(false);
            // 启用RTS信号
            dongleSerialPort->setRequestToSend(true);
            // 启用DTR信号
            dongleSerialPort->setDataTerminalReady(true);
            // 启用RTS信号
            dongleSerialPort->setRequestToSend(false);
            // 启用DTR信号
            dongleSerialPort->setDataTerminalReady(false);
        } else {
            // 启用RTS信号
            dongleSerialPort->setRequestToSend(true);
            // 启用DTR信号
            dongleSerialPort->setDataTerminalReady(true);
        }
        // showlog("串口连接成功");
        emit send_dongle_serialPort_state(1);

        connect(dongleSerialPortTimer, &QTimer::timeout, this,
                &MainWindow::readDongleSerialPortData);  // timeout执行真正的读取操作
    } else {
        appendAndSaveWifiOtaLog("串口被占用！");
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // showlog("打开错误");
    }
}

void MainWindow::closeDongleSerialPort() {
    if (dongleSerialPort->isOpen())
        dongleSerialPort->close();
    disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
               &MainWindow::readDongleSerialPortData);  // timeout执行真正的读取操作

    emit send_dongle_serialPort_state(0);
}

void MainWindow::on_clear_scan_clicked() {
    ui->mac_combo->clear();
    deviceMap.clear();
}

void MainWindow::on_mac_combo_textActivated(const QString& arg1) {
    ui->log->clear();
    ui->msgEdit->clear();
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = arg1;
        at->sendMac(macAddress);  // 发送mac地址
        qDebug() << macAddress;
        bandingMacSn(macAddress, snbanding);
        if (!ui->is_just_banding->checkState())
            on_motor_cali_clicked();
    }
}

void MainWindow::on_snbanding_returnPressed() {
    qDebug() << "on_snbanding_returnPressed";
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    ui->snbanding->clear();
}

void MainWindow::on_getMac_returnPressed() {
    getMac(ui->getMac->text());  // 文件获取
}

void MainWindow::on_damping_open_clicked() { pb->set_motor_damping_state(1); }

void MainWindow::on_damping_close_clicked() { pb->set_motor_damping_state(0); }

void MainWindow::on_disconnectButton_clicked() {
    qDebug() << "on_disconnectButton_clicked";
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址

    waitWork(100);

    closeDongleSerialPort();
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}

void MainWindow::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void MainWindow::on_getBasicInfoButton_clicked() {
    basicInfoModel->resetAllTestResult();
    pb->get_base_info();
}

void MainWindow::on_getperipheralButton_clicked() {
    peripheralModel->resetAllTestResult();
    pb->get_periph_state();
}

void MainWindow::on_stopimuCaliButton_clicked() {
    isimuCaliContinue = false;
    ui->imuCaliButton->setEnabled(true);
}

void MainWindow::on_enterShipModeButton_clicked() {
    if (1)  //(at->getConnected())
    {
        pb->set_ship_mode(1);
        showlog("已发送进入船运");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_macInput_returnPressed() {
    clearDisplay();

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
        pb->NEEDAES = 0;
        macAddress = ui->macInput->text();
        macLabel->setText("蓝牙mac: " + macAddress);
        at->sendMac(ui->macInput->text());  // 开始连接
        ui->snInput->setFocus();
        if (ui->checkbanding->checkState()) {
            ui->macInput->clear();
        }
    }
    pb->setPbMode(1);
}

void MainWindow::on_lcdTestButton_clicked() {
    // 编写屏幕测试的代码
    static int state = 0;

    if (at->getConnected()) {
        pb->set_screen_color(state);
        showlog("已发送屏幕颜色");
    } else {
        showlog("请等待连接牙刷后再试");
    }

    state++;
    if (state == 5)
        state = 0;
}

void MainWindow::on_snInput_returnPressed() {
    // 检查是否是序列号格式

    QString snPattern = SETTINGS.value("Regex/SNPattern", "^[0-9a-zA-Z]{18}$").toString();
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->snInput->text()).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->snInput->text().length()));
        showlog("要求格式为" + snPattern);

        ui->snInput->clear();
        return;
    }

    stringsn = ui->snInput->text();
    subpid = getValueBySN(ui->snInput->text()).toUtf8();
    if ("SUBPID_ERRO" == subpid) {
        QMessageBox::warning(nullptr, "Warning", "没匹配到subpid");
        return;
    }
    QByteArray sn = ui->snInput->text().toUtf8();

    if (at->getConnected()) {
        pb->set_sn(FacDevInfoType_SUB_PID, subpid);
        showlog("已绑定subpid到牙刷");

        pb->set_sn(FacDevInfoType_TAIL_SN, sn);
        showlog("已绑定sn到牙刷");

        bandSnMacToCsv(macAddress, sn);
        showlog("已绑定保存到文件");
    } else {
        showlog("请等待连接牙刷后再试");
    }

    if (ui->checkbanding->checkState()) {
    } else {
        waitWork(WAITTIME);
        on_getBasicInfoButton_clicked();
        waitWork(WAITTIME);
        on_getperipheralButton_clicked();
    }
}

void MainWindow::on_enterBurningMode_clicked() {
    if (at->getConnected()) {
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
void MainWindow::on_pushButton_2_clicked() {
    FacSetBrushRecord record;

    // specialmode 1641600010 0 120000 0 5000 0 0 0 1000 0 0 0 0 1500 1000 0 0 0
    // 0 1000
    QRegularExpression regex("(\\d+)");
    QRegularExpressionMatchIterator matchIterator = regex.globalMatch(ui->lineEdit->text());
    QList<int> data;
    while (matchIterator.hasNext()) {
        QRegularExpressionMatch match = matchIterator.next();
        QString number = match.captured(1);
        data << number.toInt();
    }

    if (data.size() == 20) {
        record.timestamp = data[0];
        record.plaque = data[1];
        int n = 2;
        for (int i = 0; i < 6; i++) {
            record.work_time.bytes[i] = data[n++];
        }

        for (int i = 0; i < 6; i++) {
            record.pressure_time.bytes[i] = data[n++];
        }

        for (int i = 0; i < 6; i++) {
            record.horizon_brush.bytes[i] = data[n++];
        }
        record.work_time.size = 24;
        record.pressure_time.size = 12;
        record.horizon_brush.size = 12;
        pb->set_brush_record(record);

        ui->msgTest->appendPlainText("发送时间:" + QDateTime::currentDateTime().toString(Qt::ISODate));
    } else {
        ui->msgTest->appendPlainText("输入错误");
    }
}

void MainWindow::on_exitBurningMode_clicked() {
    if (at->getConnected()) {
        pb->set_burning_mode(1, FacSwitch_CLOSE);
        showlog("已退出老化模式");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_music_play_clicked() {
    myAudioRecorde(1);
    QString musicFileName = ui->music_combo->currentText();
    qDebug() << "文件名为" << musicFileName;
    QByteArray musicFileNameBytes = musicFileName.toUtf8();
    pb->set_music(musicFileNameBytes);
}

void MainWindow::on_just_music_clicked() {
    QString musicFileName = ui->music_combo->currentText();
    qDebug() << "文件名为" << musicFileName;
    QByteArray musicFileNameBytes = musicFileName.toUtf8();
    pb->set_music(musicFileNameBytes);
}

void MainWindow::on_entersleep_clicked() {
    if (at->getConnected()) {
        // pb->set_forbid_sleep(FacSwitch_CLOSE);
        pb->set_sleeep(FacSwitch_OPEN);
        // waitWork(100);
        // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
        waitWork(50);
        // on_disconnectButton_clicked();
        // 3C:84:27:07:A8:D2

        showlog("已发送休眠");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_forbidsleep_clicked() {
    pb->set_forbid_sleep(FacSwitch_OPEN);
    if (at->getConnected()) {
        showlog("已禁止休眠");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_fac_mode_clicked() {
    if (1) {
        pb->set_fac_mode(1);
        showlog("设置进入工厂模式");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_getwifi_clicked() {
    if (at->getConnected()) {
        pb->get_wifi_info();
        showlog("正在获取wifi设置信息");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_disconnectwifi_clicked() {
    if (at->getConnected()) {
        pb->set_wifi_disconnect();
        showlog("已发送断开wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}
void MainWindow::on_connectwifi_clicked() {
    QString wifiName = ui->wifiUserName->text();      // SETTINGS.value("WIFI/Name").toString();
    QString wifiPassword = ui->wifiPassword->text();  // SETTINGS.value("WIFI/Password").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected()) {
        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_rebot_clicked() {
    if (1) {
        pb->set_dev_reset();
        showlog("重启");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_exit_fac_mode_clicked() {
    pb->set_fac_mode(0);
    showlog("设置退出工厂模式");
}

void MainWindow::on_fre_returnPressed() { ui->duty->setFocus(); }

void MainWindow::on_duty_returnPressed() {
    uint32_t fre = ui->fre->text().toUInt();
    float duty = ui->duty->text().toFloat();

    if (duty < 0 || duty > 100) {
        QMessageBox::warning(NULL, "警告", " 占空比输入错误！\r\n");
        return;
    }
    if (fre < 100 || fre > 1000) {
        QMessageBox::warning(NULL, "警告", " 频率输入错误！\r\n");
        return;
    }

    pb->set_motor_param(fre, duty);

    if (ui->open_motor_set->isChecked()) {
        waitWork(WAITTIME);
        pb->set_motor_state(1);
    }
    qDebug() << "频率为：" << fre << "占空比为" << duty;
}

void MainWindow::on_close_motor_clicked() { pb->set_motor_state(0); }

void MainWindow::on_start_motor_clicked() { pb->set_motor_state(1); }

void MainWindow::on_start_wifible_test_clicked() {
    typedef enum {
        STATE_IDLE,                       // 休眠状态
        STATE_WATI_GET_CORRECT_WIFIRSSI,  // 等待正确的wifi信号强度
        STATE_WATI_GET_CORRECT_BLERSSI,   // 等待正确的蓝牙信号强度
        STATE_SAVE_RESULT                 // 保存结果在本地
    } State;
    int intwifirssi;
    int intblerssi;

    State state = STATE_IDLE;
    QString wifiresult;
    QString bleresult;
    int rssitestcount = 0;
    int rssitestfailcount = 0;

    // 主状态机流程
    isrssiContinue = true;
    while (isrssiContinue) {
        switch (state) {
            case STATE_IDLE:  // 复位一切
                rssitestcount = 0;
                rssitestfailcount = 0;
                state = STATE_WATI_GET_CORRECT_WIFIRSSI;
                ui->wifi_test_result->setText("WIFI:WAIT");
                ui->wifi_test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                                    "border-radius: 10px; padding: 10px; text-align: center; ");
                ui->ble_test_result->setText("BLE:WAIT");
                ui->ble_test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                                   "border-radius: 10px; padding: 10px; text-align: center; ");
                on_connectwifi_clicked();
                waitWork(2000);

                break;

            case STATE_WATI_GET_CORRECT_WIFIRSSI: {
                intwifirssi = WIFI_RSSI.toInt();
                if (intwifirssi < HighRssi && intwifirssi > LowRssi)  // wifi信号可以
                {
                    rssitestcount++;

                    if (rssitestcount > RssiTestTime)  // wifi信号可以
                    {
                        ui->wifi_test_result->setText("WIFI:PASS");
                        ui->wifi_test_result->setStyleSheet("font-size: 33px; background-color: #00FF00; color: "
                                                            "black; border: 2px solid #00FF00; border-radius: "
                                                            "10px; padding: 10px; text-align: center;");
                        pb->set_wifi_disconnect();
                        wifiresult = "通过";
                        at->sendBLELOG(1);  // 日志关
                        state = STATE_WATI_GET_CORRECT_BLERSSI;
                        rssitestcount = 0;
                    }
                    break;
                } else {
                    rssitestcount = 0;
                    rssitestfailcount++;
                    if (rssitestfailcount > RssiTestTime)  // wifi信号不可以
                    {
                        wifiresult = "失败";
                        ui->wifi_test_result->setText("WIFI：FAIL");
                        ui->wifi_test_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: "
                                                            "black; border: 2px solid #FF0000; border-radius: "
                                                            "10px; padding: 10px; text-align: center; ");

                        qDebug() << "wifi不合格信号强度" << WIFI_RSSI;
                        ui->log->appendPlainText("wifi不合格信号强度" + WIFI_RSSI);
                        rssitestfailcount = 0;
                        at->sendBLELOG(1);  // 日志关
                        state = STATE_WATI_GET_CORRECT_BLERSSI;
                    }
                }
            }

            break;

            case STATE_WATI_GET_CORRECT_BLERSSI: {
                intblerssi = BLE_RSSI.toInt();
                if (intblerssi < BleHighRssi && intblerssi > BleLowRssi)  // 蓝牙信号可以
                {
                    rssitestcount++;
                    if (rssitestcount > RssiTestTime)  // 蓝牙信号可以
                    {
                        ui->ble_test_result->setText("BLE:PASS");
                        ui->ble_test_result->setStyleSheet("font-size: 33px; background-color: #00FF00; color: "
                                                           "black; border: 2px solid #00FF00; border-radius: "
                                                           "10px; padding: 10px; text-align: center;");
                        state = STATE_SAVE_RESULT;
                        rssitestcount = 0;
                        bleresult = "通过";
                        at->sendBLELOG(0);  // 日志关
                    }
                    break;
                } else {
                    rssitestcount = 0;
                    rssitestfailcount++;

                    if (rssitestfailcount > RssiTestTime)  // 蓝牙信号不可以
                    {
                        ui->ble_test_result->setText("BLE：FAIL");
                        ui->ble_test_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: "
                                                           "black; border: 2px solid #FF0000; border-radius: "
                                                           "10px; padding: 10px; text-align: center; ");

                        bleresult = "失败";
                        qDebug() << "蓝牙不合格信号强度" << BLE_RSSI;
                        ui->log->appendPlainText("蓝牙不合格信号强度" + BLE_RSSI);
                        state = STATE_SAVE_RESULT;
                        at->sendBLELOG(0);  // 日志关
                        rssitestfailcount = 0;
                    }
                }
            } break;
            case STATE_SAVE_RESULT: {
                saveRssiDataToCsv(intwifirssi, intblerssi, wifiresult, bleresult);
                state = STATE_IDLE;
                isrssiContinue = false;
            } break;
        }
        waitWork(100);
        QCoreApplication::processEvents();
    }
}

void MainWindow::on_setTimePushButton_clicked() {
    int timestamp = ui->dateTimeBrushSet->dateTime().toSecsSinceEpoch();
    pb->set_brush_time(timestamp);
}

void MainWindow::on_resetPushButton_clicked() { pb->set_brush_reset(); }
void MainWindow::on_sendBrushDataPushButton_clicked() { sendBrushData(false); }

void MainWindow::on_sendRandomData_clicked() { sendBrushData(true); }

void MainWindow::on_clearDataPushButton_clicked() { ui->msgTest->clear(); }
void MainWindow::on_imuCaliButton_clicked()  // 编写六轴校准的代码
{
    ui->imuCaliButton->setEnabled(false);

    typedef enum {
        STATE_IDLE,                 // 休眠状态
        STATE_WATI_CONNECT,         // 等待连接
        STATE_DISABLE_SLEEP_1,      // 进入禁止休眠
        STATE_SET_COLLECT_PARAM_1,  // 获取采集参数
        STATE_CAIL,                 // 开始校准
        STATE_SEND_CAIL_RESULT,     // 发送校准结果
        STATE_SAVE_RESULT           // 保存结果在本地
    } State;
    State state = STATE_IDLE;
    // 主状态机流程
    isimuCaliContinue = true;
    while (isimuCaliContinue) {
        switch (state) {
            case STATE_IDLE:  // 复位一切

                isovertime = 0;
                isimuCaliOk = 0;  // 是否校准完成
                is_start_ium_cali = 0;
                isStartSendCaliResult = 0;  // 是否开始发送校验结果
                pb->reset_all_pb();

                if (!at->getConnected()) {
                    at->resetConnected();
                    at->sendMac(ui->macInput->text());  // 发送mac地址
                    showlog(ui->macInput->text());
                }

                state = STATE_WATI_CONNECT;
                break;

            case STATE_WATI_CONNECT:  // 设置禁止休眠
                if (at->getConnected()) {
                    refreshBleState(1);
                    state = STATE_DISABLE_SLEEP_1;
                }
                break;

            case STATE_DISABLE_SLEEP_1:  // 设置设备采集
                if (pb->getDisableSleep()) {
                    showlog("已进入禁止休眠");
                    pb->set_imu_collect_param(FacSwitch_START);
                    state = STATE_CAIL;
                }
            case STATE_CAIL:  // 开始校准
                if (pb->get_isSetimuCollectParam()) {
                    showlog("已发送imu采集参数");
                    qimuc->imu_calib_init();
                    is_start_ium_cali = 1;
                    qDebug() << "开始校准";
                    showlog("等待校准");
                    waittime->start(imu_wait_time);
                    state = STATE_SEND_CAIL_RESULT;
                }
                break;

            case STATE_SEND_CAIL_RESULT:  // 开始发送校准值
                if (isimuCaliOk) {
                    showlog("六轴校准成功");
                    result = passValue;
                    // isStartSendCaliResult=1;//发送校准值
                    // FacUploadNineAlex x;
                    // getimuData(x);
                    pb->set_imu_cali_result(qimuc->calData);
                    waitWork(WAITTIME);
                    pb->get_imu_cali_result();
                    waitWork(WAITTIME);
                    pb->set_imu_collect_param(FacSwitch_STOP);
                    state = STATE_SAVE_RESULT;
                    waittime->stop();
                }
                if (isovertime) {
                    showlog("六轴校准失败：超时");
                    pb->set_imu_collect_param(FacSwitch_STOP);
                    state = STATE_SAVE_RESULT;
                    result = failValue;
                }

                break;

            case STATE_SAVE_RESULT:
                if (pb->get_is_get_imu_cali_data()) {
                    saveImuTestDataToCsv(macAddress, result);
                    showlog("六轴校准保存完毕");
                    showlog("六轴校准结束");
                    isimuCaliContinue = false;  // 结束
                    ui->imuCaliButton->setEnabled(true);
                    state = STATE_IDLE;
                }
                if (result == failValue) {
                    saveImuTestDataToCsv(macAddress, result);
                    showlog("六轴校准保存完毕");
                    showlog("六轴校准结束");
                    isimuCaliContinue = false;  // 结束
                    ui->imuCaliButton->setEnabled(true);
                    state = STATE_IDLE;
                }
                break;
            default: break;
        }
        QCoreApplication::processEvents();
    }
}

void MainWindow::on_buruing1_clicked() {
    static int i;
    if (i) {
        i = 0;
        pb->set_burning_mode(5, FacSwitch_OPEN);
    } else {
        i = 1;
        pb->set_burning_mode(5, FacSwitch_CLOSE);
    }
}

void MainWindow::on_get_device_sn_clicked() { pb->get_sn(FacDevInfoType_TAIL_SN); }

void MainWindow::on_close_imu_collect_clicked() { pb->set_imu_collect_param(FacSwitch_STOP); }

void MainWindow::on_open_imu_collect_clicked() { pb->set_imu_collect_param(FacSwitch_START); }

void MainWindow::on_motor_cali_clicked() {
    QMessageBox::StandardButton reply;
    is_motor_continue = true;
    motorstate = STATE_IDLE;
    uint32_t value;
    while (is_motor_continue) {
        switch (motorstate) {
            case STATE_IDLE:  // 复位一切
                showlog("开始测试");
                pb->reset_all_pb();
                at->resetConnected();
                // refreshBleState(0);
                stringsn = "";
                motorresult = passValue;
                motorstate = STATE_WATI_CONNECT;
                break;
            case STATE_WATI_CONNECT:
                if (at->getConnected()) {
                    motorstate = STATE_DISABLE_SLEEP_1;
                    showlog("蓝牙已连接");
                }
                break;

            case STATE_DISABLE_SLEEP_1:
                if (pb->getDisableSleep()) {
                    showlog("已进入禁止休眠模式");

                    on_getBasicInfoButton_clicked();
                    waitWork(100);
                    on_getperipheralButton_clicked();
                    waitWork(100);

                    if (ui->is_sero_motor->checkState()) {
                        bool ok = false;
                        value = ui->pcba_motor_cali_param->text().mid(0, 8).toUInt(&ok, 16);  // 将十六进制字符串转换为
                        if (ok) {
                            qDebug() << value;  // 输出 38822029，即十六进制字符串
                                                // "003bdf6d" 对应的数值
                        } else {
                            qDebug() << "Invalid input string";
                        }

                        pb->set_motor_cali_result_param(value);

                        motorstate = MOTOR_CALI_DATA_SET;

                    } else {
                        pb->is_motor_cali_data_set = 1;
                        motorstate = MOTOR_CALI_DATA_SET;
                    }

                } else {
                    waitWork(500);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    showlog("已重发禁止休眠");
                }
                break;

            case MOTOR_CALI_DATA_SET:
                if (pb->get_is_motor_cali_data_set()) {
                    if (ui->is_test_camera->checkState()) {
                        motorstate = CAMERA_TEST;
                        pb->set_screen_camera_state(1);
                    } else {
                        if (ui->is_sero_motor->checkState()) {
                            pb->set_sevor_motor_param(14, 12, 5.2, 190);
                        } else {
                            pb->set_motor_param(270, 60);
                            pb->set_motor_state(1);
                        }
                        showlog("已经发送电机测试指令");
                        motorstate = MOTOR_TESTING;
                    }
                } else {
                    pb->set_motor_cali_result_param(value);
                    showlog("已重发电机校准参数");
                    waitWork(500);
                }
                break;

            case CAMERA_TEST:
                waitWork(500);
                if (pb->getis_camera_control()) {
                    reply =
                        QMessageBox::question(this, "摄像头测试", "摄像头正常吗？", QMessageBox::Yes | QMessageBox::No);
                    if (reply == QMessageBox::No) {
                        motorresult = failValue;
                    }
                    showlog("已经发送电机测试指令");
                    if (ui->is_sero_motor->checkState()) {
                        pb->set_sevor_motor_param(14, 12, 5.2, 190);
                    } else {
                        pb->set_motor_param(270, 60);
                        pb->set_motor_state(1);
                    }
                    motorstate = MOTOR_TESTING;
                } else {
                    waitWork(500);

                    pb->set_screen_camera_state(1);
                    showlog("已重发开启摄像头");
                }
                break;
            case MOTOR_TESTING:
                if (pb->get_is_motor_param_set() || pb->get_is_motor_test_state()) {
                    reply = QMessageBox::question(this, "电机测试", "电机正常吗？", QMessageBox::Yes | QMessageBox::No);
                    if (reply == QMessageBox::No) {
                        motorresult = failValue;
                    }
                    motorstate = STATE_SAVE_RESULT;
                } else {
                    waitWork(500);
                    if (ui->is_sero_motor->checkState()) {
                        pb->set_sevor_motor_param(14, 12, 5.2, 190);
                    } else {
                        pb->set_motor_param(270, 60);
                        pb->set_motor_state(1);
                    }
                }
                break;

            case STATE_SAVE_RESULT:

                if (motorresult == passValue) {
                    QString mesresult = "PASS";
                    save_motor_to_csv(stringsn, macAddress, mesresult);
                    QString itemvalue = QString("|MOTOR_TEST:PASS|");
                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet("font-size: 33px; background-color: #00FF00; color: black; "
                                                   "border: 2px solid #00FF00; border-radius: 10px; padding: "
                                                   "10px; text-align: center;");
                } else if (motorresult == failValue) {
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; "
                                                   "border: 2px solid #FF0000; border-radius: 10px; padding: "
                                                   "10px; text-align: center; ");
                }

                waitWork(500);

                if (ui->is_sero_motor->checkState()) {
                    pb->set_sevor_motor_param(0, 0, 0, 0);
                } else {
                    pb->set_motor_state(0);
                }

                waitWork(500);
                pb->set_sleeep(FacSwitch_OPEN);
                waitWork(500);
                pb->set_sleeep(FacSwitch_OPEN);
                waitWork(500);
                pb->set_sleeep(FacSwitch_OPEN);
                stringsn = "";
                ui->macInput->clear();
                // ui->macInput->setFocus();
                waitWork(500);
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                waitWork(50);
                on_disconnectButton_clicked();
                showlog("测试结束");
                is_motor_continue = false;

                motorstate = STATE_IDLE;
                break;
        }

        QCoreApplication::processEvents();
    }
}

void MainWindow::on_bleTestPushButton_clicked() {
    isWifiOtaContinue = true;
    ui->bleTestPushButton->setEnabled(false);
    ui->configWifiPushButton->setEnabled(false);
    ui->otaTestPushButton->setEnabled(false);
    int times = 0;

    while (isWifiOtaContinue) {
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
            at->sendotaMac(ui->macInput->text());
        }

        waitWork(ui->testPeriodSpin->value() * 1000 / 2);

        at->sendMac("00:00:00:00:00:00");  // 发送mac地址

        waitWork(ui->testPeriodSpin->value() * 1000);

        appendAndSaveWifiOtaLog(QString(""));
        appendAndSaveWifiOtaLog(QString("%1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
        appendAndSaveWifiOtaLog(QString("test times:%1").arg(times++));
    }

    ui->bleTestPushButton->setEnabled(true);
    ui->configWifiPushButton->setEnabled(true);
    ui->otaTestPushButton->setEnabled(true);
}

void MainWindow::on_stopTestPushButton_clicked() {
    isWifiOtaContinue = false;
    ui->bleTestPushButton->setEnabled(true);
    ui->configWifiPushButton->setEnabled(true);
    ui->otaTestPushButton->setEnabled(true);
}
//压测接口
void MainWindow::on_otaTestPushButton_clicked() {
    isWifiOtaContinue = true;
    ui->bleTestPushButton->setEnabled(false);
    ui->configWifiPushButton->setEnabled(false);
    ui->otaTestPushButton->setEnabled(false);
    ui->otaTestPushButton_2->setEnabled(false);

    bool finish = false;
    QTime timeout;
    QTime totalTime;
    bool refresh = false;
    bool result = false;
    bool isStart = false;
    at->resetConnected();
    at->sendotaMac(ui->wifiOtaMacInput->text());
    pb->setPbMode(0);
    timeout.start();

    while (at->getConnected() == false) {
        waitWork(100);
        if (timeout.elapsed() > 1000 * 30) {
            appendAndSaveWifiOtaLog("otaTestPushButton蓝牙连接超时");
            isWifiOtaContinue = false;
        }
        if (isWifiOtaContinue == false)
            break;
    }
    waitWork(1000);
    ui->progressBar->show();
    ui->progressBar->setMaximum(100);

    connect(pb, &Qpb::send_ota_progress, [&](int s) {
        ui->progressBar->show();
        ui->progressBar->setMaximum(100);
        ui->progressBar->setValue(s);
        refresh = true;
        if (s == 100) {
            appendAndSaveWifiOtaLog(QString("ota time:%1s").arg(totalTime.elapsed() / 1000));
            finish = true;
        }
    });
    connect(pb, SIGNAL(send_pb_info(QString)), ui->testMsg, SLOT(appendPlainText(QString)));
    connect(pb, &Qpb::send_ota_progress, [&](int s) {
        ui->progressBar->setValue(s);
        refresh = true;
    });
    connect(pb, &Qpb::send_ota_result, [&](int r) {
        finish = true;

        if (r == 11 || r == 0) {
            result = true;
        }
        if (r == 0) {
            isStart = true;
        }
    });

    while (isWifiOtaContinue) {
        if (!dongleSerialPort->isOpen()) {
            on_connectButton_clicked();
        }
        ui->progressBar->setValue(0);
        RotasFileStatusReq RotasFiledata;
        RotasFiledata.fileType = RotasUpdateFile_WIFI_FIRMWARE;
        pb->set_start_ota_app(RotasFiledata);
        timeout.restart();
        isStart = false;

        appendAndSaveWifiOtaLog("启动OTA，开始计时");
        int counter = 0;
        while (timeout.elapsed() < 30 * 3000) {
            if (counter % 2 == 0) {
                RotasFileStatusReq RotasFiledata;
                RotasFiledata.fileType = RotasUpdateFile_WIFI_FIRMWARE;
                pb->set_start_ota_app(RotasFiledata);
            }
            if (isStart == true) {
                break;
            }
            waitWork(1000);
            counter++;
            if (isWifiOtaContinue == false)
                break;
        }
        if (isStart == false) {
            isWifiOtaContinue = false;
            appendAndSaveWifiOtaLog("设备无法接收蓝牙指令");
        }

        totalTime.start();
        finish = false;
        result = false;

        while (finish == false) {
            if (timeout.elapsed() > 3 * 60 * 1000) {
                appendAndSaveWifiOtaLog("下载超时");
                finish = true;
                break;
            }
            waitWork(200);
            if (isWifiOtaContinue == false)
                break;
            if (refresh) {
                refresh = false;
                timeout.restart();
            }
            if (at->getConnected() == false && timeout.elapsed() > 10000) {
                finish = true;
            }
            ui->lcdNumber->display(totalTime.elapsed() / 1000);
        }

        appendAndSaveWifiOtaLog(QString("升级次数:%1").arg(otaTesttimes++));
        appendAndSaveWifiOtaLog(QString("耗时:%1 s").arg(totalTime.elapsed() / 1000));
        if (result == false) {
            appendAndSaveWifiOtaLog("升级失败");
        } else {
            appendAndSaveWifiOtaLog("升级成功");
        }

        waitWork(ui->testPeriodSpin->value() * 1000);
        if (at->getConnected() == false) {
            timeout.restart();
            while (at->getConnected() == false) {
                waitWork(3000);
                at->getConnected();
                if (timeout.elapsed() > 1000 * 60 * 2) {
                    appendAndSaveWifiOtaLog("otaTestPushButton蓝牙连接超时");
                    isWifiOtaContinue = false;
                }

                if (isWifiOtaContinue == false)
                    break;
            }
        }
    }
    ui->otaTestPushButton_2->setEnabled(true);
    ui->progressBar->hide();
    ui->bleTestPushButton->setEnabled(true);
    ui->configWifiPushButton->setEnabled(true);
    ui->otaTestPushButton->setEnabled(true);
}

void MainWindow::on_configWifiPushButton_clicked() {}
void MainWindow::on_end_motor_cali_clicked() {
    qDebug() << "on_end_motor_cali_clicked";
    is_motor_continue = false;
    motorstate = STATE_IDLE;
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
}
void MainWindow::on_getInfoPushButton_clicked() { pb->get_connect_info(); }

void MainWindow::on_start_scan_clicked() {
    qDebug() << "on_start_scan_clicked";
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    ui->mac_combo->clear();
    deviceMap.clear();
    on_motor_cali_clicked();
    showlog("已经触发");
}

void MainWindow::on_enterwhitemode_clicked() { pb->set_device_mode(4); }

void MainWindow::on_close_camera_clicked() { pb->set_camera_state(0); }

void MainWindow::on_open_camera_clicked() { pb->set_camera_state(1); }

void MainWindow::on_open_camear_light_clicked() { pb->set_camera_light_state(1); }

void MainWindow::on_close_camear_light_clicked() { pb->set_camera_light_state(0); }

void MainWindow::on_exposure_time_edit_returnPressed() {
    pb->set_camera_exposure_time(ui->exposure_time_edit->text().toUInt());
}

void MainWindow::on_sweeping_angle_returnPressed() {
    qDebug() << "sweeping_angle" << ui->sweeping_angle->text().toFloat();
    qDebug() << "vibrate_freq" << ui->vibrate_freq->text().toFloat();

    pb->set_sevor_motor_param(ui->sweeping_angle->text().toUInt(), ui->vibrate_angle->text().toFloat(),
                              ui->sweeping_freq->text().toFloat(), ui->vibrate_freq->text().toUInt());
}
//单次ota接口
void MainWindow::on_otaTestPushButton_2_clicked() {
    QTime timeout;

    if (!ui->is_wifiota_press->checkState())
        ui->wifiOtaMacInput->clear();

    appendAndSaveWifiOtaLog(QDateTime::currentDateTime().toString(Qt::ISODate) + "OTA启动，开始计时");
    totalwifiOtaTime.start();
    timeout.start();
    ui->wifiotaprogress->setValue(0);
    RotasFileStatusReq RotasFiledata;
    RotasFiledata.fileType = RotasUpdateFile_WIFI_FIRMWARE;
    pb->set_start_ota_app(RotasFiledata);
    isWifiOtaContinue = true;
    while (isWifiOtaContinue) {
        if (timeout.elapsed() > 5 * 60 * 1000) {  //下载超时退出
            appendAndSaveWifiOtaLog("下载超时");
            break;
        }
        if (at->getConnected() == false) {  //蓝牙断开退出
            appendAndSaveWifiOtaLog("蓝牙断开退出");
            break;
        }

        ui->lcdNumber->display(totalwifiOtaTime.elapsed() / 1000);
        waitWork(200);
    }
}

void MainWindow::on_configWifiPushButton_2_clicked() {
    ui->configWifiPushButton_2->setEnabled(false);
    QTime configWifitimeout;
    // 更新license
    ProductLicense& license = ProductLicense::getInstance();
    ProductLicense& Testlicense = ProductLicense::getTestInstance();
    ProductLicense& Csvlicense = ProductLicense::getTestInstance();
    ProductLicense& USAlicense = ProductLicense::getTestInstance();
    ProductLicense& USAProductlicense = ProductLicense::getTestInstance();
    ProductLicense& Cloudelicense = ProductLicense::getTestInstance();
    WifiInfo info;
    QString name = ui->wifiUserName_2->text();
    QString password = ui->wifiPassword_2->text();
    QString deviceSecret;
    QString deviceName;
    QString wifiOtaProductName;
    QString iotUrl;

    if (OTAGroup->checkedId() == 0)  // 生产版本
    {
        LicensePair pair;
        if (ui->cloud_key->isChecked())
            pair = Cloudelicense.getCloudLicense("product", "k0fylisHJpn", macAddress);
        else
            pair = license.getLicense();

        appendAndSaveWifiOtaLog("");
        appendAndSaveWifiOtaLog("正在运行生产版本OTA，密钥号：" + QString::number(license.counter));
        wifiOtaProductName = pair.product_name;
        deviceName = pair.device_key;
        deviceSecret = pair.device_secret;
        iotUrl = ui->iot_url_product->text();

    } else if (OTAGroup->checkedId() == 1)  // 测试版本
    {
        if (ui->ota_secret->text() != "usmile123") {
            QMessageBox::warning(NULL, "警告", "密码错误\r\n");
            return;
        }

        LicensePair pair;
        if (ui->cloud_key->isChecked())
            pair = Cloudelicense.getCloudLicense("test", "k0fylgHIxtf", macAddress);
        else
            pair = Testlicense.getTestLicense();

        appendAndSaveWifiOtaLog("");
        appendAndSaveWifiOtaLog("开始运行测试版本OTA，密钥号：" + QString::number(Testlicense.testcounter));
        wifiOtaProductName = pair.product_name;
        deviceName = pair.device_key;
        deviceSecret = pair.device_secret;
        iotUrl = ui->iot_url_test->text();

    } else if (OTAGroup->checkedId() == 2) {
        LicensePair csvpair = Csvlicense.getNextOtaDevice();
        appendAndSaveWifiOtaLog("");
        appendAndSaveWifiOtaLog("正在运行csv文件OTA，密钥号：" + QString::number(Csvlicense.otaDeviceIndex));
        wifiOtaProductName = csvpair.product_name;
        deviceName = csvpair.device_key;
        deviceSecret = csvpair.device_secret;
        iotUrl = ui->iot_url_test->text();

    } else if (OTAGroup->checkedId() == 3) {
        LicensePair pair;
        if (ui->cloud_key->isChecked())
            pair = Cloudelicense.getCloudLicense("usaTest", "k0fylgHIxtf", macAddress);
        else
            pair = USAlicense.getUsaLicense();

        appendAndSaveWifiOtaLog("");
        appendAndSaveWifiOtaLog("正在运行北美测试OTA，密钥号：" + QString::number(USAlicense.usacounter));
        wifiOtaProductName = pair.product_name;
        deviceName = pair.device_key;
        deviceSecret = pair.device_secret;
        iotUrl = ui->iot_url_america->text();
        pb->NEEDAES = 1;

    } else if (OTAGroup->checkedId() == 4) {
        LicensePair pair;
        if (ui->cloud_key->isChecked())
            pair = Cloudelicense.getCloudLicense("usaProduct", "k0fylgHIxtf", macAddress);
        else
            pair = USAProductlicense.getProUsaLicense();

        appendAndSaveWifiOtaLog("");
        appendAndSaveWifiOtaLog("正在运行北美生产OTA，密钥号：" + QString::number(USAProductlicense.prousacounter));
        wifiOtaProductName = pair.product_name;
        deviceName = pair.device_key;
        deviceSecret = pair.device_secret;
        iotUrl = ui->iot_url_america_product->text();
        pb->NEEDAES = 1;
    }

    else {
        QMessageBox::warning(NULL, "警告", " 请选择ota环境\r\n");
        return;
    }
    waitWork(1000);
    at->resetConnected();
    at->sendbleMac(ui->wifiOtaMacInput->text());
    pb->setPbMode(0);  // app
    configWifitimeout.start();
    isWifiOtaContinue = true;
    while (at->getConnected() == false) {
        waitWork(100);
        if (configWifitimeout.elapsed() > 1000 * 10) {
            configWifitimeout.start();
            appendAndSaveWifiOtaLog("configWifiPushButton_2蓝牙连接超时");
            ui->log->clear();
            on_disconnectButton_clicked();
            if (!dongleSerialPort->isOpen()) {
                on_connectButton_clicked();
            }
            waitWork(500);

            at->sendbleMac(ui->wifiOtaMacInput->text());
        }
        if (isWifiOtaContinue == false)
            break;
    }

    if (isWifiOtaContinue == false) {
        ui->configWifiPushButton_2->setEnabled(true);
        return;
    }
    if (deviceSecret == "") {
        isWifiOtaContinue = false;
        QMessageBox::warning(NULL, "警告", " 获取密钥失败\r\n检查网络是否被（王一）配置好\r\n");
        return;
    }

    ui->productKey->setText(wifiOtaProductName);
    ui->deviceName->setText(deviceName);
    ui->deviceSecret->setText(deviceSecret);
    memset(&info, 0, sizeof(info));
    memcpy(info.wifi_name, name.toUtf8(), name.size());
    memcpy(info.wifi_password, password.toUtf8(), password.size());
    memcpy(info.product_key, wifiOtaProductName.toUtf8(), wifiOtaProductName.size());
    memcpy(info.device_name, deviceName.toUtf8(), deviceName.size());
    memcpy(info.device_secret, deviceSecret.toUtf8(), deviceSecret.size());
    memcpy(info.iot_url, iotUrl.toUtf8(), iotUrl.size());

    QString msg = QString("发送升级的信息：\n"
                          "------------------------------------\n"
                          "📶 WiFi 名称    : %1\n"
                          "🔑 WiFi 密码    : %2\n"
                          "🏷️ Product Key  : %3\n"
                          "📛 Device Name  : %4\n"
                          "🔒 Device Secret : %5\n"
                          "🌐 IoT URL       : %6\n"
                          "------------------------------------")
                      .arg(name)
                      .arg(password)
                      .arg(wifiOtaProductName)
                      .arg(deviceName)
                      .arg(deviceSecret)
                      .arg(iotUrl);
    appendAndSaveWifiOtaLog(QDateTime::currentDateTime().toString(Qt::ISODate) + msg);

    pb->set_config_network_app(info);
    appendAndSaveWifiOtaLog("已配置网络");
    // appendAndSaveWifiOtaLog(name+password);

    ui->configWifiPushButton_2->setEnabled(true);
}

void MainWindow::on_motor_cali_param_returnPressed() {
    bool ok = false;
    uint32_t value = ui->motor_cali_param->text().mid(0, 8).toUInt(&ok, 16);  // 将十六进制字符串转换为
    if (ok) {
        qDebug() << value;  // 输出 38822029，即十六进制字符串 "003bdf6d" 对应的数值
    } else {
        qDebug() << "Invalid input string";
    }
    pb->set_motor_cali_result_param(value);
}

void MainWindow::on_start_brush_clicked() { pb->set_brush_control(1); }

void MainWindow::on_open_press_collect_clicked() {
    isfirstsavedata = 1;
    pb->set_press_collect_param(FacSwitch_START);
}

void MainWindow::on_close_press_collect_clicked() { pb->set_press_collect_param(FacSwitch_CLOSE); }
void MainWindow::on_app_connect_clicked() { at->sendotaMac(ui->wifiOtaMacInput->text()); }
void MainWindow::on_wifiOtaMacInput_returnPressed() {
    on_disconnectButton_clicked();
    // on_macInput_returnPressed();
    // ui->testMsg->clear();
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->wifiOtaMacInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->wifiOtaMacInput->text();
        macLabel->setText("蓝牙mac: " + macAddress);
        pb->setPbMode(0);
        on_configWifiPushButton_2_clicked();
        if (isWifiOtaContinue == true) {
            on_otaTestPushButton_2_clicked();
        }
    }
}

void MainWindow::on_test_cali_clicked() {
    static int clickStep = 1;  // 用于跟踪当前运行的步骤
    switch (clickStep) {
        case 1:
            pb->set_motor_cali(1);
            showlog("已发送霍尔校准");
            break;

        case 2:
            pb->set_motor_cali(2);
            showlog("已发送零点校准");
            break;
    }

    clickStep++;  // 增加步骤计数

    // 如果步骤计数超过了最大步骤数，重置为第一步
    if (clickStep > 2) {
        clickStep = 1;
    }
}

void MainWindow::on_open_motor_test_clicked() { pb->set_motor_test_state(1); }

void MainWindow::on_close_motor_test_clicked() { pb->set_motor_test_state(0); }

void MainWindow::on_start_local_ota_clicked() {
    ui->local_ota_result->setText("OTA");
    ui->local_ota_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                        "border-radius: 10px; padding: 10px; text-align: center; ");

    on_connectwifi_clicked();
    waitWork(WAITTIME);
    int count = 0;
    std::map<QString, int> version_map{{"固件", 201}, {"资源", 202}, {"音频", 303}, {"主题", 304}, {"音乐文件", 305}};

    local_ota_data pack[2];
    for (int i = 0; i < 2; i++) {
        QString file_type = findChild<QComboBox*>(QString("version_type_%1").arg(i + 1))->currentText();
        QString file_size = findChild<QLineEdit*>(QString("file_size_%1").arg(i + 1))->text();
        QString version_type = findChild<QLineEdit*>(QString("version_type_%1").arg(i + 3))->text();
        QString url = findChild<QLineEdit*>(QString("url_%1").arg(i + 1))->text();
        QString md5 = findChild<QLineEdit*>(QString("md5_%1").arg(i + 1))->text();
        int check_ota_set = findChild<QCheckBox*>(QString("is_ota_%1").arg(i + 1))->checkState();

        if (!file_type.isEmpty() && !file_size.isEmpty() && !version_type.isEmpty() && !url.isEmpty() &&
            !md5.isEmpty() && check_ota_set) {
            pack[count].is_have_data = 1;
            pack[count].file_size = file_size.toInt();
            pack[count].version_code = version_type.toInt();
            pack[count].version_type = version_map[file_type];  // 这里假定file_type总能在version_map中找到对应项

            pack[count].url = url.toUtf8();
            pack[count].md5 = md5.toUtf8();
            count++;
        } else {
            showlog("没填完整信息");
        }
    }

    if (count > 0) {
        pb->set_local_ota(pack);  // 假设这里的方法参数是local_ota_data数组
    }
}

void MainWindow::on_new_connectwifi_clicked() {
    QString wifiName = SETTINGS.value("WIFI/Name").toString();
    QString wifiPassword = SETTINGS.value("WIFI/Password").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected()) {
        pb->set_new_connect_wifi(wifiNameBytes, wifiPasswordBytes, wifiPasswordBytes, wifiPassword);
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_get_imu_info_clicked() { pb->get_imu_cali_result(); }

void MainWindow::on_swing_test_clicked() {
    if (at->getConnected()) {
        if (SETTINGS.value("SYSTEM/uperMotor").toBool()) {
            showlog("跑的是P30P");
            pb->set_sevor_motor_param(3500, 14000, 10, 380);
        } else {
            pb->set_sevor_motor_param(14, 12, 5.2, 190);
        }

        showlog("已经设置摆幅测试");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_light_test_clicked() {
    if (1)  //(at->getConnected())
    {
        FacLedControl data;

        // Retrieve RGB values from line edits for LED 1
        data.led_state[0].R = ui->R1->value();
        data.led_state[0].G = ui->G1->value();
        data.led_state[0].B = ui->B1->value();
        data.led_state[0].index = LedPosition_led_left_up;

        // Retrieve RGB values from line edits for LED 2
        data.led_state[1].R = ui->R2->value();
        data.led_state[1].G = ui->G2->value();
        data.led_state[1].B = ui->B2->value();
        data.led_state[1].index = LedPosition_led_right_up;

        // Retrieve RGB values from line edits for LED 3
        data.led_state[2].R = ui->R3->value();
        data.led_state[2].G = ui->G3->value();
        data.led_state[2].B = ui->B3->value();
        data.led_state[2].index = LedPosition_led_left_down;

        // Retrieve RGB values from line edits for LED 4
        data.led_state[3].R = ui->R4->value();
        data.led_state[3].G = ui->G4->value();
        data.led_state[3].B = ui->B4->value();
        data.led_state[3].index = LedPosition_led_right_down;

        pb->set_rgb_color(data);
        showlog("已经设置RGB测试");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_start_search_clicked() {
    qDebug() << "on_start_search_clicked";
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    ui->pick_device->clear();
    deviceMap.clear();
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
}

void MainWindow::on_pick_device_textActivated(const QString& arg1) {
    ui->log->clear();
    ui->msgEdit->clear();
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        pb->NEEDAES=0;
        macAddress = arg1;
        at->sendMac(macAddress);  // 发送mac地址
    }
}

void MainWindow::on_open_motor_cali_clicked() { pb->set_motor_cali_state(1); }

void MainWindow::on_close_motor_cali_clicked() { pb->set_motor_cali_state(0); }

void MainWindow::on_R1_valueChanged(int value) {
    ui->label_46->setText(QString::number(value));

    refreshColor1();
}

void MainWindow::on_G1_valueChanged(int value) {
    ui->label_47->setText(QString::number(value));

    refreshColor1();
}

void MainWindow::on_B1_valueChanged(int value) {
    ui->label_48->setText(QString::number(value));

    refreshColor1();
}

void MainWindow::on_R2_valueChanged(int value) {
    ui->label_62->setText(QString::number(value));
    refreshColor2();
}

void MainWindow::on_G2_valueChanged(int value) {
    ui->label_63->setText(QString::number(value));
    refreshColor2();
}

void MainWindow::on_B2_valueChanged(int value) {
    ui->label_64->setText(QString::number(value));
    refreshColor2();
}

void MainWindow::on_R3_valueChanged(int value) {
    ui->label_65->setText(QString::number(value));
    refreshColor3();
}

void MainWindow::on_G3_valueChanged(int value) {
    ui->label_66->setText(QString::number(value));
    refreshColor3();
}

void MainWindow::on_B3_valueChanged(int value) {
    ui->label_67->setText(QString::number(value));
    refreshColor3();
}

void MainWindow::on_R4_valueChanged(int value) {
    ui->label_68->setText(QString::number(value));
    refreshColor4();
}

void MainWindow::on_G4_valueChanged(int value) {
    ui->label_69->setText(QString::number(value));
    refreshColor4();
}

void MainWindow::on_B4_valueChanged(int value) {
    ui->label_70->setText(QString::number(value));
    refreshColor4();
}

void MainWindow::on_pink_led_clicked() {
    static int turn = 1;
    if (turn) {
        pb->set_led_color(1, 2);
        turn = 0;
    } else {
        pb->set_led_color(0, 2);
        turn = 1;
    }
}

void MainWindow::on_white_led_clicked() {
    static int turn = 1;
    if (turn) {
        pb->set_led_color(1, 1);
        turn = 0;
    } else {
        pb->set_led_color(0, 1);
        turn = 1;
    }
}

void MainWindow::on_get_motor_log_clicked() {
    pb->set_motor_cali_state(1);
    pb->set_motor_cali_state(0);
    // 开关后日志就打开了
}

void MainWindow::on_set_imu_info_clicked() {
    calData.kx = ui->kx->text().toFloat();
    calData.ky = ui->ky->text().toFloat();
    calData.kz = ui->kz->text().toFloat();
    calData.syx = ui->syx->text().toFloat();
    calData.szx = ui->szx->text().toFloat();
    calData.szy = ui->szy->text().toFloat();
    calData.bx = ui->bx->text().toFloat();
    calData.by = ui->by->text().toFloat();
    calData.bz = ui->bz->text().toFloat();

    calData.gyro_offset[0] = ui->gyro_x_2->text().toFloat();
    calData.gyro_offset[1] = ui->gyro_y_2->text().toFloat();
    calData.gyro_offset[2] = ui->gyro_z_2->text().toFloat();

    pb->set_new_imu_cali_result(calData);
}

void MainWindow::on_calculate_returnPressed() {
    // 定义用于保存数据的数组
    CalibData csv_data[9];
    for (int x = 0; x < 9; ++x) {
        // 重置示例数据
        csv_data[x].data[0] = 0;
        csv_data[x].data[1] = 0;
        csv_data[x].data[2] = 0;
    }
    int i;
    // 打开matched_file.csv文件
    QFile file("matched_file.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Failed to open file:" << file.errorString();
        return;
    }

    QTextStream in(&file);

    // 读取文件内容，并按行分割
    while (!in.atEnd()) {
        QString line = in.readLine();
        // 使用逗号分割每一行的数据
        QStringList fields = line.split(',');
        if (fields.size() < 5) {
            qDebug() << "Invalid CSV format";
            continue;
        }

        // 提取第二列的MAC地址
        QString mac = fields[1];
        if (mac.isEmpty()) {
            qDebug() << "Empty MAC address";
            continue;
        }

        // 如果csvmac为空，表示第一次提取到MAC地址，将其赋值给csvmac
        if (csvmac.isEmpty()) {
            csvmac = mac;
        }

        // 如果mac与csvmac不相等，表示已经处理完所有与csvmac相同的行，需要将数据填入数组并执行相应函数
        if (mac != csvmac && i == 8) {
            nqimuc->acccalib_sensors_init();
            // 执行函数
            nqimuc->add_csvdata(csv_data);
            nqimuc->acccalib_sensors_task();

            // 更新csvmac
            csvmac = mac;
            for (int x = 0; x < 9; ++x) {
                // 重置示例数据
                csv_data[x].data[0] = 0;
                csv_data[x].data[1] = 0;
                csv_data[x].data[2] = 0;
            }
        }

        // 从第四列的数据中解析出数组索引 i
        QString indexStr = fields[3];
        if (!indexStr.startsWith("data:")) {
            qDebug() << "Invalid index format";
            continue;
        }
        i = indexStr.mid(5).toInt();  // 去除字符串中的"data:"部分，并转换为整数

        // 检查索引 i 是否合法
        if (i < 0 || i > 8) {
            qDebug() << "Invalid index:" << i;
            continue;
        }

        // 解析第五列的数据
        QString dataStr = fields[4];
        // 修改正则表达式以匹配带负号的数字
        QRegularExpression regex("added(-?\\d+\\.\\d+)(-?\\d+\\.\\d+)(-?\\d+\\.\\d+)");

        QRegularExpressionMatch match = regex.match(dataStr);
        if (match.hasMatch()) {
            float data0 = match.captured(1).toFloat();
            float data1 = match.captured(2).toFloat();
            float data2 = match.captured(3).toFloat();
            csv_data[i].data[0] = data0;
            csv_data[i].data[1] = data1;
            csv_data[i].data[2] = data2;
            qDebug() << "数值为" << data0 << data1 << data2;
            qDebug() << "数值为" << csv_data[i].data[0] << csv_data[i].data[1] << csv_data[i].data[2];
        } else {
            qDebug() << "Invalid data format:" << dataStr;
            continue;
        }
    }

    file.close();
}
void MainWindow::on_closeconnect_clicked() { udpSocket->close(); }
void MainWindow::on_distribution_network_clicked() {
    // 获取IP地址
    QString ipString = ui->ip_comboBox->currentText();
    QString localHostName = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(localHostName);

    QString wifiName = ui->ssid_lineEdit->text();
    QString wifiPassword = ui->password_lineEdit->text();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
    // 获取QLineEdit中的端口号文本
    QString portText = ui->port_num->text();
    QHostAddress ipAddress(ipString);

    if (udpSocket->state() == QAbstractSocket::BoundState) {
        qDebug() << "UDP socket is already bound";
        udpSocket->abort();
    } else {
        qDebug() << "UDP socket is not bound";
    }

    int port = portText.toInt();
    if (udpSocket) {
        udpSocket->close();
        delete udpSocket;
        udpSocket = nullptr;  // 避免使用已释放的内存
        udpSocket = new QUdpSocket(this);
    } else {
        udpSocket = new QUdpSocket(this);
    }

    bool bindResult = udpSocket->bind(ipAddress, port);
    qDebug() << "connect_udp,localhost: " << ipString << ":" << port;

    if (!bindResult) {
        showlog("ip绑定失败");
        return;
    }

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));
    if (at->getConnected()) {
        pb->set_new_connect_wifi(wifiNameBytes, wifiPasswordBytes, ipString, ui->port_num->text());
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_save_photo_clicked() {
    if (!viewercamrea->temporarypixmap.isNull()) {
        QPixmap pix = viewercamrea->temporarypixmap;
        // 使用QFileDialog让用户选择保存文件的路径
        QString fileName =
            QFileDialog::getSaveFileName(this, tr("Save File"), "/home", tr("Images (*.png *.xpm *.jpg)"));

        if (!fileName.isEmpty()) {
            pix.save(fileName);  // 保存为用户选择的文件
        }
    } else {
        qDebug() << "Pixmap is empty!";
    }
}

void MainWindow::on_clean_mode_clicked() { pb->set_device_mode(3); }

void MainWindow::on_nfc_write_read_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8];  // 写入数据缓冲区

    // 033BD2023668772001004800324F30450081080027200014178591141035353034303730313233313131313131170102910B0101010A063C842707A8D1
    // 3C842707A8D1
    // 35353034303730313233313131313131
    // 5504070123111111    //5504华为开头0701sku2311年月日1111数量
    QString hexString;
    QString text = ui->nfc_sn->text();

    for (int i = 0; i < text.length(); ++i) {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0'));
    }

    QString nfcdataHeadText;
    if (QString(ui->nfc_combo->currentText()).compare("U7P") == 0) {
        nfcdataHeadText =
            "033BD2023668772001004800324F3130" + getValueBySN(ui->nfc_sn->text()).toUtf8() + "810800272000141785911410";
        showlog("当前nfc写入的是U7P!");
    }
    if (QString(ui->nfc_combo->currentText()).compare("U7") == 0) {
        nfcdataHeadText =
            "033BD2023668772001004800324F3045" + getValueBySN(ui->nfc_sn->text()).toUtf8() + "810800272000141785911410";
        showlog("当前nfc写入的是U7!");
    }

    QString dataText = nfcdataHeadText + hexString + "170102910B0101010A06" + ui->nfc_mac->text().remove(":");

    QByteArray dataBytes = QByteArray::fromHex(dataText.toLatin1());  // 将十六进制字符串转换为字节数组
    int dataSize = dataBytes.size();                                  // 获取字节数组的大小
    qDebug() << "dataSize: " << dataSize << dataText;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = 100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport = parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
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
        if (st == 1)
            showlog("nfc卡识别不到");
        if (st < 0)
            showlog("nfc卡查询失败");
        if ((intptr_t)icdev > 0) {
            st = dc_exit(icdev);
            if (st != 0) {
                showlog("nfc退出失败");

            } else {
                showlog("nfc退出成功!");
                icdev = (HANDLE)-1;
            }
        }
        return;
    } else {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog(QString::fromStdString(str1));
    }

    for (int i = 0; i < dataSize; i += 4) {        // 每次处理8个字节
        int bytesToWrite = qMin(8, dataSize - i);  // 计算本次需要写入的字节数，最多8个

        memcpy(writedata, dataBytes.constData() + i,
               bytesToWrite);  // 将数据复制到写入数据缓冲区

        int ret = dc_write(icdev, 4 + i / 4,
                           writedata);  // 将写入数据缓冲区中的数据写入设备

        if (ret != 0) {
            QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
            qDebug() << "errMsg: " << writedata << errMsg;
        }
    }

    for (int i = 4; i * 4 < dataSize; i += 4) {  // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            showlog("dc_read Error!");
            if ((intptr_t)icdev > 0) {
                st = dc_exit(icdev);
                if (st != 0) {
                    showlog("nfc退出失败");

                } else {
                    showlog("nfc退出成功!");
                    icdev = (HANDLE)-1;
                }
            }
            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            showlog("dc_read Error!");
            if ((intptr_t)icdev > 0) {
                st = dc_exit(icdev);
                if (st != 0) {
                    showlog("nfc退出失败");

                } else {
                    showlog("nfc退出成功!");
                    icdev = (HANDLE)-1;
                }
            }
            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            showlog(QString::fromStdString(str1));
        }
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

void MainWindow::on_clear_nfc_data_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8] = {0x03, 0x00, 0xFE, 0x00};  // 写入数据缓冲区
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";
    int nfcport = 100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport = parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
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
        if ((intptr_t)icdev > 0) {
            st = dc_exit(icdev);
            if (st != 0) {
                showlog("nfc退出失败");

            } else {
                showlog("nfc退出成功!");
                icdev = (HANDLE)-1;
            }
        }
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
        qDebug() << "errMsg: " << writedata << errMsg;
    }

    st = dc_read(icdev, 4, rdata);
    if (st != 0) {
        showlog("dc_read Error!");
        if ((intptr_t)icdev > 0) {
            st = dc_exit(icdev);
            if (st != 0) {
                showlog("nfc退出失败");

            } else {
                showlog("nfc退出成功!");
                icdev = (HANDLE)-1;
            }
        }
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

void MainWindow::on_open_screen_show_camera_clicked() { pb->set_screen_camera_state(1); }

void MainWindow::on_close_screen_show_camera_clicked() { pb->set_screen_camera_state(0); }

void MainWindow::on_open_support_camera_clicked() { pb->set_camera_support_state(1); }

void MainWindow::on_close_support_camera_clicked() { pb->set_camera_support_state(0); }

void MainWindow::on_open_camera_picture_clicked() {
    packetMap.clear();
    faultData.clear();
    cameradatasize = 0;
    dataNumber = 0;
    pb->set_camera_picture_state(1);
    std::memset(dongle_ring_buffer, 0,
                sizeof(dongle_ring_buffer));  // 将数组全部初始化为零
    std::memset(camera_ring_buf, 0,
                sizeof(camera_ring_buf));  // 将数组全部初始化为零
}

void MainWindow::on_close_camera_picture_clicked() { pb->set_camera_picture_state(0); }

void MainWindow::on_nfc_read_clicked()

{
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    QString ReadNfcData = "";
    int dataSize = 61;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = 100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport = parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    if ((intptr_t)icdev <= 0) {
        showlog("初始化nfc接口失败!");
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
        if ((intptr_t)icdev > 0) {
            st = dc_exit(icdev);
            if (st != 0) {
                showlog("nfc退出失败");

            } else {
                showlog("nfc退出成功!");
                icdev = (HANDLE)-1;
            }
        }
        return;
    } else {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 4; i * 4 < dataSize; i += 4) {  // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            if ((intptr_t)icdev > 0) {
                st = dc_exit(icdev);
                if (st != 0) {
                    showlog("nfc退出失败");

                } else {
                    showlog("nfc退出成功!");
                    icdev = (HANDLE)-1;
                }
            }
            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //   showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            showlog("nfc信息读取失败");
            //  showlog("dc_read Error!");
            if ((intptr_t)icdev > 0) {
                st = dc_exit(icdev);
                if (st != 0) {
                    showlog("nfc退出失败");

                } else {
                    showlog("nfc退出成功!");
                    icdev = (HANDLE)-1;
                }
            }
            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //  showlog(QString::fromStdString(str1));
        }
    }
    if ((intptr_t)icdev > 0) {
        st = dc_exit(icdev);
        if (st != 0) {
            showlog("nfc退出失败");

        } else {
            showlog("nfc退出成功!");
            icdev = (HANDLE)-1;
        }
    }
    showlog("nfc信息读取结束");
    showlog("nfc内容为：" + ReadNfcData);
}

void MainWindow::on_nfcComFresh_clicked() { updateHIDComboBox(ui->NfcComboBox); }

void MainWindow::on_nfc_encode_clicked() {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    int nfcport = 100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport = parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    QString nfcName = ui->nfc_name->text();

    // 将QString转换为QByteArray，然后获取其const char *指针
    QByteArray byteArray = nfcName.toLatin1();  // 使用Latin-1编码，确保兼容性
    const char* data = byteArray.constData();

    // 创建一个用于存储数据的缓冲区，并确保长度足够
    unsigned char buffer[8];  // 假设需要写入的数据长度为8字节

    // 将const char *的数据复制到unsigned char *的buffer中
    memcpy(buffer, data, byteArray.length());

    st = dc_swr_eeprom(icdev, 0, 8, buffer);

    if (st != 0) {
        showlog("nfc烧录器写入名字失败");
    } else {
        showlog("nfc烧录器写入名字成功");
    }
}
void MainWindow::on_nfc_decode_clicked() {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];
    int nfcport = 100;
    QStringList parts = ui->NfcComboBox->currentText().split(":");
    if (parts.size() == 2) {
        nfcport = parts[0].toInt();
    }
    icdev = dc_init(nfcport, 115200);
    st = dc_srd_eeprom(icdev, 0, 8, buff_1);
    if (st != 0) {
        showlog("nfc烧录器读取失败");
    } else {
        showlog("nfc烧录器读取成功");
        // 将 buff_1 数组转换为 QString
        QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
        // 输出整个字符串
        qDebug() << "nfc设备为:" << buffStr;
    }
}

void MainWindow::on_get_device_subpid_clicked() { pb->get_sn(FacDevInfoType_SUB_PID); }

void MainWindow::on_get_battery_clicked() {
    if (at->getConnected()) {
        pb->get_battery();
        showlog("正在获取牙刷电量");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void MainWindow::on_get_motor_info_clicked() { pb->get_servo_motor_info(); }

void MainWindow::on_get_board_sn_clicked() { pb->get_sn(FacDevInfoType_BOARD_SN); }

void MainWindow::on_write_device_sn_clicked() {
    QByteArray devicesn = ui->snInput->text().toUtf8();
    pb->set_sn(FacDevInfoType_TAIL_SN, devicesn);
    showlog("已绑定尾盖sn到牙刷");
}

void MainWindow::on_write_board_sn_clicked() {
    QByteArray boardsn = ui->snInput->text().toUtf8();
    pb->set_sn(FacDevInfoType_BOARD_SN, boardsn);
    showlog("已绑定板子sn到牙刷");
}

void MainWindow::on_write_device_subpid_clicked() {
    QByteArray subpid = getValueBySN(ui->snInput->text()).toUtf8();
    if ("SUBPID_ERRO" == subpid) {
        QMessageBox::warning(nullptr, "Warning", "没匹配到subpid");
        return;
    }
    pb->set_sn(FacDevInfoType_SUB_PID, subpid);
    showlog("已绑定subpid到牙刷");
}
// void MainWindow::on_clear_picture_clicked() {
//     // 创建网络访问管理器
//     QNetworkAccessManager* manager = new QNetworkAccessManager(this);

//     // 创建请求
//     QNetworkRequest request;
//     request.setUrl(QUrl(ui->ui_ip->text() + "/clear_spiffs_function"));  // 拼接

//     // 发送 GET 请求
//     QNetworkReply* reply = manager->get(request);

//     // 处理请求完成信号
//     connect(reply, &QNetworkReply::finished, [=]() {
//         // 检查响应状态码
//         if (reply->error() == QNetworkReply::NoError) {
//             qDebug() << "Request succeeded";
//             showlog("图片删除完毕");
//         } else {
//             qDebug() << "Request failed:" << reply->errorString();
//             showlog("图片删除失败");
//         }

//         // 释放资源
//         reply->deleteLater();
//         manager->deleteLater();
//     });
// }

void MainWindow::on_clear_picture_clicked() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/clear_spiffs_function"));

    // 发送 GET 请求
    QNetworkReply* reply = manager->get(request);

    // 创建事件循环
    QEventLoop loop;

    // 连接请求完成信号到事件循环的退出槽
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    // 进入事件循环，等待网络请求完成
    loop.exec();

    // 请求完成后处理结果
    if (reply->error() == QNetworkReply::NoError) {
        qDebug() << "Request succeeded";
        showlog("图片删除完毕");
    } else {
        qDebug() << "Request failed:" << reply->errorString();
        showlog("图片删除失败");
    }

    // 释放资源
    reply->deleteLater();
    manager->deleteLater();
}
void MainWindow::on_up_picture_clicked() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/upPicture_function"));  // 拼接 "/trigger_function"
                                                                      // 到 ESP32 的 IP 地址

    // 发送 GET 请求
    QNetworkReply* reply = manager->get(request);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Request succeeded";
            showlog("向上翻页完成");
        } else {
            qDebug() << "Request failed:" << reply->errorString();
            showlog("向上翻页失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_down_picture_clicked() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/downPicture_function"));  // 拼接
                                                                        // "/trigger_function" 到
                                                                        // ESP32 的 IP 地址

    // 发送 GET 请求
    QNetworkReply* reply = manager->get(request);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Request succeeded";
            showlog("向下翻页完成");
        } else {
            qDebug() << "Request failed:" << reply->errorString();
            showlog("向下翻页失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_play_picture_clicked() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/playPicture_function"));  // 拼接
                                                                        // "/trigger_function" 到
                                                                        // ESP32 的 IP 地址

    // 发送 GET 请求
    QNetworkReply* reply = manager->get(request);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "Request succeeded";
            showlog("播放完成");
        } else {
            qDebug() << "Request failed:" << reply->errorString();
            showlog("播放失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_open_imu_collect_solve_clicked() {
    pb->set_solve_imu_collect_param(FacSwitch_START);
    deleteCsvFile("处理后的6轴IMU性能验证.csv");
    static QTimer* imu_collect_timer;  // 定时器指针作为类成员变量
    // 如果定时器已经存在且正在运行，则断开连接并停止
    if (imu_collect_timer) {
        disconnect(imu_collect_timer, &QTimer::timeout, nullptr, nullptr);
        imu_collect_timer->stop();
        imu_collect_timer->deleteLater();
    }

    // 创建并启动一个新的定时器
    imu_collect_timer = new QTimer(this);
    connect(imu_collect_timer, &QTimer::timeout, this, [=]() {
        // 定时器超时后执行的操作
        pb->set_solve_imu_collect_param(FacSwitch_STOP);  // 停止IMU采集
        showlog("10秒已到，停止IMU采集");

        // 关闭并释放定时器
        imu_collect_timer->stop();
        imu_collect_timer->deleteLater();
        imu_collect_timer = nullptr;  // 释放后将指针置为空
    });

    imu_collect_timer->start(10000);  // 10000 毫秒 = 10 秒
}

void MainWindow::on_py_test_clicked() {
    // python.exe .u7p_camera_defect_detect_env/code/onnx_inference.py --model
    // "./code/infer_240723_320_model.onnx" --img "./code/test.png"
    // python.exe ./code/onnx_inference --model
    // "./code/infer_240723_320_model.onnx" --img "绝对路径"
    //    arguments << "script.py" << QDir::currentPath() +
    //    "/图片存储/脏污正常"<< "--flag";
    // python.exe ./code/onnx_inference.py --model
    // "./code/infer_240723_320_model.onnx" --img "./code/test.png"

    QProcess process;

    // 设置虚拟环境的 Python 可执行文件路径
    QString pythonPath = "./u7p_camera_defect_detect_env/python.exe";

    // 设置要运行的 Python 脚本及其参数
    QString scriptPath = "./code/onnx_inference.py";
    QStringList arguments;
    arguments << "--model"
              << "./code/infer_240723_320_model.onnx"
              << "--img"
              << "./code/test.png";

    // 设置工作目录为虚拟环境目录
    process.setWorkingDirectory("./u7p_camera_defect_detect_env");

    // 启动进程
    process.start(pythonPath, QStringList() << scriptPath << arguments);

    // 等待进程启动
    if (!process.waitForStarted()) {
        qDebug() << "Failed to start process";
    }

    // 等待进程结束
    process.waitForFinished();

    // 获取标准输出和标准错误输出
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    qDebug() << "Output:" << output;
    qDebug() << "Error Output:" << errorOutput;
}

void MainWindow::on_close_imu_collect_solve_clicked() { pb->set_solve_imu_collect_param(FacSwitch_STOP); }

void MainWindow::on_transfer_xls_clicked() {
    QDateTime currentDateTime = QDateTime::currentDateTime();
    // 按照指定格式格式化日期和时间
    QString formattedDateTime = currentDateTime.toString("yyyyMMdd");
    // 构建新的文件名
    QString newcsvFileName =
        QString("xx_%1_s1_t1_g1_people_F_30_160_R_%2.xls").arg(macAddress.remove(':').right(4)).arg(formattedDateTime);

    convertCsvToXls("处理后的6轴IMU性能验证.csv", newcsvFileName);
}

void MainWindow::on_nfc_close_clicked() {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    icdev = dc_init(100, 115200);

    if ((intptr_t)icdev > 0) {
        st = dc_exit(icdev);
        if (st != 0) {
            showlog("nfc退出失败");
        } else {
            showlog("nfc退出成功!");
            icdev = (HANDLE)-1;
        }
    }
}

void MainWindow::on_close_motor_adc_switch_clicked() { pb->set_motor_adc_switch(0); }

void MainWindow::on_open_motor_adc_switch_clicked() { pb->set_motor_adc_switch(1); }

void MainWindow::on_downloadapp_clicked() {
    // QString url = "http://163.177.79.53:16888/versions/Readme.md";  // 替换为实际文件的URL
    // QString fileName = QFileInfo(url).fileName();
    // QString savePath = "./" + fileName;
    // downloadMyApp(url, savePath);

    checkAndUpdateFile();
}

void MainWindow::on_uploadapp_clicked() {
    QDateTime currentDateTime = QDateTime::currentDateTime();

    uploadFile("./new_production_" + currentDateTime.toString("yyyyMMdd") + ".exe",
               "http://163.177.79.53:16888/versions/");
}

void MainWindow::on_delefile_clicked() { deleteFile("http://163.177.79.53:16888/Readme.md"); }

void MainWindow::on_bleotamacInput_returnPressed() {
    on_stopBleOta_clicked();
    on_disconnectButton_clicked();

    if (!ui->is_bleota_press->checkState()) {
        clearDisplay();
    }
    ui->bleotaresult->setText("WAIT");
    ui->bleotaresult->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                    "border-radius: 10px; padding: 10px; text-align: center; ");

    // on_macInput_returnPressed();
    // ui->testMsg->clear();
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->bleotamacInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        waitWork(500);
        at->sendOTADATA(0);
        pb->reset_all_pb();
        macAddress = ui->bleotamacInput->text();
        macLabel->setText("蓝牙mac: " + macAddress);

        at->resetConnected();
        at->sendotaMac(ui->bleotamacInput->text());

        QTime bleOtaTimeConnectOut;
        bleOtaTimeConnectOut.start();
        ui->bleOtaMsg->appendPlainText(" ");
        ui->bleOtaMsg->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate));
        ui->bleOtaMsg->appendPlainText("开始连接蓝牙");
        stopBleOta = 0;  //取消停止ota
        while (at->getConnected() == false) {
            waitWork(100);
            if (bleOtaTimeConnectOut.elapsed() > 1000 * 30) {  //安装需要2分钟
                bleOtaTimeConnectOut.start();
                showlog("蓝牙连接超时,重试连接蓝牙");
                ui->log->clear();
                on_disconnectButton_clicked();
                if (!dongleSerialPort->isOpen()) {
                    on_connectButton_clicked();
                }
                waitWork(500);
                if (at->getConnected())
                    break;
                at->sendotaMac(ui->bleotamacInput->text());
            }
            if (stopBleOta) {
                showlog("停止测试");

                return;  // 停止测试
            }
        }
        pb->setPbMode(1);  //为了区分收到的解包工厂接口
        on_getBasicInfoButton_clicked();
        waitWork(500);
        on_getperipheralButton_clicked();
        waitWork(500);
        pb->setPbMode(0);  //为了区分收到的解包是app接口
        waitWork(3000);
        on_startBleOta_clicked();
    }
}

void MainWindow::on_stopBleOta_clicked() {
    stopBleOta = 1;
    bleotatimer->stop();
    disconnect(bleotatimer, &QTimer::timeout, this, nullptr);  // 断开所有与timeout信号相关的连接
    currentChunk = 0;
}
QString calculateMD5(const QByteArray& fileData) {
    void* ctx = init_md5();
    if (!ctx) {
        qDebug() << "MD5 initialization failed!";
        return QString();
    }

    update_md5(ctx, const_cast<void*>(static_cast<const void*>(fileData.data())), fileData.size());

    uint8_t* md5_result = final_md5(ctx);

    QString md5Hex;
    for (int i = 0; i < 16; i++) {
        md5Hex.append(QString::asprintf("%02x", md5_result[i]));
    }

    return md5Hex;
}
void MainWindow::on_startBleOta_clicked() {
    // 设置定时器间隔
    bool ok;
    int interval = ui->OtaTimeInterval->text().toInt(&ok);
    if (!ok || interval <= 0) {
        QMessageBox::warning(this, "警告", "无效的时间间隔");
        return;
    }
    // 断开之前的定时器连接
    if (bleotatimer->isActive()) {
        bleotatimer->stop();
    }
    disconnect(bleotatimer, &QTimer::timeout, this, nullptr);  // 断开所有与timeout信号相关的连接

    QString filePath = ui->otaFilePath->text();
    if (filePath.isEmpty()) {
        QMessageBox::warning(this, "警告", "未选择固件OTA文件");
        return;
    }

    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "错误", "无法打开固件OTA文件");
        return;
    }
    qDebug() << "当前ota产品为" << connectProductName;
    QByteArray fileData_source;
    if (connectProductName != "U7" && connectProductName != "U7P") {
        if (connectProductName == "Y20PS")
            pb->NEEDAES = 1;
        QString filePath_source = ui->otaFilePath_source->text();
        if (filePath_source.isEmpty()) {
            QMessageBox::warning(this, "警告", "未选择资源OTA文件");
            return;
        }
        QFile file_source(filePath_source);
        if (!file_source.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(this, "错误", "无法打开OTA资源文件");
            return;
        }
        fileData_source = file_source.readAll();
        showlog("资源文件大小为：" + QString::number(fileData_source.size()));
    }

    qDebug() << "文件路径" << filePath;
    QByteArray fileData = file.readAll();
    file.close();
    showlog("文件大小为：" + QString::number(fileData.size()));

    pb->set_uart_receive(1);

    while (!pb->getisSetIamApp()) {
        showlog("已发送假装app!");
        pb->set_i_am_app();  //假装是app
        waitWork(1000);
        if (stopBleOta) {
            showlog("停止测试getisSetIamApp");

            return;  // 停止测试
        }
    }
    showlog("假装app成功");

    if (connectProductName != "U7" && connectProductName != "U7P") {
        QString sourceMd5 = calculateMD5(fileData_source);
        QString FWMd5 = calculateMD5(fileData);

        qDebug() << "固件 1 MD5:" << FWMd5;
        qDebug() << "资源 0 MD5:" << sourceMd5;
        RotasFileStatusReq RotasFiledata[2];
        RotasFiledata[0].fileType = RotasUpdateFile_UI_RESOURCE;
        RotasFiledata[0].file_zip_md5.size = 32;
        // memcpy(RotasFiledata[0].file_zip_md5.bytes, sourceMd5.constData(), 32);
        memcpy(RotasFiledata[0].file_zip_md5.bytes, sourceMd5.toUtf8().constData(), sourceMd5.toUtf8().size());
        RotasFiledata[0].fileSize = fileData_source.size();

        RotasFiledata[1].fileType = RotasUpdateFile_BLE_FIRMWARE;
        RotasFiledata[1].file_zip_md5.size = 32;
        memcpy(RotasFiledata[1].file_zip_md5.bytes, FWMd5.toUtf8().constData(), FWMd5.toUtf8().size());
        RotasFiledata[1].fileSize = fileData.size();

        while (!pb->getisOtaStart()) {
            showlog("已发送开始OTA!");
            pb->set_start_multi_ble_ota_app(RotasFiledata);
            waitWork(2000);
            if (stopBleOta) {
                showlog("停止测试");

                return;  // 停止测试
            }
        }

    } else {
        RotasFileStatusReq RotasFiledata;
        RotasFiledata.fileType = RotasUpdateFile_BLE_FIRMWARE;
        RotasFiledata.fileUnzipSize = fileData.size();
        RotasFiledata.fileSize = fileData.size();

        while (!pb->getisOtaStart()) {
            showlog("已发送开始OTA!");
            pb->set_start_ota_app(RotasFiledata);
            waitWork(2000);
            if (stopBleOta) {
                showlog("停止测试");

                return;  // 停止测试
            }
        }
    }

    showlog("开始OTA!");
    waitWork(1000);
    showlog("开始发送OTA数据通道开启");
    at->sendOTADATA(1);
    showlog("已发送OTA数据通道开启");
    waitWork(1000);
    if (connectProductName != "U7" && connectProductName != "U7P") {
        int totalSize = fileData_source.size();
        int offset = 0;
        int packetSize = 224;
        int depacketSize = 0;
        while (offset < totalSize) {
            // 计算当前包大小（最后一包可能小于300字节）
            int currentSize = qMin(packetSize, totalSize - offset);
            QByteArray packet = fileData_source.mid(offset, currentSize);
            waitWork(interval);
            // 等待允许发送下一包
            if (stopBleOta) {
                showlog("发送资源数据包停止测试");

                return;  // 停止测试
            }
            QByteArray packdata;
            if (connectProductName == "Y20PS")
                packdata = pb->aes256Encrypt(packet);
            else
                packdata = packet;

            // 发送当前分包数据
            dongleSerialPort->write(packdata);
            showlog(QString("发送分包: %1/%2 字节").arg(offset + currentSize).arg(totalSize));
            depacketSize = depacketSize + packdata.size();
            showlog(QString("发送总数%1").arg(depacketSize));

            // 更新偏移量，准备下一包
            offset += currentSize;

            float progressValue = ((float)(offset + currentSize) / totalSize) * 100;
            int progressInt = (int)progressValue;

            emit sendBelSourceOtaSpeed(progressInt);
        }

        showlog("所有数据包发送完成");
    }
    pb->reset_all_pb();
    if (connectProductName != "U7" && connectProductName != "U7P") {
        while (!pb->getisOtaStart()) {  //接收请求可以发送下一包
            showlog("等待手柄请求发送固件包!");

            waitWork(2000);
            if (stopBleOta) {
                showlog("停止测试getisOtaStart");

                return;  // 停止测试
            }
        }
    }

    currentChunk = 0;
    int chunkSize = 224;  // 每包244字节
    int totalOtaSize = fileData.size();
    int numChunks = (totalOtaSize + chunkSize - 1) / chunkSize;  // 计算总包数

    bleotatimer->setInterval(interval);

    bleOtaTestTime.start();
    totalBleSendData = 0;
    // 定义定时器超时处理函数
    connect(bleotatimer, &QTimer::timeout, this, [this, fileData, chunkSize, totalOtaSize, numChunks]() mutable {
        // QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        // showlog("开始发送:" + timestamp);
        ui->bleotalcdtime->display(bleOtaTestTime.elapsed() / 1000);
        int offset = currentChunk * chunkSize;
        int size = qMin(chunkSize, totalOtaSize - offset);

        QByteArray chunk = fileData.mid(offset, size);
        if (currentChunk >= numChunks) {
            showlog("文件发送完毕!");
            qDebug() << "ota write data" << chunk.size() << currentChunk << offset << totalBleSendData;
            bleotatimer->stop();
            disconnect(bleotatimer, &QTimer::timeout, this, nullptr);  // 断开所有与timeout信号相关的连接
            currentChunk = 0;
            return;
        }
        if (connectProductName == "Y20PS")
            dongleSerialPort->write(pb->aes256Encrypt(chunk));
        else
            dongleSerialPort->write(chunk);
        totalBleSendData = chunk.size() + totalBleSendData;
        if (currentChunk % 20 == 0) {
            qDebug() << "ota write data" << chunk.size() << currentChunk << offset << totalBleSendData;
        }
        // timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        // showlog("发送包:" + timestamp);

        currentChunk++;

        float progressValue = ((float)currentChunk / numChunks) * 100;
        int progressInt = (int)progressValue;

        emit sendBelOtaSpeed(progressInt);
    });

    // 启动定时器
    QTimer::singleShot(100, this, [this]() { bleotatimer->start(); });
}

void MainWindow::setBleOtaState(int state) {
    if (state)
        bleotatimer->start(ui->OtaTimeInterval->text().toInt());
    else
        bleotatimer->stop();
}
void MainWindow::on_selectPath_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "选择文件路径");
    if (!path.isEmpty()) {
        ui->otaFilePath->setText(path);
    }
}

void MainWindow::on_ship_bomb_clicked() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    waitWork(1000);
    at->sendBOMB(ui->bombname->text(), ui->bombrssi->text(), ui->bombinterval->text(), "0008021a0408051001e6");

    pb->shipCount = 1;
}

void MainWindow::on_get_noisy_clicked() {
    showlog("开启采集噪音");
    // 如果已经存在定时器，则不创建新的定时器
    if (!noisytimer) {
        noisytimer = new QTimer(this);

        dongleSerialPort->setBaudRate(9600);

        // 定义要发送的数据缓冲区
        static const unsigned char tx_buffer[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0a};

        // 连接定时器的timeout信号到发送数据的lambda函数
        // 在构造函数或初始化函数中
        connect(noisytimer, &QTimer::timeout, this, &MainWindow::sendNoisyData);

        // 启动定时器，每秒触发一次
        noisytimer->start(1000);

        is_need_noisy_data = true;
    }
}
void MainWindow::sendNoisyData() {
    static const unsigned char tx_buffer[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0a};
    dongleSerialPort->write(reinterpret_cast<const char*>(tx_buffer), sizeof(tx_buffer));
}
void MainWindow::on_stop_noisy_clicked() {
    disconnect(noisytimer, &QTimer::timeout, this, &MainWindow::sendNoisyData);

    dongleSerialPort->setBaudRate(921600);

    showlog("停止采集噪音");
    if (noisytimer) {
        noisytimer->stop();
        delete noisytimer;
        noisytimer = nullptr;
    }
    is_need_noisy_data = false;
}

void MainWindow::on_getBackLog_clicked() { pb->get_bursh_backlog(1); }

void MainWindow::on_write_device_skuid_clicked() {
    QByteArray skuid = ui->snInput->text().toUtf8();

    pb->set_sn(FacDevInfoType_SKUID, skuid);
    showlog("已绑定skuid到牙刷");
}

void MainWindow::on_get_device_skuid_clicked() {
    pb->get_sn(FacDevInfoType_SKUID);
    showlog("开始获取skuid");
}

void MainWindow::on_set_hw_ver_clicked() {
    QByteArray hwver = ui->hw_ver->text().toUtf8();
    FacGetDevBaseInfo data;
    // 使用 strncpy 复制字符数组
    qstrncpy(data.hw_version, hwver, sizeof(data.hw_version) - 1);
    // 确保目标数组以 null 终止
    data.hw_version[sizeof(data.hw_version) - 1] = '\0';
    pb->set_base_info(FacBasInfoType_HW_VERSION, data);
}

// void MainWindow::on_set_battery_clicked() {
//     // 枚举定义
//     // typedef enum _FacBatteryType {
//     //     FacBatteryType_TWO_BATTERY = 0,
//     //     FacBatteryType_ONE_BATTERY = 1
//     // } FacBatteryType;

//     // 获取当前选中的枚举值并设置
//     int currentIndex = ui->battery_type_combox->currentIndex();
//     if (currentIndex >= 0) {
//         FacBatteryType selectedType =
//         static_cast<FacBatteryType>(ui->battery_type_combox->itemData(currentIndex).toInt());
//         pb->set_battery(selectedType);
//     }
// }

void MainWindow::on_brush_relocation_clicked() {
    pb->set_motor_damping_state(0);
    QMessageBox::warning(NULL, "警告", " 请把所有刷头置于0位\t\r\n");
    pb->set_motor_cali(2);
}

void MainWindow::on_get_now_music_clicked() { pb->get_now_music_info(); }

void MainWindow::on_send_audio_clicked() { sendPicture(ui->ui_ip->text(), "output/1.bin"); }

void MainWindow::on_audio_volume_valueChanged(int value) { ui->label_94->setText(QString::number(value)); }

void MainWindow::on_is_audio_mode_stateChanged(int arg1) {
    if (arg1) {  //音频的

        ui->high_speed_tp->setText("传输单个音频文件");
        ui->active_picture->setText("批量处理音频文件夹");
        ui->label_95->setText("音频播放倍速");
        ui->label_98->setText("倍");
        ui->send_audio->show();
        ui->audio_volume->show();
        ui->audio_volume->show();
        ui->label_93->show();
        ui->label_94->show();
        ui->play_speed->setText("1");
    }

    else {  // ui的

        ui->high_speed_tp->setText("传输单个图片文件");
        ui->active_picture->setText("批量发送处理图片文件夹");
        ui->label_95->setText("图片播放速度");
        ui->label_98->setText("ms/张");
        ui->play_speed->setText("50");
        ui->send_audio->hide();
        ui->audio_volume->hide();
        ui->label_93->hide();
        ui->label_94->hide();
    }
    qDebug() << "on_is_audio_mode_stateChanged" << arg1;
}

void MainWindow::on_play_speed_returnPressed() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 获取用户输入的SSID和密码
    QString playspeed = ui->play_speed->text();

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/play_speed"));  // 拼接 "/_config" 到 ESP32 的 IP 地址
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // 创建数据
    QByteArray data;
    data.append("playspeed=" + QUrl::toPercentEncoding(playspeed));

    // 发送 POST 请求
    QNetworkReply* reply = manager->post(request, data);

    qDebug() << "data: " << data;

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << " config request succeeded";
            showlog(" 播放速度配置发送成功");
        } else {
            qDebug() << " config request failed:" << reply->errorString();
            showlog(" 播放速度配置发送失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_uipasswordInput_returnPressed() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 获取用户输入的SSID和密码
    QString ssid = ui->uissidInput->text();
    QString password = ui->uipasswordInput->text();

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/wifi_config"));  // 拼接 "/wifi_config" 到 ESP32 的 IP 地址
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // 创建数据
    QByteArray data;
    data.append("ssid=" + QUrl::toPercentEncoding(ssid) + "&");
    data.append("password=" + QUrl::toPercentEncoding(password));

    // 发送 POST 请求
    QNetworkReply* reply = manager->post(request, data);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << "WiFi config request succeeded";
            showlog("WiFi 配置发送成功");
        } else {
            qDebug() << "WiFi config request failed:" << reply->errorString();
            showlog("WiFi 配置发送失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_ui_ypos_returnPressed() {
    // 创建网络访问管理器
    QNetworkAccessManager* manager = new QNetworkAccessManager(this);

    // 获取用户输入的SSID和密码
    QString ypos = ui->ui_ypos->text();

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() + "/y_Pos"));  // 拼接 "/_config" 到 ESP32 的 IP 地址
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    // 创建数据
    QByteArray data;
    data.append("yPos=" + QUrl::toPercentEncoding(ypos));

    // 发送 POST 请求
    QNetworkReply* reply = manager->post(request, data);

    qDebug() << "data: " << data;

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished, [=]() {
        // 检查响应状态码
        if (reply->error() == QNetworkReply::NoError) {
            qDebug() << " config request succeeded";
            showlog(" y轴坐标配置发送成功");
        } else {
            qDebug() << " config request failed:" << reply->errorString();
            showlog(" y轴坐标配置发送失败");
        }

        // 释放资源
        reply->deleteLater();
        manager->deleteLater();
    });
}

void MainWindow::on_get_press_info_clicked() { pb->get_press_cali_result(); }

void MainWindow::on_set_press_info_clicked() {
    qDebug() << "发送校准系数";

    press_calib_data_t cali_result = {0};
    qDebug() << "刷头校准值:" << ui->calibValue_brushHead->text();
    cali_result.calib_factor[MODULE_BTH] = ui->calibValue_brushHead->text().toInt();

    qDebug() << "温度校准值:" << ui->calibValue_temp->text();
    cali_result.temperature[MODULE_BTH] = ui->calibValue_temp->text().toInt();

    qDebug() << "模式按键校准值:" << ui->calibValue_modeButton->text();
    cali_result.calib_factor[MODULE_MODE_BUTTON] = ui->calibValue_modeButton->text().toInt();

    qDebug() << "温度校准值:" << ui->calibValue_temp->text();
    cali_result.temperature[MODULE_MODE_BUTTON] = ui->calibValue_temp->text().toInt();

    qDebug() << "电源按键校准值:" << ui->calibValue_powerButton->text();
    cali_result.calib_factor[MODULE_POWER_BUTTON] = ui->calibValue_powerButton->text().toInt();

    qDebug() << "辅助元件校准值:" << ui->calibValue_auxComponent->text();
    cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] = ui->calibValue_auxComponent->text().toInt();

    pb->set_press_cali_result(cali_result);
}

void MainWindow::on_AITestLine_returnPressed() { sendAiMessage(); }

void MainWindow::on_speakAi_released() {
    ui->speakAi->setText("长按语音输入");
    stopRecording();
    uploadFile("音乐/录音.wav");
}

void MainWindow::on_speakAi_pressed() {
    myAudioRecorde(0);

    ui->speakAi->setText("请说话");  // 设置按钮文本
}

void MainWindow::on_get_botton_state_clicked() { pb->get_button_state(1); }

void MainWindow::on_selectPath_source_clicked() {
    QString path = QFileDialog::getOpenFileName(this, "选择文件路径");
    if (!path.isEmpty()) {
        ui->otaFilePath_source->setText(path);
    }
}

void MainWindow::on_set_mode_returnPressed() {
    showlog("当前发送的模式是" + ui->set_mode->text());

    pb->set_device_mode(ui->set_mode->text().toInt());
}
