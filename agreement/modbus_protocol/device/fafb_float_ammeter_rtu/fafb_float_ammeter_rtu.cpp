#include "fafb_float_ammeter_rtu.h"

#include "qmodbus_pdu.h"

#include <QtEndian>
#include <cmath>
#include <cstring>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QByteArray fafbAppendRtuCrc(const QByteArray& body) {
    QByteArray frame = body;
    const quint16 crcBe = QModbusPdu::crc16ModbusRtuBigEndian(body);
    // CRC low byte first on wire (crc16ModbusRtuBigEndian high byte = CRC_L)
    frame.append(char(quint8(crcBe >> 8)));
    frame.append(char(quint8(crcBe & 0xFF)));
    return frame;
}

static quint8 fafbSlaveAddrFromParam(const QVariant& param, quint8 fallback = 1) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("slaveAddr")))
        return static_cast<quint8>(qBound(0, map.value(QStringLiteral("slaveAddr")).toInt(), 247));
    return fallback;
}

static quint16 fafbRegisterFromParam(const QVariant& param, quint16 fallback = 1) {
    if (!param.canConvert<QVariantMap>())
        return fallback;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("registerAddress")))
        return static_cast<quint16>(qBound(0, map.value(QStringLiteral("registerAddress")).toInt(), 65535));
    if (map.contains(QStringLiteral("register")))
        return static_cast<quint16>(qBound(0, map.value(QStringLiteral("register")).toInt(), 65535));
    return fallback;
}

QByteArray FaFbFloatAmmeterRtu::buildReadMeasurementRequest() {
    static const char kReq[] = {'\x88', '\xAE', '\x00', '\x11'};
    return QByteArray(kReq, int(sizeof(kReq)));
}

QByteArray FaFbFloatAmmeterRtu::buildReadVoltageRequest(quint8 slaveAddr, quint16 registerAddress) {
    QByteArray body;
    body.append(char(slaveAddr));
    body.append(char(0x03));
    QModbusPdu::appendUint16Be(body, registerAddress);
    QModbusPdu::appendUint16Be(body, 1);
    return fafbAppendRtuCrc(body);
}

bool FaFbFloatAmmeterRtu::parseCurrentResponseFrame(const QByteArray& buf, QString* valueText, int* frameOffset) {
    if (valueText)
        valueText->clear();
    if (frameOffset)
        *frameOffset = -1;
    // FA FB | status | float_be(4) | trailer = 8 bytes
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
        if (valueText)
            *valueText = QString::number(static_cast<double>(ampsF) * 1000.0, 'f', 2);
        if (frameOffset)
            *frameOffset = i;
        return true;
    }
    return false;
}

bool FaFbFloatAmmeterRtu::parseVoltageResponseFrame(const QByteArray& buf, quint8 slaveAddr, QString* valueText,
                                                    int* frameOffset) {
    if (valueText)
        valueText->clear();
    if (frameOffset)
        *frameOffset = -1;
    // slave + FC03 + byteCount=2 + data(2) + CRC(2) = 7 bytes
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
        if (valueText)
            *valueText = QString::number(static_cast<double>(raw) / 100.0, 'f', 2);
        if (frameOffset)
            *frameOffset = i;
        return true;
    }
    return false;
}

QByteArray FaFbFloatAmmeterRtu::buildRequest(int cmd, const QVariant& param) {
    pendingCmd_ = static_cast<FaFbFloatAmmeterRtuCmd>(cmd);
    pendingSlaveAddr_ = fafbSlaveAddrFromParam(param, 1);
    switch (pendingCmd_) {
    case FaFbFloatAmmeterRtuCmd::ReadMeasurement:
        return buildReadMeasurementRequest();
    case FaFbFloatAmmeterRtuCmd::ReadVoltageCentivolt:
        return buildReadVoltageRequest(pendingSlaveAddr_, fafbRegisterFromParam(param, 1));
    default:
        return {};
    }
}

bool FaFbFloatAmmeterRtu::parseResponse(const QByteArray& frame, QString* valueText) {
    if (pendingCmd_ == FaFbFloatAmmeterRtuCmd::ReadVoltageCentivolt)
        return parseVoltageResponseFrame(frame, pendingSlaveAddr_, valueText);
    return parseCurrentResponseFrame(frame, valueText);
}

void FaFbFloatAmmeterRtu::resetRxState() {
    rtuRxBuffer_.reset();
}

bool FaFbFloatAmmeterRtu::feedRx(const QByteArray& chunk, QString* valueText) {
    if (chunk.isEmpty())
        return false;
    rtuRxBuffer_.feed(chunk);
    QByteArray buf = rtuRxBuffer_.buffer();

    if (pendingCmd_ == FaFbFloatAmmeterRtuCmd::ReadVoltageCentivolt) {
        if (parseVoltageResponseFrame(buf, pendingSlaveAddr_, valueText)) {
            rtuRxBuffer_.reset();
            return true;
        }
        if (buf.size() > 64) {
            rtuRxBuffer_.reset();
            rtuRxBuffer_.feed(buf.right(32));
        }
        return false;
    }

    constexpr int kFrameLen = 8;
    while (buf.size() >= 2) {
        int idx = -1;
        for (int i = 0; i + 1 < buf.size(); ++i) {
            if (static_cast<quint8>(buf.at(i)) == 0xFA && static_cast<quint8>(buf.at(i + 1)) == 0xFB) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            if (static_cast<quint8>(buf.at(buf.size() - 1)) == 0xFA) {
                rtuRxBuffer_.reset();
                rtuRxBuffer_.feed(QByteArray(1, char(0xFA)));
            } else {
                rtuRxBuffer_.reset();
            }
            return false;
        }
        if (idx > 0) {
            buf.remove(0, idx);
            rtuRxBuffer_.reset();
            rtuRxBuffer_.feed(buf);
        }
        if (buf.size() < kFrameLen)
            return false;
        int frameOff = -1;
        if (parseCurrentResponseFrame(buf.left(kFrameLen), valueText, &frameOff) && frameOff == 0) {
            rtuRxBuffer_.reset();
            return true;
        }
        buf.remove(0, 2);
        rtuRxBuffer_.reset();
        rtuRxBuffer_.feed(buf);
    }
    return false;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
