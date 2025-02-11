#include "test_base.h"

#include <dbt.h>
#include <devguid.h>
#include <hidclass.h>
#include <hidsdi.h>
#include <initguid.h>
#include <setupapi.h>
#include <windows.h>

#include <QSet>
#include <QString>

#include "qcoreapplication.h"
#include "qprocess.h"

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

test_base::test_base() :
    log(new Qlog), dongleSerialPort(new QSerialPort(this)), pb(new Qpb(dongleSerialPort)),
    at(new Qat(dongleSerialPort)), usbSerialPort(new QSerialPort(this)), usb(new Qusb(usbSerialPort)),
    jigSerialPort(new QSerialPort(this)), jig(new Qjig(jigSerialPort)), productSerialPort(new QSerialPort(this)),
    product(new Qproduct(productSerialPort)) {
    signalAndslot();
    scanSerialPortsTimer->start(1000);  // 每秒刷新一次
    initData();
}
void test_base::initData() {
    pack.factory = SETTINGS.value("Mes/FACTORY").toString();
    pack.Employee_ID = SETTINGS.value("Mes/mUserno").toString();
    pack.action = SETTINGS.value("Mes/Action").toString();
    pack.machineNo = SETTINGS.value("Mes/machineNo").toString();
    pack.product = SETTINGS.value("Mes/Product_Name").toString();
    pack.line = SETTINGS.value("Mes/Line").toString();
    pack.model = SETTINGS.value("Mes/model").toString();
    pack.test_station = SETTINGS.value("Mes/test_station").toString();
    pack.password = SETTINGS.value("Mes/M_PASSWORD").toString();
    pack.userNo = SETTINGS.value("Mes/M_USERNO").toString();
    pack.lotName = SETTINGS.value("Mes/Work_Order").toString();
    pack.error = "NULL";

    isBrushLogGet = SETTINGS.value("SYSTEM/SaveToothbrushLog", 0).toBool();
    snPattern = SETTINGS.value("Regex/SNPattern", "^[0-9a-zA-Z]{18}$").toString();
}
void test_base::signalAndslot() {
    connect(at, SIGNAL(send_ble_state(int)), this, SLOT(refreshBleState(int)));
    connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
    connect(at, SIGNAL(sendWifiMsg(QString)), this, SLOT(getWifiMsg(QString)));
    connect(at, SIGNAL(send_dongle_ver(QString)), this, SLOT(getDongleVer(QString)));
    connect(at, SIGNAL(send_dongle_wifi(QString)), this, SLOT(getDongleWifi(QString)));

    connect(pb, SIGNAL(send_press_cali_data(FacPreSensorCalibResult)), this,
            SLOT(getPresscalidata(FacPreSensorCalibResult)));
    connect(pb, SIGNAL(send_press_data(FacUploadPresSensor)), this, SLOT(getPressSensorData(FacUploadPresSensor)));
    connect(pb, SIGNAL(sendGetBrushResponse(int)), this, SLOT(solveGetBrushResponse(int)));
    connect(pb, SIGNAL(send_button_state(FacButtonState)), this, SLOT(checkbutton(FacButtonState)));
    connect(pb, SIGNAL(send_BrushControl_state(FacBrushControl)), this, SLOT(checkBrushControlState(FacBrushControl)));
    connect(pb, SIGNAL(send_LED_CONTROL_state(FacLedControl)), this, SLOT(checkLedControlState(FacLedControl)));
    connect(pb, SIGNAL(send_camera_CONTROL_state(FacCameraControl)), this,
            SLOT(refreshCameraControl(FacCameraControl)));
    connect(pb, SIGNAL(send_imu_data(FacUploadNineAlex)), this, SLOT(getimuData(FacUploadNineAlex)));
    connect(pb, SIGNAL(send_IMU_CALIB_result(FacImuCalibResult)), this, SLOT(refreshImuCaliResult(FacImuCalibResult)));
    connect(pb, SIGNAL(send_pb_date(QString)), this, SLOT(refreshPbData(QString)));
    connect(pb, SIGNAL(send_motor_cali_msg(QString)), this, SLOT(refreshMotorCaliMsg(QString)));
    connect(pb, SIGNAL(send_periph_data(FacGetPeriphState)), this, SLOT(refreshPeriphData(FacGetPeriphState)));
    connect(pb, SIGNAL(send_Lcd_CONTROL_state(FacLcdControl)), this, SLOT(refreshLcdControl(FacLcdControl)));
    connect(pb, SIGNAL(send_base_data(FacGetDevBaseInfo)), this, SLOT(refreshBaseData(FacGetDevBaseInfo)));
    connect(pb, SIGNAL(send_battary(FacDevInfo)), this, SLOT(refreshBattaryData(FacDevInfo)));
    connect(pb, SIGNAL(send_sn_data(FacDevInfo)), this, SLOT(refreshSn(FacDevInfo)));
    connect(pb, SIGNAL(send_music_state(FacDevInfo)), this, SLOT(refreshMusicState(FacDevInfo)));

    connect(usb, SIGNAL(send_ammeter_data(QString)), this, SLOT(refreshAmmeterData(QString)));

    connect(scanSerialPortsTimer, SIGNAL(timeout()), this, SLOT(scanSerialPorts()));
    connect(this->dongleSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleDongleSerialPortError(QSerialPort::SerialPortError)));
    connect(this->usbSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleUsbSerialPortError(QSerialPort::SerialPortError)));
    connect(this->jigSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleJigSerialPortError(QSerialPort::SerialPortError)));
    connect(this->productSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleProductSerialPortError(QSerialPort::SerialPortError)));

    connect(this, SIGNAL(send_dongle_serialPort_state(int)), this, SLOT(refreshDongleUartState(int)));
    connect(this, SIGNAL(refreshUsbSerialPortState(int)), this, SLOT(refreshUsbUartState(int)));
    connect(this, SIGNAL(refreshJigSerialPortState(int)), this, SLOT(refreshJigUartState(int)));
    connect(this, SIGNAL(refreshProductSerialPortState(int)), this, SLOT(refreshProductUartState(int)));

    connect(dongleSerialPort, &QSerialPort::readyRead, this, [=]() {
        dongleSerialPortTimer->start(dongleOutTime);              // 设置100毫秒的延时
        dongleSerialPortBuf.append(dongleSerialPort->readAll());  // 将读到的数据放入缓冲区
    });
    connect(usbSerialPort, &QSerialPort::readyRead, this, [=]() {
        usbSerialPortTimer->start(10);                      // 设置100毫秒的延时
        usbSerialPortBuf.append(usbSerialPort->readAll());  // 将读到的数据放入缓冲区
    });
    connect(jigSerialPort, &QSerialPort::readyRead, this, [=]() {
        jigSerialPortTimer->start(10);                      // 设置100毫秒的延时
        jigSerialPortBuf.append(jigSerialPort->readAll());  // 将读到的数据放入缓冲区
    });
    connect(productSerialPort, &QSerialPort::readyRead, this, [=]() {
        productSerialPortTimer->start(10);                          // 设置100毫秒的延时
        productSerialPortBuf.append(productSerialPort->readAll());  // 将读到的数据放入缓冲区
    });
}

void test_base::scanSerialPorts() {
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    auto updateComboBox = [](QComboBox* comboBox, const QList<QSerialPortInfo>& ports) {
        if (!comboBox) {
            return;
        }

        // 获取当前的项目列表
        QSet<QString> currentItems;
        for (int i = 0; i < comboBox->count(); ++i) {
            currentItems.insert(comboBox->itemText(i));
        }

        // 添加新的项目
        for (const QSerialPortInfo& info : ports) {
            if (!currentItems.contains(info.portName())) {
                comboBox->addItem(info.portName());
            }
            currentItems.remove(info.portName());  // 移除已存在的项目
        }

        // 移除不存在的项目
        for (const QString& item : currentItems) {
            int index = comboBox->findText(item);
            if (index != -1) {
                comboBox->removeItem(index);
            }
        }
    };

    updateComboBox(getComNameCombo(), ports);
    updateComboBox(getUsbcomNameCombo(), ports);
    updateComboBox(getJigcomNameCombo(), ports);
    updateComboBox(getProductcomNameCombo(), ports);
}

void test_base::updateHIDComboBox(QComboBox* comboBox) {
    if (!comboBox)
        return;

    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];

    // 获取当前的项目列表
    QSet<QString> currentItems;
    for (int i = 0; i < comboBox->count(); ++i) {
        currentItems.insert(comboBox->itemText(i));
    }

    QProcess process;
    // Windows系统上使用 "wmic" 命令获取带有 USB 字符串的设备列表
    process.start("wmic path Win32_PnPEntity where \"Name like '%USB%'\" get DeviceID,Description");
    process.waitForFinished();
    QString output = process.readAllStandardOutput();

    QTextStream stream(&output);
    QString line;
    QStringList newDevices;  // 存储新的设备列表
    int k = 100;
    while (stream.readLineInto(&line)) {
        if (line.contains("DeviceID"))
            continue;  // 跳过标题行
        QStringList parts = line.split(QRegExp("\\s{2,}"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            QString device = parts[1].trimmed();
            if (device.contains("VID_0471")) {
                icdev = dc_init(k, 115200);
                st = dc_srd_eeprom(icdev, 0, 8, buff_1);
                if (st != 0) {
                    qDebug() << "nfc烧录器读取失败";
                } else {
                    qDebug() << "nfc烧录器读取成功";
                    QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
                    qDebug() << "nfc设备为:" << buffStr;
                    newDevices << QString("%1:%2").arg(k).arg(buffStr);
                }
                ++k;
            }
        }
    }

    // 移除不再存在的设备
    for (int i = 0; i < comboBox->count(); ++i) {
        QString currentItem = comboBox->itemText(i);
        if (!newDevices.contains(currentItem)) {
            comboBox->removeItem(i);
            currentItems.remove(currentItem);
            --i;  // 因为移除了一个项，所以要调整索引
        }
    }

    // 添加新的设备
    for (const QString& newDevice : newDevices) {
        if (!currentItems.contains(newDevice)) {
            comboBox->addItem(newDevice);
            currentItems.insert(newDevice);
        }
    }

    qDebug() << "设备个数" << currentItems.size();
}
QString test_base::toHex(const QByteArray& data) {
    QString hexStr;
    for (auto byte : data) {
        hexStr.append(QString::asprintf("%02X ", static_cast<unsigned char>(byte)));
    }
    return hexStr.trimmed();  // 去掉最后的空格
}
void test_base::saveDongleUartLog(QString data) {
    QString folderName = "dongle的log";
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
    QString fileName = "dongle日志_" + QString::number(m_index) + "_" + timestamp + ".log";
    QString filePath = dir.filePath(folderName + "/" + fileName);

    QFile logFile(filePath);
    if (logFile.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&logFile);
        out.setCodec("UTF-8");  // 设置编码格式为UTF-8
        // 获取当前时间的详细时间戳
        out << data << "\n";
        logFile.close();
    } else {
        qDebug() << "无法打开dongle日志文件：" << fileName;
    }
}

void test_base::getmacadress(const QByteArray& byte) {
    receivedData += QString::fromUtf8(byte);
    // qDebug() << "内容" << receivedData;
    while (receivedData.contains("\r\n")) {
        int index = receivedData.indexOf("\r\n");
        QString line = receivedData.left(index);
        receivedData = receivedData.mid(index + 2);  // 删除处理过的数据
        QRegularExpression regex("deviceName:(.*?),\\s*deviceAddress:(.*?),"
                                 "\\s*deviceRssi(?:\\s*:(.*))?");
        QRegularExpressionMatch match = regex.match(line);
        if (match.hasMatch()) {
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
        } else {
            // qDebug() << "Invalid line:" << line;
        }
    }
}

void test_base::readDongleSerialPortData() {
    dongleSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = dongleSerialPortBuf;  // 读取缓冲区数据
    dongleSerialPortBuf.clear();                // 清除缓冲区

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    pb->parseCmd(dataTemp);
    getmacadress(dataTemp);  // 搜索设备用
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));
    // 获取当前时间
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString logEntry = QString("[%1]\r\n%2").arg(timestamp, dataTemp);

    if (dataTemp.contains("内容为:")) {
        int pos = dataTemp.indexOf("内容为:");
        QString beforeContent = dataTemp.left(pos + QString("内容为").length() * 3 + 1).trimmed();
        QByteArray subsequentContent = dataTemp.mid(pos + QString("内容为").length() * 3 + 1).trimmed();
        QString hexContent = toHex(subsequentContent);
        logEdit()->appendPlainText(beforeContent + hexContent);
    } else {
        logEdit()->appendPlainText(logEntry);
    }
    saveDongleUartLog(logEntry);
}
void test_base::handleDongleSerialPortError(QSerialPort::SerialPortError error) {
    qDebug() << "DongleSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        closeDongleSerialPort();

        msgEdit()->appendPlainText("串口权限问题");
    }
}

void test_base::openDongleSerialPort() {
    if (dongleSerialPort->isOpen()) {
        disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readDongleSerialPortData);  // timeout执行真正的读取操作
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
    dongleSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (dongleSerialPort->open(QIODevice::ReadWrite)) {
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

        // showlog("串口连接成功");
        emit send_dongle_serialPort_state(1);

        connect(dongleSerialPortTimer, &QTimer::timeout, this,
                &test_base::readDongleSerialPortData);  // timeout执行真正的读取操作
    } else {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        showlog("串口被占用！");
    }
}

void test_base::closeDongleSerialPort() {
    // // 启用RTS信号
    // dongleSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // dongleSerialPort->setDataTerminalReady(false);
    if (dongleSerialPort->isOpen())
        dongleSerialPort->close();
    disconnect(dongleSerialPortTimer, &QTimer::timeout, this,
               &test_base::readDongleSerialPortData);  // timeout执行真正的读取操作

    emit send_dongle_serialPort_state(0);
    showlog("已经关闭串口");
}

void test_base::readUsbSerialPortData() {
    usbSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = usbSerialPortBuf;  // 读取缓冲区数据

    if (pack.factory == "xwd") {
        usb->parseCmd(dataTemp);  //欣旺达充电电流
    } else {
        usb->processlxModbusRTUData(dataTemp);  // 立讯充电电流
    }

    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));

    usbSerialPortBuf.clear();  // 清除缓冲区
}

void test_base::handleUsbSerialPortError(QSerialPort::SerialPortError error) {
    qDebug() << "UsbSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        closeUsbSerialPort();
        // QMessageBox::warning(NULL, "警告", " Usb串口连接断开！\t\r\n");

        // showlog("蓝牙连接断开");
    }
}

void test_base::openUsbSerialPort() {
    if (usbSerialPort->isOpen()) {
        disconnect(usbSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readUsbSerialPortData);  // timeout执行真正的读取操作
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
    usbSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (usbSerialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        usbSerialPort->setRequestToSend(true);
        // 启用DTR信号
        usbSerialPort->setDataTerminalReady(true);

        // showlog("串口连接成功");
        emit refreshUsbSerialPortState(1);

        connect(usbSerialPortTimer, &QTimer::timeout, this,
                &test_base::readUsbSerialPortData);  // timeout执行真正的读取操作
    } else {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // showlog("打开错误");
    }
}
void test_base::closeUsbSerialPort() {
    // // 启用RTS信号
    // usbSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // usbSerialPort->setDataTerminalReady(false);
    if (usbSerialPort->isOpen())
        usbSerialPort->close();
    disconnect(usbSerialPortTimer, &QTimer::timeout, this,
               &test_base::readUsbSerialPortData);  // timeout执行真正的读取操作

    emit refreshUsbSerialPortState(0);
}

void test_base::readJigSerialPortData() {
    jigSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = jigSerialPortBuf;  // 读取缓冲区数据

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    pb->parseCmd(dataTemp);
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));

    jigSerialPortBuf.clear();  // 清除缓冲区
}
void test_base::handleJigSerialPortError(QSerialPort::SerialPortError error) {
    qDebug() << "JigSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        closeJigSerialPort();
        // QMessageBox::warning(NULL, "警告", " 治具串口连接断开！\t\r\n");

        // showlog("蓝牙连接断开");
    }
}

void test_base::openJigSerialPort() {
    if (jigSerialPort->isOpen()) {
        disconnect(jigSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readJigSerialPortData);  // timeout执行真正的读取操作
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
    jigSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (jigSerialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        jigSerialPort->setRequestToSend(true);
        // 启用DTR信号
        jigSerialPort->setDataTerminalReady(true);

        // showlog("串口连接成功");
        emit refreshJigSerialPortState(1);

        connect(jigSerialPortTimer, &QTimer::timeout, this,
                &test_base::readJigSerialPortData);  // timeout执行真正的读取操作
    } else {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // showlog("打开错误");
    }
}

void test_base::closeJigSerialPort() {
    // // 启用RTS信号
    // jigSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // jigSerialPort->setDataTerminalReady(false);
    if (jigSerialPort->isOpen())
        jigSerialPort->close();
    disconnect(jigSerialPortTimer, &QTimer::timeout, this,
               &test_base::readJigSerialPortData);  // timeout执行真正的读取操作

    emit refreshJigSerialPortState(0);
}

void test_base::readProductSerialPortData() {
    productSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = productSerialPortBuf;  // 读取缓冲区数据

    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    if (isBrushLogGet)
        log->save_brush_log(m_index, macAddress, dataTemp);
    processReceivedData(dataTemp);
    logEdit()->appendPlainText("收到牙刷日志");
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // msgEdit()->appendPlainText(QString::fromUtf8(dataTemp));

    productSerialPortBuf.clear();  // 清除缓冲区
}
void test_base::handleProductSerialPortError(QSerialPort::SerialPortError error) {
    qDebug() << "ProductSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        closeProductSerialPort();
        // QMessageBox::warning(NULL, "警告", " 产品串口连接断开！\t\r\n");

        // showlog("蓝牙连接断开");
    }
}

void test_base::openProductSerialPort() {
    if (productSerialPort->isOpen()) {
        disconnect(productSerialPortTimer, &QTimer::timeout, this,
                   &test_base::readProductSerialPortData);  // timeout执行真正的读取操作
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
    productSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (productSerialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        productSerialPort->setRequestToSend(true);
        // 启用DTR信号
        productSerialPort->setDataTerminalReady(true);

        // showlog("串口连接成功");
        emit refreshProductSerialPortState(1);
        //  at->ask_mac();//连接串口过程，复位牙刷写入资源复位损坏
        connect(productSerialPortTimer, &QTimer::timeout, this,
                &test_base::readProductSerialPortData);  // timeout执行真正的读取操作
    } else {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // showlog("打开错误");
    }
}

void test_base::closeProductSerialPort() {
    // // 启用RTS信号
    // productSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // productSerialPort->setDataTerminalReady(false);
    if (productSerialPort->isOpen())
        productSerialPort->close();
    disconnect(productSerialPortTimer, &QTimer::timeout, this,
               &test_base::readProductSerialPortData);  // timeout执行真正的读取操作

    emit refreshProductSerialPortState(0);
}

void test_base::refreshPbData(QString data) { msgEdit()->appendPlainText(data); }

void test_base::showlog(QString msg) {
    if (!msgEdit()) {
        qDebug() << "Error: msgEdit() is nullptr!";
        return;  // 避免空指针调用
    }
    msgEdit()->appendPlainText(msg);
    qDebug() << getIndex() << msg;
}

int test_base::getIndex() { return m_index; }
void test_base::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

void test_base::updateMainStyle(QString style) {
    // QSS文件初始化界面样式
    QString stylesheet;
    QFile qss(style);
    if (qss.open(QFile::ReadOnly)) {
        qDebug() << "qss load";
        QTextStream filetext(&qss);
        stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    } else {
        qDebug() << "qss not load";
        qss.setFileName("/qss/" + style);
        if (qss.open(QFile::ReadOnly)) {
            qDebug() << "qss load";
            QTextStream filetext(&qss);
            stylesheet = filetext.readAll();
            this->setStyleSheet(stylesheet);
            qss.close();
        }
    }
}

void test_base::solveGetBrushResponse(int data) { getRespone = data; }

// condition=1是成功
int test_base::sendCommandWithRetry(std::function<void()> commandFunc) {
    static int retryCount = 0;
    canGoNext = false;
    sendRetryOver = false;
    if (commandFunc != nullptr) {
        showlog("发送pb初始指令");
        commandFunc();  // 重新发送指令
    }

    // 启动定时器
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        qDebug() << "retryCount=" << retryCount
                 << QString("sendCommandWithRetry定时器触发时间: %1, timer 地址: %2")
                        .arg(currentTime)
                        .arg(reinterpret_cast<quintptr>(timer), 0, 16);  // 以16进制显示地址

        if (!getRespone) {          // 根据传递进来的条件判断是否未收到响应
            if (retryCount < 20) {  // 如果还有重试次数
                if (commandFunc != nullptr && !(retryCount % 5)) {
                    showlog("重新发送指令发送pb指令");
                    commandFunc();  // 重新发送指令
                }
                retryCount++;
            } else {
                disconnect(timer, &QTimer::timeout, this, nullptr);
                getRespone = 0;
                retryCount = 0;
                sendRetryOver = 1;
                timer->stop();  // 达到最大重试次数，停止定时器

                showlog("达到最大重试次数，停止定时器");
                delete timer;
                return 0;
            }
        } else {  // 如果收到响应
            disconnect(timer, &QTimer::timeout, this, nullptr);
            timer->stop();
            delete timer;
            retryCount = 0;
            getRespone = 0;
            canGoNext = 1;
            testState++;
            showlog("sendCommandWithRetry完成，收到牙刷响应");
            return 1;
        }
        return 0;
    });

    timer->start(300);  // 启动定时器
    return 0;
}
QString test_base::exportTableContent() {
    if (testResultTable() == nullptr) {
        showlog("exportTableContent不存在表格");
        return "不存在表格";
    }

    QStringList exportedContent;
    for (int row = 0; row < testResultTable()->rowCount(); ++row) {
        QString testItem = testResultTable()->item(row, 0)->text();
        QString testData = testResultTable()->item(row, 1)->text();
        QString itemValue = QString("|%1:%2").arg(testItem).arg(testData);

        exportedContent << itemValue;
    }
    // 将所有内容连接成一个字符串，不在最后一个元素后面添加换行符
    QString result = exportedContent.join("") + "|";

    qDebug() << result;
    return result;
}

void test_base::testResultTableUpdate(QVector<TestItem>& testItems) {
    if (testResultTable() == nullptr) {
        showlog("testResultTableUpdate不存在表格");
        return;
    }

    for (const auto& item : testItems) {
        // 获取当前时间戳
        // QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");

        // 插入新的一行
        int row = testResultTable()->rowCount();
        testResultTable()->insertRow(row);

        // 自动滚动到表格底部
        testResultTable()->scrollToBottom();

        // 设置每列的数据
        // testResultTable()->setItem(row, 0, new QTableWidgetItem(timestamp));
        testResultTable()->setItem(row, 0, new QTableWidgetItem(item.testItem));
        testResultTable()->setItem(row, 1, new QTableWidgetItem(item.testData));

        // 设置结果列的数据，假设结果是一个字符串
        QTableWidgetItem* resultItem = new QTableWidgetItem(item.testResult);
        QTableWidgetItem* askItem = new QTableWidgetItem(item.ask);

        // 设置失败状态的背景颜色为红色
        if (item.testResult == "失败") {
            resultItem->setBackground(QBrush(Qt::red));
        } else if (item.testResult == "通过") {
            resultItem->setBackground(QBrush(Qt::green));
        }

        testResultTable()->setItem(row, 2, resultItem);
        testResultTable()->setItem(row, 3, askItem);
    }
    log->saveTestCsv(upperComputerVer, getMacLineEdit()->text(), macInputLineEdit()->text(), testItems);

    testItems.clear();
}
void test_base::updateTestData(QVector<TestItem>& testItems) {
    for (auto& item : testItems) {
        QStringList expectedValueList = item.ask.split('=');
        if (expectedValueList.contains(item.testData)) {
            item.testResult = passValue;
        } else {
            item.testResult = failValue;
        }
    }
    testResultTableUpdate(testItems);
}
void test_base::solveMesSucess(const int mechines) {
    if (getMesStateQlabel() == nullptr) {
        showlog("mes的状态文本为空");
        return;
    }
    if (mechines == getIndex()) {
        TestItem test;
        test.testItem = "mes操作";
        test.testData = QString::number(mechines);
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);
        testResultTableUpdate(testItems);

        showlog("mes操作成功");
        getMesStateQlabel()->setText("MES");
        getMesStateQlabel()->setStyleSheet(
            "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid "
            "#00FF00; border-radius: 10px; padding: 10px; text-align: center;");
    }
}

void test_base::solveMesData(const int mechines, QString msg) {
    if (getMesStateQlabel() == nullptr) {
        showlog("mes的状态文本为空");
        return;
    }
    if (getEndTestButton() == nullptr) {
        showlog("结束测试按钮未绑定");
        return;
    }

    if (mechines == getIndex()) {
        TestItem test;
        test.testItem = "mes报错";
        test.testData = msg;
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);
        testResultTableUpdate(testItems);

        showlog("MES:报错信息:" + msg);
        isTestContinue = false;
        showlog("停止运行");
        getEndTestButton()->click();
        getMesStateQlabel()->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
            "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");

        bandingresult = false;

        getMacLineEdit()->setDisabled(0);
        macInputLineEdit()->setDisabled(0);

        getMacLineEdit()->clear();
        getMacLineEdit()->setFocus();
        emit send_end_test(getIndex());
    }
}
void test_base::testResultTableInit() {
    pb->APP_VERSION = upperComputerVer;
    LockProductUI();
    if (testResultTable() == nullptr) {
        showlog("testResultTableInit不存在表格");
        return;
    }
    testResultTable()->clear();
    // 初始化表格
    testResultTable()->setColumnCount(4);  // 三列，分别为Mac地址、SN码和时间戳

    testResultTable()->setRowCount(0);  // 初始行数为0，因为还没有数据

    // 设置表格标题
    QStringList headers;
    headers << "项目"
            << "数据"
            << "结果"
            << "要求";
    testResultTable()->setHorizontalHeaderLabels(headers);
    // 设置表格自适应列宽
    testResultTable()->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}
void test_base::LockProductUI() {
    if (SETTINGS.value("SYSTEM/LockProductUI", 0).toBool())
        getIsUseMes()->setEnabled(false);

    if (!getIsUseMes() || !getIsFormMes()) {
        qDebug() << "Error: getIsUseMes() or getIsFormMes() is nullptr!";
        return;
    }

    if (pack.factory == "无mes厂") {
        getIsUseMes()->setCheckState(Qt::Unchecked);
        getIsFormMes()->setCheckState(Qt::Unchecked);
    }
}
void test_base::getMac(QString sn_to_search) {
    if (!getIsFormMes()->isChecked()) {
        // 在合适的位置定义变量
        QString fileName = "mac_sn.txt";

        if (SETTINGS.value("Mes/FACTORY").toString() == "hq")
            fileName = "mac_sn.txt";
        else if (SETTINGS.value("Mes/FACTORY").toString() == "lx")
            fileName = "mac_sn.txt";
        else if (SETTINGS.value("Mes/FACTORY").toString() == "jj")
            fileName = "mac_sn.txt";
        else if (SETTINGS.value("Mes/FACTORY").toString() == "xwd") {
            QString path = "\\\\172.60.1.249\\sgpub\\LTC\\MAC\\mac_sn.txt";
            QFileInfo fileInfo(path);
            // 创建一个文件对象
            if (fileInfo.exists() && fileInfo.isFile()) {
                fileName = path;
                // file.setFileName(path);
                showlog("云端文件存在，正在打开云端文件...");
            } else {
                fileName = "mac_sn.txt";
                // file.setFileName("mac_sn.txt");
                showlog("云端文件不存在，正在打开本地文件...");
            }
        } else {
            fileName = "mac_sn.txt";
        }

        // 在需要的地方使用这个变量创建 QFile 对象
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly)) {  // 打开文件
            QTextStream in(&file);
            while (!in.atEnd()) {                      // 逐行读取文件
                QString line = in.readLine();          // 读取一行
                QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
                if (fields.count() >= 2) {             // 至少需要两个字段
                    QString sn = fields.at(0);         // 第一个字段是sn
                    QString mac = fields.at(1);        // 第二个字段是mac
                    if (sn == sn_to_search) {          // 检查是否是待检索的sn
                        showlog("这是从文件获取的mac地址");
                        macInputLineEdit()->setText(mac);
                        macInputLineEdit()->returnPressed();

                        qDebug() << getIndex() << "The corresponding mac is: " << mac;
                        break;
                    }
                } else {
                    showlog("存在没有逗号分开的" + QString::number(fields.count()) + line);
                }
            }
            file.close();  // 关闭文件
        }
        if (!getIsFormMes()->isChecked() && macInputLineEdit()->text().isEmpty()) {
            getMacLineEdit()->clear();
            showlog("找不到mac地址，清空当前输入的sn");
        }
    }
}

void test_base::getDongleVer(QString data) { showlog("当前dongle的版本为：" + data); }
void test_base::refreshMesState(int state) {
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}
