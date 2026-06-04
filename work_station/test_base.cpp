#include "test_base.h"

#include "my_set/my_typedef.h"

#include <dbt.h>
#include <devguid.h>
#include <hidclass.h>
#include <hidsdi.h>
#include <initguid.h>
#include <setupapi.h>
#include <windows.h>

#include <QDateTime>
#include <QDebug>
#include <QSet>
#include <QString>

#include "qcoreapplication.h"
#include "qprocess.h"
#include "agreement/qProtocol/qfctp/qfctp.h"
#include "agreement/qProtocol/qaiot/qaiot.h"
#include "common_utils.h"

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "ole32.lib")

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

test_base::test_base(QWidget* parent) :
    QWidget(parent),
    log(new Qlog),
    dongleSerialChannel_(new SerialChannel(this)),
    usbSerialChannel_(new SerialChannel(this)),
    jigSerialChannel_(new SerialChannel(this)),
    productSerialChannel_(new SerialChannel(this)),
    dongleSerialPort(dongleSerialChannel_->port()),
    pb(new Qpb(dongleSerialPort)),
    qfctp(new Qfctp(dongleSerialPort)),
    qaiot(new Qaiot(dongleSerialPort)),
    at(new Qat(dongleSerialPort)),
    usbSerialPort(usbSerialChannel_->port()),
    usb(new Qusb(usbSerialPort)),
    visa(new Qvisa(this)),
    jigSerialPort(jigSerialChannel_->port()),
    jig(new Qjig(jigSerialPort)),
    productSerialPort(productSerialChannel_->port()),
    product(new Qproduct(productSerialPort, this)) {
    protocolManager.bindQpb(pb);
    protocolManager.bindQfctp(qfctp);
    protocolManager.bindQaiot(qaiot);
    const std::string protocolName = SETTINGS.value("SYSTEM/ProtocolType", "qpb").toString().toStdString();
    auto selectedType = QProtocolManager::protocolTypeFromString(protocolName);
    if (selectedType == QProtocolManager::ProtocolType::Unknown) {
        selectedType = QProtocolManager::ProtocolType::Qpb;
    }
    protocolManager.setCurrentProtocolType(selectedType);
    showlog(QStringLiteral("当前设备协议：%1")
                .arg(QString::fromStdString(
                    QProtocolManager::protocolTypeToString(protocolManager.currentProtocolType()))));

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
    connect(&protocolManager, &QProtocolManager::send_pb_date, this, &test_base::refreshPbData);
    connect(at, SIGNAL(send_ble_state(int)), this, SLOT(refreshBleState(int)));
    connect(at, SIGNAL(sendGetProductResponse(int)), this, SLOT(solveGetBrushResponse(int)));
    connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
    connect(at, SIGNAL(sendWifiMsg(QString)), this, SLOT(getWifiMsg(QString)));
    connect(at, SIGNAL(send_dongle_ver(QString)), this, SLOT(getDongleVer(QString)));
    connect(at, SIGNAL(send_dongle_wifi(QString)), this, SLOT(getDongleWifi(QString)));

    connect(pb, SIGNAL(send_press_cali_data(ProtocolPressCalibResultData)), this,
            SLOT(getPresscalidata(ProtocolPressCalibResultData)));
    connect(pb, SIGNAL(send_press_data(ProtocolPressSampleData)), this, SLOT(getPressSensorData(ProtocolPressSampleData)));
    connect(&protocolManager, SIGNAL(sendGetProductResponse(int)), this, SLOT(solveGetBrushResponse(int)));
    connect(&protocolManager, SIGNAL(send_button_state(ProtocolButtonStateData)), this, SLOT(checkbutton(ProtocolButtonStateData)));
    connect(pb, SIGNAL(send_BrushControl_state(ProtocolBrushControlData)), this,
            SLOT(checkBrushControlState(ProtocolBrushControlData)));
    connect(pb, SIGNAL(send_LED_CONTROL_state(ProtocolLedControlData)), this, SLOT(checkLedControlState(ProtocolLedControlData)));
    connect(pb, SIGNAL(send_camera_CONTROL_state(ProtocolCameraControlData)), this,
            SLOT(refreshCameraControl(ProtocolCameraControlData)));
    connect(pb, SIGNAL(send_imu_data(ProtocolImuSampleData)), this, SLOT(getimuData(ProtocolImuSampleData)));
    connect(pb, SIGNAL(send_IMU_CALIB_result(ProtocolImuCalibResultData)), this,
            SLOT(refreshImuCaliResult(ProtocolImuCalibResultData)));
    connect(pb, SIGNAL(send_motor_cali_msg(QString)), this, SLOT(refreshMotorCaliMsg(QString)));
    connect(&protocolManager, SIGNAL(send_periph_data(ProtocolPeriphStateData)), this, SLOT(refreshPeriphData(ProtocolPeriphStateData)));
    connect(pb, SIGNAL(send_Lcd_CONTROL_state(ProtocolLcdControlData)), this, SLOT(refreshLcdControl(ProtocolLcdControlData)));
    connect(&protocolManager, SIGNAL(send_base_data(ProtocolBaseInfoData)), this, SLOT(refreshBaseData(ProtocolBaseInfoData)));
    connect(&protocolManager, SIGNAL(send_battary(ProtocolBatteryData)), this, SLOT(refreshBattaryData(ProtocolBatteryData)));
    connect(&protocolManager, SIGNAL(send_sn_data(ProtocolSnData)), this, SLOT(refreshSn(ProtocolSnData)));
    connect(qfctp, SIGNAL(send_rssi_read(ProtocolRssiData)), this, SLOT(refreshRssiRead(ProtocolRssiData)));
    connect(qfctp, SIGNAL(send_charge_current_read(ProtocolUInt32ValueData)), this, SLOT(refreshChargeCurrentRead(ProtocolUInt32ValueData)));
    connect(qfctp, SIGNAL(send_key_signal_read(ProtocolUInt32ValueData)), this, SLOT(refreshKeySignalRead(ProtocolUInt32ValueData)));
    connect(qfctp, SIGNAL(send_tuple_parsed(ProtocolTupleData)), this, SLOT(refreshTupleData(ProtocolTupleData)));
    connect(pb, SIGNAL(send_music_state(ProtocolMusicStateData)), this, SLOT(refreshMusicState(ProtocolMusicStateData)));

    connect(usb, SIGNAL(send_ammeter_data(QString)), this, SLOT(refreshAmmeterData(QString)));
    connect(jig, SIGNAL(send_amplitude_data(QString)), this, SLOT(refreshAmplitudeData(QString)));

    connect(scanSerialPortsTimer, SIGNAL(timeout()), this, SLOT(scanSerialPorts()));
    connect(dongleSerialChannel_, &SerialChannel::frameReceived, this, [this](const QByteArray& data) {
        onDongleSerialFrame(data);
    });
    connect(usbSerialChannel_, &SerialChannel::frameReceived, this, [this](const QByteArray& data) {
        onUsbSerialFrame(data);
    });
    connect(jigSerialChannel_, &SerialChannel::frameReceived, this, [this](const QByteArray& data) {
        onJigSerialFrame(data);
    });
    connect(productSerialChannel_, &SerialChannel::frameReceived, this, [this](const QByteArray& data) {
        onProductSerialFrame(data);
    });
    connect(dongleSerialChannel_, &SerialChannel::errorOccurred, this, &test_base::handleDongleSerialPortError);
    connect(usbSerialChannel_, &SerialChannel::errorOccurred, this, &test_base::handleUsbSerialPortError);
    connect(jigSerialChannel_, &SerialChannel::errorOccurred, this, &test_base::handleJigSerialPortError);
    connect(productSerialChannel_, &SerialChannel::errorOccurred, this, &test_base::handleProductSerialPortError);

    connect(this, SIGNAL(send_dongle_serialPort_state(int)), this, SLOT(refreshDongleUartState(int)));
    connect(this, SIGNAL(refreshUsbSerialPortState(int)), this, SLOT(refreshUsbUartState(int)));
    connect(this, SIGNAL(refreshJigSerialPortState(int)), this, SLOT(refreshJigUartState(int)));
    connect(this, SIGNAL(refreshProductSerialPortState(int)), this, SLOT(refreshProductUartState(int)));

}

void test_base::resetVisaBackend() {
    if (visa) {
        visa->closeConnection();
    }
}

void test_base::scanSerialPorts() {
    SerialChannel::updateComboBoxPorts(getComNameCombo());
    SerialChannel::updateComboBoxPorts(getUsbcomNameCombo());
    SerialChannel::updateComboBoxPorts(getJigcomNameCombo());
    SerialChannel::updateComboBoxPorts(getProductcomNameCombo());
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
    return CommonUtils::toHexUpperSpaced(data);
}
void test_base::saveDongleUartLog(QString data) {
    const QString folderName = QStringLiteral("所有log/dongle的log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        qDebug() << "无法创建目录:" << folderName;
        return;
    }

    const QString fileName =
        QStringLiteral("dongle日志_") + QString::number(m_index) + QLatin1Char('_') + CommonUtils::dateStampYmd() +
        QStringLiteral(".log");
    const QString filePath = CommonUtils::joinPath(folderName, fileName);

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

void test_base::onDongleSerialFrame(const QByteArray& dataTemp) {
    // qDebug() << getIndex()<< "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    protocolManager.parseCmd(dataTemp);
    getmacadress(dataTemp);  // 搜索设备用
    // getmacadress(dataTemp);
    //  qDebug() << getIndex()<< QString::fromUtf8(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));
    // 获取当前时间
    const QString timestamp = CommonUtils::formatTimestampMs();
    QString logEntry = QString("[%1]\r\n%2").arg(timestamp, dataTemp);

    if (dataTemp.contains("内容为:")) {
        int pos = dataTemp.indexOf("内容为:");
        QString beforeContent = dataTemp.left(pos + QString("内容为").length() * 3 + 1).trimmed();
        QByteArray subsequentContent = dataTemp.mid(pos + QString("内容为").length() * 3 + 1).trimmed();
        const QString hexContent = CommonUtils::toHexUpperSpaced(subsequentContent);
        logEdit()->appendPlainText(beforeContent + hexContent);
    } else {
        logEdit()->appendPlainText(logEntry);
    }
    saveDongleUartLog(logEntry);
}
void test_base::handleDongleSerialPortError(QSerialPort::SerialPortError error, const QString& message) {
    if (error == QSerialPort::NoError)
        return;
    qWarning() << "DongleSerialPort串口异常"
               << "port=" << dongleSerialChannel_->portName() << "code=" << error << "detail=" << message;
    if (error == QSerialPort::PermissionError) {
        closeDongleSerialPort();
        msgEdit()->appendPlainText(QStringLiteral("串口权限问题"));
    }
}

void test_base::openDongleSerialPort() {
    SerialChannel::OpenParams params;
    params.portName = getComNameCombo()->currentText();
    params.baudRate = dongleBaudRate;
    params.readDebounceMs = dongleOutTime;
    params.rtsDtrMode = SerialChannel::RtsDtrMode::ToggleReset;

    if (dongleSerialChannel_->open(params)) {
        showlog(QStringLiteral("当前设备协议：%1")
                    .arg(QString::fromStdString(
                        QProtocolManager::protocolTypeToString(protocolManager.currentProtocolType()))));
        emit send_dongle_serialPort_state(1);
    } else {
        showlog(QStringLiteral("串口被占用！"));
    }
}

void test_base::closeDongleSerialPort() {
    dongleSerialChannel_->close();
    emit send_dongle_serialPort_state(0);
    showlog(QStringLiteral("已经关闭串口"));
}

void test_base::onUsbSerialFrame(const QByteArray& dataTemp) {
    usb->parseCmd(dataTemp);
}

void test_base::handleUsbSerialPortError(QSerialPort::SerialPortError error, const QString& message) {
    Q_UNUSED(message);
    qDebug() << "UsbSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
        closeUsbSerialPort();
}

void test_base::openUsbSerialPort() {
    SerialChannel::OpenParams params;
    params.portName = getUsbcomNameCombo()->currentText();
    params.baudRate = usbBaudRate;
    params.readDebounceMs = 10;
    params.rtsDtrMode = SerialChannel::RtsDtrMode::Enable;

    if (usbSerialChannel_->open(params))
        emit refreshUsbSerialPortState(1);
}

void test_base::closeUsbSerialPort() {
    usbSerialChannel_->close();
    emit refreshUsbSerialPortState(0);
}

void test_base::onJigSerialFrame(const QByteArray& dataTemp) {
    qDebug() << getIndex() << "data len : " << dataTemp.size();
    jig->parseCmd(dataTemp);
}

void test_base::handleJigSerialPortError(QSerialPort::SerialPortError error, const QString& message) {
    Q_UNUSED(message);
    qDebug() << "JigSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
        closeJigSerialPort();
}

void test_base::openJigSerialPort() {
    SerialChannel::OpenParams params;
    params.portName = getJigcomNameCombo()->currentText();
    params.baudRate = jigBaudRate;
    params.readDebounceMs = 10;
    params.rtsDtrMode = SerialChannel::RtsDtrMode::Enable;

    if (jigSerialChannel_->open(params))
        emit refreshJigSerialPortState(1);
}

void test_base::closeJigSerialPort() {
    jigSerialChannel_->close();
    emit refreshJigSerialPortState(0);
}

void test_base::onProductSerialFrame(const QByteArray& dataTemp) {
    qDebug() << getIndex() << "product data len : " << dataTemp.size();
    if (product)
        product->parseCmd(dataTemp);
    if (isBrushLogGet)
        log->save_brush_log(m_index, macAddress, dataTemp);
    processReceivedData(dataTemp);
    logEdit()->appendPlainText(QStringLiteral("收到设备日志"));
}

void test_base::handleProductSerialPortError(QSerialPort::SerialPortError error, const QString& message) {
    Q_UNUSED(message);
    qDebug() << "ProductSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError)
        closeProductSerialPort();
}

void test_base::openProductSerialPort() {
    if (product)
        product->clearProductSerialRxAccum();

    SerialChannel::OpenParams params;
    params.portName = getProductcomNameCombo()->currentText();
    params.baudRate = productBaudRate;
    params.readDebounceMs = 10;
    params.rtsDtrMode = SerialChannel::RtsDtrMode::Enable;

    if (productSerialChannel_->open(params))
        emit refreshProductSerialPortState(1);
}

void test_base::closeProductSerialPort() {
    productSerialChannel_->close();
    if (product)
        product->clearProductSerialRxAccum();
    emit refreshProductSerialPortState(0);
}

void test_base::refreshPbData(QString data) { msgEdit()->appendPlainText(data); }

void test_base::showlog(QString msg) {
    qDebug() << getIndex() << msg;
    if (!msgEdit()) {
        return;
    }
    msgEdit()->appendPlainText(msg);
}

int test_base::getIndex() const { return m_index; }
void test_base::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

void test_base::updateMainStyle(QString style) {
    applyWidgetStyleSheet(this, style);
}

bool test_base::isCommandRetryResponseAccepted(const QObject* source) const {
    if (!commandRetryTimer) {
        return false;
    }
    switch (commandWaitSource_) {
    case CommandWaitSource::Any:
        return true;
    case CommandWaitSource::ProductProtocol:
        if (source == at) {
            qDebug() << QStringLiteral("sendCommandWithRetry: 忽略 dongle AT，等待产测协议");
            return false;
        }
        return true;
    case CommandWaitSource::DongleAt:
        if (source != static_cast<const QObject*>(at)) {
            qDebug() << QStringLiteral("sendCommandWithRetry: 忽略非 dongle AT");
            return false;
        }
        return true;
    }
    return true;
}

void test_base::finishCommandRetryWait(bool success, const QString& logMessage) {
    if (!commandRetryTimer) {
        return;
    }
    disconnect(commandRetryTimer, &QTimer::timeout, this, nullptr);
    commandRetryTimer->stop();
    commandRetryTimer->deleteLater();
    commandRetryTimer = nullptr;

    lastCommandRetryCount = commandRetrySendCount;
    commandRetryCount = 0;
    commandRetrySendCount = 0;
    commandWaitSource_ = CommandWaitSource::Any;
    getRespone = 0;

    canGoNext = true;
    sendRetryOver = !success;

    if (!logMessage.isEmpty()) {
        showlog(logMessage);
    }
}

void test_base::solveGetBrushResponse(int data) {
    if (!isCommandRetryResponseAccepted(sender())) {
        return;
    }
    const bool success = (data != 0);
    finishCommandRetryWait(
        success,
        success ? QStringLiteral("sendCommandWithRetry完成，收到设备响应")
                : QStringLiteral("设备应答失败（协议 FAIL）"));
}

void test_base::onCommandRetryTimerTimeout() {
    if (!commandRetryTimer) {
        return;
    }
    qDebug() << "retryCount=" << commandRetryCount
             << QString("sendCommandWithRetry定时器触发, timer=%1")
                    .arg(reinterpret_cast<quintptr>(commandRetryTimer), 0, 16);

    if (commandRetryCount < 20) {
        if (commandRetryFunc_ && (commandRetryCount % 5) == 0) {
            showlog(QStringLiteral("重新发送指令%1").arg(commandRetryCount));
            ++commandRetrySendCount;
            commandRetryFunc_();
        }
        ++commandRetryCount;
        return;
    }

    finishCommandRetryWait(false, QStringLiteral("达到最大重试次数，停止定时器"));
}

int test_base::sendCommandWithRetry(std::function<void()> commandFunc, int timeoutMs) {
    if (commandRetryTimer) {
        disconnect(commandRetryTimer, &QTimer::timeout, this, nullptr);
        commandRetryTimer->stop();
        commandRetryTimer->deleteLater();
        commandRetryTimer = nullptr;
    }

    commandRetryFunc_ = std::move(commandFunc);
    commandRetryCount = 0;
    commandRetrySendCount = 0;
    lastCommandRetryCount = 0;
    canGoNext = false;
    sendRetryOver = false;
    getRespone = 0;

    if (commandRetryFunc_) {
        commandRetrySendCount = 1;
        commandRetryFunc_();
    }

    commandRetryTimer = new QTimer(this);
    connect(commandRetryTimer, &QTimer::timeout, this, &test_base::onCommandRetryTimerTimeout);
    commandRetryTimer->start(timeoutMs);
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
        QTableWidgetItem* testItemCell = new QTableWidgetItem(item.testItem);
        QTableWidgetItem* testDataCell = new QTableWidgetItem(item.testData);
        QTableWidgetItem* resultItem = new QTableWidgetItem(item.testResult);
        QTableWidgetItem* askItem = new QTableWidgetItem(item.ask);
        testItemCell->setTextAlignment(Qt::AlignCenter);
        testDataCell->setTextAlignment(Qt::AlignCenter);
        resultItem->setTextAlignment(Qt::AlignCenter);
        askItem->setTextAlignment(Qt::AlignCenter);

        // 设置失败状态的背景颜色为红色
        if (item.testResult == "失败") {
            resultItem->setBackground(QBrush(Qt::red));
        } else if (item.testResult == "通过") {
            resultItem->setBackground(QBrush(Qt::green));
        }

        testResultTable()->setItem(row, 0, testItemCell);
        testResultTable()->setItem(row, 1, testDataCell);
        testResultTable()->setItem(row, 2, resultItem);
        testResultTable()->setItem(row, 3, askItem);
    }
    log->saveTestCsv(upperComputerVer, getMacLineEdit()->text(), macInputLineEdit()->text(), testItems);

    testItems.clear();
}
void test_base::updateTestData(QVector<TestItem>& testItems) {
    for (auto& item : testItems) {
        item.testResult =
            CommonUtils::compareVersions(item.ask, item.testData) ? passValue : failValue;
    }
    testResultTableUpdate(testItems);
}
void test_base::solveMesSucess(const int mechines) {
    if (getMesStateQlabel() == nullptr) {
        showlog("mes的状态文本为空");
        return;
    }
    if (mechines == getIndex()) {
        // appendStationResult(testItems, QStringLiteral("MES启动"), QStringLiteral("OK"), passValue);
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
    pb->setAppVersion(upperComputerVer);
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
    testResultTable()->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    // 列宽随表格尺寸变化，拖拽外部大小时同步缩放
    testResultTable()->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    testResultTable()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
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
void test_base::closeEvent(QCloseEvent*) {
    qDebug() << getIndex() << "test_base关闭";
    isTestContinue = 0;
    at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00");  // 发送mac地址
    waitWork(50);
}
void test_base::getDongleVer(QString data) { showlog("当前dongle的版本为：" + data); }
void test_base::refreshMesState(int state) {
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}

bool test_base::compareVersions(const QString& versionList, const QString& versionToCompare) {
    return CommonUtils::compareVersions(versionList, versionToCompare);
}
QString test_base::getValueBySN(const QString& sn) {
    QString truncatedSN;

    if (SETTINGS.value("Mes/Product_Name").toString() == "U7" || SETTINGS.value("Mes/Product_Name").toString() == "U7P")
        truncatedSN = sn.left(8);
    else
        truncatedSN = sn.left(9);

    showlog("truncatedSN:" + truncatedSN);

    QString fileName = "sn_subpid.txt";
    // 在需要的地方使用这个变量创建 QFile 对象
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {  // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                      // 逐行读取文件
            QString line = in.readLine();          // 读取一行
            QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {             // 至少需要两个字段
                QString filesn = fields.at(0);     // 第一个字段是sn
                QString subpid = fields.at(1);     // 第二个字段是subpid
                if (filesn == truncatedSN) {       // 检查是否是待检索的sn
                    showlog("文件匹配到的subpid：" + subpid);
                    return subpid;
                }
            }
        }
        file.close();  // 关闭文件
    }

    return "SUBPID_ERRO";
}
QString test_base::generateDateCode() {
    const QDateTime currentDateTime = QDateTime::currentDateTime();
    const int year = currentDateTime.date().year() % 100;
    const int month = currentDateTime.date().month();
    const int day = currentDateTime.date().day();

    char monthCode;
    if (month >= 1 && month <= 9) {
        monthCode = static_cast<char>('0' + month);
    } else {
        monthCode = static_cast<char>('A' + (month - 10));
    }

    char dayCode;
    if (day >= 1 && day <= 9) {
        dayCode = static_cast<char>('0' + day);
    } else {
        dayCode = static_cast<char>('A' + (day - 10));
    }

    return QString::number(year) + QChar::fromLatin1(monthCode) + QChar::fromLatin1(dayCode);
}

bool test_base::applyAdaptiveV3ProductBySn(QLineEdit* snEdit) {
    if (!snEdit) {
        return false;
    }
    QString snText = snEdit->text().trimmed();
    snText.remove(QRegularExpression(QStringLiteral("[^0-9A-Za-z]")));
    if (snText.isEmpty()) {
        return false;
    }
    snEdit->setText(snText);

    const QString upperSn = snText.toUpper();
    QString targetProduct;
    int targetLen = 0;

    if (upperSn.contains(QStringLiteral("V3PRO")) || snText.length() == 15) {
        targetProduct = QStringLiteral("V3Pro");
        targetLen = 15;
    } else if (upperSn.contains(QStringLiteral("V3")) || snText.length() == 12) {
        targetProduct = QStringLiteral("V3");
        targetLen = 12;
    } else {
        return false;
    }

    const QString targetRegex = QStringLiteral("^[0-9a-zA-Z]{%1}$").arg(targetLen);
    SETTINGS.setValue(QStringLiteral("Mes/Product_Name"), targetProduct);
    SETTINGS.setValue(QStringLiteral("MES/Product_Name"), targetProduct);
    SETTINGS.setValue(QStringLiteral("Regex/SNPattern"), targetRegex);
    SETTINGS.sync();

    pack.product = targetProduct;
    snPattern = targetRegex;
    showlog(QStringLiteral("识别产品型号：%1，SN校验规则切换为%2").arg(targetProduct, snPattern));
    return true;
}

QString test_base::parseMacFromSn(const QString& snCode) {
    QString sn = snCode;
    sn.remove(QRegularExpression("\\s+"));
    if (sn.length() < 16) {
        qDebug() << "[parseMacFromSn] 长度太短 trimLen=" << sn.length();
        return QString("长度太短");
    }
    QString macRaw = sn.mid(4, 12).toUpper();
    if (!QRegularExpression("^[0-9A-F]{12}$").match(macRaw).hasMatch()) {
        qDebug() << "[parseMacFromSn] 不符合规则 macRaw=" << macRaw;
        return QString("不符合规则");
    }
    const QString mac = QString("%1:%2:%3:%4:%5:%6")
                            .arg(macRaw.mid(0, 2))
                            .arg(macRaw.mid(2, 2))
                            .arg(macRaw.mid(4, 2))
                            .arg(macRaw.mid(6, 2))
                            .arg(macRaw.mid(8, 2))
                            .arg(macRaw.mid(10, 2));
    qDebug() << "[parseMacFromSn] ok" << mac;
    return mac;
}

void test_base::appendStationResult(QVector<TestItem>& testItems, const QString& item, const QString& data, const QString& result) {
    TestItem test;
    test.testItem = item;
    test.testData = data;
    test.testResult = result;
    test.ask = "通过";
    testItems.append(test);
}
