#include "qaiot_app.h"

#include "dongle_phy_codec.h"

#include <QDateTime>
#include <QDebug>
#include <QStringList>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// 应用层 Service ID（规范 3.1.1）。Dongle 通道看服务小节，不看命令字。
static const quint8 kSvcDevMgmt = 0x01; // 5.1 Device Management → 通道 5
static const quint8 kSvcFctAte = 0x04;  // 5.4 FCT&ATE → 通道 1
static const quint8 kSvcOta = 0x05;     // 5.5 OTA → 通道 2 APP
static const quint8 kSvcData = 0x07;    // 5.7 Data Service → 通道 6
static const quint8 kSvcPump = 0x10;    // 5.10 Pump Service → 通道 7

static bool dongleChannelForService(quint8 serviceId, quint8* channel, QString* name) {
    quint8 ch = 0;
    const char* text = nullptr;
    switch (serviceId) {
    case kSvcDevMgmt:
        ch = kDonglePhyChannelDevMgmt;
        text = "DEV_MGMT";
        break;
    case kSvcFctAte:
        ch = kDonglePhyChannelFac;
        text = "FAC";
        break;
    case kSvcOta:
        ch = kDonglePhyChannelApp;
        text = "APP";
        break;
    case kSvcData:
        ch = kDonglePhyChannelData;
        text = "DATA";
        break;
    case kSvcPump:
        ch = kDonglePhyChannelPump;
        text = "PUMP";
        break;
    default:
        return false;
    }
    if (channel)
        *channel = ch;
    if (name)
        *name = QString::fromLatin1(text);
    return true;
}

static const quint8 kCidNegotiate = 0x01;
static const quint8 kCidGetVersion = 0x04;
static const quint8 kCidSetTime = 0x0A;
static const quint8 kCidGetBattery = 0x0B;
static const quint8 kCidPumpCommonSet = 0x01;
static const quint8 kCidPumpCommonGet = 0x02;
static const quint8 kCidPumpWorkSet = 0x03;
static const quint8 kCidPumpWorkGet = 0x04;
static const quint8 kCidLightSet = 0x07;
static const quint8 kCidLightGet = 0x08;
static const quint8 kCidHeatSet = 0x09;
static const quint8 kCidHeatGet = 0x0A;

static const quint8 kTypeList = 0x01;
static const quint8 kTypeStruct = 0x02;
static const quint8 kTypeSide = 0x03;
static const quint8 kTlvError = 127;
static const quint32 kErrOk = 100000;

static QString hexText(const QByteArray& data) {
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

static QByteArray u8Bytes(quint8 v) {
    return QByteArray(1, static_cast<char>(v));
}

static QByteArray u16be(quint16 v) {
    QByteArray out;
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>(v & 0xFF));
    return out;
}

static QByteArray u32be(quint32 v) {
    QByteArray out;
    out.append(static_cast<char>((v >> 24) & 0xFF));
    out.append(static_cast<char>((v >> 16) & 0xFF));
    out.append(static_cast<char>((v >> 8) & 0xFF));
    out.append(static_cast<char>(v & 0xFF));
    return out;
}

static quint16 readU16be(const QByteArray& v) {
    if (v.size() < 2)
        return 0;
    return static_cast<quint16>((static_cast<quint8>(v.at(0)) << 8) | static_cast<quint8>(v.at(1)));
}

static quint32 readU32be(const QByteArray& v) {
    if (v.size() < 4)
        return 0;
    return (static_cast<quint32>(static_cast<quint8>(v.at(0))) << 24) |
           (static_cast<quint32>(static_cast<quint8>(v.at(1))) << 16) |
           (static_cast<quint32>(static_cast<quint8>(v.at(2))) << 8) |
           static_cast<quint32>(static_cast<quint8>(v.at(3)));
}

static QString utf8Trimmed(const QByteArray& raw) {
    int n = raw.size();
    while (n > 0 && raw.at(n - 1) == '\0')
        --n;
    return QString::fromUtf8(raw.constData(), n).trimmed();
}

static QString sideName(quint8 side) {
    switch (side) {
    case 0x00:
        return QStringLiteral("Left");
    case 0x01:
        return QStringLiteral("Right");
    case 0x02:
        return QStringLiteral("Sync");
    case 0x03:
        return QStringLiteral("Independence");
    default:
        return QString::number(side);
    }
}

static QString errorText(quint32 code) {
    switch (code) {
    case 100000:
        return QStringLiteral("成功");
    case 100001:
        return QStringLiteral("未知error类型");
    case 100002:
        return QStringLiteral("不支持该Service的请求");
    case 100003:
        return QStringLiteral("不支持该Command的请求");
    case 100004:
        return QStringLiteral("无权限");
    case 100005:
        return QStringLiteral("系统忙");
    case 100006:
        return QStringLiteral("请求格式错误");
    case 100007:
        return QStringLiteral("参数错误");
    case 100008:
        return QStringLiteral("申请内存失败");
    case 100009:
        return QStringLiteral("响应超时");
    case 100010:
        return QStringLiteral("此命令需要加密传输");
    case 110001:
        return QStringLiteral("对端未连接");
    case 110002:
        return QStringLiteral("低电禁止开奶碗灯");
    case 110003:
        return QStringLiteral("旅行锁使能禁止启动泵阀");
    case 110004:
        return QStringLiteral("当前模式不支持");
    default:
        return QStringLiteral("未知错误码");
    }
}

static QString pumpStatusName(quint8 v) {
    switch (v) {
    case 0x00:
        return QStringLiteral("空闲");
    case 0x01:
        return QStringLiteral("吸乳中");
    case 0x02:
        return QStringLiteral("暂停");
    case 0x03:
        return QStringLiteral("停止");
    default:
        return QString::number(v);
    }
}

static QString pumpModeName(quint8 v) {
    switch (v) {
    case 0x01:
        return QStringLiteral("按摩");
    case 0x02:
        return QStringLiteral("吸乳");
    case 0x03:
        return QStringLiteral("混合");
    case 0x04:
        return QStringLiteral("自定义");
    case 0x05:
        return QStringLiteral("PP追奶");
    case 0x06:
        return QStringLiteral("2+8奶量提升");
    case 0x07:
        return QStringLiteral("开奶");
    default:
        return QString::number(v);
    }
}

static QString connName(quint8 v) {
    switch (v) {
    case 0x00:
        return QStringLiteral("单机");
    case 0x01:
        return QStringLiteral("对侧");
    case 0x02:
        return QStringLiteral("多设备");
    default:
        return QString::number(v);
    }
}

// 加热/灯状态字段表：0x00=on，0x01=off
static QString onOffName(quint8 v) {
    if (v == 0x00)
        return QStringLiteral("开");
    if (v == 0x01)
        return QStringLiteral("关");
    return QString::number(v);
}

static QString clockText(int sec) {
    if (sec < 0)
        return QStringLiteral("-");
    return QStringLiteral("%1:%2").arg(sec / 60, 2, 10, QChar('0')).arg(sec % 60, 2, 10, QChar('0'));
}

static QString heatTempText(int level) {
    if (level >= 1 && level <= 6)
        return QStringLiteral("%1°C（%2档）").arg(35 + level).arg(level);
    if (level < 0)
        return QStringLiteral("-");
    return QStringLiteral("档位%1").arg(level);
}

static bool variantToUInt(const QVariant& v, quint32* out) {
    if (!out || !v.isValid())
        return false;
    bool ok = false;
    const QString text = v.toString().trimmed();
    quint32 n = 0;
    if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        n = text.mid(2).toUInt(&ok, 16);
    else
        n = v.toUInt(&ok);
    if (!ok)
        return false;
    *out = n;
    return true;
}

static QVariantMap asMap(const QVariant& data) {
    if (data.canConvert<QVariantMap>() && data.type() == QVariant::Map)
        return data.toMap();
    return {};
}

static bool mapValue(const QVariantMap& map, const QStringList& keys, QVariant* out) {
    for (const QString& key : keys) {
        if (!map.contains(key))
            continue;
        if (out)
            *out = map.value(key);
        return true;
    }
    return false;
}

static bool parseSideText(const QString& text, quint8* side) {
    const QString t = text.trimmed().toLower();
    if (t == QStringLiteral("left") || t == QStringLiteral("l"))
        *side = 0x00;
    else if (t == QStringLiteral("right") || t == QStringLiteral("r"))
        *side = 0x01;
    else if (t == QStringLiteral("sync"))
        *side = 0x02;
    else if (t == QStringLiteral("independence") || t == QStringLiteral("independent") || t == QStringLiteral("single"))
        *side = 0x03;
    else
        return false;
    return true;
}

static bool takeSide(const QVariant& data, quint8* side, bool* hasSide) {
    if (hasSide)
        *hasSide = false;
    const QVariantMap map = asMap(data);
    if (map.isEmpty())
        return true;
    QVariant raw;
    if (!mapValue(map, {QStringLiteral("side"), QStringLiteral("pump_side"), QStringLiteral("Param_side")}, &raw))
        return true;
    quint8 value = 0;
    if (parseSideText(raw.toString(), &value)) {
        if (side)
            *side = value;
        if (hasSide)
            *hasSide = true;
        return true;
    }
    quint32 n = 0;
    if (!variantToUInt(raw, &n) || n > 3)
        return false;
    if (side)
        *side = static_cast<quint8>(n);
    if (hasSide)
        *hasSide = true;
    return true;
}

static QString macText(const QByteArray& raw) {
    if (raw.size() == 6) {
        QStringList parts;
        for (int i = 0; i < 6; ++i)
            parts << QStringLiteral("%1").arg(static_cast<quint8>(raw.at(i)), 2, 16, QChar('0')).toUpper();
        return parts.join(QLatin1Char(':'));
    }
    return utf8Trimmed(raw);
}

QaiotApp::QaiotApp(QSerialPort* parent) : qProtocol(parent), serialPort(parent) {
}

void QaiotApp::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty())
        return;

    QList<QByteArray> inners;
    if (!tryUnwrapPhyPacket(byte, inners)) {
        if (static_cast<uint8_t>(byte.at(0)) == AiotLink::kSof)
            inners.append(byte);
    }

    for (const QByteArray& inner : inners) {
        const QString act = pendingAction_.trimmed().isEmpty() ? QStringLiteral("-") : pendingAction_;
        qDebug().noquote() << QStringLiteral("[QAIOT-APP] RX %1:").arg(act) << hexText(inner);
        QVector<AiotLinkCodec::Frame> frames;
        if (!linkCodec_.feed(inner, &frames))
            continue;
        for (const AiotLinkCodec::Frame& fr : frames)
            handleLinkFrame(fr);
    }
}

void QaiotApp::handleLinkFrame(const AiotLinkCodec::Frame& frame) {
    if (frame.control & AiotLink::kCtrlRsp)
        return;

    const uint8_t fsnBits = static_cast<uint8_t>(frame.control & AiotLink::kCtrlFsnMask);
    QByteArray pdu;
    if (fsnBits == AiotLink::kCtrlFsnNone) {
        reassembling_ = false;
        reassembly_.clear();
        pdu = frame.payload;
    } else if (fsnBits == AiotLink::kCtrlFsnStart) {
        reassembling_ = true;
        expectFsn_ = static_cast<uint8_t>(frame.fsn + 1);
        reassembly_ = frame.payload;
        return;
    } else if (fsnBits == AiotLink::kCtrlFsnMiddle) {
        if (!reassembling_ || frame.fsn != expectFsn_) {
            reassembling_ = false;
            reassembly_.clear();
            return;
        }
        reassembly_.append(frame.payload);
        ++expectFsn_;
        return;
    } else {
        if (!reassembling_ || frame.fsn != expectFsn_) {
            reassembling_ = false;
            reassembly_.clear();
            return;
        }
        reassembly_.append(frame.payload);
        pdu = reassembly_;
        reassembling_ = false;
        reassembly_.clear();
    }

    Message message;
    QString errorMessage;
    if (!parseMessage(pdu, &message, &errorMessage)) {
        qDebug() << "QAIOT-APP 应用层解析失败:" << errorMessage;
        return;
    }
    handleAppMessage(message);
}

void QaiotApp::handleAppMessage(const Message& message) {
    const QString act = pendingAction_.trimmed().isEmpty() ? QStringLiteral("-") : pendingAction_;
    qDebug().noquote() << QStringLiteral("QAIOT-APP RX [%1] %2").arg(act, describeMessage(message));

    const bool pendingHit = pendingService_ == message.serviceId && pendingCommand_ == message.commandId;

    TlvNode err;
    if (findTlvDeep(message.tlvs, kTlvError, &err) && err.value.size() >= 4) {
        const quint32 code = readU32be(err.value);
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("应用层 错误码 %1（%2） action=%3")
                       .arg(code)
                       .arg(errorText(code))
                       .arg(pendingAction_));
        if (code != kErrOk) {
            if (pendingHit)
                emit sendGetProductResponse(0);
            return;
        }
    }

    if (message.serviceId == kSvcDevMgmt && message.commandId == kCidNegotiate) {
        QStringList parts;
        TlvNode n;
        if (findTlv(message.tlvs, 0x01, &n) && !n.value.isEmpty())
            parts << QStringLiteral("protocol_version=%1").arg(static_cast<quint8>(n.value.at(0)));
        if (findTlv(message.tlvs, 0x02, &n) && n.value.size() >= 2) {
            const int mfs = readU16be(n.value);
            if (mfs > 8)
                maxPayload_ = mfs - 6;
            parts << QStringLiteral("max_frame_size=%1").arg(mfs);
        }
        if (findTlv(message.tlvs, 0x03, &n) && n.value.size() >= 2)
            parts << QStringLiteral("mtu=%1").arg(readU16be(n.value));
        if (findTlv(message.tlvs, 0x09, &n) && n.value.size() >= 4)
            parts << QStringLiteral("cmd_timeout_ms=%1").arg(readU32be(n.value));
        if (findTlv(message.tlvs, 0x0A, &n) && !n.value.isEmpty())
            parts << QStringLiteral("max_fsn_count=%1").arg(static_cast<quint8>(n.value.at(0)));
        if (findTlv(message.tlvs, 0x0B, &n) && !n.value.isEmpty()) {
            useLinkV2_ = true;
            parts << QStringLiteral("link_layer_version=%1").arg(static_cast<quint8>(n.value.at(0)));
        }
        if (!parts.isEmpty())
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("应用层协商 %1").arg(parts.join(QStringLiteral(" "))));
    } else if (message.serviceId == kSvcDevMgmt && message.commandId == kCidGetVersion) {
        ProtocolBaseInfoData info;
        TlvNode n;
        if (findTlv(message.tlvs, 0x01, &n))
            info.ble_version = utf8Trimmed(n.value);
        if (findTlv(message.tlvs, 0x03, &n))
            info.hw_version = utf8Trimmed(n.value);
        if (findTlv(message.tlvs, 0x04, &n))
            info.soft_version = utf8Trimmed(n.value);
        if (findTlv(message.tlvs, 0x07, &n))
            info.product_name = utf8Trimmed(n.value);
        if (!info.soft_version.isEmpty() || !info.hw_version.isEmpty() || !info.ble_version.isEmpty() ||
            !info.product_name.isEmpty()) {
            emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("应用层版本 soft=%1 hw=%2 bt=%3 model=%4")
                           .arg(info.soft_version, info.hw_version, info.ble_version, info.product_name));
        }
        if (findTlv(message.tlvs, 0x05, &n) && !n.value.isEmpty()) {
            ProtocolMacData mac;
            mac.mac = macText(n.value);
            emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(mac));
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层 MAC %1").arg(mac.mac));
        }
        if (findTlv(message.tlvs, 0x06, &n) && !n.value.isEmpty()) {
            ProtocolSnData sn;
            sn.type = ProtocolSnType::TailSn;
            sn.value = utf8Trimmed(n.value);
            emitReport(QStringLiteral("ProtocolSnData"), QVariant::fromValue(sn));
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层 SN %1").arg(sn.value));
        }
    } else if (message.serviceId == kSvcDevMgmt && message.commandId == kCidGetBattery) {
        TlvNode list;
        QList<TlvNode> oneGroup;
        const QList<TlvNode>* structs = nullptr;
        if (findTlv(message.tlvs, kTypeList, &list) && list.hasChildren) {
            structs = &list.children;
        } else {
            oneGroup.append(TlvNode());
            oneGroup.last().hasChildren = true;
            oneGroup.last().children = message.tlvs;
            structs = &oneGroup;
        }
        for (const TlvNode& node : *structs) {
            const QList<TlvNode>& fields = node.children;
            TlvNode sideNode;
            TlvNode percentNode;
            const bool hasSide = findTlv(fields, kTypeSide, &sideNode) && !sideNode.value.isEmpty();
            if (!findTlv(fields, 0x04, &percentNode) || percentNode.value.isEmpty())
                continue;
            ProtocolBatteryData bat;
            bat.percent = static_cast<quint8>(percentNode.value.at(0));
            emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(bat));
            const QString side = hasSide ? sideName(static_cast<quint8>(sideNode.value.at(0))) : QStringLiteral("-");
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("应用层电量 side=%1 percent=%2").arg(side).arg(bat.percent));
        }
    } else if (message.serviceId == kSvcPump && message.commandId == kCidPumpWorkGet) {
        TlvNode list;
        if (findTlv(message.tlvs, kTypeList, &list)) {
            QStringList statusLines;
            QStringList modeLines;
            QStringList levelLines;
            QStringList timeLines;
            int lastLevel = -1;
            int lastElapsed = -1;
            for (const TlvNode& st : list.children) {
                TlvNode n;
                QString side;
                if (findTlv(st.children, kTypeSide, &n) && !n.value.isEmpty())
                    side = sideName(static_cast<quint8>(n.value.at(0)));
                const QString prefix = side.isEmpty() ? QString() : side + QLatin1Char(' ');
                int sideStatus = -1;
                if (findTlv(st.children, 0x04, &n) && !n.value.isEmpty()) {
                    sideStatus = static_cast<quint8>(n.value.at(0));
                    statusLines << prefix + pumpStatusName(static_cast<quint8>(sideStatus));
                }
                if (findTlv(st.children, 0x05, &n) && !n.value.isEmpty())
                    modeLines << prefix + pumpModeName(static_cast<quint8>(n.value.at(0)));
                if (findTlv(st.children, 0x06, &n) && !n.value.isEmpty()) {
                    const int lv = static_cast<quint8>(n.value.at(0));
                    levelLines << prefix + QStringLiteral("%1档").arg(lv);
                    // 空闲侧常报 0 档。加减档只跟吸乳中/暂停的那一侧，避免被 0 档拉回 1
                    if (sideStatus == 0x01 || sideStatus == 0x02)
                        lastLevel = lv;
                    else if (lastLevel < 0 && lv > 0)
                        lastLevel = lv;
                }
                if (findTlv(st.children, 0x0A, &n) && n.value.size() >= 2) {
                    lastElapsed = readU16be(n.value);
                    timeLines << prefix + clockText(lastElapsed);
                }
            }
            QVariantMap panel;
            panel.insert(QStringLiteral("kind"), QStringLiteral("pump"));
            if (!statusLines.isEmpty())
                panel.insert(QStringLiteral("status"), statusLines.join(QStringLiteral(" / ")));
            if (!modeLines.isEmpty())
                panel.insert(QStringLiteral("mode"), modeLines.join(QStringLiteral(" / ")));
            if (!levelLines.isEmpty())
                panel.insert(QStringLiteral("levelText"), levelLines.join(QStringLiteral(" / ")));
            if (lastLevel >= 0)
                panel.insert(QStringLiteral("level"), lastLevel);
            if (!timeLines.isEmpty())
                panel.insert(QStringLiteral("elapsedText"), timeLines.join(QStringLiteral(" / ")));
            if (lastElapsed >= 0)
                panel.insert(QStringLiteral("elapsed"), lastElapsed);
            if (panel.size() > 1) {
                emitReport(QStringLiteral("ProtocolAiotAppPanel"), panel);
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("应用层吸乳状态 %1 %2 %3 %4")
                               .arg(panel.value(QStringLiteral("status")).toString(),
                                    panel.value(QStringLiteral("mode")).toString(),
                                    panel.value(QStringLiteral("levelText")).toString(),
                                    panel.value(QStringLiteral("elapsedText")).toString()));
            }
        }
    } else if (message.serviceId == kSvcPump && message.commandId == kCidPumpCommonGet) {
        TlvNode list;
        if (findTlv(message.tlvs, kTypeList, &list)) {
            QStringList connLines;
            for (const TlvNode& st : list.children) {
                TlvNode n;
                QString side;
                if (findTlv(st.children, kTypeSide, &n) && !n.value.isEmpty())
                    side = sideName(static_cast<quint8>(n.value.at(0)));
                if (findTlv(st.children, 0x04, &n) && !n.value.isEmpty()) {
                    const QString prefix = side.isEmpty() ? QString() : side + QLatin1Char(' ');
                    connLines << prefix + connName(static_cast<quint8>(n.value.at(0)));
                }
            }
            if (!connLines.isEmpty()) {
                QVariantMap panel;
                panel.insert(QStringLiteral("kind"), QStringLiteral("common"));
                panel.insert(QStringLiteral("status"), connLines.join(QStringLiteral(" / ")));
                emitReport(QStringLiteral("ProtocolAiotAppPanel"), panel);
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("应用层连接状态 %1").arg(panel.value(QStringLiteral("status")).toString()));
            }
        }
    } else if (message.serviceId == kSvcPump && message.commandId == kCidHeatGet) {
        TlvNode list;
        if (findTlv(message.tlvs, kTypeList, &list)) {
            QStringList stateLines;
            QStringList tempLines;
            QStringList timeLines;
            int lastLevel = -1;
            int lastDuration = -1;
            int lastOn = -1;
            for (const TlvNode& st : list.children) {
                TlvNode n;
                QString side;
                if (findTlv(st.children, kTypeSide, &n) && !n.value.isEmpty())
                    side = sideName(static_cast<quint8>(n.value.at(0)));
                const QString prefix = side.isEmpty() ? QString() : side + QLatin1Char(' ');
                if (findTlv(st.children, 0x04, &n) && !n.value.isEmpty()) {
                    lastOn = static_cast<quint8>(n.value.at(0)) == 0x00 ? 1 : 0;
                    stateLines << prefix + onOffName(static_cast<quint8>(n.value.at(0)));
                }
                if (findTlv(st.children, 0x05, &n) && !n.value.isEmpty()) {
                    lastLevel = static_cast<quint8>(n.value.at(0));
                    tempLines << prefix + heatTempText(lastLevel);
                }
                if (findTlv(st.children, 0x06, &n) && n.value.size() >= 2) {
                    lastDuration = readU16be(n.value);
                    timeLines << prefix + clockText(lastDuration);
                }
            }
            QVariantMap panel;
            panel.insert(QStringLiteral("kind"), QStringLiteral("heat"));
            if (!stateLines.isEmpty())
                panel.insert(QStringLiteral("status"), stateLines.join(QStringLiteral(" / ")));
            if (lastOn >= 0)
                panel.insert(QStringLiteral("on"), lastOn);
            if (!tempLines.isEmpty())
                panel.insert(QStringLiteral("tempText"), tempLines.join(QStringLiteral(" / ")));
            if (lastLevel >= 0)
                panel.insert(QStringLiteral("level"), lastLevel);
            if (!timeLines.isEmpty())
                panel.insert(QStringLiteral("durationText"), timeLines.join(QStringLiteral(" / ")));
            if (lastDuration >= 0)
                panel.insert(QStringLiteral("duration"), lastDuration);
            if (panel.size() > 1) {
                emitReport(QStringLiteral("ProtocolAiotAppPanel"), panel);
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("应用层加热 %1 %2 %3")
                               .arg(panel.value(QStringLiteral("status")).toString(),
                                    panel.value(QStringLiteral("tempText")).toString(),
                                    panel.value(QStringLiteral("durationText")).toString()));
            }
        }
    } else if (message.serviceId == kSvcPump && message.commandId == kCidLightGet) {
        TlvNode list;
        if (findTlv(message.tlvs, kTypeList, &list)) {
            QStringList stateLines;
            QStringList modeLines;
            int lastType = -1;
            int lastOn = -1;
            for (const TlvNode& st : list.children) {
                TlvNode n;
                QString side;
                if (findTlv(st.children, kTypeSide, &n) && !n.value.isEmpty())
                    side = sideName(static_cast<quint8>(n.value.at(0)));
                const QString prefix = side.isEmpty() ? QString() : side + QLatin1Char(' ');
                int type = -1;
                if (findTlv(st.children, 0x04, &n) && !n.value.isEmpty())
                    type = static_cast<quint8>(n.value.at(0));
                if (findTlv(st.children, 0x05, &n) && !n.value.isEmpty()) {
                    lastOn = static_cast<quint8>(n.value.at(0)) == 0x00 ? 1 : 0;
                    stateLines << prefix + onOffName(static_cast<quint8>(n.value.at(0)));
                }
                if (type >= 0) {
                    lastType = type;
                    modeLines << prefix + QStringLiteral("模式%1").arg(type + 1);
                }
            }
            QVariantMap panel;
            panel.insert(QStringLiteral("kind"), QStringLiteral("light"));
            if (!stateLines.isEmpty())
                panel.insert(QStringLiteral("status"), stateLines.join(QStringLiteral(" / ")));
            if (lastOn >= 0)
                panel.insert(QStringLiteral("on"), lastOn);
            if (!modeLines.isEmpty())
                panel.insert(QStringLiteral("mode"), modeLines.join(QStringLiteral(" / ")));
            if (lastType >= 0)
                panel.insert(QStringLiteral("type"), lastType);
            if (panel.size() > 1) {
                emitReport(QStringLiteral("ProtocolAiotAppPanel"), panel);
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("应用层红光 %1 %2")
                               .arg(panel.value(QStringLiteral("status")).toString(),
                                    panel.value(QStringLiteral("mode")).toString()));
            }
        }
    } else if (!pendingHit) {
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("应用层上报 svc=0x%1 cid=0x%2")
                       .arg(message.serviceId, 2, 16, QChar('0'))
                       .arg(message.commandId, 2, 16, QChar('0')));
    }

    if (pendingHit)
        emit sendGetProductResponse(1);
}

void QaiotApp::set(DeviceCmd cmd, const QVariant& data) {
    const QVariantMap map = asMap(data);
    switch (cmd) {
    case DeviceCmd::AppLinkNegotiate: {
        QList<TlvNode> tlvs;
        auto putU = [&](quint8 type, const QStringList& keys, int width) {
            QVariant raw;
            if (!mapValue(map, keys, &raw))
                return;
            quint32 n = 0;
            if (!variantToUInt(raw, &n))
                return;
            if (width == 1)
                tlvs.append(makeLeaf(type, u8Bytes(static_cast<quint8>(n))));
            else if (width == 2)
                tlvs.append(makeLeaf(type, u16be(static_cast<quint16>(n))));
            else
                tlvs.append(makeLeaf(type, u32be(n)));
        };
        if (map.isEmpty())
            tlvs.append(makeLeaf(0x01, u8Bytes(0x01)));
        else
            putU(0x01, {QStringLiteral("protocol_version")}, 1);
        putU(0x02, {QStringLiteral("max_frame_size")}, 2);
        putU(0x03, {QStringLiteral("mtu"), QStringLiteral("maximum_tranimission_unit")}, 2);
        putU(0x04, {QStringLiteral("transmit_interval")}, 2);
        putU(0x09, {QStringLiteral("cmd_timeout_ms")}, 4);
        putU(0x0A, {QStringLiteral("max_fsn_count")}, 1);
        putU(0x0B, {QStringLiteral("link_layer_version")}, 1);
        bool versionHeader = false;
        for (const TlvNode& n : tlvs) {
            if (n.type == 0x0B)
                versionHeader = true;
        }
        sendServiceCommand(kSvcDevMgmt, kCidNegotiate, tlvs, QStringLiteral("连接参数协商"), versionHeader);
        break;
    }
    case DeviceCmd::AppSetTime: {
        quint32 unixSec = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
        QVariant raw;
        if (mapValue(map, {QStringLiteral("time")}, &raw)) {
            quint32 n = 0;
            if (variantToUInt(raw, &n))
                unixSec = n;
        }
        QList<TlvNode> tlvs;
        tlvs.append(makeLeaf(0x01, u32be(unixSec)));
        quint16 offset = 0x0800;
        if (mapValue(map, {QStringLiteral("time_offset")}, &raw)) {
            quint32 n = 0;
            if (variantToUInt(raw, &n))
                offset = static_cast<quint16>(n);
        }
        tlvs.append(makeLeaf(0x02, u16be(offset)));
        sendServiceCommand(kSvcDevMgmt, kCidSetTime, tlvs, QStringLiteral("设置时间"));
        break;
    }
    case DeviceCmd::TravelLock: {
        int enable = 1;
        QVariant raw;
        if (mapValue(map, {QStringLiteral("enable"), QStringLiteral("on"), QStringLiteral("value")}, &raw))
            enable = raw.toInt() ? 1 : 0;
        else if (data.isValid() && map.isEmpty())
            enable = data.toInt() ? 1 : 0;
        QList<TlvNode> fields;
        quint8 side = 0;
        bool hasSide = false;
        if (takeSide(data, &side, &hasSide) && hasSide)
            fields.append(makeLeaf(kTypeSide, u8Bytes(side)));
        fields.append(makeLeaf(0x04, u8Bytes(static_cast<quint8>(enable))));
        if (mapValue(map, {QStringLiteral("wear_detect"), QStringLiteral("wear_detect_enable")}, &raw))
            fields.append(makeLeaf(0x05, u8Bytes(raw.toInt() ? 1 : 0)));
        const QList<TlvNode> tlvs = {makeParent(kTypeList, {makeParent(kTypeStruct, fields)})};
        sendServiceCommand(kSvcPump, kCidPumpCommonSet, tlvs,
                           enable ? QStringLiteral("开旅行锁") : QStringLiteral("关旅行锁"));
        break;
    }
    case DeviceCmd::AppPumpControl: {
        QList<TlvNode> fields;
        quint8 side = 0;
        bool hasSide = false;
        if (!takeSide(data, &side, &hasSide)) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层吸乳参数 side 非法"));
            return;
        }
        if (hasSide)
            fields.append(makeLeaf(kTypeSide, u8Bytes(side)));
        QVariant raw;
        if (mapValue(map, {QStringLiteral("enable")}, &raw)) {
            quint32 n = 1;
            const QString text = raw.toString().trimmed().toLower();
            if (text == QStringLiteral("pause"))
                n = 2;
            else if (text == QStringLiteral("stop"))
                n = 3;
            else if (text == QStringLiteral("enable") || text == QStringLiteral("on"))
                n = 1;
            else if (!variantToUInt(raw, &n))
                n = 1;
            // 字段表：1 开 / 2 暂停 / 3 停
            fields.append(makeLeaf(0x04, u8Bytes(static_cast<quint8>(n))));
        }
        const bool hasMode = mapValue(map, {QStringLiteral("mode")}, &raw);
        if (hasMode) {
            quint32 mode = 0;
            if (variantToUInt(raw, &mode))
                fields.append(makeLeaf(0x05, u8Bytes(static_cast<quint8>(mode))));
            quint32 type = 0;
            QVariant typeRaw;
            if (mapValue(map, {QStringLiteral("type"), QStringLiteral("pump_type")}, &typeRaw))
                variantToUInt(typeRaw, &type);
            fields.append(makeLeaf(0x09, u8Bytes(static_cast<quint8>(type))));
        }
        if (mapValue(map, {QStringLiteral("level")}, &raw)) {
            quint32 n = 0;
            if (variantToUInt(raw, &n))
                fields.append(makeLeaf(0x06, u8Bytes(static_cast<quint8>(n))));
        }
        if (mapValue(map, {QStringLiteral("freq")}, &raw)) {
            quint32 n = 0;
            const QString text = raw.toString().trimmed().toLower();
            if (text == QStringLiteral("slow"))
                n = 0;
            else if (text == QStringLiteral("mid"))
                n = 1;
            else if (text == QStringLiteral("high"))
                n = 2;
            else
                variantToUInt(raw, &n);
            fields.append(makeLeaf(0x07, u8Bytes(static_cast<quint8>(n))));
        }
        if (fields.isEmpty() || (fields.size() == 1 && hasSide)) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层吸乳参数为空"));
            return;
        }
        const QList<TlvNode> tlvs = {makeParent(kTypeList, {makeParent(kTypeStruct, fields)})};
        sendServiceCommand(kSvcPump, kCidPumpWorkSet, tlvs, QStringLiteral("设置吸乳参数"));
        break;
    }
    case DeviceCmd::AppHeatControl: {
        QList<TlvNode> fields;
        quint8 side = 0;
        bool hasSide = false;
        if (!takeSide(data, &side, &hasSide)) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层加热 side 非法"));
            return;
        }
        if (hasSide)
            fields.append(makeLeaf(kTypeSide, u8Bytes(side)));
        QVariant raw;
        quint32 enable = 0;
        if (mapValue(map, {QStringLiteral("enable")}, &raw))
            enable = raw.toInt() ? 1 : 0;
        quint32 level = 1;
        if (mapValue(map, {QStringLiteral("level")}, &raw))
            variantToUInt(raw, &level);
        quint32 duration = 300;
        if (mapValue(map, {QStringLiteral("duration"), QStringLiteral("heat_duration_time")}, &raw))
            variantToUInt(raw, &duration);
        fields.append(makeLeaf(0x04, u8Bytes(static_cast<quint8>(enable))));
        fields.append(makeLeaf(0x05, u8Bytes(static_cast<quint8>(level))));
        fields.append(makeLeaf(0x06, u16be(static_cast<quint16>(duration))));
        const QList<TlvNode> tlvs = {makeParent(kTypeList, {makeParent(kTypeStruct, fields)})};
        sendServiceCommand(kSvcPump, kCidHeatSet, tlvs, enable ? QStringLiteral("开加热") : QStringLiteral("关加热"));
        break;
    }
    case DeviceCmd::AppLightControl: {
        QVariant raw;
        quint32 type = 0;
        if (!mapValue(map, {QStringLiteral("type"), QStringLiteral("light_type")}, &raw) || !variantToUInt(raw, &type)) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层红光缺少灯类型"));
            return;
        }
        QList<TlvNode> fields;
        quint8 side = 0;
        bool hasSide = false;
        if (!takeSide(data, &side, &hasSide)) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层红光 side 非法"));
            return;
        }
        if (hasSide)
            fields.append(makeLeaf(kTypeSide, u8Bytes(side)));
        fields.append(makeLeaf(0x04, u8Bytes(static_cast<quint8>(type))));
        quint32 enable = 1;
        const bool hasEnable = mapValue(map, {QStringLiteral("enable")}, &raw);
        if (hasEnable)
            enable = raw.toInt() ? 1 : 0;
        fields.append(makeLeaf(0x05, u8Bytes(static_cast<quint8>(enable))));
        const QList<TlvNode> tlvs = {makeParent(kTypeList, {makeParent(kTypeStruct, fields)})};
        sendServiceCommand(kSvcPump, kCidLightSet, tlvs, enable ? QStringLiteral("开红光") : QStringLiteral("关红光"));
        break;
    }
    default:
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("应用层协议未映射 set 命令，cmd=%1").arg(static_cast<int>(cmd)));
        break;
    }
}

void QaiotApp::get(DeviceCmd cmd, const QVariant& param) {
    quint8 side = 0;
    bool hasSide = false;
    if (!takeSide(param, &side, &hasSide)) {
        emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("应用层查询 side 非法"));
        return;
    }
    QList<TlvNode> filter;
    if (hasSide)
        filter.append(makeLeaf(kTypeSide, u8Bytes(side)));

    switch (cmd) {
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
    case DeviceCmd::DeviceInfo:
    case DeviceCmd::MacRead:
    case DeviceCmd::Sn:
        sendServiceCommand(kSvcDevMgmt, kCidGetVersion, {}, QStringLiteral("读版本"));
        break;
    case DeviceCmd::GetBattery:
        sendServiceCommand(kSvcDevMgmt, kCidGetBattery, filter, QStringLiteral("读电量"));
        break;
    case DeviceCmd::AppPumpStatus:
        sendServiceCommand(kSvcPump, kCidPumpWorkGet, filter, QStringLiteral("读吸乳状态"));
        break;
    case DeviceCmd::AppCommonStatus:
        sendServiceCommand(kSvcPump, kCidPumpCommonGet, filter, QStringLiteral("读通用状态"));
        break;
    case DeviceCmd::AppHeatStatus:
        sendServiceCommand(kSvcPump, kCidHeatGet, filter, QStringLiteral("读加热状态"));
        break;
    case DeviceCmd::AppLightStatus:
        sendServiceCommand(kSvcPump, kCidLightGet, filter, QStringLiteral("读红光状态"));
        break;
    default:
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("应用层协议未映射 get 命令，cmd=%1").arg(static_cast<int>(cmd)));
        break;
    }
}

bool QaiotApp::sendCustomMessage(const QVariantMap& map) {
    quint32 svc = 0;
    quint32 cid = 0;
    if (!variantToUInt(map.value(QStringLiteral("serviceId"), map.value(QStringLiteral("service"))), &svc) ||
        svc > 0xFF) {
        qWarning() << "QAIOT-APP 通用发送缺少 serviceId";
        return false;
    }
    const quint8 serviceId = static_cast<quint8>(svc);
    if (!variantToUInt(map.value(QStringLiteral("commandId"), map.value(QStringLiteral("command"))), &cid) ||
        cid > 0xFF) {
        qWarning() << "QAIOT-APP 通用发送缺少 commandId";
        return false;
    }
    const quint8 commandId = static_cast<quint8>(cid);

    QList<TlvNode> tlvs;
    QString errorMessage;
    const QVariant tlvListValue = map.value(QStringLiteral("tlvs"));
    if (tlvListValue.isValid()) {
        const QVariantList list = tlvListValue.toList();
        for (const QVariant& item : list) {
            TlvNode node;
            if (!tlvFromVariant(item, &node, &errorMessage)) {
                qWarning() << "QAIOT-APP TLV 参数非法:" << errorMessage;
                return false;
            }
            tlvs.append(node);
        }
    } else if (map.contains(QStringLiteral("type"))) {
        TlvNode node;
        if (!tlvFromVariant(map, &node, &errorMessage)) {
            qWarning() << "QAIOT-APP TLV 参数非法:" << errorMessage;
            return false;
        }
        tlvs.append(node);
    }
    return sendServiceCommand(serviceId, commandId, tlvs, map.value(QStringLiteral("action")).toString());
}

bool QaiotApp::sendServiceCommand(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs,
                                  const QString& actionName, bool versionHeader) {
    quint8 channel = 0;
    QString channelName;
    if (!dongleChannelForService(serviceId, &channel, &channelName)) {
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("应用层 svc=0x%1 没有对应的 Dongle 通道")
                       .arg(serviceId, 2, 16, QChar('0')));
        return false;
    }
    pendingService_ = serviceId;
    pendingCommand_ = commandId;
    pendingAction_ = actionName.trimmed().isEmpty() ? QStringLiteral("应用层命令") : actionName.trimmed();
    const QByteArray app = buildMessage(serviceId, commandId, tlvs);
    emitReport(QStringLiteral("ProtocolPbDate"),
               QStringLiteral("应用层 TX [%1] svc=0x%2 cid=0x%3 ch=%4 %5")
                   .arg(pendingAction_)
                   .arg(serviceId, 2, 16, QChar('0'))
                   .arg(commandId, 2, 16, QChar('0'))
                   .arg(channel)
                   .arg(channelName));
    return sendAppPdu(app, versionHeader || useLinkV2_, channel);
}

bool QaiotApp::sendAppPdu(const QByteArray& appPdu, bool versionHeader, quint8 channel) {
    if (!serialPort || !serialPort->isOpen()) {
        qWarning() << "QAIOT-APP 串口未打开，未发送数据";
        return false;
    }
    const uint8_t psn = versionHeader ? psnCounter_++ : 0;
    const QVector<QByteArray> linkFrames =
        AiotLinkCodec::buildFramesForPdu(appPdu, maxPayload_, versionHeader, psn);
    for (const QByteArray& link : linkFrames) {
        const QByteArray phy = wrapPhyPacket(link, channel);
        if (phy.isEmpty())
            return false;
        const qint64 n = serialPort->write(phy);
        qDebug().noquote() << QStringLiteral("[QAIOT-APP] TX %1:").arg(pendingAction_) << hexText(link);
        if (n != phy.size()) {
            qWarning() << "QAIOT-APP 发送不完整" << n << "/" << phy.size();
            return false;
        }
    }
    return true;
}

QByteArray QaiotApp::wrapPhyPacket(const QByteArray& innerPacket, quint8 channel) const {
    if (skipDonglePhyWrap())
        return innerPacket;
    return wrapDonglePhyTxPacket(innerPacket, channel);
}

bool QaiotApp::tryUnwrapPhyPacket(const QByteArray& packet, QList<QByteArray>& outPackets) {
    if (skipDonglePhyWrap()) {
        if (packet.isEmpty())
            return false;
        outPackets.append(packet);
        return true;
    }
    const int before = outPackets.size();
    phyRx_.feed(packet, outPackets);
    return outPackets.size() > before;
}

QaiotApp::TlvNode QaiotApp::makeLeaf(quint8 type, const QByteArray& value) const {
    TlvNode n;
    n.type = type & 0x7F;
    n.hasChildren = false;
    n.value = value;
    return n;
}

QaiotApp::TlvNode QaiotApp::makeParent(quint8 type, const QList<TlvNode>& children) const {
    TlvNode n;
    n.type = type & 0x7F;
    n.hasChildren = true;
    n.children = children;
    return n;
}

bool QaiotApp::findTlv(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const {
    for (const TlvNode& n : tlvs) {
        if (n.type == type) {
            if (out)
                *out = n;
            return true;
        }
    }
    return false;
}

bool QaiotApp::findTlvDeep(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const {
    for (const TlvNode& n : tlvs) {
        if (n.type == type) {
            if (out)
                *out = n;
            return true;
        }
        if (n.hasChildren && findTlvDeep(n.children, type, out))
            return true;
    }
    return false;
}

bool QaiotApp::parseMessage(const QByteArray& frame, Message* message, QString* errorMessage) const {
    if (!message)
        return false;
    if (frame.size() < 2) {
        if (errorMessage)
            *errorMessage = QStringLiteral("报文长度不足");
        return false;
    }
    message->serviceId = static_cast<quint8>(frame.at(0));
    message->commandId = static_cast<quint8>(frame.at(1));
    message->tlvs.clear();
    return parseTlvs(frame, 2, frame.size(), &message->tlvs, errorMessage);
}

bool QaiotApp::parseTlvs(const QByteArray& data, int start, int end, QList<TlvNode>* out, QString* errorMessage) const {
    if (!out || start < 0 || end > data.size() || start > end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 范围非法");
        return false;
    }
    int pos = start;
    while (pos < end) {
        TlvNode node;
        const quint8 rawType = static_cast<quint8>(data.at(pos++));
        node.hasChildren = (rawType & 0x80u) != 0;
        node.type = rawType & 0x7Fu;
        int length = 0;
        if (!readVarLength(data, &pos, end, &length, errorMessage))
            return false;
        if (pos + length > end) {
            if (errorMessage)
                *errorMessage = QStringLiteral("TLV 长度越界 type=%1 length=%2").arg(node.type).arg(length);
            return false;
        }
        if (node.hasChildren && length > 0) {
            if (!parseTlvs(data, pos, pos + length, &node.children, errorMessage))
                return false;
        } else {
            node.value = data.mid(pos, length);
        }
        pos += length;
        out->append(node);
    }
    return true;
}

bool QaiotApp::readVarLength(const QByteArray& data, int* pos, int end, int* length, QString* errorMessage) const {
    if (!pos || !length || *pos >= end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Length 字段缺失");
        return false;
    }
    const quint8 b0 = static_cast<quint8>(data.at((*pos)++));
    if ((b0 & 0x80u) == 0) {
        *length = b0;
        return true;
    }
    if (*pos >= end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Length 第二字节缺失");
        return false;
    }
    const quint8 b1 = static_cast<quint8>(data.at((*pos)++));
    if ((b1 & 0x80u) != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Length 超过当前支持的 2 字节范围");
        return false;
    }
    *length = ((b0 & 0x7F) << 7) | (b1 & 0x7F);
    return true;
}

QByteArray QaiotApp::buildMessage(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs) const {
    QByteArray frame;
    frame.append(static_cast<char>(serviceId));
    frame.append(static_cast<char>(commandId));
    frame.append(buildTlvs(tlvs));
    return frame;
}

QByteArray QaiotApp::buildTlvs(const QList<TlvNode>& tlvs) const {
    QByteArray out;
    for (const TlvNode& tlv : tlvs) {
        const QByteArray payload = tlv.hasChildren ? buildTlvs(tlv.children) : tlv.value;
        quint8 rawType = tlv.type & 0x7Fu;
        if (tlv.hasChildren)
            rawType |= 0x80u;
        out.append(static_cast<char>(rawType));
        out.append(encodeVarLength(payload.size()));
        out.append(payload);
    }
    return out;
}

QByteArray QaiotApp::encodeVarLength(int length) const {
    QByteArray out;
    if (length < 0 || length > 0x3FFF)
        return out;
    if (length <= 0x7F) {
        out.append(static_cast<char>(length & 0x7F));
        return out;
    }
    out.append(static_cast<char>(0x80u | ((length >> 7) & 0x7F)));
    out.append(static_cast<char>(length & 0x7F));
    return out;
}

bool QaiotApp::tlvFromVariant(const QVariant& value, TlvNode* out, QString* errorMessage) const {
    if (!out)
        return false;
    const QVariantMap map = value.toMap();
    if (map.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 需要 QVariantMap");
        return false;
    }
    quint32 type = 0;
    if (!variantToUInt(map.value(QStringLiteral("type")), &type) || type > 0x7F) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV type 非法");
        return false;
    }
    out->type = static_cast<quint8>(type);
    out->children.clear();
    out->value.clear();
    if (map.contains(QStringLiteral("children"))) {
        out->hasChildren = true;
        const QVariantList children = map.value(QStringLiteral("children")).toList();
        for (const QVariant& childVar : children) {
            TlvNode child;
            if (!tlvFromVariant(childVar, &child, errorMessage))
                return false;
            out->children.append(child);
        }
        return true;
    }
    out->hasChildren = false;
    quint32 wide = 0;
    if (map.contains(QStringLiteral("u32")) && variantToUInt(map.value(QStringLiteral("u32")), &wide)) {
        out->value = u32be(wide);
        return true;
    }
    if (map.contains(QStringLiteral("u16")) && variantToUInt(map.value(QStringLiteral("u16")), &wide)) {
        out->value = u16be(static_cast<quint16>(wide));
        return true;
    }
    const QVariant raw = map.value(QStringLiteral("value"));
    if (!raw.isValid() || raw.isNull()) {
        out->value.clear();
        return true;
    }
    if (raw.type() == QVariant::ByteArray) {
        out->value = raw.toByteArray();
        return true;
    }
    if (raw.type() == QVariant::String) {
        const QString s = raw.toString().trimmed();
        if (s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) || s.contains(QLatin1Char(' '))) {
            out->value = QByteArray::fromHex(s.toLatin1());
            return true;
        }
        out->value = s.toUtf8();
        return true;
    }
    if (raw.canConvert<int>()) {
        quint32 n = 0;
        if (!variantToUInt(raw, &n) || n > 0xFF) {
            if (errorMessage)
                *errorMessage = QStringLiteral("value 整数需在 0~255，多字节请用 u16/u32");
            return false;
        }
        out->value = u8Bytes(static_cast<quint8>(n));
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("不支持的 value 类型");
    return false;
}

QString QaiotApp::describeMessage(const Message& message) const {
    QStringList lines;
    lines << QStringLiteral("svc=0x%1 cid=0x%2")
                 .arg(message.serviceId, 2, 16, QChar('0'))
                 .arg(message.commandId, 2, 16, QChar('0'));
    for (const TlvNode& tlv : message.tlvs)
        lines << describeTlv(tlv, 0);
    return lines.join(QStringLiteral(" | "));
}

QString QaiotApp::describeTlv(const TlvNode& tlv, int depth) const {
    const QString pad = QString(depth * 2, QLatin1Char(' '));
    if (tlv.hasChildren) {
        QStringList parts;
        parts << QStringLiteral("%1TL(type=0x%2){").arg(pad).arg(tlv.type, 2, 16, QChar('0'));
        for (const TlvNode& c : tlv.children)
            parts << describeTlv(c, depth + 1);
        parts << QStringLiteral("%1}").arg(pad);
        return parts.join(QLatin1Char(' '));
    }
    return QStringLiteral("%1TLV(type=0x%2,len=%3,val=%4)")
        .arg(pad)
        .arg(tlv.type, 2, 16, QChar('0'))
        .arg(tlv.value.size())
        .arg(hexText(tlv.value));
}
