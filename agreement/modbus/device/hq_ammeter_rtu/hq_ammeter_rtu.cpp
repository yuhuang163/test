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

ModbusRtuCodec::AmmeterReading HqAmmeterModbusRtu::parseResponseFrame(const QByteArray& frame) {
    return ModbusRtuCodec::parseHqHoldRegisterFrame(frame);
}

// --- IModbusRtuDevice interface bridge ---

QByteArray HqAmmeterModbusRtu::buildRequest(int cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (static_cast<HqAmmeterRtuCmd>(cmd)) {
    case HqAmmeterRtuCmd::ReadMeasurement:
        return buildReadMeasurementRequest();
    case HqAmmeterRtuCmd::SetBaud115200:
        return buildSetBaud115200Request();
    default:
        return {};
    }
}

bool HqAmmeterModbusRtu::parseResponse(const QByteArray& frame, QString* valueText) {
    const auto reading = parseResponseFrame(frame);
    if (!reading.ok)
        return false;
    if (valueText)
        *valueText = reading.valueText;
    return true;
}
