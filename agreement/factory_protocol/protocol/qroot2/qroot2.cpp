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
static constexpr int kMaxFrameBodyLen = 64;

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
                // 固件回包只返回前35位SN，而发送的是39位(后补4个0)；校验以 head==0x00 为主，回显与发送前缀匹配
                pass = expected.startsWith(echo) || echo.startsWith(expected);
            }
        }
        pendingWriteSn_.clear();
        emit sendGetProductResponse(pass ? 1 : 0);
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
        // Ack payload：byte0 电量%（0~100），byte1 工作/充电状态（充电中为 02）；勿再用 0xA8 当电量
        ProtocolBatteryData battery;
        if (!body.isEmpty())
            battery.percent = static_cast<quint8>(body.at(0));
        if (body.size() >= 2)
            battery.chargeState = static_cast<quint8>(body.at(1));
        qDebug().noquote() << "[Qroot2] GetState(0xB0) percent=" << battery.percent
                           << "chargeState=" << battery.chargeState;
        emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(battery));
        emit sendGetProductResponse(1);
        hasPending_ = false;
        break;
    }
    case PoseAngle: {
        // Ack 4B：pitch、roll 各 int16 大端，单位 0.1°；固件不判 ±10°，由上位机卡控
        ProtocolRoot2PoseAngleData angle;
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

void Qroot2::sendDeviceSnWrite(const QByteArray& sn, quint8 snType) {
    Q_UNUSED(snType);
    // 写 SN：AA 55 00 A1 (len) (SN) (chk)；len=CAL=SN 字节数，body 为 SN 明文
    // 产品要求写入39位，如传入标准的35位SN，自动在末尾补足4个'0'达到39位
    QByteArray wireSn = sn.trimmed();
    if (wireSn.size() < 39) {
        wireSn = wireSn.leftJustified(39, '0');
    } else if (wireSn.size() > 39) {
        wireSn = wireSn.left(39);
    }
    pendingWriteSn_ = wireSn;
    sendPacket(Req, DeviceSnWrite, pendingWriteSn_);
}

bool Qroot2::setSn(const QVariant& data) {
    // 测试流程 normalizeSendParam 会归一成 DeviceSnPayload；须先解包，勿直接 toString
    if (data.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = data.value<DeviceSnPayload>();
        if (payload.sn.isEmpty()) {
            qWarning() << "[Qroot2] SN写入参数为空";
            return false;
        }
        const quint8 snType = static_cast<quint8>(payload.which_sn);
        sendDeviceSnWrite(payload.sn, snType != 0 ? snType : static_cast<quint8>(FacDevInfoType_TAIL_SN));
        return true;
    }
    const QByteArray sn = truncateUtf8Bytes(data, 40, QStringLiteral("sn"));
    if (sn.isEmpty()) {
        qWarning() << "[Qroot2] SN写入参数为空";
        return false;
    }
    sendDeviceSnWrite(sn, static_cast<quint8>(FacDevInfoType_TAIL_SN));
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
    case DeviceCmd::RootSuctionTest: {
        // Air1 模式/档位分开发（0xA2/0xA3）；本命令仅发模式，档位请用 Root2LevelSet
        quint8 mode = 0x02;
        if (data.canConvert<QVariantMap>())
            mode = static_cast<quint8>(data.toMap().value(QStringLiteral("mode"), 0x02).toUInt() & 0xFF);
        else
            buildSuctionModeLevel(data, &mode, nullptr);
        sendPacket(Req, ModeSet, QByteArray(1, static_cast<char>(mode)));
        return;
    }
    case DeviceCmd::ShipMode:
        // AA 55 00 AB 01 00 54；Ack：AA 55 01 AB 01 00 [cs]
        sendPacket(Req, PowerOff, QByteArray(1, '\x00'));
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
        sendPacket(Req, SoftVersion, QByteArray(1, '\x01'));
        return;
    case DeviceCmd::MacRead:
        sendPacket(Req, MacRead, QByteArray(1, '\x01'));
        return;
    case DeviceCmd::Root2KeyTest:
    case DeviceCmd::Root2StateRead:
    case DeviceCmd::ButtonState:
        // 请求帧（空 body）：AA 55 00 A6 00 5A
        sendPacket(Req, KeyTest, QByteArray());
        return;
    case DeviceCmd::GetBattery:
        // AA 55 00 B0 00 50；Ack 第 1 字节电量 bat_val（0~100%）
        sendPacket(Req, GetState, QByteArray());
        return;
    case DeviceCmd::RootPumpStallCurrentQuery:
        sendPacket(Req, PumpStallCurrent, QByteArray());
        return;
    case DeviceCmd::Root2PoseAngleRead:
        // AA 55 00 DA 00 26；Ack 4B pitch/roll（0.1° 有符号大端）
        sendPacket(Req, PoseAngle, QByteArray());
        return;
    case DeviceCmd::Root2PoseSwitchRead:
        // AA 55 00 DC 00 24
        sendPacket(Req, PoseSwitchRead, QByteArray());
        return;
    case DeviceCmd::Root2BowlEmptyCapRead:
        // AA 55 00 AD 00 53；回包按空碗 ReportType 分发
        pendingBowlCalibCapView_ = BowlCalibCapView::Empty;
        sendPacket(Req, BowlCalibRead, QByteArray());
        return;
    case DeviceCmd::Root2BowlFullCapRead:
        pendingBowlCalibCapView_ = BowlCalibCapView::Full;
        sendPacket(Req, BowlCalibRead, QByteArray());
        return;
    case DeviceCmd::Sn: {
        const auto which = static_cast<FacDevInfoType>(param.toInt());
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
