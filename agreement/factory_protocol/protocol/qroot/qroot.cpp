#include "qroot.h"

#include "common_utils.h"

#include <QDebug>
#include <QVariantMap>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr quint8 kSop0 = 0xAA;
constexpr quint8 kSop1 = 0x55;
constexpr int kHeaderSize = 5; // SOP(2)+CT+CID+CAL
constexpr quint8 kPhyTxHeaderByte = 0xCC;
constexpr quint8 kPhyRxHeaderByte = 0xAA;
constexpr int kPhyHeaderSize = 8;
constexpr quint8 kPhyChannelFac = 1;
/** 0x9A 开/关按键上报的 Ack body：0xFF=已接收（旧固件曾为 0x00） */
constexpr quint8 kKeyNotifySwitchAck = 0xFF;
/** 内层 body 合理上限；串口噪声偶发伪帧头 + 超大 CAL 会卡住后续合法应答 */
constexpr int kMaxFrameBodyLen = 64;

} // namespace

QByteArray Qroot::wrapPhyPacket(const QByteArray& innerPacket) {
    if (innerPacket.isEmpty() || innerPacket.size() > 0xFF)
        return {};
    QByteArray phy;
    phy.reserve(kPhyHeaderSize + 2 + innerPacket.size());
    phy.append(QByteArray(kPhyHeaderSize, static_cast<char>(kPhyTxHeaderByte)));
    phy.append(static_cast<char>(innerPacket.size()));
    phy.append(static_cast<char>(kPhyChannelFac));
    phy.append(innerPacket);
    return phy;
}

void Qroot::feedPhyRx(const QByteArray& data, QList<QByteArray>& outInnerPackets) {
    const auto resetPhy = [this]() {
        phyState_ = PhyIdle;
        phyHeaderHits_ = 0;
        phyExpectedLen_ = 0;
        phyChannel_ = 0;
        phyPayload_.clear();
    };

    for (const char ch : data) {
        const quint8 x = static_cast<quint8>(ch);
        switch (phyState_) {
        case PhyIdle:
            if (x == kPhyRxHeaderByte) {
                phyHeaderHits_ = 1;
                phyState_ = PhyHeader;
            }
            break;
        case PhyHeader:
            if (x == kPhyRxHeaderByte) {
                if (++phyHeaderHits_ == kPhyHeaderSize)
                    phyState_ = PhyChannel;
            } else {
                resetPhy();
            }
            break;
        case PhyChannel:
            phyChannel_ = x;
            phyState_ = PhyLen;
            break;
        case PhyLen:
            phyExpectedLen_ = static_cast<int>(x);
            if (phyExpectedLen_ <= 0) {
                qWarning() << "[Qroot] dongle 外层包长度非法:" << phyExpectedLen_;
                resetPhy();
                break;
            }
            phyPayload_.clear();
            phyPayload_.reserve(phyExpectedLen_);
            phyState_ = PhyPayload;
            break;
        case PhyPayload:
            phyPayload_.append(static_cast<char>(x));
            if (phyPayload_.size() >= phyExpectedLen_) {
                if (phyChannel_ == kPhyChannelFac)
                    outInnerPackets.append(phyPayload_);
                else
                    qWarning() << "[Qroot] dongle 通道异常 channel=" << phyChannel_;
                resetPhy();
            }
            break;
        default:
            resetPhy();
            break;
        }
    }
}

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
    const QByteArray phyPacket = wrapPhyPacket(frame);
    if (phyPacket.isEmpty()) {
        qWarning() << "[Qroot] dongle 封包失败";
        return false;
    }
    const qint64 n = serialPort_->write(phyPacket);
    if (!serialPort_->waitForBytesWritten(3000) || n != phyPacket.size()) {
        qWarning() << "[Qroot] write failed";
        return false;
    }
    qDebug().noquote() << "[Qroot] TX:" << phyPacket.toHex(' ').toUpper()
                       << "inner:" << frame.toHex(' ').toUpper();
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

QString Qroot::formatSoftVersion(const QByteArray& body) {
    if (body.size() >= 2)
        return QStringLiteral("%1.%2").arg(static_cast<quint8>(body.at(0))).arg(static_cast<quint8>(body.at(1)));
    if (body.size() >= 1)
        return QString::number(static_cast<quint8>(body.at(0)));
    return {};
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

quint8 Qroot::parseSystemControlCommand(const QVariant& data, quint8 defaultValue) {
    if (!data.isValid() || data.isNull())
        return defaultValue;
    if (data.canConvert<int>())
        return static_cast<quint8>(data.toInt() & 0xFF);
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(QStringLiteral("command")))
            return static_cast<quint8>(map.value(QStringLiteral("command")).toUInt() & 0xFF);
        if (map.contains(QStringLiteral("value")))
            return static_cast<quint8>(map.value(QStringLiteral("value")).toUInt() & 0xFF);
    }
    return defaultValue;
}

quint8 Qroot::parseOnOffParam(const QVariant& data, quint8 defaultValue) {
    if (!data.isValid() || data.isNull())
        return defaultValue;
    if (data.canConvert<int>())
        return data.toInt() != 0 ? 1 : 0;
    if (data.canConvert<QVariantList>()) {
        const QVariantList list = data.toList();
        if (!list.isEmpty())
            return list.at(0).toInt() != 0 ? 1 : 0;
    }
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(QStringLiteral("on")))
            return map.value(QStringLiteral("on")).toInt() != 0 ? 1 : 0;
        if (map.contains(QStringLiteral("value")))
            return map.value(QStringLiteral("value")).toInt() != 0 ? 1 : 0;
        if (map.contains(QStringLiteral("switch")))
            return map.value(QStringLiteral("switch")).toInt() != 0 ? 1 : 0;
    }
    return defaultValue;
}

QString Qroot::formatKeyNotifyLabel(quint8 keyId) {
    switch (keyId) {
    case 1:
        return QStringLiteral("加挡位按键(9A01)");
    case 2:
        return QStringLiteral("减挡位按键(9A02)");
    case 3:
        return QStringLiteral("模式按键(9A03)");
    default:
        return QStringLiteral("按键上报(9A ID=0x%1)").arg(keyId, 2, 16, QLatin1Char('0'));
    }
}

void Qroot::emitKeyNotifyReport(quint8 keyId) {
    ProtocolButtonStateData btn;
    btn.keyButtonId = keyId;
    qDebug().noquote() << "[Qroot] KeyNotify:" << formatKeyNotifyLabel(keyId);
    emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
}

void Qroot::emitToggleKeyNotifyReport(quint8 state) {
    ProtocolButtonStateData btn;
    btn.modeButtonState = state;
    qDebug().noquote() << "[Qroot] ToggleKeyNotify: state=0x"
                       << QString::number(state, 16).rightJustified(2, QLatin1Char('0')).toUpper();
    emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
}

void Qroot::emitBatteryInfoReport(const QByteArray& body) {
    if (body.size() < 3)
        return;
    ProtocolBatteryData battery;
    battery.percent = static_cast<quint8>(body.at(0));
    const int voltageRaw =
        (static_cast<quint8>(body.at(1)) << 8) | static_cast<quint8>(body.at(2));
    // 协议电压为大端 0.01V（如 0x01AA=426 → 4.26V），内部统一存 mV
    battery.voltageMv = voltageRaw * 10;
    emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(battery));
}

void Qroot::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty())
        return;
    QList<QByteArray> innerPackets;
    feedPhyRx(byte, innerPackets);
    for (const QByteArray& inner : innerPackets) {
        qDebug().noquote() << "[Qroot] RX inner:" << inner.toHex(' ').toUpper();
        rxBuffer_.append(inner);
    }
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
        if (rxBuffer_.size() < kHeaderSize)
            return;

        const quint8 cal = static_cast<quint8>(rxBuffer_.at(4));
        // 超大 CAL 多为串口噪声伪造的 AA55 头，丢弃帧头后重新搜，避免堵死后续合法应答
        if (cal > kMaxFrameBodyLen) {
            qWarning() << "[Qroot] invalid CAL" << cal << ", resync";
            rxBuffer_.remove(0, 2);
            continue;
        }

        const int frameLen = kHeaderSize + cal + 1;
        if (rxBuffer_.size() < frameLen)
            return;

        const QByteArray frame = rxBuffer_.left(frameLen);
        const quint8 expectCs = static_cast<quint8>(frame.at(frameLen - 1));
        const quint8 actualCs = checksum8(frame.left(frameLen - 1));
        if (expectCs != actualCs) {
            // 校验失败：只跳过 AA55，不要按 CAL 整帧丢弃（否则会吞掉夹在伪帧后的合法包）
            qWarning() << "[Qroot] checksum mismatch, resync";
            rxBuffer_.remove(0, 2);
            continue;
        }

        rxBuffer_.remove(0, frameLen);
        const quint8 ct = static_cast<quint8>(frame.at(2));
        const quint8 cid = static_cast<quint8>(frame.at(3));
        const QByteArray body = frame.mid(kHeaderSize, cal);
        qDebug().noquote() << "[Qroot] RX:" << frame.toHex(' ').toUpper();
        handleFrame(ct, cid, body);
    }
}

void Qroot::handleFrame(quint8 ct, quint8 cid, const QByteArray& body) {
    if (ct == Nack) {
        keyNotifySwitchPending_ = false;
        emit sendGetProductResponse(0);
        hasPending_ = false;
        ProtocolResultData result;
        result.result = 0;
        emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        return;
    }

    // 设备按键主动上报：实测 CT=Ack(0x01)；文档亦允许 Notify(0x03)
    if (body.size() >= 1 && (ct == Notify || ct == Ack)) {
        if (cid == KeyNotify) {
            const quint8 keyId = static_cast<quint8>(body.at(0));
            // 开/关上报 Ack：body=0xFF 表示已接收，须完成 sendCommandWithRetry
            if (ct == Ack && keyNotifySwitchPending_ &&
                (keyId == kKeyNotifySwitchAck || keyId == 0x00)) {
                keyNotifySwitchPending_ = false;
                ProtocolResultData result;
                result.result = 1;
                emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
                emit sendGetProductResponse(1);
                hasPending_ = false;
                return;
            }
            if (keyId >= 1 && keyId <= 3) {
                emitKeyNotifyReport(keyId);
                return;
            }
            return;
        }
        if (cid == ToggleKeyNotify) {
            emitToggleKeyNotifyReport(static_cast<quint8>(body.at(0)));
            return;
        }
    }

    if (ct == Notify) {
        if (cid == FlangeStatus && body.size() >= 1) {
            ProtocolFlangeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolFlangeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == NtcStatus && body.size() >= 2) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0)) | (static_cast<quint8>(body.at(1)) << 8);
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
            return;
        }
        if (cid == BatteryTemp && body.size() >= 1) {
            ProtocolBatteryTempData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolBatteryTempData"), QVariant::fromValue(status));
            return;
        }
        if (cid == BatteryInfo && body.size() >= 1 && static_cast<quint8>(body.at(0)) == kFactoryResetParam && body.size() < 3) {
            ProtocolResultData result;
            result.result = 1;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
            return;
        }
        if (cid == BatteryInfo && body.size() >= 3) {
            emitBatteryInfoReport(body);
            return;
        }
        if (cid == FactoryReset && body.size() >= 1 && static_cast<quint8>(body.at(0)) == kFactoryResetParam) {
            ProtocolResultData result;
            result.result = 1;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
            return;
        }
        if (cid == AgingEnter && body.size() >= 1) {
            ProtocolResultData result;
            result.result = static_cast<quint8>(body.at(0)) == 0 ? 1 : 0;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
            return;
        }
        if (cid == HeatTemp && body.size() >= 1) {
            ProtocolHeatTempData status;
            status.type = static_cast<quint8>(body.at(0));
            qDebug().noquote() << "[Qroot] HeatTemp:" << status.type << "C";
            emitReport(QStringLiteral("ProtocolHeatTempData"), QVariant::fromValue(status));
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
    case AgingEnter:
        if (cid == AgingEnter && body.size() >= 1) {
            ProtocolResultData result;
            result.result = static_cast<quint8>(body.at(0)) == 0 ? 1 : 0;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case SoftVersion:
        if (body.size() >= 1) {
            ProtocolBaseInfoData info;
            info.soft_version = formatSoftVersion(body);
            if (body.size() >= 3)
                info.hw_version = QString::number(static_cast<quint8>(body.at(2)));
            qDebug().noquote() << "[Qroot] SoftVersion:" << info.soft_version
                               << "hw:" << info.hw_version;
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
    case BatteryInfo:
        if (body.size() >= 3) {
            emitBatteryInfoReport(body);
        } else if (body.size() >= 1 && static_cast<quint8>(body.at(0)) == kFactoryResetParam) {
            ProtocolResultData result;
            result.result = 1;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case FactoryReset:
        if (body.size() >= 1) {
            const quint8 ctrl = static_cast<quint8>(body.at(0));
            if (ctrl >= kSystemControlShutdown && ctrl <= kFactoryResetParam) {
                ProtocolResultData result;
                result.result = 1;
                emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
            }
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case BatteryTemp:
        if (body.size() >= 1) {
            ProtocolBatteryTempData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolBatteryTempData"), QVariant::fromValue(status));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case SuctionTest:
        if (body.size() >= 1) {
            ProtocolResultData result;
            result.result = 1;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case PumpStallCurrent:
        if (body.size() >= 2) {
            // 协议约定堵转 ADC 高字节在前（大端）
            ProtocolPumpStallCurrentData stall;
            stall.adcValue = (static_cast<quint8>(body.at(0)) << 8) | static_cast<quint8>(body.at(1));
            qDebug().noquote() << "[Qroot] PumpStallCurrent ADC=" << stall.adcValue
                               << QStringLiteral("(0x%1)").arg(stall.adcValue, 4, 16, QChar('0'));
            emitReport(QStringLiteral("ProtocolPumpStallCurrentData"), QVariant::fromValue(stall));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case HeatLevelControl:
        if (body.size() >= 1) {
            ProtocolResultData result;
            // Ack 结果 0xFF 表示执行成功（与按键开关 Ack 约定一致）
            result.result = (static_cast<quint8>(body.at(0)) == 0xFF) ? 1 : 0;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case DeviceSnRead:
        if (!body.isEmpty()) {
            ProtocolSnData sn;
            sn.type = pendingSnType_;
            sn.value = QString::fromUtf8(body.left(40)).trimmed();
            qDebug().noquote() << "[Qroot] DeviceSN:" << sn.value << "len=" << body.size();
            emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(sn));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case TupleRead:
        if (body.size() >= 30) {
            const QString prod = QString::fromLatin1(body.left(6)).trimmed();
            const QString dev = QString::fromLatin1(body.mid(6, 16)).trimmed();
            const QByteArray cipher8 = body.mid(22, 8);
            ProtocolTupleData tuple;
            tuple.productId = prod;
            tuple.deviceId = dev;
            tuple.keyCipherHex = QString::fromLatin1(cipher8.toHex());
            // 有会话内写入的完整 16B key 时，按固件算法解密后 8 位明文
            if (!lastEncryptionKey_.isEmpty()) {
                const QByteArray plain =
                    CommonUtils::decryptRootTupleKeyTail(lastEncryptionKey_, cipher8);
                const bool ok = CommonUtils::matchRootTupleKeyTail(lastEncryptionKey_, cipher8);
                tuple.key = QString::fromLatin1(plain);
                tuple.keyDecrypted = ok;
                qDebug().noquote() << "[Qroot] TupleRead prod=" << prod << "dev=" << dev
                                   << "keyCipher=" << tuple.keyCipherHex << "keyPlainTail=" << tuple.key
                                   << "decryptOk=" << ok;
            } else {
                tuple.key = tuple.keyCipherHex;
                tuple.keyDecrypted = false;
                qDebug().noquote() << "[Qroot] TupleRead prod=" << prod << "dev=" << dev
                                   << "keyCipher=" << tuple.keyCipherHex
                                   << "(无缓存密钥，未解密；比对请用云端/写入密钥做 RC4)";
            }
            emitReport(QStringLiteral("ProtocolTupleData"), QVariant::fromValue(tuple));
        } else {
            qWarning() << "[Qroot] TupleRead len" << body.size() << "expect >=30";
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case DeviceSnWrite:
    case ProductIdWrite:
    case DeviceIdWrite:
    case EncryptionKeyWrite:
        {
            ProtocolResultData result;
            result.result = 1;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case PumpControl:
        if (body.size() >= 1) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case FlangeStatus:
        if (body.size() >= 1) {
            ProtocolFlangeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolFlangeData"), QVariant::fromValue(status));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case NtcStatus:
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
    case HeatTemp:
        if (body.size() >= 1) {
            ProtocolHeatTempData status;
            status.type = static_cast<quint8>(body.at(0));
            qDebug().noquote() << "[Qroot] HeatTemp Ack:" << status.type << "C";
            emitReport(QStringLiteral("ProtocolHeatTempData"), QVariant::fromValue(status));
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
    // 查询类指令使用 Notify 类型 + 0x01 参数（含 0x80 电池温度、0xE0 电量、0x96~0x9E 等）
    sendPacket(Notify, static_cast<quint8>(cid), QByteArray(1, '\x01'));
}

void Qroot::sendKeyNotifySwitch(quint8 enable) {
    keyNotifySwitchPending_ = true;
    sendPacket(Notify, KeyNotify, QByteArray(1, static_cast<char>(enable ? 1 : 0)));
}

QByteArray Qroot::buildPumpControlBody(const QVariant& data) {
    QVariantMap map;
    if (data.canConvert<QVariantMap>())
        map = data.toMap();
    const auto u8 = [&map](const QString& key, quint8 def) -> quint8 {
        if (!map.contains(key))
            return def;
        return static_cast<quint8>(map.value(key).toUInt() & 0xFF);
    };
    QByteArray body(10, '\0');
    body[0] = static_cast<char>(u8(QStringLiteral("status"), 1));
    body[1] = static_cast<char>(u8(QStringLiteral("mode"), 1));
    body[2] = static_cast<char>(u8(QStringLiteral("level"), 1));
    body[3] = static_cast<char>(u8(QStringLiteral("customMode"), 1));
    body[4] = static_cast<char>(u8(QStringLiteral("customLevel"), 1));
    body[5] = static_cast<char>(u8(QStringLiteral("customFreq"), 2));
    body[6] = static_cast<char>(u8(QStringLiteral("controlType"), 2));
    body[7] = static_cast<char>(u8(QStringLiteral("pumpFreq"), 1));
    return body;
}

void Qroot::sendPumpControl(const QByteArray& body10) {
    if (body10.size() != 10) {
        qWarning() << "[Qroot] PumpControl body must be 10 bytes, got" << body10.size();
        return;
    }
    sendPacket(Req, PumpControl, body10);
}

QByteArray Qroot::buildSuctionTestBody(const QVariant& data) {
    QVariantMap map;
    if (data.canConvert<QVariantMap>())
        map = data.toMap();
    const auto u8 = [&map](const QString& key, quint8 def) -> quint8 {
        if (!map.contains(key))
            return def;
        return static_cast<quint8>(map.value(key).toUInt() & 0xFF);
    };
    quint8 switchVal = 1;
    if (map.contains(QStringLiteral("switch")))
        switchVal = u8(QStringLiteral("switch"), 1);
    else if (map.contains(QStringLiteral("status")))
        switchVal = u8(QStringLiteral("status"), 1);
    else if (map.contains(QStringLiteral("on")))
        switchVal = u8(QStringLiteral("on"), 1);
    QByteArray body(3, '\0');
    body[0] = static_cast<char>(switchVal ? 1 : 0);
    body[1] = static_cast<char>(u8(QStringLiteral("mode"), 1));
    body[2] = static_cast<char>(u8(QStringLiteral("level"), 8));
    return body;
}

void Qroot::sendSuctionTest(const QByteArray& body3) {
    if (body3.size() != 3) {
        qWarning() << "[Qroot] SuctionTest body must be 3 bytes, got" << body3.size();
        return;
    }
    sendPacket(Req, SuctionTest, body3);
}

QByteArray Qroot::buildHeatLevelControlBody(const QVariant& data) {
    QVariantMap map;
    if (data.canConvert<QVariantMap>())
        map = data.toMap();
    const auto u8 = [&map](const QString& key, quint8 def) -> quint8 {
        if (!map.contains(key))
            return def;
        return static_cast<quint8>(map.value(key).toUInt() & 0xFF);
    };
    quint8 switchVal = 1;
    if (map.contains(QStringLiteral("switch")))
        switchVal = u8(QStringLiteral("switch"), 1);
    else if (map.contains(QStringLiteral("on")))
        switchVal = u8(QStringLiteral("on"), 1);
    else if (map.contains(QStringLiteral("status")))
        switchVal = u8(QStringLiteral("status"), 1);
    quint8 level = 0;
    if (map.contains(QStringLiteral("level")))
        level = u8(QStringLiteral("level"), 0);
    else if (map.contains(QStringLiteral("heatLevel")))
        level = u8(QStringLiteral("heatLevel"), 0);
    if (level > 2)
        level = 2;
    QByteArray body(2, '\0');
    body[0] = static_cast<char>(switchVal ? 1 : 0);
    body[1] = static_cast<char>(level);
    return body;
}

void Qroot::sendHeatLevelControl(const QByteArray& body2) {
    if (body2.size() != 2) {
        qWarning() << "[Qroot] HeatLevelControl body must be 2 bytes, got" << body2.size();
        return;
    }
    sendPacket(Req, HeatLevelControl, body2);
}

QByteArray Qroot::truncateUtf8Bytes(const QVariant& data, int maxLen, const QString& valueKey) {
    QByteArray bytes;
    if (data.canConvert<DeviceSnPayload>()) {
        bytes = data.value<DeviceSnPayload>().sn;
    } else if (data.type() == QVariant::ByteArray) {
        bytes = data.toByteArray();
    } else if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(valueKey))
            bytes = map.value(valueKey).toByteArray();
        else if (map.contains(QStringLiteral("sn")))
            bytes = map.value(QStringLiteral("sn")).toByteArray();
        else if (map.contains(QStringLiteral("value")))
            bytes = map.value(QStringLiteral("value")).toByteArray();
    } else {
        bytes = data.toString().toUtf8();
    }
    if (bytes.size() > maxLen)
        bytes = bytes.left(maxLen);
    return bytes;
}

void Qroot::sendDeviceSnWrite(const QByteArray& sn) {
    sendPacket(Req, DeviceSnWrite, sn.left(40));
}

void Qroot::sendProductIdWrite(const QByteArray& productId) {
    sendPacket(Req, ProductIdWrite, productId.left(6));
}

void Qroot::sendDeviceIdWrite(const QByteArray& deviceId) {
    sendPacket(Req, DeviceIdWrite, deviceId.left(16));
}

void Qroot::sendEncryptionKeyWrite(const QByteArray& key) {
    lastEncryptionKey_ = CommonUtils::normalizeRootEncryptionKey(key);
    sendPacket(Req, EncryptionKeyWrite, key.left(16));
}

bool Qroot::setSn(const QVariant& data) {
    if (data.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = data.value<DeviceSnPayload>();
        switch (payload.which_sn) {
        case FacDevInfoType_SKUID:
            sendProductIdWrite(payload.sn);
            return true;
        case FacDevInfoType_SUB_PID:
            sendDeviceIdWrite(payload.sn);
            return true;
        case FacDevInfoType_BOARD_SN:
        case FacDevInfoType_TAIL_SN:
        default:
            sendDeviceSnWrite(payload.sn);
            return true;
        }
    }

    const QVariantList list = data.toList();
    if (list.size() >= 2) {
        const auto which = static_cast<FacDevInfoType>(list.at(0).toInt());
        const QByteArray sn = list.at(1).toByteArray();
        if (which == FacDevInfoType_SKUID) {
            sendProductIdWrite(sn);
            return true;
        }
        if (which == FacDevInfoType_SUB_PID) {
            sendDeviceIdWrite(sn);
            return true;
        }
        sendDeviceSnWrite(sn);
        return true;
    }

    const QByteArray sn = truncateUtf8Bytes(data, 40, QStringLiteral("sn"));
    if (sn.isEmpty()) {
        qWarning() << "[Qroot] SN写入参数为空";
        return false;
    }
    sendDeviceSnWrite(sn);
    return true;
}

bool Qroot::sendSystemControl(quint8 ctrl) {
    return sendPacket(Req, FactoryReset, QByteArray(1, static_cast<char>(ctrl)));
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
    case DeviceCmd::RootSuctionTest:
        sendSuctionTest(buildSuctionTestBody(data));
        return;
    case DeviceCmd::RootHeatLevelControl:
        sendHeatLevelControl(buildHeatLevelControlBody(data));
        return;
    case DeviceCmd::RootPumpControl:
        sendPumpControl(buildPumpControlBody(data));
        return;
    case DeviceCmd::Sn:
        if (setSn(data))
            return;
        break;
    case DeviceCmd::WriteKey: {
        const QByteArray key = truncateUtf8Bytes(data, 16, QStringLiteral("value"));
        if (key.isEmpty()) {
            qWarning() << "[Qroot] WriteKey 参数为空";
            return;
        }
        sendEncryptionKeyWrite(key);
        return;
    }
    case DeviceCmd::BurningMode: {
        bool wantExit = false;
        if (data.canConvert<QVariantMap>()) {
            const QVariantMap map = data.toMap();
            const QVariant vEnter = map.value(QStringLiteral("enter"));
            const QVariant vSwitch = map.value(QStringLiteral("switch"));
            wantExit =
                (vEnter.isValid() && vEnter.toInt() == 0) || (vSwitch.isValid() && vSwitch.toInt() == 0);
        } else if (data.canConvert<QVariantList>()) {
            const QVariantList list = data.toList();
            if (list.size() >= 2)
                wantExit = list.at(1).toInt() == 0;
        }
        if (wantExit) {
            qWarning() << "[Qroot] BurningMode exit not supported (0xAF enter only)";
            return;
        }
        sendPacket(Notify, AgingEnter, QByteArray(1, '\x01'));
        return;
    }
    case DeviceCmd::ShipMode:
        // 与 Qfctp ShipMode→关机 对齐：Req 0xFC + 0x01
        sendSystemControl(kSystemControlShutdown);
        return;
    case DeviceCmd::DevReset:
        sendSystemControl(kSystemControlReboot);
        return;
    case DeviceCmd::RootEnterOta:
        sendSystemControl(kSystemControlOta);
        return;
    case DeviceCmd::FactoryReset:
        sendPacket(Notify, FactoryReset, QByteArray(1, static_cast<char>(kFactoryResetParam)));
        return;
    case DeviceCmd::RootSystemControl:
        // 兼容旧步骤 ini（Param_command=1~4）；新配置请改用上面拆分命令
        sendSystemControl(parseSystemControlCommand(data, kSystemControlShutdown));
        return;
    default:
        qWarning() << "[Qroot] unsupported set cmd" << static_cast<int>(cmd);
        break;
    }
}

void Qroot::get(DeviceCmd cmd, const QVariant& param) {
    switch (cmd) {
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
        sendPacket(Req, SoftVersion, {});
        return;
    case DeviceCmd::MacRead:
        sendPacket(Req, MacRead, {});
        return;
    case DeviceCmd::GetBattery:
        sendQuery(BatteryInfo);
        return;
    case DeviceCmd::RootBatteryTempQuery:
        sendQuery(BatteryTemp);
        return;
    case DeviceCmd::RootFlangeQuery:
        sendQuery(FlangeStatus);
        return;
    case DeviceCmd::RootPumpStallCurrentQuery:
        sendPacket(Req, PumpStallCurrent, {});
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
    case DeviceCmd::TupleRead:
        sendPacket(Req, TupleRead, {});
        return;
    case DeviceCmd::Sn: {
        const auto which = static_cast<FacDevInfoType>(param.toInt());
        // 读 productKey/deviceName 无独立 CID，兼容走三元组 F7
        if (which == FacDevInfoType_SUB_PID || which == FacDevInfoType_SKUID) {
            sendPacket(Req, TupleRead, {});
            return;
        }
        pendingSnType_ = (which == FacDevInfoType_BOARD_SN) ? ProtocolSnType::BoardSn : ProtocolSnType::TailSn;
        sendPacket(Req, DeviceSnRead, {});
        return;
    }
    case DeviceCmd::ButtonState:
        sendKeyNotifySwitch(parseOnOffParam(param, 1));
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
