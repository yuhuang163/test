#include "dongle_at_device.h"
#include <QDebug>
#include <QSerialPort>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

DongleAtDevice::DongleAtDevice(QSerialPort* port, QObject* parent) : QObject(parent), serialPort_(port) {
    registerCommand();
}

void DongleAtDevice::onFrameReceived(const AtFrame& frame) {
    auto it = commandList_.find(frame.cmd);
    if (it != commandList_.end()) {
        it->second(frame.parameter);
    }
}

void DongleAtDevice::set(DongleCmd cmd, const QVariant& data) {
    switch (cmd) {
    case DongleCmd::BleScanConnect:
        sendAtLine(QStringLiteral("AT+MAC=%1\r\n").arg(data.toString()));
        break;
    case DongleCmd::BleDirectConnect:
        sendAtLine(QStringLiteral("AT+DCON=%1\r\n").arg(data.toString()));
        break;
    case DongleCmd::BleOtaConnect:
        sendAtLine(QStringLiteral("AT+OTA=%1\r\n").arg(data.toString()));
        break;
    case DongleCmd::BleAppConnect:
        sendAtLine(QStringLiteral("AT+BLE=%1\r\n").arg(data.toString()));
        break;
    case DongleCmd::BleMainConnect:
        sendAtLine(QStringLiteral("AT+MAIN=%1\r\n").arg(data.toString()));
        break;
    case DongleCmd::OtaDataPassthrough:
        sendAtLine(QStringLiteral("AT+OTADATA=%1\r\n").arg(data.toInt()));
        break;
    case DongleCmd::OtaPktSize:
        sendAtLine(QStringLiteral("AT+OTAPKTSIZE=%1\r\n").arg(data.toInt()));
        break;
    case DongleCmd::BleMtu:
        sendAtLine(QStringLiteral("AT+BLEMTU=%1\r\n").arg(data.toInt()));
        break;
    case DongleCmd::MainDataPassthrough:
        sendAtLine(QStringLiteral("AT+MAINDATA=%1\r\n").arg(data.toInt()));
        break;
    case DongleCmd::BleLog: {
        int state = data.toInt();
        if (state > 1) {
            state = 1;
        }
        sendAtLine(QStringLiteral("AT+BLELOG=%1\r\n").arg(state));
        break;
    }
    case DongleCmd::GetSuction: {
        int state = data.toInt();
        if (state > 1) {
            state = 1;
        }
        sendAtLine(QStringLiteral("AT+SUCTION=%1\r\n").arg(state));
        break;
    }
    case DongleCmd::BleDeviceLog:
        sendAtLine(QStringLiteral("AT+BLEDEVICELOG=%1\r\n").arg(data.toInt()));
        break;
    case DongleCmd::Bomb: {
        const QVariantMap map = data.toMap();
        const QString line = QStringLiteral("AT+BOMB=%1,%2,%3,%4\r\n")
                                 .arg(map.value(QStringLiteral("deviceName")).toString(),
                                      map.value(QStringLiteral("rssi")).toString(),
                                      map.value(QStringLiteral("connectionInterval")).toString(),
                                      map.value(QStringLiteral("command")).toString());
        sendAtLine(line);
        break;
    }
    }
}

void DongleAtDevice::get(DongleCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (cmd) {
    case DongleCmd::GetGmac:
        sendAtLine(QStringLiteral("AT+GMAC\r\n"));
        break;
    }
}

bool DongleAtDevice::sendCustomMessage(const QVariantMap& map) {
    if (map.contains(QStringLiteral("line"))) {
        sendAtLine(map.value(QStringLiteral("line")).toString());
        return true;
    }
    if (map.contains(QStringLiteral("bomb"))) {
        const QVariantMap bomb = map.value(QStringLiteral("bomb")).toMap();
        set(DongleCmd::Bomb, bomb);
        return true;
    }
    const QString atKey = map.value(QStringLiteral("at")).toString();
    if (!atKey.isEmpty()) {
        const QString value = map.value(QStringLiteral("value")).toString();
        const QString line = value.isEmpty() ? QStringLiteral("AT+%1\r\n").arg(atKey)
                                             : QStringLiteral("AT+%1=%2\r\n").arg(atKey, value);
        sendAtLine(line);
        return true;
    }
    qWarning() << "DongleAtDevice sendCustomMessage 缺少 line / at / bomb 参数";
    return false;
}

void DongleAtDevice::sendAtLine(const QString& line) {
    qDebug().noquote() << "AT TX:" << line.trimmed();
    if (!serialPort_ || !serialPort_->isOpen()) {
        qWarning() << "DongleAtDevice: 串口未打开，AT TX 失败";
        return;
    }
    serialPort_->write(line.toLocal8Bit());
}

void DongleAtDevice::registerCommand() {
    using namespace std::placeholders;
    commandList_["AT"] = std::bind(&DongleAtDevice::help, this, _1);
    commandList_["AT+BLERSSI"] = std::bind(&DongleAtDevice::rssi, this, _1);
    commandList_["AT+DONGLEVER"] = std::bind(&DongleAtDevice::dongle_ver, this, _1);
    commandList_["AT+WIFINAME"] = std::bind(&DongleAtDevice::dongle_wifi, this, _1);
    commandList_["AT+CONNECT_SUCCESS"] = std::bind(&DongleAtDevice::connected, this, _1);
    commandList_["AT+DISCONNECT"] = std::bind(&DongleAtDevice::disconnected, this, _1);
    commandList_["AT+WIFIRSSI"] = std::bind(&DongleAtDevice::wifi_rssi, this, _1);
    commandList_["AT+WIFI_CONNECT_SUCCESS"] = std::bind(&DongleAtDevice::WIFI_connected, this, _1);
    commandList_["AT+WIFI_DISCONNECT"] = std::bind(&DongleAtDevice::WIFI_disconnected, this, _1);
    commandList_["AT+WIFI_DATA"] = std::bind(&DongleAtDevice::SEND_WIFI_DATA, this, _1);
    commandList_["AT+WIFIIP"] = std::bind(&DongleAtDevice::SEND_WIFI_IP, this, _1);
    commandList_["ATCH"] = std::bind(&DongleAtDevice::scan_result, this, _1);
    commandList_["AT+DEVICENAME"] = std::bind(&DongleAtDevice::device_name, this, _1);
    commandList_["AT+SUCTION_DATA"] = std::bind(&DongleAtDevice::suction_data, this, _1);
}

void DongleAtDevice::SEND_WIFI_DATA(const QString& p) {
    emitReport(QStringLiteral("ProtocolDongleWifiMsgData"), QVariant::fromValue(ProtocolDongleWifiMsgData{p}));
}

void DongleAtDevice::SEND_WIFI_IP(const QString& p) {
    emitReport(QStringLiteral("ProtocolDongleWifiIpData"), QVariant::fromValue(ProtocolDongleWifiIpData{p}));
}

void DongleAtDevice::WIFI_connected(const QString& p) {
    Q_UNUSED(p);
    iswifiConnected = true;
    emitReport(QStringLiteral("ProtocolDongleWifiStateData"), QVariant::fromValue(ProtocolDongleWifiStateData{1}));
}

void DongleAtDevice::WIFI_disconnected(const QString& p) {
    Q_UNUSED(p);
    iswifiConnected = false;
    emitReport(QStringLiteral("ProtocolDongleWifiStateData"), QVariant::fromValue(ProtocolDongleWifiStateData{0}));
}

void DongleAtDevice::connected(const QString& p) {
    Q_UNUSED(p);
    qDebug() << "at蓝牙连接成功";
    emit sendGetProductResponse(1);
    emitReport(QStringLiteral("ProtocolDongleBleStateData"), QVariant::fromValue(ProtocolDongleBleStateData{1}));
    isConnected = true;
}

void DongleAtDevice::disconnected(const QString& p) {
    Q_UNUSED(p);
    emitReport(QStringLiteral("ProtocolDongleBleStateData"), QVariant::fromValue(ProtocolDongleBleStateData{0}));
    isConnected = false;
    qDebug() << "at蓝牙连接断开";
}

void DongleAtDevice::help(const QString& p) {
    Q_UNUSED(p);
    qDebug() << "this is AT help";
}

void DongleAtDevice::dongle_ver(const QString& p) {
    qDebug() << "dongle版本为=" << p;
    emitReport(QStringLiteral("ProtocolDongleVersionData"), QVariant::fromValue(ProtocolDongleVersionData{p}));
}

void DongleAtDevice::device_name(const QString& p) {
    qDebug() << "dongle设备名称为" << p;
    emitReport(QStringLiteral("ProtocolDongleDeviceNameData"), QVariant::fromValue(ProtocolDongleDeviceNameData{p}));
}
void DongleAtDevice::dongle_wifi(const QString& p) {
    qDebug() << "dongle的wifi为" << p;
    emitReport(QStringLiteral("ProtocolDongleWifiSsidData"), QVariant::fromValue(ProtocolDongleWifiSsidData{p}));
}
void DongleAtDevice::rssi(const QString& p) {
    if (!p.isEmpty() && p.at(0) == QLatin1Char('-'))
        emitReport(QStringLiteral("ProtocolDongleBleRssiData"), QVariant::fromValue(ProtocolDongleBleRssiData{p}));
}

void DongleAtDevice::wifi_rssi(const QString& p) {
    iswifiConnected = true;
    emitReport(QStringLiteral("ProtocolDongleWifiStateData"), QVariant::fromValue(ProtocolDongleWifiStateData{1}));
    emitReport(QStringLiteral("ProtocolDongleWifiRssiData"), QVariant::fromValue(ProtocolDongleWifiRssiData{p}));
}

void DongleAtDevice::scan_result(const QString& p) {
    // 格式: GT 5-777, deviceAddress:c4:27:8c:9d:97:77, deviceRssi:-84
    ProtocolDongleScanResultData data;
    const QStringList parts = p.split(QStringLiteral(","));
    if (parts.size() >= 1) {
        data.deviceName = parts[0].trimmed();
    }
    for (const QString& part : parts) {
        const QString trimmed = part.trimmed();
        if (trimmed.startsWith(QStringLiteral("deviceAddress:"))) {
            data.deviceAddress = trimmed.mid(QStringLiteral("deviceAddress:").length()).trimmed();
        } else if (trimmed.startsWith(QStringLiteral("deviceRssi:"))) {
            data.deviceRssi = trimmed.mid(QStringLiteral("deviceRssi:").length()).trimmed();
        }
    }
    qDebug() << "Dongle scan result:" << data.deviceName << data.deviceAddress << data.deviceRssi;
    emitReport(QStringLiteral("ProtocolDongleScanResultData"), QVariant::fromValue(data));
}

void DongleAtDevice::suction_data(const QString& p) {
    const QStringList parts = p.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return;
    bool okLeft = false;
    bool okRight = false;
    const double left = parts.at(0).toDouble(&okLeft);
    const double right = parts.at(1).toDouble(&okRight);
    if (!okLeft || !okRight)
        return;
    ProtocolDongleSuctionData data;
    data.leftKpa = left;
    data.rightKpa = right;
    emitReport(QStringLiteral("ProtocolDongleSuctionData"), QVariant::fromValue(data));
}
