#include "mainwindow.h"
#include "ui_mainwindow.h"

QString MainWindow::getMotorStateString(FacMotoState state)
{
    switch (state)
    {
    case FacMotoState_UGT_E_SS_IDLE_A:
        return "空闲任务A";
    case FacMotoState_UGT_E_SS_INIT_B:
        return "空闲任务B";
    case FacMotoState_UGT_E_SS_STOP_C:
        return "停止任务C";
    case FacMotoState_UGT_E_SS_START_D:
        return "开始任务D";
    case FacMotoState_UGT_E_SS_PRECHARGE_E:
        return "预充任务E";
    case FacMotoState_UGT_E_SS_TRACK_F:
        return "追踪任务F";
    case FacMotoState_UGT_E_SS_IDENTIFY_G:
        return "识别任务G";
    case FacMotoState_UGT_E_SS_ALIGNMENT_H:
        return "定位任务H";
    case FacMotoState_UGT_E_SS_OPENLOOP_I:
        return "开环任务I";
    case FacMotoState_UGT_E_SS_CLOSEDLOOP_J:
        return "闭环任务J";
    case FacMotoState_UGT_E_SS_BRAKE_K:
        return "打断任务K";
    case FacMotoState_UGT_E_SS_FAULT_L:
        return "故障任务L";
    case FacMotoState_UGT_E_SS_WAIT_M:
        return "等待任务M";
    case FacMotoState_UGT_E_SS_WRITE_FLASH:
        return "写入任务N";
    default:
        return "未知状态";
    }
}

QString MainWindow::getMotorFaultCodeString(FacMotorFaultCode faultCode)
{
    switch (faultCode)
    {
    case FacMotorFaultCode_NoFault:
        return "无异常";
    case FacMotorFaultCode_OverVoltage:
        return "过压";
    case FacMotorFaultCode_UnderVoltage:
        return "欠压";
    case FacMotorFaultCode_OverCurrent:
        return "软件过流";
    case FacMotorFaultCode_HardOverCurrent:
        return "硬件过流";
    case FacMotorFaultCode_OverTempMotor:
        return "电机过温";
    case FacMotorFaultCode_Abnormal_working_current:
        return "工作状态电流达到饱和";
    case FacMotorFaultCode_OverTempIGBT:
        return "IPM过温";
    case FacMotorFaultCode_StartupFail:
        return "启动失败";
    case FacMotorFaultCode_StartupFailContious:
        return "连续启动失败";
    case FacMotorFaultCode_OverLoad:
        return "软件过载，降额运行";
    case FacMotorFaultCode_OverWindSpeed:
        return "超速";
    case FacMotorFaultCode_LosePhase:
        return "丢相";
    case FacMotorFaultCode_OverIAs:
        return "A相过流";
    case FacMotorFaultCode_OverIBs:
        return "B相过流";
    case FacMotorFaultCode_OverICs:
        return "C相过流";
    case FacMotorFaultCode_OverIAsIBsICs:
        return "全相过流";
    case FacMotorFaultCode_STOPMode:
        return "停止模式";
    case FacMotorFaultCode_MotorStall:
        return "堵转";
    default:
        return "未知错误码";
    }
}

QString MainWindow::getCaliMarkString(CaliMark caliMark)
{
    switch (caliMark)
    {
    case CaliMark_zer_hallzer_comple_flag:
        return "零点霍尔校准完成标志";
    case CaliMark_us_zer_comple_flag:
        return "零点电压校准完成标志";
    case CaliMark_hallzer_comple_flag:
        return "霍尔校准完成标志";
    case CaliMark_zer_hallzer_incomple_flag:
        return "零点霍尔校准未完成标志";
    default:
        return "未知校准标志";
    }
}
void MainWindow::get_servo_motor_info_msg(FacMotorCalibResult data)
{
    // ui->msgEdit->appendPlainText("伺服电机校准标志为"+QString::number(data.value_item.servo_info.motor_cali_mark));
    // ui->msgEdit->appendPlainText("伺服电机电流为"+QString::number(data.value_item.servo_info.motor_current));
    // ui->msgEdit->appendPlainText("伺服电机工作状态为"+QString::number(data.value_item.servo_info.motor_state));
    // ui->msgEdit->appendPlainText("伺服电机电压为"+QString::number(data.value_item.servo_info.motor_voltage));
    // ui->msgEdit->appendPlainText("伺服电机错误码为"+QString::number(data.value_item.servo_info.FaultCode));

    ui->msgEdit->appendPlainText(
        "伺服电机校准标志：" +
        getCaliMarkString(static_cast<CaliMark>(data.value_item.servo_info.motor_cali_mark)));
    ui->msgEdit->appendPlainText("伺服电机电流：" +
                                 QString::number(data.value_item.servo_info.motor_current));
    ui->msgEdit->appendPlainText(
        "伺服电机工作状态：" +
        getMotorStateString(static_cast<FacMotoState>(data.value_item.servo_info.motor_state)));
    ui->msgEdit->appendPlainText("伺服电机电压：" +
                                 QString::number(data.value_item.servo_info.motor_voltage));
    ui->msgEdit->appendPlainText("伺服电机错误码：" +
                                 getMotorFaultCodeString(static_cast<FacMotorFaultCode>(
                                     data.value_item.servo_info.FaultCode)));
}
void MainWindow::updateHIDComboBox(QComboBox *comboBox)
{
    if (!comboBox)
        return;

    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];

    // 获取当前的项目列表
    QSet<QString> currentItems;
    for (int i = 0; i < comboBox->count(); ++i)
    {
        currentItems.insert(comboBox->itemText(i));
    }

    QProcess process;
    // Windows系统上使用 "wmic" 命令获取带有 USB 字符串的设备列表
    process.start("wmic path Win32_PnPEntity where \"Name like '%USB%'\" get DeviceID,Description");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    QTextStream stream(&output);
    QString line;
    QStringList newDevices;   // 存储新的设备列表
    int k = 100;
    while (stream.readLineInto(&line))
    {
        if (line.contains("DeviceID"))
            continue;   // 跳过标题行
        QStringList parts = line.split(QRegExp("\\s{2,}"), Qt::SkipEmptyParts);
        if (parts.size() >= 2)
        {
            QString device = parts[1].trimmed();
            if (device.contains("VID_0471"))
            {
                icdev = dc_init(k, 115200);
                st = dc_srd_eeprom(icdev, 0, 8, buff_1);
                if (st != 0)
                {
                    qDebug() << "nfc烧录器读取失败";
                }
                else
                {
                    qDebug() << "nfc烧录器读取成功";
                    QString buffStr =
                        QString::fromLatin1(reinterpret_cast<const char *>(buff_1), 8);
                    qDebug() << "nfc设备为:" << buffStr;
                    newDevices << QString("%1:%2").arg(k).arg(buffStr);
                }
                ++k;
            }
        }
    }

    // 移除不再存在的设备
    for (int i = 0; i < comboBox->count(); ++i)
    {
        QString currentItem = comboBox->itemText(i);
        if (!newDevices.contains(currentItem))
        {
            comboBox->removeItem(i);
            currentItems.remove(currentItem);
            --i;   // 因为移除了一个项，所以要调整索引
        }
    }

    // 添加新的设备
    for (const QString &newDevice : newDevices)
    {
        if (!currentItems.contains(newDevice))
        {
            comboBox->addItem(newDevice);
            currentItems.insert(newDevice);
        }
    }

    qDebug() << "设备个数" << currentItems.size();
}
void MainWindow::processTheDatagram(QByteArray &datagram)
{
    if (ui->quicklz_picture->isChecked())
    {
        // 调用解压缩函数
        uint32_t decompressed_size = quicklz_decompress(
            reinterpret_cast<uint8_t *>(datagram.data()), datagram.size(), my_write_cb, my_read_cb);

        // 检查解压缩结果
        if (decompressed_size > 0)
        {
            qDebug() << "解压缩成功，解压后的数据大小为" << decompressed_size << "字节。";
            ui->msgEdit->appendPlainText("解压缩成功");

            // 读取解压后的数据并处理
            QFile file("output.dat");
            if (file.open(QIODevice::ReadOnly))
            {
                datagram = file.readAll();
                file.close();

                // 处理解压后的数据
                qDebug() << "解压后的数据内容：" << datagram;
            }
            else
            {
                ui->msgEdit->appendPlainText("无法打开解压后的数据文件。");
            }
        }
        else
        {
            ui->msgEdit->appendPlainText("解压缩失败。");
        }
    }

    // qDebug() << "Received datagram with length: " << datagram.size();

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
    // // 绘制图像和矩形
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

    QPen pen(Qt::red);                                               // 创建一个红色的画笔
    painter.setPen(pen);                                             // 设置画笔颜色
    painter.drawImage(0, 0, image);                                  // 在 pixmap 上绘制图片
    painter.drawRect(Rect1_X, Rect1_Y, Rect1_Width, Rect1_Height);   // 绘制第一个矩形
    // painter.drawRect(Rect2_X, Rect2_Y, Rect2_Width, Rect2_Height);   // 绘制第二个矩形
    viewercamrea->updateImage();   // 更新视图
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
        qDebug() << "写入摄像头数据包长度" << write_len;
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
        if (frame_buf[i] == 0xCC && frame_buf[i + 1] == 0xCC && frame_buf[i + 2] == 0xCC &&
            frame_buf[i + 3] == 0xCC)
        {
            head = (ext_uart_phy_layer_t *)&frame_buf[i];
            if (head->magic == EXT_UART_MAGIC)
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

void MainWindow::updateImageOnMainThread()
{
    processTheDatagram(pictureByteArray);   // 显示图片
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
            // qDebug() << "串口环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }

        ext_uart_phy_layer_t *head = (ext_uart_phy_layer_t *)frame_buf;

        if (head->magic == EXT_UART_MAGIC)
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
// int totalsize = 0;

void MainWindow::readDongleSerialPortData()
{
    dongleSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = dongleSerialPortBuf;   // 读取缓冲区数据

    int write_len = 0;
    int len = dataTemp.size();
    write_len = dongleRingBuf->usmile_ring_buffer_write(
        &p_dongleRingBuffer, reinterpret_cast<uint8_t *>(dataTemp.data()), dataTemp.size());

    // totalsize = dataTemp.size() + totalsize;
    // qDebug() << "totalsize:" << totalsize;

    if (write_len < len)
    {
        qDebug() << "write_len:" << write_len << "len:" << dataTemp.size();
    }

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
    {
        ui->msgEdit->appendPlainText("dongle串口连接成功");
        uartStatusLabel = new QLabel("串口连接：<font color='green'>成功</font>");
    }
    else
    {
        uartStatusLabel = new QLabel("串口连接：<font color='red'>失败</font>");
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

            qDebug() << "数据包：" << ++dataNumber;

            if (frame_size > ring_size)
            {
                qDebug() << "图片帧数据不完整" << "需要" << frame_size << "实际为" << ring_size;
                break;
            }

            qDebug() << "图片帧数据完整" << "需要" << frame_size << "实际为" << ring_size
                     << sizeof(head);

            // 从环形缓冲区中读取整个帧的数据
            cameraRingBuf->usmile_ring_buffer_pick(&p_cameraRingBuffer, frame_picture_buf,
                                                   frame_size);

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
                qDebug() << "crc校验失败" << crc_cali << crc16 << "head->data[0]" << head->data[0]
                         << "head->data[1]" << head->data[1];
                // printSquareData(head->data, head->data_size);
                qDebug() << "图片数据包够了，开始显示" << ring_size;
                QByteArray byteArray(reinterpret_cast<const char *>(head),
                                     head->data_size + PICTURE_PHY_LAYER_HEAD_SIZE);
                pictureByteArray = byteArray;
                emit imageProcessed();
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
        pb->set_forbid_sleep(FacSwitch_OPEN);

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
    if (data.dev_info[0].which_value_item == FacDevInfoValue_board_sn_tag)
    {
        qDebug() << "原始的board_sn：" << data.dev_info[0].value_item.board_sn;
        QString board_sn_string = QString::fromUtf8(data.dev_info[0].value_item.board_sn);
        board_sn->setText("板子sn:" + board_sn_string);
    }

    if (data.dev_info[0].which_value_item == FacDevInfoValue_sub_pid_tag)
    {
        qDebug() << "原始的sub_pid：" << data.dev_info[0].value_item.sub_pid;
        QString sub_pid_string = QString::fromUtf8(data.dev_info[0].value_item.sub_pid);
        sub_pid->setText("sub_pid:" + sub_pid_string);
    }

    if (data.dev_info[0].which_value_item == FacDevInfoValue_tail_sn_tag)
    {
        qDebug() << "原始的tail_sn：" << data.dev_info[0].value_item.tail_sn;
        QString tail_sn_string = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
        tail_sn->setText("尾盖sn:" + tail_sn_string);
    }
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
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    // 在合适的位置定义变量
    QString fileName = "mac_sn.txt";

    if (settings.value("Mes/FACTORY").toString() == "hq")
        fileName = "mac_sn.txt";
    else if (settings.value("Mes/FACTORY").toString() == "lx")
        fileName = "mac_sn.txt";
    else if (settings.value("Mes/FACTORY").toString() == "jj")
        fileName = "mac_sn.txt";
    else
    {
        QString path = "\\\\172.60.1.249\\sgpub\\LTC\\MAC\\mac_sn.txt";
        QFileInfo fileInfo(path);
        // 创建一个文件对象
        if (fileInfo.exists() && fileInfo.isFile())
        {
            fileName = path;
            // file.setFileName(path);
            ui->msgEdit->appendPlainText("云端文件存在，正在打开云端文件...");
        }
        else
        {
            fileName = "mac_sn.txt";
            // file.setFileName("mac_sn.txt");
            ui->msgEdit->appendPlainText("云端文件不存在，正在打开本地文件...");
        }
    }

    // 在需要的地方使用这个变量创建 QFile 对象
    QFile file(fileName);
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
QString MainWindow::getValueBySN(const QString &sn)
{
    QString truncatedSN = sn.left(8);
    ui->msgEdit->appendPlainText("truncatedSN:" + truncatedSN);

    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString value = settings.value("SUBPID/" + truncatedSN, "SUBPID_ERRO").toString();
    ui->msgEdit->appendPlainText("匹配到的subpid：" + value);

    return value;
}

void MainWindow::waitWork(int ms)
{
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

void MainWindow::stopRecording()
{
    ui->msgEdit->appendPlainText("录音结束");
    audioRecorder->stop();
}
void MainWindow::refresh_pb_data(QString data)
{
    ui->msgEdit->appendPlainText(data);
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
void MainWindow::showlog(QString msg)
{
    ui->msgEdit->appendPlainText(msg);
    qDebug() << "mainwindow的" << msg;
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
void MainWindow::saveCustom()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    settings.setValue("Window/Size", this->size());

    QString baseKey = QString("mechine/%1").arg(0);
    // 保存COM口相关信息
    settings.setValue(QString("%1/%2").arg(baseKey).arg("comName"),
                      ui->comNameCombo->currentText());

    qDebug() << "保存内容" << ui->comNameCombo->currentText();
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
    QString baseKey = QString("mechine/%1").arg(0);
    QString comName = settings.value(QString("%1/comName").arg(baseKey)).toString();
    ui->comNameCombo->setCurrentText(comName);
    qDebug() << "配置内容" << comName;
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
    QString ble_ver = "=" + settings.value("ProductInfo/Ble_Ver").toString();

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
                                  {"motor_ver", "电机版本号", motor_ver},
                                  {"ble_ver", "蓝牙版本号", ble_ver}

    };

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
        pb, &Qpb::send_base_data, this,
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
            basicInfoModel->getTestItemByName("camera_id")
                ->setData(QString("%1").arg(baseInfo.camera_version), Qt::DisplayRole);

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
            basicInfoModel->getTestItemByName("ble_ver")->setData(
                QString("%1").arg(baseInfo.ble_version), Qt::DisplayRole);

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
    QObject::connect(pb, &Qpb::send_periph_data, this,
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
