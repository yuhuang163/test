#include "fixture_uart.h"

#include "qdebug.h"
#include "qserialportinfo.h"
#include "ui_fixture_uart.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

Fixture_uart::Fixture_uart(QWidget* parent) :
    QWidget(parent), ui(new Ui::Fixture_uart), fixtureSerialPort(new QSerialPort(this)) {
    ui->setupUi(this);
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }

    connect(this->fixtureSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(FixturehandleError(QSerialPort::SerialPortError)));

    connect(fixtureSerialPort, &QSerialPort::readyRead, this, [=]() {
        fixtureSerialPortTimer->start(10);                          // 设置100毫秒的延时
        fixtureSerialPortBuf.append(fixtureSerialPort->readAll());  // 将读到的数据放入缓冲区
    });

    fixRingBuf = new RingBuf(&p_fixRingBuffer, fix_ring_buffer, 1, sizeof(fix_ring_buffer));

    // 启动后台线程
    future = QtConcurrent::run([this]() {
        while (running.load()) {
            solve_frame();
            QThread::msleep(10);  // 等待10毫秒
        }
    });
    running.store(true);
    FixtureCommandInit();
}

Fixture_uart::~Fixture_uart() {
    running.store(false);
    // 等待线程结束
    future.waitForFinished();
    delete ui;
}

// void Fixture_uart::closeEvent(QCloseEvent *)
// {
//     running.store(false);
//     // 等待线程结束
//     future.waitForFinished();
// }

void Fixture_uart::closeFixtureSerialPort() {
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixturecomNameCombo->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);

    refresh_Fixtureuart_state(0);

    if (fixtureSerialPort->isOpen())
        fixtureSerialPort->close();
    disconnect(fixtureSerialPortTimer, &QTimer::timeout, this,
               &Fixture_uart::readFixtureSerialPortData);  // timeout执行真正的读取操作
}

void Fixture_uart::refresh_Fixtureuart_state(int state) {
    if (state)
        ui->Fixtureuartstate->setText("治具串口连接：<font color='green'>成功</font>");
    else
        ui->Fixtureuartstate->setText("治具串口连接：<font color='red'>失败</font>");
}

void Fixture_uart::on_FixtureconnectButton_clicked() { openFixtureSerialPort(); }

void Fixture_uart::on_FixturedisconnectButton_clicked() {
    closeFixtureSerialPort();
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);
}

void Fixture_uart::on_FixturerefreshCom_clicked() {
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }
}

void Fixture_uart::openFixtureSerialPort(void) {
    // 设置串口名
    fixtureSerialPort->setPortName(ui->FixturecomNameCombo->currentText());
    // 设置波特率
    fixtureSerialPort->setBaudRate(fixBaudRate);  // 飞安瑞是9600治具是115200
    // 设置数据位
    fixtureSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    fixtureSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    fixtureSerialPort->setStopBits(QSerialPort::OneStop);
    fixtureSerialPort->setReadBufferSize(4096);
    // 设置流控制
    fixtureSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制
    if (fixtureSerialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        fixtureSerialPort->setRequestToSend(true);
        // 启用DTR信号
        fixtureSerialPort->setDataTerminalReady(true);
        // showlog("Fixture串口连接成功");
        refresh_Fixtureuart_state(1);
        ui->FixturerefreshCom->setEnabled(false);
        ui->FixturecomNameCombo->setEnabled(false);
        ui->FixtureconnectButton->setEnabled(false);
        fixtureSerialPort->setDataTerminalReady(true);

        connect(fixtureSerialPortTimer, &QTimer::timeout, this,
                &Fixture_uart::readFixtureSerialPortData);  // timeout执行真正的读取操作
    } else {
        QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        //  showlog("打开错误");
    }
}
void Fixture_uart::readFixtureSerialPortData() {
    fixtureSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = fixtureSerialPortBuf;  // 读取缓冲区数据
    fixtureSerialPortBuf.clear();                // 清除缓冲区

    qDebug() << "hyj接收到治具数据" << dataTemp;
    int write_len = 0;
    int len = dataTemp.size();
    write_len = fixRingBuf->usmile_ring_buffer_write(&p_fixRingBuffer, reinterpret_cast<uint8_t*>(dataTemp.data()),
                                                     dataTemp.size());

    if (write_len < len) {
        qDebug() << "write_len:" << write_len << "len:" << dataTemp.size();
    }
    qDebug() << "开始处理 << dataTemp.size()";

    processimuReceivedData(dataTemp);
    save_Fixture_uart_log(0, dataTemp);
}
void Fixture_uart::solve_frame(void) {
    while (true) {
        // 从环形缓冲区中读取帧头
        fixRingBuf->usmile_ring_buffer_pick(&p_fixRingBuffer, frame_buf,
                                            FIX_PHY_LAYER_HEAD_SIZE + FIX_PHY_LAYER_FRAME_SIZE);
        int ring_size = fixRingBuf->usmile_ring_buffer_items_count_get(&p_fixRingBuffer);
        if (ring_size <= FIX_PHY_LAYER_HEADER_ADN_CRC) {
            // qDebug() << "串口环形缓冲区中的数据不足一个完整帧的大小" << ring_size;
            break;
        }

        ext_uart_phy_layer_t* head = (ext_uart_phy_layer_t*)frame_buf;

        if (head->magic == EXT_UART_PHY_LAYER_MAGIC) {
            int frame_size = head->length;

            if (frame_size > ring_size) {
                // qDebug() << "串口帧数据不完整"
                //          << "需要" << frame_size << "实际为" << ring_size;
                break;
            }

            // 从环形缓冲区中读取整个帧的数据
            fixRingBuf->usmile_ring_buffer_pick(&p_fixRingBuffer, frame_buf, frame_size);

            if (frame_buf[head->length - 1] == 0xAA) {
                // 创建 QByteArray 并将结构体内容拷贝进去
                QByteArray buffer(reinterpret_cast<const char*>(head), frame_size);
                if (head->length == 0x05) {
                    processshortdData(buffer);
                } else {
                    processReceivedData(buffer);
                }

            } else {
                qDebug() << "head content:" << QByteArray(reinterpret_cast<char*>(head), head->length * 2).toHex();
                qDebug() << "尾巴校验失败" << frame_buf[head->length - 1] << head->end << head->chargingCurrent
                         << head->magic;
            }

            // 删除已经处理的帧数据
            fixRingBuf->usmile_ring_buffer_delete(&p_fixRingBuffer, frame_size);
        } else {
            qDebug() << "串口数据流错误寻找下一帧";
            qDebug() << "串口数据包头为:"
                     << QByteArray(reinterpret_cast<char*>(frame_buf), FIX_PHY_LAYER_HEAD_SIZE).toHex();

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
int Fixture_uart::ext_ble_find_next_frame(void) {
    int i = 0;
    ext_uart_phy_layer_t* head = NULL;
    int len = fixRingBuf->usmile_ring_buffer_pick(&p_fixRingBuffer, frame_buf, 2 * 1024);
    // qDebug() << "查找下一帧，剩下：" << len;

    for (i = 0; i < len; i++) {
        if (frame_buf[i] == 0x55) {
            head = (ext_uart_phy_layer_t*)&frame_buf[i];
            if (head->magic == EXT_UART_PHY_LAYER_MAGIC) {
                qDebug() << "匹配到了串口数据包头";
                fixRingBuf->usmile_ring_buffer_delete(&p_fixRingBuffer, i);
                return 1;
            }
        }
    }

    fixRingBuf->usmile_ring_buffer_delete(&p_fixRingBuffer, len);

    return 0;
}

// 1是发送的，0是接收的
void Fixture_uart::save_Fixture_uart_log(int txrx, QByteArray data) {
    QString folderName = "所有log/治具log";
    QDir dir;

    // 检查并创建目录
    if (!dir.exists(folderName)) {
        if (!dir.mkpath(folderName)) {
            qDebug() << "无法创建目录:" << folderName;
            return;
        }
    }
    // 获取当前时间并格式化为字符串
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd");

    // 生成文件路径
    QString fileName = "治具日志" + timestamp + ".log";
    QString filePath = dir.filePath(folderName + "/" + fileName);

    QFile logFile(filePath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "写入成功治具日志";
        // 写入数据
        QTextStream out(&logFile);
        out.setCodec("UTF-8");  // 设置编码格式为UTF-8
        // 获取当前时间的详细时间戳
        QString detailedTimestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");

        // 将数据转换为16进制格式
        QString hexData = data.toHex(' ').toUpper();

        if (txrx)  // 发送的
        {
            out << detailedTimestamp << tr("- tx发送的原始数据为：") << data << "\n";
            out << detailedTimestamp << tr("- tx发送的16进制数据：") << hexData << "\n";
        } else {
            out << detailedTimestamp << tr("- rx接收的原始数据为：") << data << "\n";
            out << detailedTimestamp << tr("- rx接收的16进制数据：") << hexData << "\n";
        }

        logFile.close();
    } else {
        qDebug() << "无法打开治具日志文件：" << fileName;
    }
}
void Fixture_uart::processshortdData(const QByteArray& data) {
    FixturePacketData datapack;
    if (data.size() == 5 && data.at(3) == -52)  // cc  01
    {
        datapack.machineNumber = data.at(1);
        datapack.sleep = 1;
        emit send_data_to_mechine_sleep(datapack);
        qDebug() << "开始休眠";
    }

    qDebug() << "data.at(0)" << data.at(0) << data.size();
    if (data.size() == 5 && data.at(1) == -86 && data.at(3) == -86)  // AA  AA
    {
        qDebug() << "开始测试";

        emit send_data_to_mechine_start();
    }
}
void Fixture_uart::processimuReceivedData(const QByteArray& data) {
    QString data_command = data;

    if (data_command.contains("OK")) {
        emit send_data_to_mechine_imu(1);
        qDebug() << "收到治具ok指令";
    }

    if (data_command.contains("ERROR")) {
        emit send_data_to_mechine_imu(0);
    }

    if (data_command.contains("BarCode?\r")) {
#if Y20_Pro_PRESS_TEST
        send_command_to_machine(COMMAND_ID_CHEAK_STATUS, 0);
#endif
        emit send_data_to_mechine_press(1);
    }

    if (data_command.contains("Ready\r")) {
        emit send_data_to_mechine_press(2);
    }

    if (data_command.contains("START1_OK\r\n")) {
        emit send_data_to_mechine_press(3);
    }

    if (data_command.contains("START2_OK\r\n")) {
        emit send_data_to_mechine_press(4);
    }
    data_command.clear();
}

void Fixture_uart::processReceivedData(const QByteArray& data) {
    QByteArray receivebuf = data;
    if (receivebuf.size() > receivebuf.at(2)) {
        qDebug() << "收到的pcba治具数据大于" << receivebuf.at(2);
        for (int i = 0; i < receivebuf.size(); i++) {
            if (static_cast<int>(receivebuf.at(i)) == 85) {
                uchar length = receivebuf.at(2);
                qDebug() << "i====" << i;
                receivebuf = receivebuf.remove(0, i);
                receivebuf = receivebuf.remove(length, receivebuf.size());
                qDebug() << "截取包头到包尾内容";
            }
        }
        qDebug() << "receivebuf.size()====" << receivebuf.size();
        if (receivebuf.size() > receivebuf.at(2)) {
            receivebuf.clear();
            return;
        }
    }

    qDebug() << "开始打包治具数据包";
    FixturePacketData datapack;  // 发送出去的数据包
    qDebug() << "数值为" << receivebuf.toHex();
    qDebug() << "数值为" << receivebuf.size();
    qDebug() << "数值为" << static_cast<int>(receivebuf.at(0))
             << static_cast<int>(receivebuf.at(receivebuf.size() - 1));

    // 最小长度检查
    if (receivebuf.size() < 3) {
        qDebug() << "接收到的数据包长度不足";
        return;
    }

    // 获取数据包的长度
    uchar length = receivebuf.at(2);
    qDebug() << "length为：" << length;

    // 验证数据包的实际长度
    if (receivebuf.size() < static_cast<int>(length)) {
        qDebug() << "接收到的数据包长度不足";
        return;
    }

    if (receivebuf.size() > static_cast<int>(length)) {
        qDebug() << "实际治具串口的长度大于包里的长度说明";
        if (static_cast<int>(receivebuf.at(length)) != -86)  // 末尾为aa
        {
            qDebug() << "数据有效";
            // receivebuf=data.at(2);
        } else {
            qDebug() << "治具数据无效";
            return;
        }
    }

    // 验证数据包的起始和结束标志
    if (static_cast<int>(receivebuf.at(0)) != 85 || static_cast<int>(receivebuf.at(receivebuf.size() - 1)) != -86) {
        qDebug() << "接收到的数据包开头或结尾格式不正确";
        return;
    }

    datapack.machineNumber = receivebuf.at(1);
    datapack.staticCurrent = (static_cast<uint8_t>(receivebuf.at(3)) << 8) | static_cast<uint8_t>(receivebuf.at(4));
    datapack.workingCurrent = (static_cast<uint8_t>(receivebuf.at(5)) << 8) | static_cast<uint8_t>(receivebuf.at(6));
    datapack.overVoltageLight = receivebuf.at(7);
    datapack.button1 = receivebuf.at(8);
    datapack.button2 = receivebuf.at(9);

    // uint chargingCurrent;
    datapack.chargingCurrent = (static_cast<uint8_t>(receivebuf.at(10)) << 8) | static_cast<uint8_t>(receivebuf.at(11));

    if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool()) {
        datapack.musicCurrent =
            (static_cast<uint8_t>(receivebuf.at(12)) << 8) | static_cast<uint8_t>(receivebuf.at(13));
    } else {
        datapack.music_state = receivebuf.at(12);
    }
    if (SETTINGS.value("SYSTEM/TestShippingCurrent").toBool()) {
        datapack.shipCurrent = (static_cast<uint8_t>(receivebuf.at(14)) << 8) | static_cast<uint8_t>(receivebuf.at(15));
    }

    // 在这里使用提取出的字段进行后续处理
    qDebug() << "机号:" << datapack.machineNumber;
    qDebug() << "静态电流值:" << datapack.staticCurrent << "ua";
    qDebug() << "工作电流:" << datapack.workingCurrent << "ma";
    qDebug() << "过压灯正常:" << datapack.overVoltageLight;
    qDebug() << "按键1:" << datapack.button1;
    qDebug() << "按键2:" << datapack.button2;
    qDebug() << "充电电流:" << datapack.chargingCurrent << "ma";

    if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool()) {
        qDebug() << "音频电流:" << datapack.musicCurrent;
    } else {
        qDebug() << "音频情况:" << datapack.music_state;
    }

    emit send_data_to_mechine(datapack);

    receivebuf.clear();
}

// 欣旺达的六轴治具通信
void Fixture_uart::sendimuData(imuFixtureState fixstate) {
    if (!fixtureSerialPort->isOpen()) {
        qDebug() << "带页面的治具串口未打开，无法发送数据";

        // QMessageBox::warning(NULL, "警告", " 未打开串口\t\r\n  无法发送数据！\r\n");
        return;
    }

    QByteArray dataToSend;

    switch (fixstate) {
        case STATE_START: dataToSend = QByteArray("START"); break;
        case STATE_END: dataToSend = QByteArray("END"); break;
        case STATE_RESET: dataToSend = QByteArray("RESET"); break;
        case STATE_RETURN: dataToSend = QByteArray("S0180"); break;
        case STATE_40: dataToSend = QByteArray("S0040"); break;
        case STATE_FU40: dataToSend = QByteArray("S1040"); break;
        case STATE_BRUSH_UP: dataToSend = QByteArray("B0000"); break;
        case STATE_BRUSH_DOWN: dataToSend = QByteArray("B0180"); break;
        case STATE_BRUSH_LEFT: dataToSend = QByteArray("B0090"); break;
        case STATE_BRUSH_RIGHT: dataToSend = QByteArray("B0270"); break;
        case STATE_HOME: dataToSend = QByteArray("HOME"); break;
        default: break;
    }

    if (!dataToSend.isEmpty()) {
        fixtureSerialPort->write(dataToSend);
        save_Fixture_uart_log(1, dataToSend);
        start_fix_action(1);
    }
}
void Fixture_uart::set_camera_action(camreaFixtureState fixstate) {
    if (!fixtureSerialPort->isOpen()) {
        qDebug() << "带页面的治具串口未打开，无法发送数据";

        // QMessageBox::warning(NULL, "警告", " 未打开串口\t\r\n  无法发送数据！\r\n");
        return;
    }

    QByteArray dataToSend;

    switch (fixstate) {
        case STATE_THOROUGHFARE1_IN: dataToSend = QByteArray("CTL_IN1\r\n"); break;
        case STATE_THOROUGHFARE1_OUT: dataToSend = QByteArray("CTL_OUT1\r\n"); break;
        case STATE_THOROUGHFARE2_IN: dataToSend = QByteArray("CTL_IN2\r\n"); break;
        case STATE_THOROUGHFARE2_OUT: dataToSend = QByteArray("CTL_OUT2\r\n"); break;
        case STATE_THOROUGHFARE3_IN: dataToSend = QByteArray("CTL_IN3\r\n"); break;
        case STATE_THOROUGHFARE3_OUT: dataToSend = QByteArray("CTL_OUT3\r\n"); break;

        default: break;
    }

    if (!dataToSend.isEmpty()) {
        fixtureSerialPort->write(dataToSend);
        save_Fixture_uart_log(1, dataToSend);
        start_fix_action(1);
    }
}

void Fixture_uart::sendFixtureData(FixtureState fixstate) {
    if (!fixtureSerialPort->isOpen()) {
        QMessageBox::warning(NULL, "警告", " 未打开治具串口\t\r\n  无法发送数据！\r\n");
        return;
    }

    switch (fixstate) {
        case STATE_CYLINDER_OPEN:
            fixtureSerialPort->write(QByteArray::fromHex("5501050001"));  // 气缸1动作
            break;
        case STATE_RELAY1_OPEN:
            fixtureSerialPort->write(QByteArray::fromHex("5501010001"));  // 继电器1吸合
            break;
        case STATE_RELAY1_RESET:
            fixtureSerialPort->write(QByteArray::fromHex("5501010000"));  // 继电器1复位
            break;
        case STATE_CYLINDER_RESET:
            fixtureSerialPort->write(QByteArray::fromHex("5501050000"));  // 气缸1复位
            break;
        case STATE_RELAY4_OPEN:
            fixtureSerialPort->write(QByteArray::fromHex("5501040001"));  // 继电器4吸合
            break;
        case STATE_RELAY4_RESET: fixtureSerialPort->write(QByteArray::fromHex("5501040000"));  // 继电器4复位
    }
}

void Fixture_uart::send_start_command(int i) {
    const char* commands[] = {"AA01", "AA02", "AA03", "AA04", "AA05", "AA06", "AA07", "AA08", "AA09", "AA0A", "A1"};

    if (i > 0 && i <= 10) {
        fixtureSerialPort->write(QByteArray::fromHex(commands[i - 1]));
        qDebug() << "已发送开始命令" << commands[i - 1];
        save_Fixture_uart_log(1, QByteArray::fromHex(commands[i - 1]));
    } else {
        qDebug() << "Invalid command index";
    }
}
void Fixture_uart::send_start_sleep_command(int i) {
    const char* commands[] = {"CC01", "CC02", "CC03", "CC04", "CC05", "CC06", "CC07", "CC08", "CC09", "CC0A", "A1"};

    if (i > 0 && i <= 10) {
        fixtureSerialPort->write(QByteArray::fromHex(commands[i - 1]));
        save_Fixture_uart_log(1, QByteArray::fromHex(commands[i - 1]));

        qDebug() << "已发送开始休眠命令" << commands[i - 1];
    } else {
        qDebug() << "Invalid command index";
    }
}
void Fixture_uart::send_start_white_modle_command(int i) {
    const char* commands[] = {"EE01", "EE02", "EE03", "EE04", "EE05", "EE06", "EE07", "EE08", "EE09", "EE0A", "A1"};

    if (i > 0 && i <= 10) {
        fixtureSerialPort->write(QByteArray::fromHex(commands[i - 1]));
        save_Fixture_uart_log(1, QByteArray::fromHex(commands[i - 1]));

        qDebug() << "已发送已经进入亮白模式命令" << commands[i - 1];
    } else {
        qDebug() << "Invalid command index";
    }
}

void Fixture_uart::FixturehandleError(QSerialPort::SerialPortError error) {
    qDebug() << "串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        if (fixtureSerialPort->isOpen())
            fixtureSerialPort->close();

        disconnect(fixtureSerialPortTimer, &QTimer::timeout, this,
                   &Fixture_uart::readFixtureSerialPortData);  // timeout执行真正的读取操作

        ui->FixturerefreshCom->setEnabled(true);
        ui->FixturecomNameCombo->setEnabled(true);
        ui->FixtureconnectButton->setEnabled(true);
        QMessageBox::warning(NULL, "警告", " 治具串口被拔出！\t\r\n");
        ui->Fixtureuartstate->setText("串口连接：<font color='red'>拔出</font>");
    }
}

void Fixture_uart::FixtureCommandInit(void) {
#if 0
    // Y20Pro校准治具
    commands[COMMAND_ID_TRAY_IN][0] = {"TARY_I\r\n"};
    commands[COMMAND_ID_TRAY_OUT][0] = {"TARY_O\r\n"};
    commands[COMMAND_ID_FIXED_BLOCK_UP][0] = {"PROD_U\r\n"};
    commands[COMMAND_ID_FIXED_BLOCK_DOWN][0] = {"PROD_D\r\n"};
    commands[COMMAND_ID_KEY_UP][0] = {"FAMA_U\r\n"};
    commands[COMMAND_ID_KEY_DOWN_200][0] = {"FAMA_D\r\n"};
    commands[COMMAND_ID_BTH_UP][0] = {"HEAD_U\r\n"};
    commands[COMMAND_ID_BTH_DOWN_200][0] = {"HEAD_D\r\n"};
#else
    // Y20Pro校准治具
    commands[COMMAND_ID_TRAY_IN][0] = {"TARY_I"};
    commands[COMMAND_ID_TRAY_OUT][0] = {"TARY_O"};
    commands[COMMAND_ID_FIXED_BLOCK_UP][0] = {"PROD_U"};
    commands[COMMAND_ID_FIXED_BLOCK_DOWN][0] = {"PROD_D"};
    commands[COMMAND_ID_KEY_UP][0] = {"FAMA_U"};
    commands[COMMAND_ID_KEY_DOWN_200][0] = {"FAMA_D"};
    commands[COMMAND_ID_BTH_UP][0] = {"HEAD_U"};
    commands[COMMAND_ID_BTH_DOWN_200][0] = {"HEAD_D"};
#endif

    // Y20Pro测试治具
    commands[COMMAND_ID_CHEAK_STATUS][0] = {"Check:Pass\r"};
    commands[COMMAND_ID_RESULT][0] = {"End:1,1,1\r"};
    commands[COMMAND_ID_RESULT][1] = {"End:1,1,1,1\r"};
    for (int i = 0; i < 4; i++) {
        QString command1 = QString("Set:%1H,500\r").arg(i + 1);
        QString command2 = QString("Set:%1H:3500\r").arg(i + 1);
        QString command3 = QString("Set:%1H:4500\r").arg(i + 1);
        QString command4 = QString("Rst:%1\r").arg(i + 1);
        QString command5 = QString("Set:%1B:2300\r").arg(i + 1);
        QString command6 = QString("Rst:%1\r").arg(i + 1);

        commands[COMMAND_ID_BTH_PRESS_50][i] = strdup(command1.toStdString().c_str());
        commands[COMMAND_ID_BTH_PRESS_350][i] = strdup(command2.toStdString().c_str());
        commands[COMMAND_ID_BTH_PRESS_450][i] = strdup(command3.toStdString().c_str());
        commands[COMMAND_ID_BTH_PRESS_UP][i] = strdup(command4.toStdString().c_str());
        commands[COMMAND_ID_KEY_PRESS_230][i] = strdup(command5.toStdString().c_str());
        commands[COMMAND_ID_KEY_PRESS_UP][i] = strdup(command6.toStdString().c_str());

        qDebug() << "commmmmmmm:" << commands[COMMAND_ID_BTH_PRESS_50][i];
    }

// F20校准测试治具
#if 0  // F20项目取消
    commands[COMMAND_ID_F20_FIXED][0] =         {"C"};  // 手柄夹具下压
    commands[COMMAND_ID_F20_UNFIXED][0] =       {"O"};  // 手柄夹具上升
    commands[COMMAND_ID_KEY_DOWN][0] =          {"F"};  // 按键下压
    commands[COMMAND_ID_KEY_UP][0] =            {"G"};  // 按键上升
    commands[COMMAND_ID_KEY_SWITCH_MODE][0] =   {"W"};  // 切换按键
    commands[COMMAND_ID_KEY_SWITCH_POWER][0] =  {"M"};  // 切换按键
#endif

    // U7、P30P校准测试治具
    commands[COMMAND_ID_FAMA_100_O][0] = {"FAMA1_100G_O\r\n"};
    commands[COMMAND_ID_FAMA_100_C][0] = {"FAMA1_100G_C\r\n"};
    commands[COMMAND_ID_FAMA_300_O][0] = {"FAMA1_300G_O\r\n"};
    commands[COMMAND_ID_FAMA_300_C][0] = {"FAMA1_300G_C\r\n"};
    commands[COMMAND_ID_FAMA_100_O][1] = {"FAMA2_100G_O\r\n"};
    commands[COMMAND_ID_FAMA_100_C][1] = {"FAMA2_100G_C\r\n"};
    commands[COMMAND_ID_FAMA_300_O][1] = {"FAMA2_300G_O\r\n"};
    commands[COMMAND_ID_FAMA_300_C][1] = {"FAMA2_300G_C\r\n"};
}

void Fixture_uart::delay_msec(unsigned int msec) {
    QEventLoop loop;                                // 定义一个新的事件循环
    QTimer::singleShot(msec, &loop, SLOT(quit()));  // 创建单次定时器，槽函数为事件循环的退出函数
    loop.exec();  // 事件循环开始执行，程序会卡在这里，直到定时时间到，本循环被退出
}

void Fixture_uart::send_command_to_machine(int command_id, int numb) {
    if (!fixtureSerialPort->isOpen()) {
        QMessageBox::warning(NULL, "警告", " 未打开串口\t\r\n  无法发送数据！\r\n");
        return;
    }

    // 每帧数据包之间添加时间间隔
    qint64 current_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    qDebug() << "last" << last_sent_timestamp << "current" << current_timestamp;
    if (current_timestamp - last_sent_timestamp < 100 && last_sent_timestamp != 0) {
        qDebug() << "short time,current_time - last:" << current_timestamp - last_sent_timestamp;
        delay_msec(100);
    }
    last_sent_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();


    last_commid_timestamp = last_sent_timestamp;

    if (command_id >= COMMAND_ID_INVALID && command_id < COMMAND_ID_MAX && numb >= 0 && numb < 4) {
        qDebug() << "command_id:" << command_id << "numb" << numb;
        fixtureSerialPort->write(commands[command_id][numb]);
        qDebug() << "已发送命令" << commands[command_id][numb];
        save_Fixture_uart_log(1, commands[command_id][numb]);
    } else {
        qDebug() << "Invalid command index";
    }
}
