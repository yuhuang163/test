#include "qfctp.h"
#include <QDebug>
#include <QVariantMap>
#include <QString>

#include "comm_protocol_builder.h"
#include "comm_protocol_parser.h"

#include <cstring>
#include <vector>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

static uint16_t qfctpReadLe16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

static constexpr uint8_t kPhyTxHeaderByte = 0xCC;
static constexpr uint8_t kPhyRxHeaderByte = 0xAA;
static constexpr int     kPhyHeaderSize = 8;
// 与 qpb 中通道定义保持一致：INVALID=0, FAC=1, APP=2, MAIN=3
static constexpr uint8_t kPhyChannelFac = 1;
static constexpr uint8_t kPhyChannelApp = 2;
static constexpr uint8_t kPhyChannelMain = 3;
static constexpr uint16_t kTestsService = COMM_PROTOCOL_TESTS_SERVICE;
static constexpr uint16_t kSystemConfigService = COMM_PROTOCOL_SYSTEM_CONFIG_SERVICE;
static constexpr uint16_t kAlgoService = COMM_PROTOCOL_ALGO_SERVICE;

// TESTS SERVICE TLV
static constexpr uint16_t kTlvFactoryMode = 0x0001;
static constexpr uint16_t kTlvAgingModeSet = 0x0002;
static constexpr uint16_t kTlvSuctionMode = 0x0003;
static constexpr uint16_t kTlvAgingStatus = 0x0004;
static constexpr uint16_t kTlvSnWrite = 0x0007;
static constexpr uint16_t kTlvProductIdWrite = 0x0008;
static constexpr uint16_t kTlvDeviceIdWrite = 0x0009;
static constexpr uint16_t kTlvKeyWrite = 0x000A;
static constexpr uint16_t kTlvTupleRead = 0x000B;
static constexpr uint16_t kTlvDeviceException = 0x000C;
static constexpr uint16_t kTlvCompensationSet = 0x000D;
static constexpr uint16_t kTlvSnRead = 0x000E;
static constexpr uint16_t kTlvFactoryDoneWrite = 0x000F;
static constexpr uint16_t kTlvAgingModeExit = 0x0010;
static constexpr uint16_t kTlvBtSignalMode = 0x0011;
static constexpr uint16_t kTlvBtNoSignalMode = 0x0012;
static constexpr uint16_t kTlvBtFreqMode = 0x0013;
static constexpr uint16_t kTlvTrimSet = 0x0014;
static constexpr uint16_t kTlvTrimGet = 0x0015;
static constexpr uint16_t kTlvStandbyMode = 0x0016;
static constexpr uint16_t kTlvSensorState = 0x0017;
static constexpr uint16_t kTlvFactoryDoneRead = 0x0018;
static constexpr uint16_t kTlvRssiRead = 0x0019;
static constexpr uint16_t kTlvBatteryRead = 0x001A;
static constexpr uint16_t kTlvKeySignalRead = 0x001B;
static constexpr uint16_t kTlvLcdBacklight = 0x001C;
static constexpr uint16_t kTlvLightReportCtrl = 0x001D;
static constexpr uint16_t kTlvLightCalibWrite = 0x001E;
static constexpr uint16_t kTlvLightCalibRead = 0x001F;
static constexpr uint16_t kTlvChargeCurrentRead = 0x0020;

// SYSTEM CONFIG SERVICE TLV
static constexpr uint16_t kTlvFwVersionRead = 0x0001;
static constexpr uint16_t kTlvMacRead = 0x0002;
static constexpr uint16_t kTlvMacWrite = 0x0003;
static constexpr uint16_t kTlvPowerOff = 0x0004;
static constexpr uint16_t kTlvLedTest = 0x0005;
static constexpr uint16_t kTlvNightLightSet = 0x0006;
static constexpr uint16_t kTlvFactoryReset = 0x0007;

void qfctp_on_full_frame(const uint8_t *frame_data, uint16_t frame_len, void *user_data)
{
    auto *self = static_cast<Qfctp *>(user_data);
    self->handleFullFrame(frame_data, frame_len);
}

Qfctp::Qfctp(QSerialPort* parent) : QSerialPort(parent)
{
    serialPort = parent;
}

void Qfctp::handleFullFrame(const uint8_t *frameData, uint16_t frameLen)
{
    comm_protocol_frame_node_t frame{};
    std::memset(&frame, 0, sizeof(frame));
    const int rc = comm_protocol_frame_deserialize(frameData, frameLen, &frame);
    if (rc != COMM_PROTOCOL_ERROR_NONE) {
        qWarning() << "FCTP 帧反序列化失败:" << rc;
        return;
    }
    qDebug() << "FCTP 帧 seq" << frame.seq << "type" << frame.frame_type
             << "payload" << frame.payload_length << "services"
             << frame.payload.service_count;

    for (uint16_t i = 0; i < frame.payload.service_count && i < COMM_PROTOCOL_MAX_SERVICE_NUM; ++i) {
        const comm_protocol_service_node_t &svc = frame.payload.services[i];
        if (frame.frame_type == COMM_PROTOCOL_FRAME_TYPE_RESPONSE) {
            handleResponseService(frame.seq, svc.svr_id, svc.tlvs, svc.srv_length);
        } else if (frame.frame_type == COMM_PROTOCOL_FRAME_TYPE_NOTIFY) {
            handleNotifyService(svc.svr_id, svc.tlvs, svc.srv_length);
        }
    }
}

void Qfctp::handleResponseService(uint8_t seq, uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen)
{
    uint16_t off = 0;
    uint16_t errCode = 0xFFFF;
    bool hasErrCode = false;
    uint16_t mainType = 0;
    const uint8_t *mainValue = nullptr;
    uint16_t mainLen = 0;

    while (tlvs != nullptr && off + COMM_PROTOCOL_TLV_HEADER_SIZE <= serviceLen) {
        const uint8_t *p = tlvs + off;
        const uint16_t type = qfctpReadLe16(p);
        const uint16_t len = qfctpReadLe16(p + 2);
        if (off + COMM_PROTOCOL_TLV_HEADER_SIZE + len > serviceLen) {
            qWarning() << "FCTP 响应TLV长度非法, svc=" << serviceId << "type=" << type;
            break;
        }
        const uint8_t *value = p + COMM_PROTOCOL_TLV_HEADER_SIZE;
        if (type == PROTOCOL_SERVICE_ERROR_CODE_TLV_ID && len >= 2) {
            errCode = qfctpReadLe16(value);
            hasErrCode = true;
        } else {
            mainType = type;
            mainValue = value;
            mainLen = len;
        }
        off = static_cast<uint16_t>(off + COMM_PROTOCOL_TLV_HEADER_SIZE + len);
    }

    PendingRequest req = m_pendingRequests.take(seq);
    if (req.kind == RequestKind::Unknown) {
        req.serviceId = serviceId;
        req.tlvType = mainType;
        req.actionName = "未知请求";
    }

    int mainValueAsInt = 0;
    bool hasMainValueAsInt = false;
    if (mainValue != nullptr && mainLen >= 1) {
        if (mainLen == 1) {
            mainValueAsInt = mainValue[0];
            hasMainValueAsInt = true;
        } else if (mainLen >= 2) {
            mainValueAsInt = qfctpReadLe16(mainValue);
            hasMainValueAsInt = true;
        }
    }

    qDebug() << "FCTP 响应"
             << "seq=" << static_cast<int>(seq)
             << "service=" << serviceId
             << "tlv=" << mainType
             << "len=" << mainLen
             << "err=" << (hasErrCode ? QString::number(errCode) : "-");
    handleRequestResult(seq, req, mainValueAsInt, hasMainValueAsInt, errCode, hasErrCode);

    if (req.kind == RequestKind::SnRead && mainValue != nullptr && mainLen > 0) {
        qInfo() << "FCTP SN读取:" << QByteArray(reinterpret_cast<const char *>(mainValue), mainLen);
    } else if (req.kind == RequestKind::TupleRead && mainValue != nullptr && mainLen >= 38) {
        const QByteArray raw(reinterpret_cast<const char *>(mainValue), mainLen);
        qInfo() << "FCTP 三元组读取 prod=" << raw.left(6).toHex(' ')
                << "dev=" << raw.mid(6, 16).toHex(' ')
                << "key=" << raw.mid(22, 16).toHex(' ');
    } else if ((req.kind == RequestKind::AgingStatusGet) && mainValue != nullptr && mainLen >= 7) {
        const uint8_t status = mainValue[0];
        const uint16_t loops = qfctpReadLe16(mainValue + 1);
        const uint32_t seconds = static_cast<uint32_t>(mainValue[3])
                               | (static_cast<uint32_t>(mainValue[4]) << 8)
                               | (static_cast<uint32_t>(mainValue[5]) << 16)
                               | (static_cast<uint32_t>(mainValue[6]) << 24);
        qInfo() << "FCTP 老化状态 status=" << status << "loops=" << loops << "seconds=" << seconds;
    }
}

void Qfctp::handleNotifyService(uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen)
{
    uint16_t off = 0;
    while (tlvs != nullptr && off + COMM_PROTOCOL_TLV_HEADER_SIZE <= serviceLen) {
        const uint8_t *p = tlvs + off;
        const uint16_t type = qfctpReadLe16(p);
        const uint16_t len = qfctpReadLe16(p + 2);
        if (off + COMM_PROTOCOL_TLV_HEADER_SIZE + len > serviceLen) {
            break;
        }
        const uint8_t *value = p + COMM_PROTOCOL_TLV_HEADER_SIZE;
        if (serviceId == kAlgoService && type == 0x0006 && len > 0) {
            qInfo() << "FCTP 光感上报数据:" << QByteArray(reinterpret_cast<const char *>(value), len).toHex(' ');
        } else {
            qDebug() << "FCTP NOTIFY" << "service=" << serviceId << "type=" << type << "len=" << len;
        }
        off = static_cast<uint16_t>(off + COMM_PROTOCOL_TLV_HEADER_SIZE + len);
    }
}

void Qfctp::handleRequestResult(uint8_t seq, const PendingRequest &req, int mainValue, bool hasMainValue, uint16_t errCode, bool hasErrCode)
{
    const bool errOk = hasErrCode && (errCode == 0u);
    const bool bizOk = !hasMainValue || (mainValue == 1);
    const bool ok = errOk && bizOk;
    qInfo() << "FCTP 应答判定"
            << req.actionName
            << "seq=" << static_cast<int>(seq)
            << "biz=" << (hasMainValue ? QString::number(mainValue) : "-")
            << "err=" << (hasErrCode ? QString::number(errCode) : "-")
            << (ok ? "成功" : "失败/不完整");
}

QByteArray Qfctp::wrapPhyPacket(const QByteArray &innerPacket) const
{
    // 外层协议: [8*0xCC][len:1][channel:1][payload:len]
    if (innerPacket.isEmpty()) {
        return {};
    }
    if (innerPacket.size() > 0xFF) {
        qWarning() << "FCTP 外层封装失败，内层长度超过 255:" << innerPacket.size();
        return {};
    }

    std::vector<uint8_t> newBuffer;
    newBuffer.reserve(static_cast<size_t>(kPhyHeaderSize + 1 + 1 + innerPacket.size()));
    newBuffer.insert(newBuffer.end(), kPhyHeaderSize, kPhyTxHeaderByte);
    newBuffer.push_back(static_cast<uint8_t>(innerPacket.size()));
    // 默认发送 FAC 通道
    newBuffer.push_back(kPhyChannelFac);
    newBuffer.insert(newBuffer.end(),
                     reinterpret_cast<const uint8_t *>(innerPacket.constData()),
                     reinterpret_cast<const uint8_t *>(innerPacket.constData()) + innerPacket.size());

    return QByteArray(reinterpret_cast<const char *>(newBuffer.data()), static_cast<int>(newBuffer.size()));
}

bool Qfctp::tryUnwrapPhyPacket(const QByteArray &packet, QList<QByteArray> &outPackets)
{
    const auto resetPhyState = [this]() {
        m_phyState = PHY_STATE_IDLE;
        m_phyHeaderHits = 0;
        m_phyExpectedLen = 0;
        m_phyChannel = 0;
        m_phyPayload.clear();
    };

    std::vector<uint8_t> data(packet.begin(), packet.end());
    for (auto x : data) {
        switch (m_phyState) {
        case PHY_STATE_IDLE:
            if (x == kPhyRxHeaderByte) {
                m_phyHeaderHits = 1;
                m_phyState = PHY_STATE_HEADER;
            }
            break;
        case PHY_STATE_HEADER:
            if (x == kPhyRxHeaderByte) {
                ++m_phyHeaderHits;
                if (m_phyHeaderHits == kPhyHeaderSize) {
                    m_phyState = PHY_STATE_CHANNEL;
                }
            } else {
                resetPhyState();
            }
            break;
        case PHY_STATE_CHANNEL:
            m_phyChannel = x;
            m_phyState = PHY_STATE_LEN;
            break;

        case PHY_STATE_LEN:
            m_phyExpectedLen = static_cast<int>(x);
            if (m_phyExpectedLen <= 0) {
                qWarning() << "FCTP 外层包长度非法:" << m_phyExpectedLen;
                resetPhyState();
                break;
            }
            m_phyPayload.clear();
            m_phyPayload.reserve(m_phyExpectedLen);
            m_phyState = PHY_STATE_PAYLOAD;
            break;

        case PHY_STATE_PAYLOAD:
            m_phyPayload.push_back(static_cast<char>(x));
            if (m_phyPayload.size() >= m_phyExpectedLen) {
                if (m_phyChannel == kPhyChannelFac || m_phyChannel == kPhyChannelApp || m_phyChannel == kPhyChannelMain) {
                    outPackets.push_back(m_phyPayload);
                } else {
                    qWarning() << "FCTP 外层包通道异常，channel =" << static_cast<int>(m_phyChannel);
                }
                resetPhyState();
            }
            break;
        default:
            resetPhyState();
            break;
        }
    }
    return !outPackets.isEmpty();
}

bool Qfctp::sendPacket(const QByteArray &innerPacket, QByteArray *outPhyPacket) const
{
    if (serialPort == nullptr) {
        qWarning() << "FCTP 串口对象为空，未发送产测模式帧";
        return false;
    }
    if (!serialPort->isOpen()) {
        qWarning() << "FCTP 串口未打开，未发送产测模式帧";
        return false;
    }
    const QByteArray phyPacket = wrapPhyPacket(innerPacket);
    if (phyPacket.isEmpty()) {
        return false;
    }
    if (outPhyPacket != nullptr) {
        *outPhyPacket = phyPacket;
    }
    const qint64 n = serialPort->write(phyPacket);
    if (n != phyPacket.size()) {
        qWarning() << "FCTP 产测模式发送不完整" << n << "/" << phyPacket.size();
        return false;
    }
    return true;
}

bool Qfctp::sendServiceTlv(uint16_t serviceId, uint16_t tlvType, QByteArray value, const char *actionName, uint8_t *outSeq)
{
    comm_protocol_builder_t b{};
    std::memset(&b, 0, sizeof(b));
    const uint8_t seq = static_cast<uint8_t>(++m_fctpSeq);
    if (comm_protocol_builder_init(&b, 64, seq, COMM_PROTOCOL_FRAME_TYPE_REQUEST) != 0) {
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    comm_protocol_service_t *svc = comm_protocol_builder_init_service(&b, serviceId);
    if (svc == nullptr) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    comm_protocol_tlv_node_t node{};
    node.type   = tlvType;
    node.length = static_cast<uint16_t>(value.size());
    node.value  = value.isEmpty() ? nullptr : reinterpret_cast<uint8_t *>(value.data());
    if (comm_protocol_builder_add_tlv(&b, svc, &node) != 0) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    uint16_t outSize = 0;
    uint8_t *raw     = comm_protocol_builder_finalize(&b, &outSize);
    if (raw == nullptr) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    const QByteArray pkt(reinterpret_cast<const char *>(raw), static_cast<int>(outSize));
    comm_protocol_builder_free(&b);
    if (pkt.isEmpty()) {
        qWarning() << "FCTP" << actionName << "组包失败";
        return false;
    }

    QByteArray phyPacket;
    if (!sendPacket(pkt, &phyPacket)) {
        return false;
    }

    qDebug() << "FCTP 已发送" << actionName
             << "seq=" << static_cast<int>(m_fctpSeq)
             << "service=" << serviceId
             << "tlv=" << tlvType
             << "inner=" << pkt.toHex(' ')
             << "outer=" << phyPacket.toHex(' ');
    if (outSeq != nullptr) {
        *outSeq = seq;
    }
    return true;
}

bool Qfctp::sendTestsServiceTlv(uint16_t tlvType, QByteArray value, const char *actionName, RequestKind kind)
{
    return sendRequest(kind, kTestsService, tlvType, value, actionName);
}

bool Qfctp::sendRequest(RequestKind kind, uint16_t serviceId, uint16_t tlvType, const QByteArray &value, const char *actionName)
{
    uint8_t seq = 0;
    if (!sendServiceTlv(serviceId, tlvType, value, actionName, &seq)) {
        return false;
    }
    PendingRequest req{};
    req.kind = kind;
    req.serviceId = serviceId;
    req.tlvType = tlvType;
    req.actionName = QString::fromUtf8(actionName);
    m_pendingRequests.insert(seq, req);
    return true;
}

void Qfctp::sendFactoryTestMode(bool enter)
{
    const uint8_t v = enter ? 1u : 0u;
    const QByteArray value(reinterpret_cast<const char *>(&v), 1);
    sendTestsServiceTlv(kTlvFactoryMode, value, enter ? "产测模式进入" : "产测模式退出", RequestKind::FactoryMode);
}

bool Qfctp::handleSetSn(const QVariant &data)
{
    if (data.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = data.value<DeviceSnPayload>();
        switch (payload.which_sn) {
        case FacDevInfoType_SUB_PID:
            return sendTestsServiceTlv(kTlvProductIdWrite, payload.sn, "写SUB_PID", RequestKind::TupleWriteProductId);
        case FacDevInfoType_SKUID:
            return sendTestsServiceTlv(kTlvDeviceIdWrite, payload.sn, "写SKUID", RequestKind::TupleWriteDeviceId);
        case FacDevInfoType_BOARD_SN:
            qWarning() << "Qfctp BOARD_SN 使用SN通道兼容写入";
            return sendTestsServiceTlv(kTlvSnWrite, payload.sn, "写BOARD_SN", RequestKind::SnWrite);
        case FacDevInfoType_TAIL_SN:
        default:
            return sendTestsServiceTlv(kTlvSnWrite, payload.sn, "写SN", RequestKind::SnWrite);
        }
    }

    const QVariantList list = data.toList();
    if (list.size() >= 2) {
        const auto which = static_cast<FacDevInfoType>(list.at(0).toInt());
        const QByteArray sn = list.at(1).toByteArray();
        if (which == FacDevInfoType_SUB_PID) {
            return sendTestsServiceTlv(kTlvProductIdWrite, sn, "写SUB_PID", RequestKind::TupleWriteProductId);
        }
        if (which == FacDevInfoType_SKUID) {
            return sendTestsServiceTlv(kTlvDeviceIdWrite, sn, "写SKUID", RequestKind::TupleWriteDeviceId);
        }
        if (which == FacDevInfoType_BOARD_SN) {
            qWarning() << "Qfctp BOARD_SN 使用SN通道兼容写入";
            return sendTestsServiceTlv(kTlvSnWrite, sn, "写BOARD_SN", RequestKind::SnWrite);
        }
        return sendTestsServiceTlv(kTlvSnWrite, sn, "写SN", RequestKind::SnWrite);
    }

    QByteArray value = data.toByteArray();
    if (value.isEmpty()) {
        const QVariantMap map = data.toMap();
        value = map.value("sn").toByteArray();
    }
    if (value.isEmpty()) {
        qWarning() << "Qfctp SN写入参数为空";
        return false;
    }
    return sendTestsServiceTlv(kTlvSnWrite, value, "写SN", RequestKind::SnWrite);
}

bool Qfctp::setCaseAgingMode(const QVariantMap &map)
{
    QByteArray value;
    value.append(static_cast<char>(map.value("mode").toInt() & 0xFF));
    const uint32_t sec = map.value("seconds").toUInt();
    value.append(static_cast<char>(sec & 0xFF));
    value.append(static_cast<char>((sec >> 8) & 0xFF));
    value.append(static_cast<char>((sec >> 16) & 0xFF));
    value.append(static_cast<char>((sec >> 24) & 0xFF));
    return sendTestsServiceTlv(kTlvAgingModeSet, value, "老化模式设置", RequestKind::AgingModeSet);
}

bool Qfctp::setCaseAgingExit()
{
    return sendTestsServiceTlv(kTlvAgingModeExit, {}, "老化模式退出", RequestKind::AgingModeExit);
}

bool Qfctp::setCaseSuctionMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvSuctionMode, QByteArray(1, static_cast<char>(v)), "吸力测试模式", RequestKind::SuctionMode);
}

bool Qfctp::setCaseBtSignalMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvBtSignalMode, QByteArray(1, static_cast<char>(v)), "蓝牙信令测试模式", RequestKind::BtSignalMode);
}

bool Qfctp::setCaseBtNoSignalMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvBtNoSignalMode, QByteArray(1, static_cast<char>(v)), "蓝牙非信令测试模式", RequestKind::BtNoSignalMode);
}

bool Qfctp::setCaseBtFreqMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvBtFreqMode, QByteArray(1, static_cast<char>(v)), "蓝牙校频测试模式", RequestKind::BtFreqMode);
}

bool Qfctp::setCaseStandbyMode(const QVariantMap &map)
{
    const uint8_t v = map.value("enter").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvStandbyMode, QByteArray(1, static_cast<char>(v)), "待机模式", RequestKind::StandbyMode);
}

bool Qfctp::setCaseWriteProductId(const QVariantMap &map)
{
    return sendTestsServiceTlv(kTlvProductIdWrite, map.value("value").toByteArray(), "写产品ID", RequestKind::TupleWriteProductId);
}

bool Qfctp::setCaseWriteDeviceId(const QVariantMap &map)
{
    return sendTestsServiceTlv(kTlvDeviceIdWrite, map.value("value").toByteArray(), "写设备ID", RequestKind::TupleWriteDeviceId);
}

bool Qfctp::setCaseWriteKey(const QVariantMap &map)
{
    return sendTestsServiceTlv(kTlvKeyWrite, map.value("value").toByteArray(), "写密钥", RequestKind::TupleWriteKey);
}

bool Qfctp::setCaseFactoryDoneWrite(const QVariantMap &map)
{
    const uint8_t v = map.value("done").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvFactoryDoneWrite, QByteArray(1, static_cast<char>(v)), "写产测完成标识", RequestKind::FactoryDoneWrite);
}

bool Qfctp::setCaseTrimSet(const QVariantMap &map)
{
    const uint32_t trim = map.value("value").toUInt();
    QByteArray value;
    value.append(static_cast<char>(trim & 0xFF));
    value.append(static_cast<char>((trim >> 8) & 0xFF));
    value.append(static_cast<char>((trim >> 16) & 0xFF));
    value.append(static_cast<char>((trim >> 24) & 0xFF));
    return sendTestsServiceTlv(kTlvTrimSet, value, "写trim", RequestKind::TrimSet);
}

bool Qfctp::setCaseMacWrite(const QVariantMap &map)
{
    return sendRequest(RequestKind::MacWrite, kSystemConfigService, kTlvMacWrite, map.value("value").toByteArray(), "写MAC");
}

bool Qfctp::setCaseNightLightSet(const QVariantMap &map)
{
    const uint8_t v = static_cast<uint8_t>(map.value("value").toUInt() & 0xFF);
    return sendRequest(RequestKind::NightLightSet, kSystemConfigService, kTlvNightLightSet, QByteArray(1, static_cast<char>(v)), "夜灯亮度设置");
}

bool Qfctp::setCaseLedTest(const QVariantMap &map)
{
    const uint8_t v = map.value("on").toInt() != 0 ? 1u : 0u;
    return sendRequest(RequestKind::LedTest, kSystemConfigService, kTlvLedTest, QByteArray(1, static_cast<char>(v)), "显示测试");
}

bool Qfctp::setCaseFactoryReset()
{
    return sendRequest(RequestKind::FactoryReset, kSystemConfigService, kTlvFactoryReset, {}, "恢复出厂");
}

bool Qfctp::setCasePowerOff()
{
    return sendRequest(RequestKind::PowerOff, kSystemConfigService, kTlvPowerOff, {}, "设备关机");
}

bool Qfctp::setCaseLcdBacklight(const QVariantMap &map)
{
    const uint8_t v = map.value("on").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvLcdBacklight, QByteArray(1, static_cast<char>(v)), "LCD背光控制", RequestKind::LcdBacklightSet);
}

bool Qfctp::setCaseLightReportControl(const QVariantMap &map)
{
    const uint8_t v = map.value("start").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvLightReportCtrl, QByteArray(1, static_cast<char>(v)), "光感上报控制", RequestKind::LightReportControl);
}

bool Qfctp::setCaseLightCalibWrite(const QVariantMap &map)
{
    QByteArray value;
    const uint8_t idx = static_cast<uint8_t>(map.value("index").toUInt() & 0xFF);
    const int32_t calib = map.value("value").toInt();
    value.append(static_cast<char>(idx));
    value.append(static_cast<char>(calib & 0xFF));
    value.append(static_cast<char>((calib >> 8) & 0xFF));
    value.append(static_cast<char>((calib >> 16) & 0xFF));
    value.append(static_cast<char>((calib >> 24) & 0xFF));
    return sendTestsServiceTlv(kTlvLightCalibWrite, value, "光感校准写入", RequestKind::LightCalibWrite);
}

bool Qfctp::getCaseTupleRead()
{
    return sendTestsServiceTlv(kTlvTupleRead, {}, "读取三元组", RequestKind::TupleRead);
}

bool Qfctp::getCaseTrimRead()
{
    return sendTestsServiceTlv(kTlvTrimGet, {}, "读取trim", RequestKind::TrimGet);
}

bool Qfctp::getCaseFwVersionRead()
{
    return sendRequest(RequestKind::FwVersionGet, kSystemConfigService, kTlvFwVersionRead, {}, "读取固件版本");
}

bool Qfctp::getCaseMacRead()
{
    return sendRequest(RequestKind::MacRead, kSystemConfigService, kTlvMacRead, {}, "读取MAC");
}

bool Qfctp::setCaseCompensationSet(const QVariantMap &map)
{
    const uint8_t v = map.value("enable").toInt() != 0 ? 1u : 0u;
    return sendTestsServiceTlv(kTlvCompensationSet, QByteArray(1, static_cast<char>(v)), "吸力补偿开关", RequestKind::CompensationSet);
}

bool Qfctp::getCaseRssiRead(const QVariantMap &map)
{
    const uint8_t v = static_cast<uint8_t>(map.value("mode").toUInt() & 0xFF);
    return sendTestsServiceTlv(kTlvRssiRead, QByteArray(1, static_cast<char>(v)), "读取RSSI", RequestKind::RssiGet);
}

bool Qfctp::getCaseKeySignalRead(const QVariantMap &map)
{
    const uint8_t keyId = static_cast<uint8_t>(map.value("key").toUInt() & 0xFF);
    return sendTestsServiceTlv(kTlvKeySignalRead, QByteArray(1, static_cast<char>(keyId)), "读取按键电容值", RequestKind::KeySignalGet);
}

bool Qfctp::getCaseLightCalibRead(const QVariantMap &map)
{
    const uint8_t idx = static_cast<uint8_t>(map.value("index").toUInt() & 0xFF);
    return sendTestsServiceTlv(kTlvLightCalibRead, QByteArray(1, static_cast<char>(idx)), "读取光感校准值", RequestKind::LightCalibRead);
}

bool Qfctp::getCaseChargeCurrentRead()
{
    return sendTestsServiceTlv(kTlvChargeCurrentRead, {}, "读取充电电流", RequestKind::ChargeCurrentGet);
}

bool Qfctp::getCaseAgingStatusRead(const QVariantMap &map)
{
    const uint8_t mode = static_cast<uint8_t>(map.value("mode").toUInt() & 0xFF);
    return sendTestsServiceTlv(kTlvAgingStatus, QByteArray(1, static_cast<char>(mode)), "读取老化状态", RequestKind::AgingStatusGet);
}

bool Qfctp::getCaseFactoryDoneRead()
{
    return sendTestsServiceTlv(kTlvFactoryDoneRead, {}, "读取产测完成标识", RequestKind::FactoryDoneRead);
}

bool Qfctp::getCaseDeviceExceptionRead()
{
    return sendTestsServiceTlv(kTlvDeviceException, {}, "读取设备异常状态", RequestKind::DeviceExceptionGet);
}

bool Qfctp::getCasePeriphStateRead()
{
    return sendTestsServiceTlv(kTlvSensorState, {}, "读取外设sensor状态", RequestKind::SensorStateGet);
}

bool Qfctp::getCaseBatteryRead()
{
    return sendTestsServiceTlv(kTlvBatteryRead, {}, "读取电池电量", RequestKind::BatteryGet);
}

void Qfctp::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty()) {
        return;
    }
    QList<QByteArray> innerPackets;
    if (!tryUnwrapPhyPacket(byte, innerPackets)) {
        return;
    }
    for (const auto &inner : innerPackets) {
        const auto *p = reinterpret_cast<const uint8_t *>(inner.constData());
        const int  rc = comm_protocol_assemble_packet(p, static_cast<uint16_t>(inner.size()), qfctp_on_full_frame, this);
        if (rc < 0) {
            qWarning() << "comm_protocol_assemble_packet 错误:" << rc;
        }
    }
}

void Qfctp::set(DeviceCmd cmd, const QVariant& data) {
    switch (cmd) {
    case DeviceCmd::FacMode:
        sendFactoryTestMode(data.toInt() != 0);
        return;
    case DeviceCmd::AgingModeSet:
        if (setCaseAgingMode(data.toMap())) return;
        break;
    case DeviceCmd::AgingModeExit:
        if (setCaseAgingExit()) return;
        break;
    case DeviceCmd::SuctionMode:
        if (setCaseSuctionMode(data.toMap())) return;
        break;
    case DeviceCmd::BtSignalMode:
        if (setCaseBtSignalMode(data.toMap())) return;
        break;
    case DeviceCmd::BtNoSignalMode:
        if (setCaseBtNoSignalMode(data.toMap())) return;
        break;
    case DeviceCmd::BtFreqMode:
        if (setCaseBtFreqMode(data.toMap())) return;
        break;
    case DeviceCmd::StandbyMode:
        if (setCaseStandbyMode(data.toMap())) return;
        break;
    case DeviceCmd::Sn:
        if (handleSetSn(data)) {
            return;
        }
        break;
    case DeviceCmd::WriteProductId:
        if (setCaseWriteProductId(data.toMap())) return;
        break;
    case DeviceCmd::WriteDeviceId:
        if (setCaseWriteDeviceId(data.toMap())) return;
        break;
    case DeviceCmd::WriteKey:
        if (setCaseWriteKey(data.toMap())) return;
        break;
    case DeviceCmd::FactoryDoneWrite:
        if (setCaseFactoryDoneWrite(data.toMap())) return;
        break;
    case DeviceCmd::TrimSet:
        if (setCaseTrimSet(data.toMap())) return;
        break;
    case DeviceCmd::MacWrite:
        if (setCaseMacWrite(data.toMap())) return;
        break;
    case DeviceCmd::NightLightSet:
        if (setCaseNightLightSet(data.toMap())) return;
        break;
    case DeviceCmd::LedTest:
        if (setCaseLedTest(data.toMap())) return;
        break;
    case DeviceCmd::FactoryReset:
        if (setCaseFactoryReset()) return;
        break;
    case DeviceCmd::PowerOff:
        if (setCasePowerOff()) return;
        break;
    case DeviceCmd::LcdBacklight:
        if (setCaseLcdBacklight(data.toMap())) return;
        break;
    case DeviceCmd::LightReportControl:
        if (setCaseLightReportControl(data.toMap())) return;
        break;
    case DeviceCmd::LightCalibWrite:
        if (setCaseLightCalibWrite(data.toMap())) return;
        break;
    case DeviceCmd::CompensationSet:
        if (setCaseCompensationSet(data.toMap())) return;
        break;
    case DeviceCmd::BaseInfo:
    case DeviceCmd::Battery:
        qWarning() << "Qfctp 请改用细分DeviceCmd，不再使用BaseInfo/Battery携带op";
        break;
    case DeviceCmd::Sleep: {
        const bool enter = (data.toInt() != 0);
        if (sendTestsServiceTlv(kTlvStandbyMode, QByteArray(1, static_cast<char>(enter ? 1 : 0)), "待机模式(兼容Sleep入口)", RequestKind::StandbyMode)) {
            return;
        }
        break;
    }
    case DeviceCmd::DeviceInfo:
        qWarning() << "Qfctp DeviceInfo(set)请改用细分DeviceCmd";
        break;
    default:
        break;
    }
    emit send_pb_date(QString("Qfctp 暂未实现/参数非法 set 命令，cmd=%1 data=%2")
                          .arg(static_cast<int>(cmd))
                          .arg(data.toString()));
}

void Qfctp::get(DeviceCmd cmd, const QVariant& param) {
    switch (cmd) {
    case DeviceCmd::GetSn:
    case DeviceCmd::Sn: {
        const auto which = static_cast<FacDevInfoType>(param.toInt());
        if (which == FacDevInfoType_SUB_PID || which == FacDevInfoType_SKUID) {
            sendTestsServiceTlv(kTlvTupleRead, {}, "读三元组(兼容GetSn参数)", RequestKind::TupleRead);
        } else {
            if (which == FacDevInfoType_BOARD_SN) {
                qWarning() << "Qfctp BOARD_SN 使用SN读取通道兼容";
            }
            sendTestsServiceTlv(kTlvSnRead, {}, "读SN", RequestKind::SnRead);
        }
        return;
    }
    case DeviceCmd::TupleRead:
        if (getCaseTupleRead()) return;
        break;
    case DeviceCmd::TrimRead:
        if (getCaseTrimRead()) return;
        break;
    case DeviceCmd::FwVersionRead:
        if (getCaseFwVersionRead()) return;
        break;
    case DeviceCmd::MacRead:
        if (getCaseMacRead()) return;
        break;
    case DeviceCmd::BaseInfo:
    case DeviceCmd::GetBaseInfo:
        if (getCaseTupleRead()) return;
        break;
    case DeviceCmd::LightSensorInfo:
        if (getCaseChargeCurrentRead()) return;
        break;
    case DeviceCmd::RssiRead:
        if (getCaseRssiRead(param.toMap())) return;
        break;
    case DeviceCmd::KeySignalRead:
        if (getCaseKeySignalRead(param.toMap())) return;
        break;
    case DeviceCmd::LightCalibRead:
        if (getCaseLightCalibRead(param.toMap())) return;
        break;
    case DeviceCmd::ChargeCurrentRead:
        if (getCaseChargeCurrentRead()) return;
        break;
    case DeviceCmd::AgingStatusRead:
        if (getCaseAgingStatusRead(param.toMap())) return;
        break;
    case DeviceCmd::FactoryDoneRead:
        if (getCaseFactoryDoneRead()) return;
        break;
    case DeviceCmd::DeviceExceptionRead:
        if (getCaseDeviceExceptionRead()) return;
        break;
    case DeviceCmd::DeviceInfo:
        if (getCaseDeviceExceptionRead()) return;
        break;
    case DeviceCmd::PeriphState:
        if (getCasePeriphStateRead()) return;
        break;
    case DeviceCmd::Battery:
    case DeviceCmd::GetBattery:
        if (getCaseBatteryRead()) return;
        break;
    default:
        break;
    }
    emit send_pb_date(QString("Qfctp 暂未实现/参数非法 get 命令，cmd=%1 param=%2")
                          .arg(static_cast<int>(cmd))
                          .arg(param.toString()));
}
