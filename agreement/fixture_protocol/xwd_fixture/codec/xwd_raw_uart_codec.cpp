#include "xwd_raw_uart_codec.h"

#include "fixture_uart_codec.h"
#include "qmodbus_pdu.h"

#include <QRegularExpression>
#include <QtEndian>
#include <cmath>
#include <cstring>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdRawUartCodec {

QByteArray encodeRawText(const QString& text, bool* parsedAsHex) {
    // 纯十六进制（如 11 11 22）→ hex 字节；含非 hex 字母（如 readonce / readonce\r\n）→ UTF-8 原文
    return FixtureUartCodec::encodeRawOrHexText(text, parsedAsHex);
}

bool parseReadOnceReply(const QString& text, double* ch1Ma, double* ch2Ma, bool* hasCh1, bool* hasCh2) {
    if (ch1Ma)
        *ch1Ma = 0;
    if (ch2Ma)
        *ch2Ma = 0;
    if (hasCh1)
        *hasCh1 = false;
    if (hasCh2)
        *hasCh2 = false;

    static const QRegularExpression re(
        QStringLiteral(R"(CH\s*([12])\s*:\s*([-+]?\d+(?:\.\d+)?)\s*V\s*([-+]?\d+(?:\.\d+)?)\s*mA)"),
        QRegularExpression::CaseInsensitiveOption);

    bool any = false;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int ch = m.captured(1).toInt(&ok);
        if (!ok)
            continue;
        const double ma = m.captured(3).toDouble(&ok);
        if (!ok)
            continue;
        if (ch == 1) {
            if (ch1Ma)
                *ch1Ma = ma;
            if (hasCh1)
                *hasCh1 = true;
        } else if (ch == 2) {
            if (ch2Ma)
                *ch2Ma = ma;
            if (hasCh2)
                *hasCh2 = true;
        }
        any = true;
    }
    return any;
}

QByteArray buildAir1FloatCurrentReadRequest() {
    static const char kReq[] = {'\x88', '\xAE', '\x00', '\x11'};
    return QByteArray(kReq, int(sizeof(kReq)));
}

bool parseAir1FloatCurrentReply(const QByteArray& buf, double* amperesOut, int* frameOffset) {
    if (amperesOut)
        *amperesOut = 0;
    if (frameOffset)
        *frameOffset = -1;
    // FA FB | status | float_be(4) | trailer  → 8 字节
    constexpr int kFrameLen = 8;
    for (int i = 0; i + kFrameLen <= buf.size(); ++i) {
        if (static_cast<quint8>(buf.at(i)) != 0xFA || static_cast<quint8>(buf.at(i + 1)) != 0xFB)
            continue;
        const quint32 hostBits =
            qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(buf.constData() + i + 3));
        float ampsF = 0.f;
        static_assert(sizeof(float) == 4, "IEEE754 float");
        std::memcpy(&ampsF, &hostBits, sizeof(ampsF));
        if (!std::isfinite(ampsF))
            continue;
        if (amperesOut)
            *amperesOut = static_cast<double>(ampsF);
        if (frameOffset)
            *frameOffset = i;
        return true;
    }
    return false;
}

QByteArray buildAir1ModbusVoltageReadRequest(quint8 slaveAddr, quint16 registerAddress) {
    QByteArray body;
    body.append(char(slaveAddr));
    body.append(char(0x03));
    QModbusPdu::appendUint16Be(body, registerAddress);
    QModbusPdu::appendUint16Be(body, 1);
    const quint16 crcBe = QModbusPdu::crc16ModbusRtuBigEndian(body);
    body.append(char(quint8(crcBe >> 8)));
    body.append(char(quint8(crcBe & 0xFF)));
    return body;
}

bool parseAir1ModbusVoltageReply(const QByteArray& buf, quint8 slaveAddr, double* voltsOut,
                                 int* frameOffset) {
    if (voltsOut)
        *voltsOut = 0;
    if (frameOffset)
        *frameOffset = -1;
    // 从站 + FC03 + byteCount=2 + data(2) + CRC(2) = 7 字节
    constexpr int kFrameLen = 7;
    for (int i = 0; i + kFrameLen <= buf.size(); ++i) {
        const QByteArray cand = buf.mid(i, kFrameLen);
        quint8 byteCount = 0;
        if (!QModbusPdu::validateRtuFrame(cand, &byteCount))
            continue;
        if (byteCount != 2)
            continue;
        if (static_cast<quint8>(cand.at(0)) != slaveAddr)
            continue;
        if (static_cast<quint8>(cand.at(1)) != 0x03)
            continue;
        const quint16 raw = QModbusPdu::readUint16Be(cand.constData() + 3);
        if (voltsOut)
            *voltsOut = static_cast<double>(raw) / 100.0;
        if (frameOffset)
            *frameOffset = i;
        return true;
    }
    return false;
}

} // namespace XwdRawUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
