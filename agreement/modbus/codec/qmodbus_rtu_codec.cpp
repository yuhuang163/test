#include "qmodbus_rtu_codec.h"

#include "qmodbus_pdu.h"

#include <QtGlobal>
#include <cmath>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace ModbusRtuCodec {

namespace {
constexpr int kReg0HighIndex = 3;
constexpr int kReg0LowIndex = 4;
constexpr int kReg1HighIndex = 5;
constexpr int kReg1LowIndex = 6;
constexpr int kUnitExponentIndex = 8;
} // namespace

quint16 readBigEndianU16(const QByteArray& buffer, int highIndex, int lowIndex) {
    return (static_cast<quint8>(buffer.at(highIndex)) << 8) | static_cast<quint8>(buffer.at(lowIndex));
}

AmmeterReading parseHqHoldRegisterFrame(const QByteArray& frame) {
    AmmeterReading result;
    quint8 byteCount = 0;
    if (!QModbusPdu::validateRtuFrame(frame, &byteCount)) {
        return result;
    }
    if (frame.size() <= kUnitExponentIndex) {
        return result;
    }
    const quint16 reg0 = readBigEndianU16(frame, kReg0HighIndex, kReg0LowIndex);
    const quint16 reg1 = readBigEndianU16(frame, kReg1HighIndex, kReg1LowIndex);
    const qint8 unitExponent = static_cast<qint8>(frame.at(kUnitExponentIndex));
    const qint32 currentValue = (reg0 << 16) | reg1;
    const double currentA = currentValue * std::pow(10, unitExponent) * 1e-9;
    result.ok = true;
    result.valueText = QString::number(currentA);
    return result;
}

AmmeterReading parseLxHoldRegisterFrame(const QByteArray& frame) {
    AmmeterReading result;
    quint8 byteCount = 0;
    if (!QModbusPdu::validateRtuFrame(frame, &byteCount)) {
        return result;
    }
    const quint16 reg0 = readBigEndianU16(frame, kReg0HighIndex, kReg0LowIndex);
    const quint16 reg1 = readBigEndianU16(frame, kReg1HighIndex, kReg1LowIndex);
    const qint32 currentValue = (reg0 << 16) | reg1;
    result.ok = true;
    result.valueText = QString::number(static_cast<double>(currentValue));
    return result;
}

} // namespace ModbusRtuCodec
