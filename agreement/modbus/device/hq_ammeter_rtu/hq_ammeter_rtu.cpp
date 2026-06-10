#include "hq_ammeter_rtu.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QByteArray HqAmmeterModbusRtu::buildReadMeasurementRequest() {
    return QByteArray::fromHex(QStringLiteral("01030000000305CB").toLatin1());
}

QByteArray HqAmmeterModbusRtu::buildSetBaud115200Request() {
    return QByteArray::fromHex(QStringLiteral("010600030006F9C8").toLatin1());
}

ModbusRtuCodec::AmmeterReading HqAmmeterModbusRtu::parseResponse(const QByteArray& frame) {
    return ModbusRtuCodec::parseHqHoldRegisterFrame(frame);
}
