#include "qaiot.h"

#include "aiot_link_defs.h"

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
        // 与 TX 同层：打 PHY 解出后的完整链路帧（含 5A/Length/Control/CRC）
        qDebug().noquote() << "[QAIOT] RX:" << hexText(inner);
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
    // 完整 TLV 摘要只打调试日志，避免刷 UI
    qDebug().noquote() << QStringLiteral("QAIOT RX %1").arg(describeMessage(message));

    TlvNode err;
    if (findTlvDeep(message.tlvs, AiotLink::kTlvErrorCode, &err) && err.value.size() >= 4) {
        const quint32 code = (static_cast<quint8>(err.value.at(0)) << 24) |
                             (static_cast<quint8>(err.value.at(1)) << 16) |
                             (static_cast<quint8>(err.value.at(2)) << 8) |
                             static_cast<quint8>(err.value.at(3));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 错误码 %1 action=%2").arg(code).arg(pendingAction_));
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
        // GET：complete=0x04；SET 应答可能回 0x01
        const quint8 completeType = (message.commandId == AiotLink::kFctCidGetFactoryStatus)
                                        ? AiotLink::kFctGetTlvFactoryComplete
                                        : AiotLink::kFctSetTlvFactoryComplete;
        if (findTlvDeep(message.tlvs, completeType, &n) && !n.value.isEmpty()) {
            const bool done = static_cast<quint8>(n.value.at(0)) == 0x01;
            emitReport(QStringLiteral("ProtocolFactoryDoneData"),
                       QVariant::fromValue(ProtocolFactoryDoneData{done}));
        }

        // GET 顺带回填名称/固件/硬件/蓝牙 MAC
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

            QString macText;
            if (findTlv(message.tlvs, AiotLink::kFctGetTlvMac, &n) && n.value.size() >= 6) {
                // 主窗口 BaseInfo 从 ble_mac 取蓝牙 MAC（按字节逆序显示）
                info.ble_mac.size = 6;
                for (int i = 0; i < 6; ++i)
                    info.ble_mac.bytes[i] = static_cast<uint8_t>(n.value.at(i));
                // 与 qroot/qfctp 一致：线序首字节对应显示末字节
                QStringList parts;
                parts.reserve(6);
                for (int i = 5; i >= 0; --i) {
                    parts.append(QString::number(static_cast<quint8>(n.value.at(i)), 16)
                                     .rightJustified(2, QLatin1Char('0'))
                                     .toUpper());
                }
                macText = parts.join(QLatin1Char(':'));
            }

            if (!info.product_name.isEmpty() || !info.soft_version.isEmpty() || !info.hw_version.isEmpty() ||
                !info.res_version.isEmpty() || info.ble_mac.size > 0) {
                emitReport(QStringLiteral("ProtocolBaseInfoData"), QVariant::fromValue(info));
                emitReport(QStringLiteral("ProtocolPbDate"),
                           QStringLiteral("QAIOT 产测状态 name=%1 soft=%2 hw=%3 res=%4 mac=%5")
                               .arg(info.product_name, info.soft_version, info.hw_version, info.res_version,
                                    macText));
            }
            if (!macText.isEmpty())
                emitReport(QStringLiteral("ProtocolMacData"), QVariant::fromValue(ProtocolMacData{macText}));
        }

        const quint8 modeTypeTlv = (message.commandId == AiotLink::kFctCidGetFactoryStatus)
                                       ? AiotLink::kFctGetTlvModeType
                                       : AiotLink::kFctSetTlvModeType;
        const quint8 modeStatusTlv = (message.commandId == AiotLink::kFctCidGetFactoryStatus)
                                         ? AiotLink::kFctGetTlvModeStatus
                                         : AiotLink::kFctSetTlvModeEnable;
        quint8 modeType = 0xFF;
        quint8 modeStatus = 0xFF;
        if (findTlvDeep(message.tlvs, modeTypeTlv, &n) && !n.value.isEmpty())
            modeType = static_cast<quint8>(n.value.at(0));
        if (findTlvDeep(message.tlvs, modeStatusTlv, &n) && !n.value.isEmpty())
            modeStatus = static_cast<quint8>(n.value.at(0));
        if (modeType != 0xFF || modeStatus != 0xFF) {
            emitReport(QStringLiteral("ProtocolPbDate"),
                       QStringLiteral("QAIOT 工厂模式 type=0x%1 status=0x%2")
                           .arg(modeType, 2, 16, QChar('0'))
                           .arg(modeStatus, 2, 16, QChar('0')));
        }
    } else if (message.commandId == AiotLink::kFctCidGetDeviceData) {
        // 嵌套 device_data_type / device_data → 三元组
        QString prod, dev, key, sn;
        for (const TlvNode& item : message.tlvs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(item.children, 0x05, &typeNode) && item.type != 0x04)
                continue;
            if (!findTlv(item.hasChildren ? item.children : message.tlvs, 0x05, &typeNode))
                findTlvDeep(message.tlvs, 0x05, &typeNode);
            if (!findTlvDeep(message.tlvs, 0x06, &dataNode))
                continue;
            if (typeNode.value.isEmpty())
                continue;
            const quint8 dt = static_cast<quint8>(typeNode.value.at(0));
            const QString text = QString::fromUtf8(dataNode.value);
            if (dt == 0x01)
                sn = text;
            else if (dt == 0x02)
                prod = text;
            else if (dt == 0x03)
                dev = text;
            else if (dt == 0x04)
                key = text;
        }
        // 简化：扫全部 leaf
        QList<TlvNode> structs;
        for (const TlvNode& t : message.tlvs) {
            if (t.type == 0x04 || t.hasChildren)
                structs.append(t);
            for (const TlvNode& c : t.children) {
                if (c.type == 0x04 || c.hasChildren)
                    structs.append(c);
            }
        }
        for (const TlvNode& st : structs) {
            TlvNode typeNode, dataNode;
            if (!findTlv(st.children, 0x05, &typeNode) || !findTlv(st.children, 0x06, &dataNode))
                continue;
            if (typeNode.value.isEmpty())
                continue;
            const quint8 dt = static_cast<quint8>(typeNode.value.at(0));
            const QString text = QString::fromUtf8(dataNode.value);
            if (dt == 0x01)
                sn = text;
            else if (dt == 0x02)
                prod = text;
            else if (dt == 0x03)
                dev = text;
            else if (dt == 0x04)
                key = text;
        }
        if (!sn.isEmpty())
            emitReport(QStringLiteral("ProtocolSnData"),
                       QVariant::fromValue(ProtocolSnData{ProtocolSnType::TailSn, sn}));
        if (!prod.isEmpty() || !dev.isEmpty() || !key.isEmpty())
            emitReport(QStringLiteral("ProtocolTupleData"),
                       QVariant::fromValue(ProtocolTupleData{prod, dev, key}));
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
        emitReport(QStringLiteral("ProtocolBatteryData"), QVariant::fromValue(batteryData));
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT 电池 percent=%1 voltage=%2mV")
                       .arg(batteryData.percent)
                       .arg(batteryData.voltageMv));
    } else if (message.commandId == AiotLink::kFctCidGetSensor) {
        emitReport(QStringLiteral("ProtocolPbDate"),
                   QStringLiteral("QAIOT FCT CID=0x%1 已收应答").arg(message.commandId, 2, 16, QChar('0')));
    }
}

void Qaiot::set(DeviceCmd cmd, const QVariant& data) {
    const QVariantMap map = data.toMap();
    switch (cmd) {
    case DeviceCmd::FacMode: {
        // SET CID=0x02：list(0x02)/struct(0x03)/type(0x04)/enable(0x05)
        const int enable = data.canConvert<int>() ? data.toInt() : map.value(QStringLiteral("on"), 1).toInt();
        const quint8 modeType = static_cast<quint8>(
            map.value(QStringLiteral("mode"), AiotLink::kFctModeFactoryTest).toInt());
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctSetTlvModeType, u8(modeType)));
        modeChildren.append(makeLeaf(AiotLink::kFctSetTlvModeEnable, u8(enable ? 0x01 : 0x00)));
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctSetTlvModeList,
                       {makeParent(AiotLink::kFctSetTlvModeStruct, modeChildren)})};
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus, tlvs,
                           enable ? QStringLiteral("进入产测模式") : QStringLiteral("退出产测模式"));
        break;
    }
    case DeviceCmd::BurningMode: {
        const int enable = map.value(QStringLiteral("switch"), map.value(QStringLiteral("enter"), 1)).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctSetTlvModeType, u8(AiotLink::kFctModeAging)));
        modeChildren.append(makeLeaf(AiotLink::kFctSetTlvModeEnable, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctSetTlvModeList,
                        {makeParent(AiotLink::kFctSetTlvModeStruct, modeChildren)})},
            QStringLiteral("老化模式"));
        break;
    }
    case DeviceCmd::SuctionMode: {
        const int enable = map.value(QStringLiteral("on"), map.value(QStringLiteral("switch"), 1)).toInt();
        QList<TlvNode> modeChildren;
        modeChildren.append(makeLeaf(AiotLink::kFctSetTlvModeType, u8(AiotLink::kFctModeSuction)));
        modeChildren.append(makeLeaf(AiotLink::kFctSetTlvModeEnable, u8(enable ? 0x01 : 0x00)));
        sendServiceCommand(
            AiotLink::kSvcFctAte, AiotLink::kFctCidSetFactoryStatus,
            {makeParent(AiotLink::kFctSetTlvModeList,
                        {makeParent(AiotLink::kFctSetTlvModeStruct, modeChildren)})},
            QStringLiteral("吸力模式"));
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
    case DeviceCmd::WriteKey: {
        // 写入通用设备数据：type 01 SN / 02~04 三元组
        QList<TlvNode> items;
        auto appendItem = [&](quint8 dataType, const QByteArray& bytes) {
            if (bytes.isEmpty())
                return;
            QList<TlvNode> ch;
            ch.append(makeLeaf(0x05, u8(dataType)));
            ch.append(makeLeaf(0x06, bytes));
            items.append(makeParent(0x04, ch));
        };
        appendItem(0x01, mapToUtf8(map, QStringLiteral("sn")));
        appendItem(0x02, mapToUtf8(map, QStringLiteral("productId")));
        appendItem(0x03, mapToUtf8(map, QStringLiteral("deviceId")));
        appendItem(0x04, mapToUtf8(map, QStringLiteral("deviceSecret")));
        if (items.isEmpty() && data.type() == QVariant::String)
            appendItem(0x01, data.toString().toUtf8());
        if (items.isEmpty()) {
            emitReport(QStringLiteral("ProtocolPbDate"), QStringLiteral("QAIOT 写 SN/三元组缺少字段"));
            return;
        }
        QList<TlvNode> tlvs;
        tlvs.append(makeLeaf(0x01, u8(0x02))); // Independent
        tlvs.append(makeParent(0x03, items));
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSetDeviceData, tlvs, QStringLiteral("写设备数据"));
        break;
    }
    case DeviceCmd::ButtonState: {
        // 模拟按键 CID=0x10（规范 Virtual Key）
        const quint8 key = static_cast<quint8>(map.value(QStringLiteral("key"), data.toInt()).toUInt());
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidSimulateKey, {makeLeaf(0x01, u8(key))},
                           QStringLiteral("模拟按键"));
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
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
    case DeviceCmd::DeviceInfo: {
        // CID=0x01：查 name/fw/mac/hw/res（各 Type Length=0）
        const QList<TlvNode> tlvs = {
            queryLeaf(AiotLink::kFctGetTlvDeviceName),
            queryLeaf(AiotLink::kFctGetTlvFwVersion),
            queryLeaf(AiotLink::kFctGetTlvMac),
            queryLeaf(AiotLink::kFctGetTlvHwVersion),
            queryLeaf(AiotLink::kFctGetTlvResVersion),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus, tlvs,
                           QStringLiteral("读产测状态/版本"));
        break;
    }
    case DeviceCmd::GetBattery: {
        // CID=0x0E：查询 battery_data_struct（含 percent/voltage/current/temp）
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetBatteryInfo, {queryLeaf(0x01)},
                           QStringLiteral("读电量"));
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
        // CID=0x03：side + data_type（SN=0x01 / 三元组=0x02~0x04）
        QList<TlvNode> items;
        auto appendDataType = [&](quint8 dataType) {
            items.append(makeParent(0x04, {makeLeaf(0x05, u8(dataType))}));
        };
        if (cmd == DeviceCmd::Sn) {
            appendDataType(0x01);
        } else {
            appendDataType(0x02);
            appendDataType(0x03);
            appendDataType(0x04);
        }
        const QList<TlvNode> tlvs = {
            makeLeaf(0x01, u8(0x02)), // Independent
            makeParent(0x03, items),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetDeviceData, tlvs,
                           QStringLiteral("读设备数据/三元组"));
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
        // CID=0x08：list/struct/sensor_type；可经 param.type 指定，默认红外
        quint8 sensorType = 0x05; // Infrared
        if (map.contains(QStringLiteral("type")))
            sensorType = static_cast<quint8>(map.value(QStringLiteral("type")).toUInt());
        else if (map.contains(QStringLiteral("sensorType")))
            sensorType = static_cast<quint8>(map.value(QStringLiteral("sensorType")).toUInt());
        else if (param.type() != QVariant::Map && param.canConvert<int>())
            sensorType = static_cast<quint8>(param.toInt());
        const QList<TlvNode> tlvs = {
            makeParent(0x01, {makeParent(0x02, {makeLeaf(0x03, u8(sensorType))})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetSensor, tlvs, QStringLiteral("读传感器"));
        break;
    }
    case DeviceCmd::AgingStatusRead: {
        // 查询老化模式使能状态
        const QList<TlvNode> tlvs = {
            makeParent(AiotLink::kFctGetTlvModeList,
                       {makeParent(AiotLink::kFctGetTlvModeStruct,
                                   {makeLeaf(AiotLink::kFctGetTlvModeType, u8(AiotLink::kFctModeAging)),
                                    queryLeaf(AiotLink::kFctGetTlvModeStatus)})}),
        };
        sendServiceCommand(AiotLink::kSvcFctAte, AiotLink::kFctCidGetFactoryStatus, tlvs,
                           QStringLiteral("读老化/产测状态"));
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
    pendingAction_ = actionName;
    const QByteArray app = buildMessage(svc, commandId, tlvs);
    emitReport(QStringLiteral("ProtocolPbDate"),
               QStringLiteral("QAIOT TX %1 svc=0x%2 cid=0x%3")
                   .arg(actionName)
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
        // 与 qroot 一致：完整 PHY（8×CC+Len+Ch+链路帧）不刷屏，只打内层链路帧
        // qDebug().noquote() << "[QAIOT] TX:" << hexText(phy)
        //                    << "inner:" << hexText(link);
        qDebug().noquote() << "[QAIOT] TX:" << hexText(link);
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
    lines << QStringLiteral("svc=0x%1 cid=0x%2")
                 .arg(message.serviceId, 2, 16, QChar('0'))
                 .arg(message.commandId, 2, 16, QChar('0'));
    for (const TlvNode& tlv : message.tlvs)
        lines << describeTlv(tlv);
    return lines.join(QStringLiteral(" | "));
}

QString Qaiot::describeTlv(const TlvNode& tlv, int depth) const {
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
