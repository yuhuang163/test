#include "test_base.h"
#include "qcoreapplication.h"
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
test_base::test_base()
    : log(new Qlog), dongleSerialPort(new QSerialPort(this)), pb(new Qpb(dongleSerialPort)),
      at(new Qat(dongleSerialPort)), usbSerialPort(new QSerialPort(this)),
      usb(new Qusb(usbSerialPort)), jigSerialPort(new QSerialPort(this)),
      jig(new Qjig(jigSerialPort)), productSerialPort(new QSerialPort(this)),
      product(new Qproduct(productSerialPort))
{
    signalAndslot();
    scanSerialPortsTimer->start(1000);   // 每秒刷新一次
    initData();
}
void test_base::initData()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    pack.factory = settings.value("Mes/FACTORY").toString();
    pack.Employee_ID = settings.value("Mes/mUserno").toString();
    pack.action = settings.value("Mes/Action").toString();
    pack.machineNo = settings.value("Mes/machineNo").toString();
    pack.product = settings.value("Mes/Product_Name").toString();
    pack.line = settings.value("Mes/Line").toString();
    pack.model = settings.value("Mes/model").toString();
    pack.test_station = settings.value("Mes/test_station").toString();
    pack.password = settings.value("Mes/M_PASSWORD").toString();
    pack.userNo = settings.value("Mes/M_USERNO").toString();
    pack.error = "NULL";

    snPattern = settings.value("Regex/SNPattern", "^[0-9a-zA-Z]{18}$").toString();


}
void test_base::signalAndslot()
{
    qDebug() << "test的信号绑定";
    connect(at, SIGNAL(send_ble_state(int)), this, SLOT(refresh_ble_state(int)));
    connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refresh_ble_rssi(QString)));
    connect(at, SIGNAL(sendwifimsg(QString)), this, SLOT(get_wifi_msg(QString)));
    connect(at, SIGNAL(send_dongle_ver(QString)), this, SLOT(get_dongle_ver(QString)));
    connect(at, SIGNAL(send_dongle_wifi(QString)), this, SLOT(get_dongle_wifi(QString)));

    connect(pb, SIGNAL(sendGetBrushResponse(int)), this, SLOT(solveGetBrushResponse(int)));

    connect(pb, SIGNAL(send_button_state(FacButtonState)), this, SLOT(checkbutton(FacButtonState)));
    connect(pb, SIGNAL(send_BrushControl_state(FacBrushControl)), this,
            SLOT(check_BrushControl_state(FacBrushControl)));
    connect(pb, SIGNAL(send_LED_CONTROL_state(FacLedControl)), this,
            SLOT(check_LED_CONTROL_state(FacLedControl)));
    connect(pb, SIGNAL(send_camera_CONTROL_state(FacCameraControl)), this,
            SLOT(refresh_camera_CONTROL(FacCameraControl)));
    connect(pb, SIGNAL(imuDataReady(FacUploadNineAlex)), this, SLOT(getimuData(FacUploadNineAlex)));
    connect(pb, SIGNAL(send_IMU_CALIB_result(FacImuCalibResult)), this,
            SLOT(update_IMU_CALIB_result(FacImuCalibResult)));
    connect(pb, SIGNAL(send_pb_date(QString)), this, SLOT(refresh_pb_data(QString)));
    connect(pb, SIGNAL(send_motor_cali_msg(QString)), this, SLOT(refresh_motor_cali_msg(QString)));
    connect(pb, SIGNAL(periphStateReady(FacGetPeriphState)), this,
            SLOT(refresh_periph_data(FacGetPeriphState)));
    connect(pb, SIGNAL(send_Lcd_CONTROL_state(FacLcdControl)), this,
            SLOT(refresh_Lcd_CONTROL(FacLcdControl)));
    connect(pb, SIGNAL(baseInfoReady(FacGetDevBaseInfo)), this,
            SLOT(refresh_base_data(FacGetDevBaseInfo)));
    connect(pb, SIGNAL(send_battary(FacDevInfo)), this, SLOT(refresh_battary_data(FacDevInfo)));
    connect(pb, SIGNAL(send_sn_data(FacDevInfo)), this, SLOT(refresh_sn(FacDevInfo)));

    connect(usb, SIGNAL(send_ammeter_data(QString)), this, SLOT(refresh_ammeter_data(QString)));


    connect(scanSerialPortsTimer, SIGNAL(timeout()), this, SLOT(scanSerialPorts()));
    connect(this->dongleSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleDongleSerialPortError(QSerialPort::SerialPortError)));
    connect(this->usbSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleUsbSerialPortError(QSerialPort::SerialPortError)));
    connect(this->jigSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleJigSerialPortError(QSerialPort::SerialPortError)));
    connect(this->productSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleProductSerialPortError(QSerialPort::SerialPortError)));

    connect(this, SIGNAL(refreshDongleSerialPortState(int)), this,
            SLOT(refresh_dongle_uart_state(int)));
    connect(this, SIGNAL(refreshUsbSerialPortState(int)), this, SLOT(refresh_usb_uart_state(int)));
    connect(this, SIGNAL(refreshJigSerialPortState(int)), this, SLOT(refresh_jig_uart_state(int)));
    connect(this, SIGNAL(refreshProductSerialPortState(int)), this,
            SLOT(refresh_product_uart_state(int)));

    connect(dongleSerialPort, &QSerialPort::readyRead, this,
            [=]()
            {
                dongleSerialPortTimer->start(10);   // 设置100毫秒的延时
                dongleSerialPortBuf.append(dongleSerialPort->readAll());   // 将读到的数据放入缓冲区
            });

    connect(usbSerialPort, &QSerialPort::readyRead, this,
            [=]()
            {
                usbSerialPortTimer->start(10);                       // 设置100毫秒的延时
                usbSerialPortBuf.append(usbSerialPort->readAll());   // 将读到的数据放入缓冲区
            });
    connect(jigSerialPort, &QSerialPort::readyRead, this,
            [=]()
            {
                jigSerialPortTimer->start(10);                       // 设置100毫秒的延时
                jigSerialPortBuf.append(jigSerialPort->readAll());   // 将读到的数据放入缓冲区
            });
    connect(productSerialPort, &QSerialPort::readyRead, this,
            [=]()
            {
                productSerialPortTimer->start(10);   // 设置100毫秒的延时
                productSerialPortBuf.append(
                    productSerialPort->readAll());   // 将读到的数据放入缓冲区
            });
}

void test_base::scanSerialPorts()
{
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    auto updateComboBox = [](QComboBox *comboBox, const QList<QSerialPortInfo> &ports)
    {
        if (!comboBox)
        {
            return;
        }

        // 获取当前的项目列表
        QSet<QString> currentItems;
        for (int i = 0; i < comboBox->count(); ++i)
        {
            currentItems.insert(comboBox->itemText(i));
        }
        // 添加新的项目
        for (const QSerialPortInfo &info : ports)
        {
            if (!currentItems.contains(info.portName()))
            {
                comboBox->addItem(info.portName());
            }
            currentItems.remove(info.portName());   // 移除已存在的项目
        }

        // 移除不存在的项目
        for (const QString &item : currentItems)
        {
            int index = comboBox->findText(item);
            if (index != -1)
            {
                comboBox->removeItem(index);
            }
        }
    };

    updateComboBox(getComNameCombo(), ports);
    updateComboBox(getUsbcomNameCombo(), ports);
    updateComboBox(getJigcomNameCombo(), ports);
    updateComboBox(getProductcomNameCombo(), ports);
}
void test_base::readDongleSerialPortData()
{
    dongleSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = dongleSerialPortBuf;   // 读取缓冲区数据

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    pb->parseCmd(dataTemp);
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));
    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logEntry = QString("[%1] %2").arg(timestamp, dataTemp);
    // 将最终字符串追加到日志编辑器中
    logEdit()->appendPlainText(logEntry);

    dongleSerialPortBuf.clear();   // 清除缓冲区
}
void test_base::handleDongleSerialPortError(QSerialPort::SerialPortError error)
{
    qDebug() << "DongleSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
    {
        closeDongleSerialPort();
        // QMessageBox::warning(NULL, "警告", " Dongle串口连接断开！\t\r\n");

        // ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}

void test_base::openDongleSerialPort()
{
    if (dongleSerialPort->isOpen())
    {
        disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readDongleSerialPortData);   // timeout执行真正的读取操作
        dongleSerialPort->close();
    }

    // 设置串口名
    dongleSerialPort->setPortName(getComNameCombo()->currentText());
    // 设置波特率
    dongleSerialPort->setBaudRate(dongleBaudRate);
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
        // 启用RTS信号
        dongleSerialPort->setRequestToSend(false);
        // 启用DTR信号
        dongleSerialPort->setDataTerminalReady(false);
        // 启用RTS信号
        dongleSerialPort->setRequestToSend(true);
        // 启用DTR信号
        dongleSerialPort->setDataTerminalReady(true);

        // ui->msgEdit->appendPlainText("串口连接成功");
        emit refreshDongleSerialPortState(1);

        connect(dongleSerialPortTimer, &QTimer::timeout, this,
                &test_base::readDongleSerialPortData);   // timeout执行真正的读取操作
    }
    else
    {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // ui->msgEdit->appendPlainText("打开错误");
    }
}

void test_base::closeDongleSerialPort()
{
    // // 启用RTS信号
    // dongleSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // dongleSerialPort->setDataTerminalReady(false);
    if (dongleSerialPort->isOpen())
        dongleSerialPort->close();
    disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
               &test_base::readDongleSerialPortData);   // timeout执行真正的读取操作

    emit refreshDongleSerialPortState(0);
}

void test_base::readUsbSerialPortData()
{
    usbSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = usbSerialPortBuf;   // 读取缓冲区数据

    usb->processlxModbusRTUData(dataTemp);   // 立讯充电电流

    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));

    usbSerialPortBuf.clear();   // 清除缓冲区
}

void test_base::handleUsbSerialPortError(QSerialPort::SerialPortError error)
{
    qDebug() << "UsbSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
    {
        closeUsbSerialPort();
        // QMessageBox::warning(NULL, "警告", " Usb串口连接断开！\t\r\n");

        // ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}

void test_base::openUsbSerialPort()
{
    if (usbSerialPort->isOpen())
    {
        disconnect(usbSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readUsbSerialPortData);   // timeout执行真正的读取操作
        usbSerialPort->close();
    }

    // 设置串口名
    usbSerialPort->setPortName(getUsbcomNameCombo()->currentText());
    // 设置波特率
    usbSerialPort->setBaudRate(usbBaudRate);
    // 设置数据位
    usbSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    usbSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    usbSerialPort->setStopBits(QSerialPort::OneStop);
    usbSerialPort->setReadBufferSize(4096);

    // 设置流控制
    usbSerialPort->setFlowControl(QSerialPort::NoFlowControl);   // 设置为无流控制

    if (usbSerialPort->open(QIODevice::ReadWrite))
    {
        // 启用RTS信号
        usbSerialPort->setRequestToSend(true);
        // 启用DTR信号
        usbSerialPort->setDataTerminalReady(true);

        // ui->msgEdit->appendPlainText("串口连接成功");
        emit refreshUsbSerialPortState(1);

        connect(usbSerialPortTimer, &QTimer::timeout, this,
                &test_base::readUsbSerialPortData);   // timeout执行真正的读取操作
    }
    else
    {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // ui->msgEdit->appendPlainText("打开错误");
    }
}
void test_base::closeUsbSerialPort()
{
    // // 启用RTS信号
    // usbSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // usbSerialPort->setDataTerminalReady(false);
    if (usbSerialPort->isOpen())
        usbSerialPort->close();
    disconnect(usbSerialPortTimer, &QTimer::timeout, this,
               &test_base::readUsbSerialPortData);   // timeout执行真正的读取操作

    emit refreshUsbSerialPortState(0);
}

void test_base::readJigSerialPortData()
{
    jigSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = jigSerialPortBuf;   // 读取缓冲区数据

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    pb->parseCmd(dataTemp);
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));

    jigSerialPortBuf.clear();   // 清除缓冲区
}
void test_base::handleJigSerialPortError(QSerialPort::SerialPortError error)
{
    qDebug() << "JigSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
    {
        closeJigSerialPort();
        // QMessageBox::warning(NULL, "警告", " 治具串口连接断开！\t\r\n");

        // ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}

void test_base::openJigSerialPort()
{
    if (jigSerialPort->isOpen())
    {
        disconnect(jigSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readJigSerialPortData);   // timeout执行真正的读取操作
        jigSerialPort->close();
    }

    // 设置串口名
    jigSerialPort->setPortName(getJigcomNameCombo()->currentText());
    // 设置波特率
    jigSerialPort->setBaudRate(jigBaudRate);
    // 设置数据位
    jigSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    jigSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    jigSerialPort->setStopBits(QSerialPort::OneStop);
    jigSerialPort->setReadBufferSize(4096);

    // 设置流控制
    jigSerialPort->setFlowControl(QSerialPort::NoFlowControl);   // 设置为无流控制

    if (jigSerialPort->open(QIODevice::ReadWrite))
    {
        // 启用RTS信号
        jigSerialPort->setRequestToSend(true);
        // 启用DTR信号
        jigSerialPort->setDataTerminalReady(true);

        // ui->msgEdit->appendPlainText("串口连接成功");
        emit refreshJigSerialPortState(1);

        connect(jigSerialPortTimer, &QTimer::timeout, this,
                &test_base::readJigSerialPortData);   // timeout执行真正的读取操作
    }
    else
    {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // ui->msgEdit->appendPlainText("打开错误");
    }
}

void test_base::closeJigSerialPort()
{
    // // 启用RTS信号
    // jigSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // jigSerialPort->setDataTerminalReady(false);
    if (jigSerialPort->isOpen())
        jigSerialPort->close();
    disconnect(jigSerialPortTimer, &QTimer::timeout, this,
               &test_base::readJigSerialPortData);   // timeout执行真正的读取操作

    emit refreshJigSerialPortState(0);
}

void test_base::readProductSerialPortData()
{
    productSerialPortTimer->stop();               // 关闭定时器
    QByteArray dataTemp = productSerialPortBuf;   // 读取缓冲区数据

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();

    log->save_brush_log(macAddress, dataTemp);
    processReceivedData(dataTemp);
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // msgEdit()->appendPlainText(QString::fromUtf8(dataTemp));

    productSerialPortBuf.clear();   // 清除缓冲区
}
void test_base::handleProductSerialPortError(QSerialPort::SerialPortError error)
{
    qDebug() << "ProductSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
    {
        closeProductSerialPort();
        // QMessageBox::warning(NULL, "警告", " 产品串口连接断开！\t\r\n");

        // ui->msgEdit->appendPlainText("蓝牙连接断开");
    }
}

void test_base::openProductSerialPort()
{
    if (productSerialPort->isOpen())
    {
        disconnect(productSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readProductSerialPortData);   // timeout执行真正的读取操作
        productSerialPort->close();
    }

    // 设置串口名
    productSerialPort->setPortName(getProductcomNameCombo()->currentText());
    // 设置波特率
    productSerialPort->setBaudRate(productBaudRate);
    // 设置数据位
    productSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    productSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    productSerialPort->setStopBits(QSerialPort::OneStop);
    productSerialPort->setReadBufferSize(4096);

    // 设置流控制
    productSerialPort->setFlowControl(QSerialPort::NoFlowControl);   // 设置为无流控制

    if (productSerialPort->open(QIODevice::ReadWrite))
    {
        // 启用RTS信号
        productSerialPort->setRequestToSend(true);
        // 启用DTR信号
        productSerialPort->setDataTerminalReady(true);

        // ui->msgEdit->appendPlainText("串口连接成功");
        emit refreshProductSerialPortState(1);
        at->ask_mac();
        connect(productSerialPortTimer, &QTimer::timeout, this,
                &test_base::readProductSerialPortData);   // timeout执行真正的读取操作
    }
    else
    {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // ui->msgEdit->appendPlainText("打开错误");
    }
}

void test_base::closeProductSerialPort()
{
    // // 启用RTS信号
    // productSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // productSerialPort->setDataTerminalReady(false);
    if (productSerialPort->isOpen())
        productSerialPort->close();
    disconnect(productSerialPortTimer, &QTimer::timeout, this,
               &test_base::readProductSerialPortData);   // timeout执行真正的读取操作

    emit refreshProductSerialPortState(0);
}
void test_base::waitWork(int ms)
{
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
void test_base::update_main_style(QString style)
{
    // QSS文件初始化界面样式
    QString stylesheet;
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

void test_base::solveGetBrushResponse(int data)
{
    getRespone = data;
}
// condition=1是成功
int test_base::sendCommandWithRetry(std::function<void()> commandFunc)
{
    static int retryCount = 0;
    canGoNext = false;
    if (commandFunc != nullptr)
    {
        commandFunc();   // 重新发送指令
    }

    // 启动定时器
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this,
            [=]()
            {
                if (!getRespone)
                {   // 根据传递进来的条件判断是否未收到响应
                    if (retryCount < 3)
                    {   // 如果还有重试次数
                        if (commandFunc != nullptr)
                        {
                            commandFunc();   // 重新发送指令
                        }
                        retryCount++;
                    }
                    else
                    {
                        getRespone = 0;
                        retryCount = 0;
                        timer->stop();   // 达到最大重试次数，停止定时器
                        delete timer;
                        return 0;
                    }
                }
                else
                {   // 如果收到响应
                    timer->stop();
                    delete timer;
                    retryCount = 0;
                    getRespone = 0;
                    canGoNext = 1;
                    testState++;
                    qDebug() << "sendCommandWithRetry完成";
                    return 1;
                }
                return 0;
            });

    timer->start(500);   // 启动定时器
    return 0;
}
