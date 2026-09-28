#include "qroot2.h"

#include "common_utils.h"

#include <QDebug>
#include <QVariantMap>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static constexpr quint8 kSop0 = 0xAA;
static constexpr quint8 kSop1 = 0x55;
static constexpr int kHeaderSize = 5;
/** 0xF0 应答含 35B SN，基础约 51B，预留扩展 */
static constexpr int kMaxFrameBodyLen = 200;

/** 【仅 M5 Smart】0xF0 请求默认 4B 产品 SN 占位（协议：可为空时发 0xAA551100） */
static QByteArray defaultDeviceInfoReqBody() {
    return QByteArray::fromHex("AA551100");
}

/** M5 Smart 0xFC 系统控制参数（仅 Root2M5* 命令显式发送） */
static constexpr quint8 kM5SysShutdown = 0x01;
static constexpr quint8 kM5SysReboot = 0x02;
static constexpr quint8 kM5SysOta = 0x03;
static constexpr quint8 kM5SysFactoryReset = 0x04;

static QString decodeM5SnBody(const QByteArray& body) {
    // Ack：SN(33/35B) + 预留 4B；CAL=0x25/0x27
    int snBytes = body.size();
    if (snBytes >= 37)
        snBytes -= 4;
    if (snBytes > 35)
        snBytes = 35;
    if (snBytes <= 0)
        return {};
    return QString::fromLatin1(body.constData(), snBytes).trimmed();
}

// Air1 读/写 SN 应答：首字节为状态，后续为定长 SN 区；勿用 fromUtf8(body) 否则遇 \\0 截成空串
static QString decodeAir1SnFieldBody(const QByteArray& body) {
    if (body.isEmpty())
        return QString();
    if (body.size() >= 2)
        return QString::fromLatin1(body.constData() + 1, body.size() - 1).trimmed();
    return QString::fromLatin1(body).trimmed();
}

QByteArray Qroot2::wrapPhyPacket(const QByteArray& innerPacket) {
    return wrapDonglePhyTxPacket(innerPacket, kDonglePhyChannelFac);
}

void Qroot2::feedPhyRx(const QByteArray& data, QList<QByteArray>& outInnerPackets) {
    phyRx_.feed(data, outInnerPackets);
}

Qroot2::Qroot2(QSerialPort* port) : qProtocol(port), serialPort_(port) {
}

quint8 Qroot2::checksum8(const QByteArray& data) {
    quint32 sum = 0;
    for (const char ch : data)
        sum += static_cast<quint8>(ch);
    return static_cast<quint8>(~sum);
}

QByteArray Qroot2::buildPacket(quint8 ct, quint8 cid, const QByteArray& body) {
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

bool Qroot2::sendPacket(quint8 ct, quint8 cid, const QByteArray& body) {
    if (!serialPort_ || !serialPort_->isOpen()) {
        qWarning() << "[Qroot2] serial not open";
        return false;
    }
    const QByteArray frame = buildPacket(ct, cid, body);
    const QByteArray wirePacket = skipDonglePhyWrap() ? frame : wrapPhyPacket(frame);
    if (wirePacket.isEmpty()) {
        qWarning() << "[Qroot2] dongle 封包失败";
        return false;
    }
    const qint64 n = serialPort_->write(wirePacket);
    if (!serialPort_->waitForBytesWritten(3000) || n != wirePacket.size()) {
        qWarning() << "[Qroot2] write failed";
        return false;
    }
    qDebug().noquote() << "[Qroot2] TX:" << frame.toHex(' ').toUpper();
    if (ct == Req)
        pendingCid_ = cid;
    hasPending_ = (ct == Req);
    return true;
}

QString Qroot2::formatMacFromWire(const QByteArray& body) {
    // Air1 协议：Body 固定为 1 字节状态码 + 12 字节 ASCII HEX 字符串（如 0x00 + "DAE10000042F"）
    if (body.size() < 13)
        return {};

    const QString hexStr = QString::fromLatin1(body.constData() + 1, 12).trimmed().toUpper();
    if (hexStr.size() != 12)
        return {};

    QStringList parts;
    parts.reserve(6);
    for (int i = 0; i < 12; i += 2) {
        parts.append(hexStr.mid(i, 2));
    }
    return parts.join(QLatin1Char(':'));
}

QString Qroot2::formatSoftVersion(const QByteArray& body) {
    if (body.size() >= 3)
        return QStringLiteral("%1.%2").arg(static_cast<quint8>(body.at(1))).arg(static_cast<quint8>(body.at(2)));
    if (body.size() >= 2)
        return QStringLiteral("%1.%2").arg(static_cast<quint8>(body.at(0))).arg(static_cast<quint8>(body.at(1)));
    if (body.size() >= 1)
        return QString::number(static_cast<quint8>(body.at(0)));
    return {};
}

/** 【仅 M5 Smart】打包版本字节 */
QString Qroot2::formatPackedVersion(quint8 packed) {
    return QStringLiteral("%1.%2").arg(packed / 10).arg(packed % 10);
}

/** 【仅 M5 Smart】0xF0 产品型号内部编码 */
QString Qroot2::productModelName(quint8 code) {
    switch (code) {
    case 0x01:
        return QStringLiteral("M9");
    case 0x02:
        return QStringLiteral("G1");
    case 0x03:
        return QStringLiteral("AIR1");
    case 0x04:
    case 0x07: // 实机 0xF0 回包：M5 Smart 见型号字节 0x07
        return QStringLiteral("M5 Smart");
    default:
        return QStringLiteral("0x%1").arg(code, 2, 16, QLatin1Char('0')).toUpper();
    }
}

/** 【仅 M5 Smart】0xC0 吸奶器控制 10B（与 Qroot 字段一致） */
QByteArray Qroot2::buildPumpControlBody(const QVariant& data) {
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

QByteArray Qroot2::parseMacToWire(const QVariant& data) {
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

quint8 Qroot2::parseOnOffParam(const QVariant& data, quint8 defaultValue) {
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
        if (map.contains(QStringLiteral("state")))
            return map.value(QStringLiteral("state")).toInt() != 0 ? 1 : 0;
        if (map.contains(QStringLiteral("pump_state")))
            return map.value(QStringLiteral("pump_state")).toInt() != 0 ? 1 : 0;
        if (map.contains(QStringLiteral("value")))
            return map.value(QStringLiteral("value")).toInt() != 0 ? 1 : 0;
        if (map.contains(QStringLiteral("switch")))
            return map.value(QStringLiteral("switch")).toInt() != 0 ? 1 : 0;
    }
    return defaultValue;
}

quint8 Qroot2::parseUInt8Param(const QVariant& data, quint8 defaultValue, const QString& key) {
    if (!data.isValid() || data.isNull())
        return defaultValue;
    if (data.canConvert<int>())
        return static_cast<quint8>(data.toInt() & 0xFF);
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(key))
            return static_cast<quint8>(map.value(key).toUInt() & 0xFF);
        if (map.contains(QStringLiteral("value")))
            return static_cast<quint8>(map.value(QStringLiteral("value")).toUInt() & 0xFF);
        if (map.contains(QStringLiteral("mode")))
            return static_cast<quint8>(map.value(QStringLiteral("mode")).toUInt() & 0xFF);
        if (map.contains(QStringLiteral("level")))
            return static_cast<quint8>(map.value(QStringLiteral("level")).toUInt() & 0xFF);
    }
    return defaultValue;
}

QByteArray Qroot2::truncateUtf8Bytes(const QVariant& data, int maxLen, const QString& valueKey) {
    QByteArray sn;
    if (data.canConvert<QByteArray>())
        sn = data.toByteArray();
    else if (data.canConvert<QVariantMap>())
        sn = data.toMap().value(valueKey).toString().trimmed().toUtf8();
    else
        sn = data.toString().trimmed().toUtf8();
    return sn.left(maxLen);
}

void Qroot2::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty())
        return;
    if (skipDonglePhyWrap()) {
        rxBuffer_.append(byte);
        drainRxBuffer();
        return;
    }
    QList<QByteArray> innerPackets;
    feedPhyRx(byte, innerPackets);
    for (const QByteArray& inner : innerPackets)
        rxBuffer_.append(inner);
    drainRxBuffer();
}

void Qroot2::drainRxBuffer() {
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
        if (cal > kMaxFrameBodyLen) {
            qWarning() << "[Qroot2] invalid CAL" << cal << ", resync";
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
            qWarning() << "[Qroot2] checksum mismatch, resync";
            rxBuffer_.remove(0, 2);
            continue;
        }

        rxBuffer_.remove(0, frameLen);
        const quint8 ct = static_cast<quint8>(frame.at(2));
        const quint8 cid = static_cast<quint8>(frame.at(3));
        const QByteArray body = frame.mid(kHeaderSize, cal);
        qDebug().noquote() << "[Qroot2] RX:" << frame.toHex(' ').toUpper();
        handleFrame(ct, cid, body);
    }
}

void Qroot2::handleFrame(quint8 ct, quint8 cid, const QByteArray& body) {
    if (ct == Nack) {
        emit sendGetProductResponse(0);
        hasPending_ = false;
        pendingBowlCalibCapView_ = BowlCalibCapView::None;
        ProtocolResultData result;
        result.result = 0;
        emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        return;
    }

    // 角度异常停机：Notify 0xD3，payload 与 0xDA 同为 pitch/roll（0.1° 大端）
    if (ct == Notify && cid == PoseAbnormal) {
        ProtocolRoot2PoseAngleData angle;
        angle.fromAbnormalNotify = true;
        if (body.size() >= 4) {
            const auto be16s = [&body](int off) -> qint16 {
                const quint16 u =
                    (static_cast<quint16>(static_cast<quint8>(body.at(off))) << 8)
                    | static_cast<quint8>(body.at(off + 1));
                return static_cast<qint16>(u);
            };
            angle.pitch01deg = be16s(0);
            angle.roll01deg = be16s(2);
        }
        qDebug().noquote() << "[Qroot2] PoseAbnormal(0xD3) pitch=" << (angle.pitch01deg / 10.0)
                           << "deg roll=" << (angle.roll01deg / 10.0) << "deg"
                           << "(raw 0.1°=" << angle.pitch01deg << "," << angle.roll01deg << ")";
        emitReport(QStringLiteral("ProtocolRoot2PoseAngleData"), QVariant::fromValue(angle));
        return;
    }

    if (ct != Ack)
        return;
    if (hasPending_ && cid != pendingCid_)
        return;

    switch (cid) {
    case LedControl:
    case Vibration:
    case MacWrite:
    case ModeSet:
    case LevelSet:
    case PumpState:
    case BowlCalib:
    case PoseSwitch:
    case PoseCalib:
    case PowerOff:
    case DeviceSnWrite: {
        bool pass = false;
        if (body.isEmpty()) {
            pass = true;
        } else if (body.size() == 1) {
            const quint8 code = static_cast<quint8>(body.at(0));
            pass = (code == 0xFF || code == 0x00);
            qDebug().noquote() << "[Qroot2] DeviceSN write ack code=" << Qt::hex << code;
        } else if (body.size() >= 2) {
            const quint8 head = static_cast<quint8>(body.at(0));
            const QString echo = decodeAir1SnFieldBody(body);
            qDebug().noquote() << "[Qroot2] DeviceSN write ack, head=" << Qt::hex << head << "echo=" << echo;
            pass = (head == 0x00);
            if (pass && !pendingWriteSn_.isEmpty() && !echo.isEmpty()) {
                const QString expected = QString::fromLatin1(pendingWriteSn_).trimmed();
                // 写入/回显均为 ≤35 位；校验以 head==0x00 为主，回显与发送前缀匹配
                pass = expected.startsWith(echo) || echo.startsWith(expected);
            }
        }
        pendingWriteSn_.clear();
        emit sendGetProductResponse(pass ? 1 : 0);
        hasPending_ = false;
        break;
    }
    case ProductIdWrite:
    case DeviceIdWrite:
    case EncryptionKeyWrite: {
        // 【仅 M5 Smart】0xF8/F9/FA 烧录应答：成功即过
        ProtocolResultData result;
        result.result = 1;
        emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case TupleRead: {
        // 【仅 M5 Smart】0xF7 Ack CAL=0x1E：productID(6)+deviceID(16)+KeyTail(8 RC4)
        if (body.size() >= 30) {
            const QString prod = QString::fromLatin1(body.left(6)).trimmed();
            const QString dev = QString::fromLatin1(body.mid(6, 16)).trimmed();
            const QByteArray cipher8 = body.mid(22, 8);
            ProtocolTupleData tuple;
            tuple.productId = prod;
            tuple.deviceId = dev;
            tuple.keyCipherHex = QString::fromLatin1(cipher8.toHex());
            if (!lastEncryptionKey_.isEmpty()) {
                const QByteArray plain =
                    CommonUtils::decryptRootTupleKeyTail(lastEncryptionKey_, cipher8);
                const bool ok = CommonUtils::matchRootTupleKeyTail(lastEncryptionKey_, cipher8);
                tuple.key = QString::fromLatin1(plain);
                tuple.keyDecrypted = ok;
                qDebug().noquote() << "[Qroot2][M5] TupleRead(0xF7) prod=" << prod << "dev=" << dev
                                   << "keyCipher=" << tuple.keyCipherHex << "keyPlainTail=" << tuple.key
                                   << "decryptOk=" << ok;
            } else {
                tuple.key = tuple.keyCipherHex;
                tuple.keyDecrypted = false;
                qDebug().noquote() << "[Qroot2][M5] TupleRead(0xF7) prod=" << prod << "dev=" << dev
                                   << "keyCipher=" << tuple.keyCipherHex
                                   << "(无缓存密钥，未解密；比对请用云端/写入密钥做 RC4)";
            }
            emitReport(QStringLiteral("ProtocolTupleData"), QVariant::fromValue(tuple));
        } else {
            qWarning() << "[Qroot2][M5] TupleRead(0xF7) len" << body.size() << "expect >=30";
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case TestMode: {
        // Ack：AA 55 01 AF 02 [result] [mode]；0成功 1长度错 2 mode 非 0/1 3 自检已做过
        const quint8 result = body.isEmpty() ? 0 : static_cast<quint8>(body.at(0));
        const quint8 mode = body.size() >= 2 ? static_cast<quint8>(body.at(1)) : 0;
        const bool pass = (result == 0);
        if (result == 3)
            qWarning() << "[Qroot2] TestMode 自检已做过，拒绝再进";
        else if (!pass)
            qWarning() << "[Qroot2] TestMode fail result=" << result << "mode=" << mode;
        else
            qDebug().noquote() << "[Qroot2] TestMode ok mode=" << mode;
        emit sendGetProductResponse(pass ? 1 : 0);
        hasPending_ = false;
        break;
    }
    case SoftVersion:
        if (body.size() >= 1) {
            ProtocolBaseInfoData info;
            info.soft_version = formatSoftVersion(body);
            if (body.size() >= 4)
                info.hw_version = QString::number(static_cast<quint8>(body.at(3)));
            else if (body.size() >= 3)
                info.hw_version = QString::number(static_cast<quint8>(body.at(2)));
            qDebug().noquote() << "[Qroot2] SoftVersion:" << info.soft_version << "hw:" << info.hw_version;
            emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case DeviceInfoGet:
        // 【仅 M5 Smart】0xF0：型号/平台/硬软版/自定义模式ID/满奶参数与开关/35B SN/加密/小版本/预留…
        if (body.size() >= 4) {
            ProtocolBaseInfoData info;
            info.product_name = productModelName(static_cast<quint8>(body.at(0)));
            info.hw_version = formatPackedVersion(static_cast<quint8>(body.at(2)));
            info.soft_version = formatPackedVersion(static_cast<quint8>(body.at(3)));
            if (body.size() >= 9)
                info.res_version = formatPackedVersion(static_cast<quint8>(body.at(8)));
            if (body.size() >= 48) {
                const quint8 patch = static_cast<quint8>(body.at(47));
                if (patch != 0)
                    info.soft_version += QStringLiteral(".%1").arg(patch);
            }
            qDebug().noquote() << "[Qroot2][M5] DeviceInfo(0xF0):" << info.product_name
                               << "sw:" << info.soft_version << "hw:" << info.hw_version
                               << "res:" << info.res_version << "bodyLen:" << body.size();
            emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
        } else {
            qWarning() << "[Qroot2][M5] DeviceInfo(0xF0) body too short:" << body.size();
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case DeviceSnGetM5: {
        // 【仅 M5 Smart】0xF1：SN 33/35B + 预留 4B
        ProtocolSnData sn;
        sn.type = pendingSnType_;
        sn.value = decodeM5SnBody(body);
        qDebug().noquote() << "[Qroot2][M5] DeviceSN(0xF1):" << sn.value << "bodyLen=" << body.size();
        if (!sn.value.isEmpty())
            emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(sn));
        emit sendGetProductResponse(sn.value.isEmpty() ? 0 : 1);
        hasPending_ = false;
        break;
    }
    case SystemControl: {
        // 【仅 M5 Smart】0xFC Ack 回显控制字
        const quint8 ctrl = body.isEmpty() ? 0 : static_cast<quint8>(body.at(0));
        qDebug().noquote() << "[Qroot2][M5] SystemControl(0xFC) ctrl=" << ctrl;
        ProtocolResultData result;
        result.result = 1;
        emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case PumpControl: {
        // 【仅 M5 Smart】0xC0 Ack：回显 controlType（0左1右2双边）
        if (!body.isEmpty()) {
            ProtocolTypeData status;
            status.type = static_cast<quint8>(body.at(0));
            emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case MacRead:
        if (body.size() >= 13) {
            const quint8 status = static_cast<quint8>(body.at(0));
            if (status == 0x00) {
                ProtocolMacData mac;
                mac.mac = formatMacFromWire(body);
                qDebug().noquote() << "[Qroot2] MAC:" << mac.mac;
                emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(mac));
            } else {
                qWarning() << "[Qroot2] MacRead status fail:" << status;
            }
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case DeviceSnRead:
        if (!body.isEmpty()) {
            ProtocolSnData sn;
            sn.type = pendingSnType_;
            sn.value = decodeAir1SnFieldBody(body);
            qDebug().noquote() << "[Qroot2] DeviceSN:" << sn.value;
            emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(sn));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case PumpStallCurrent:
        if (body.size() >= 2) {
            ProtocolPumpStallCurrentData stall;
            stall.adcValue = (static_cast<quint8>(body.at(0)) << 8) | static_cast<quint8>(body.at(1));
            qDebug().noquote() << "[Qroot2] PumpStallCurrent mV=" << stall.adcValue;
            emitReport(QStringLiteral("ProtocolPumpStallCurrentData"), QVariant::fromValue(stall));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case KeyTest:
        if (body.size() >= 1) {
            ProtocolButtonStateData btn;
            if (body.size() >= 4) {
                btn.powerButtonState = static_cast<quint8>(body.at(1));
                btn.modeButtonState = static_cast<quint8>(body.at(2));
                btn.keyButtonId = static_cast<quint8>(body.at(3));
            } else if (body.size() >= 2) {
                btn.keyButtonId = static_cast<quint8>(body.at(1));
            }
            btn.isKeyNotify = false; // 0xA6 运行状态快照，档位字节可为 0，不是空闲按键上报
            qDebug().noquote() << "[Qroot2] 0xA6 State: pump_state=" << btn.powerButtonState
                               << "mode=" << btn.modeButtonState << "level=" << btn.keyButtonId;
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
            ProtocolResultData result;
            result.result = static_cast<quint8>(body.at(0)) == 0 ? 1 : 0;
            emitReport(QStringLiteral("ProtocolResultData"), QVariant::fromValue(result));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    case GetState: {
        // Air1：byte0 电量% + byte1 工作/充电状态
        // 【仅 M5 Smart】CAL=0x2D：电量/状态/左右/模式/档位/自定义…（见协议 3.1.7）
        ProtocolBatteryData battery;
        if (!body.isEmpty())
            battery.percent = static_cast<quint8>(body.at(0));
        if (body.size() >= 2)
            battery.chargeState = static_cast<quint8>(body.at(1));
        qDebug().noquote() << "[Qroot2] GetState(0xB0) percent=" << battery.percent
                           << "chargeState=" << battery.chargeState << "bodyLen=" << body.size();
        emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(battery));
        // M5 长包：同步上报运行模式/档位，便于 Root2StateRead 卡控
        if (body.size() >= 5) {
            ProtocolButtonStateData btn;
            btn.powerButtonState = static_cast<quint8>(body.at(1)); // 状态：停/跑/充…
            btn.modeButtonState = static_cast<quint8>(body.at(3));  // 吸奶模式
            btn.keyButtonId = static_cast<quint8>(body.at(4));      // 吸奶档位
            btn.isKeyNotify = false;
            qDebug().noquote() << "[Qroot2][M5] GetState mode=" << btn.modeButtonState
                               << "level=" << btn.keyButtonId;
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
        }
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case PoseAngle: {
        // Ack 4B：pitch、roll 各 int16 大端，单位 0.1°；固件不判 ±10°，由上位机卡控
        ProtocolRoot2PoseAngleData angle;
        angle.fromAbnormalNotify = false;
        if (body.size() >= 4) {
            const auto be16s = [&body](int off) -> qint16 {
                const quint16 u =
                    (static_cast<quint16>(static_cast<quint8>(body.at(off))) << 8)
                    | static_cast<quint8>(body.at(off + 1));
                return static_cast<qint16>(u);
            };
            angle.pitch01deg = be16s(0);
            angle.roll01deg = be16s(2);
        }
        qDebug().noquote() << "[Qroot2] PoseAngle(0xDA) pitch=" << (angle.pitch01deg / 10.0)
                           << "deg roll=" << (angle.roll01deg / 10.0) << "deg"
                           << "(raw 0.1°=" << angle.pitch01deg << "," << angle.roll01deg << ")";
        emitReport(QStringLiteral("ProtocolRoot2PoseAngleData"), QVariant::fromValue(angle));
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case PoseSwitchRead: {
        // Ack：AA 55 01 DC 01 [状态] [校验]；00关 01开（角度检测，决定是否因姿势停泵）
        ProtocolTypeData status;
        status.type = body.isEmpty() ? 0 : static_cast<quint8>(body.at(0));
        qDebug().noquote() << "[Qroot2] PoseSwitchRead(0xDC)=" << status.type
                           << (status.type ? QStringLiteral("开") : QStringLiteral("关"));
        emitReport(QStringLiteral("ProtocolTypeData"), QVariant::fromValue(status));
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case BowlCalibRead: {
        // Ack：AA 55 01 AD 0D [result][empty×3 BE][full×3 BE][cs]
        ProtocolRoot2BowlCalibCapData cap;
        const auto be16 = [&body](int off) -> int {
            if (body.size() < off + 2)
                return 0;
            return (static_cast<quint8>(body.at(off)) << 8) | static_cast<quint8>(body.at(off + 1));
        };
        if (!body.isEmpty())
            cap.result = static_cast<quint8>(body.at(0));
        cap.emptyCh0 = be16(1);
        cap.emptyCh1 = be16(3);
        cap.emptyCh2 = be16(5);
        cap.fullCh0 = be16(7);
        cap.fullCh1 = be16(9);
        cap.fullCh2 = be16(11);
        const QString reportType = (pendingBowlCalibCapView_ == BowlCalibCapView::Full)
            ? QStringLiteral("ProtocolRoot2BowlFullCapData")
            : QStringLiteral("ProtocolRoot2BowlEmptyCapData");
        qDebug().noquote() << "[Qroot2] BowlCalibRead(0xAD)" << reportType
                           << "empty=" << cap.emptyCh0 << cap.emptyCh1 << cap.emptyCh2
                           << "full=" << cap.fullCh0 << cap.fullCh1 << cap.fullCh2;
        emitReport(reportType, QVariant::fromValue(cap));
        pendingBowlCalibCapView_ = BowlCalibCapView::None;
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    default:
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
}

QByteArray Qroot2::buildLedControlBody(const QVariant& data) {
    quint8 color = 0;
    quint8 state = 1;
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(QStringLiteral("color")))
            color = static_cast<quint8>(map.value(QStringLiteral("color")).toUInt() & 0xFF);
        else if (map.contains(QStringLiteral("led")))
            color = static_cast<quint8>(map.value(QStringLiteral("led")).toUInt() & 0xFF);
        if (map.contains(QStringLiteral("on")))
            state = map.value(QStringLiteral("on")).toInt() != 0 ? 1 : 0;
        else if (map.contains(QStringLiteral("state")))
            state = map.value(QStringLiteral("state")).toInt() != 0 ? 1 : 0;
        else if (map.contains(QStringLiteral("switch")))
            state = map.value(QStringLiteral("switch")).toInt() != 0 ? 1 : 0;
    } else {
        state = parseOnOffParam(data, 1);
    }
    // Air1 灯控 body 仅 颜色+状态 两字节：color 0x02=红灯（其它值按固件定义），state 0关1开
    QByteArray body;
    body.append(static_cast<char>(color));
    body.append(static_cast<char>(state));
    return body;
}

QByteArray Qroot2::buildSuctionModeLevel(const QVariant& data, quint8* modeOut, quint8* levelOut) {
    quint8 mode = 0x02;
    quint8 level = 1;
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(QStringLiteral("mode")))
            mode = static_cast<quint8>(map.value(QStringLiteral("mode")).toUInt() & 0xFF);
        if (map.contains(QStringLiteral("level")))
            level = static_cast<quint8>(map.value(QStringLiteral("level")).toUInt() & 0xFF);
    } else if (data.canConvert<QVariantList>()) {
        const QVariantList list = data.toList();
        if (list.size() >= 2) {
            mode = static_cast<quint8>(list.at(0).toUInt() & 0xFF);
            level = static_cast<quint8>(list.at(1).toUInt() & 0xFF);
        }
    }
    if (modeOut)
        *modeOut = mode;
    if (levelOut)
        *levelOut = level;
    return QByteArray();
}

void Qroot2::sendDeviceSnWrite(const QByteArray& sn, quint8 snType, int snLength) {
    Q_UNUSED(snType);
    // 写 SN：AA 55 00 A1 (len) (SN) (chk)；len=CAL=SN 字节数，body 为 SN 明文
    // Param_length：35=原样（M5）；39=不足则末尾补 '0'（Air1）
    const int targetLen = (snLength == 39) ? 39 : 35;
    QByteArray wireSn = sn.trimmed();
    if (targetLen == 39) {
        if (wireSn.size() < 39)
            wireSn = wireSn.leftJustified(39, '0');
        else if (wireSn.size() > 39)
            wireSn = wireSn.left(39);
    } else if (wireSn.size() > 35) {
        wireSn = wireSn.left(35);
    }
    pendingWriteSn_ = wireSn;
    sendPacket(Req, DeviceSnWrite, pendingWriteSn_);
}

void Qroot2::sendProductIdWrite(const QByteArray& productId) {
    // 【仅 M5 Smart】0xF8 写 productID ≤6
    sendPacket(Req, ProductIdWrite, productId.left(6));
}

void Qroot2::sendDeviceIdWrite(const QByteArray& deviceId) {
    // 【仅 M5 Smart】0xF9 写 deviceID ≤16
    sendPacket(Req, DeviceIdWrite, deviceId.left(16));
}

void Qroot2::sendEncryptionKeyWrite(const QByteArray& key) {
    // 【仅 M5 Smart】0xFA 写 EncryptionKey ≤16；缓存完整 key 供 0xF7 KeyTail RC4 校验
    lastEncryptionKey_ = CommonUtils::normalizeRootEncryptionKey(key);
    sendPacket(Req, EncryptionKeyWrite, key.left(16));
}

bool Qroot2::setSn(const QVariant& data) {
    // 测试流程 normalizeSendParam 会归一成 DeviceSnPayload；须先解包，勿直接 toString
    if (data.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = data.value<DeviceSnPayload>();
        if (payload.sn.isEmpty()) {
            qWarning() << "[Qroot2] SN写入参数为空";
            return false;
        }
        switch (payload.which_sn) {
        case FacDevInfoType_SKUID:
            // which_sn=7 → 0xF8 productID
            sendProductIdWrite(payload.sn);
            return true;
        case FacDevInfoType_SUB_PID:
            // which_sn=6 → 0xF9 deviceID
            sendDeviceIdWrite(payload.sn);
            return true;
        case FacDevInfoType_BOARD_SN:
        case FacDevInfoType_TAIL_SN:
        default:
            sendDeviceSnWrite(payload.sn, static_cast<quint8>(payload.which_sn), payload.snLength);
            return true;
        }
    }
    int snLength = 35;
    if (data.canConvert<QVariantMap>()) {
        const QVariantMap map = data.toMap();
        if (map.contains(QStringLiteral("length")))
            snLength = map.value(QStringLiteral("length")).toInt();
        else if (map.contains(QStringLiteral("snLength")))
            snLength = map.value(QStringLiteral("snLength")).toInt();
    }
    const int maxLen = (snLength == 39) ? 39 : 35;
    const QByteArray sn = truncateUtf8Bytes(data, maxLen, QStringLiteral("sn"));
    if (sn.isEmpty()) {
        qWarning() << "[Qroot2] SN写入参数为空";
        return false;
    }
    sendDeviceSnWrite(sn, static_cast<quint8>(FacDevInfoType_TAIL_SN), snLength);
    return true;
}

void Qroot2::set(DeviceCmd cmd, const QVariant& data) {
    switch (cmd) {
    case DeviceCmd::Root2PoseSwitch:
        sendPacket(Req, PoseSwitch, QByteArray(1, static_cast<char>(parseOnOffParam(data, 1))));
        return;
    case DeviceCmd::Root2PoseCalib:
        sendPacket(Req, PoseCalib, QByteArray(1, '\x01'));
        return;
    case DeviceCmd::Root2BowlCalib:
        sendPacket(Req, BowlCalib, QByteArray(1, static_cast<char>(parseOnOffParam(data, 0))));
        return;
    case DeviceCmd::BurningMode: {
        bool wantExit = false;
        if (data.canConvert<QVariantMap>()) {
            const QVariantMap map = data.toMap();
            const QVariant vEnter = map.value(QStringLiteral("enter"));
            const QVariant vSwitch = map.value(QStringLiteral("switch"));
            wantExit =
                (vEnter.isValid() && vEnter.toInt() == 0) || (vSwitch.isValid() && vSwitch.toInt() == 0);
        }
        if (wantExit) {
            qWarning() << "[Qroot2] BurningMode 无退出指令（0xAF 仅 0自检/1老化）";
            return;
        }
        quint8 mode = parseUInt8Param(data, 1, QStringLiteral("mode"));
        if (mode > 1)
            mode = 1;
        sendPacket(Req, TestMode, QByteArray(1, static_cast<char>(mode)));
        return;
    }
    case DeviceCmd::Root2ModeSet:
        sendPacket(Req, ModeSet, QByteArray(1, static_cast<char>(parseUInt8Param(data, 0x02, QStringLiteral("mode")))));
        return;
    case DeviceCmd::Root2LevelSet:
        sendPacket(Req, LevelSet, QByteArray(1, static_cast<char>(qBound(1, static_cast<int>(parseUInt8Param(data, 1, QStringLiteral("level"))), 15))));
        return;
    case DeviceCmd::Root2PumpState:
        sendPacket(Req, PumpState, QByteArray(1, static_cast<char>(parseOnOffParam(data, 1))));
        return;
    case DeviceCmd::RootPumpControl:
        // 【仅 M5 Smart】0xC0；步骤显式选用
        sendPacket(Req, PumpControl, buildPumpControlBody(data));
        return;
    case DeviceCmd::Root2LedControl:
    case DeviceCmd::LedColor:
    case DeviceCmd::LedTest:
        sendPacket(Req, LedControl, buildLedControlBody(data));
        return;
    case DeviceCmd::RootVibration:
    case DeviceCmd::MotorTestState:
        sendPacket(Req, Vibration, QByteArray(1, static_cast<char>(parseOnOffParam(data, 1))));
        return;
    case DeviceCmd::MacWrite:
        sendPacket(Req, MacWrite, parseMacToWire(data));
        return;
    case DeviceCmd::Sn:
        if (setSn(data))
            return;
        break;
    case DeviceCmd::WriteKey: {
        // 【仅 M5 Smart】0xFA；value=$TUPLE_DEVICE_SECRET
        const QByteArray key = truncateUtf8Bytes(data, 16, QStringLiteral("value"));
        if (key.isEmpty()) {
            qWarning() << "[Qroot2] WriteKey 参数为空";
            return;
        }
        sendEncryptionKeyWrite(key);
        return;
    }
    case DeviceCmd::RootSuctionTest: {
        // Air1：本命令仅发模式；档位请用 Root2LevelSet；M5 请用 RootPumpControl
        quint8 mode = 0x02;
        if (data.canConvert<QVariantMap>())
            mode = static_cast<quint8>(data.toMap().value(QStringLiteral("mode"), 0x02).toUInt() & 0xFF);
        else
            buildSuctionModeLevel(data, &mode, nullptr);
        sendPacket(Req, ModeSet, QByteArray(1, static_cast<char>(mode)));
        return;
    }
    case DeviceCmd::ShipMode:
        // Air1：AA 55 00 AB 01 00 54；M5 关机请用 Root2M5Shutdown
        sendPacket(Req, PowerOff, QByteArray(1, '\x00'));
        return;
    case DeviceCmd::Root2M5Shutdown:
        sendPacket(Req, SystemControl, QByteArray(1, static_cast<char>(kM5SysShutdown)));
        return;
    case DeviceCmd::Root2M5Reboot:
        sendPacket(Req, SystemControl, QByteArray(1, static_cast<char>(kM5SysReboot)));
        return;
    case DeviceCmd::Root2M5EnterOta:
        sendPacket(Req, SystemControl, QByteArray(1, static_cast<char>(kM5SysOta)));
        return;
    case DeviceCmd::FactoryReset:
    case DeviceCmd::Root2M5FactoryReset:
        sendPacket(Req, SystemControl, QByteArray(1, static_cast<char>(kM5SysFactoryReset)));
        return;
    case DeviceCmd::DevReset:
        qWarning() << "[Qroot2] DevReset 请改用 Root2M5Reboot（0xFC+0x02）";
        return;
    default:
        qWarning() << "[Qroot2] unsupported set cmd" << static_cast<int>(cmd);
        break;
    }
}

void Qroot2::get(DeviceCmd cmd, const QVariant& param) {
    switch (cmd) {
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
        // Air1：0xA5；M5 请用 Root2M5DeviceInfo（0xF0）
        sendPacket(Req, SoftVersion, QByteArray(1, '\x01'));
        return;
    case DeviceCmd::DeviceInfo:
    case DeviceCmd::Root2M5DeviceInfo:
        sendPacket(Req, DeviceInfoGet, defaultDeviceInfoReqBody());
        return;
    case DeviceCmd::MacRead:
        sendPacket(Req, MacRead, QByteArray(1, '\x01'));
        return;
    case DeviceCmd::Root2KeyTest:
        sendPacket(Req, KeyTest, QByteArray());
        return;
    case DeviceCmd::Root2StateRead:
    case DeviceCmd::ButtonState:
        // Air1：0xA6；M5 完整状态请用 Root2M5PumpStatus（0xB0）
        sendPacket(Req, KeyTest, QByteArray());
        return;
    case DeviceCmd::Root2M5PumpStatus:
    case DeviceCmd::GetBattery:
        // Air1 / M5 均 Req 0xB0；M5 Ack 更长，解析见 handleFrame
        sendPacket(Req, GetState, QByteArray());
        return;
    case DeviceCmd::RootPumpStallCurrentQuery:
        sendPacket(Req, PumpStallCurrent, QByteArray());
        return;
    case DeviceCmd::Root2PoseAngleRead:
        sendPacket(Req, PoseAngle, QByteArray());
        return;
    case DeviceCmd::Root2PoseAbnormalWait:
        qDebug().noquote() << "[Qroot2] waiting PoseAbnormal Notify 0xD3";
        return;
    case DeviceCmd::Root2PoseSwitchRead:
        sendPacket(Req, PoseSwitchRead, QByteArray());
        return;
    case DeviceCmd::Root2BowlEmptyCapRead:
        pendingBowlCalibCapView_ = BowlCalibCapView::Empty;
        sendPacket(Req, BowlCalibRead, QByteArray());
        return;
    case DeviceCmd::Root2BowlFullCapRead:
        pendingBowlCalibCapView_ = BowlCalibCapView::Full;
        sendPacket(Req, BowlCalibRead, QByteArray());
        return;
    case DeviceCmd::Root2M5SnRead:
        pendingSnType_ = ProtocolSnType::TailSn;
        sendPacket(Req, DeviceSnGetM5, QByteArray());
        return;
    case DeviceCmd::TupleRead:
        // 【仅 M5 Smart】读三元组 AA 55 00 F7 00 09
        sendPacket(Req, TupleRead, QByteArray());
        return;
    case DeviceCmd::Sn: {
        const auto which = static_cast<FacDevInfoType>(param.toInt());
        // 读 productKey/deviceName 无独立 CID，兼容走 0xF7
        if (which == FacDevInfoType_SUB_PID || which == FacDevInfoType_SKUID) {
            sendPacket(Req, TupleRead, QByteArray());
            return;
        }
        pendingSnType_ = (which == FacDevInfoType_BOARD_SN) ? ProtocolSnType::BoardSn : ProtocolSnType::TailSn;
        const quint8 snType = static_cast<quint8>(which != FacDevInfoType_WIFI_INFO ? which : FacDevInfoType_TAIL_SN);
        sendPacket(Req, DeviceSnRead, QByteArray(1, static_cast<char>(snType)));
        return;
    }
    default:
        qWarning() << "[Qroot2] unsupported get cmd" << static_cast<int>(cmd);
        break;
    }
}

bool Qroot2::sendCustomMessage(const QVariantMap& map) {
    const QVariant cidVar = map.value(QStringLiteral("cid"));
    if (!cidVar.isValid())
        return false;
    const quint8 ct = map.contains(QStringLiteral("ct")) ? static_cast<quint8>(map.value(QStringLiteral("ct")).toUInt()) : Req;
    const quint8 cid = static_cast<quint8>(cidVar.toUInt());
    QByteArray body;
    if (map.contains(QStringLiteral("body")))
        body = map.value(QStringLiteral("body")).toByteArray();
    else if (map.contains(QStringLiteral("value")))
        body = QByteArray(1, static_cast<char>(map.value(QStringLiteral("value")).toUInt()));
    return sendPacket(ct, cid, body);
}
