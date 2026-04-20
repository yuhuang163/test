#include "qfctp.h"
#include <QDebug>
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

    if (frame.frame_type != COMM_PROTOCOL_FRAME_TYPE_RESPONSE) {
        return;
    }
    if (frame.payload.service_count < 1) {
        return;
    }
    const comm_protocol_service_node_t &svc = frame.payload.services[0];
    if (svc.svr_id != COMM_PROTOCOL_TESTS_SERVICE) {
        return;
    }
    uint8_t  modeAck   = 0xFF;
    uint16_t errTlv    = 0xFFFF;
    bool     haveMode  = false;
    bool     haveErr   = false;
    uint16_t off       = 0;
    while (svc.tlvs != nullptr && off + COMM_PROTOCOL_TLV_HEADER_SIZE <= svc.srv_length) {
        const uint8_t *p   = svc.tlvs + off;
        const uint16_t t   = qfctpReadLe16(p);
        const uint16_t L   = qfctpReadLe16(p + 2);
        if (off + COMM_PROTOCOL_TLV_HEADER_SIZE + L > svc.srv_length) {
            break;
        }
        const uint8_t *val = p + COMM_PROTOCOL_TLV_HEADER_SIZE;
        if (t == COMM_PROTOCOL_TLV_TYPE_FACTORY_TEST_MODE && L >= 1u) {
            modeAck  = val[0];
            haveMode = true;
        }
        if (t == PROTOCOL_SERVICE_ERROR_CODE_TLV_ID && L >= 2u) {
            errTlv   = qfctpReadLe16(val);
            haveErr  = true;
        }
        off = static_cast<uint16_t>(off + COMM_PROTOCOL_TLV_HEADER_SIZE + L);
    }
    if (haveMode || haveErr) {
        const bool ok = haveMode && haveErr && (modeAck == 1u) && (errTlv == 0u);
        qDebug() << "FCTP 产测模式应答" << "mode tlv:"
                  << (haveMode ? QString::number(modeAck) : "-")
                  << "err tlv:" << (haveErr ? QString::number(errTlv) : "-")
                  << "判定" << (ok ? "成功" : "失败/不完整");
    }
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
                     qWarning() << "FCTP 外层完成:" << m_phyExpectedLen;
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

void Qfctp::sendFactoryTestMode(bool enter)
{
    comm_protocol_builder_t b{};
    std::memset(&b, 0, sizeof(b));
    const uint8_t seq = static_cast<uint8_t>(++m_fctpSeq);
    if (comm_protocol_builder_init(&b, 64, seq, COMM_PROTOCOL_FRAME_TYPE_REQUEST) != 0) {
        qWarning() << "FCTP 产测模式组包失败";
        return;
    }
    comm_protocol_service_t *svc = comm_protocol_builder_init_service(&b, COMM_PROTOCOL_TESTS_SERVICE);
    if (svc == nullptr) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP 产测模式组包失败";
        return;
    }
    uint8_t                 v = enter ? 1u : 0u;
    comm_protocol_tlv_node_t node{};
    node.type   = COMM_PROTOCOL_TLV_TYPE_FACTORY_TEST_MODE;
    node.length = 1;
    node.value  = &v;
    if (comm_protocol_builder_add_tlv(&b, svc, &node) != 0) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP 产测模式组包失败";
        return;
    }
    uint16_t outSize = 0;
    uint8_t *raw     = comm_protocol_builder_finalize(&b, &outSize);
    if (raw == nullptr) {
        comm_protocol_builder_free(&b);
        qWarning() << "FCTP 产测模式组包失败";
        return;
    }
    const QByteArray pkt(reinterpret_cast<const char *>(raw), static_cast<int>(outSize));
    comm_protocol_builder_free(&b);
    if (pkt.isEmpty()) {
        qWarning() << "FCTP 产测模式组包失败";
        return;
    }

    QByteArray phyPacket;
    if (!sendPacket(pkt, &phyPacket)) {
        return;
    }
    qDebug() << "FCTP 已发送产测模式" << (enter ? "进入" : "退出")
             << "seq=" << static_cast<int>(m_fctpSeq)
             << "inner=" << pkt.toHex(' ')
             << "outer=" << phyPacket.toHex(' ');
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
    case DeviceCmd::FacMode: {
        // 与 qpb 风格保持一致：使用同一个 DeviceCmd，通过 data 控制开关
        const bool enter = data.toInt() != 0;
        sendFactoryTestMode(enter);
        return;
    }
    default:
        qWarning() << "Qfctp 暂未实现 set 命令，cmd =" << static_cast<int>(cmd);
        return;
    }
}

void Qfctp::get(DeviceCmd cmd, const QVariant& param) {
    (void)param;
    qWarning() << "Qfctp 暂未实现 get 命令，cmd =" << static_cast<int>(cmd);
}
