#include "qaiot.h"

#include "Abini.h"
#include "aiot_link_defs.h"

#include <QDateTime>
#include <QDebug>
#include <QStringList>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {
constexpr int kPhyHeaderSize = 8;
constexpr uint8_t kPhyTxHeaderByte = 0xCC; // 上位机→Dongle
constexpr uint8_t kPhyRxHeaderByte = 0xAA; // Dongle→上位机（产测设备数据包）
constexpr uint8_t kPhyChannelFac = 1;

QString hexText(const QByteArray& data) {
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

bool toByteValue(const QVariant& v, quint8* out) {
    bool ok = false;
    const int n = v.toInt(&ok);
    if (!ok || n < 0 || n > 0xFF)
        return false;
    if (out)
        *out = static_cast<quint8>(n);
    return true;
}

QByteArray mapToUtf8(const QVariantMap& map, const QString& key) {
    if (!map.contains(key))
        return {};
    return map.value(key).toString().toUtf8();
}

/** 解析 device_side_id：0 Left / 1 Right / 2 Independent；默认 Independent */
quint8 parseDeviceSideIdText(const QString& raw) {
    const QString s = raw.trimmed();
    if (s.isEmpty())
        return AiotLink::kFctDeviceSideIndependent;
    bool ok = false;
    const uint n = s.toUInt(&ok);
    if (ok && n <= 2u)
        return static_cast<quint8>(n);
    const QString lower = s.toLower();
    if (lower == QLatin1String("left") || lower == QLatin1String("l") || s.contains(QStringLiteral("左")))
        return AiotLink::kFctDeviceSideLeft;
    if (lower == QLatin1String("right") || lower == QLatin1String("r") || s.contains(QStringLiteral("右")))
        return AiotLink::kFctDeviceSideRight;
    if (lower == QLatin1String("independent") || lower == QLatin1String("single") || lower == QLatin1String("s")
        || s.contains(QStringLiteral("单")) || s.contains(QStringLiteral("独立")) || lower == QLatin1String("f")
        || s.contains(QStringLiteral("未指定")))
        return AiotLink::kFctDeviceSideIndependent;
    return AiotLink::kFctDeviceSideIndependent;
}

quint8 resolveDeviceSideId(const QVariantMap& map, int sideIdOverride = -1) {
    if (sideIdOverride >= 0 && sideIdOverride <= 2)
        return static_cast<quint8>(sideIdOverride);
    static const QStringList keys = {QStringLiteral("side"),
                                     QStringLiteral("device_side_id"),
                                     QStringLiteral("deviceSideId"),
                                     QStringLiteral("sideId"),
                                     QStringLiteral("position")};
    for (const QString& key : keys) {
        if (!map.contains(key))
            continue;
        return parseDeviceSideIdText(map.value(key).toString());
    }
    // 步骤未写 Param_side 时，跟主界面「三元组位置」/ SETTINGS Tuple/Position
    const QString uiPos = SETTINGS.value(QStringLiteral("Tuple/Position")).toString().trimmed();
    if (!uiPos.isEmpty())
        return parseDeviceSideIdText(uiPos);
    return AiotLink::kFctDeviceSideIndependent;
}

QString deviceSideIdTip(quint8 side) {
    switch (side) {
    case AiotLink::kFctDeviceSideLeft:
        return QStringLiteral("Left");
    case AiotLink::kFctDeviceSideRight:
        return QStringLiteral("Right");
    case AiotLink::kFctDeviceSideIndependent:
    default:
        return QStringLiteral("Independent");
    }
}

/** 显示 MAC → 线序 6 字节（与 qroot 一致：显示首字节对应线序末字节） */
QByteArray parseMacToWire(const QVariant& data) {
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
        if (text.isEmpty())
            text = map.value(QStringLiteral("string")).toString().trimmed();
    }
    text.remove(QLatin1Char(':'));
    text.remove(QLatin1Char('-'));
    text.remove(QLatin1Char(' '));
    if (text.size() % 2 != 0)
        text.prepend(QLatin1Char('0'));
    const QByteArray hex = QByteArray::fromHex(text.toLatin1());
    if (hex.size() != 6)
        return {};
    QByteArray wire(6, '\0');
    for (int i = 0; i < 6; ++i)
        wire[i] = hex.at(5 - i);
    return wire;
}

QString macWireToDisplay(const QByteArray& wire) {
    if (wire.size() < 6)
        return {};
    QStringList parts;
    parts.reserve(6);
    for (int i = 5; i >= 0; --i) {
        parts.append(QString::number(static_cast<quint8>(wire.at(i)), 16)
                         .rightJustified(2, QLatin1Char('0'))
                         .toUpper());
    }
    return parts.join(QLatin1Char(':'));
}

QByteArray utcTimestampBe4() {
    const quint32 ts = static_cast<quint32>(QDateTime::currentSecsSinceEpoch());
    QByteArray out(4, '\0');
    out[0] = static_cast<char>((ts >> 24) & 0xFF);
    out[1] = static_cast<char>((ts >> 16) & 0xFF);
    out[2] = static_cast<char>((ts >> 8) & 0xFF);
    out[3] = static_cast<char>(ts & 0xFF);
    return out;
}

bool parseUtcTimestampBe4(const QByteArray& value, quint32* outTs) {
    if (value.size() < 4)
        return false;
    const quint32 ts = (static_cast<quint8>(value.at(0)) << 24) | (static_cast<quint8>(value.at(1)) << 16)
                       | (static_cast<quint8>(value.at(2)) << 8) | static_cast<quint8>(value.at(3));
    if (outTs)
        *outTs = ts;
    return true;
}

/** 设备数据时间戳：epoch + UTC/本地可读时间 */
QString formatDeviceDataTimestamp(quint32 ts) {
    const QDateTime utc = QDateTime::fromSecsSinceEpoch(ts, Qt::UTC);
    const QDateTime local = QDateTime::fromSecsSinceEpoch(static_cast<qint64>(ts));
    return QStringLiteral("%1 (UTC %2 / 本地 %3)")
        .arg(ts)
        .arg(utc.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")))
        .arg(local.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
}

/** FCT&ATE CID → 中文名（日志对照） */
QString fctCidName(quint8 cid) {
    switch (cid) {
    case AiotLink::kFctCidGetFactoryStatus:
        return QStringLiteral("获取产测状态");
    case AiotLink::kFctCidSetFactoryStatus:
        return QStringLiteral("设置产测状态");
    case AiotLink::kFctCidGetDeviceData:
        return QStringLiteral("获取通用设备数据");
    case AiotLink::kFctCidSetDeviceData:
        return QStringLiteral("设置通用设备数据");
    case AiotLink::kFctCidSetRfTest:
        return QStringLiteral("设置射频测试");
    case AiotLink::kFctCidGetRfData:
        return QStringLiteral("获取射频数据");
    case AiotLink::kFctCidSetRfData:
        return QStringLiteral("设置射频数据");
    case AiotLink::kFctCidGetSensor:
        return QStringLiteral("获取传感器");
    case AiotLink::kFctCidSetSensor:
        return QStringLiteral("设置传感器");
    case AiotLink::kFctCidDeviceControl:
        return QStringLiteral("设备控制");
    case AiotLink::kFctCidGetBatteryInfo:
        return QStringLiteral("获取电量信息");
    case AiotLink::kFctCidSimulateKey:
        return QStringLiteral("模拟按键");
    case AiotLink::kFctCidVirtualBattery:
        return QStringLiteral("电量模拟测试");
    case AiotLink::kFctCidDutNotify:
        return QStringLiteral("测试数据主动上报");
    default:
        return QStringLiteral("CID=0x%1").arg(cid, 2, 16, QChar('0'));
    }
}

QString fctModeName(quint8 mode) {
    switch (mode) {
    case AiotLink::kFctModeIdle:
        return QStringLiteral("空闲模式");
    case AiotLink::kFctModeFactoryTest:
        return QStringLiteral("工厂测试模式");
    case AiotLink::kFctModeAging:
        return QStringLiteral("老化测试模式");
    case AiotLink::kFctModeSuction:
        return QStringLiteral("吸力测试模式");
    case AiotLink::kFctModeSuctionCompensate:
        return QStringLiteral("吸力补偿模式");
    case AiotLink::kFctModeAte:
        return QStringLiteral("ATE自动化测试模式");
    default:
        return QStringLiteral("模式0x%1").arg(mode, 2, 16, QChar('0'));
    }
}

QString fctKeyName(quint8 key) {
    switch (key) {
    case 0x01:
        return QStringLiteral("电源按键");
    case 0x02:
        return QStringLiteral("开始按键");
    case 0x03:
        return QStringLiteral("模式按键");
    case 0x04:
        return QStringLiteral("频率按键");
    case 0x05:
        return QStringLiteral("母乳按键");
    case 0x06:
        return QStringLiteral("左控制按键");
    case 0x07:
        return QStringLiteral("右控制按键");
    case 0x08:
        return QStringLiteral("恢复出厂按键");
    case 0x09:
        return QStringLiteral("旅行锁按键");
    case 0x0A:
        return QStringLiteral("旋钮左转");
    case 0x0B:
        return QStringLiteral("旋钮右转");
    default:
        return QStringLiteral("按键0x%1").arg(key, 2, 16, QChar('0'));
    }
}

QString fctSensorName(quint8 sensorType) {
    switch (sensorType) {
    case 0x00:
        return QStringLiteral("IMU");
    case 0x01:
        return QStringLiteral("压力");
    case 0x02:
        return QStringLiteral("气流");
    case 0x03:
        return QStringLiteral("TOF");
    case 0x04:
        return QStringLiteral("电容");
    case 0x05:
        return QStringLiteral("红外");
    case 0x06:
        return QStringLiteral("生物阻抗");
    case 0x07:
        return QStringLiteral("液位");
    case 0x08:
        return QStringLiteral("温度");
    case 0x09:
        return QStringLiteral("湿度");
    case 0x0A:
        return QStringLiteral("接近");
    case 0x0B:
        return QStringLiteral("电流");
    case 0x0C:
        return QStringLiteral("霍尔");
    case 0x0D:
        return QStringLiteral("编码器");
    default:
        return QStringLiteral("传感器0x%1").arg(sensorType, 2, 16, QChar('0'));
    }
}

/** 规范 Error Code 表（通用错误码 Type=127）→ 中文描述 */
QString aiotErrorCodeText(quint32 code) {
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
    case 100009:
        return QStringLiteral("响应超时");
    case 101001:
        return QStringLiteral("入参为空/数据非法");
    case 101002:
        return QStringLiteral("设备电池电量低（禁止恢复出厂设置）");
    case 107001:
        return QStringLiteral("非法查询");
    default:
        return QStringLiteral("未知错误码");
    }
}
} // namespace

Qaiot::Qaiot(QSerialPort* parent) : qProtocol(parent), serialPort(parent) {
}

void Qaiot::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty())
        return;

    QList<QByteArray> inners;
    if (!tryUnwrapPhyPacket(byte, inners)) {
        // 兼容直连：仅无 PHY 头、且本身是 AIOT 链路 SOF 时才当内层
        if (!byte.isEmpty() && static_cast<uint8_t>(byte.at(0)) == AiotLink::kSof)
            inners.append(byte);
    }

    for (const QByteArray& inner : inners) {
        // 与 TX 同层：打 PHY 解出后的完整链路帧，并带上当前待应答指令名
        const QString act = pendingAction_.trimmed().isEmpty() ? QStringLiteral("-") : pendingAction_;
        qDebug().noquote() << QStringLiteral("[QAIOT] RX %1:").arg(act) << hexText(inner);
        QVector<AiotLinkCodec::Frame> frames;
        if (!linkCodec_.feed(inner, &frames))
            continue;
        for (const AiotLinkCodec::Frame& fr : frames)
            handleLinkFrame(fr);
    }
}

void Qaiot::handleLinkFrame(const AiotLinkCodec::Frame& frame) {
    if (frame.control & AiotLink::kCtrlRsp) {
        qDebug() << "QAIOT 链路层响应帧，忽略应用解析 control=" << frame.control;
        return;
    }

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
    } else { // end
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
        qDebug() << "QAIOT 应用层解析失败:" << errorMessage;
        return;
    }
    handleAppMessage(message);
}

void Qaiot::handleAppMessage(const Message& message) {
    // 完整 TLV 摘要只打调试日志，避免刷 UI；带上指令名便于对照步骤
    const QString act = pendingAction_.trimmed().isEmpty() ? QStringLiteral("-") : pendingAction_;
    qDebug().noquote() << QStringLiteral("QAIOT RX [%1] %2").arg(act, describeMessage(message));

    TlvNode err;
    if (findTlvDeep(message.tlvs, AiotLink::kTlvErrorCode, &err) && err.value.size() >= 4) {
        const quint32 code = (static_cast<quint8>(err.value.at(0)) << 24) |
                             (static_cast<quint8>(err.value.at(1)) << 16) |
                             (static_cast<quint8>(err.value.at(2)) << 8) |
                             static_cast<quint8>(err.value.at(3));
        const QString desc = aiotErrorCodeText(code);
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 错误码 %1（%2） action=%3")
                       .arg(code)
                       .arg(desc)
                       .arg(pendingAction_));
        emit sendGetProductResponse(0);
        return;
    }

    if (message.serviceId != AiotLink::kSvcFctAte) {
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 忽略非 FCT 服务 svc=0x%1")
                       .arg(message.serviceId, 2, 16, QChar('0')));
        return;
    }
    handleFctResponse(message);

    // 有应答即视为传输层完成（业务卡控由上层 report 判定）
    if (pendingService_ == message.serviceId && pendingCommand_ == message.commandId)
        emit sendGetProductResponse(1);
}

void Qaiot::handleFctResponse(const Message& message) {
    if (message.commandId == AiotLink::kFctCidGetFactoryStatus ||
        message.commandId == AiotLink::kFctCidSetFactoryStatus) {
        TlvNode n;
        // GET：complete=0x04；SET：complete=0x01；模式 type/status 均为 0x22/0x23
        const quint8 completeType = (message.commandId == AiotLink::kFctCidGetFactoryStatus)
                                        ? AiotLink::kFctGetTlvFactoryComplete
                                        : AiotLink::kFctSetTlvFactoryComplete;
        if (findTlvDeep(message.tlvs, completeType, &n) && !n.value.isEmpty()) {
            const bool done = static_cast<quint8>(n.value.at(0)) == 0x01;
            emitReport(QStringLiteral("ProtocolFactoryDoneData"),
                       QVariant::fromValue(ProtocolFactoryDoneData{done}));
        }

        // GET 顺带回填名称/固件/硬件/资源版本（MAC 改走 CID=0x03 Type=0x05）
        if (message.commandId == AiotLink::kFctCidGetFactoryStatus) {
            ProtocolBaseInfoData info;
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvDeviceName, &n))
                info.product_name = QString::fromUtf8(n.value);
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvFwVersion, &n))
                info.soft_version = QString::fromUtf8(n.value);
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvHwVersion, &n))
                info.hw_version = QString::fromUtf8(n.value);
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvResVersion, &n))
                info.res_version = QString::fromUtf8(n.value);

            if (!info.product_name.isEmpty() || !info.soft_version.isEmpty() || !info.hw_version.isEmpty() ||
                !info.res_version.isEmpty()) {
                emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 产测状态 name=%1 soft=%2 hw=%3 res=%4")
                               .arg(info.product_name, info.soft_version, info.hw_version, info.res_version));
            }
        }

        // GET/SET 工厂模式字段 Type 已统一为 0x22/0x23
        quint8 modeType = 0xFF;
        quint8 modeStatus = 0xFF;
        if (findTlvDeep(message.tlvs, AiotLink::kFctGetTlvModeType, &n) && !n.value.isEmpty())
            modeType = static_cast<quint8>(n.value.at(0));
        if (findTlvDeep(message.tlvs, AiotLink::kFctGetTlvModeStatus, &n) && !n.value.isEmpty())
            modeStatus = static_cast<quint8>(n.value.at(0));
        if (modeType != 0xFF || modeStatus != 0xFF) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 工厂模式 type=0x%1 status=0x%2")
                           .arg(modeType, 2, 16, QChar('0'))
                           .arg(modeStatus, 2, 16, QChar('0')));
        }
    } else if (message.commandId == AiotLink::kFctCidGetDeviceData) {
        // device_data_type：01 SN / 02~04 三元组 / 05 MAC；另有 side(0x01)/时间戳(0x02)
        QString prod, dev, key, sn, macText;
        ProtocolBaseInfoData macInfo;
        QString sideTip;
        QString tsTip;
        TlvNode meta;
        if (findTlvDeep(message.tlvs, AiotLink::kFctDeviceSideId, &meta) && !meta.value.isEmpty())
            sideTip = deviceSideIdTip(static_cast<quint8>(meta.value.at(0)));
        quint32 ts = 0;
        if (findTlvDeep(message.tlvs, AiotLink::kFctDeviceDataTimestamp, &meta)
            && parseUtcTimestampBe4(meta.value, &ts))
            tsTip = formatDeviceDataTimestamp(ts);

        QList<TlvNode> structs;
        for (const TlvNode& t : message.tlvs) {
            if (t.type == 0x04)
                structs.append(t);
            for (const TlvNode& c : t.children) {
                if (c.type == 0x04)
                    structs.append(c);
                for (const TlvNode& cc : c.children) {
                    if (cc.type == 0x04)
                        structs.append(cc);
                }
            }
        }
        for (const TlvNode& st : structs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(st.children, 0x05, &typeNode) || !findTlv(st.children, 0x06, &dataNode))
                continue;
            if (typeNode.value.isEmpty())
                continue;
            const quint8 dt = static_cast<quint8>(typeNode.value.at(0));
            if (dt == AiotLink::kFctDataTypeMac) {
                if (dataNode.value.size() < 6)
                    continue;
                macInfo.ble_mac.size = 6;
                for (int i = 0; i < 6; ++i)
                    macInfo.ble_mac.bytes[i] = static_cast<uint8_t>(dataNode.value.at(i));
                macText = macWireToDisplay(dataNode.value);
                continue;
            }
            const QString text = QString::fromUtf8(dataNode.value);
            if (dt == AiotLink::kFctDataTypeSn)
                sn = text;
            else if (dt == AiotLink::kFctDataTypeProductId)
                prod = text;
            else if (dt == AiotLink::kFctDataTypeDeviceId)
                dev = text;
            else if (dt == AiotLink::kFctDataTypeDeviceSecret)
                key = text;
        }
        if (!sn.isEmpty())
            emitReport(QStringLiteral("ProtocolSnData"),
                       QVariant::fromValue(ProtocolSnData{ProtocolSnType::TailSn, sn}));
        if (!prod.isEmpty() || !dev.isEmpty() || !key.isEmpty())
            emitReport(QStringLiteral("ProtocolTupleData"),
                       QVariant::fromValue(ProtocolTupleData{prod, dev, key}));
        if (!macText.isEmpty()) {
            emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(macInfo));
            emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(ProtocolMacData{macText}));
        }

        QStringList parts;
        if (!sideTip.isEmpty())
            parts << QStringLiteral("side=%1").arg(sideTip);
        if (!tsTip.isEmpty())
            parts << QStringLiteral("时间戳=%1").arg(tsTip);
        if (!sn.isEmpty())
            parts << QStringLiteral("SN=%1").arg(sn);
        if (!prod.isEmpty())
            parts << QStringLiteral("productKey=%1").arg(prod);
        if (!dev.isEmpty())
            parts << QStringLiteral("deviceName=%1").arg(dev);
        if (!key.isEmpty())
            parts << QStringLiteral("deviceSecret=%1").arg(key);
        if (!macText.isEmpty())
            parts << QStringLiteral("MAC=%1").arg(macText);
        if (!parts.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 设备数据 %1").arg(parts.join(QLatin1Char(' '))));
        }
    } else if (message.commandId == AiotLink::kFctCidGetRfData) {
        TlvNode n;
        if (findTlv(message.tlvs, 0x01, &n) && n.value.size() >= 1) {
            int rssi = static_cast<qint8>(static_cast<quint8>(n.value.at(0)));
            if (n.value.size() >= 2)
                rssi = static_cast<qint16>((static_cast<quint8>(n.value.at(0)) << 8) |
                                           static_cast<quint8>(n.value.at(1)));
            emitReport(QStringLiteral("ProtocolRssiData"), QVariant::fromValue(ProtocolRssiData{rssi}));
        }
    } else if (message.commandId == AiotLink::kFctCidGetBatteryInfo) {
        TlvNode n;
        ProtocolBatteryData batteryData;
        if (findTlvDeep(message.tlvs, 0x02, &n) && !n.value.isEmpty())
            batteryData.percent = static_cast<int>(static_cast<quint8>(n.value.at(0)));
        else if (findTlvDeep(message.tlvs, 0x01, &n) && !n.value.isEmpty() && !n.hasChildren)
            batteryData.percent = static_cast<int>(static_cast<quint8>(n.value.at(0)));
        if (findTlvDeep(message.tlvs, 0x03, &n) && n.value.size() >= 2) {
            batteryData.voltageMv = (static_cast<quint8>(n.value.at(0)) << 8) |
                                    static_cast<quint8>(n.value.at(1));
        }
        if (findTlvDeep(message.tlvs, 0x04, &n) && n.value.size() >= 2) {
            // battery_current 为 int16 BE（放电为负）；勿按无符号拼装
            const quint16 raw = static_cast<quint16>((static_cast<quint8>(n.value.at(0)) << 8) |
                                                     static_cast<quint8>(n.value.at(1)));
            batteryData.currentMa = static_cast<int>(static_cast<qint16>(raw));
        }
        if (findTlvDeep(message.tlvs, 0x05, &n) && !n.value.isEmpty())
            batteryData.temperatureC = static_cast<int>(static_cast<qint8>(static_cast<quint8>(n.value.at(0))));
        emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(batteryData));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 电池 percent=%1 voltage=%2mV current=%3mA temp=%4C")
                       .arg(batteryData.percent)
                       .arg(batteryData.voltageMv)
                       .arg(batteryData.currentMa)
                       .arg(batteryData.temperatureC));
    } else if (message.commandId == AiotLink::kFctCidGetSensor) {
        TlvNode typeNode, dataNode;
        QString tip = QStringLiteral("读传感器");
        if (findTlvDeep(message.tlvs, 0x03, &typeNode) && !typeNode.value.isEmpty()) {
            const quint8 st = static_cast<quint8>(typeNode.value.at(0));
            tip = QStringLiteral("读%1传感器").arg(fctSensorName(st));
        }
        QString dataHex;
        if (findTlvDeep(message.tlvs, 0x04, &dataNode))
            dataHex = hexText(dataNode.value);
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT %1 data=%2").arg(tip, dataHex.isEmpty() ? QStringLiteral("-") : dataHex));
    } else if (message.commandId == AiotLink::kFctCidDutNotify) {
        // CID=0x1A 主动上报：list(0x01)/struct(0x02)/type(0x03)/value(0x04)；type=0x00 为按键
        QList<TlvNode> structs;
        TlvNode listNode;
        if (findTlv(message.tlvs, AiotLink::kFctDutNotifyList, &listNode)) {
            for (const TlvNode& c : listNode.children) {
                if (c.type == AiotLink::kFctDutNotifyStruct)
                    structs.append(c);
            }
        }
        if (structs.isEmpty()) {
            for (const TlvNode& t : message.tlvs) {
                if (t.type == AiotLink::kFctDutNotifyStruct)
                    structs.append(t);
                for (const TlvNode& c : t.children) {
                    if (c.type == AiotLink::kFctDutNotifyStruct)
                        structs.append(c);
                }
            }
        }
        for (const TlvNode& st : structs) {
            TlvNode typeNode;
            TlvNode valueNode;
            if (!findTlv(st.children, AiotLink::kFctDutNotifyType, &typeNode) || typeNode.value.isEmpty())
                continue;
            if (!findTlv(st.children, AiotLink::kFctDutNotifyValue, &valueNode) || valueNode.value.isEmpty())
                continue;
            const quint8 notifyType = static_cast<quint8>(typeNode.value.at(0));
            if (notifyType != AiotLink::kFctDutNotifyTypeVirtualKey) {
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 主动上报 type=0x%1 value=%2")
                               .arg(notifyType, 2, 16, QChar('0'))
                               .arg(hexText(valueNode.value)));
                continue;
            }
            const quint8 key = static_cast<quint8>(valueNode.value.at(0));
            ProtocolButtonStateData btn;
            btn.keyButtonId = static_cast<int>(key);
            // 与 FCTP 对齐：普通键 mode=短按(1)；旋钮用 mode 表示方向 1左/2右
            if (key == 0x0A)
                btn.modeButtonState = 1;
            else if (key == 0x0B)
                btn.modeButtonState = 2;
            else
                btn.modeButtonState = 1;
            if (key == 0x01)
                btn.powerButtonState = 1;
            emitReport(QStringLiteral("ProtocolButtonStateData"), QVariant::fromValue(btn));
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 按键上报 %1(0x%2)")
                           .arg(fctKeyName(key))
                           .arg(key, 2, 16, QChar('0')));
        }
    }
}

void Qaiot::set(DeviceCmd cmd, const QVariant& data) {
    const QVariantMap map = data.toMap();
    switch (cmd) {
    case DeviceCmd::FacMode: {
        // SET CID=0x02：list(0x20)/struct(0x21)/type(0x22)/status(0x23)；Param_mode 指定工厂模式类型
        int enable = 1;
        if (!map.isEmpty()) {
            enable = map.value(QStringLiteral("on"),
                               map.value(QStringLiteral("value"), map.value(QStringLiteral("switch"), 1)))
                         .toInt();
        } else if (data.canConvert<int>()) {
            enable = data.toInt();
        }
        const quint8 modeType = static_cast<quint8>(
            map.value(QStringLiteral("mode"), AiotLink::kFctModeFactoryTest).toInt());
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(modeType)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctGetTlvModeList,
                       {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})};
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus, tlvs,
                           QStringLiteral("%1%2").arg(enable ? QStringLiteral("进入") : QStringLiteral("退出"),
                                                     fctModeName(modeType)));
        break;
    }
    case DeviceCmd::BurningMode: {
        const int enable = map.value(QStringLiteral("switch"), map.value(QStringLiteral("enter"), 1)).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeAging)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctGetTlvModeList,
                        {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})},
            enable ? QStringLiteral("进入老化测试模式") : QStringLiteral("退出老化测试模式"));
        break;
    }
    case DeviceCmd::SuctionMode: {
        const int enable = map.value(QStringLiteral("on"), map.value(QStringLiteral("switch"), 1)).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeSuction)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctGetTlvModeList,
                        {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})},
            enable ? QStringLiteral("进入吸力测试模式") : QStringLiteral("退出吸力测试模式"));
        break;
    }
    case DeviceCmd::CompensationSet: {
        // 吸力补偿模式 Type=0x04
        const int enable = map.value(QStringLiteral("on"), map.value(QStringLiteral("switch"), map.value(QStringLiteral("value"), 1))).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeSuctionCompensate)));
        modeChildren.append(makeLeaf(AiotLink::kFctGetTlvModeStatus, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctGetTlvModeList,
                        {makeParent(AiotLink::kFctGetTlvModeStruct, modeChildren)})},
            enable ? QStringLiteral("进入吸力补偿模式") : QStringLiteral("退出吸力补偿模式"));
        break;
    }
    case DeviceCmd::FacResult: {
        quint8 done = 0x01;
        if (data.canConvert<int>())
            done = data.toInt() ? 0x01 : 0x00;
        else if (map.contains(QStringLiteral("done")))
            done = map.value(QStringLiteral("done")).toInt() ? 0x01 : 0x00;
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
                           {makeLeaf(AiotLink::kFctSetTlvFactoryComplete, u8(done))},
                           QStringLiteral("写产测完成标识"));
        break;
    }
    case DeviceCmd::BtSignalMode:
    case DeviceCmd::BtNoSignalMode:
    case DeviceCmd::BtFreqMode: {
        quint8 rfType = 0x00;
        if (cmd == DeviceCmd::BtNoSignalMode)
            rfType = 0x01;
        else if (cmd == DeviceCmd::BtFreqMode)
            rfType = 0x02;
        const int enable = map.value(QStringLiteral("on"), 1).toInt();
        QList<TlvNode> children;
        children.append(makeLeaf(0x02, u8(rfType)));
        children.append(makeLeaf(0x03, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetRfTest, {makeParent(0x01, children)},
                           QStringLiteral("设置射频测试"));
        break;
    }
    case DeviceCmd::TrimSet: {
        const quint8 trim = static_cast<quint8>(map.value(QStringLiteral("trim"), map.value(QStringLiteral("value"))).toUInt());
        const quint8 power = static_cast<quint8>(map.value(QStringLiteral("power"), 0).toUInt());
        QList<TlvNode> children;
        children.append(makeLeaf(0x03, u8(power)));
        children.append(makeLeaf(0x04, u8(trim)));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetRfData, {makeParent(0x02, children)},
                           QStringLiteral("写射频 trim"));
        break;
    }
    case DeviceCmd::Sn:
    case DeviceCmd::WriteKey:
    case DeviceCmd::MacWrite: {
        // CID=0x04：device_side_id + UTC 时间戳 + data_type(01 SN / 02~04 三元组 / 05 MAC)
        QList<TlvNode> items;
        int sideOverride = -1;
        auto appendItem = [&](quint8 dataType, const QByteArray& bytes) {
            if (bytes.isEmpty())
                return;
            QList<TlvNode> ch;
            ch.append(makeLeaf(0x05, u8(dataType)));
            ch.append(makeLeaf(0x06, bytes));
            items.append(makeParent(0x04, ch));
        };
        if (cmd == DeviceCmd::MacWrite) {
            const QByteArray mac6 = parseMacToWire(data.isValid() ? data : QVariant(map));
            if (mac6.isEmpty()) {
                emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写 MAC 格式无效"));
                return;
            }
            appendItem(AiotLink::kFctDataTypeMac, mac6);
        } else if (data.canConvert<DeviceSnPayload>()) {
            const DeviceSnPayload payload = data.value<DeviceSnPayload>();
            sideOverride = payload.sideId;
            quint8 dataType = AiotLink::kFctDataTypeSn;
            if (payload.which_sn == FacDevInfoType_SKUID)
                dataType = AiotLink::kFctDataTypeProductId;
            else if (payload.which_sn == FacDevInfoType_SUB_PID)
                dataType = AiotLink::kFctDataTypeDeviceId;
            appendItem(dataType, payload.sn);
        } else {
            appendItem(AiotLink::kFctDataTypeSn, mapToUtf8(map, QStringLiteral("sn")));
            QByteArray productId = mapToUtf8(map, QStringLiteral("productId"));
            if (productId.isEmpty())
                productId = mapToUtf8(map, QStringLiteral("productKey"));
            QByteArray deviceId = mapToUtf8(map, QStringLiteral("deviceId"));
            if (deviceId.isEmpty())
                deviceId = mapToUtf8(map, QStringLiteral("deviceName"));
            QByteArray deviceSecret = mapToUtf8(map, QStringLiteral("deviceSecret"));
            if (deviceSecret.isEmpty())
                deviceSecret = mapToUtf8(map, QStringLiteral("key"));
            if (deviceSecret.isEmpty())
                deviceSecret = mapToUtf8(map, QStringLiteral("value"));
            appendItem(AiotLink::kFctDataTypeProductId, productId);
            appendItem(AiotLink::kFctDataTypeDeviceId, deviceId);
            appendItem(AiotLink::kFctDataTypeDeviceSecret, deviceSecret);
            if (items.isEmpty() && data.type() == QVariant::String)
                appendItem(AiotLink::kFctDataTypeSn, data.toString().toUtf8());
        }
        if (items.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写设备数据缺少字段"));
            return;
        }
        const quint8 side = resolveDeviceSideId(map, sideOverride);
        const QByteArray tsBytes = utcTimestampBe4();
        quint32 ts = 0;
        parseUtcTimestampBe4(tsBytes, &ts);
        QList<TlvNode> tlvs;
        tlvs.append(makeLeaf(AiotLink::kFctDeviceSideId, u8(side)));
        tlvs.append(makeLeaf(AiotLink::kFctDeviceDataTimestamp, tsBytes)); // device_data_timestap（必选）
        tlvs.append(makeParent(0x03, items));
        QString tip = QStringLiteral("写设备数据");
        if (cmd == DeviceCmd::MacWrite)
            tip = QStringLiteral("写MAC");
        else if (cmd == DeviceCmd::WriteKey)
            tip = QStringLiteral("写三元组");
        tip += QStringLiteral(" side=%1 时间戳=%2").arg(deviceSideIdTip(side), formatDeviceDataTimestamp(ts));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetDeviceData, tlvs, tip);
        break;
    }
    case DeviceCmd::ButtonState: {
        // 模拟按键 CID=0x10：Param_int / Param_key = 0x01~0x0B
        quint8 key = 0;
        if (map.contains(QStringLiteral("key")))
            key = static_cast<quint8>(map.value(QStringLiteral("key")).toUInt());
        else if (map.contains(QStringLiteral("int")))
            key = static_cast<quint8>(map.value(QStringLiteral("int")).toUInt());
        else if (data.canConvert<int>())
            key = static_cast<quint8>(data.toUInt());
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSimulateKey, {makeLeaf(0x01, u8(key))},
                           QStringLiteral("模拟%1").arg(fctKeyName(key)));
        break;
    }
    case DeviceCmd::SetBattery: {
        // CID=0x13：可选 percent/voltageMv/currentMa/temperatureC（各通道独立；置 0=该通道真实值）
        const bool hasPercent = map.contains(QStringLiteral("percent"))
                                || map.contains(QStringLiteral("simbatterypercent"));
        const bool hasVoltage = map.contains(QStringLiteral("voltageMv"))
                                || map.contains(QStringLiteral("voltage"))
                                || map.contains(QStringLiteral("mV"))
                                || map.contains(QStringLiteral("simbatteryvoltagemv"));
        const bool hasCurrent = map.contains(QStringLiteral("currentMa"))
                                || map.contains(QStringLiteral("current"))
                                || map.contains(QStringLiteral("simbatterycurrentma"));
        const bool hasTemp = map.contains(QStringLiteral("temperatureC"))
                             || map.contains(QStringLiteral("temperature"))
                             || map.contains(QStringLiteral("temp"))
                             || map.contains(QStringLiteral("simbatterytemperaturec"));

        QList<TlvNode> tlvs;
        QStringList tipParts;
        if (hasPercent) {
            const quint8 pct = static_cast<quint8>(
                map.value(QStringLiteral("percent"), map.value(QStringLiteral("simbatterypercent")))
                    .toUInt());
            tlvs.append(makeLeaf(AiotLink::kFctVirtualBattPercent, u8(pct)));
            tipParts << QStringLiteral("%1%").arg(pct);
        }
        quint16 mv = 0;
        bool setMv = false;
        if (hasVoltage) {
            if (map.contains(QStringLiteral("voltageMv")))
                mv = static_cast<quint16>(map.value(QStringLiteral("voltageMv")).toUInt());
            else if (map.contains(QStringLiteral("simbatteryvoltagemv")))
                mv = static_cast<quint16>(map.value(QStringLiteral("simbatteryvoltagemv")).toUInt());
            else if (map.contains(QStringLiteral("voltage")))
                mv = static_cast<quint16>(map.value(QStringLiteral("voltage")).toUInt());
            else
                mv = static_cast<quint16>(map.value(QStringLiteral("mV")).toUInt());
            setMv = true;
        } else if (map.contains(QStringLiteral("value"))) {
            // 兼容旧 Param_value=mV（现电压 TLV 为 0x02）
            mv = static_cast<quint16>(map.value(QStringLiteral("value")).toUInt());
            setMv = true;
        } else if (!hasPercent && !hasCurrent && !hasTemp && data.canConvert<int>()) {
            mv = static_cast<quint16>(data.toUInt());
            setMv = true;
        }
        if (setMv) {
            tlvs.append(makeLeaf(AiotLink::kFctVirtualBattVoltageMv, u16be(mv)));
            tipParts << QStringLiteral("%1mV").arg(mv);
        }
        if (hasCurrent) {
            const qint16 ma = static_cast<qint16>(
                map.value(QStringLiteral("currentMa"),
                          map.value(QStringLiteral("current"),
                                    map.value(QStringLiteral("simbatterycurrentma"))))
                    .toInt());
            tlvs.append(makeLeaf(AiotLink::kFctVirtualBattCurrentMa, u16be(static_cast<quint16>(ma))));
            tipParts << QStringLiteral("%1mA").arg(ma);
        }
        if (hasTemp) {
            const qint8 tc = static_cast<qint8>(
                map.value(QStringLiteral("temperatureC"),
                          map.value(QStringLiteral("temperature"),
                                    map.value(QStringLiteral("temp"),
                                              map.value(QStringLiteral("simbatterytemperaturec")))))
                    .toInt());
            tlvs.append(makeLeaf(AiotLink::kFctVirtualBattTempC, u8(static_cast<quint8>(tc))));
            tipParts << QStringLiteral("%1°C").arg(tc);
        }
        if (tlvs.isEmpty()) {
            qWarning() << "[QAIOT] SetBattery CID=0x13 缺少 percent/voltageMv/currentMa/temperatureC";
            break;
        }
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidVirtualBattery, tlvs,
                           tipParts.join(QStringLiteral(" ")));
        break;
    }
    case DeviceCmd::LightCalibWrite: {
        // CID=0x09：写传感器校准/配置；Param_type + Param_data(hex，可空则写 00)
        quint8 sensorType = 0x05;
        if (map.contains(QStringLiteral("type")))
            sensorType = static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
        else if (map.contains(QStringLiteral("sensorType")))
            sensorType = static_cast<quint8>(map.value(QStringLiteral("sensorType")).toUInt());
        QByteArray calib = QByteArray::fromHex(
            map.value(QStringLiteral("data"), map.value(QStringLiteral("value"))).toString().toLatin1());
        if (calib.isEmpty())
            calib = QByteArray(1, '\0');
        const QList<TlvNode> tlvs = {
            makeParent(0x01,
                       {makeParent(0x02,
                                   {makeLeaf(0x03, u8(sensorType)), makeLeaf(0x04, calib)})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetSensor, tlvs,
                           QStringLiteral("写%1传感器校准").arg(fctSensorName(sensorType)));
        break;
    }
    case DeviceCmd::ShipMode:
    case DeviceCmd::DevReset:
    case DeviceCmd::FactoryReset: {
        // 设备控制命令：用 type 区分（规范字段较多，先发最小控制字）
        quint8 ctrl = 0x01;
        if (cmd == DeviceCmd::DevReset)
            ctrl = 0x02;
        else if (cmd == DeviceCmd::FactoryReset)
            ctrl = 0x03;
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidDeviceControl, {makeLeaf(0x01, u8(ctrl))},
                           QStringLiteral("设备控制"));
        break;
    }
    default:
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 暂未映射 set 命令，cmd=%1").arg(static_cast<int>(cmd)));
        break;
    }
}

void Qaiot::get(DeviceCmd cmd, const QVariant& param) {
    const QVariantMap map = param.toMap();
    // GET：请求侧带 Type（Value 可空，Length=0）标明要查询的字段
    auto queryLeaf = [this](quint8 type) { return makeLeaf(type, {}); };

    switch (cmd) {
    case DeviceCmd::SoftVersionRead: {
        // CID=0x01：Param_field=soft_version|hw_version|res_version（默认固件）
        const QString field = map.value(QStringLiteral("field")).toString().trimmed().toLower();
        quint8 tlvType = AiotLink::kFctGetTlvFwVersion;
        QString tip = QStringLiteral("读固件版本");
        if (field == QLatin1String("hw_version") || field == QLatin1String("hw")) {
            tlvType = AiotLink::kFctGetTlvHwVersion;
            tip = QStringLiteral("读硬件版本");
        } else if (field == QLatin1String("res_version") || field == QLatin1String("res")) {
            tlvType = AiotLink::kFctGetTlvResVersion;
            tip = QStringLiteral("读资源版本");
        }
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus, {queryLeaf(tlvType)}, tip);
        break;
    }
    case DeviceCmd::BaseInfo:
    case DeviceCmd::DeviceInfo:
        // CID=0x01：只查设备名称 Type=0x01
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus,
                           {queryLeaf(AiotLink::kFctGetTlvDeviceName)}, QStringLiteral("读设备名称"));
        break;
    case DeviceCmd::MacRead: {
        // CID=0x03：读 device_mac_address（Type=0x05）
        const quint8 side = resolveDeviceSideId(map);
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidGetDeviceData,
            {makeLeaf(AiotLink::kFctDeviceSideId, u8(side)),
             makeParent(0x03, {makeParent(0x04, {makeLeaf(0x05, u8(AiotLink::kFctDataTypeMac))})})},
            QStringLiteral("读MAC side=%1").arg(deviceSideIdTip(side)));
        break;
    }
    case DeviceCmd::GetBattery: {
        // CID=0x0E：Param_field=percent|voltage|current|temperature（默认查整包 struct 0x01）
        const QString field = map.value(QStringLiteral("field")).toString().trimmed().toLower();
        QList<TlvNode> tlvs;
        QString tip = QStringLiteral("读电量");
        if (field == QLatin1String("percent") || field == QLatin1String("battery_percent")) {
            tlvs = {makeParent(0x01, {queryLeaf(0x02)})};
            tip = QStringLiteral("读电池百分比");
        } else if (field == QLatin1String("voltage") || field == QLatin1String("voltageMv") ||
                   field == QLatin1String("battery_voltage")) {
            tlvs = {makeParent(0x01, {queryLeaf(0x03)})};
            tip = QStringLiteral("读电池电压");
        } else if (field == QLatin1String("current") || field == QLatin1String("currentMa") ||
                   field == QLatin1String("battery_current")) {
            tlvs = {makeParent(0x01, {queryLeaf(0x04)})};
            tip = QStringLiteral("读电池电流");
        } else if (field == QLatin1String("temperature") || field == QLatin1String("temperatureC") ||
                   field == QLatin1String("battery_temperature")) {
            tlvs = {makeParent(0x01, {queryLeaf(0x05)})};
            tip = QStringLiteral("读电池温度");
        } else {
            tlvs = {queryLeaf(0x01)};
        }
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetBatteryInfo, tlvs, tip);
        break;
    }
    case DeviceCmd::FactoryDoneRead:
    case DeviceCmd::FacResult:
        // 只查产测完成标识 Type=0x04，不附带模式列表 0x20
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus,
                           {queryLeaf(AiotLink::kFctGetTlvFactoryComplete)}, QStringLiteral("读产测状态"));
        break;
    case DeviceCmd::TupleRead:
    case DeviceCmd::Sn: {
        // CID=0x03：device_side_id + data_type；TupleRead 可经 Param_dataType/Param_type 只查单项
        QList<TlvNode> items;
        auto appendDataType = [&](quint8 dataType) {
            items.append(makeParent(0x04, {makeLeaf(0x05, u8(dataType))}));
        };
        if (cmd == DeviceCmd::Sn) {
            appendDataType(AiotLink::kFctDataTypeSn);
        } else {
            quint8 onlyType = 0;
            if (map.contains(QStringLiteral("dataType")))
                onlyType = static_cast<quint8>(map.value(QStringLiteral("dataType")).toUInt());
            else if (map.contains(QStringLiteral("type")))
                onlyType = static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
            if (onlyType == AiotLink::kFctDataTypeProductId || onlyType == AiotLink::kFctDataTypeDeviceId ||
                onlyType == AiotLink::kFctDataTypeDeviceSecret) {
                appendDataType(onlyType);
            } else {
                appendDataType(AiotLink::kFctDataTypeProductId);
                appendDataType(AiotLink::kFctDataTypeDeviceId);
                appendDataType(AiotLink::kFctDataTypeDeviceSecret);
            }
        }
        const quint8 side = resolveDeviceSideId(map);
        const QList<TlvNode> tlvs = {
            makeLeaf(AiotLink::kFctDeviceSideId, u8(side)),
            makeParent(0x03, items),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetDeviceData, tlvs,
                           QStringLiteral("读设备数据/三元组 side=%1").arg(deviceSideIdTip(side)));
        break;
    }
    case DeviceCmd::RssiRead:
    case DeviceCmd::TrimRead: {
        // CID=0x06：rssi=0x01 / trim=0x02
        QList<TlvNode> tlvs;
        if (cmd == DeviceCmd::RssiRead)
            tlvs.append(queryLeaf(0x01));
        if (cmd == DeviceCmd::TrimRead)
            tlvs.append(queryLeaf(0x02));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetRfData, tlvs, QStringLiteral("读射频数据"));
        break;
    }
    case DeviceCmd::PeriphState:
    case DeviceCmd::LightSensorInfo: {
        // CID=0x08：list/struct/sensor_type；Param_type=0x00~0x0D，默认红外
        quint8 sensorType = 0x05;
        if (map.contains(QStringLiteral("type")))
            sensorType = static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
        else if (map.contains(QStringLiteral("sensorType")))
            sensorType = static_cast<quint8>(map.value(QStringLiteral("sensorType")).toUInt());
        else if (param.type() != QVariant::Map && param.canConvert<int>())
            sensorType = static_cast<quint8>(param.toInt());
        const QList<TlvNode> tlvs = {
            makeParent(0x01, {makeParent(0x02, {makeLeaf(0x03, u8(sensorType))})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetSensor, tlvs,
                           QStringLiteral("读%1传感器").arg(fctSensorName(sensorType)));
        break;
    }
    case DeviceCmd::AgingStatusRead: {
        // 查询工厂模式使能；Param_mode 指定类型（默认老化）
        const quint8 modeType =
            static_cast<quint8>(map.value(QStringLiteral("mode"), AiotLink::kFctModeAging).toUInt());
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctGetTlvModeList,
                       {makeParent(AiotLink::kFctGetTlvModeStruct,
                                   {makeLeaf(AiotLink::kFctGetTlvModeType, u8(modeType)),
                                    queryLeaf(AiotLink::kFctGetTlvModeStatus)})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus, tlvs,
                           QStringLiteral("读%1状态").arg(fctModeName(modeType)));
        break;
    }
    default:
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 暂未映射 get 命令，cmd=%1").arg(static_cast<int>(cmd)));
        break;
    }
}

bool Qaiot::sendCustomMessage(const QVariantMap& map) {
    quint8 serviceId = AiotLink::kSvcFctAte;
    quint8 commandId = 0;
    if (!toByteValue(map.value(QStringLiteral("commandId")), &commandId)) {
        qWarning() << "QAIOT 通用发送缺少或非法 commandId";
        return false;
    }
    // Qaiot 固定 Service=0x04；若传入其它值则忽略并告警
    quint8 reqService = AiotLink::kSvcFctAte;
    if (toByteValue(map.value(QStringLiteral("serviceId")), &reqService) &&
        reqService != AiotLink::kSvcFctAte) {
        qWarning() << "QAIOT 仅支持 svc=0x04，已忽略 serviceId=" << reqService;
    }
    serviceId = AiotLink::kSvcFctAte;

    QList<TlvNode> tlvs;
    QString errorMessage;
    const QVariant tlvListValue = map.value(QStringLiteral("tlvs"));
    if (tlvListValue.isValid()) {
        const QVariantList list = tlvListValue.toList();
        for (const QVariant& item : list) {
            TlvNode node;
            if (!tlvFromVariant(item, &node, &errorMessage)) {
                qWarning() << "QAIOT TLV 参数非法:" << errorMessage;
                return false;
            }
            tlvs.append(node);
        }
    } else if (map.contains(QStringLiteral("type"))) {
        TlvNode node;
        if (!tlvFromVariant(map, &node, &errorMessage)) {
            qWarning() << "QAIOT TLV 参数非法:" << errorMessage;
            return false;
        }
        tlvs.append(node);
    }

    return sendServiceCommand(serviceId, commandId, tlvs, map.value(QStringLiteral("action")).toString());
}

bool Qaiot::sendServiceCommand(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs,
                               const QString& actionName) {
    Q_UNUSED(serviceId);
    // AIOT 上位机路径只走 FCT&ATE Service 0x04
    const quint8 svc = AiotLink::kSvcFctAte;
    pendingService_ = svc;
    pendingCommand_ = commandId;
    pendingAction_ = actionName.trimmed().isEmpty() ? fctCidName(commandId) : actionName.trimmed();
    const QByteArray app = buildMessage(svc, commandId, tlvs);
    emitReport(QStringLiteral("ProtocolPbDate"),
               QStringLiteral("QAIOT TX [%1] svc=0x%2 cid=0x%3")
                   .arg(pendingAction_)
                   .arg(svc, 2, 16, QChar('0'))
                   .arg(commandId, 2, 16, QChar('0')));
    return sendAppPdu(app);
}

bool Qaiot::sendAppPdu(const QByteArray& appPdu) {
    if (!serialPort || !serialPort->isOpen()) {
        qWarning() << "QAIOT 串口未打开，未发送数据";
        return false;
    }
    const QVector<QByteArray> linkFrames = AiotLinkCodec::buildFramesForPdu(appPdu);
    for (const QByteArray& link : linkFrames) {
        const QByteArray phy = wrapPhyPacket(link);
        if (phy.isEmpty())
            return false;
        const qint64 n = serialPort->write(phy);
        // 与 qroot 一致：完整 PHY（8×CC+Len+Ch+链路帧）不刷屏，只打内层链路帧 + 指令名
        qDebug().noquote() << QStringLiteral("[QAIOT] TX %1:").arg(pendingAction_) << hexText(link);
        if (n != phy.size()) {
            qWarning() << "QAIOT 发送不完整" << n << "/" << phy.size();
        return false;
        }
    }
    return true;
}

QByteArray Qaiot::wrapPhyPacket(const QByteArray& innerPacket) const {
    if (innerPacket.isEmpty() || innerPacket.size() > 0xFF)
        return {};
    QByteArray out;
    out.reserve(kPhyHeaderSize + 2 + innerPacket.size());
    out.append(QByteArray(kPhyHeaderSize, static_cast<char>(kPhyTxHeaderByte)));
    out.append(static_cast<char>(innerPacket.size()));
    out.append(static_cast<char>(kPhyChannelFac));
    out.append(innerPacket);
    return out;
}

bool Qaiot::tryUnwrapPhyPacket(const QByteArray& packet, QList<QByteArray>& outPackets) {
    const auto resetPhy = [this]() {
        phyState_ = PhyIdle;
        phyHeaderHits_ = 0;
        phyExpectedLen_ = 0;
        phyChannel_ = 0;
        phyPayload_.clear();
    };

    for (unsigned char x : packet) {
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
                    phyState_ = PhyChannel; // 收包：通道在前（见 dongle协议.md）
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
                qWarning() << "[QAIOT] dongle 外层包长度非法:" << phyExpectedLen_;
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
                if (phyChannel_ == kPhyChannelFac || phyChannel_ == 2 || phyChannel_ == 3)
                    outPackets.append(phyPayload_);
                else
                    qWarning() << "[QAIOT] dongle 通道异常 channel=" << phyChannel_;
                resetPhy();
            }
            break;
        default:
            resetPhy();
            break;
        }
    }
    return !outPackets.isEmpty();
}

Qaiot::TlvNode Qaiot::makeLeaf(quint8 type, const QByteArray& value) const {
    TlvNode n;
    n.type = type & 0x7F;
    n.hasChildren = false;
    n.value = value;
    return n;
}

Qaiot::TlvNode Qaiot::makeParent(quint8 type, const QList<TlvNode>& children) const {
    TlvNode n;
    n.type = type & 0x7F;
    n.hasChildren = true;
    n.children = children;
    return n;
}

QByteArray Qaiot::u8(quint8 v) const {
    return QByteArray(1, static_cast<char>(v));
}

QByteArray Qaiot::u16be(quint16 v) const {
    QByteArray b(2, Qt::Uninitialized);
    b[0] = static_cast<char>((v >> 8) & 0xFF);
    b[1] = static_cast<char>(v & 0xFF);
    return b;
}

QByteArray Qaiot::u32be(quint32 v) const {
    QByteArray b(4, Qt::Uninitialized);
    b[0] = static_cast<char>((v >> 24) & 0xFF);
    b[1] = static_cast<char>((v >> 16) & 0xFF);
    b[2] = static_cast<char>((v >> 8) & 0xFF);
    b[3] = static_cast<char>(v & 0xFF);
    return b;
}

bool Qaiot::findTlv(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const {
    for (const TlvNode& t : tlvs) {
        if (t.type == type) {
            if (out)
                *out = t;
            return true;
        }
    }
        return false;
    }

bool Qaiot::findTlvDeep(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const {
    if (findTlv(tlvs, type, out))
        return true;
    for (const TlvNode& t : tlvs) {
        if (t.hasChildren && findTlvDeep(t.children, type, out))
            return true;
    }
    return false;
}

bool Qaiot::parseMessage(const QByteArray& frame, Message* message, QString* errorMessage) const {
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

bool Qaiot::parseTlvs(const QByteArray& data, int start, int end, QList<TlvNode>* out, QString* errorMessage) const {
    if (!out || start < 0 || end > data.size() || start > end) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 范围非法");
        return false;
    }

    int pos = start;
    while (pos < end) {
        TlvNode node;
        node.rawType = static_cast<quint8>(data.at(pos++));
        node.hasChildren = (node.rawType & 0x80u) != 0;
        node.type = node.rawType & 0x7Fu;

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

bool Qaiot::readVarLength(const QByteArray& data, int* pos, int end, int* length, QString* errorMessage) const {
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

QByteArray Qaiot::buildMessage(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs) const {
    QByteArray frame;
    frame.append(static_cast<char>(serviceId));
    frame.append(static_cast<char>(commandId));
    frame.append(buildTlvs(tlvs));
    return frame;
}

QByteArray Qaiot::buildTlvs(const QList<TlvNode>& tlvs) const {
    QByteArray out;
    for (const TlvNode& tlv : tlvs) {
        QByteArray payload = tlv.hasChildren ? buildTlvs(tlv.children) : tlv.value;
        quint8 rawType = tlv.type & 0x7Fu;
        if (tlv.hasChildren && !payload.isEmpty())
            rawType |= 0x80u;
        out.append(static_cast<char>(rawType));
        out.append(encodeVarLength(payload.size()));
        out.append(payload);
    }
    return out;
}

QByteArray Qaiot::encodeVarLength(int length) const {
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

bool Qaiot::tlvFromVariant(const QVariant& value, TlvNode* out, QString* errorMessage) const {
    if (!out)
        return false;
    const QVariantMap map = value.toMap();
    if (map.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 需要 QVariantMap");
        return false;
    }

    quint8 type = 0;
    if (!toByteValue(map.value(QStringLiteral("type")), &type) || type > 0x7F) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV type 非法");
        return false;
    }
    out->type = type;
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
    return valueFromVariant(map.value(QStringLiteral("value")), &out->value, errorMessage);
}

bool Qaiot::valueFromVariant(const QVariant& value, QByteArray* out, QString* errorMessage) const {
    if (!out)
        return false;
    if (!value.isValid() || value.isNull()) {
        out->clear();
        return true;
    }
    if (value.type() == QVariant::ByteArray) {
        *out = value.toByteArray();
        return true;
    }
    if (value.type() == QVariant::String) {
        const QString s = value.toString().trimmed();
        if (s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) || s.contains(QLatin1Char(' '))) {
            QByteArray hex = QByteArray::fromHex(s.toLatin1());
            *out = hex;
            return true;
        }
        *out = s.toUtf8();
            return true;
        }
    if (value.canConvert<int>()) {
        bool ok = false;
        const int n = value.toInt(&ok);
        if (!ok || n < 0 || n > 0xFF) {
            if (errorMessage)
                *errorMessage = QStringLiteral("value 整数需在 0~255");
            return false;
        }
        *out = QByteArray(1, static_cast<char>(n));
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("不支持的 value 类型");
    return false;
}

QString Qaiot::describeMessage(const Message& message) const {
    QStringList lines;
    lines << QStringLiteral("svc=0x%1 cid=0x%2(%3)")
                 .arg(message.serviceId, 2, 16, QChar('0'))
                 .arg(message.commandId, 2, 16, QChar('0'))
                 .arg(fctCidName(message.commandId));
    for (const TlvNode& tlv : message.tlvs)
        lines << describeTlv(tlv, 0, message.commandId);
    return lines.join(QStringLiteral(" | "));
}

QString Qaiot::describeTlv(const TlvNode& tlv, int depth, quint8 commandId) const {
    const QString pad = QString(depth * 2, QLatin1Char(' '));
    if (tlv.hasChildren) {
        QStringList parts;
        parts << QStringLiteral("%1TL(type=0x%2){").arg(pad).arg(tlv.type, 2, 16, QChar('0'));
        for (const TlvNode& c : tlv.children)
            parts << describeTlv(c, depth + 1, commandId);
        parts << QStringLiteral("%1}").arg(pad);
        return parts.join(QLatin1Char(' '));
    }
    // 仅 CID=0x03/0x04 的 Type=0x02 为 device_data_timestap
    quint32 ts = 0;
    if ((commandId == AiotLink::kFctCidGetDeviceData || commandId == AiotLink::kFctCidSetDeviceData)
        && tlv.type == AiotLink::kFctDeviceDataTimestamp && parseUtcTimestampBe4(tlv.value, &ts)) {
        return QStringLiteral("%1TLV(type=0x%2,len=%3,val=%4 时间戳=%5)")
            .arg(pad)
            .arg(tlv.type, 2, 16, QChar('0'))
            .arg(tlv.value.size())
            .arg(hexText(tlv.value), formatDeviceDataTimestamp(ts));
    }
    return QStringLiteral("%1TLV(type=0x%2,len=%3,val=%4)")
        .arg(pad)
        .arg(tlv.type, 2, 16, QChar('0'))
        .arg(tlv.value.size())
        .arg(hexText(tlv.value));
}
