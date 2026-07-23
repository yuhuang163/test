#include "multi_temp_logger_rtu.h"

#include "common_utils.h"
#include "qmodbus_pdu.h"

#include <QtEndian>
#include <cstring>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr int kRegsPerChannel = 20;
constexpr int kTempRegOffsetInChannel = 18; // 通道内温度低字寄存器相对偏移

QByteArray appendRtuCrc(const QByteArray& body) {
    QByteArray frame = body;
    const quint16 crcBe = QModbusPdu::crc16ModbusRtuBigEndian(body);
    // 协议：CRC 低字节在前；crc16ModbusRtuBigEndian 返回值高字节=CRC_L
    frame.append(char(quint8(crcBe >> 8)));
    frame.append(char(quint8(crcBe & 0xFF)));
    return frame;
}

QString txHexFromParam(const QVariant& param) {
    if (param.userType() == QMetaType::QString)
        return param.toString().trimmed();
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        if (map.contains(QStringLiteral("txHex")))
            return map.value(QStringLiteral("txHex")).toString().trimmed();
        if (map.contains(QStringLiteral("string")))
            return map.value(QStringLiteral("string")).toString().trimmed();
        if (map.size() == 1)
            return map.constBegin().value().toString().trimmed();
    }
    return QString();
}

int slaveAddrFromParam(const QVariant& param, int fallback = 1) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("slaveAddr")))
        return qBound(1, map.value(QStringLiteral("slaveAddr")).toInt(), 247);
    if (map.contains(QStringLiteral("addr")))
        return qBound(1, map.value(QStringLiteral("addr")).toInt(), 247);
    return fallback;
}

int channelFromParam(const QVariant& param, int fallback = 1) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("channel")))
        return qBound(1, map.value(QStringLiteral("channel")).toInt(), 64);
    return fallback;
}

} // namespace

QByteArray MultiTempLoggerModbusRtu::buildReadChannelTempRequest(int slaveAddr, int channel1Based) {
    const int channel = qBound(1, channel1Based, 64);
    const int addr = qBound(1, slaveAddr, 247);
    const quint16 startReg = static_cast<quint16>((channel - 1) * kRegsPerChannel + kTempRegOffsetInChannel);
    QByteArray body;
    body.append(char(quint8(addr)));
    body.append(char(0x03));
    QModbusPdu::appendUint16Be(body, startReg);
    QModbusPdu::appendUint16Be(body, 2);
    return appendRtuCrc(body);
}

QByteArray MultiTempLoggerModbusRtu::buildSendRawRequest(const QVariant& param) {
    const QString hex = txHexFromParam(param);
    if (hex.isEmpty())
        return {};
    bool ok = false;
    const QByteArray frame = CommonUtils::fromHexString(hex, &ok);
    return ok ? frame : QByteArray();
}

bool MultiTempLoggerModbusRtu::parseTemperatureFrame(const QByteArray& frame, double* outCelsius,
                                                     QString* valueText) {
    quint8 byteCount = 0;
    if (!QModbusPdu::validateRtuFrame(frame, &byteCount))
        return false;
    if (frame.size() < 3)
        return false;
    const quint8 fc = static_cast<quint8>(frame.at(1));
    if (fc & 0x80)
        return false;

    // 读保持寄存器且至少 4 字节：寄存器低字 + 高字 → IEEE754（文档示例 42 F6 E6 66 = 123.45）
    if (fc == 0x03 && byteCount >= 4 && frame.size() >= 7) {
        const quint16 lowWord = QModbusPdu::readUint16Be(frame.constData() + 3);
        const quint16 highWord = QModbusPdu::readUint16Be(frame.constData() + 5);
        const quint32 beBits = (static_cast<quint32>(highWord) << 16) | lowWord;
        const quint32 hostBits = qFromBigEndian(beBits);
        float temp = 0.0f;
        static_assert(sizeof(float) == 4, "float must be 4 bytes");
        std::memcpy(&temp, &hostBits, sizeof(temp));
        if (outCelsius)
            *outCelsius = static_cast<double>(temp);
        if (valueText)
            *valueText = QString::number(static_cast<double>(temp), 'f', 2);
        return true;
    }

    if (valueText)
        *valueText = CommonUtils::toHexUpperSpaced(frame);
    return true;
}

QByteArray MultiTempLoggerModbusRtu::buildRequest(int cmd, const QVariant& param) {
    switch (static_cast<MultiTempLoggerRtuCmd>(cmd)) {
    case MultiTempLoggerRtuCmd::ReadChannelTemp:
        return buildReadChannelTempRequest(slaveAddrFromParam(param), channelFromParam(param));
    case MultiTempLoggerRtuCmd::SendRaw:
        return buildSendRawRequest(param);
    default:
        return {};
    }
}

bool MultiTempLoggerModbusRtu::parseResponse(const QByteArray& frame, QString* valueText) {
    return parseTemperatureFrame(frame, nullptr, valueText);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
