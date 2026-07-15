#include "asd9026a_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Asd9026aCodec {

quint16 crc16Modbus(const QByteArray& data) {
    quint16 crc = 0xFFFF;
    for (const char byte : data) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            if (crc & 0x0001)
                crc = (crc >> 1) ^ 0xA001;
            else
                crc >>= 1;
        }
    }
    return crc;
}

QByteArray buildFrame(quint8 moduleAddr, quint8 funcCode, quint8 cmdCode, const QByteArray& payload) {
    QByteArray body;
    body.reserve(4 + payload.size() + 2);
    body.append(static_cast<char>(moduleAddr));
    body.append(static_cast<char>(funcCode));
    body.append(static_cast<char>(cmdCode));
    body.append(static_cast<char>(payload.size()));
    if (!payload.isEmpty())
        body.append(payload);

    const quint16 crc = crc16Modbus(body);
    body.append(static_cast<char>(crc & 0xFF));
    body.append(static_cast<char>((crc >> 8) & 0xFF));
    return body;
}

bool parseFrame(const QByteArray& raw, quint8* moduleAddr, quint8* funcCode, quint8* cmdCode, QByteArray* payload,
                QString* errorMessage) {
    if (raw.size() < 6) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 帧长度不足");
        return false;
    }

    const QByteArray body = raw.left(raw.size() - 2);
    const quint16 recvCrc = static_cast<quint8>(raw.at(raw.size() - 2)) | (static_cast<quint8>(raw.at(raw.size() - 1)) << 8);
    const quint16 calcCrc = crc16Modbus(body);
    if (recvCrc != calcCrc) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A CRC 校验失败");
        return false;
    }

    const int payloadLen = static_cast<quint8>(body.at(3));
    if (body.size() != 4 + payloadLen) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 数据长度与帧不匹配");
        return false;
    }

    if (moduleAddr)
        *moduleAddr = static_cast<quint8>(body.at(0));
    if (funcCode)
        *funcCode = static_cast<quint8>(body.at(1));
    if (cmdCode)
        *cmdCode = static_cast<quint8>(body.at(2));
    if (payload)
        *payload = body.mid(4, payloadLen);
    return true;
}

QByteArray appendLe32(quint32 value) {
    QByteArray out;
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
    return out;
}

quint32 readLe32(const QByteArray& data, int offset) {
    if (offset + 4 > data.size())
        return 0;
    return static_cast<quint8>(data.at(offset)) | (static_cast<quint8>(data.at(offset + 1)) << 8)
           | (static_cast<quint8>(data.at(offset + 2)) << 16) | (static_cast<quint8>(data.at(offset + 3)) << 24);
}

quint32 readBe32(const QByteArray& data, int offset) {
    if (offset + 4 > data.size())
        return 0;
    return (static_cast<quint32>(static_cast<quint8>(data.at(offset))) << 24)
           | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 1))) << 16)
           | (static_cast<quint32>(static_cast<quint8>(data.at(offset + 2))) << 8)
           | static_cast<quint32>(static_cast<quint8>(data.at(offset + 3)));
}

} // namespace Asd9026aCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
