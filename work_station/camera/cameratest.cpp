
#include "cameratest.h"

#include <QColor>
#include <QPainter>

#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_cameratest.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

void cameratest::on_pushButton_clicked() {
    // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd设备
    // ui->macInput->setText("3C:84:27:07:A8:D2");
    // ui->macInput->setText("3C:84:27:20:01:3E");
    // ui->macInput->setText("3c:84:27:20:00:c6");
    // ui->macInput->setText("B4:56:5D:BF:53:71");   // wd设备
    // ui->macInput->setText("b4:56:5d:bf:54:4e");   // wd设备
    // ui->macInput->setText("b4:56:5d:bf:57:9d");
    // ui->macInput->setText("F8:8F:C8:57:73:E9");

    // 假设 imagePath 是您的绝对路径
    QString imagePath = "./u7p_camera_defect_detect_env/code/解析度.png";

    // 尝试加载图像
    QImage image;
    if (!image.load(imagePath)) {
        qWarning() << "从路径加载图像失败:" << imagePath;
        return;
    } else {
        qDebug() << "从路径加载图像成功:" << imagePath;
    }

    // 检查图像是否为空
    if (image.isNull()) {
        qWarning() << "图像为空，无法绘制。";
        return;
    }

    if (!viewercamrea) {
        qWarning() << "viewercamrea 对象为空，无法绘制图像和矩形。";
        return;
    }

    // 绘制图像和矩形
    viewercamrea->pixmap = QPixmap::fromImage(image);
    viewercamrea->temporarypixmap = QPixmap::fromImage(image);

    viewercamrea_py->pixmap = QPixmap::fromImage(image);

    QPainter painter(&viewercamrea->pixmap);

    int Rect1_X = SETTINGS.value("CAMERA/Rect1_X", 70).toInt();
    int Rect1_Y = SETTINGS.value("CAMERA/Rect1_Y", 25).toInt();
    int Rect1_Width = SETTINGS.value("CAMERA/Rect1_Width", 40).toInt();
    int Rect1_Height = SETTINGS.value("CAMERA/Rect1_Height", 25).toInt();

    // int Rect2_X = SETTINGS.value("CAMERA/Rect2_X", 64).toInt();
    // int Rect2_Y = SETTINGS.value("CAMERA/Rect2_Y", 0).toInt();
    // int Rect2_Width = SETTINGS.value("CAMERA/Rect2_Width", 52).toInt();
    // int Rect2_Height = SETTINGS.value("CAMERA/Rect2_Height", 34).toInt();

    QPen pen(Qt::red);                                              // 创建一个红色的画笔
    painter.setPen(pen);                                            // 设置画笔颜色
    painter.drawImage(0, 0, image);                                 // 在 pixmap 上绘制图片
    painter.drawRect(Rect1_X, Rect1_Y, Rect1_Width, Rect1_Height);  // 绘制第一个矩形
    // painter.drawRect(Rect2_X, Rect2_Y, Rect2_Width, Rect2_Height);   // 绘制第二个矩形

    viewercamrea->updateImage();  // 更新视图
}
cameratest::cameratest(int index, QWidget* parent) : test_base(parent), ui(new Ui::cameratest) {
    m_index = index;
    pack.mechines = getIndex();
    dongleOutTime = 5;  // 太快会死锁
    upperComputerVer = CAMERA_VER;
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");

    scanSerialPorts();  // 要搜索 一下一开始

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    // mes失败停止。

    startDirtyTime = SETTINGS.value("CAMERA/startDirtyTime", 2).toInt();
    CameraGetTime = SETTINGS.value("CAMERA/CameraGetTime", 6000).toInt();
    showlog("action=" + pack.test_station);
    showlog("model=" + pack.model);
    showlog("action=" + pack.action);
    showlog("line=" + pack.line);
    showlog("machineNo=" + pack.machineNo);

    connect(this, SIGNAL(sendPicture_speed(int)), ui->picture_speed, SLOT(setValue(int)));

    connect(this, SIGNAL(send_fault_data_packet(int, const QVector<int>&)), pb,
            SLOT(set_camera_fault_data_packet(int, const QVector<int>&)));

    // 将定时器的timeout信号连接到槽函数onTimeout
    connect(cameraSendTimer, &QTimer::timeout, this, &cameratest::onTimeout);
    connect(this, SIGNAL(send_thread_date(QString)), this, SLOT(refreshPbData(QString)));
    connect(this, &cameratest::send_image_processed, this, &cameratest::updateImageOnMainThread);
    connect(this, &cameratest::send_image_processed, this, &cameratest::start_dirty_test);
    connect(this, &cameratest::send_image_processed, this, &cameratest::start_offset_test);

    connect(pb, SIGNAL(send_get_picture_send_over(FacPictureDataAck)), this,
            SLOT(getPictureSendOver(FacPictureDataAck)));
    viewercamrea = new ImageViewer("image_markings.png", this);
    ui->verticalLayout->addWidget(viewercamrea);  // 将 ImageViewer 添加到布局中
    viewercamrea->show();                         // 显示 ImageViewer

    viewercamrea_py = new ImageViewer("image_markings.png", this);
    ui->verticalLayout_py->addWidget(viewercamrea_py);  // 将 ImageViewer 添加到布局中
    viewercamrea_py->show();                            // 显示 ImageViewer

    dongleRingBuf = new RingBuf(&p_dongleRingBuffer, dongle_ring_buffer, 1, sizeof(dongle_ring_buffer));
    cameraRingBuf = new RingBuf(&p_cameraRingBuffer, camera_ring_buf, 1, sizeof(camera_ring_buf));

    future = QtConcurrent::run([this]() {
        while (running.load()) {
            solve_frame();
            QCoreApplication::processEvents();

            // QThread::msleep(10);   // 等待10毫秒
        }
    });
    running.store(true);
    // //连接信号和槽
    // QObject::connect(cameratimer, &QTimer::timeout, this, &cameratest::solve_frame);
    // //启动定时器
    // cameratimer->start(10);
    testResultTableInit();

    udpSocket = new QUdpSocket(this);

    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}
void cameratest::write_camera_data(uint8_t* p_data, int data_len) {
    int surpluse_space = 0;

    if (1) {
        surpluse_space = cameraRingBuf->usmile_ring_buffer_surplus_space_get(&p_cameraRingBuffer);
        if (surpluse_space < data_len) {
            qDebug() << "环形队列满了，剩下：" << surpluse_space;
            return;
        }

        int write_len = cameraRingBuf->usmile_ring_buffer_write(&p_cameraRingBuffer, p_data, data_len);
        if (write_len < data_len) {
            qDebug() << "write_len:" << write_len << "len:" << data_len;
        }
    } else {
        qDebug() << "通道不是摄像头";
    }
}

int cameratest::ext_ble_find_next_frame(void) {
    int i = 0;
    ext_uart_phy_layer_t* head = NULL;
    int len = dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, 2 * 1024);
    // qDebug() << "查找下一帧，剩下：" << len;

    for (i = 0; i < len; i++) {
        if (frame_buf[i] == 0xCC && frame_buf[i + 1] == 0xCC && frame_buf[i + 2] == 0xCC && frame_buf[i + 3] == 0xCC &&
            frame_buf[i + 8] != 0xCC) {
            head = (ext_uart_phy_layer_t*)&frame_buf[i];
            if (head->magic == EXT_UART_MAGIC) {
                qDebug() << "匹配到了串口帧头";

                dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, i);

                return 1;
            }
        }
    }

    dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, len);

    return 0;
}
int cameratest::ext_ble_find_next_picture_frame(QByteArray& picturedata) {
    int len = picturedata.size();
    ext_picture_layer_t* head = nullptr;

    // 获取原始数据指针
    const uchar* data = reinterpret_cast<const uchar*>(picturedata.constData());

    for (int i = 0; i <= len - 4; ++i) {
        if (data[i] == 0xA5 && data[i + 1] == 0xA5 && data[i + 2] == 0xA5 && data[i + 3] == 0xA5) {
            if (i >= 20) {
                // 将数据转换为指向结构体的指针
                head = reinterpret_cast<ext_picture_layer_t*>(const_cast<uchar*>(&data[i - 20]));
                if (head->reserved == EXT_PICTURE_PHY_LAYER_MAGIC) {
                    qDebug() << "匹配到了图片数据包头";
                    picturedata.remove(0, i - 20);
                    return 1;
                }
            }
        }
    }

    picturedata.clear();
    return 0;
}

void cameratest::printSquareData(uint8_t* data, int data_size) {
    if (data_size < 36000)
        return;
    const int dimension = 32;          // 每行的列数
    const int totalItems = data_size;  // 数据总量
    for (int i = 0; i < totalItems; i += dimension) {
        QString row;
        for (int j = 0; j < dimension && (i + j) < totalItems; ++j) {
            row += QString("%1 ").arg(data[i + j], 2, 16, QChar('0'));
            row += " ";
        }
        qDebug().noquote() << row;
    }
}

void cameratest::solve_picture_frame(QByteArray picturedata) {
    uint16_t crc16 = NULL;
    uint16_t crc_cali = 0;
    while (true) {
        int ring_size = picturedata.size();
        if (ring_size <= PICTURE_PHY_LAYER_HEADER_ADN_CRC) {
            qDebug() << "摄像头环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }

        ext_picture_layer_t* head = (ext_picture_layer_t*)picturedata.data();

        if (head->reserved == EXT_PICTURE_PHY_LAYER_MAGIC) {
            int frame_size = head->data_size + PICTURE_PHY_LAYER_HEAD_SIZE;
            float progressValue = ((float)ring_size / frame_size) * 100;
            // 如果需要整数，可以转换为 int
            int progressInt = (int)progressValue;

            emit sendPicture_speed(progressInt);
            QCoreApplication::processEvents(QEventLoop::AllEvents);
            if (frame_size > ring_size) {
                // qDebug() << "图片帧数据不完整" << "需要" << frame_size << "实际为" << ring_size;
                break;
            }

            // qDebug() << "图片帧数据完整" << "需要" << frame_size << "实际为" << ring_size
            //          << sizeof(head);

            crc16 = head->data_crc16;
            crc_cali = CRC16(head->data, head->data_size);
            if (crc16 == crc_cali) {
                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char*>(head), frame_size);
                pictureByteArray = byteArray;
                emit send_image_processed();

            } else {
                qDebug() << "哈哈哈2" << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
                showlog("图片的crc校验失败");
                qDebug() << "head content:"
                         << QByteArray(reinterpret_cast<char*>(head), PICTURE_PHY_LAYER_HEAD_SIZE).toHex();

                qDebug() << "crc校验失败" << crc_cali << crc16 << "head->data[0]" << head->data[0] << "head->data[1]"
                         << head->data[1];

                // printSquareData(head->data, head->data_size);

                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char*>(head), frame_size);
                pictureByteArray = byteArray;
                emit send_image_processed();
            }
            packetMap.clear();
            picturedata.clear();
            memset(head, 0, PICTURE_PHY_LAYER_HEAD_SIZE);

        } else {
            qDebug() << "数据流错误寻找下一帧";

            if (ext_ble_find_next_picture_frame(picturedata)) {
                continue;
            } else {
                qDebug() << "找不到图片帧头";
                break;
            }
        }

        // 处理完一帧数据后，让 Qt 处理事件循环，以确保界面更新等操作得到及时响应
        QCoreApplication::processEvents();
    }
}
void cameratest::solve_frame(void) {
    while (true) {
        // 从环形缓冲区中读取帧头

        dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, UART_PHY_LAYER_HEAD_SIZE);

        int ring_size = dongleRingBuf->usmile_ring_buffer_items_count_get(&p_dongleRingBuffer);
        if (ring_size <= UART_PHY_LAYER_HEADER_ADN_LEN) {
            // qDebug() << "串口环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }
        // qDebug() << "ring_size" << ring_size;
        // qDebug() << "frame_buf:"
        //          << QByteArray(reinterpret_cast<char *>(frame_buf), 500) .toHex();
        ext_uart_phy_layer_t* head = (ext_uart_phy_layer_t*)frame_buf;

        if (head->magic == EXT_UART_MAGIC) {
            // qDebug() << "now content:"
            //          << QByteArray(reinterpret_cast<char *>(head), 280).toHex();

            int frame_size = UART_PHY_LAYER_HEADER_ADN_LEN + head->length;
            if (frame_size > ring_size) {
                // qDebug() << "串口帧数据不完整"
                //          << "需要" << frame_size << "实际为" << ring_size;
                break;
            }

            // 从环形缓冲区中读取整个帧的数据
            dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, frame_size);
            // qDebug() << "head content:"
            //          << QByteArray(reinterpret_cast<char *>(head), 280).toHex();
            // qDebug() << "2串口帧数据大小"<<frame_size<< "2数据大小"<<head->length;

            // if (head->data[head->length]==0x0d&&head->data[head->length+1] == 0x0A)
            if (head->channel == PHY_CHANNEL_CAMREA) {
                qDebug() << "图片数据包的第一字节为" << head->data[0];

                //  emit send_thread_date("图片数据包的第一字节为" + QString::number(head->data[0]));
                cameradatasize = cameradatasize + head->length - 1;
                // emit send_thread_date("图片数据包总数" + QString::number(dataNumber));
                // emit send_thread_date("图片数据包总字节数" +
                // QString::number(cameradatasize=cameradatasize+head->length-1));
                ++dataNumber;

                //  qDebug() << "1串口帧数据大小"<<frame_size<< "1数据大小"<<head->length;

                QByteArray byteArray(reinterpret_cast<const char*>(head->data), head->length);
                addPacket(byteArray);

                solve_picture_frame(reassembleData());

            } else {
                qDebug() << "串口数据通道不符:" << QByteArray(reinterpret_cast<char*>(head), 280).toHex();
                showlog("串口数据通道不符");
                qDebug() << "串口数据通道不符" << QString("%1").arg(head->data[head->length], 2, 16, QChar('0'))
                         << QString("%1").arg(head->data[head->length + 1], 2, 16, QChar('0'));
            }

            // 删除已经处理的帧数据
            dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, frame_size);
            std::memset(frame_buf, 0, sizeof(frame_buf));
            qDebug() << "删除串口帧数据" << frame_size;
        } else {
            // qDebug() << "串口数据流错误寻找下一帧";
            // qDebug() << "串口数据包头为:"
            //          << QByteArray(reinterpret_cast<char*>(frame_buf), UART_PHY_LAYER_HEAD_SIZE).toHex();
            // qDebug() << "串口数据为:" << QByteArray(reinterpret_cast<char*>(frame_buf), 50).toHex();

            if (ext_ble_find_next_frame()) {
                continue;
            } else {
                // qDebug() << "找不到帧头";
                break;
            }
        }

        // 处理完一帧数据后，让 Qt 处理事件循环，以确保界面更新等操作得到及时响应
        QCoreApplication::processEvents();
    }
}
void cameratest::refreshCameraControl(FacCameraControl style) {
    qDebug() << getIndex() << "收到摄像头控制回应" << style.result;
    is_camera_control = 1;
}

cameratest::~cameratest() {
    //
    delete ui;
}
void cameratest::getDongleWifi(QString data) {
    showlog("获取到了wifi名字" + data);
    // if (getIndex() == 1) {
    // showlog("自动保持wifi名字" + data);
    //     // 保存密码
    //     SETTINGS.setValue("WIFI/Password", "usmile123");
    //     // 保存名称，带有索引
    //     SETTINGS.setValue(QString("WIFI/Name"), data);
    // }

    ui->client_ip_label->setText(SETTINGS.value(QString("WIFI/IP"), "请在配置中设置").toString());
    ui->port_num->setText(QString::number(getIndex()));
    ui->ssid_lineEdit->setText(SETTINGS.value(QString("WIFI/Name"), "请在配置文件中设置").toString());
    ui->password_lineEdit->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
}
void cameratest::on_disconnectButton_clicked() {
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refreshBleState(0);
}
void cameratest::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        pb->set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}
void cameratest::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void cameratest::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}
void cameratest::on_macInput_returnPressed() {
    deinit_distribution_network();
    viewercamrea->pixmap = QPixmap();
    viewercamrea->temporarypixmap = QPixmap();
    viewercamrea->updateImage();
    viewercamrea_py->pixmap = QPixmap();
    viewercamrea_py->temporarypixmap = QPixmap();
    viewercamrea_py->updateImage();

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

        isTestContinue = true;
        emit send_go_next_focus();

        state = STATE_IDLE;
    }
}

void cameratest::bandSnMacToCsv(const QString& macAddress, const QString& sn) {
    QString folderPath = "D:/测试结果";

    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists()) {
        QDir().mkpath(folderPath);
    }

    // 构建完整的文件路径
    QString filePath = QDir(folderPath).filePath("绑定文件.csv");
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);
        // 检查是否是文件的开头，如果是，则写入表头
        if (file.pos() == 0) {
            QStringList headers;
            headers << "时间"
                    << "mac地址"
                    << "sn码";
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
        qDebug() << getIndex() << "文件保存到" << filePath;
    } else {
        qDebug() << getIndex() << "文件没关或者其他问题";
        showlog("文件无法打开");
    }
}

void cameratest::closeEvent(QCloseEvent*) {
    running.store(false);
    // 等待线程结束
    future.waitForFinished();
    qDebug() << getIndex() << "开始关闭";
    isTestContinue = false;
}
void cameratest::refreshSn(FacDevInfo data) {
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("存储尾盖sn:" + stringsn);
}
//正常上报0，失败上报机号，上报了就结束测试了
void cameratest::canGoNextMechine(int x) {
    is_canGoNext = 1;
    qDebug() << getIndex() << "得到信息" << getIndex();
    if (x == getIndex()) {
        result = failValue;
        state = STATE_SAVE_RESULT;
        showlog("摄像头测试失败");
    }
}
void cameratest::processInspection(QString stringsn) {
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
void cameratest::onTimeout() {
    packetMap.clear();
    faultData.clear();
    dataNumber = 0;
    // showlog("再次获取图片");
    pb->set(DeviceCmd::CameraPictureState, 1);
    std::memset(dongle_ring_buffer, 0,
                sizeof(dongle_ring_buffer));  // 将数组全部初始化为零
    std::memset(camera_ring_buf, 0,
                sizeof(camera_ring_buf));  // 将数组全部初始化为零
    showlog("图片数据包大小=" + QString::number(cameradatasize));
    cameradatasize = 0;
}

void cameratest::refreshBaseData(FacGetDevBaseInfo data) {
    if (refresh_times) {
        refresh_times = 0;
        // 读取软件版本字符串
        QString Camera_Id = SETTINGS.value("ProductInfo/Camera_Id").toString();
        qDebug() << "Read Camera_Id:" << Camera_Id;

        // 检查软件版本、资源版本和老化状态是否匹配
        if (compareVersions(Camera_Id, data.camera_version)) {
            showlog("摄像头id正确" + QString::fromUtf8(data.camera_version));

            TestItem test;
            test.testItem = "摄像头id";
            test.testData = QString::fromUtf8(data.camera_version);
            test.testResult = "通过";
            test.ask = Camera_Id;
            testItems.append(test);

            testResultTableUpdate(testItems);

        } else {
            TestItem test;
            test.testItem = "摄像头id";
            test.testData = QString::fromUtf8(data.camera_version);
            test.testResult = "失败";
            test.ask = Camera_Id;
            testItems.append(test);

            testResultTableUpdate(testItems);

            showlog("状态错误");
            showlog("当前设备摄像头id" + QString::fromUtf8(data.camera_version) + "配置文件摄像头id" + Camera_Id);
            result = failValue;
            isTestContinue = false;
            showlog("停止运行");

            on_stopTest_clicked();
        }
    }
}

void cameratest::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
            case STATE_IDLE:  // 复位一切

                showlog("开始测试");
                pb->reset_all_pb();
                refresh_times = 1;
                at->resetConnected();
                displayRectangles = false;
                refreshBleState(0);
                ui->tail_sn->setText("存储尾盖sn:");
                stringsn = "";
                cameraSendTimer->stop();
                result = passValue;
                is_canGoNext = 0;
                is_camera_control = 0;
                picutre_offset_times = 0;
                can_start_dirty_test = 0;
                TestTime.start();
                waitWork(1000);
                at->sendMac(ui->macInput->text());  // 发送mac地址
                showlog("MAC地址为：" + ui->macInput->text());

                state = STATE_WATI_CONNECT;
                break;
            case STATE_WATI_CONNECT:
                if (at->getConnected()) {
                    qDebug() << getIndex() << "蓝牙状态" << at->getConnected();
                    waitWork(WAITTIME);
                    showlog("蓝牙连接成功");
                    sendCommandWithRetry([&]() { pb->get(DeviceCmd::BaseInfo); });
                    bandSnMacToCsv(macAddress, sn);
                    state = STATE_WATI_BASE_INFO;
                }
                break;

            case STATE_WATI_BASE_INFO:

                if (canGoNext) {
                    showlog("已获取到设备信息");
                    pb->set(DeviceCmd::CameraState, 0);
                    state = STATE_DISABLE_SLEEP_1;
                }

                break;
            case STATE_DISABLE_SLEEP_1:
                if (pb->getState(Qpb::PbStateType::DisableSleep)) {
                    showlog("已进入禁止休眠模式");
                    if (SETTINGS.value("SYSTEM/BluetoothImageTransfer").toBool()) {
                        // 设置定时器的超时时间为6000毫秒（6秒）
                        cameraSendTimer->setInterval(CameraGetTime);
                        // 启动定时器
                        cameraSendTimer->start();
                        dataNumber = 0;
                        packetMap.clear();
                        faultData.clear();
                        ui->log->appendPlainText("开始发送开摄像头");
                        pb->set(DeviceCmd::CameraPictureState, 1);
                        std::memset(dongle_ring_buffer, 0,
                                    sizeof(dongle_ring_buffer));  // 将数组全部初始化为零
                        std::memset(camera_ring_buf, 0,
                                    sizeof(camera_ring_buf));  // 将数组全部初始化为零

                        state = CAMERA_TEST;

                        showlog("等待显示照片");
                    } else {
                        on_distribution_network_clicked();
                        state = STATE_NET_SET;
                    }
                } else {
                    waitWork(500);
                    pb->set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
                    showlog("已发送禁止休眠");
                }
                break;

            case STATE_NET_SET:
                waitWork(500);
                if (pb->getState(Qpb::PbStateType::WifiSet)) {
                    showlog("已配网成功");
                    //  waitWork(5000);
                    showlog("现在打开摄像头");
                    pb->set(DeviceCmd::CameraState, 1);
                    showlog("锁定曝光时间");
                    on_exposure_time_edit_returnPressed();
                    // pb->set(DeviceCmd::CameraLightState, 1);
                    state = CAMERA_TEST;
                    showlog("等待显示照片");
                } else {
                    waitWork(500);
                    on_distribution_network_clicked();
                    showlog("已重新发送配网");
                }
                break;

            case CAMERA_TEST:
                is_camera_control = 1;
                if (is_camera_control && is_canGoNext) {
                    showlog("摄像头测试通过");
                    is_canGoNext = 0;
                    is_camera_control = 0;
                    state = STATE_SAVE_RESULT;
                }
                break;

            case STATE_SAVE_RESULT:
                showlog("开始保存结果" + QString::number(getIndex()));
                switch (getIndex()) {
                    case 1: emit send_set_camera_action(STATE_THOROUGHFARE1_OUT); break;
                    case 2: emit send_set_camera_action(STATE_THOROUGHFARE2_OUT); break;
                    case 3: emit send_set_camera_action(STATE_THOROUGHFARE3_OUT); break;
                    default: break;
                }

                if (result == passValue) {
                    QString mesresult = "PASS";
                    pack.result = mesresult;
                    pack.itemvalue = QString("|CAMERA_TEST:PASS|");
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }

                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((result == failValue)) {
                    QString mesresult = "NG";
                    pack.result = mesresult;
                    pack.itemvalue = QString("|CAMERA_TEST:NG|");
                    pack.sn = ui->getMac->text();

                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }

                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                }
                deinit_distribution_network();
                pb->set(DeviceCmd::CameraState, 0);
                pb->set(DeviceCmd::CameraLightState, 0);

                stringsn = "";
                ui->macInput->clear();
                ui->getMac->clear();
                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                // ui->jxl_normal->setDisabled(0);
                // ui->jxl_abnormal->setDisabled(0);
                // waitWork(WAITTIME);
                pb->set(DeviceCmd::DevReset);  //重启彻底断网，别发了
                waitWork(500);
                emit send_end_test(getIndex());
                if (pack.factory == "lx" && m_index == 1)
                    ui->getMac->setFocus();
                at->sendMac("00:00:00:00:00:00");  // 发送mac地址
                waitWork(150);
                on_disconnectButton_clicked();
                showlog("测试结束");
                isTestContinue = false;
                state = STATE_IDLE;
                break;

            case STATE_PROCESS_INSPECTION: break;
        }

        //  QCoreApplication::processEvents();
    }
}

void cameratest::on_lcdTestButton_clicked() {
    pb->set(DeviceCmd::DevReset);
    // waitWork(WAITTIME);
    //
    // waitWork(WAITTIME);
    // pb->set(DeviceCmd::FacMode, 0);
    // waitWork(WAITTIME);
    // pb->set(DeviceCmd::FacMode, 1);
    // waitWork(WAITTIME);
    // pb->set(DeviceCmd::FacMode, 0);
    // waitWork(WAITTIME);
    // pb->set(DeviceCmd::FacMode, 1);
    // waitWork(WAITTIME);
}

void cameratest::getTestValue(const int mechines, const QString value) {
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

    // bandingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试mac——sn
}

void cameratest::on_getMac_returnPressed() {
    testResultTableInit();

    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
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
    sn = ui->getMac->text().toUtf8();
    showlog("正在查询mac地址");
    getMac(ui->getMac->text());  // 文件获取
    processInspection(ui->getMac->text());

    processGetMesTestValue();  // mes获取
}
void cameratest::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}
void cameratest::on_close_camera_clicked() { pb->set(DeviceCmd::CameraState, 0); }

void cameratest::on_open_camera_clicked() { pb->set(DeviceCmd::CameraState, 1); }

void cameratest::on_open_camear_light_clicked() { pb->set(DeviceCmd::CameraLightState, 1); }

void cameratest::on_close_camear_light_clicked() { pb->set(DeviceCmd::CameraLightState, 0); }
void cameratest::on_distribution_network_clicked() {
    // 获取IP地址
    QString ipString = "0.0.0.0";
    QString localHostName = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(localHostName);

    // for (QHostAddress address : hostInfo.addresses()) {
    //     if (address.protocol() == QAbstractSocket::IPv4Protocol) {
    //         qDebug() << getIndex() << address.toString();
    //         ipString = address.toString();
    //     }
    // }
    ipString = ui->client_ip_label->text();

    QString wifiName = ui->ssid_lineEdit->text();
    QString wifiPassword = ui->password_lineEdit->text();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    // if (!isExecuted) {
    // 获取QLineEdit中的端口号文本
    QString portText = ui->port_num->text();
    int port = portText.toInt();

    QHostAddress ipAddress(ipString);

    if (udpSocket->state() == QAbstractSocket::BoundState) {
        qDebug() << "UDP socket is already bound";
        udpSocket->abort();
    } else {
        qDebug() << "UDP socket is not bound";
    }

    bool bindResult = udpSocket->bind(ipAddress, port);
    qDebug() << getIndex() << "connect_udp,localhost: " << ipString << ":" << port;

    if (!bindResult) {
        showlog("ip绑定失败" + udpSocket->errorString());
        udpSocket->abort();
        return;
    } else {
        showlog("ip绑定成功");
    }

    connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

    //     isExecuted = true;
    // }
    if (at->getConnected()) {
        pb->set(DeviceCmd::NewWifiConnect, QVariantList{wifiNameBytes, wifiPasswordBytes, ipString, ui->port_num->text()});
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}
void cameratest::deinit_distribution_network() {
    if (udpSocket->state() == QAbstractSocket::BoundState) {
        qDebug() << "Unbinding UDP socket";
        udpSocket->abort();  // 关闭 UDP 连接
    }
    if (udpSocket->isOpen()) {
        udpSocket->close();
    }
    disconnect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));  // 断开 readyRead 信号

    showlog("UDP 连接已关闭");
    QString wifiName = "test_over";
    QString wifiPassword = "8888888888";
    QString ipString = "0.0.0.0";
    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();
    pb->set(DeviceCmd::NewWifiConnect, QVariantList{wifiNameBytes, wifiPasswordBytes, ipString, ui->port_num->text()});
}
void cameratest::on_save_photo_clicked() {
    if (!viewercamrea->pixmap.isNull()) {
        QPixmap pix = viewercamrea->pixmap;
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
void cameratest::readDongleSerialPortData() {
    dongleSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = dongleSerialPortBuf;  // 读取缓冲区数据
    dongleSerialPortBuf.clear();                // 清除缓冲区

    int write_len = 0;
    int len = dataTemp.size();
    write_len = dongleRingBuf->usmile_ring_buffer_write(&p_dongleRingBuffer,
                                                        reinterpret_cast<uint8_t*>(dataTemp.data()), dataTemp.size());

    if (write_len < len) {
        qDebug() << "write_len:" << write_len << "len:" << dataTemp.size();
        showlog("写入串口池失败");
    }

    // solve_frame();

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    pb->parseCmd(dataTemp);
    // processTheDatagram(dataTemp);
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

    // if (dataTemp.contains("内容为:")) {
    //     int pos = dataTemp.indexOf("内容为:");
    //     QString beforeContent = dataTemp.left(pos + QString("内容为").length() * 3 + 1).trimmed();
    //     QByteArray subsequentContent = dataTemp.mid(pos + QString("内容为").length() * 3 + 1).trimmed();
    //     QString hexContent = toHex(subsequentContent);
    //     logEdit()->appendPlainText(beforeContent + hexContent);
    // } else {
    //     QString logEntry = QString("[%1]\r\n%2").arg(timestamp, dataTemp);
    //     logEdit()->appendPlainText(logEntry);
    // }

    QString logEntry = QString("[%1]\r\n%2").arg(timestamp, dataTemp);
    logEdit()->appendPlainText(logEntry);
}

void cameratest::readPendingDatagrams() {
    while (udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(udpSocket->pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;
        udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);
        processTheDatagram(datagram);
        emit send_image_processed();
    }
}
void cameratest::processTheDatagram(QByteArray& datagram) {
    qDebug() << "Received datagram with length: " << datagram.size();

    uint8_t* data = reinterpret_cast<uint8_t*>(datagram.data());

    // 提取前8个字节并转换为uint64_t
    uint64_t timestamp = 0;
    if (datagram.size() >= sizeof(uint64_t)) {
        timestamp = *reinterpret_cast<uint64_t*>(data);
    } else {
        qWarning() << "Datagram is too short to contain timestamp.";
        return;
    }

    QString timestampStr = QString::number(timestamp);
    ui->timestamp_value->setText(timestampStr);

    // 提取图像数据
    QByteArray imageBytes;

    if (datagram.size() > PICTURE_PHY_LAYER_HEAD_SIZE) {
        imageBytes = datagram.mid(PICTURE_PHY_LAYER_HEAD_SIZE);
    }

    // 如果没有图像数据，则返回
    if (imageBytes.isEmpty()) {
        qWarning() << "No image data found in datagram.";
        return;
    }

    // 检查并打印图像数据大小
    qDebug() << "图像数据大小:" << imageBytes.size();
    if (imageBytes.isEmpty()) {
        qWarning() << "图像数据为空。";
        return;
    }

    // 打印图像数据的前20字节
    qDebug() << "图像数据前20字节:" << imageBytes.left(20).toHex();

    // 尝试将 QByteArray 转换为 QImage
    QImage image(reinterpret_cast<const uchar*>(imageBytes.data()), static_cast<unsigned char>(datagram[8]),
                 static_cast<unsigned char>(datagram[12]), QImage::Format_Grayscale8);

    if (image.isNull()) {
        qWarning() << "直接从字节数据转换图像失败，尝试使用 loadFromData。";

        // 如果直接转换失败，尝试使用 loadFromData
        if (!image.loadFromData(imageBytes)) {
            qWarning() << "从数据加载图像失败。";
            return;
        } else {
            qDebug() << "从数据加载图像成功。";
        }
    } else {
        qDebug() << "直接从字节数据转换图像成功。";
        showlog("请判断视频是否正常");
    }

    qDebug() << "图像加载成功。";
    // 继续处理加载成功的图像，例如显示在界面上

    // 检查图像大小是否符合预期
    if (image.width() != static_cast<unsigned char>(datagram[8]) ||
        image.height() != static_cast<unsigned char>(datagram[12])) {
        qWarning() << "Image size is not as expected (180x200).";
        return;
    }

    if (!viewercamrea) {
        qWarning() << "viewercamrea 对象为空，无法绘制图像和矩形。";
        return;
    }

    // 检查图像是否为空
    if (image.isNull()) {
        qWarning() << "图像为空，无法绘制。";
        return;
    }

    // 绘制图像和矩形
    viewercamrea->pixmap = QPixmap::fromImage(image);
    viewercamrea->temporarypixmap = QPixmap::fromImage(image);

    viewercamrea_py->pixmap = QPixmap::fromImage(image);

    QPainter painter(&viewercamrea->pixmap);

    int Rect1_X = SETTINGS.value("CAMERA/Rect1_X", 70).toInt();
    int Rect1_Y = SETTINGS.value("CAMERA/Rect1_Y", 25).toInt();
    int Rect1_Width = SETTINGS.value("CAMERA/Rect1_Width", 40).toInt();
    int Rect1_Height = SETTINGS.value("CAMERA/Rect1_Height", 25).toInt();

    // int Rect2_X = SETTINGS.value("CAMERA/Rect2_X", 64).toInt();
    // int Rect2_Y = SETTINGS.value("CAMERA/Rect2_Y", 0).toInt();
    // int Rect2_Width = SETTINGS.value("CAMERA/Rect2_Width", 52).toInt();
    // int Rect2_Height = SETTINGS.value("CAMERA/Rect2_Height", 34).toInt();

    QPen pen(Qt::red);                                              // 创建一个红色的画笔
    painter.setPen(pen);                                            // 设置画笔颜色
    painter.drawImage(0, 0, image);                                 // 在 pixmap 上绘制图片
    painter.drawRect(Rect1_X, Rect1_Y, Rect1_Width, Rect1_Height);  // 绘制第一个矩形
    // painter.drawRect(Rect2_X, Rect2_Y, Rect2_Width, Rect2_Height);   // 绘制第二个矩形
    viewercamrea->updateImage();  // 更新视图
}
void cameratest::updateImageOnMainThread() {
    processTheDatagram(pictureByteArray);  // 显示图片
}
void cameratest::start_dirty_test() {
    if (can_start_dirty_test) {
        picutre_dirty_times++;
        if (picutre_dirty_times >= startDirtyTime) {
            picutre_dirty_times = 0;
            can_start_dirty_test = 0;
            on_DirtyTestButton_clicked();
        }
    }
}
void cameratest::start_offset_test() {
    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool()) {
        picutre_offset_times++;
        if (picutre_offset_times == 2) {
            qDebug() << "start_offset_test:" << picutre_offset_times;
            picutre_offset_times = 4;
            on_ResolutionTestButton_clicked();
        }
    }
}
void cameratest::on_normal_clicked() {
    canGoNextMechine(0);
    cameraSendTimer->stop();
}

void cameratest::on_abnormal_clicked() {
    canGoNextMechine(getIndex());
    cameraSendTimer->stop();
}

void cameratest::on_exposure_time_edit_returnPressed() {
    pb->set(DeviceCmd::CameraExposureTime, ui->exposure_time_edit->text().toUInt());
}
void cameratest::on_DirtyTestButton_clicked() {
    QString filePath;
    if (!viewercamrea->temporarypixmap.isNull()) {
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");
        QString fileName = ui->getMac->text() + "_" + timestamp + ".png";
        QString date = currentDateTime.toString("yyyyMMdd");
        QString saveDir = QDir::currentPath() + "/图片存储/脏污自动化测试原图/" + date;
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        filePath = saveDir + "/" + fileName;
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    } else {
        showlog("未识别到图片");
        return;
    }

    QProcess process;

    QString pythonPath = "./u7p_camera_defect_detect_env/python.exe";
    QString scriptPath;
    QStringList arguments;
    if (pack.product == "Q20P") {
        scriptPath = "./code/onnx_inference_camera_stain_q30_250113.py";
        arguments << "--model"
                  << "./code/q30_model_250114.onnx"
                  << "--img" << filePath;
    } else {  //给木星
        scriptPath = "./code/onnx_inference_camera_stain.py";
        arguments << "--model"
                  << "./code/infer_camera_stain_240911_320_model.onnx"
                  << "--img" << filePath;
    }

    process.setWorkingDirectory("./u7p_camera_defect_detect_env");
    qDebug() << "指令内容为:" << scriptPath << arguments;
    process.start(pythonPath, QStringList() << scriptPath << arguments);
    if (!process.waitForStarted()) {
        qDebug() << "Failed to start process";
    }
    process.waitForFinished();
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    qDebug() << "Output:" << output;
    qDebug() << "Error Output:" << errorOutput;

    QString zwTestResult;
    QList<QRect> rectangles;
    QList<Qt::GlobalColor> rectanglesColor;

    QStringList parts = output.trimmed().split('\n');
    for (const QString& part : parts) {
        QStringList values = part.trimmed().split(' ');
        if (values.size() != 5) {
            qDebug() << "Unexpected output format";
            continue;
        }
        bool ok;
        int x1 = values[0].toInt(&ok);
        int y1 = values[1].toInt(&ok);
        int w = values[2].toInt(&ok);
        int h = values[3].toInt(&ok);
        int flag = values[4].toInt(&ok);

        qDebug() << "坐标" << x1 << y1 << w << h << flag;
        if (!ok) {
            qDebug() << "Error parsing integers";
            continue;
        }
        rectangles.append(QRect(x1, y1, w, h));
        // flag = 1 OK图片
        //     flag = 0 NG图片有脏污
        //     flag = -1 NG图片出现裂缝（画面撕裂）
        if (flag == -1) {
            TestItem test;
            test.testItem = "脏污测试";
            test.testData = "通过";
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            zwTestResult = "脏污测试通过";
            showlog("脏污测试通过");
            on_normal_clicked();
        }

        if (flag == 10) {
            TestItem test;
            test.testItem = "脏污测试";
            test.testData = "有脏污";
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            zwTestResult = "脏污测试失败";
            showlog("有脏污");
            rectanglesColor.append(Qt::red);
            on_abnormal_clicked();
        }
        if (flag == 1) {
            TestItem test;
            test.testItem = "脏污测试";
            test.testData = "可放过脏污";
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            showlog("有可放过脏污");
            rectanglesColor.append(Qt::green);
            on_normal_clicked();
        }
        if (flag == 0) {
            TestItem test;
            test.testItem = "脏污测试";
            test.testData = "图片出现裂缝（画面撕裂）";
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            zwTestResult = "图片出现裂缝（画面撕裂）";
            showlog("图片出现裂缝（画面撕裂）");
            on_abnormal_clicked();
        }
    }

    QImage image;
    if (!image.load(filePath)) {
        qWarning() << "Failed to load image from file path:" << filePath;
        return;
    }
    QPixmap pixmapzw = QPixmap::fromImage(image);
    QPainter painter(&pixmapzw);

    // 确保两个列表的大小相同
    if (rectangles.size() != rectanglesColor.size()) {
        qWarning() << "The size of rectangles and rectanglesColor should be the same.";
        return;
    }

    for (int i = 0; i < rectangles.size(); ++i) {
        const QRect& rect = rectangles[i];
        Qt::GlobalColor color = rectanglesColor[i];

        QPen pen(color);
        painter.setPen(pen);

        // qDebug() << "开始画矩形";
        painter.drawRect(rect);
    }

    painter.end();
    viewercamrea_py->pixmap = pixmapzw;
    // qDebug() << "开始更新";
    viewercamrea_py->updateImage();

    if (!viewercamrea_py->pixmap.isNull()) {
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");
        QString fileName = zwTestResult + "_" + ui->getMac->text() + "_" + timestamp + ".png";
        QString date = currentDateTime.toString("yyyyMMdd");

        QString saveDir = QDir::currentPath() + "/图片存储/脏污自动化测试结果图/" + date;
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        filePath = saveDir + "/" + fileName;
        if (!viewercamrea_py->pixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
}

void cameratest::on_stopTest_clicked() {
    showlog("用户按了结束测试");
    pb->set(DeviceCmd::DevReset);
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    cameraSendTimer->stop();
    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
    switch (getIndex()) {
        case 1: emit send_set_camera_action(STATE_THOROUGHFARE1_OUT); break;
        case 2: emit send_set_camera_action(STATE_THOROUGHFARE2_OUT); break;
        case 3: emit send_set_camera_action(STATE_THOROUGHFARE3_OUT); break;

        default: break;
    }
}

void cameratest::on_OffsetTest_clicked() {
    // python.exe .u7p_camera_defect_detect_env/code/onnx_inference.py --model "./code/infer_240723_320_model.onnx"
    // --img "./code/test.png"
    // python.exe ./code/onnx_inference --model "./code/infer_240723_320_model.onnx" --img "绝对路径"
    //    arguments << "script.py" << QDir::currentPath() + "/图片存储/脏污正常"<< "--flag";
    // python.exe ./code/onnx_inference.py --model "./code/infer_240723_320_model.onnx" --img "./code/test.png"

    int Rect1_X = SETTINGS.value("CAMERA/Rect1_X", 70).toInt();
    int Rect1_Y = SETTINGS.value("CAMERA/Rect1_Y", 25).toInt();
    int Rect1_Width = SETTINGS.value("CAMERA/Rect1_Width", 40).toInt();
    int Rect1_Height = SETTINGS.value("CAMERA/Rect1_Height", 25).toInt();

    QString filePath;
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName = ui->getMac->text() + "_" + timestamp + ".png";
        QString date = currentDateTime.toString("yyyyMMdd");

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/偏移自动化测试原图/" + date;
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    } else {
        showlog("未识别到图片");
        return;
    }

    QProcess process;
    // 设置虚拟环境的 Python 可执行文件路径
    QString pythonPath = "./u7p_camera_defect_detect_env/python.exe";

    // 设置要运行的 Python 脚本及其参数
    QString scriptPath = "./code/onnx_inference.py";
    QStringList arguments;

    if (pack.product == "Q20P") {
        scriptPath = "./code/onnx_inference_pos_q30_250113.py";
        arguments << "--model"
                  << "./code/q30_model_250114.onnx"
                  << "--img" << filePath << "--x1_s" << QString::number(Rect1_X) << "--y1_s" << QString::number(Rect1_Y)
                  << "--w_s" << QString::number(Rect1_Width) << "--h_s" << QString::number(Rect1_Height);
    } else {  //给木星
        arguments << "--model"
                  << "./code/infer_240725_320_model.onnx"
                  << "--img" << filePath << "--x1_s" << QString::number(Rect1_X) << "--y1_s" << QString::number(Rect1_Y)
                  << "--w_s" << QString::number(Rect1_Width) << "--h_s" << QString::number(Rect1_Height);
    }
    // 设置工作目录为虚拟环境目录
    process.setWorkingDirectory("./u7p_camera_defect_detect_env");

    qDebug() << "指令内容为:" << scriptPath << arguments;
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
    int flag;
    // 解析输出字符串
    QStringList parts = output.trimmed().split(' ');
    if (parts.size() != 5) {
        qDebug() << "Unexpected output format";
    }

    bool ok;
    int x1 = parts[0].toInt(&ok);
    int y1 = parts[1].toInt(&ok);
    int w = parts[2].toInt(&ok);
    int h = parts[3].toInt(&ok);
    flag = parts[4].toInt(&ok);
    QString pyTestResult;
    if (!ok) {
        qDebug() << "Error parsing integers";
    }

    // 输出解析后的数据
    qDebug() << "Parsed Values:";
    qDebug() << "x1:" << x1;
    qDebug() << "y1:" << y1;
    qDebug() << "w:" << w;
    qDebug() << "h:" << h;
    qDebug() << "flag:" << flag;
    if (flag == 1) {
        TestItem test;
        test.testItem = "偏位测试";
        test.testData = "通过";
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        pyTestResult = "偏位测试通过";
        showlog("偏位测试通过");
    }

    if (flag == 0) {
        TestItem test;
        test.testItem = "偏位测试";
        test.testData = "刷头偏位";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        pyTestResult = "刷头偏位";
        showlog("刷头偏位");

        on_abnormal_clicked();
    }
    if (flag == -1) {
        TestItem test;
        test.testItem = "偏位测试";
        test.testData = "算法计算失败";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        pyTestResult = "算法计算失败";
        showlog("算法计算失败");
        on_abnormal_clicked();
    }

    QImage image;
    image.load(filePath);  // 加载图像文件
    QPainter painter(&viewercamrea_py->pixmap);

    int Rect2_X = x1;
    int Rect2_Y = y1;
    int Rect2_Width = w;
    int Rect2_Height = h;

    QPen pen(Qt::red);    // 创建一个红色的画笔
    painter.setPen(pen);  // 设置画笔颜色

    painter.drawImage(0, 0, image);  // 在 pixmap 上绘制图片

    painter.drawRect(Rect1_X, Rect1_Y, Rect1_Width, Rect1_Height);  // 绘制第一个矩形
    painter.drawRect(Rect2_X, Rect2_Y, Rect2_Width, Rect2_Height);  // 绘制第二个矩形
    viewercamrea_py->updateImage();                                 // 更新视图
    painter.end();                                                  // 明确结束绘图操作
    if (!viewercamrea_py->pixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName = pyTestResult + "_" + ui->getMac->text() + "_" + timestamp + ".png";
        QString date = currentDateTime.toString("yyyyMMdd");

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/偏移自动化测试结果图/" + date;
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea_py->pixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }

    if (flag == 1) {
        switch (getIndex()) {
            case 1: emit send_set_camera_action(STATE_THOROUGHFARE1_IN); break;
            case 2: emit send_set_camera_action(STATE_THOROUGHFARE2_IN); break;
            case 3: emit send_set_camera_action(STATE_THOROUGHFARE3_IN); break;

            default: break;
        }

        if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool())
            on_normal_clicked();
        else
            can_start_dirty_test = 1;
        showlog("可以开始赃污测试");
    }
}

// 添加数据包到容器
void cameratest::addPacket(const QByteArray& packet) {
    int seqNumber = static_cast<uchar>(packet[0]);
    packetMap.insert(seqNumber, packet.mid(1));  // 存储去掉序号的数据部分
    qDebug() << "添加包:" << seqNumber;
}

// 检查是否有丢失的包
void cameratest::checkMissingPackets() {
    int lastSeqNumber = packetMap.isEmpty() ? -1 : packetMap.lastKey();

    for (int i = 0; i <= lastSeqNumber; ++i) {
        if (!packetMap.contains(i)) {
            faultData.append(i);
        }
    }

    if (faultData.isEmpty()) {
        qDebug() << "No missing packets.";
    } else {
        qDebug() << "Missing packets:" << faultData;
    }
}
// 重新组装数据包
QByteArray cameratest::reassembleData() {
    // 从 QMap 获取序号列表
    QList<int> keysList = packetMap.keys();

    // 将 QList 转换为 QVector
    QVector<int> keysVector(keysList.begin(), keysList.end());

    std::sort(keysVector.begin(), keysVector.end());  // 按序号排序

    QByteArray completeData;

    for (int seq : keysVector) {
        completeData.append(packetMap.value(seq));
    }

    return completeData;
}
void cameratest::getPictureSendOver(FacPictureDataAck x) {
    waitWork(50);  //等待数据彻底处理完毕
    checkMissingPackets();
    qDebug() << "错误个数" + QString::number(faultData.size()) << x.send_data_over;
    //   showlog("错误个数"+QString::number(faultData.size()));
    emit send_fault_data_packet(faultData.size(), faultData);
}

void cameratest::on_ResolutionTestButton_clicked() {
    int flag = 1;
    if (!SETTINGS.value("SYSTEM/SimplePcbaTest").toBool()) {
        //  ui->ResolutionTestButton->setDisabled(1);
        QString filePath;
        if (!viewercamrea->temporarypixmap.isNull()) {
            QDateTime currentDateTime = QDateTime::currentDateTime();
            QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");
            QString fileName = ui->getMac->text() + "_" + timestamp + ".png";
            QString date = currentDateTime.toString("yyyyMMdd");

            QString saveDir = QDir::currentPath() + "/图片存储/解析力测试原图/" + date;
            QDir dir(saveDir);
            if (!dir.exists()) {
                dir.mkpath(".");
            }
            filePath = saveDir + "/" + fileName;
            if (!viewercamrea->temporarypixmap.save(filePath)) {
                qDebug() << "Failed to save image:" << fileName;
            } else {
                qDebug() << "Image saved successfully to:" << filePath;
            }
        } else {
            showlog("未识别到图片");
            return;
        }
        // python.exe ./code/blur_detect_out.py --img "./code/blur_test.png"
        // python.exe ./code/blur_detect_out.py --img
        // "D:/new_production/build-new_production-Desktop_Qt_5_15_2_MSVC2019_64bit-Debug/bin/_20241010_134321.png"
        // python.exe ./code/blur_detect_out.py --img "./code/blur_test.png"

        QProcess process;

        QString pythonPath = "./u7p_camera_defect_detect_env/python.exe";
        QString scriptPath;
        QStringList arguments;
        if (pack.product == "Q20P") {
            scriptPath = "./code/onnx_inference_clearness_q30_250113.py";
            arguments << "--model"
                      << "./code/q30_model_250114.onnx"
                      << "--img" << filePath;
        } else {  //给木星
            scriptPath = "./code/blur_detect_out.py";
            arguments << "--img" << filePath;
        }

        process.setWorkingDirectory("./u7p_camera_defect_detect_env");
        qDebug() << "指令内容为:" << scriptPath << arguments;
        process.start(pythonPath, QStringList() << scriptPath << arguments);
        if (!process.waitForStarted()) {
            qDebug() << "Failed to start process";
        }
        process.waitForFinished();
        QString output = process.readAllStandardOutput();
        QString errorOutput = process.readAllStandardError();
        qDebug() << "Output:" << output;
        qDebug() << "Error Output:" << errorOutput;
        if (output == "") {
            TestItem test;
            test.testItem = "解析力测试";
            test.testData = "图片违规";
            test.testResult = "失败";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            on_abnormal_clicked();
            return;
        }
        QString jxlTestResult;

        QList<QRect> rectangles;
        QStringList parts = output.trimmed().split('\n');
        for (const QString& part : parts) {
            QStringList values = part.trimmed().split(' ');
            if (values.size() != 3) {
                qDebug() << "Unexpected output format";
                continue;
            }
            bool ok;
            flag = values[0].toInt(&ok);
            float mid_clearness = values[1].toFloat(&ok);
            float boundary_clearness = values[2].toFloat(&ok);

            if (!ok) {
                qDebug() << "Error parsing integers";
                continue;
            }
            // rectangles.append(QRect(x1, y1, w, h));
            // flag = 1 OK图片
            //     flag = 0 NG图片有脏污
            //     flag = -1 NG图片出现裂缝（画面撕裂）
            if (flag == 1) {
                TestItem test;
                test.testItem = "解析力测试";
                test.testData = "中间清晰度:" + QString::number(mid_clearness) +
                                "边缘清晰度:" + QString::number(boundary_clearness);
                test.testResult = "通过";
                test.ask = "通过";
                testItems.append(test);

                testResultTableUpdate(testItems);

                jxlTestResult = "解析力测试通过";
                showlog("解析力测试通过");
            }

            if (flag == 0) {
                TestItem test;
                test.testItem = "解析力测试";
                test.testData = "中间清晰度:" + QString::number(mid_clearness) +
                                "边缘清晰度:" + QString::number(boundary_clearness);
                ;
                test.testResult = "失败";
                test.ask = "通过";
                testItems.append(test);
                testResultTableUpdate(testItems);
                jxlTestResult = "解析力测试失败";
                showlog("解析力测试失败");

                on_abnormal_clicked();
            }
            if (flag == -1) {
                TestItem test;
                test.testItem = "解析力测试";
                test.testData = "无法测试";
                test.testResult = "失败";
                test.ask = "通过";
                testItems.append(test);

                testResultTableUpdate(testItems);

                jxlTestResult = "解析度盘歪";
                showlog("图片算法处理不了，大概率是解析度盘太歪了");
                on_abnormal_clicked();
            }
        }

        QImage image;
        if (!image.load(filePath)) {
            qWarning() << "Failed to load image from file path:" << filePath;
            return;
        }
        QPixmap pixmap = QPixmap::fromImage(image);
        QPainter painter(&pixmap);
        QPen pen(Qt::red);
        painter.setPen(pen);
        for (const QRect& rect : rectangles) {
            painter.drawRect(rect);
        }
        viewercamrea_py->pixmap = pixmap;
        viewercamrea_py->updateImage();

        if (!viewercamrea_py->pixmap.isNull()) {
            QDateTime currentDateTime = QDateTime::currentDateTime();
            QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");
            QString fileName = jxlTestResult + "_" + ui->getMac->text() + "_" + timestamp + ".png";
            QString date = currentDateTime.toString("yyyyMMdd");
            QString saveDir = QDir::currentPath() + "/图片存储/解析力自动化测试结果图/" + date;
            QDir dir(saveDir);
            if (!dir.exists()) {
                dir.mkpath(".");
            }
            filePath = saveDir + "/" + fileName;
            if (!viewercamrea_py->pixmap.save(filePath)) {
                qDebug() << "Failed to save image:" << fileName;
            } else {
                qDebug() << "Image saved successfully to:" << filePath;
            }
        }
        // ui->jxl_normal->setDisabled(0);
        // ui->jxl_abnormal->setDisabled(0);
        // ui->DirtyTestButton->setDisabled(0);
        if (flag == 1)
            on_OffsetTest_clicked();
    } else {
        if (flag == 1)
            on_OffsetTest_clicked();
    }
}


