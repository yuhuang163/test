#include "qfctp.h"
#include <QDebug>
#include <QString>
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

Qfctp::Qfctp(QSerialPort* parent) : QSerialPort(parent) {}

void Qfctp::parseCmd(const QByteArray& byte) {
    (void)byte;
}

void Qfctp::set(DeviceCmd cmd, const QVariant& data) {
    (void)data;
    qWarning() << QString::fromUtf8(u8"Qfctp 暂未实现 set 命令，cmd =") << static_cast<int>(cmd);
}

void Qfctp::get(DeviceCmd cmd, const QVariant& param) {
    (void)param;
    qWarning() << QString::fromUtf8(u8"Qfctp 暂未实现 get 命令，cmd =") << static_cast<int>(cmd);
}
