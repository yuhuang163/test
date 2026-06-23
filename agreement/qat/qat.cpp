#include "qat.h"

#include <QDebug>
#include <map>

#include "qcoreapplication.h"
#include "qdatetime.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {
bool isPrintableAtLine(const QString& line) {
    if (!line.startsWith(QStringLiteral("AT"))) {
        return false;
    }
    for (const QChar& ch : line) {
        const ushort u = ch.unicode();
        if (u == '\r' || u == '\n' || u == '\t') {
            continue;
        }
        if (u < 0x20 || u > 0x7E) {
            return false;
        }
    }
    return true;
}
} // namespace

Qat::Qat(QSerialPort* parent) : QObject(parent), serialPort(parent) {
    if (!serialPort) {
        qDebug() << "Qat指向一个空指针";
        return;
    }

    registerCommand();
    connect(this, SIGNAL(command(QString, QString)), this, SLOT(processCmd(QString, QString)));
}

void Qat::parseCmd(const QByteArray& byte) {
    int cangonext = 0;
    foreach (char c, byte) {
        dataQueue.push_back((uint8_t)c);
    }

    // QString data = byte;
    // if (data.contains("deviceName")) {
    //     disconnected(data);
    // }

    while (dataQueue.isEmpty() == false) {
        char c = dataQueue.dequeue();

        switch (state) {
        case STATE_IDLE:

            if (c == 'A') {
                cmd += c;
                state = STATE_RECEIVING_T;
            }
            break;

        case STATE_RECEIVING_T:
            if (c == 'T') {
                cmd += c;
                state = STATE_RECEIVING_COMMAND;
            } else {
                cmd.clear();
                state = STATE_IDLE;
            }
            break;

        case STATE_RECEIVING_COMMAND:
            if (c == '\r') {
                cangonext = 1;
            } else if (cangonext && c == '\n') {
                cangonext = 0;
                const QString atLine = parameter.isEmpty() ? cmd + "\r\n" : cmd + "=" + parameter + "\r\n";
                if (isPrintableAtLine(atLine)) {
                    qDebug().noquote() << "AT RX:" << atLine.trimmed();
                    // qDebug() << "发射13"<<cmd;
                    emit command(cmd, parameter);
                }
                // qDebug() << "发射1"<<cmd;
                cmd.clear();
                parameter.clear();
                // qDebug() << "发射23"<<cmd;
                state = STATE_IDLE;
            } else if (c == '=') {
                state = STATE_RECEIVING_PARAMETER;
            } else {
                cmd += c;
                if (cmd.size() > 1024) {
                    cmd.clear();
                    state = STATE_IDLE;
                }
            }
            break;

        case STATE_RECEIVING_PARAMETER:
            if (c == '\r') {
                cangonext = 1;
            } else if (cangonext && c == '\n') {
                cangonext = 0;
                const QString atLine = parameter.isEmpty() ? cmd + "\r\n" : cmd + "=" + parameter + "\r\n";
                if (isPrintableAtLine(atLine)) {
                    qDebug().noquote() << "AT RX:" << atLine.trimmed();
                    emit command(cmd, parameter);
                }
                cmd.clear();
                parameter.clear();
                state = STATE_IDLE;
            } else {
                parameter += c;
                if (parameter.size() > 1024) {
                    parameter.clear();
                    state = STATE_IDLE;
                }
            }
            break;

        default:
            break;
        }
    }
}

void Qat::set(DongleCmd cmd, const QVariant& data) {
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
        qDebug() << "炸弹内容：" << line.trimmed();
        sendAtLine(line);
        break;
    }
    }
}

void Qat::get(DongleCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (cmd) {
    case DongleCmd::GetGmac:
        sendAtLine(QStringLiteral("AT+GMAC\r\n"));
        break;
    }
}

bool Qat::sendCustomMessage(const QVariantMap& map) {
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
    qWarning() << "Qat sendCustomMessage 缺少 line / at / bomb 参数";
    return false;
}

void Qat::sendAtLine(const QString& line) {
    if (!serialPort || !serialPort->isOpen()) {
        qDebug() << "发送AT指令时候，未打开dongle串口，取消发送";
        return;
    }
    const QByteArray data = line.toLocal8Bit();
    qDebug().noquote() << "AT TX:" << line.trimmed();
    serialPort->write(data);
}

void Qat::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
void Qat::registerCommand() {
    // 在这里注册命令即可
    commandList["AT"] = std::bind(&Qat::help, this, std::placeholders::_1);
    commandList["AT+BLERSSI"] = std::bind(&Qat::rssi, this, std::placeholders::_1);
    commandList["AT+DONGLEVER"] = std::bind(&Qat::dongle_ver, this, std::placeholders::_1);
    commandList["AT+WIFINAME"] = std::bind(&Qat::dongle_wifi, this, std::placeholders::_1);
    commandList["AT+CONNECT_SUCCESS"] = std::bind(&Qat::connected, this, std::placeholders::_1);
    commandList["AT+DISCONNECT"] = std::bind(&Qat::disconnected, this, std::placeholders::_1);
    commandList["AT+WIFIRSSI"] = std::bind(&Qat::wifi_rssi, this, std::placeholders::_1);
    commandList["AT+WIFI_CONNECT_SUCCESS"] = std::bind(&Qat::WIFI_connected, this, std::placeholders::_1);
    commandList["AT+WIFI_DISCONNECT"] = std::bind(&Qat::WIFI_disconnected, this, std::placeholders::_1);
    commandList["AT+WIFI_DATA"] = std::bind(&Qat::SEND_WIFI_DATA, this, std::placeholders::_1);
    commandList["AT+WIFIIP"] = std::bind(&Qat::SEND_WIFI_IP, this, std::placeholders::_1);
}

void Qat::SEND_WIFI_DATA(QString p) {
    emitReport(QStringLiteral("ProtocolDongleWifiMsgData"), QVariant::fromValue(ProtocolDongleWifiMsgData{p}));
}

void Qat::SEND_WIFI_IP(QString p) {
    emitReport(QStringLiteral("ProtocolDongleWifiIpData"), QVariant::fromValue(ProtocolDongleWifiIpData{p}));
}

void Qat::WIFI_connected(QString p) {
    Q_UNUSED(p);
    iswifiConnected = true;
    emitReport(QStringLiteral("ProtocolDongleWifiStateData"), QVariant::fromValue(ProtocolDongleWifiStateData{1}));
}

void Qat::WIFI_disconnected(QString p) {
    Q_UNUSED(p);
    iswifiConnected = false;
    emitReport(QStringLiteral("ProtocolDongleWifiStateData"), QVariant::fromValue(ProtocolDongleWifiStateData{0}));
}

void Qat::connected(QString p) {
    Q_UNUSED(p);
    qDebug() << "at蓝牙连接成功";

    emit sendGetProductResponse(1);
    emitReport(QStringLiteral("ProtocolDongleBleStateData"), QVariant::fromValue(ProtocolDongleBleStateData{1}));
    isConnected = true;
}

void Qat::disconnected(QString p) {
    Q_UNUSED(p);
    emitReport(QStringLiteral("ProtocolDongleBleStateData"), QVariant::fromValue(ProtocolDongleBleStateData{0}));
    isConnected = false;
    qDebug() << "at蓝牙连接断开";
}

void Qat::help(QString p) {
    Q_UNUSED(p);
    qDebug() << "this is AT help";
}

void Qat::dongle_ver(QString p) {
    qDebug() << "dongle版本为= " + p;
    emitReport(QStringLiteral("ProtocolDongleVersionData"), QVariant::fromValue(ProtocolDongleVersionData{p}));
}

void Qat::dongle_wifi(QString p) {
    qDebug() << "dongle的wifi为" + p;
    emitReport(QStringLiteral("ProtocolDongleWifiSsidData"), QVariant::fromValue(ProtocolDongleWifiSsidData{p}));
}

void Qat::rssi(QString p) {
    if (!p.isEmpty() && p.at(0) == QLatin1Char('-'))
        emitReport(QStringLiteral("ProtocolDongleBleRssiData"), QVariant::fromValue(ProtocolDongleBleRssiData{p}));
}

void Qat::wifi_rssi(QString p) {
    iswifiConnected = true;
    emitReport(QStringLiteral("ProtocolDongleWifiStateData"), QVariant::fromValue(ProtocolDongleWifiStateData{1}));
    emitReport(QStringLiteral("ProtocolDongleWifiRssiData"), QVariant::fromValue(ProtocolDongleWifiRssiData{p}));
}

void Qat::processCmd(QString cmd, QString parameter) {
    auto it = commandList.find(cmd);
    if (it != commandList.end()) {
        // 调用回调函数
        it->second(parameter);
        // qDebug() << "ccmd为 : " << cmd;
    } else {
        // qDebug() << "不支持的AT指令 : " << cmd;
    }
}
