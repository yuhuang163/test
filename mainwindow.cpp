#include "mainwindow.h"
#include "ImageWindow.h"
#include "productlicense.h"
#include "qeventloop.h"
#include "ui_mainwindow.h"
#include <QRandomGenerator>

// f4:12:fa:c5:51:c6
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
extern "C"   // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}

void MainWindow::on_pushButton_clicked()
{
    ui->macInput->setText("f4:12:fa:c5:51:c6");
    // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷

    ui->macInput->setText("3C:84:27:07:A8:D2");
    ui->macInput->setText("B4:56:5D:BF:53:66");
    ui->macInput->setText("B4:56:5D:BF:53:71");   // wd牙刷
    on_macInput_returnPressed();
    // pb->set_device_mode();//进入亮白
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), dongleSerialPort(new QSerialPort(this)), pb(new Qpb(dongleSerialPort)),
      at(new Qat(dongleSerialPort)), qimuc(new imu_calibrate), basicInfoModel(new TestModel),
      nqimuc(new new_imu_calibrate), peripheralModel(new TestModel), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // setAttribute(Qt::WA_QuitOnClose,  true); //关闭此窗口，会立即执行析构函数
    update_main_style("Ubuntu.qss");
    recoverCustom();
    ui->wifi_test_result->setText("WIFI:WAIT");
    ui->wifi_test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    ui->ble_test_result->setText("BLE:WAIT");
    ui->ble_test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    // this->setCentralWidget(ui->tabWidget);
    ui->local_ota_result->setText("OTA");
    ui->local_ota_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    on_refreshButton_clicked();
    initBasicInfo();
    initPeriphState();

    OTAGroup->addButton(ui->product_ver, 0);   // 分组1、序号0
    OTAGroup->addButton(ui->test_ver, 1);      // 分组1、序号1

    WifiStatusLabel = new QLabel("wifi连接：<font color='red'>失败</font>");
    bleStatusLabel = new QLabel("蓝牙连接：<font color='red'>失败</font>");
    uartStatusLabel = new QLabel("串口连接：<font color='red'>失败</font>");
    macLabel = new QLabel("蓝牙mac:                        ");
    // board_sn = new QLabel("板子sn:                        ");
    tail_sn = new QLabel("尾盖sn:                        ");
    ota_source_set(1);   // 一开机锁住
    ota_fw_set(1);       // 一开机锁住
    ui->statusbar->addPermanentWidget(board_sn);
    ui->statusbar->addPermanentWidget(tail_sn);
    ui->statusbar->addPermanentWidget(macLabel);
    ui->statusbar->addPermanentWidget(bleStatusLabel);
    ui->statusbar->addPermanentWidget(WifiStatusLabel);
    ui->statusbar->addPermanentWidget(uartStatusLabel);
    ui->statusbar->addPermanentWidget(
        new QLabel("电刷产测工具 V1.1.6" + QString(__DATE__) + " " + QString(__TIME__)));
    connect(nqimuc, SIGNAL(send_imu_cali_msg(QString)), this, SLOT(refresh_imu_cali_msg(QString)));
    connect(pb, SIGNAL(send_pb_date(QString)), this, SLOT(refresh_pb_data(QString)));
    connect(pb, SIGNAL(pressSensorDataReady(FacUploadPresSensor)), this,
            SLOT(getPressSensorData(FacUploadPresSensor)));
    connect(pb, SIGNAL(imuDataReady(FacUploadNineAlex)), this, SLOT(getimuData(FacUploadNineAlex)));
    connect(pb, SIGNAL(send_battary(FacDevInfo)), this, SLOT(refresh_battary_data(FacDevInfo)));
    connect(pb, SIGNAL(send_wifi_State(FacDevInfo)), this, SLOT(update_wifi(FacDevInfo)));
    connect(pb, SIGNAL(send_sn_data(FacDevInfo)), this, SLOT(refresh_sn(FacDevInfo)));
    connect(this->dongleSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleDongleSerialPortError(QSerialPort::SerialPortError)));
    connect(at, SIGNAL(sendwifimsg(QString)), this, SLOT(get_wifi_msg(QString)));
    connect(pb, SIGNAL(send_FactroyCmd_INTERNET_OTA(FacInternetOta)), this,
            SLOT(update_local_ota_result(FacInternetOta)));
    connect(pb, SIGNAL(send_motor_cali_msg(QString)), this, SLOT(refresh_motor_cali_msg(QString)));

    connect(pb, SIGNAL(send_FactroyCmd_WIFI_DEMAND(FacWifiDemand)), this,
            SLOT(update_FactroyCmd_WIFI_DEMAND(FacWifiDemand)));

    connect(pb, SIGNAL(send_IMU_CALIB_result(FacImuCalibResult)), this,
            SLOT(update_IMU_CALIB_result(FacImuCalibResult)));
    connect(this, SIGNAL(refreshDongleSerialPortState(int)), this,
            SLOT(refresh_dongle_uart_state(int)));
    connect(ui->is_ota_1, SIGNAL(stateChanged(int)), this, SLOT(ota_source_set(int)));
    connect(ui->is_ota_2, SIGNAL(stateChanged(int)), this, SLOT(ota_fw_set(int)));
    connect(ui->show_rssi, SIGNAL(stateChanged(int)), this, SLOT(show_ble_rssi(int)));

    connect(at, SIGNAL(send_ble_state(int)), this, SLOT(refresh_ble_state(int)));
    connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refresh_ble_rssi(QString)));
    connect(at, SIGNAL(send_wifi_rssi(QString)), this, SLOT(refresh_wifi_rssi(QString)));
    // connect(at, SIGNAL(send_WIFI_state(int)), this, SLOT(refresh_WIFI_state(int)));

    connect(waittime, &QTimer::timeout,
            [=]()
            {
                isovertime = 1;
                waittime->stop();
            });
    connect(dongleSerialPort, &QSerialPort::readyRead, this,
            [=]()
            {
                dongleSerialPortTimer->start(100);   // 设置100毫秒的延时
                dongleSerialPortBuf.append(dongleSerialPort->readAll());   // 将读到的数据放入缓冲区
            });
    QFont font("Arial", 10);
    ui->gyro_x->setFont(font);
    ui->gyro_y->setFont(font);
    ui->gyro_z->setFont(font);
    ui->acc_x->setFont(font);
    ui->acc_y->setFont(font);
    ui->acc_z->setFont(font);
    ui->get_mac->setFocus();

    ui->music_combo->addItem("/data/audio/scan_f.bin");
    ui->music_combo->addItem("/data/audio/white.bin");
    for (int i = 0; i <= 15; ++i)
    {
        QString item = QString("/data/audio/sound%1.bin").arg(i);
        ui->music_combo->addItem(item);
    }
    // 201固件  202资源  303音频 304主题 305音乐文件
    QStringList items = {"固件", "资源", "音频", "主题", "音乐文件"};

    for (auto &item : items)
    {
        ui->version_type_1->addItem(item);
        ui->version_type_2->addItem(item);
    }
    ui->version_type_1->setCurrentIndex(1);
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    HighRssi = settings.value("WIFI/HighRssi").toDouble();
    LowRssi = settings.value("WIFI/LowRssi").toDouble();
    BleHighRssi = settings.value("BLE/HighRssi").toDouble();
    BleLowRssi = settings.value("BLE/LowRssi").toDouble();
    RssiTestTime = settings.value("BLE/RssiCount").toInt();

    standbattary = settings.value("BATTARY/standbattary").toDouble();
    QRegExp regExp("[0-9]{1,},[0-9]{1,},[0-9]{1,},[0-9]{1,},[0-9]{1,},[0-9]{1,}");
    QValidator *validator = new QRegExpValidator(regExp, this);
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
    // foreach (const QSerialPortInfo &info, QSerialPortInfo::availablePorts()) {
    //     comNameCombo->addItem(info.portName());
    //  }

    ui->progressBar->hide();

    if (!settings.value("SYSTEM/local_ota").toInt())
        ui->tabWidget->setTabVisible(8, false);

    ui->high_speed_tp->installEventFilter(this);
    ui->high_speed_tp->setAcceptDrops(true);

    ui->active_picture->installEventFilter(this);
    ui->active_picture->setAcceptDrops(true);

    if (settings.value("Mes/FACTORY").toString() == "hq")
        ui->statusbar->addPermanentWidget(new QLabel("华勤"));
    else if (settings.value("Mes/FACTORY").toString() == "lx")
        ui->statusbar->addPermanentWidget(new QLabel("立讯"));
    else if (settings.value("Mes/FACTORY").toString() == "jj")
        ui->statusbar->addPermanentWidget(new QLabel("金进"));
    else
        ui->statusbar->addPermanentWidget(new QLabel("欣旺达"));

    viewercamrea = new ImageViewer("image.png", this);
    ui->verticalLayout_31->addWidget(viewercamrea);   // 将 ImageViewer 添加到布局中
    viewercamrea->show();                             // 显示 ImageViewer

    dongleRingBuf =
        new RingBuf(&p_dongleRingBuffer, dongle_ring_buffer, 1, sizeof(dongle_ring_buffer));
    cameraRingBuf = new RingBuf(&p_cameraRingBuffer, camera_ring_buf, 1, sizeof(camera_ring_buf));
}

void MainWindow::on_pushButton_3_clicked()
{
    // pb->getbattary();
    // pb->getbattary();
    // pb->getbattary();
    // pb->getbattary();
    // pb->getbattary();
    // pb->getbattary();

    // const int bufferSize = 50 * 1024;  // 数组大小
    // QString hexDump;  // 用于存储十六进制字符串

    // // 遍历 camera_ring_buf 数组
    // for (int i = 0; i < bufferSize; ++i) {
    //     // 将 uint8_t 类型的数组元素转换为十六进制字符串，并拼接到 hexDump 字符串中
    //     hexDump += QString("%1 ").arg(camera_ring_buf[i], 2, 16, QChar('0')).toUpper();

    //     // 每 243 个元素打印一行
    //     if ((i + 1) % 243 == 0) {
    //         qDebug().noquote() << hexDump;  // 打印一行十六进制内容
    //         hexDump.clear();  // 清空 hexDump 字符串，准备下一行打印
    //     }
    // }

    // // 打印剩余内容（不足一行的部分）
    // if (!hexDump.isEmpty()) {
    //     qDebug().noquote() << hexDump;
    // }

    qDebug() << "接下来是串口的数据";
    const int bufferSize1 = 50 * 1024;   // 数组大小
    QString hexDump1;                    // 用于存储十六进制字符串

    // 遍历 dongle_ring_buffer 数组
    for (int i = 0; i < bufferSize1; ++i)
    {
        // 将 uint8_t 类型的数组元素转换为十六进制字符串，并拼接到 hexDump 字符串中
        hexDump1 += QString("%1 ").arg(dongle_ring_buffer[i], 2, 16, QChar('0')).toUpper();

        // 每 243 个元素打印一行
        if ((i + 1) % 243 == 0)
        {
            qDebug().noquote() << hexDump1;   // 打印一行十六进制内容
            hexDump1.clear();                 // 清空 hexDump 字符串，准备下一行打印
        }
    }

    // 打印剩余内容（不足一行的部分）
    if (!hexDump1.isEmpty())
    {
        qDebug().noquote() << hexDump1;
    }
}

void MainWindow::write_camera_data(uint8_t *p_data, int data_len)
{
    int surpluse_space = 0;

    if (1)
    {
        surpluse_space = cameraRingBuf->usmile_ring_buffer_surplus_space_get(&p_cameraRingBuffer);
        if (surpluse_space < data_len)
        {
            qDebug() << "环形队列满了，剩下：" << surpluse_space;
            return;
        }

        int write_len =
            cameraRingBuf->usmile_ring_buffer_write(&p_cameraRingBuffer, p_data, data_len);
        // qDebug() << "写入摄像头数据包长度"<<write_len;
    }
    else
    {
        qDebug() << "通道不是摄像头";
    }
}
int MainWindow::ext_ble_find_next_frame(void)
{
    int i = 0;
    ext_uart_phy_layer_t *head = NULL;
    int len = dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, 2 * 1024);
    // qDebug() << "查找下一帧，剩下：" << len;

    for (i = 0; i < len; i++)
    {
        if (frame_buf[i] == 0xAA && frame_buf[i + 1] == 0xAA && frame_buf[i + 2] == 0xAA &&
            frame_buf[i + 3] == 0xAA)
        {
            head = (ext_uart_phy_layer_t *)&frame_buf[i];
            if (head->magic == EXT_UART_PHY_LAYER_MAGIC)
            {
                qDebug() << "匹配到了串口数据包头";

                dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, i);

                return 1;
            }
        }
    }

    dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, len);

    return 0;
}
int MainWindow::ext_ble_find_next_picture_frame(void)
{
    int i = 0;
    ext_picture_layer_t *head = NULL;
    int len =
        cameraRingBuf->usmile_ring_buffer_pick(&p_cameraRingBuffer, frame_picture_buf, 2 * 1024);
    // qDebug() << "查找下一帧，剩下：" << len;

    for (i = 0; i < len; i++)
    {
        if (frame_picture_buf[i] == 0xA5 && frame_picture_buf[i + 1] == 0xA5 &&
            frame_picture_buf[i + 2] == 0xA5 && frame_picture_buf[i + 3] == 0xA5)
        {
            head = (ext_picture_layer_t *)&frame_picture_buf[i - 20];
            if (head->reserved == EXT_PICTURE_PHY_LAYER_MAGIC)
            {
                qDebug() << "匹配到了图片数据包头";

                cameraRingBuf->usmile_ring_buffer_delete(&p_cameraRingBuffer, i - 20);

                return 1;
            }
        }
    }

    cameraRingBuf->usmile_ring_buffer_delete(&p_cameraRingBuffer, len);

    return 0;
}

void printSquareData(uint8_t *data, size_t data_size)
{
    if (data_size < 36000)
        return;
    const int dimension = 32;           // 每行的列数
    const int totalItems = data_size;   // 数据总量
    for (int i = 0; i < totalItems; i += dimension)
    {
        QString row;
        for (int j = 0; j < dimension && (i + j) < totalItems; ++j)
        {
            row += QString("%1 ").arg(data[i + j], 2, 16, QChar('0'));
            row += " ";
        }
        qDebug().noquote() << row;
    }
}

void MainWindow::solve_picture_frame(void)
{
    // cameraRingBuf = new RingBuf(&p_cameraRingBuffer, camera_ring_buf, 1,
    // sizeof(camera_ring_buf));
    uint16_t crc16 = NULL;
    uint16_t crc_cali = 0;
    while (true)
    {
        // 从环形缓冲区中读取帧头
        cameraRingBuf->usmile_ring_buffer_pick(&p_cameraRingBuffer, frame_picture_buf,
                                               PICTURE_PHY_LAYER_HEAD_SIZE);
        int ring_size = cameraRingBuf->usmile_ring_buffer_items_count_get(&p_cameraRingBuffer);
        if (ring_size <= PICTURE_PHY_LAYER_HEADER_ADN_CRC)
        {
            qDebug() << "摄像头环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }

        ext_picture_layer_t *head = (ext_picture_layer_t *)frame_picture_buf;
        if (head->width == 0xb4 && head->height == 0xc8)
        {

            int frame_size = head->data_size + PICTURE_PHY_LAYER_HEAD_SIZE;
            qDebug() << ++dataNumber;

            if (frame_size > ring_size)
            {
                qDebug() << "图片帧数据不完整" << "需要" << frame_size << "实际为" << ring_size;
                break;
            }

            // 从环形缓冲区中读取整个帧的数据
            cameraRingBuf->usmile_ring_buffer_pick(&p_cameraRingBuffer, frame_picture_buf,
                                                   frame_size);

            qDebug() << "图片帧数据完整" << "需要" << frame_size << "实际为" << ring_size
                     << sizeof(head);

            crc16 = head->data_crc16;
            crc_cali = CRC16(head->data, head->data_size);
            if (crc16 == crc_cali)
            {
                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char *>(head), frame_size);
                processTheDatagram(byteArray);   // 显示图片
            }
            else
            {
                qDebug() << "head content:"
                         << QByteArray(reinterpret_cast<char *>(head), PICTURE_PHY_LAYER_HEAD_SIZE)
                                .toHex();
                qDebug() << "crc校验失败" << crc_cali << crc16 << "head->data[0]" << head->data[0]
                         << "head->data[1]" << head->data[1];
                printSquareData(head->data, head->data_size);
                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char *>(head),
                                     head->data_size + PICTURE_PHY_LAYER_HEAD_SIZE);
                processTheDatagram(byteArray);   // 显示图片
            }

            // 删除已经处理的帧数据
            dongleRingBuf->usmile_ring_buffer_delete(&p_cameraRingBuffer, frame_size);

        }
        else
        {
            qDebug() << "数据流错误寻找下一帧";
            qDebug() << "数据包头为:"
                     << QByteArray(reinterpret_cast<char *>(frame_picture_buf),
                                   PICTURE_PHY_LAYER_HEAD_SIZE)
                            .toHex();

            if (ext_ble_find_next_picture_frame())
            {
                continue;
            }
            else
            {
                qDebug() << "找不到帧头";
                break;
            }
        }

        // 处理完一帧数据后，让 Qt 处理事件循环，以确保界面更新等操作得到及时响应
        QCoreApplication::processEvents();
    }
}

void MainWindow::solve_frame(void)
{
    while (true)
    {
        // 从环形缓冲区中读取帧头
        dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf,
                                               UART_PHY_LAYER_HEAD_SIZE);
        int ring_size = dongleRingBuf->usmile_ring_buffer_items_count_get(&p_dongleRingBuffer);
        if (ring_size <= UART_PHY_LAYER_HEADER_ADN_CRC)
        {
            qDebug() << "串口环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }

        ext_uart_phy_layer_t *head = (ext_uart_phy_layer_t *)frame_buf;

        if (head->magic == EXT_UART_PHY_LAYER_MAGIC)
        {
            int frame_size = UART_PHY_LAYER_HEADER_ADN_CRC + head->length;

            if (frame_size > ring_size)
            {
                qDebug() << "串口帧数据不完整" << "需要" << frame_size << "实际为" << ring_size;

                break;
            }

            // 从环形缓冲区中读取整个帧的数据
            dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, frame_size);

            // if (head->data[head->length]==0x0d&&head->data[head->length+1] == 0x0A)
            if (1)
            {
                // 处理帧数据
                write_camera_data(head->data, head->length);

                solve_picture_frame();
            }
            else
            {
                qDebug() << "head content:"
                         << QByteArray(reinterpret_cast<char *>(head), 280).toHex();

                qDebug() << "尾巴校验失败"
                         << QString("%1").arg(head->data[head->length], 2, 16, QChar('0'))
                         << QString("%1").arg(head->data[head->length + 1], 2, 16, QChar('0'));
            }

            // 删除已经处理的帧数据
            dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, frame_size);
        }
        else
        {
            // qDebug() << "串口数据流错误寻找下一帧";
            // qDebug() << "串口数据包头为:"
            //          << QByteArray(reinterpret_cast<char *>(frame_buf), UART_PHY_LAYER_HEAD_SIZE)
            //                 .toHex();

            if (ext_ble_find_next_frame())
            {
                continue;
            }
            else
            {
                // qDebug() << "找不到帧头";
                break;
            }
        }

        // 处理完一帧数据后，让 Qt 处理事件循环，以确保界面更新等操作得到及时响应
        QCoreApplication::processEvents();
    }
}
int totalsize = 0;

void MainWindow::readDongleSerialPortData()
{
    dongleSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = dongleSerialPortBuf;   // 读取缓冲区数据

    int write_len = 0;
    int len = dataTemp.size();
    write_len = dongleRingBuf->usmile_ring_buffer_write(
        &p_dongleRingBuffer, reinterpret_cast<uint8_t *>(dataTemp.data()), dataTemp.size());

    totalsize = dataTemp.size() + totalsize;
    qDebug() << "totalsize:" << totalsize;

    if (write_len < len)
    {
        qDebug() << "write_len:" << write_len << "len:" << dataTemp.size();
    }

    solve_frame();

    at->parseCmd(dataTemp);   // at回应用
    pb->parseCmd(dataTemp);
    getmacadress(dataTemp);   // 搜索设备用
    // qDebug() << "串口接收到的码为:" << dataTemp.toHex(' ');
    ui->log->appendPlainText(QString::fromUtf8(dataTemp));
    dongleSerialPortBuf.clear();   // 清除缓冲区
}

void MainWindow::refresh_dongle_uart_state(int state)
{
    if (state)
        ui->msgEdit->appendPlainText("dongle串口连接成功");
    else
    {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        ui->msgEdit->appendPlainText("dongle串口连接断开");
    }
}
void MainWindow::refresh_imu_cali_msg(QString msg)
{
    ui->msgEdit->appendPlainText(msg);
    // qDebug() << "收到数据: " << getIndex();

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
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_六轴测试log.csv";
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
            qDebug() << "Error creating file";
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
        rowData << stringsn << csvmac << timestamp << msg;
        stream << rowData.join(",") << "\n";

        file.close();
        qDebug() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << "Error appending to file";
    }
}
void MainWindow::refresh_motor_cali_msg(QString msg)
{
    ui->msgEdit->appendPlainText(msg);
    // qDebug() << "收到数据: " << getIndex();

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
            qDebug() << "Error creating file";
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
        rowData << stringsn << macAddress << timestamp << msg;
        stream << rowData.join(",") << "\n";

        file.close();
        qDebug() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << "Error appending to file";
    }
}
void MainWindow::show_ble_rssi(int state)
{
    if (state == 2)
        state = 1;
    at->sendBLELOG(state);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->high_speed_tp || watched == ui->active_picture)
    {
        if (event->type() == QEvent::DragEnter)
        {
            // [[2]]: 当拖放时鼠标进入label时, label接受拖放的动作
            QDragEnterEvent *dee = dynamic_cast<QDragEnterEvent *>(event);
            dee->acceptProposedAction();
            return true;
        }
        else if (event->type() == QEvent::Drop)
        {
            // [[3]]: 当放操作发生后, 取得拖放的数据
            QDropEvent *de = dynamic_cast<QDropEvent *>(event);
            QList<QUrl> urls = de->mimeData()->urls();
            if (urls.isEmpty())
            {
                return true;
            }
            QString path = urls.first().toLocalFile();
            qDebug() << "路径为：" << path;

            // [[4]]: 在label上显示拖放的图片
            QImage image(path);
            // QImage对I/O优化过, QPixmap对显示优化
            if (!image.isNull() || !path.isNull())
            {
                if (ui->active_picture->underMouse())
                {   // 如果拖到了active_picture上

                    renameFilesInFolder(path);
                }
                if (ui->high_speed_tp->underMouse())
                {   // 如果拖到了high_speed_tp上
                    ui->high_speed_tp->setPixmap(QPixmap::fromImage(image));
                    QFileInfo fileInfo(path);
                    QString fileName = fileInfo.fileName();
                    convertImageTo16BitPaletteHigh(path, fileName);
                }
                else
                {
                    qDebug() << "Image dropped on an unknown widget.";
                }
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void MainWindow::convertImageTo16BitPaletteHigh(const QString &imagePath,
                                                const QString &outputFileName)
{
    // 加载图片
    QImage image(imagePath);
    if (image.isNull())
    {
        qDebug() << "无法加载图片：" << imagePath;
        return;
    }

    // 将图片转换为16位调色板格式
    QImage convertedImage = image.convertToFormat(QImage::Format_RGB16);

    // 如果输出目录不存在，则创建
    QDir outputDir("output");
    if (!outputDir.exists())
    {
        if (!outputDir.mkpath("."))
        {
            qDebug() << "无法创建输出目录";
            return;
        }
    }

    // 创建输出文件路径
    //  QString outputFilePath = outputDir.absoluteFilePath(outputFileName);
    QString outputFilePath = outputDir.filePath(outputFileName);
    // 打开输出文件
    QFile file(outputFilePath);
    if (!file.open(QIODevice::WriteOnly))
    {
        qDebug() << "无法打开输出文件：" << outputFilePath;
        return;
    }

    // 将图片宽度和高度以16位整数（大端字节序）写入文件
    quint16 width = convertedImage.width();
    quint16 height = convertedImage.height();
    file.write(reinterpret_cast<const char *>(&height), sizeof(quint16));
    file.write(reinterpret_cast<const char *>(&width), sizeof(quint16));

    // 写入图片数据
    for (int y = 0; y < convertedImage.height(); ++y)
    {
        for (int x = 0; x < convertedImage.width(); ++x)
        {
            // 获取像素颜色
            QRgb pixelColor = convertedImage.pixel(x, y);

            // 提取R、G、B分量
            uint16_t r = qRed(pixelColor) >> 3;     // 5位
            uint16_t g = qGreen(pixelColor) >> 2;   // 6位
            uint16_t b = qBlue(pixelColor) >> 3;    // 5位

            // 将分量合并成16位格式
            uint16_t pixelValue = (r << 11) | (g << 5) | b;

            // 大端字节序下的字节顺序交换
            uint16_t bigEndianValue = ((pixelValue & 0xFF00) >> 8) | ((pixelValue & 0x00FF) << 8);

            // 将像素值写入文件
            file.write(reinterpret_cast<const char *>(&bigEndianValue), sizeof(quint16));
        }
    }

    file.close();
    send_picture(ui->ui_ip->text(), outputFilePath);
}

bool compareFileNames(const QString &fileName1, const QString &fileName2)
{
    // 将文件名转换为数字并进行比较
    int number1 = fileName1.left(fileName1.indexOf('.')).toInt();
    int number2 = fileName2.left(fileName2.indexOf('.')).toInt();
    return number1 < number2;
}

void MainWindow::renameFilesInFolder(const QString &folderPath)
{
    QDir folder(folderPath);
    if (!folder.exists())
    {
        qDebug() << "文件夹不存在";
        return;
    }

    // 获取文件夹中所有文件的列表
    QStringList files = folder.entryList(QDir::Files, QDir::Name);
    int fileCount = files.size();
    std::sort(files.begin(), files.end(), compareFileNames);
    qDebug() << "files:" << files;
    // 遍历文件夹中的文件
    for (int i = 0; i < fileCount; ++i)
    {
        QString fileName = files.at(i);
        QString filePath = folder.filePath(fileName);

        // 重命名文件为顺序编号
        QString newFileName = QString::number(i + 1) + ".png";
        QString newFilePath = folder.filePath(newFileName);

        // 检查新文件名是否已经存在
        if (QFile::exists(newFilePath))
        {
            qDebug() << "文件" << newFilePath << "已经存在，跳过重命名";
            convertImageTo16BitPaletteHigh(newFilePath, newFileName);
            continue;
        }

        QImage image(newFilePath);
        ui->active_picture->setPixmap(QPixmap::fromImage(image));
        if (!QFile::rename(filePath, newFilePath))
        {
            qDebug() << "重命名文件" << filePath << "失败";
            convertImageTo16BitPaletteHigh(newFilePath, newFileName);
            continue;
        }

        qDebug() << "文件" << filePath << "重命名为" << newFilePath;

        // 运行convertImageTo16BitPaletteHigh函数
        convertImageTo16BitPaletteHigh(newFilePath, newFileName);
    }
}
void MainWindow::on_clear_picture_clicked()
{
    // 创建网络访问管理器
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);

    // 创建请求
    QNetworkRequest request;
    request.setUrl(QUrl(ui->ui_ip->text() +
                        "/trigger_function"));   // 拼接 "/trigger_function" 到 ESP32 的 IP 地址

    // 发送 GET 请求
    QNetworkReply *reply = manager->get(request);

    // 处理请求完成信号
    connect(reply, &QNetworkReply::finished,
            [=]()
            {
                // 检查响应状态码
                if (reply->error() == QNetworkReply::NoError)
                {
                    qDebug() << "Request succeeded";
                    ui->msgEdit->appendPlainText("图片删除完毕");
                }
                else
                {
                    qDebug() << "Request failed:" << reply->errorString();
                    ui->msgEdit->appendPlainText("图片删除失败");
                }

                // 释放资源
                reply->deleteLater();
                manager->deleteLater();
            });
}

void MainWindow::send_picture(const QString &url, const QString &filePath)
{
    qDebug() << "filePath" << filePath;
    // 创建QFile实例
    QFile *file = new QFile(filePath);
    if (!file->open(QIODevice::ReadOnly))
    {
        qDebug() << "文件打开失败";
        return;
    }

    // 构造QHttpMultiPart对象，并设置四个部分("update", "file", "submit")
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 设置"name"部分
    QHttpPart namePart;
    namePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"update\""));
    namePart.setBody(file->fileName().toLocal8Bit());   // 设置文件名

    // 设置"file"部分
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"file\"; filename=\"" + file->fileName() + "\""));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant("image/png"));   // 设定上传文件类型
    filePart.setBodyDevice(file);
    file->setParent(multiPart);

    // 添加"name"和"file"部分
    multiPart->append(namePart);
    multiPart->append(filePart);

    // 构造请求
    QNetworkRequest request(url);

    // 使用QNetworkAccessManager发送请求
    QNetworkAccessManager *manager = new QNetworkAccessManager();
    QNetworkReply *reply = manager->post(request, multiPart);
    multiPart->setParent(reply);
    qDebug() << "发送文件：" << filePath;

    // 创建事件循环
    QEventLoop loop;

    // 连接请求完成信号和事件循环退出
    QObject::connect(reply, &QNetworkReply::finished,
                     [&loop, reply, filePath, ui = this->ui]()
                     {
                         // 如果请求响应完成，打印响应结果
                         if (reply->error() == QNetworkReply::NoError)
                         {
                             qDebug() << "发送完成" << filePath;
                             ui->msgEdit->appendPlainText("发送完成" + filePath);
                         }
                         else
                         {
                             qDebug() << "发送失败" << filePath;
                             qDebug() << "错误信息：" << reply->errorString();
                         }
                         // 退出事件循环
                         loop.quit();
                     });

    // 开始事件循环，等待请求完成
    loop.exec();
}

void MainWindow::update_IMU_CALIB_result(FacImuCalibResult x)
{
    ui->msgEdit->appendPlainText(QString("gyro_x: %1").arg(qint32(x.gyro_x)));
    ui->msgEdit->appendPlainText(QString("gyro_y: %1").arg(qint32(x.gyro_y)));
    ui->msgEdit->appendPlainText(QString("gyro_z: %1").arg(qint32(x.gyro_z)));
    ui->msgEdit->appendPlainText(QString("bx: %1").arg(x.new_cali.bx));
    ui->msgEdit->appendPlainText(QString("by: %1").arg(x.new_cali.by));
    ui->msgEdit->appendPlainText(QString("bz: %1").arg(x.new_cali.bz));
    ui->msgEdit->appendPlainText(QString("kx: %1").arg(x.new_cali.kx));
    ui->msgEdit->appendPlainText(QString("ky: %1").arg(x.new_cali.ky));
    ui->msgEdit->appendPlainText(QString("kz: %1").arg(x.new_cali.kz));
    ui->msgEdit->appendPlainText(QString("syx: %1").arg(x.new_cali.syx));
    ui->msgEdit->appendPlainText(QString("szx: %1").arg(x.new_cali.szx));
    ui->msgEdit->appendPlainText(QString("szy: %1").arg(x.new_cali.szy));
}
void MainWindow::ota_fw_set(int state)
{
    for (int i = 1; i < 2; i++)
    {
        // 获取相关对象引用
        QComboBox *combo = findChild<QComboBox *>(QString("version_type_%1").arg(i + 1));
        QLineEdit *lineEdit_fileSize = findChild<QLineEdit *>(QString("file_size_%1").arg(i + 1));
        QLineEdit *lineEdit_versionType =
            findChild<QLineEdit *>(QString("version_type_%1").arg(i + 3));
        QLineEdit *lineEdit_url = findChild<QLineEdit *>(QString("url_%1").arg(i + 1));
        QLineEdit *lineEdit_md5 = findChild<QLineEdit *>(QString("md5_%1").arg(i + 1));

        if (state)
        {   // 失能
            combo->setEnabled(false);
            lineEdit_fileSize->setEnabled(false);
            lineEdit_versionType->setEnabled(false);
            lineEdit_url->setEnabled(false);
            lineEdit_md5->setEnabled(false);
        }
        else
        {   // 使能
            combo->setEnabled(true);
            lineEdit_fileSize->setEnabled(true);
            lineEdit_versionType->setEnabled(true);
            lineEdit_url->setEnabled(true);
            lineEdit_md5->setEnabled(true);
        }
    }
}
void MainWindow::ota_source_set(int state)
{
    for (int i = 0; i < 1; i++)
    {
        // 获取相关对象引用
        QComboBox *combo = findChild<QComboBox *>(QString("version_type_%1").arg(i + 1));
        QLineEdit *lineEdit_fileSize = findChild<QLineEdit *>(QString("file_size_%1").arg(i + 1));
        QLineEdit *lineEdit_versionType =
            findChild<QLineEdit *>(QString("version_type_%1").arg(i + 3));
        QLineEdit *lineEdit_url = findChild<QLineEdit *>(QString("url_%1").arg(i + 1));
        QLineEdit *lineEdit_md5 = findChild<QLineEdit *>(QString("md5_%1").arg(i + 1));

        if (state)
        {   // 失能
            combo->setEnabled(false);
            lineEdit_fileSize->setEnabled(false);
            lineEdit_versionType->setEnabled(false);
            lineEdit_url->setEnabled(false);
            lineEdit_md5->setEnabled(false);
        }
        else
        {   // 使能
            combo->setEnabled(true);
            lineEdit_fileSize->setEnabled(true);
            lineEdit_versionType->setEnabled(true);
            lineEdit_url->setEnabled(true);
            lineEdit_md5->setEnabled(true);
        }
    }
}

void MainWindow::refresh_WIFI_state(int state)
{
    if (state)
    {
        // ui->WifiStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        //  ui->msgEdit->appendPlainText("WIFI连接成功");
    }
    else
    {
        //  ui->WifiStatusLabel->setText("WIFI连接：<font color='red'>失败</font>");
        //  ui->msgEdit->appendPlainText("WIFI连接断开");
    }
}
void MainWindow::refresh_ble_state(int state)
{
    if (state)
    {
        bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        ui->msgEdit->appendPlainText("蓝牙连接成功");
        pb->setDevForbidSleepState(FacSwitch_OPEN);

        ui->msgEdit->appendPlainText("已发送禁止休眠");
    }
    else
    {
        bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}

void MainWindow::refresh_battary_data(FacDevInfo adc)
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
        chargeStateStr = "充电状态为：未知";
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
    voltage = adc.dev_info[0].value_item.battery.voltage / 1000.0;
    QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    QRegularExpressionMatch match = regex.match(chargeStateStr);
    chargestate = match.captured(2);
    ;
    if (adc.dev_info[0].value_item.battery.charge_state == 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary)
    {
        is_battary_test = 1;
        charageresult = "通过";
        voltageresult = "通过";
    }
    if (adc.dev_info[0].value_item.battery.charge_state != 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary)
    {
        charageresult = "失败";
        voltageresult = "通过";
        is_battary_test = 2;   // 状态不对
    }
    if (adc.dev_info[0].value_item.battery.charge_state == 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary)
    {
        voltageresult = "失败";
        charageresult = "通过";
        is_battary_test = 3;   // 电压不对
    }
    if (adc.dev_info[0].value_item.battery.charge_state != 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary)
    {
        is_battary_test = 4;   // 全部不对
        voltageresult = "失败";
        charageresult = "失败";
    }

    save_battary_data_to_csv(voltage, chargestate, charageresult, voltageresult);
}

void MainWindow::save_battary_data_to_csv(double vol, QString charge_state, QString chares,
                                          QString volres)
{
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
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_电池情况报告.csv";
    QString filePath = QDir(folderPath).filePath(fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);

        // 写入表头
        QStringList headers;
        headers << "mac地址" << "时间戳" << "测试项" << "测试数据" << "测试结果";

        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        stream << headers.join(",") << "\n";

        // 写入数据 - wifi
        QStringList rowData;

        // 写入数据 - 充电状态
        rowData.clear();   // 清空rowData，以便添加新的数据
        rowData << macAddress << timestamp << "充电状态" << chargestate << chares;
        stream << rowData.join(",") << "\n";

        // 写入数据 - 电压
        rowData.clear();   // 清空rowData，以便添加新的数据
        rowData << macAddress << timestamp << "当前电压" << QString::number(vol) << volres;
        stream << rowData.join(",") << "\n";

        file.close();
        qDebug() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << "Error appending to file";
    }
}

void MainWindow::refresh_sn(FacDevInfo data)
{
    qDebug() << "board_sn load" << data.dev_info[0].value_item.board_sn;
    qDebug() << "tail_sn load" << data.dev_info[0].value_item.tail_sn;
    QString board_sn_string = QString::fromUtf8(data.dev_info[0].value_item.board_sn);
    QString tail_sn_string = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    // board_sn->setText("板子sn:"+board_sn_string);
    tail_sn->setText("尾盖sn:" + tail_sn_string);
}
void MainWindow::refresh_ble_rssi(QString data)
{
    // qDebug() << "rssi = " << data;
    ui->BLE_RSSI->setText("BLE的RSSI：" + data);
    BLE_RSSI = data;
}
void MainWindow::refresh_wifi_rssi(QString data)
{
    // qDebug() << "rssi = " << data;
    ui->WIFI_RSSI->setText("WIFI的RSSI：" + data);
    WIFI_RSSI = data;
}
void MainWindow::get_wifi_msg(QString data)
{
    // qDebug() << "收到wifi数据为" << data;
    QStringList parts = data.split("-");
    int numPairs = parts.size() / 2;
    for (int i = 0; i < numPairs; ++i)
    {
        QString macAddress = parts[i * 2];
        QString rssi = "-" + parts[i * 2 + 1];
        // wifiMac= wifiMac.toUpper();
        // qDebug() << "连接的的wifiMac:" << macAddress;
        // qDebug() << "RSSI:" << rssi;
        // qDebug() << " 设备的wifiMac:" << wifiMac;
        // if(macAddress==wifiMac)
        // {
        //     // qDebug() << getIndex()<< " 收到rssi啦" << rssi;
        //     refresh_WIFI_state(1);
        //     WIFI_RSSI=rssi;

        // }
        WIFI_RSSI = rssi;
        ui->WIFI_RSSI->setText("WIFI的RSSI：" + rssi);
    }
}

void MainWindow::update_wifi(FacDevInfo wifi)
{
    QString wifiName = QString::fromUtf8(wifi.dev_info[0].value_item.wifi_info.wifi_name);
    ui->msgEdit->appendPlainText(wifiName);
    QString wifipassword = QString::fromUtf8(wifi.dev_info[0].value_item.wifi_info.wifi_password);
    ui->msgEdit->appendPlainText(wifipassword);
}
void MainWindow::update_main_style(QString style)
{
    // QSS文件初始化界面样式
    QString stylesheet;
    // QFile qss("../tooth_brush_debug_tools/qss/" + style);
    QFile qss(style);
    if (qss.open(QFile::ReadOnly))
    {
        qDebug() << "qss load";
        QTextStream filetext(&qss);
        stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    }
    else
    {
        qDebug() << "qss not load";
        qss.setFileName("/qss/" + style);
        if (qss.open(QFile::ReadOnly))
        {
            qDebug() << "qss load";
            QTextStream filetext(&qss);
            stylesheet = filetext.readAll();
            this->setStyleSheet(stylesheet);
            qss.close();
        }
    }
}
void MainWindow::getimuData(FacUploadNineAlex x)
{
    for (int i = 0; i < x.data_count; i++)
    {
        orgData.acc[0] = x.data[i].acc_x;
        orgData.acc[1] = x.data[i].acc_y;
        orgData.acc[2] = x.data[i].acc_z;
        orgData.gyro[0] = x.data[i].gyro_x;
        orgData.gyro[1] = x.data[i].gyro_y;
        orgData.gyro[2] = x.data[i].gyro_z;

        ui->gyro_x->setText("gyro_x=" + QString::number(orgData.gyro[0]));
        ui->gyro_y->setText("gyro_y=" + QString::number(orgData.gyro[1]));
        ui->gyro_z->setText("gyro_z=" + QString::number(orgData.gyro[2]));
        ui->acc_x->setText("acc_x=" + QString::number(orgData.acc[0]));
        ui->acc_y->setText("acc_y=" + QString::number(orgData.acc[1]));
        ui->acc_z->setText("acc_z=" + QString::number(orgData.acc[2]));
    }
    // qDebug()<<"收到六轴数据";
    int ret = 0;
    if (is_start_ium_cali)
    {
        for (int i = 0; i < x.data_count; i++)
        {
            orgData.acc[0] = x.data[i].acc_x;
            orgData.acc[1] = x.data[i].acc_y;
            orgData.acc[2] = x.data[i].acc_z;
            orgData.gyro[0] = x.data[i].gyro_x;
            orgData.gyro[1] = x.data[i].gyro_y;
            orgData.gyro[2] = x.data[i].gyro_z;

            ui->gyro_x->setText("gyro_x=" + QString::number(orgData.gyro[0]));
            ui->gyro_y->setText("gyro_y=" + QString::number(orgData.gyro[1]));
            ui->gyro_z->setText("gyro_z=" + QString::number(orgData.gyro[2]));
            ui->acc_x->setText("acc_x=" + QString::number(orgData.acc[0]));
            ui->acc_y->setText("acc_y=" + QString::number(orgData.acc[1]));
            ui->acc_z->setText("acc_z=" + QString::number(orgData.acc[2]));

            ret = qimuc->imu_calib_proc(&orgData);

            qDebug() << "imu校准结果" << ret;
            if (ret == 0)
            {
                qDebug() << "校准完成" << ret;
                isimuCaliOk = 1;
                is_start_ium_cali = 0;
                break;
            }
        }
    }

    // qDebug() << "isStartSendCaliResult:"<<isStartSendCaliResult;
    // if(ret == 0&&isStartSendCaliResult)
    //    {

    //        isStartSendCaliResult=0;
    //    }
}

MainWindow::~MainWindow()
{
    is_motor_continue = false;
    isimuCaliContinue = false;
    isrssiContinue = false;
    isContinue = false;
    delete ui;
}

void MainWindow::saveCustom()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    settings.setValue("Window/Size", this->size());
}

void MainWindow::recoverCustom()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
    QVariant windowSize(availableSize / 4 * 3);
    this->resize(settings.value("Window/Size", windowSize).toSize());
    restoreState(settings.value("Window/windowState").toByteArray());
    ui->wifiUserName->setText(settings.value("WIFI/Name", "请在配置文件中设置").toString());
    ui->wifiPassword->setText(settings.value("WIFI/Password", "123445566").toString());
}

void MainWindow::initBasicInfo()
{
    struct namePair
    {
        QString name;
        QString displayName;
        QString settings;
    };
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    // 读取 ProductInfo 节下的键值对
    productName = "=" + settings.value("ProductInfo/Product_Name").toString();
    QString appProtocolVersion =
        "=" + settings.value("ProductInfo/App_Protocol_Version").toString();
    QString factoryProtocolVersion =
        "=" + settings.value("ProductInfo/Factory_Protocol_Version").toString();
    QString hardwareVersion = "=" + settings.value("ProductInfo/Hardware_Version").toString();
    QString softwareVersion = "=" + settings.value("ProductInfo/Software_Version").toString();
    QString camera_id = "=" + settings.value("ProductInfo/Camera_Id").toString();

    QString resourceVersion = "=" + settings.value("ProductInfo/Resource_Version").toString();
    QString algorithmVersion = "=" + settings.value("ProductInfo/Algorithm_Version").toString();
    QString pressureSenseVersion =
        "=" + settings.value("ProductInfo/Pressure_Sense_Version").toString();
    QString imuId = "=" + settings.value("ProductInfo/IMU_ID").toString();

    QString age_state = "=" + settings.value("ProductInfo/Age_State").toString();

    QString motor_ver = "=" + settings.value("ProductInfo/Motor_Ver").toString();

    imu_wait_time = settings.value("IMU/IMU_Wait_Time", "15000").toInt();

    music_time = settings.value("music/music_Time", "music_time").toInt();

    QList<namePair> basicItems = {{"product_name", "产品名称", productName},
                                  {"pb_phone_ver", "app协议版本号", appProtocolVersion},
                                  {"camera_id", "摄像头版本号", camera_id},
                                  {"pb_factory_ver", "工厂协议版本号", factoryProtocolVersion},
                                  {"hw_version", "硬件版本号", hardwareVersion},
                                  {"soft_version", "软件版本号", softwareVersion},
                                  {"res_version", "资源版本号", resourceVersion},
                                  {"algo_version", "算法版本号", algorithmVersion},
                                  {"presure_version", "压感版本号", pressureSenseVersion},
                                  {"ble_mac", "蓝牙的MAC", ""},
                                  {"wifi_mac", "WIFI的MAC", ""},
                                  {"imu_id", "IMU版本号", imuId},
                                  {"age_state", "老化状态", age_state},
                                  {"motor_ver", "电机版本号", motor_ver}};

    for (auto name : basicItems)
    {
        basicInfoModel->addTestItem(new TestItems(name.name, name.displayName, name.settings));
    }

    ui->baseView->setModel(basicInfoModel);
    ui->baseView->setColumnHidden(1, true);
    ui->baseView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->baseView->scrollToBottom();

    basicInfoModel->resetAllTestResult();

    QObject::connect(
        pb, &Qpb::baseInfoReady, this,
        [=](FacGetDevBaseInfo baseInfo)
        {
            basicInfoModel->getTestItemByName("product_name")
                ->setData(baseInfo.product_name, Qt::DisplayRole);
            basicInfoModel->getTestItemByName("algo_version")
                ->setData(baseInfo.algo_version, Qt::DisplayRole);
            basicInfoModel->getTestItemByName("pb_phone_ver")
                ->setData(QString("%1").arg(baseInfo.pb_phone_ver), Qt::DisplayRole);
            basicInfoModel->getTestItemByName("pb_factory_ver")
                ->setData(QString("%1").arg(baseInfo.pb_factory_ver), Qt::DisplayRole);
            basicInfoModel->getTestItemByName("hw_version")
                ->setData(baseInfo.hw_version, Qt::DisplayRole);
            basicInfoModel->getTestItemByName("soft_version")
                ->setData(QString("%1").arg(baseInfo.soft_version), Qt::DisplayRole);
            basicInfoModel->getTestItemByName("res_version")
                ->setData(baseInfo.res_version, Qt::DisplayRole);
            basicInfoModel->getTestItemByName("presure_version")
                ->setData(baseInfo.presure_version, Qt::DisplayRole);

            QString mac;

            for (int var = baseInfo.ble_mac.size - 1; var >= 0; --var)
            {
                mac += QString("%1").arg(baseInfo.ble_mac.bytes[var], 2, 16, QChar('0')).toUpper();
                if (var > 0)
                {
                    mac += ":";
                }
            }

            qDebug() << "mac结果" << mac;
            basicInfoModel->getTestItemByName("ble_mac")->setData(mac, Qt::DisplayRole);
            mac.clear();

            for (int var = 0; var < baseInfo.wifi_mac.size; ++var)
            {
                mac += QString("%1").arg(baseInfo.wifi_mac.bytes[var], 2, 16, QChar('0')).toUpper();
                if (var < baseInfo.wifi_mac.size - 1)
                {
                    mac += ":";
                }
            }

            qDebug() << "wifimac结果" << mac;
            basicInfoModel->getTestItemByName("wifi_mac")->setData(mac, Qt::DisplayRole);
            basicInfoModel->getTestItemByName("imu_id")->setData(QString("%1").arg(baseInfo.imu_id),
                                                                 Qt::DisplayRole);
            basicInfoModel->getTestItemByName("age_state")
                ->setData(QString("%1").arg(baseInfo.ageing_state), Qt::DisplayRole);
            basicInfoModel->getTestItemByName("motor_ver")
                ->setData(QString("%1").arg(baseInfo.motor_version), Qt::DisplayRole);

            writeDataToCSVFile();
        });
}

void MainWindow::initPeriphState()
{
    struct namePair
    {
        QString name;
        QString displayName;
        QString settings;
    };
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    // 读取 PeripheralStatus 节下的键值对
    QString imuStatus = "=" + settings.value("PeripheralStatus/IMU_Status").toString();
    QString flashStatus = "=" + settings.value("PeripheralStatus/Flash_Status").toString();
    QString magneticStatus = "=" + settings.value("PeripheralStatus/Magnetic_Status").toString();
    QString pressureStatus = "=" + settings.value("PeripheralStatus/Pressure_Status").toString();

    QList<namePair> items = {
        {"imu_state", "imu状态", imuStatus},
        {"flash_state", "flash状态", flashStatus},
        {"magnet_state", "地磁状态", magneticStatus},
        {"press_state", "压感状态", pressureStatus},
    };

    for (auto name : items)
    {
        peripheralModel->addTestItem(new TestItems(name.name, name.displayName, name.settings));
    }
    ui->peripheralView->scrollToBottom();
    ui->peripheralView->setModel(peripheralModel);
    ui->peripheralView->setColumnHidden(1, true);
    ui->peripheralView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    peripheralModel->resetAllTestResult();
    QObject::connect(pb, &Qpb::periphStateReady, this,
                     [=](FacGetPeriphState state)
                     {
                         peripheralModel->getTestItemByName("flash_state")
                             ->setData(QString("%1").arg(state.flash_state), Qt::DisplayRole);
                         peripheralModel->getTestItemByName("imu_state")
                             ->setData(QString("%1").arg(state.imu_state), Qt::DisplayRole);
                         peripheralModel->getTestItemByName("magnet_state")
                             ->setData(QString("%1").arg(state.magnet_state), Qt::DisplayRole);
                         peripheralModel->getTestItemByName("press_state")
                             ->setData(QString("%1").arg(state.press_state), Qt::DisplayRole);
                         writePeripheralDataToCSVFile();
                     });
}

void MainWindow::save_RSSI_data_to_csv(int intwifirssi, int intblerssi, QString wifiresult,
                                       QString bleresult)
{
    // 获取桌面路径
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    // 构建 "测试结果" 文件夹的完整路径
    QString folderPath = QDir(desktopPath).filePath("测试结果");

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists())
    {
        QDir().mkpath(folderPath);
    }

    // 构建完整的文件路径
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString filePath =
        QDir(folderPath)
            .filePath(settings.value("ProductInfo/Product_Name").toString() + "wifi蓝牙报告.csv");
    QFile file(filePath);

    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);
        // 写入表头
        QStringList headers;
        headers << "mac地址" << "时间戳" << "测试项" << "测试数据" << "测试结果";

        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        stream << headers.join(",") << "\n";
        // 写入数据
        QStringList rowData;
        rowData << macAddress << timestamp << "wifi信号强度" << QString::number(intwifirssi)
                << wifiresult;
        stream << rowData.join(",") << "\n";

        rowData.clear();   // 清空rowData，以便添加新的数据

        rowData << macAddress << timestamp << "ble信号强度" << QString::number(intblerssi)
                << bleresult;
        stream << rowData.join(",") << "\n";

        file.close();
        qDebug() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << "Error appending to file";
    }
}

void writeDataToCSV(const QString &filePath, const QStringList &headers,
                    const QList<QList<QString>> &data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
    {
        qDebug() << "无法打开文件：" << file.errorString();
        return;
    }

    QTextStream stream(&file);

    // 写入表头
    for (const QString &header : headers)
    {
        stream << header << ",";
    }
    stream << "\n";

    // 写入数据
    for (const QList<QString> &rowData : data)
    {
        for (const QString &data : rowData)
        {
            stream << data << ",";
        }
        stream << "\n";
    }

    file.close();
    qDebug() << "保存完成";
}

// 在你的代码中调用该函数
void MainWindow::writeDataToCSVFile()
{
    // 例子：写入 basicInfoModel 的数据
    QStringList basicInfoHeaders;
    QList<QList<QString>> basicInfoData;

    // 添加表头
    basicInfoHeaders << "MAC地址";
    basicInfoHeaders << "测试时间";
    for (int col = 0; col < basicInfoModel->columnCount(); ++col)
    {
        basicInfoHeaders << basicInfoModel->headerData(col, Qt::Horizontal).toString();
    }

    // 添加数据
    for (int row = 0; row < basicInfoModel->rowCount(); ++row)
    {
        QList<QString> rowData;

        rowData << macAddress;

        // 获取当前日期和时间，并以 "yyyy年MM月dd日_hh时mm分ss秒" 格式转换为中文字符串
        rowData << QDateTime::currentDateTime().toString("yyyy年MM月dd日_hh时mm分ss秒");

        for (int col = 0; col < basicInfoModel->columnCount(); ++col)
        {
            rowData << basicInfoModel->data(basicInfoModel->index(row, col), Qt::DisplayRole)
                           .toString();
        }
        basicInfoData << rowData;
    }
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
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_设备基础信息.csv";
    QString filePath = QDir(folderPath).filePath(fileName);

    // 调用函数写入 CSV 文件
    writeDataToCSV(filePath, basicInfoHeaders, basicInfoData);
}

// 在你的代码中调用该函数
void MainWindow::writePeripheralDataToCSVFile()
{
    // 例子：写入 peripheralModel 的数据
    QStringList peripheralHeaders;
    QList<QList<QString>> peripheralData;

    // 添加表头
    peripheralHeaders << "MAC地址";
    peripheralHeaders << "测试时间";
    // 添加表头
    for (int col = 0; col < peripheralModel->columnCount(); ++col)
    {
        peripheralHeaders << peripheralModel->headerData(col, Qt::Horizontal).toString();
    }

    // 添加数据
    for (int row = 0; row < peripheralModel->rowCount(); ++row)
    {
        QList<QString> rowData;
        rowData << macAddress;

        // 获取当前日期和时间，并以 "yyyy年MM月dd日_hh时mm分ss秒" 格式转换为中文字符串
        rowData << QDateTime::currentDateTime().toString("yyyy年MM月dd日_hh时mm分ss秒");
        for (int col = 0; col < peripheralModel->columnCount(); ++col)
        {
            rowData << peripheralModel->data(peripheralModel->index(row, col), Qt::DisplayRole)
                           .toString();
        }
        peripheralData << rowData;
    }

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
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_外设信息.csv";
    QString filePath = QDir(folderPath).filePath(fileName);
    // 调用函数写入 CSV 文件
    writeDataToCSV(filePath, peripheralHeaders, peripheralData);
}

void MainWindow::closeEvent(QCloseEvent *)
{
    saveCustom();
    isrssiContinue = false;
}

void MainWindow::handleDongleSerialPortError(QSerialPort::SerialPortError error)
{
    qDebug() << "DongleSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
    {
        closeDongleSerialPort();
        // QMessageBox::warning(NULL, "警告", " Dongle串口连接断开！\t\r\n");

        // ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}

void MainWindow::openDongleSerialPort()
{
    if (dongleSerialPort->isOpen())
    {
        disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
                   &MainWindow::readDongleSerialPortData);   // timeout执行真正的读取操作
        dongleSerialPort->close();
    }

    // 设置串口名
    dongleSerialPort->setPortName(ui->comNameCombo->currentText());
    // 设置波特率
    dongleSerialPort->setBaudRate(115200);
    // 设置数据位
    dongleSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    dongleSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    dongleSerialPort->setStopBits(QSerialPort::OneStop);
    dongleSerialPort->setReadBufferSize(4096);

    // 设置流控制
    dongleSerialPort->setFlowControl(QSerialPort::NoFlowControl);   // 设置为无流控制

    if (dongleSerialPort->open(QIODevice::ReadWrite))
    {
        // 启用RTS信号
        dongleSerialPort->setRequestToSend(true);
        // 启用DTR信号
        dongleSerialPort->setDataTerminalReady(true);

        // ui->msgEdit->appendPlainText("串口连接成功");
        emit refreshDongleSerialPortState(1);

        connect(dongleSerialPortTimer, &QTimer::timeout, this,
                &MainWindow::readDongleSerialPortData);   // timeout执行真正的读取操作
    }
    else
    {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // ui->msgEdit->appendPlainText("打开错误");
    }
}

void MainWindow::closeDongleSerialPort()
{
    // // 启用RTS信号
    // dongleSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // dongleSerialPort->setDataTerminalReady(false);
    if (dongleSerialPort->isOpen())
        dongleSerialPort->close();
    disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
               &MainWindow::readDongleSerialPortData);   // timeout执行真正的读取操作

    emit refreshDongleSerialPortState(0);
}

void MainWindow::getmacadress(const QByteArray &byte)
{
    receivedData += QString::fromUtf8(byte);
    // qDebug() << "内容" << receivedData;
    while (receivedData.contains("\r\n"))
    {
        int index = receivedData.indexOf("\r\n");
        QString line = receivedData.left(index);
        receivedData = receivedData.mid(index + 2);   // 删除处理过的数据
        QRegularExpression regex(
            "deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
        QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch())
        {
            QString deviceName = match.captured(1).trimmed();
            QString deviceAddress = match.captured(2).trimmed();
            QString deviceRssi = match.captured(3).trimmed();
            // qDebug() << "设备名称：" << deviceName;
            //  qDebug() << "设备地址：" << deviceAddress;
            // qDebug() << "设备RSSI：" << deviceRssi;

            // 更新 deviceMap、updateComboBox 等
            deviceMap[deviceAddress]["Name"] = deviceName;
            deviceMap[deviceAddress]["Rssi"] = deviceRssi;

            updateComboBox();
        }
        else
        {
            // qDebug() << "Invalid line:" << line;
        }
    }
}

void MainWindow::on_clear_scan_clicked()
{
    ui->mac_combo->clear();
    deviceMap.clear();
}
void MainWindow::updateComboBox()
{
    // 遍历设备信息，根据 rssi 的值进行过滤
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it)
    {
        QString deviceAddress = it.key();
        QString deviceName = it.value()["Name"];
        QString deviceRssi = it.value()["Rssi"];

        // qDebug() << "设备地址：" << deviceName<<deviceAddress<<deviceRssi;
        if (deviceName.contains(ui->name_range->text()) &&
            deviceRssi.toInt() > ui->rssi_range->text().toInt() && deviceAddress.length() == 17)
        {
            int index = ui->mac_combo->findText(deviceAddress);
            if (index == -1)
            {
                ui->mac_combo->addItem(deviceAddress);

                if (ui->is_scan_connect->checkState())
                    at->sendMac(deviceAddress);   // 发送mac地址
                qDebug() << "有新增" << deviceAddress;
            }
            index = ui->pick_device->findText(deviceAddress);

            if (index == -1)
            {
                ui->pick_device->addItem(deviceAddress);
                if (ui->is_scan_connect->checkState())
                    at->sendMac(deviceAddress);   // 发送mac地址
                qDebug() << "有新增" << deviceAddress;
            }
        }
    }
}
void MainWindow::on_mac_combo_textActivated(const QString &arg1)
{
    ui->log->clear();
    ui->msgEdit->clear();
    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch())
    {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    }
    else
    {
        macAddress = arg1;
        at->sendMac(macAddress);   // 发送mac地址
        qDebug() << macAddress;
        banding_mac_sn(macAddress, snbanding);
        on_motor_cali_clicked();
    }
}

void MainWindow::on_snbanding_returnPressed()
{
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    ui->snbanding->clear();
}

void MainWindow::banding_mac_sn(QString bandingmac, QString bandingsn)
{
    QFile file("mac_sn.txt");   // 创建一个文件对象
    if (file.open(QIODevice::ReadWrite | QIODevice::Append))
    {                                                    // 打开文件
        QTextStream out(&file);                          // 创建一个文本流对象
        out << bandingsn << "," << bandingmac << endl;   // 将sn和mac写入文件
        file.close();                                    // 关闭文件
    }
}
void MainWindow::get_mac(QString sn_to_search)
{
    QString path = "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt";
    QFileInfo fileInfo(path);
    QFile file;
    // 创建一个文件对象
    if (fileInfo.exists() && fileInfo.isFile())
    {
        file.setFileName(path);
        ui->msgEdit->appendPlainText("云端文件存在，正在打开云端文件...");
    }
    else
    {
        file.setFileName("mac_sn.txt");
        ui->msgEdit->appendPlainText("云端文件不存在，正在打开本地文件...");
    }

    if (file.open(QIODevice::ReadOnly))
    {   // 打开文件
        QTextStream in(&file);
        while (!in.atEnd())
        {                                           // 逐行读取文件
            QString line = in.readLine();           // 读取一行
            QStringList fields = line.split(",");   // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2)
            {                                 // 至少需要两个字段
                QString sn = fields.at(0);    // 第一个字段是sn
                QString mac = fields.at(1);   // 第二个字段是mac
                if (sn == sn_to_search)
                {   // 检查是否是待检索的sn
                    ui->msgEdit->appendPlainText("这是从文件获取的mac地址");

                    if (ui->is_start_ota->checkState())
                    {
                        ui->get_mac->clear();
                        ui->get_mac->setFocus();
                        ui->macInput_7->setText(mac);
                        on_macInput_7_returnPressed();
                        ui->msgEdit->appendPlainText("开始ota升级");
                    }
                    else
                    {
                        ui->macInput->setText(mac);
                        on_macInput_returnPressed();
                        qDebug() << "The corresponding mac is: " << mac;
                    }

                    break;
                }
            }
        }
        file.close();   // 关闭文件
    }
}

void MainWindow::on_get_mac_returnPressed()
{
    get_mac(ui->get_mac->text());   // 文件获取
}

void MainWindow::on_damping_open_clicked()
{
    pb->set_motor_damping_state(1);
}

void MainWindow::on_damping_close_clicked()
{
    pb->set_motor_damping_state(0);
}

void MainWindow::on_disconnectButton_clicked()
{
    closeDongleSerialPort();
    ui->refreshButton->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refresh_ble_state(0);
}

void MainWindow::on_refreshButton_clicked()
{
    ui->comNameCombo->clear();
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ui->comNameCombo->addItem(info.portName());
    }
}

void MainWindow::on_connectButton_clicked()
{
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void MainWindow::on_getBasicInfoButton_clicked()
{
    basicInfoModel->resetAllTestResult();
    pb->getBaseInfo();
}

void MainWindow::on_getperipheralButton_clicked()
{
    peripheralModel->resetAllTestResult();
    pb->getPeriphState();
}

void MainWindow::on_stopimuCaliButton_clicked()
{
    isimuCaliContinue = false;
    ui->imuCaliButton->setEnabled(true);
}

void MainWindow::save_imu_test_data_to_csv(const QString &macAddress, const QString &result)
{
    // 获取桌面路径
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    // 构建 "测试结果" 文件夹的完整路径
    QString folderPath = QDir(desktopPath).filePath("测试结果");

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists())
    {
        QDir().mkpath(folderPath);
    }

    // 构建完整的文件路径
    QString filePath = QDir(folderPath).filePath("六轴测试报告.csv");
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);
        // 检查是否是文件的开头，如果是，则写入表头
        if (file.pos() == 0)
        {
            QStringList headers;
            headers << "时间" << "mac地址" << "六轴测试结果";
            stream << headers.join(",") << "\n";
        }
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        // 写入数据
        QStringList rowData;
        rowData << timestamp;
        rowData << macAddress;
        rowData << result;
        stream << rowData.join(",") << "\n";
        file.close();
        qDebug() << "文件保存到" << filePath;
    }
    else
    {
        qDebug() << "文件没关或者其他问题";
    }
}

void MainWindow::on_lcdTestButton_clicked()
{
    // 编写屏幕测试的代码
    static int state = 0;

    if (at->getConnected())
    {
        pb->set_screen_color(state);
        ui->msgEdit->appendPlainText("已设置屏幕颜色");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }

    state++;
    if (state == 5)
        state = 0;
}

void MainWindow::on_enterShipModeButton_clicked()
{
    if (1)   //(at->getConnected())
    {
        pb->set_ship_mode(1);
        ui->msgEdit->appendPlainText("已设置进入船运");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::delay(int ms)
{
    QTime t;
    t.start();
    while (t.elapsed() < ms)
    {
        QCoreApplication::processEvents();
    }
}

void MainWindow::waitWork(int ms)
{
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
void MainWindow::on_macInput_returnPressed()
{
    clear_display();

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
        macLabel->setText("蓝牙mac: " + macAddress);
        at->sendMac(ui->macInput->text());   // 开始连接
        ui->snInput->setFocus();
        if (ui->checkbanding->checkState())
        {
            ui->macInput->clear();
        }
    }
    pb->setPbMode(1);
}
void MainWindow::sendData(bool is_random)
{
    int start = ui->startDateTime->date().day();
    int end = ui->endDateTime->date().day() + 1;
    int timestamp = ui->startDateTime->dateTime().toSecsSinceEpoch();

    for (int i = start; i < end; i++)
    {
        // 设置手柄时间为start time
        pb->set_brush_time(timestamp);
        waitWork(WAITTIME);
        // 设置刷牙数据
        uint32_t work_time[6];
        uint16_t pressure_time[6], horizon_brush[6];
        FacSetBrushRecord record;
        int brushtime = timestamp;
        for (int j = 0; j < ui->brushTimesspinBox->value(); j++)
        {
            if (is_random)
            {
                SendRadomDataPushButton();
                waitWork(10);
            }
            brushtime += 3600 * 3;
            record.timestamp = brushtime;
            record.plaque = ui->spinBox->value();

            QRegularExpression regex("\\d{1,},\\d{1,},\\d{1,},\\d{1,},\\d{1,},\\d{1,}");
            QRegularExpressionMatch match = regex.match(ui->brushTime->text());
            int i = 0;
            if (match.hasMatch())
            {
                QString matchedText = match.captured(0);
                QStringList numbers = matchedText.split(",");
                for (const QString &number : numbers)
                {
                    work_time[i++] = number.toInt();
                    qDebug() << "time" << number.toInt();
                }
            }
            match = regex.match(ui->pressureTime->text());
            i = 0;
            if (match.hasMatch())
            {
                QString matchedText = match.captured(0);
                QStringList numbers = matchedText.split(",");
                for (const QString &number : numbers)
                {
                    pressure_time[i++] = number.toInt();
                    qDebug() << "time" << number.toInt();
                }
            }
            match = regex.match(ui->horizalBrushTime->text());
            i = 0;
            if (match.hasMatch())
            {
                QString matchedText = match.captured(0);
                QStringList numbers = matchedText.split(",");
                for (const QString &number : numbers)
                {
                    horizon_brush[i++] = number.toInt();
                    qDebug() << "time" << number.toInt();
                }
            }
            record.work_time.size = 24;
            memcpy(record.work_time.bytes, work_time, 24);
            record.pressure_time.size = 12;
            memcpy(record.pressure_time.bytes, pressure_time, 12);
            record.horizon_brush.size = 12;
            memcpy(record.horizon_brush.bytes, horizon_brush, 12);
            pb->set_brush_record(record);
            ui->msgTest->appendPlainText("发送时间:" +
                                         QDateTime::currentDateTime().toString(Qt::ISODate));
            ui->msgTest->appendPlainText("");
            ui->msgTest->appendPlainText(
                "手柄时间:" + QDateTime::fromSecsSinceEpoch(timestamp).toString(Qt::ISODate));
            ui->msgTest->appendPlainText(
                "刷牙记录时间:" +
                QDateTime::fromSecsSinceEpoch(record.timestamp).toString(Qt::ISODate));
            ui->msgTest->appendPlainText(QString("牙菌斑:%1").arg(ui->spinBox->value()));
            ui->msgTest->appendPlainText("刷牙时长:" + ui->brushTime->text());
            ui->msgTest->appendPlainText("过压时长:" + ui->pressureTime->text());
            ui->msgTest->appendPlainText("横刷时长:" + ui->horizalBrushTime->text());
            ui->msgTest->appendPlainText("\n");
            waitWork(500);
        }
        timestamp += 86400;   // 增加一天
    }
}
void MainWindow::on_snInput_returnPressed()
{
    // 检查是否是序列号格式
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString snPattern = settings.value("Regex/SNPattern", "^[0-9a-zA-Z]{18}$").toString();
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->snInput->text()).hasMatch())
    {
        ui->msgEdit->appendPlainText("序列号错误");
        ui->snInput->clear();
        return;
    }

    stringsn = ui->snInput->text();
    QByteArray sn = ui->snInput->text().toUtf8();

    if (ui->checkbanding->checkState())
    {
        pb->send_sn(FacDevInfoType_TAIL_SN, sn);
        ui->msgEdit->appendPlainText("已绑定保存");
        ui->macInput->setFocus();
        ui->snInput->clear();
    }
    else
    {
        // 编写SN绑定的代码
        // 先判断蓝牙是否连接上，如果没连接上，不能绑定

        if (at->getConnected())
        {
            pb->send_sn(FacDevInfoType_TAIL_SN, sn);

            band_sn_mac_to_csv(macAddress, sn);
            ui->msgEdit->appendPlainText("已绑定保存");
        }
        else
        {
            ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
        }
        waitWork(WAITTIME);
        on_getBasicInfoButton_clicked();
        waitWork(WAITTIME);
        on_getperipheralButton_clicked();
    }
}

void MainWindow::band_sn_mac_to_csv(const QString &macAddress, const QString &sn)
{
    // 获取桌面路径
    QString desktopPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    // 构建 "测试结果" 文件夹的完整路径
    QString folderPath = QDir(desktopPath).filePath("测试结果");

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists())
    {
        QDir().mkpath(folderPath);
    }

    // 构建完整的文件路径
    QString filePath = QDir(folderPath).filePath("绑定文件.csv");
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);
        // 检查是否是文件的开头，如果是，则写入表头
        if (file.pos() == 0)
        {
            QStringList headers;
            headers << "时间" << "mac地址" << "sn码";
            stream << headers.join(",") << "\n";
        }
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        // 写入数据
        QStringList rowData;
        rowData << timestamp;
        rowData << macAddress;
        rowData << sn;
        stream << rowData.join(",") << "\n";
        file.close();
        qDebug() << "文件保存到" << filePath;
    }
    else
    {
        qDebug() << "文件没关或者其他问题";
    }
}

void MainWindow::clear_display()
{
    ui->gyro_x->setText("gyro_x=");
    ui->gyro_y->setText("gyro_y=");
    ui->gyro_z->setText("gyro_z=");
    ui->acc_x->setText("acc_x=");
    ui->acc_y->setText("acc_y=");
    ui->acc_z->setText("acc_z=");
    ui->battary_state->setText("充电状态为：");

    ui->battary_value->setText("电量为：");
    ui->battary_voltage->setText("电压为：");
    ui->msgEdit->clear();
    ui->log->clear();
    peripheralModel->resetAllTestResult();
    basicInfoModel->resetAllTestResult();
}

void MainWindow::on_enterBurningMode_clicked()
{
    if (at->getConnected())
    {
        if (ui->burningModeCombo->currentText() == "老化1")
            pb->set_burning_mode(1, FacSwitch_OPEN);
        if (ui->burningModeCombo->currentText() == "老化2")
            pb->set_burning_mode(2, FacSwitch_OPEN);
        if (ui->burningModeCombo->currentText() == "老化3")
            pb->set_burning_mode(3, FacSwitch_OPEN);
        if (ui->burningModeCombo->currentText() == "老化4")
            pb->set_burning_mode(4, FacSwitch_OPEN);
        ui->msgEdit->appendPlainText("已设置老化");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}
void MainWindow::on_pushButton_2_clicked()
{
    FacSetBrushRecord record;

    // specialmode 1641600010 0 120000 0 5000 0 0 0 1000 0 0 0 0 1500 1000 0 0 0 0 1000
    QRegularExpression regex("(\\d+)");
    QRegularExpressionMatchIterator matchIterator = regex.globalMatch(ui->lineEdit->text());
    QList<int> data;
    while (matchIterator.hasNext())
    {
        QRegularExpressionMatch match = matchIterator.next();
        QString number = match.captured(1);
        data << number.toInt();
    }

    if (data.size() == 20)
    {
        record.timestamp = data[0];
        record.plaque = data[1];
        int n = 2;
        for (int i = 0; i < 6; i++)
        {
            record.work_time.bytes[i] = data[n++];
        }

        for (int i = 0; i < 6; i++)
        {
            record.pressure_time.bytes[i] = data[n++];
        }

        for (int i = 0; i < 6; i++)
        {
            record.horizon_brush.bytes[i] = data[n++];
        }
        record.work_time.size = 24;
        record.pressure_time.size = 12;
        record.horizon_brush.size = 12;
        pb->set_brush_record(record);

        ui->msgTest->appendPlainText("发送时间:" +
                                     QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    else
    {
        ui->msgTest->appendPlainText("输入错误");
    }
}

void MainWindow::on_exitBurningMode_clicked()
{
    if (at->getConnected())
    {
        pb->set_burning_mode(1, FacSwitch_CLOSE);
        ui->msgEdit->appendPlainText("已退出老化模式");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_music_play_clicked()
{
    QString musicFileName = ui->music_combo->currentText();
    qDebug() << "文件名为" << musicFileName;
    QByteArray musicFileNameBytes = musicFileName.toUtf8();
    // 获取可用的音频输入设备列表
    QList<QAudioDeviceInfo> devices = QAudioDeviceInfo::availableDevices(QAudio::AudioInput);
    // 询问用户要使用哪个音频设备，并显示设备信息
    QStringList deviceList;
    for (const QAudioDeviceInfo &deviceInfo : devices)
    {
        deviceList.append(deviceInfo.deviceName());
    }
    bool ok;
    QString deviceName = QInputDialog::getItem(this, tr("选择音频输入设备"), tr("音频输入设备:"),
                                               deviceList, 0, false, &ok);
    if (!ok)
    {
        // 如果用户取消了选择设备，则退出函数
        return;
    }
    qDebug() << "Selected device: " << deviceName;
    // 查找选择的设备并设置为录音设备
    QAudioDeviceInfo selectedDevice;
    for (const QAudioDeviceInfo &deviceInfo : devices)
    {
        if (deviceInfo.deviceName() == deviceName)
        {
            selectedDevice = deviceInfo;
            break;
        }
    }
    if (selectedDevice.isNull())
    {
        qDebug() << "Selected device not found!";
        return;
    }

    // 初始化 QAudioRecorder
    audioRecorder = new QAudioRecorder(this);
    // 设置音频配置
    QAudioEncoderSettings audioSettings;
    audioSettings.setCodec("audio/pcm");
    audioSettings.setSampleRate(44100);
    audioSettings.setBitRate(16);
    audioSettings.setChannelCount(1);
    audioSettings.setQuality(QMultimedia::HighQuality);
    audioRecorder->setEncodingSettings(audioSettings);
    // 设置录音设备
    audioRecorder->setAudioInput(selectedDevice.deviceName());
    // 设置输出路径并开始录制
    audioRecorder->setOutputLocation(QUrl::fromLocalFile(generateOutputFilePath()));
    QTimer::singleShot(music_time, this, SLOT(stopRecording()));
    ui->msgEdit->appendPlainText("开始录音中");
    audioRecorder->record();
    pb->set_music(musicFileNameBytes);
}

void MainWindow::on_just_music_clicked()
{
    QString musicFileName = ui->music_combo->currentText();
    qDebug() << "文件名为" << musicFileName;
    QByteArray musicFileNameBytes = musicFileName.toUtf8();
    pb->set_music(musicFileNameBytes);
}
void MainWindow::stopRecording()
{
    ui->msgEdit->appendPlainText("录音结束");
    audioRecorder->stop();
}

void MainWindow::on_entersleep_clicked()
{
    if (at->getConnected())
    {
        //  pb->setDevForbidSleepState(FacSwitch_CLOSE);
        pb->set_sleeep(FacSwitch_OPEN);
        // waitWork(100);
        // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
        waitWork(50);
        // on_disconnectButton_clicked();
        // 3C:84:27:07:A8:D2

        ui->msgEdit->appendPlainText("已设置休眠");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_forbidsleep_clicked()
{
    pb->setDevForbidSleepState(FacSwitch_OPEN);
    if (at->getConnected())
    {
        ui->msgEdit->appendPlainText("已禁止休眠");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_fac_mode_clicked()
{
    if (1)
    {
        pb->set_fac_mode(1);
        ui->msgEdit->appendPlainText("设置进入工厂模式");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_getbattary_clicked()
{
    if (at->getConnected())
    {
        pb->getbattary();
        ui->msgEdit->appendPlainText("正在获取牙刷电量");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_getwifi_clicked()
{
    if (at->getConnected())
    {
        pb->getwifistate();
        ui->msgEdit->appendPlainText("正在获取wifi设置信息");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_disconnectwifi_clicked()
{
    if (at->getConnected())
    {
        pb->disconnect_wifi();
        ui->msgEdit->appendPlainText("已设置断开wifi");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}
void MainWindow::on_connectwifi_clicked()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString wifiName = settings.value("WIFI/Name").toString();
    QString wifiPassword = settings.value("WIFI/Password").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected())
    {
        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
        ui->msgEdit->appendPlainText("已设置连接wifi");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_rebot_clicked()
{
    if (1)
    {
        pb->setDevRstState();
        ui->msgEdit->appendPlainText("重启");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_exit_fac_mode_clicked()
{
    if (at->getConnected())
    {
        pb->set_fac_mode(0);
        ui->msgEdit->appendPlainText("设置退出工厂模式");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_fre_returnPressed()
{
    ui->duty->setFocus();
}

void MainWindow::on_duty_returnPressed()
{
    uint32_t fre = ui->fre->text().toUInt();
    float duty = ui->duty->text().toFloat();

    if (duty < 0 || duty > 100)
    {
        QMessageBox::warning(NULL, "警告", " 占空比输入错误！\r\n");
        return;
    }
    if (fre < 100 || fre > 1000)
    {
        QMessageBox::warning(NULL, "警告", " 频率输入错误！\r\n");
        return;
    }

    pb->set_motor_param(fre, duty);

    if (ui->open_motor_set->isChecked())
    {
        waitWork(WAITTIME);
        pb->set_motor_state(1);
    }
}

void MainWindow::on_close_motor_clicked()
{
    pb->set_motor_state(0);
}

void MainWindow::on_start_motor_clicked()
{
    pb->set_motor_state(1);
}

void MainWindow::on_start_wifible_test_clicked()
{
    typedef enum
    {
        STATE_IDLE,                        // 休眠状态
        STATE_WATI_GET_CORRECT_WIFIRSSI,   // 等待正确的wifi信号强度
        STATE_WATI_GET_CORRECT_BLERSSI,    // 等待正确的蓝牙信号强度
        STATE_SAVE_RESULT                  // 保存结果在本地
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
    while (isrssiContinue)
    {
        switch (state)
        {
        case STATE_IDLE:   // 复位一切
            rssitestcount = 0;
            rssitestfailcount = 0;
            state = STATE_WATI_GET_CORRECT_WIFIRSSI;
            ui->wifi_test_result->setText("WIFI:WAIT");
            ui->wifi_test_result->setStyleSheet(
                "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
            ui->ble_test_result->setText("BLE:WAIT");
            ui->ble_test_result->setStyleSheet(
                "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
            on_connectwifi_clicked();
            waitWork(2000);

            break;

        case STATE_WATI_GET_CORRECT_WIFIRSSI:
        {
            intwifirssi = WIFI_RSSI.toInt();
            if (intwifirssi < HighRssi && intwifirssi > LowRssi)   // wifi信号可以
            {
                rssitestcount++;

                if (rssitestcount > RssiTestTime)   // wifi信号可以
                {
                    ui->wifi_test_result->setText("WIFI:PASS");
                    ui->wifi_test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
                    pb->disconnect_wifi();
                    wifiresult = "通过";
                    at->sendBLELOG(1);   // 日志关
                    state = STATE_WATI_GET_CORRECT_BLERSSI;
                    rssitestcount = 0;
                }
                break;
            }
            else
            {
                rssitestcount = 0;
                rssitestfailcount++;
                if (rssitestfailcount > RssiTestTime)   // wifi信号不可以
                {
                    wifiresult = "失败";
                    ui->wifi_test_result->setText("WIFI：FAIL");
                    ui->wifi_test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

                    qDebug() << "wifi不合格信号强度" << WIFI_RSSI;
                    ui->log->appendPlainText("wifi不合格信号强度" + WIFI_RSSI);
                    rssitestfailcount = 0;
                    at->sendBLELOG(1);   // 日志关
                    state = STATE_WATI_GET_CORRECT_BLERSSI;
                }
            }
        }

        break;

        case STATE_WATI_GET_CORRECT_BLERSSI:
        {
            intblerssi = BLE_RSSI.toInt();
            if (intblerssi < BleHighRssi && intblerssi > BleLowRssi)   // 蓝牙信号可以
            {
                rssitestcount++;
                if (rssitestcount > RssiTestTime)   // 蓝牙信号可以
                {
                    ui->ble_test_result->setText("BLE:PASS");
                    ui->ble_test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
                    state = STATE_SAVE_RESULT;
                    rssitestcount = 0;
                    bleresult = "通过";
                    at->sendBLELOG(0);   // 日志关
                }
                break;
            }
            else
            {
                rssitestcount = 0;
                rssitestfailcount++;

                if (rssitestfailcount > RssiTestTime)   // 蓝牙信号不可以
                {
                    ui->ble_test_result->setText("BLE：FAIL");
                    ui->ble_test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

                    bleresult = "失败";
                    qDebug() << "蓝牙不合格信号强度" << BLE_RSSI;
                    ui->log->appendPlainText("蓝牙不合格信号强度" + BLE_RSSI);
                    state = STATE_SAVE_RESULT;
                    at->sendBLELOG(0);   // 日志关
                    rssitestfailcount = 0;
                }
            }
        }
        break;
        case STATE_SAVE_RESULT:
        {
            save_RSSI_data_to_csv(intwifirssi, intblerssi, wifiresult, bleresult);
            state = STATE_IDLE;
            isrssiContinue = false;
        }
        break;
        }
        waitWork(100);
        QCoreApplication::processEvents();
    }
}
void MainWindow::SendRecord()
{
    uint32_t work_time[6];
    uint16_t pressure_time[6], horizon_brush[6];
    FacSetBrushRecord record;
    // record.timestamp = ui->dateTimeTest->dateTime().toSecsSinceEpoch();
    record.plaque = ui->spinBox->value();

    QRegularExpression regex("\\d{1,},\\d{1,},\\d{1,},\\d{1,},\\d{1,},\\d{1,}");
    QRegularExpressionMatch match = regex.match(ui->brushTime->text());
    int i = 0;
    if (match.hasMatch())
    {
        QString matchedText = match.captured(0);
        QStringList numbers = matchedText.split(",");
        for (const QString &number : numbers)
        {
            work_time[i++] = number.toInt();
            qDebug() << "time" << number.toInt();
        }
    }

    match = regex.match(ui->pressureTime->text());
    i = 0;
    if (match.hasMatch())
    {
        QString matchedText = match.captured(0);
        QStringList numbers = matchedText.split(",");
        for (const QString &number : numbers)
        {
            pressure_time[i++] = number.toInt();
            qDebug() << "time" << number.toInt();
        }
    }

    match = regex.match(ui->horizalBrushTime->text());
    i = 0;
    if (match.hasMatch())
    {
        QString matchedText = match.captured(0);
        QStringList numbers = matchedText.split(",");
        for (const QString &number : numbers)
        {
            horizon_brush[i++] = number.toInt();
            qDebug() << "time" << number.toInt();
        }
    }

    record.work_time.size = 24;
    memcpy(record.work_time.bytes, work_time, 24);

    record.pressure_time.size = 12;
    memcpy(record.pressure_time.bytes, pressure_time, 12);

    record.horizon_brush.size = 12;
    memcpy(record.horizon_brush.bytes, horizon_brush, 12);

    pb->set_brush_record(record);

    ui->msgTest->appendPlainText("发送时间:" + QDateTime::currentDateTime().toString(Qt::ISODate));
    ui->msgTest->appendPlainText("记录生成时间:" +
                                 ui->dateTimeBrushSet->dateTime().toString(Qt::ISODate));
    ui->msgTest->appendPlainText(QString("牙菌斑:%1").arg(ui->spinBox->value()));
    ui->msgTest->appendPlainText("刷牙时长:" + ui->brushTime->text());
    ui->msgTest->appendPlainText("过压时长:" + ui->pressureTime->text());
    ui->msgTest->appendPlainText("横刷时长:" + ui->horizalBrushTime->text());
    ui->msgTest->appendPlainText("\n");
}

void MainWindow::SendRadomDataPushButton()
{
    QString work_time;
    for (int i = 0; i < 6; i++)
    {
        int randomNumber = QRandomGenerator::global()->bounded(30000, 100001);
        work_time += QString("%1").arg(randomNumber);
        if (i != 5)
        {
            work_time += QString(",");
        }
    }

    ui->brushTime->setText(work_time);

    QString pressure_time;
    for (int i = 0; i < 6; i++)
    {
        int randomNumber = QRandomGenerator::global()->bounded(1000, 30000);
        pressure_time += QString("%1").arg(randomNumber);
        if (i != 5)
        {
            pressure_time += QString(",");
        }
    }

    ui->pressureTime->setText(pressure_time);

    QString horizantol_time;
    for (int i = 0; i < 6; i++)
    {
        int randomNumber = QRandomGenerator::global()->bounded(1000, 30000);
        horizantol_time += QString("%1").arg(randomNumber);
        if (i != 5)
        {
            horizantol_time += QString(",");
        }
    }

    ui->horizalBrushTime->setText(horizantol_time);
    ui->spinBox->setValue(QRandomGenerator::global()->bounded(0, 100));
}

void MainWindow::on_setTimePushButton_clicked()
{
    int timestamp = ui->dateTimeBrushSet->dateTime().toSecsSinceEpoch();
    pb->set_brush_time(timestamp);
}

void MainWindow::on_resetPushButton_clicked()
{
    pb->set_brush_reset();
}
void MainWindow::on_sendDataPushButton_clicked()
{
    sendData(false);
}

void MainWindow::on_sendRandomData_clicked()
{
    sendData(true);
}

void MainWindow::on_clearDataPushButton_clicked()
{
    ui->msgTest->clear();
}
void MainWindow::on_imuCaliButton_clicked()   // 编写六轴校准的代码
{
    QString result = "";
    QString passValue = "通过";
    QString failValue = "失败";

    ui->imuCaliButton->setEnabled(false);

    typedef enum
    {
        STATE_IDLE,                  // 休眠状态
        STATE_WATI_CONNECT,          // 等待连接
        STATE_DISABLE_SLEEP_1,       // 进入禁止休眠
        STATE_SET_COLLECT_PARAM_1,   // 获取采集参数
        STATE_CAIL,                  // 开始校准
        STATE_SEND_CAIL_RESULT,      // 发送校准结果
        STATE_SAVE_RESULT            // 保存结果在本地
    } State;
    State state = STATE_IDLE;
    // 主状态机流程
    isimuCaliContinue = true;
    while (isimuCaliContinue)
    {
        switch (state)
        {
        case STATE_IDLE:   // 复位一切

            isovertime = 0;
            isimuCaliOk = 0;   // 是否校准完成
            is_start_ium_cali = 0;
            isStartSendCaliResult = 0;   // 是否开始发送校验结果
            pb->reset_all_pb();

            if (!at->getConnected())
            {
                at->resetConnected();
                at->sendMac(ui->macInput->text());   // 发送mac地址
            }

            state = STATE_WATI_CONNECT;
            break;

        case STATE_WATI_CONNECT:   // 设置禁止休眠
            if (at->getConnected())
            {
                ui->msgEdit->appendPlainText("蓝牙连接成功");
                refresh_ble_state(1);
                state = STATE_DISABLE_SLEEP_1;
            }
            break;

        case STATE_DISABLE_SLEEP_1:   // 设置设备采集
            if (pb->getDisableSleep())
            {
                ui->msgEdit->appendPlainText("已进入禁止休眠");
                pb->setimuCollectParam(FacSwitch_START);
                state = STATE_CAIL;
            }
        case STATE_CAIL:   // 开始校准
            if (pb->get_isSetimuCollectParam())
            {
                ui->msgEdit->appendPlainText("已设置imu采集参数");
                qimuc->imu_calib_init();
                is_start_ium_cali = 1;
                qDebug() << "开始校准";
                ui->msgEdit->appendPlainText("等待校准");
                waittime->start(imu_wait_time);
                state = STATE_SEND_CAIL_RESULT;
            }
            break;

        case STATE_SEND_CAIL_RESULT:   // 开始发送校准值
            if (isimuCaliOk)
            {
                ui->msgEdit->appendPlainText("六轴校准成功");
                result = passValue;
                // isStartSendCaliResult=1;//发送校准值
                // FacUploadNineAlex x;
                // getimuData(x);
                pb->sendimuCaliResult(qimuc->calData);
                waitWork(WAITTIME);
                pb->getimuCaliResult();
                waitWork(WAITTIME);
                pb->setimuCollectParam(FacSwitch_STOP);
                state = STATE_SAVE_RESULT;
                waittime->stop();
            }
            if (isovertime)
            {
                ui->msgEdit->appendPlainText("六轴校准失败：超时");
                pb->setimuCollectParam(FacSwitch_STOP);
                state = STATE_SAVE_RESULT;
                result = failValue;
            }

            break;

        case STATE_SAVE_RESULT:
            if (pb->get_is_get_imu_cali_data())
            {
                save_imu_test_data_to_csv(macAddress, result);
                ui->msgEdit->appendPlainText("六轴校准保存完毕");
                ui->msgEdit->appendPlainText("六轴校准结束");
                isimuCaliContinue = false;   // 结束
                ui->imuCaliButton->setEnabled(true);
                state = STATE_IDLE;
            }
            if (result == failValue)
            {
                save_imu_test_data_to_csv(macAddress, result);
                ui->msgEdit->appendPlainText("六轴校准保存完毕");
                ui->msgEdit->appendPlainText("六轴校准结束");
                isimuCaliContinue = false;   // 结束
                ui->imuCaliButton->setEnabled(true);
                state = STATE_IDLE;
            }
            break;
        default:

            break;
        }
        QCoreApplication::processEvents();
    }
}
void MainWindow::showlog(QString msg)
{
    ui->msgEdit->appendPlainText(msg);
    qDebug() << "mainwindow的" << msg;
}

void MainWindow::on_buruing1_clicked()
{
    static int i;
    if (i)
    {
        i = 0;
        pb->set_burning_mode(5, FacSwitch_OPEN);
    }
    else
    {
        i = 1;
        pb->set_burning_mode(5, FacSwitch_CLOSE);
    }
}

void MainWindow::on_get_device_sn_clicked()
{
    pb->get_sn(FacDevInfoType_TAIL_SN);
}

void MainWindow::on_close_imu_collect_clicked()
{
    pb->setimuCollectParam(FacSwitch_STOP);
}

void MainWindow::on_open_imu_collect_clicked()
{
    pb->setimuCollectParam(FacSwitch_START);
}

void MainWindow::on_motor_cali_clicked()
{
    QMessageBox::StandardButton reply;
    is_motor_continue = true;
    motorstate = STATE_IDLE;
    uint32_t value;
    while (is_motor_continue)
    {
        switch (motorstate)
        {
        case STATE_IDLE:   // 复位一切
            ui->msgEdit->appendPlainText("开始测试");
            pb->reset_all_pb();
            at->resetConnected();
            // refresh_ble_state(0);
            stringsn = "";
            motorresult = passValue;
            motorstate = STATE_WATI_CONNECT;
            break;
        case STATE_WATI_CONNECT:
            if (at->getConnected())
            {
                motorstate = STATE_DISABLE_SLEEP_1;
                ui->msgEdit->appendPlainText("蓝牙已连接");
            }
            break;

        case STATE_DISABLE_SLEEP_1:
            if (pb->getDisableSleep())
            {
                ui->msgEdit->appendPlainText("已进入禁止休眠模式");

                on_getBasicInfoButton_clicked();
                waitWork(100);
                on_getperipheralButton_clicked();
                waitWork(100);
                bool ok = false;
                value = ui->pcba_motor_cali_param->text().mid(0, 8).toUInt(
                    &ok, 16);   // 将十六进制字符串转换为
                if (ok)
                {
                    qDebug() << value;   // 输出 38822029，即十六进制字符串 "003bdf6d" 对应的数值
                }
                else
                {
                    qDebug() << "Invalid input string";
                }
                waitWork(WAITTIME);
                pb->set_motor_cali_result_param(value);

                motorstate = MOTOR_CALI_DATA_SET;
            }
            else
            {
                waitWork(500);
                pb->setDevForbidSleepState(FacSwitch_OPEN);
                ui->msgEdit->appendPlainText("已重发禁止休眠");
            }
            break;

        case MOTOR_CALI_DATA_SET:
            if (pb->get_is_motor_cali_data_set())
            {
                if (ui->is_test_camera->checkState())
                {
                    motorstate = CAMERA_TEST;
                    pb->set_screen_camera_state(1);
                }
                else
                {
                    pb->set_sevor_motor_param(14, 12, 5.2, 190);
                    ui->msgEdit->appendPlainText("已经发送电机测试指令");
                    motorstate = MOTOR_TESTING;
                }
            }
            else
            {
                pb->set_motor_cali_result_param(value);
                ui->msgEdit->appendPlainText("已重发电机校准参数");
                waitWork(500);
            }
            break;

        case CAMERA_TEST:
            waitWork(500);
            if (pb->getis_camera_control())
            {
                reply = QMessageBox::question(this, "摄像头测试", "摄像头正常吗？",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    motorresult = failValue;
                }
                ui->msgEdit->appendPlainText("已经发送电机测试指令");
                pb->set_sevor_motor_param(14, 12, 5.2, 190);
                motorstate = MOTOR_TESTING;
            }
            else
            {
                waitWork(500);

                pb->set_screen_camera_state(1);
                ui->msgEdit->appendPlainText("已重发开启摄像头");
            }
            break;
        case MOTOR_TESTING:
            if (pb->get_is_motor_test_state())
            {
                reply = QMessageBox::question(this, "电机测试", "电机正常吗？",
                                              QMessageBox::Yes | QMessageBox::No);
                if (reply == QMessageBox::No)
                {
                    motorresult = failValue;
                }
                motorstate = STATE_SAVE_RESULT;
            }
            else
            {
                waitWork(500);
                pb->set_sevor_motor_param(14, 12, 5.2, 190);
            }
            break;

        case STATE_SAVE_RESULT:

            if (motorresult == passValue)
            {
                QString mesresult = "PASS";
                save_motor_to_csv(stringsn, macAddress, mesresult);
                QString itemvalue = QString("|MOTOR_TEST:PASS|");
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
            }
            else if (motorresult == failValue)
            {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
            }
            pb->set_sevor_motor_param(0, 0, 0, 0);
            stringsn = "";
            ui->macInput->clear();
            // ui->macInput->setFocus();
            waitWork(500);
            at->sendMac("00:00:00:00:00:00");   // 发送mac地址
            waitWork(50);
            on_disconnectButton_clicked();
            ui->msgEdit->appendPlainText("测试结束");
            is_motor_continue = false;

            motorstate = STATE_IDLE;
            break;
        }

        QCoreApplication::processEvents();
    }
}

void MainWindow::on_bleTestPushButton_clicked()
{
    isContinue = true;
    ui->bleTestPushButton->setEnabled(false);
    ui->configWifiPushButton->setEnabled(false);
    ui->otaTestPushButton->setEnabled(false);
    int times = 0;

    while (isContinue)
    {
        on_macInput_7_returnPressed();
        waitWork(ui->testPeriodSpin->value() * 1000);
        ui->testMsg->appendPlainText(QString(""));
        ui->testMsg->appendPlainText(
            QString("%1").arg(QDateTime::currentDateTime().toString(Qt::ISODate)));
        ui->testMsg->appendPlainText(QString("test times:%1").arg(times++));
        // on_getBasicInfoButton_clicked();
    }

    ui->bleTestPushButton->setEnabled(true);
    ui->configWifiPushButton->setEnabled(true);
    ui->otaTestPushButton->setEnabled(true);
}

void MainWindow::on_stopTestPushButton_clicked()
{
    isContinue = false;
}
void MainWindow::refresh_pb_data(QString data)
{
    ui->msgEdit->appendPlainText(data);
}
void MainWindow::on_otaTestPushButton_clicked()
{
    isContinue = true;
    ui->bleTestPushButton->setEnabled(false);
    ui->configWifiPushButton->setEnabled(false);
    ui->otaTestPushButton->setEnabled(false);
    ui->otaTestPushButton_2->setEnabled(false);
    int times = 1;
    bool finish = false;
    QTime timeout;
    QTime totalTime;
    bool refresh = false;
    bool result = false;
    bool isStart = false;
    at->resetConnected();
    at->sendotaMac(ui->macInput_7->text());
    pb->setPbMode(0);
    timeout.start();

    while (at->getConnected() == false)
    {
        waitWork(100);
        if (timeout.elapsed() > 1000 * 30)
        {
            ui->testMsg->appendPlainText("蓝牙连接超时");
            isContinue = false;
        }
        if (isContinue == false)
            break;
    }
    waitWork(1000);
    ui->progressBar->show();
    ui->progressBar->setMaximum(100);
    connect(pb, SIGNAL(sendInfo(QString)), ui->testMsg, SLOT(appendPlainText(QString)));
    connect(pb, &Qpb::sendOTAProgress,
            [&](int s)
            {
                ui->progressBar->setValue(s);
                refresh = true;
            });
    connect(pb, &Qpb::sendOTAResult,
            [&](int r)
            {
                finish = true;
                qDebug() << r;
                if (r == 11 || r == 0)
                {
                    result = true;
                }
                if (r == 0)
                {
                    isStart = true;
                }
            });

    while (isContinue)
    {
        if (!dongleSerialPort->isOpen())
        {
            on_connectButton_clicked();
        }
        ui->progressBar->setValue(0);
        pb->startOTA();
        timeout.restart();
        isStart = false;
        ui->testMsg->appendPlainText("----------------");
        ui->testMsg->appendPlainText("OTA启动，开始计时");
        int counter = 0;
        while (timeout.elapsed() < 30 * 3000)
        {
            if (counter % 2 == 0)
            {
                pb->startOTA();
            }
            if (isStart == true)
            {
                break;
            }
            waitWork(1000);
            counter++;
            if (isContinue == false)
                break;
        }
        if (isStart == false)
        {
            isContinue = false;
            ui->testMsg->appendPlainText("设备无法接收蓝牙指令");
        }

        totalTime.start();
        finish = false;
        result = false;

        while (finish == false)
        {
            if (timeout.elapsed() > 3 * 60 * 1000)
            {
                ui->testMsg->appendPlainText("下载超时");
                finish = true;
                break;
            }
            waitWork(200);
            if (isContinue == false)
                break;
            if (refresh)
            {
                refresh = false;
                timeout.restart();
            }
            if (at->getConnected() == false && timeout.elapsed() > 10000)
            {
                finish = true;
            }
            ui->lcdNumber->display(totalTime.elapsed() / 1000);
        }

        ui->testMsg->appendPlainText(QString("升级次数:%1").arg(times++));
        ui->testMsg->appendPlainText(QString("耗时:%1 s").arg(totalTime.elapsed() / 1000));
        if (result == false)
        {
            ui->testMsg->appendPlainText("升级失败");
        }
        else
        {
            ui->testMsg->appendPlainText("升级成功");
        }

        waitWork(ui->testPeriodSpin->value() * 1000);
        if (at->getConnected() == false)
        {
            timeout.restart();
            while (at->getConnected() == false)
            {
                waitWork(3000);
                at->getConnected();
                if (timeout.elapsed() > 1000 * 60 * 2)
                {
                    ui->testMsg->appendPlainText("蓝牙连接超时");
                    isContinue = false;
                }

                if (isContinue == false)
                    break;
            }
        }
    }
    disconnect(pb, SIGNAL(sendInfo(QString)), ui->testMsg, SLOT(appendPlainText(QString)));
    ui->otaTestPushButton_2->setEnabled(true);
    ui->progressBar->hide();
    ui->bleTestPushButton->setEnabled(true);
    ui->configWifiPushButton->setEnabled(true);
    ui->otaTestPushButton->setEnabled(true);
}

void MainWindow::save_motor_to_csv(QString SN, QString Mac, QString csvresult)
{
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
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_电机测试报告.csv";
    QString filePath = QDir(folderPath).filePath(fileName);

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
    {
        QTextStream stream(&file);

        // 写入表头
        QStringList headers;
        headers << "sn" << "上位机版本" << "mac地址" << "时间戳" << "测试项" << "测试结果";

        // 获取当前时间戳
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        stream << headers.join(",") << "\n";

        // 写入数据
        QStringList rowData;
        rowData << SN << Mac << timestamp << "电机测试" << csvresult;
        stream << rowData.join(",") << "\n";
        rowData.clear();   // 清空rowData，以便添加新的数据
        file.close();
        qDebug() << "Data appended to" << filePath;
    }
    else
    {
        qDebug() << "Error appending to file";
    }
}

void MainWindow::on_configWifiPushButton_clicked() {}
void MainWindow::on_end_motor_cali_clicked()
{
    is_motor_continue = false;
    motorstate = STATE_IDLE;
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
}
void MainWindow::on_getInfoPushButton_clicked()
{
    pb->getConnectInfo();
}

void MainWindow::on_start_scan_clicked()
{
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    ui->mac_combo->clear();
    deviceMap.clear();
    on_motor_cali_clicked();
    ui->msgEdit->appendPlainText("已经触发");
}

void MainWindow::on_enterwhitemode_clicked()
{
    pb->set_device_mode(4);
}

void MainWindow::on_close_camera_clicked()
{
    pb->set_camera_state(0);
}

void MainWindow::on_open_camera_clicked()
{
    pb->set_camera_state(1);
}

void MainWindow::on_open_camear_light_clicked()
{
    pb->set_camera_light_state(1);
}

void MainWindow::on_close_camear_light_clicked()
{
    pb->set_camera_light_state(0);
}

void MainWindow::on_exposure_time_edit_returnPressed()
{
    pb->set_camera_exposure_time(ui->exposure_time_edit->text().toUInt());
}

void MainWindow::on_sweeping_angle_returnPressed()
{
    qDebug() << "sweeping_angle" << ui->sweeping_angle->text().toFloat();
    qDebug() << "vibrate_freq" << ui->vibrate_freq->text().toFloat();

    pb->set_sevor_motor_param(
        ui->sweeping_angle->text().toUInt(), ui->vibrate_angle->text().toFloat(),
        ui->sweeping_freq->text().toFloat(), ui->vibrate_freq->text().toUInt());
}
void MainWindow::on_otaTestPushButton_2_clicked()
{
    isContinue = true;
    ui->bleTestPushButton->setEnabled(false);
    ui->configWifiPushButton->setEnabled(false);
    ui->otaTestPushButton->setEnabled(false);
    ui->otaTestPushButton_2->setEnabled(false);
    int times = 1;
    bool finish = false;
    QTime timeout;
    QTime totalTime;
    bool refresh = false;

    QStringList s;
    s << "成功" << "GENERAL" << "低电量" << "已经在OTA中" << "文件过大" << "MD5错误" << "文件不支持"
      << "FLASH错误" << "No Memory" << "TRANS_TIMEOUT" << "TRANS_OVER_RANGE" << " 下载成功"
      << "下载失败";

    at->resetConnected();
    at->sendotaMac(ui->macInput_7->text());
    pb->setPbMode(0);
    timeout.start();

    while (at->getConnected() == false)
    {
        waitWork(100);
        if (timeout.elapsed() > 1000 * 30)
        {
            ui->testMsg->appendPlainText("蓝牙连接超时");
            isContinue = false;
        }
        if (isContinue == false)
            break;
    }
    waitWork(1000);

    //    connect(pb, &Qpb::sendInfo, [&](QString s) {
    //        ui->testMsg->appendPlainText(" ");
    //        ui->testMsg->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate));
    //        ui->testMsg->appendPlainText(s);
    //    });

    //    connect(pb, &Qpb::sendOTAProgress, [&](int s) {
    //        ui->progressBar->show();
    //        ui->progressBar->setMaximum(100);
    //        ui->progressBar->setValue(s);
    //        refresh = true;
    //        if(s == 100)
    //        {
    //            //ui->testMsg->appendPlainText(QString("ota time:%1
    //            s").arg(totalTime.elapsed()/1000)); finish = true;
    //        }
    //    });

    //    connect(pb, &Qpb::sendOTAResult, [&](int r) {
    //        ui->testMsg->appendPlainText("OTA result : ");
    //        ui->testMsg->appendPlainText(s[r]);
    //        finish = true;
    //    });

    waitWork(100);
    ui->macInput_7->clear();
    // ui->macInput_7->setFocus();
    while (isContinue)
    {
        if (!dongleSerialPort->isOpen())
        {
            on_connectButton_clicked();
        }
        ui->progressBar->setValue(0);
        pb->startOTA();
        timeout.start();
        ui->testMsg->appendPlainText("----------------");
        ui->testMsg->appendPlainText("OTA启动，开始计时");
        totalTime.start();
        finish = false;
        while (finish == false)
        {
            if (timeout.elapsed() > 3 * 60 * 1000)
            {
                ui->testMsg->appendPlainText("下载超时");
                finish = true;
                break;
            }
            if (at->getConnected() == false)
            {
                finish = true;
            }
            waitWork(200);
            if (isContinue == false)
                break;

            if (refresh)
            {
                refresh = false;
                timeout.restart();
            }
            ui->lcdNumber->display(totalTime.elapsed() / 1000);
        }

        ui->testMsg->appendPlainText(QString("升级次数:%1").arg(times++));
        ui->testMsg->appendPlainText(QString("耗时:%1 s").arg(totalTime.elapsed() / 1000));
        waitWork(ui->testPeriodSpin->value() * 1000);
        break;
    }
    ui->otaTestPushButton_2->setEnabled(true);
    ui->progressBar->hide();
    ui->bleTestPushButton->setEnabled(true);
    ui->configWifiPushButton->setEnabled(true);
    ui->otaTestPushButton->setEnabled(true);
}

void MainWindow::on_configWifiPushButton_2_clicked()
{
    ui->configWifiPushButton_2->setEnabled(false);
    QTime timeout;
    at->resetConnected();
    at->sendbleMac(ui->macInput_7->text());
    pb->setPbMode(0);
    timeout.start();
    isContinue = true;
    while (at->getConnected() == false)
    {
        waitWork(100);
        if (timeout.elapsed() > 1000 * 30)
        {
            ui->testMsg->appendPlainText("蓝牙连接超时");
            isContinue = false;
        }
        if (isContinue == false)
            break;
    }
    waitWork(1000);
    if (isContinue == false)
    {
        ui->configWifiPushButton_2->setEnabled(true);
        return;
    }

    // 更新license
    ProductLicense &license = ProductLicense::getInstance();
    ProductLicense &Testlicense = ProductLicense::getTestInstance();

    LicensePair pair = license.getLicense();
    LicensePair testpair = Testlicense.getLicense();
    WifiInfo info;
    QString name = ui->wifiUserName->text();
    QString password = ui->wifiPassword->text();
    QString deviceSecret;
    QString deviceName;
    QString productName;
    if (OTAGroup->checkedId() == 0)   // 生产版本
    {
        ui->testMsg->appendPlainText("正在运行生产版本OTA");
        productName = pair.product_name;
        deviceName = pair.device_key;
        deviceSecret = pair.device_secret;
    }
    if (OTAGroup->checkedId() == 1)   // 测试版本
    {
        ui->testMsg->appendPlainText("正在运行测试版本OTA");
        productName = testpair.product_name;
        deviceName = testpair.device_key;
        deviceSecret = testpair.device_secret;
    }

    ui->productKey->setText(productName);
    ui->deviceName->setText(deviceName);
    ui->deviceSecret->setText(deviceSecret);
    memset(&info, 0, sizeof(info));
    memcpy(info.wifi_name, name.toUtf8(), name.size());
    memcpy(info.wifi_password, password.toUtf8(), password.size());
    memcpy(info.product_key, productName.toUtf8(), productName.size());
    memcpy(info.device_name, deviceName.toUtf8(), deviceName.size());
    memcpy(info.device_secret, deviceSecret.toUtf8(), deviceSecret.size());

    pb->configNetwork(info);
    waitWork(1000);
    waitWork(1000);
    ui->testMsg->appendPlainText("已配置网络");
    // ui->testMsg->appendPlainText(name+password);

    ui->configWifiPushButton_2->setEnabled(true);
}

void MainWindow::on_motor_cali_param_returnPressed()
{
    bool ok = false;
    uint32_t value =
        ui->motor_cali_param->text().mid(0, 8).toUInt(&ok, 16);   // 将十六进制字符串转换为
    if (ok)
    {
        qDebug() << value;   // 输出 38822029，即十六进制字符串 "003bdf6d" 对应的数值
    }
    else
    {
        qDebug() << "Invalid input string";
    }
    pb->set_motor_cali_result_param(value);
}
void MainWindow::getPressSensorData(FacUploadPresSensor x)
{
    int32_t mode_button_value = 0;
    int32_t brush_head_value = 0;
    ui->msgEdit->appendPlainText("保存压感数据ing");
    for (int i = 0; i < x.sensor_data_count; i++)
    {
        ui->brush_value->setText("刷头压力：" +
                                 QString::number(int16_t(x.sensor_data[i].brush_head.value)));
        ui->brush_adc->setText("刷头adc：" +
                               QString::number(int16_t(x.sensor_data[i].brush_head.adc)));
        ui->botton_adc->setText("按键adc：" +
                                QString::number(int16_t(x.sensor_data[i].mode_button.adc)));
        ui->botton_value1->setText("按键压力：" +
                                   QString::number(int16_t(x.sensor_data[i].mode_button.value)));

        if (x.sensor_data[i].mode_button.value & 0x8000)
        {   // 判断最高位是否为 1，表示负数
            mode_button_value = static_cast<int32_t>(x.sensor_data[i].mode_button.value |
                                                     0xFFFF0000);   // 扩展为 32 位的负数
        }
        else
        {
            mode_button_value =
                static_cast<int32_t>(x.sensor_data[i].mode_button.value);   // 保持正数不变
        }
        if (x.sensor_data[i].brush_head.value & 0x8000)
        {   // 判断最高位是否为 1，表示负数
            brush_head_value = static_cast<int32_t>(x.sensor_data[i].brush_head.value |
                                                    0xFFFF0000);   // 扩展为 32 位的负数
        }
        else
        {
            brush_head_value =
                static_cast<int32_t>(x.sensor_data[i].brush_head.value);   // 保持正数不变
        }
        saveDataToLocalFolder(x.sensor_data[i].brush_head.adc, brush_head_value,
                              x.sensor_data[i].mode_button.adc, mode_button_value, isfirstsavedata);
        isfirstsavedata = 0;
    }
}

void MainWindow::saveDataToLocalFolder(uint32_t data1, int data2, uint32_t data3, int data4,
                                       bool appHeader)
{
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
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_压感数据报告.csv";
    QString filePath = QDir(folderPath).filePath(fileName);
    // 打开文件，如果无法打开则返回
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text))
    {
        qWarning("无法打开文件");
        return;
    }
    // 创建文本流对象
    QTextStream out(&file);
    if (file.pos() == 0 || appHeader)
    {
        QStringList headers;
        headers << "时间戳" << "MAC地址" << "刷头ADC" << "刷头压力" << "模式按键ADC"
                << "模式按键压力";
        out << headers.join(",") << "\n";
    }
    // 获取当前时间戳
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestamp = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");
    // 写入数据
    out << timestamp << "," << macAddress << "," << data1 << "," << data2 << "," << data3 << ","
        << data4 << "\n";
    // 关闭文件
    file.close();
}
void MainWindow::on_start_brush_clicked()
{
    pb->setbrushcontrol(1);
}

void MainWindow::on_open_press_collect_clicked()
{
    isfirstsavedata = 1;
    pb->setPressCollectParam(FacSwitch_START);
}

void MainWindow::on_close_press_collect_clicked()
{
    pb->setPressCollectParam(FacSwitch_CLOSE);
}
void MainWindow::on_app_connect_clicked()
{
    at->sendotaMac(ui->macInput_7->text());
}
void MainWindow::on_macInput_7_returnPressed()
{
    // on_macInput_returnPressed();
    // ui->testMsg->clear();
    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput_7->text()).hasMatch())
    {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    }
    else
    {
        macAddress = ui->macInput_7->text();
        macLabel->setText("蓝牙mac: " + macAddress);
        // at->sendbleMac(ui->macInput_7->text());   // 开始连接
        pb->setPbMode(0);
        on_configWifiPushButton_2_clicked();
        on_otaTestPushButton_2_clicked();
    }
}

void MainWindow::on_test_cali_clicked()
{
    static int clickStep = 1;   // 用于跟踪当前运行的步骤
    switch (clickStep)
    {
    case 1:
        pb->set_motor_cali(1);
        ui->msgEdit->appendPlainText("霍尔校准");
        break;

    case 2:
        pb->set_motor_cali(2);
        ui->msgEdit->appendPlainText("零点校准");
        break;
    }

    clickStep++;   // 增加步骤计数

    // 如果步骤计数超过了最大步骤数，重置为第一步
    if (clickStep > 2)
    {
        clickStep = 1;
    }
}

void MainWindow::on_open_motor_test_clicked()
{
    pb->set_motor_test_state(1);
}

void MainWindow::on_close_motor_test_clicked()
{
    pb->set_motor_test_state(0);
}

void MainWindow::on_start_local_ota_clicked()
{
    ui->local_ota_result->setText("OTA");
    ui->local_ota_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    on_connectwifi_clicked();
    waitWork(WAITTIME);
    int count = 0;
    std::map<QString, int> version_map{
        {"固件", 201}, {"资源", 202}, {"音频", 303}, {"主题", 304}, {"音乐文件", 305}};

    local_ota_data pack[2];
    for (int i = 0; i < 2; i++)
    {
        QString file_type =
            findChild<QComboBox *>(QString("version_type_%1").arg(i + 1))->currentText();
        QString file_size = findChild<QLineEdit *>(QString("file_size_%1").arg(i + 1))->text();
        QString version_type =
            findChild<QLineEdit *>(QString("version_type_%1").arg(i + 3))->text();
        QString url = findChild<QLineEdit *>(QString("url_%1").arg(i + 1))->text();
        QString md5 = findChild<QLineEdit *>(QString("md5_%1").arg(i + 1))->text();
        int check_ota_set = findChild<QCheckBox *>(QString("is_ota_%1").arg(i + 1))->checkState();

        if (!file_type.isEmpty() && !file_size.isEmpty() && !version_type.isEmpty() &&
            !url.isEmpty() && !md5.isEmpty() && check_ota_set)
        {
            pack[count].is_have_data = 1;
            pack[count].file_size = file_size.toInt();
            pack[count].version_code = version_type.toInt();
            pack[count].version_type =
                version_map[file_type];   // 这里假定file_type总能在version_map中找到对应项

            pack[count].url = url.toUtf8();
            pack[count].md5 = md5.toUtf8();
            count++;
        }
        else
        {
            ui->msgEdit->appendPlainText("没填完整信息");
        }
    }

    if (count > 0)
    {
        pb->set_local_ota(pack);   // 假设这里的方法参数是local_ota_data数组
    }
}

void MainWindow::update_FactroyCmd_WIFI_DEMAND(FacWifiDemand x)
{
    if (x.result)
    {
        WifiStatusLabel->setText("WIFI连接：<font color='red'>失败</font>");
        ui->msgEdit->appendPlainText("WIFI连接断开");
    }
    else
    {
        WifiStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        ui->msgEdit->appendPlainText("WIFI连接成功");
    }
}
void MainWindow::update_local_ota_result(FacInternetOta x)
{
    if (x.result)
        qDebug() << "本地ota返回错误，结果为" << x.result;
    ui->local_ota_result->setText("OTA");
    ui->local_ota_result->setStyleSheet(
        "font-size: 33px; background-color: #00FF00; color: black; border: 2px "
        "solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");
}
QString MainWindow::generateOutputFilePath()
{
    QString outputPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    QString musicFolderPath = QDir(outputPath).filePath("音乐");
    QDir musicFolder;
    if (!musicFolder.exists(musicFolderPath))
    {
        // 如果文件夹不存在则创建之
        if (!musicFolder.mkdir(musicFolderPath))
        {
            // 创建文件夹失败则返回桌面路径
            qDebug() << "Failed to create folder: " << musicFolderPath;
            return QDir(outputPath).filePath("录音.wav");
        }
    }
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString fileName = stringsn + "录音" + timestamp + ".wav";
    return QDir(musicFolderPath).filePath(fileName);
}

void MainWindow::on_new_connectwifi_clicked()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString wifiName = settings.value("WIFI/Name").toString();
    QString wifiPassword = settings.value("WIFI/Password").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected())
    {
        pb->connect_wifi(wifiNameBytes, wifiPasswordBytes, wifiPasswordBytes, wifiPassword);
        ui->msgEdit->appendPlainText("已设置连接wifi");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_get_imu_info_clicked()
{
    pb->getimuCaliResult();
}

void MainWindow::on_swing_test_clicked()
{
    if (at->getConnected())
    {
        pb->set_sevor_motor_param(14, 12, 5.2, 190);
        ui->msgEdit->appendPlainText("已经设置摆幅测试");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_light_test_clicked()
{
    if (1)   //(at->getConnected())
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
        ui->msgEdit->appendPlainText("已经设置RGB测试");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }
}

void MainWindow::on_start_search_clicked()
{
    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }

    ui->pick_device->clear();
    deviceMap.clear();
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
}

void MainWindow::on_pick_device_textActivated(const QString &arg1)
{
    ui->log->clear();
    ui->msgEdit->clear();
    if (!dongleSerialPort->isOpen())
    {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch())
    {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    }
    else
    {
        macAddress = arg1;
        at->sendMac(macAddress);   // 发送mac地址
    }
}

void MainWindow::on_open_motor_cali_clicked()
{
    pb->set_motor_cali_state(1);
}

void MainWindow::on_close_motor_cali_clicked()
{
    pb->set_motor_cali_state(0);
}

void MainWindow::refresh_color1()
{
    int r = ui->R1->value();
    int g = ui->G1->value();
    int b = ui->B1->value();

    QString styleSheet =
        QString("QLineEdit { background-color: rgb(%1, %2, %3); }").arg(r).arg(g).arg(b);
    ui->light1->setStyleSheet(styleSheet);
}

void MainWindow::refresh_color2()
{
    int r = ui->R2->value();
    int g = ui->G2->value();
    int b = ui->B2->value();

    QString styleSheet =
        QString("QLineEdit { background-color: rgb(%1, %2, %3); }").arg(r).arg(g).arg(b);
    ui->light2->setStyleSheet(styleSheet);
}

void MainWindow::refresh_color3()
{
    int r = ui->R3->value();
    int g = ui->G3->value();
    int b = ui->B3->value();

    QString styleSheet =
        QString("QLineEdit { background-color: rgb(%1, %2, %3); }").arg(r).arg(g).arg(b);
    ui->light3->setStyleSheet(styleSheet);
}
void MainWindow::refresh_color4()
{
    int r = ui->R4->value();
    int g = ui->G4->value();
    int b = ui->B4->value();

    QString styleSheet =
        QString("QLineEdit { background-color: rgb(%1, %2, %3); }").arg(r).arg(g).arg(b);
    ui->light4->setStyleSheet(styleSheet);
}

void MainWindow::on_R1_valueChanged(int value)
{
    ui->label_46->setText(QString::number(value));

    refresh_color1();
}

void MainWindow::on_G1_valueChanged(int value)
{
    ui->label_47->setText(QString::number(value));

    refresh_color1();
}

void MainWindow::on_B1_valueChanged(int value)
{
    ui->label_48->setText(QString::number(value));

    refresh_color1();
}

void MainWindow::on_R2_valueChanged(int value)
{
    ui->label_62->setText(QString::number(value));
    refresh_color2();
}

void MainWindow::on_G2_valueChanged(int value)
{
    ui->label_63->setText(QString::number(value));
    refresh_color2();
}

void MainWindow::on_B2_valueChanged(int value)
{
    ui->label_64->setText(QString::number(value));
    refresh_color2();
}

void MainWindow::on_R3_valueChanged(int value)
{
    ui->label_65->setText(QString::number(value));
    refresh_color3();
}

void MainWindow::on_G3_valueChanged(int value)
{
    ui->label_66->setText(QString::number(value));
    refresh_color3();
}

void MainWindow::on_B3_valueChanged(int value)
{
    ui->label_67->setText(QString::number(value));
    refresh_color3();
}

void MainWindow::on_R4_valueChanged(int value)
{
    ui->label_68->setText(QString::number(value));
    refresh_color4();
}

void MainWindow::on_G4_valueChanged(int value)
{
    ui->label_69->setText(QString::number(value));
    refresh_color4();
}

void MainWindow::on_B4_valueChanged(int value)
{
    ui->label_70->setText(QString::number(value));
    refresh_color4();
}

void MainWindow::on_pink_led_clicked()
{
    static int turn = 1;
    if (turn)
    {
        pb->set_led_color(1, 2);
        turn = 0;
    }
    else
    {
        pb->set_led_color(0, 2);
        turn = 1;
    }
}

void MainWindow::on_white_led_clicked()
{
    static int turn = 1;
    if (turn)
    {
        pb->set_led_color(1, 1);
        turn = 0;
    }
    else
    {
        pb->set_led_color(0, 1);
        turn = 1;
    }
}

void MainWindow::on_get_motor_log_clicked()
{
    pb->set_motor_cali_state(1);
    pb->set_motor_cali_state(0);
    // 开关后日志就打开了
}

void MainWindow::on_set_imu_info_clicked()
{
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

    pb->send_new_imuCaliResult(calData);
}

void MainWindow::on_calculate_returnPressed()
{
    // 定义用于保存数据的数组
    CalibData csv_data[9];
    for (int x = 0; x < 9; ++x)
    {
        // 重置示例数据
        csv_data[x].data[0] = 0;
        csv_data[x].data[1] = 0;
        csv_data[x].data[2] = 0;
    }
    int i;
    // 打开matched_file.csv文件
    QFile file("matched_file.csv");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Failed to open file:" << file.errorString();
        return;
    }

    QTextStream in(&file);

    // 读取文件内容，并按行分割
    while (!in.atEnd())
    {
        QString line = in.readLine();
        // 使用逗号分割每一行的数据
        QStringList fields = line.split(',');
        if (fields.size() < 5)
        {
            qDebug() << "Invalid CSV format";
            continue;
        }

        // 提取第二列的MAC地址
        QString mac = fields[1];
        if (mac.isEmpty())
        {
            qDebug() << "Empty MAC address";
            continue;
        }

        // 如果csvmac为空，表示第一次提取到MAC地址，将其赋值给csvmac
        if (csvmac.isEmpty())
        {
            csvmac = mac;
        }

        // 如果mac与csvmac不相等，表示已经处理完所有与csvmac相同的行，需要将数据填入数组并执行相应函数
        if (mac != csvmac && i == 8)
        {
            nqimuc->acccalib_sensors_init();
            // 执行函数
            nqimuc->add_csvdata(csv_data);
            nqimuc->acccalib_sensors_task();

            // 更新csvmac
            csvmac = mac;
            for (int x = 0; x < 9; ++x)
            {
                // 重置示例数据
                csv_data[x].data[0] = 0;
                csv_data[x].data[1] = 0;
                csv_data[x].data[2] = 0;
            }
        }

        // 从第四列的数据中解析出数组索引 i
        QString indexStr = fields[3];
        if (!indexStr.startsWith("data:"))
        {
            qDebug() << "Invalid index format";
            continue;
        }
        i = indexStr.mid(5).toInt();   // 去除字符串中的"data:"部分，并转换为整数

        // 检查索引 i 是否合法
        if (i < 0 || i > 8)
        {
            qDebug() << "Invalid index:" << i;
            continue;
        }

        // 解析第五列的数据
        QString dataStr = fields[4];
        // 修改正则表达式以匹配带负号的数字
        QRegularExpression regex("added(-?\\d+\\.\\d+)(-?\\d+\\.\\d+)(-?\\d+\\.\\d+)");

        QRegularExpressionMatch match = regex.match(dataStr);
        if (match.hasMatch())
        {
            float data0 = match.captured(1).toFloat();
            float data1 = match.captured(2).toFloat();
            float data2 = match.captured(3).toFloat();
            csv_data[i].data[0] = data0;
            csv_data[i].data[1] = data1;
            csv_data[i].data[2] = data2;
            qDebug() << data0 << data1 << data2;
            qDebug() << csv_data[i].data[0] << csv_data[i].data[1] << csv_data[i].data[2];
        }
        else
        {
            qDebug() << "Invalid data format:" << dataStr;
            continue;
        }
    }

    file.close();
}

void MainWindow::processTheDatagram(QByteArray &datagram)
{
    //qDebug() << "Received datagram with length: " << datagram.size();

    uint8_t *data = reinterpret_cast<uint8_t *>(datagram.data());

    // 提取前8个字节并转换为uint64_t
    uint64_t timestamp = 0;
    if (datagram.size() >= sizeof(uint64_t))
    {
        timestamp = *reinterpret_cast<uint64_t *>(data);
    }
    else
    {
        qWarning() << "Datagram is too short to contain timestamp.";
        return;
    }

    QString timestampStr = QString::number(timestamp);
    ui->timestamp_value->setText(timestampStr);

    // 提取图像数据
    QByteArray imageBytes;   // 从第8个字节开始提取图像数据

    if (datagram.size() > PICTURE_PHY_LAYER_HEAD_SIZE)
    {
        imageBytes = datagram.mid(PICTURE_PHY_LAYER_HEAD_SIZE);
    }

    // 如果没有图像数据，则返回
    if (imageBytes.isEmpty())
    {
        qWarning() << "No image data found in datagram.";
        return;
    }

    // 检查并打印图像数据大小
    qDebug() << "图像数据大小:" << imageBytes.size();
    if (imageBytes.isEmpty())
    {
        qWarning() << "图像数据为空。";
        return;
    }



    // 尝试将 QByteArray 转换为 QImage
    QImage image(reinterpret_cast<const uchar *>(imageBytes.data()), 180, 200,
                 QImage::Format_Grayscale8);

    if (image.isNull())
    {
        // 打印图像数据的前20字节
        qDebug() << "图像数据前20字节:" << imageBytes.left(20).toHex();
        qWarning() << "直接从字节数据转换图像失败，尝试使用 loadFromData。";

        // 如果直接转换失败，尝试使用 loadFromData
        if (!image.loadFromData(imageBytes))
        {
            qWarning() << "从数据加载图像失败。";
            return;
        }
        else
        {
            qDebug() << "从数据加载图像成功。";
        }
    }


    qDebug() << "图像加载成功";
    // 继续处理加载成功的图像，例如显示在界面上

    // 检查图像大小是否符合预期
    if (image.width() != 180 || image.height() != 200)
    {
        qWarning() << "Image size is not as expected (180x200).";
        return;
    }

    if (!viewercamrea)
    {
        qWarning() << "viewercamrea 对象为空，无法绘制图像和矩形。";
        return;
    }

    // 检查图像是否为空
    if (image.isNull())
    {
        qWarning() << "图像为空，无法绘制。";
        return;
    }

    // 绘制图像和矩形
    viewercamrea->pixmap = QPixmap::fromImage(image);
    QPainter painter(&viewercamrea->pixmap);

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    int Rect1_X = settings.value("CAMERA/Rect1_X", 70).toInt();
    int Rect1_Y = settings.value("CAMERA/Rect1_Y", 25).toInt();
    int Rect1_Width = settings.value("CAMERA/Rect1_Width", 40).toInt();
    int Rect1_Height = settings.value("CAMERA/Rect1_Height", 25).toInt();

    // int Rect2_X = settings.value("CAMERA/Rect2_X", 64).toInt();
    // int Rect2_Y = settings.value("CAMERA/Rect2_Y", 0).toInt();
    // int Rect2_Width = settings.value("CAMERA/Rect2_Width", 52).toInt();
    // int Rect2_Height = settings.value("CAMERA/Rect2_Height", 34).toInt();

    QPen pen(Qt::red);     // 创建一个红色的画笔
    painter.setPen(pen);   // 设置画笔颜色

    painter.drawImage(0, 0, image);   // 在 pixmap 上绘制图片

    painter.drawRect(Rect1_X, Rect1_Y, Rect1_Width, Rect1_Height);   // 绘制第一个矩形
    // painter.drawRect(Rect2_X, Rect2_Y, Rect2_Width, Rect2_Height);   // 绘制第二个矩形

    viewercamrea->updateImage();   // 更新视图
}

void MainWindow::readPendingDatagrams()
{
    while (udpSocket->hasPendingDatagrams())
    {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        processTheDatagram(datagram);
    }
}

void MainWindow::on_distribution_network_clicked()
{
    // 获取IP地址
    QString ipString = "0.0.0.0";
    QString localHostName = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(localHostName);
    qDebug() << "Local IP addresses:";

    for (QHostAddress address : hostInfo.addresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            qDebug() << address.toString();
            ipString = address.toString();
            break;
        }
    }

    ui->client_ip_label->setText(ipString);

    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString wifiName = ui->ssid_lineEdit->text();
    QString wifiPassword = ui->password_lineEdit->text();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected())
    {
        pb->connect_wifi(wifiNameBytes, wifiPasswordBytes, ipString, ui->port_num->text());
        ui->msgEdit->appendPlainText("已设置连接wifi");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }

    static bool isExecuted = false;
    if (!isExecuted)
    {
        // 获取QLineEdit中的端口号文本
        QString portText = ui->port_num->text();
        bool conversionOk;
        int port = portText.toInt(&conversionOk);

        udpSocket = new QUdpSocket(this);
        QHostAddress ipAddress(ipString);
        bool bindResult = udpSocket->bind(ipAddress, port);
        qDebug() << "connect_udp,localhost: " << ipString << ":" << port;
        qDebug() << "Bind result: " << bindResult;

        if (!bindResult)
        {
            ui->msgEdit->appendPlainText("ip绑定失败");
        }

        connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

        isExecuted = true;
    }
}

void MainWindow::on_save_photo_clicked()
{
    if (!viewercamrea->pixmap.isNull())
    {
        QPixmap pix = viewercamrea->pixmap;
        // 使用QFileDialog让用户选择保存文件的路径
        QString fileName = QFileDialog::getSaveFileName(this, tr("Save File"), "/home",
                                                        tr("Images (*.png *.xpm *.jpg)"));

        if (!fileName.isEmpty())
        {
            pix.save(fileName);   // 保存为用户选择的文件
        }
    }
    else
    {
        qDebug() << "Pixmap is empty!";
    }
}

void MainWindow::on_clean_mode_clicked()
{
    pb->set_motor_param(270, 60);
    pb->set_motor_state(1);
    waitWork(1000);

    pb->set_device_mode(3);
}

void MainWindow::on_nfc_write_read_clicked()
{
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8];   // 写入数据缓冲区

    // 033BD2023668772001004800324F30450081080027200014178591141035353034303730313233313131313131170102910B0101010A063C842707A8D1
    // 3C842707A8D1
    // 35353034303730313233313131313131
    // 5504070123111111    //5504华为开头0701sku2311年月日1111数量
    QString hexString;
    QString text = ui->nfc_sn->text();

    for (int i = 0; i < text.length(); ++i)
    {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0'));
    }

    QString dataText = "033BD2023668772001004800324F304500810800272000141785911410" + hexString +
                       "170102910B0101010A06" + ui->nfc_mac->text().remove(":");

    QByteArray dataBytes =
        QByteArray::fromHex(dataText.toLatin1());   // 将十六进制字符串转换为字节数组
    int dataSize = dataBytes.size();                // 获取字节数组的大小
    qDebug() << "dataSize: " << dataSize << dataText;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    icdev = dc_init(100, 115200);
    if ((intptr_t)icdev <= 0)
    {
        ui->msgEdit->appendPlainText("Init Com Error!");
        return;
    }
    else
    {
        ui->msgEdit->appendPlainText("Init Com OK!");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0)
    {
        if (st == 1)
            ui->msgEdit->appendPlainText("nfc卡识别不到");
        if (st < 0)
            ui->msgEdit->appendPlainText("nfc卡查询失败");
        return;
    }
    else
    {
        ui->msgEdit->appendPlainText("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char *)szSnr;
        ui->msgEdit->appendPlainText(QString::fromStdString(str1));
    }

    for (int i = 0; i < dataSize; i += 4)
    {                                               // 每次处理8个字节
        int bytesToWrite = qMin(8, dataSize - i);   // 计算本次需要写入的字节数，最多8个

        memcpy(writedata, dataBytes.constData() + i, bytesToWrite);   // 将数据复制到写入数据缓冲区

        int ret = dc_write(icdev, 4 + i / 4, writedata);   // 将写入数据缓冲区中的数据写入设备

        if (ret != 0)
        {
            QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
            qDebug() << "errMsg: " << writedata << errMsg;
        }
    }

    for (int i = 4; i * 4 < dataSize; i += 4)
    {   // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0)
        {
            ui->msgEdit->appendPlainText("dc_read Error!");
            return;
        }
        else
        {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char *)rdatahex;
            ui->msgEdit->appendPlainText(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16)
    {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0)
        {
            ui->msgEdit->appendPlainText("dc_read Error!");
            return;
        }
        else
        {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char *)rdatahex;
            ui->msgEdit->appendPlainText(QString::fromStdString(str1));
        }
    }

    if ((intptr_t)icdev > 0)
    {
        st = dc_exit(icdev);
        if (st != 0)
        {
            ui->msgEdit->appendPlainText("dc_exit Error!");
            return;
        }
        else
        {
            ui->msgEdit->appendPlainText("dc_exit OK!");
            icdev = (HANDLE)-1;
        }
    }
    return;
}

void MainWindow::on_clear_nfc_data_clicked()
{
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8] = {0x03, 0x00, 0xFE, 0x00};   // 写入数据缓冲区
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    icdev = dc_init(100, 115200);
    if ((intptr_t)icdev <= 0)
    {
        ui->msgEdit->appendPlainText("Init Com Error!");
        return;
    }
    else
    {
        ui->msgEdit->appendPlainText("Init Com OK!");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0)
    {
        ui->msgEdit->appendPlainText("dc_card_n Error!");

        return;
    }
    else
    {
        ui->msgEdit->appendPlainText("dc_card_n Ok!");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char *)szSnr;
        ui->msgEdit->appendPlainText(QString::fromStdString(str1));
    }

    int ret = dc_write(icdev, 4, writedata);   // 将写入数据缓冲区中的数据写入设备

    if (ret != 0)
    {
        QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
        qDebug() << "errMsg: " << writedata << errMsg;
    }

    st = dc_read(icdev, 4, rdata);
    if (st != 0)
    {
        ui->msgEdit->appendPlainText("dc_read Error!");
        return;
    }
    else
    {
        memset(rdatahex, 0x00, sizeof(rdatahex));
        hex_a(rdata, rdatahex, 4);
        std::string str1 = (char *)rdatahex;
        ui->msgEdit->appendPlainText(QString::fromStdString(str1));
    }

    if ((intptr_t)icdev > 0)
    {
        st = dc_exit(icdev);
        if (st != 0)
        {
            ui->msgEdit->appendPlainText("dc_exit Error!");
            return;
        }
        else
        {
            ui->msgEdit->appendPlainText("dc_exit OK!");
            icdev = (HANDLE)-1;
        }
    }
    return;
}

void MainWindow::on_open_screen_show_camera_clicked()
{
    pb->set_screen_camera_state(1);
}

void MainWindow::on_close_screen_show_camera_clicked()
{
    pb->set_screen_camera_state(0);
}

void MainWindow::on_open_support_camera_clicked()
{
    pb->set_camera_support_state(1);
}

void MainWindow::on_close_support_camera_clicked()
{
    pb->set_camera_support_state(0);
}

void MainWindow::on_open_camera_picture_clicked()
{
    totalsize = 0;
dataNumber=0;
    pb->set_camera_picture_state(1);
    std::memset(dongle_ring_buffer, 0, sizeof(dongle_ring_buffer));   // 将数组全部初始化为零
    std::memset(camera_ring_buf, 0, sizeof(camera_ring_buf));   // 将数组全部初始化为零
}

void MainWindow::on_close_camera_picture_clicked()
{
    pb->set_camera_picture_state(0);
}
