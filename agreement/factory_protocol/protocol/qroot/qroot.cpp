#include "qroot.h"

#include <QDebug>
#include <QVariantMap>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr quint8 kSop0 = 0xAA;
constexpr quint8 kSop1 = 0x55;
constexpr int kHeaderSize = 5; // SOP(2)+CT+CID+CAL

} // namespace

Qroot::Qroot(QSerialPort* port) : qProtocol(port), serialPort_(port) {
}

quint8 Qroot::checksum8(const QByteArray& data) {
    quint32 sum = 0;
    for (const char ch : data)
        sum += static_cast<quint8>(ch);
    return static_cast<quint8>(~sum);
}

QByteArray Qroot::buildPacket(quint8 ct, quint8 cid, const QByteArray& body) {
    QByteArray packet;
    packet.reserve(kHeaderSize + body.size() + 1);
    packet.append(static_cast<char>(kSop0));
    packet.append(static_cast<char>(kSop1));
    packet.append(static_cast<char>(ct));
    packet.append(static_cast<char>(cid));
    packet.append(static_cast<char>(body.size()));
    if (!body.isEmpty())
        packet.append(body);
    packet.append(static_cast<char>(checksum8(packet)));
    return packet;
}

bool Qroot::sendPacket(quint8 ct, quint8 cid, const QByteArray& body) {
    if (!serialPort_ || !serialPort_->isOpen()) {
        qWarning() << "[Qroot] serial not open";
        return false;
    }
    const QByteArray frame = buildPacket(ct, cid, body);
    const qint64 n = serialPort_->write(frame);
    if (!serialPort_->waitForBytesWritten(3000) || n != frame.size()) {
        qWarning() << "[Qroot] write failed";
        return false;
    }
    qDebug().noquote() << "[Qroot] TX:" << frame.toHex(' ').toUpper();
    if (ct == Req)
        pendingCid_ = cid;
    hasPending_ = (ct == Req);
    return true;
}

QString Qroot::formatMacFromWire(const QByteArray& mac6) {
    if (mac6.size() != 6)
        return {};
    QStringList parts;
    parts.reserve(6);
    for (int i = 5; i >= 0; --i)
        parts.append(QString::number(static_cast<quint8>(mac6.at(i)), 16).rightJustified(2, QLatin1Char('0')).toUpper());
    return parts.join(QLatin1Char(':'));
}

QString Qroot::formatSoftVersion(quint8 raw) {
    return QStringLiteral("%1.%2").arg(raw / 10).arg(raw % 10);
}

QByteArray Qroot::parseMacToWire(const QVariant& data) {
    if (data.canConvert<QByteArray>()) {
        const QByteArray bytes = data.toByteArray();
        if (bytes.size() == 6)
            return bytes;
    }
    QString text = data.toString().trimmed();
    if (text.isEmpty() && data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        text = map.value(QStringLiteral("mac")).toString().trimmed();
        if (text.isEmpty())
            text = map.value(QStringLiteral("value")).toString().trimmed();
    }
    text.remove(QLatin1Char(':'));
    text.remove(QLatin1Char('-'));
    text.remove(QLatin1Char(' '));
    if (text.size() % 2 != 0)
        text.prepend(QLatin1Char('0'));
    const QByteArray hex = QByteArray::fromHex(text.toLatin1());
    if (hex.size() != 6)
        return {};
    QByteArray wire;
    wire.resize(6);
    for (int i = 0; i < 6; ++i)
        wire[i] = hex.at(5 - i);
    return wire;
}

quint8 Qroot::parseOnOffParam(const QVariant& data, quint8 defaultValue) {
    if (!data.isValid() || data.isNull())
        return defaultValue;
    if (data.canConvert<int>())
        return data.toInt() != 0 ? 1 : 0;
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(QStringLiteral("value")))
            return map.value(QStringLiteral("value")).toInt() != 0 ? 1 : 0;
        if (map.contains(QStringLiteral("switch")))
            return map.value(QStringLiteral("switch")).toInt() != 0 ? 1 : 0;
    }
    return defaultValue;
}

void Qroot::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty())
        return;
    rxBuffer_.append(byte);
    drainRxBuffer();
}

void Qroot::drainRxBuffer() {
    while (rxBuffer_.size() >= kHeaderSize + 1) {
        const int start = rxBuffer_.indexOf(QByteArray::fromHex("AA55"));
        if (start < 0) {
            if (rxBuffer_.size() > 512)
                rxBuffer_.clear();
            return;
        }
        if (start > 0)
            rxBuffer_.remove(0, start);
        if (rxBuffer_.size() < 2)
            return;
        if (rxBuffer_.size() < kHeaderSize)
            return;
        const quint8 cal = static_cast<quint8>(rxBuffer_.at(4));
        const int frameLen = kHeaderSize + cal + 1;
        if (rxBuffer_.size() < frameLen)
            return;
        const QByteArray frame = rxBuffer_.left(frameLen);
        rxBuffer_.remove(0, frameLen);
        const quint8 expectCs = static_cast<quint8>(frame.at(frameLen - 1));
        const quint8 actualCs = checksum8(frame.left(frameLen - 1));
        if (expectCs != actualCs) {
            qWarning() << "[Qroot] checksum mismatch";
            continue;
        }
        const quint8 ct = static_cast<quint8>(frame.at(2));
        const quint8 cid = static_cast<quint8>(frame.at(3));
        const QByteArray body = frame.mid(kHeaderSize, cal);
        qDebug().noquote() << "[Qroot] RX:" << frame.toHex(' ').toUpper();
        handleFrame(ct, cid, body);
    }
}

void Qroot::handleFrame(quint8 ct, quint8 cid, const QByteArray& body) {
    if (ct == Nack) {
        emit sendGetProductResponse(0);
        hasPending_ = false;
        ProtocolResultData result;
        result.result = 0;
        emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        return;
    }

    if (ct == Notify) {
        if (cid == KeyNotify && body.size() >= 1) {
            ProtocolButtonStateData btn;
            btn.keyButtonId = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
            return;
        }
        if (cid == ToggleKeyNotify && body.size() >= 1) {
            ProtocolButtonStateData btn;
            btn.modeButtonState = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
            return;
        }
        if (cid == FlangeStatus && body.size() >= 1) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == NtcStatus && body.size() >= 2) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0)) | (static_cast<quint8>(body.at(1)) << 8);
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == HeatTemp && body.size() >= 1) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == VibStatus && body.size() >= 1) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == PumpTestEnter && body.size() >= 1) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == PumpTestExit && body.size() >= 1) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        return;
    }

    if (ct != Ack)
        return;

    if (hasPending_ && cid != pendingCid_)
        return;

    switch (cid) {
    case TestMode:
    case LedTest:
    case Vibration:
    case MacWrite:
    case PumpTestEnter:
    case PumpTestExit:
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case SoftVersion:
        if (body.size() >= 1) {
            ProtocolBaseInfoData info;
            info.soft_version = formatSoftVersion(static_cast<quint8>(body.at(0)));
            emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case MacRead:
        if (body.size() >= 6) {
            ProtocolMacData mac;
            mac.mac = formatMacFromWire(body.left(6));
            emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(mac));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case FlangeStatus:
    case NtcStatus:
    case HeatTemp:
    case VibStatus:
        if (body.size() >= 1) {
            ProtocolTypeData status;
            if (cid == NtcStatus && body.size() >= 2)
                status.type = static_cast<quint8>(body.at(0)) | (static_cast<quint8>(body.at(1)) << 8);
            else
                status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    default:
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
}

void Qroot::sendTestMode(quint8 mode) {
    sendPacket(Req, TestMode, QByteArray(1, static_cast<char>(mode)));
}

void Qroot::sendLedTest(quint8 mode) {
    sendPacket(Req, LedTest, QByteArray(1, static_cast<char>(mode)));
}

void Qroot::sendVibration(quint8 mode) {
    sendPacket(Req, Vibration, QByteArray(1, static_cast<char>(mode)));
}

void Qroot::sendMacWrite(const QByteArray& mac6) {
    if (mac6.size() != 6) {
        qWarning() << "[Qroot] invalid MAC length";
        return;
    }
    sendPacket(Req, MacWrite, mac6);
}

void Qroot::sendQuery(CommandId cid) {
    // 文档中 0x96~0x9E 查询使用 Notify 类型 + 0x01 参数
    sendPacket(Notify, static_cast<quint8>(cid), QByteArray(1, '\x01'));
}

void Qroot::set(DeviceCmd cmd, const QVariant& data) {
    switch (cmd) {
    case DeviceCmd::FacMode: {
        const quint8 mode = parseOnOffParam(data, 1);
        sendTestMode(mode);
        return;
    }
    case DeviceCmd::LedTest:
    case DeviceCmd::LedColor:
        sendLedTest(parseOnOffParam(data, 1));
        return;
    case DeviceCmd::MotorTestState:
    case DeviceCmd::RootVibration:
        sendVibration(parseOnOffParam(data, 1));
        return;
    case DeviceCmd::MacWrite: {
        const QByteArray mac6 = parseMacToWire(data);
        sendMacWrite(mac6);
        return;
    }
    case DeviceCmd::RootPumpTestEnter:
        sendPacket(Notify, PumpTestEnter, QByteArray(1, '\x01'));
        return;
    case DeviceCmd::RootPumpTestExit:
        sendPacket(Notify, PumpTestExit, QByteArray(1, '\x01'));
        return;
    default:
        qWarning() << "[Qroot] unsupported set cmd" << static_cast<int>(cmd);
        break;
    }
}

void Qroot::get(DeviceCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (cmd) {
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
        sendPacket(Req, SoftVersion, {});
        return;
    case DeviceCmd::MacRead:
        sendPacket(Req, MacRead, {});
        return;
    case DeviceCmd::RootFlangeQuery:
        sendQuery(FlangeStatus);
        return;
    case DeviceCmd::RootNtcQuery:
        sendQuery(NtcStatus);
        return;
    case DeviceCmd::RootHeatTempQuery:
        sendQuery(HeatTemp);
        return;
    case DeviceCmd::RootVibStatusQuery:
        sendQuery(VibStatus);
        return;
    default:
        qWarning() << "[Qroot] unsupported get cmd" << static_cast<int>(cmd);
        break;
    }
}

bool Qroot::sendCustomMessage(const QVariantMap& map) {
    const QVariant ctVar = map.value(QStringLiteral("ct"));
    const QVariant cidVar = map.value(QStringLiteral("cid"));
    if (!cidVar.isValid())
        return false;
    const quint8 ct = ctVar.isValid() ? static_cast<quint8>(ctVar.toUInt()) : Req;
    const quint8 cid = static_cast<quint8>(cidVar.toUInt());
    QByteArray body;
    if (map.contains(QStringLiteral("body")))
        body = map.value(QStringLiteral("body")).toByteArray();
    else if (map.contains(QStringLiteral("value")))
        body = QByteArray(1, static_cast<char>(map.value(QStringLiteral("value")).toUInt()));
    return sendPacket(ct, cid, body);
}
