#include "qat.h"

#include <QDebug>

#include "qcoreapplication.h"
#include "qdatetime.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
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
}  // namespace

Qat::Qat(QSerialPort* parent) : QSerialPort(parent), serialPort(parent) {
    if (!serialPort) {
        qDebug() << "Qat指向一个空指针";
        return;
    }

    registerCommand();
    connect(this, SIGNAL(command(QString, QString)), this, SLOT(processCmd(QString, QString)));
}

void Qat::parseCmd(const QByteArray& byte) {
    int cangonext = 0;
    foreach (char c, byte) { dataQueue.push_back((uint8_t)c); }

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

            default: break;
        }
    }
}

void Qat::sendCmd(QString cmd) {
    if (!serialPort->isOpen()) {
        qDebug() << "发送AT指令时候，未打开dongle串口，取消发送";
        return;
    }
    const QByteArray data = cmd.toLocal8Bit();
    qDebug().noquote() << "AT TX:" << cmd.trimmed();
    serialPort->write(data);
}
void Qat::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
void Qat::sendMac(QString mac) {
    QString s = "AT+MAC=" + mac + "\r\n";
    sendCmd(s);
}
void Qat::sendDcon(QString mac) {
    QString s = "AT+DCON=" + mac + "\r\n";
    sendCmd(s);
}
void Qat::sendMAIN(QString mac) {
    QString s = "AT+MAIN=" + mac + "\r\n";
    sendCmd(s);
}
void Qat::sendBLELOG(int state) {
    if (state > 1)
        state = 1;
    QString s = "AT+BLELOG=" + QString::number(state) + "\r\n";
    sendCmd(s);
}
void Qat::sendBLEDEVICELOG(int state) {
    QString s = "AT+BLEDEVICELOG=" + QString::number(state) + "\r\n";
    sendCmd(s);
}
//使用的时候需要延时1秒，防止和ota数据粘包了
void Qat::sendOTADATA(int state) {
    QString s = "AT+OTADATA=" + QString::number(state) + "\r\n";
    sendCmd(s);
}
void Qat::sendMAINDATA(int state) {
    QString s = "AT+MAINDATA=" + QString::number(state) + "\r\n";
    sendCmd(s);
}

void Qat::sendBOMB(QString devicename, QString rssi, QString connectionInterval, QString command) {
    QString s = "AT+BOMB=" + devicename + "," + rssi + "," + connectionInterval + "," + command + "\r\n";
    qDebug() << "炸弹内容：" << s;
    sendCmd(s);
}
void Qat::ask_mac() {
    QString s = "AT+GMAC\r\n";
    sendCmd(s);
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

void Qat::SEND_WIFI_DATA(QString p) { sendWifiMsg(p); }
void Qat::SEND_WIFI_IP(QString p) { sendWifiIp(p); }

void Qat::sendbleMac(QString mac) {
    QString s = "AT+BLE=" + mac + "\r\n";
    sendCmd(s);
}

void Qat::sendotaMac(QString mac) {
    QString s = "AT+OTA=" + mac + "\r\n";
    sendCmd(s);
}
void Qat::WIFI_connected(QString p) {Q_UNUSED(p);
    iswifiConnected = true;
    // qDebug() << "wifi连接成功";
    emit send_WIFI_state(1);
}
void Qat::WIFI_disconnected(QString p) {Q_UNUSED(p);
    iswifiConnected = false;
    //  qDebug() << "wifi连接断开";
    emit send_WIFI_state(0);
}

void Qat::connected(QString p) {Q_UNUSED(p);
    qDebug() << "at蓝牙连接成功";

    emit sendGetProductResponse(1);
    emit send_ble_state(1);
    isConnected = true;
}
void Qat::disconnected(QString p) {Q_UNUSED(p);
    emit send_ble_state(0);
    isConnected = false;
    qDebug() << "at蓝牙连接断开";
}

void Qat::help(QString p) {Q_UNUSED(p); qDebug() << "this is AT help"; }

void Qat::dongle_ver(QString p) {
    qDebug() << "dongle版本为= " + p;
    emit send_dongle_ver(p);
}
void Qat::dongle_wifi(QString p) {
    qDebug() << "dongle的wifi为" + p;
    emit send_dongle_wifi(p);
}

void Qat::rssi(QString p) {
    // qDebug() << "rssi = " << p;
    if (p.at(0) == "-")
        emit send_rssi(p);
}

void Qat::wifi_rssi(QString p) {
    iswifiConnected = true;
    emit send_WIFI_state(1);
    // qDebug() << "rssi = " << p;
    emit send_wifi_rssi(p);
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
