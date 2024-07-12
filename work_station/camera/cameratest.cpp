
#include "cameratest.h"
#include "ImageWindow.h"
#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_cameratest.h"
#include <QPainter>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
void cameratest::on_pushButton_clicked()
{
    // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    ui->macInput->setText("3C:84:27:07:A8:D2");
    ui->macInput->setText("3C:84:27:20:01:3E");
    ui->macInput->setText("3c:84:27:20:00:c6");
    ui->macInput->setText("B4:56:5D:BF:53:71");   // wd牙刷
    ui->macInput->setText("b4:56:5d:bf:54:4e");   // wd牙刷
    ui->macInput->setText("b4:56:5d:bf:57:9d");
    ui->macInput->setText("F8:8F:C8:57:73:E9");


    on_macInput_returnPressed();



    // for(int i=0 ;i<500;i++)
    // {
    //     ui->log->appendPlainText("当前次数为：" + QString::number(i));
    //         at->sendMac(ui->macInput->text());   // 开始连接
    //     waitWork(1000);

    // }
}
cameratest::cameratest(int index, QWidget *parent) : ui(new Ui::cameratest), m_index(index)
{
    ui->setupUi(this);
    update_main_style("Ubuntu.qss");

    scanSerialPorts();   // 要搜索一下一开始

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    // mes失败停止。

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    settings.setValue("Window/Size", this->size());

    ui->msgEdit->appendPlainText("action=" + pack.test_station);
    ui->msgEdit->appendPlainText("model=" + pack.model);
    ui->msgEdit->appendPlainText("action=" + pack.action);
    ui->msgEdit->appendPlainText("line=" + pack.line);
    ui->msgEdit->appendPlainText("machineNo=" + pack.machineNo);

    // 将定时器的timeout信号连接到槽函数onTimeout
    connect(cameraSendTimer, &QTimer::timeout, this, &cameratest::onTimeout);

    connect(this, &cameratest::imageProcessed, this, &cameratest::updateImageOnMainThread);

    viewercamrea = new ImageViewer("image_markings.png", this);
    ui->verticalLayout->addWidget(viewercamrea);   // 将 ImageViewer 添加到布局中
    viewercamrea->show();                          // 显示 ImageViewer
    dongleRingBuf =
        new RingBuf(&p_dongleRingBuffer, dongle_ring_buffer, 1, sizeof(dongle_ring_buffer));
    cameraRingBuf = new RingBuf(&p_cameraRingBuffer, camera_ring_buf, 1, sizeof(camera_ring_buf));

    // 启动后台线程
    future = QtConcurrent::run([this]() {
        while (running.load()) {
            solve_frame();
            QThread::msleep(10); // 等待10毫秒
        }
    });
    running.store(true);

    // //连接信号和槽
    // QObject::connect(cameratimer, &QTimer::timeout, this, &cameratest::solve_frame);
    // //启动定时器
    // cameratimer->start(10);
}
void cameratest::write_camera_data(uint8_t *p_data, int data_len)
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
int cameratest::ext_ble_find_next_frame(void)
{
    int i = 0;
    ext_uart_phy_layer_t *head = NULL;
    int len = dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, 2 * 1024);
    // qDebug() << "查找下一帧，剩下：" << len;

    for (i = 0; i < len; i++)
    {
        if (frame_buf[i] == 0xCC && frame_buf[i + 1] == 0xCC && frame_buf[i + 2] == 0xCC &&
            frame_buf[i + 3] == 0xCC)
        {
            head = (ext_uart_phy_layer_t *)&frame_buf[i];
            if (head->magic == EXT_UART_MAGIC)
            {
                qDebug() << "匹配到了串口帧头";

                dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, i);

                return 1;
            }
        }
    }

    dongleRingBuf->usmile_ring_buffer_delete(&p_dongleRingBuffer, len);

    return 0;
}
int cameratest::ext_ble_find_next_picture_frame(void)
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
            if (head->reserved == EXT_PICTURE_PHY_LAYER_MAGIC && head->width == 0xb4 &&
                head->height == 0xc8)
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

void cameratest::printSquareData(uint8_t *data, size_t data_size)
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

void cameratest::solve_picture_frame(void)
{
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
        if (head->width == 0xb4 && head->height == 0xc8)   // 回车
        {
            int frame_size = head->data_size + PICTURE_PHY_LAYER_HEAD_SIZE;

            if (frame_size > ring_size)
            {
                qDebug() << "图片帧数据不完整" << "需要" << frame_size << "实际为" << ring_size
                         << "包数" << ring_size / 244 << "余数" << ring_size % 244 << "包头内容为"
                         << frame_picture_buf[ring_size];
                break;
            }

            // 从环形缓冲区中读取整个帧的数据
            cameraRingBuf->usmile_ring_buffer_pick(&p_cameraRingBuffer, frame_picture_buf,
                                                   frame_size);
            qDebug() << "图片帧数据完整" << "需要" << frame_size << "实际为" << ring_size << "包数"
                     << ring_size / 244 << "余数" << ring_size % 244 << "包头内容为"
                     << frame_picture_buf[ring_size];

            qDebug() << "图片帧数据完整" << "需要" << frame_size << "实际为" << ring_size
                     << sizeof(head);

            crc16 = head->data_crc16;
            crc_cali = CRC16(head->data, head->data_size);
            if (crc16 == crc_cali)
            {
                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char *>(head), frame_size);
                pictureByteArray = byteArray;
                emit imageProcessed();
            }
            else
            {
                qDebug() << "head content:"
                         << QByteArray(reinterpret_cast<char *>(head), PICTURE_PHY_LAYER_HEAD_SIZE)
                                .toHex();
                ui->msgEdit->appendPlainText("图片的crc错误");
                qDebug() << "crc校验失败" << crc_cali << crc16 << "head->data[0]" << head->data[0]
                         << "head->data[1]" << head->data[1];
                // printSquareData(head->data, head->data_size);
                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char *>(head), frame_size);
                pictureByteArray = byteArray;
                emit imageProcessed();
            }

            // 删除已经处理的帧数据
            dongleRingBuf->usmile_ring_buffer_delete(&p_cameraRingBuffer, frame_size);
        }
        else
        {
            qDebug() << "数据流错误寻找下一帧不满足head->width == 0xb4 && head->height == 0xc8";
            qDebug() << "当前数据包头为:"
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

        // QThread::msleep(100); // 等待1秒
        QCoreApplication::processEvents();
    }
}

void cameratest::solve_frame(void)
{
    while (true)
    {
        // 从环形缓冲区中读取帧头
        dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf,
                                               UART_PHY_LAYER_HEAD_SIZE);
        int ring_size = dongleRingBuf->usmile_ring_buffer_items_count_get(&p_dongleRingBuffer);
        if (ring_size <= UART_PHY_LAYER_HEADER_ADN_CRC)
        {
            // qDebug() << "串口环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }

        ext_uart_phy_layer_t *head = (ext_uart_phy_layer_t *)frame_buf;

        if (head->magic == EXT_UART_MAGIC)
        {
            int frame_size = UART_PHY_LAYER_HEADER_ADN_CRC + head->length;

            if (frame_size > ring_size)
            {
                // qDebug() << "串口帧数据不完整" << "需要" << frame_size << "实际为" << ring_size;
                break;
            }

            // 从环形缓冲区中读取整个帧的数据
            dongleRingBuf->usmile_ring_buffer_pick(&p_dongleRingBuffer, frame_buf, frame_size);

            // if (head->data[head->length]==0x0d&&head->data[head->length+1] == 0x0A)
            if (1)
            {
                // 处理帧数据
                write_camera_data(head->data, head->length);
                cameradatasize = head->length + cameradatasize;
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
            qDebug() << "串口数据流错误寻找下一帧";
            qDebug() << "串口数据包头为:"
                     << QByteArray(reinterpret_cast<char *>(frame_buf), UART_PHY_LAYER_HEAD_SIZE)
                            .toHex();

            if (ext_ble_find_next_frame())
            {
                continue;
            }
            else
            {
                qDebug() << "找不到串口帧头";
                break;
            }
        }
        QCoreApplication::processEvents();
        // QThread::msleep(100); // 等待1秒
    }
}
void cameratest::refresh_camera_CONTROL(FacCameraControl style)
{
    qDebug() << getIndex() << "收到摄像头控制回应" << style.result;
    is_camera_control = 1;
}

cameratest::~cameratest()
{
    //
    delete ui;
}

void cameratest::on_disconnectButton_clicked()
{
    closeDongleSerialPort();
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    refresh_ble_state(0);
}
void cameratest::refresh_ble_state(int state)
{
    if (state)
    {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   ui->msgEdit->appendPlainText("蓝牙连接成功");
        pb->set_forbid_sleep(FacSwitch_OPEN);
        ui->msgEdit->appendPlainText("已发送禁止休眠");
    }
    else
    {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}
void cameratest::refresh_dongle_uart_state(int state)
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

void cameratest::on_connectButton_clicked()
{
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}
void cameratest::on_macInput_returnPressed()
{
    viewercamrea->pixmap = QPixmap();

    viewercamrea->temporarypixmap= QPixmap();
    viewercamrea->updateImage();

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
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
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        isScreenContinue = true;
        emit goNextFocus();

        state = STATE_IDLE;
    }
}

void cameratest::solveMesSucess(const int mechines)
{
    if (mechines == getIndex())
    {
        ui->msgEdit->appendPlainText("mes操作成功");
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; border-radius: 10px; padding: 10px; text-align: center;");

        mes_set_ok = 1;
    }
}
void cameratest::solveMesData(const int mechines, QString msg)
{
    if (mechines == getIndex())
    {
        ui->msgEdit->appendPlainText("MES:报错信息:" + msg);
        ui->macInput->setDisabled(0);
        ui->get_mac->setDisabled(0);
        isScreenContinue = false;
        ui->msgEdit->appendPlainText("停止运行");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
        emit endTest(getIndex());

        ui->get_mac->clear();
        ui->get_mac->setFocus();
    }
}

void cameratest::band_sn_mac_to_csv(const QString &macAddress, const QString &sn)
{
    QString folderPath = "D:/测试结果";

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
        qDebug() << getIndex() << "文件保存到" << filePath;
    }
    else
    {
        qDebug() << getIndex() << "文件没关或者其他问题";
        ui->msgEdit->appendPlainText("文件无法打开");
    }
}

void cameratest::closeEvent(QCloseEvent *)
{

    running.store(false);
    // 等待线程结束
    future.waitForFinished();
    qDebug() << getIndex() << "开始关闭";
    isScreenContinue = false;
}
void cameratest::refresh_sn(FacDevInfo data)
{
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("存储尾盖sn:" + stringsn);
}

void cameratest::can_go_next(int x)
{
    is_can_go_next = 1;
    qDebug() << getIndex() << "得到信息" << getIndex();
    if (x == getIndex())
    {
        result = failValue;
        ui->msgEdit->appendPlainText("停止运行");
        state = STATE_SAVE_RESULT;
        ui->msgEdit->appendPlainText("摄像头测试失败");
        ui->test_result->setText("FAIL");
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}
void cameratest::processInspection(QString stringsn)
{
    if (stringsn != "" || !ui->isusemes->checkState())
    {
        if (ui->isusemes->checkState())
        {
            ui->msgEdit->appendPlainText("正在进行站前检测");
            pack.sn = stringsn;
            pack.mechines = getIndex();
            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit sendProcessInspection(pack);
        }
    }
    else
    {
        ui->msgEdit->appendPlainText("SN比对错误");
    }

    if (!ui->isusemes->checkState())   // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet(
            "font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid #FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}
void cameratest::onTimeout()
{
    ui->msgEdit->appendPlainText("再次获取图片");
    pb->set_camera_picture_state(1);
    std::memset(dongle_ring_buffer, 0,
                sizeof(dongle_ring_buffer));   // 将数组全部初始化为零
    std::memset(camera_ring_buf, 0,
                sizeof(camera_ring_buf));   // 将数组全部初始化为零
    ui->msgEdit->appendPlainText("图片数据包大小=" + QString::number(cameradatasize));
    cameradatasize = 0;
}
void cameratest::start_task()
{
    if (isScreenContinue)
    {
        ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state)
        {
        case STATE_IDLE:   // 复位一切

            ui->msgEdit->appendPlainText("开始测试");
            pb->reset_all_pb();
            testItems.clear();
            at->resetConnected();
            displayRectangles = false;
            refresh_ble_state(0);
            ui->tail_sn->setText("存储尾盖sn:");
            stringsn = "";
            cameraSendTimer->stop();
            result = passValue;
            is_can_go_next = 0;
            is_camera_control = 0;
            TestTime.start();
            dongleOutTime = 10;   // 太快会死锁
            at->sendMac(ui->macInput->text());   // 发送mac地址
            qDebug() << getIndex() << macAddress;
            state = STATE_WATI_CONNECT;
            break;
        case STATE_WATI_CONNECT:
            if (at->getConnected())
            {
                pb->set_camera_state(0);
                qDebug() << getIndex() << "蓝牙状态" << at->getConnected();
                waitWork(WAITTIME);
                ui->msgEdit->appendPlainText("蓝牙连接成功");
                pb->set_sn(FacDevInfoType_TAIL_SN, sn);
                band_sn_mac_to_csv(macAddress, sn);
                state = STATE_BANDING;
            }
            break;

        case STATE_BANDING:
            if (pb->get_is_banding_ok())
            {
                pb->get_base_info();
                ui->msgEdit->appendPlainText("sn已成功绑定保存");
                state = STATE_DISABLE_SLEEP_1;
            }
            else
            {
                waitWork(500);
                pb->set_sn(FacDevInfoType_TAIL_SN, sn);
                ui->msgEdit->appendPlainText("正在重试sn绑定");
            }

            break;
        case STATE_DISABLE_SLEEP_1:
            if (pb->getDisableSleep())
            {
                ui->msgEdit->appendPlainText("已进入禁止休眠模式");
                if (pack.product == "U7P")
                {
                    // 设置定时器的超时时间为6000毫秒（6秒）
                    cameraSendTimer->setInterval(6000);
                    // 启动定时器
                    cameraSendTimer->start();

                    pb->set_camera_picture_state(1);
                    std::memset(dongle_ring_buffer, 0,
                                sizeof(dongle_ring_buffer));   // 将数组全部初始化为零
                    std::memset(camera_ring_buf, 0,
                                sizeof(camera_ring_buf));   // 将数组全部初始化为零

                    state = CAMERA_TEST;

                    ui->msgEdit->appendPlainText("等待显示照片");
                }
                else
                {
                    on_distribution_network_clicked();
                    state = STATE_NET_SET;
                }
            }
            else
            {
                waitWork(500);
                pb->set_forbid_sleep(FacSwitch_OPEN);
                ui->msgEdit->appendPlainText("已发送禁止休眠");
            }
            break;

        case STATE_NET_SET:
            waitWork(500);
            if (pb->get_is_wif_set())
            {
                ui->msgEdit->appendPlainText("已配网成功");
                //  waitWork(5000);
                ui->msgEdit->appendPlainText("现在打开摄像头");
                pb->set_camera_state(1);
                ui->msgEdit->appendPlainText("锁定曝光时间");
                on_exposure_time_edit_returnPressed();
                // pb->set_camera_light_state(1);
                state = CAMERA_TEST;
                ui->msgEdit->appendPlainText("等待显示照片");
            }
            else
            {
                waitWork(500);
                on_distribution_network_clicked();
                ui->msgEdit->appendPlainText("已重新发送配网");
            }
            break;

        case CAMERA_TEST:
            is_camera_control = 1;
            if (is_camera_control && is_can_go_next)
            {
                ui->msgEdit->appendPlainText("摄像头测试通过");
                is_can_go_next = 0;
                is_camera_control = 0;

                state = STATE_SAVE_RESULT;
            }
            break;

        case STATE_SAVE_RESULT:

            if (result == passValue)
            {
                QString mesresult = "PASS";

                pack.result = mesresult;

                pack.itemvalue = QString("|CAMERA_TEST:PASS|");

                pack.sn = ui->get_mac->text();

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

                pack.itemvalue = QString("|CAMERA_TEST:NG|");

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
            test.testItem = "摄像头测试";
            test.testData = "";
            test.testResult = result;
            testItems.append(test);
            log->saveTestCsv(CAMERA_VER, ui->get_mac->text(), ui->macInput->text(), testItems);

            pb->set_camera_state(0);
            pb->set_camera_light_state(0);

            stringsn = "";
            ui->macInput->clear();
            ui->get_mac->clear();
            ui->macInput->setDisabled(0);
            ui->get_mac->setDisabled(0);
            waitWork(WAITTIME);
            pb->set_dev_reset();
            waitWork(WAITTIME);
            emit endTest(getIndex());

            at->sendMac("00:00:00:00:00:00");   // 发送mac地址
            waitWork(150);
            on_disconnectButton_clicked();
            ui->msgEdit->appendPlainText("测试结束");
            isScreenContinue = false;
            state = STATE_IDLE;
            break;
        }

        //  QCoreApplication::processEvents();
    }
}

void cameratest::on_lcdTestButton_clicked()
{
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
void cameratest::showlog(QString msg)
{
    ui->msgEdit->appendPlainText(msg);
    qDebug() << getIndex() << msg;
}

void cameratest::refreshMesState(int state)
{
    if (state)
        ui->msgEdit->appendPlainText("mes登录成功");
    else
        ui->msgEdit->appendPlainText("mes登录失败");
}

void cameratest::getTestValue(const int mechines, const QString value)
{
    // ui->msgEdit->appendPlainText(value);
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
            ui->msgEdit->appendPlainText("mes未找到匹配的MAC地址");
            ui->msgEdit->appendPlainText(value);
        }
    }
    // ui->msgEdit->appendPlainText(value);
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
            on_macInput_returnPressed();
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
void cameratest::get_dongle_ver(QString data)
{
    ui->msgEdit->appendPlainText("当前dongle的版本为：" + data);
}
void cameratest::get_mac(QString sn_to_search)
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
            {                                 // 至少需要两个字段
                QString sn = fields.at(0);    // 第一个字段是sn
                QString mac = fields.at(1);   // 第二个字段是mac
                if (sn == sn_to_search)
                {   // 检查是否是待检索的sn
                    ui->msgEdit->appendPlainText("这是从文件获取的mac地址");
                    ui->macInput->setText(mac);
                    on_macInput_returnPressed();
                    qDebug() << getIndex() << "The corresponding mac is: " << mac;
                    break;
                }
            }
            else
            {
                ui->msgEdit->appendPlainText("存在没有逗号分开的" +
                                             QString::number(fields.count()) + line);
            }
        }
        file.close();   // 关闭文件
    }
}
void cameratest::on_get_mac_returnPressed()
{
    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; padding: 10px; text-align: center; ");
    ui->get_mac->setDisabled(1);
    ui->macInput->setDisabled(1);
    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->get_mac->text()).hasMatch())
    {
        ui->get_mac->setDisabled(0);
        ui->macInput->setDisabled(0);
        ui->msgEdit->appendPlainText("序列号错误");
        ui->get_mac->clear();
        return;
    }
    sn = ui->get_mac->text().toUtf8();
    ui->msgEdit->appendPlainText("正在查询mac地址");
    get_mac(ui->get_mac->text());   // 文件获取
    processInspection(ui->get_mac->text());

    processGetMesTestValue();   // mes获取
}
void cameratest::processGetMesTestValue()
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
void cameratest::on_close_camera_clicked()
{
    pb->set_camera_state(0);
}

void cameratest::on_open_camera_clicked()
{
    pb->set_camera_state(1);
}

void cameratest::on_open_camear_light_clicked()
{
    pb->set_camera_light_state(1);
}

void cameratest::on_close_camear_light_clicked()
{
    pb->set_camera_light_state(0);
}
void cameratest::on_distribution_network_clicked()
{
    // 获取IP地址
    QString ipString = "0.0.0.0";
    QString localHostName = QHostInfo::localHostName();
    QHostInfo hostInfo = QHostInfo::fromName(localHostName);
    qDebug() << getIndex() << "Local IP addresses:";

    for (QHostAddress address : hostInfo.addresses())
    {
        if (address.protocol() == QAbstractSocket::IPv4Protocol)
        {
            qDebug() << getIndex() << address.toString();
            // ipString = address.toString();
        }
    }
    ipString = ui->client_ip_label->text();
    ui->client_ip_label->setText(ipString);

    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString wifiName = ui->ssid_lineEdit->text();
    QString wifiPassword = ui->password_lineEdit->text();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected())
    {
        pb->set_new_connect_wifi(wifiNameBytes, wifiPasswordBytes, ipString, ui->port_num->text());
        ui->msgEdit->appendPlainText("已设置连接wifi");
    }
    else
    {
        ui->msgEdit->appendPlainText("请等待连接牙刷后再试");
    }

    if (!isExecuted)
    {
        // 获取QLineEdit中的端口号文本
        QString portText = ui->port_num->text();
        bool conversionOk;
        int port = portText.toInt(&conversionOk);

        udpSocket = new QUdpSocket(this);
        QHostAddress ipAddress(ipString);
        bool bindResult = udpSocket->bind(ipAddress, port);
        qDebug() << getIndex() << "connect_udp,localhost: " << ipString << ":" << port;
        qDebug() << getIndex() << "Bind result: " << bindResult;

        if (!bindResult)
        {
            ui->msgEdit->appendPlainText("ip绑定失败");
        }
        else
        {
            ui->msgEdit->appendPlainText("ip绑定成功");
        }

        connect(udpSocket, SIGNAL(readyRead()), this, SLOT(readPendingDatagrams()));

        isExecuted = true;
    }
}

void cameratest::on_save_photo_clicked()
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
void cameratest::readDongleSerialPortData()
{
    dongleSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = dongleSerialPortBuf;   // 读取缓冲区数据
    int write_len = 0;
    int len = dataTemp.size();
    write_len = dongleRingBuf->usmile_ring_buffer_write(
        &p_dongleRingBuffer, reinterpret_cast<uint8_t *>(dataTemp.data()), dataTemp.size());

    if (write_len < len)
    {
        qDebug() << "write_len:" << write_len << "len:" << dataTemp.size();
        ui->msgEdit->appendPlainText("写入串口池失败");
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
    QString logEntry = QString("[%1] %2").arg(timestamp, dataTemp);
    // 将最终字符串追加到日志编辑器中
    logEdit()->appendPlainText(logEntry);
    dongleSerialPortBuf.clear();   // 清除缓冲区
}

void cameratest::readPendingDatagrams()
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
void cameratest::processTheDatagram(QByteArray &datagram)
{
    qDebug() << "Received datagram with length: " << datagram.size();

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

    // 打印图像数据的前20字节
    qDebug() << "图像数据前20字节:" << imageBytes.left(20).toHex();

    // 尝试将 QByteArray 转换为 QImage
    QImage image(reinterpret_cast<const uchar *>(imageBytes.data()), 180, 200,
                 QImage::Format_Grayscale8);

    if (image.isNull())
    {
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
    else
    {
        qDebug() << "直接从字节数据转换图像成功。";
        ui->msgEdit->appendPlainText("请判断视频是否正常");
    }

    qDebug() << "图像加载成功。";
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
    viewercamrea->temporarypixmap = QPixmap::fromImage(image);



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
void cameratest::updateImageOnMainThread()
{
    processTheDatagram(pictureByteArray);   // 显示图片
}
void cameratest::on_normal_clicked()
{
    can_go_next(0);
    cameraSendTimer->stop();
}

void cameratest::on_abnormal_clicked()
{
    can_go_next(getIndex());
    cameraSendTimer->stop();
}

void cameratest::on_exposure_time_edit_returnPressed()
{
    pb->set_camera_exposure_time(ui->exposure_time_edit->text().toUInt());
}

void cameratest::on_OffsetTest_clicked()
{
    displayRectangles = !displayRectangles;   // 切换 displayRectangles 的值
    // displayImage("image.png");              //
    // 重新显示图片，这里假设你有一个保存当前图片路径的变量 currentImagePath
}

void cameratest::on_DirtyTestButton_clicked()
{
    QImage image("image.png");   // 加载图像

    // 设置脏污的阈值参数，该参数需要释放到配置文件外让产线人员进行调参。
    int threshold = 100;   // 设置阈值
    int blockWidth = 3;
    int blockHeight = 3;

    QPainter painter(&image);
    QPen pen;
    pen.setColor(Qt::red);
    painter.setPen(pen);

    bool isDirty = false;   // 设置一个标志，表示是否发现了低亮度区域

    for (int i = 0; i < image.width() - blockWidth; i += blockWidth)
    {
        for (int j = 0; j < image.height() - blockHeight; j += blockHeight)
        {
            int sumBrightness = 0;
            for (int x = i; x < i + blockWidth; ++x)
            {
                for (int y = j; y < j + blockHeight; ++y)
                {
                    QColor color(image.pixel(x, y));
                    sumBrightness += color.lightness();   // 计算亮度
                }
            }
            int averageBrightness = sumBrightness / (blockWidth * blockHeight);
            if (averageBrightness < threshold)
            {
                // qDebug() << "Low brightness detected at (" << i << ", " << j << ")";
                // QString message = QString("Low brightness detected at (%1, %2)").arg(i).arg(j);
                // QMessageBox::information(nullptr, "Information", message);
                painter.fillRect(QRect(i, j, blockWidth, blockHeight),
                                 Qt::red);   // 在低亮度区域绘制红色矩形
                isDirty = true;              // 发现了低亮度区域，将标志设置为true
            }
        }
    }

    painter.end();
    image.save("image_markings.png");   // 保存修改后的图像
                                        // displayImage("image_markings.png");

    if (!isDirty)
    {   // 如果没有发现低亮度区域，弹出信息框
        QMessageBox::information(this, tr("Information"), tr("No dirty area detected."));
    }
}

void cameratest::on_stopTest_clicked()
{
    at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    waitWork(100);
    ui->macInput->setDisabled(0);
    ui->get_mac->setDisabled(0);
    cameraSendTimer->stop();
    ui->macInput->clear();
    ui->get_mac->clear();
    ui->get_mac->setFocus();
    on_disconnectButton_clicked();
}

void cameratest::on_jxl_abnormal_clicked()
{
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName =  ui->get_mac->text()+"_"+ timestamp + ".png";

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/解析力不正常";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        QString filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
}


void cameratest::on_jxl_normal_clicked()
{
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName =  ui->get_mac->text()+"_"+ timestamp + ".png";

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/解析力正常";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        QString filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
}


void cameratest::on_zw_normal_clicked()
{
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName =  ui->get_mac->text()+"_"+ timestamp + ".png";

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/脏污正常";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        QString filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
}


void cameratest::on_zw_abnormal_clicked()
{
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName =  ui->get_mac->text()+"_"+ timestamp + ".png";

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/脏污不正常";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        QString filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
}


void cameratest::on_py_normal_clicked()
{
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName =  ui->get_mac->text()+"_"+ timestamp + ".png";

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/偏移正常";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        QString filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
    on_normal_clicked();
}


void cameratest::on_py_abnormal_clicked()
{
    if (!viewercamrea->temporarypixmap.isNull()) {
        // 获取当前日期时间
        QDateTime currentDateTime = QDateTime::currentDateTime();
        QString timestamp = currentDateTime.toString("yyyyMMdd_HHmmss");

        // 生成文件名
        QString fileName =  ui->get_mac->text()+"_"+ timestamp + ".png";

        // 指定保存目录并检查是否存在，不存在则创建
        QString saveDir = QDir::currentPath() + "/图片存储/偏移不正常";
        QDir dir(saveDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }

        // 完整的保存路径
        QString filePath = saveDir + "/" + fileName;

        // 保存图片
        if (!viewercamrea->temporarypixmap.save(filePath)) {
            qDebug() << "Failed to save image:" << fileName;
        } else {
            qDebug() << "Image saved successfully to:" << filePath;
        }
    }
     on_abnormal_clicked();
}

