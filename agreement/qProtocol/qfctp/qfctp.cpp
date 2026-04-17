#include "qfctp.h"
#include <QDebug>

Qfctp::Qfctp(QSerialPort* parent) : QSerialPort(parent) {}

void Qfctp::parseCmd(const QByteArray& byte) {
    (void)byte;
}

void Qfctp::set(DeviceCmd cmd, const QVariant& data) {
    (void)data;
    qWarning() << "Qfctp 暂未实现 set 命令，cmd =" << static_cast<int>(cmd);
}

void Qfctp::get(DeviceCmd cmd, const QVariant& param) {
    (void)param;
    qWarning() << "Qfctp 暂未实现 get 命令，cmd =" << static_cast<int>(cmd);
}
