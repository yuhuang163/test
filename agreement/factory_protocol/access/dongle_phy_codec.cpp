#include "dongle_phy_codec.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** 与 AT+SUCTION_DATA 一致：int32 为 0.01 kPa（centi-kPa） */
constexpr double kCentiKpaToKpa = 0.01;

qint32 readLeI32(const char* p) {
    const auto u = static_cast<quint32>(static_cast<quint8>(p[0])) | (static_cast<quint32>(static_cast<quint8>(p[1])) << 8)
                   | (static_cast<quint32>(static_cast<quint8>(p[2])) << 16)
                   | (static_cast<quint32>(static_cast<quint8>(p[3])) << 24);
    return static_cast<qint32>(u);
}

} // namespace

bool parseDongleSuctionUplinkPayload(const QByteArray& payload, ProtocolDongleSuctionData* out) {
    if (!out || payload.size() < kDongleSuctionUplinkPayloadLen)
        return false;
    const char* p = payload.constData();
    out->dongleTimestampMs = readLeI32(p);
    out->ch1Kpa = static_cast<double>(readLeI32(p + 4)) * kCentiKpaToKpa;
    out->ch2Kpa = static_cast<double>(readLeI32(p + 8)) * kCentiKpaToKpa;
    out->ch3Kpa = static_cast<double>(readLeI32(p + 12)) * kCentiKpaToKpa;
    return true;
}

DonglePhyRxCodec::DonglePhyRxCodec(quint8 acceptedChannelMask, const char* warnLogTag)
    : acceptedChannelMask_(acceptedChannelMask), warnLogTag_(warnLogTag) {}

void DonglePhyRxCodec::reset() {
    resetState();
}

void DonglePhyRxCodec::setAcceptedChannelMask(quint8 mask) {
    acceptedChannelMask_ = mask;
}

quint8 DonglePhyRxCodec::acceptedChannelMask() const {
    return acceptedChannelMask_;
}

void DonglePhyRxCodec::setSuctionHandler(SuctionHandler handler) {
    suctionHandler_ = std::move(handler);
}

void DonglePhyRxCodec::resetState() {
    state_ = Idle;
    headerHits_ = 0;
    expectedLen_ = 0;
    channel_ = 0;
    payload_.clear();
    outInnerPackets_ = nullptr;
}

void DonglePhyRxCodec::feed(const QByteArray& chunk, QList<QByteArray>& outInnerPackets, QList<quint8>* outChannels) {
    outInnerPackets_ = &outInnerPackets;
    QList<quint8>* outChannels_ = outChannels;
    for (char ch : chunk) {
        const quint8 x = static_cast<quint8>(ch);
        switch (state_) {
        case Idle:
            if (x == kDonglePhyRxHeaderByte) {
                headerHits_ = 1;
                state_ = Header;
            }
            break;
        case Header:
            if (x == kDonglePhyRxHeaderByte) {
                if (++headerHits_ == kDonglePhyHeaderSize)
                    state_ = Channel;
            } else {
                resetState();
                outInnerPackets_ = &outInnerPackets;
                // 帧头中断：当前字节可能是下一帧首个 0xAA
                if (x == kDonglePhyRxHeaderByte) {
                    headerHits_ = 1;
                    state_ = Header;
                }
            }
            break;
        case Channel:
            channel_ = x;
            state_ = Len;
            break;
        case Len:
            expectedLen_ = static_cast<int>(x);
            if (expectedLen_ <= 0) {
                if (warnLogTag_)
                    qWarning() << warnLogTag_ << "dongle 外层包长度非法:" << expectedLen_;
                resetState();
                outInnerPackets_ = &outInnerPackets;
                break;
            }
            payload_.clear();
            payload_.reserve(expectedLen_);
            state_ = Payload;
            break;
        case Payload:
            payload_.append(static_cast<char>(x));
            if (payload_.size() >= expectedLen_) {
                if (channel_ == kDonglePhyChannelSuction) {
                    // channel=4 一律走吸力分支；payload 只要可解析即上报
                    if (suctionHandler_) {
                        ProtocolDongleSuctionData data;
                        if (parseDongleSuctionUplinkPayload(payload_, &data))
                            suctionHandler_(data);
                    }
                    // 无 suctionHandler_ 时静默丢弃（channel=4 由 QAT AtSuctionFrameCodec 注册）
                } else if (channel_ < 8 && (acceptedChannelMask_ & quint8(1u << channel_)) != 0) {
                    outInnerPackets.append(payload_);
                    if (outChannels_)
                        outChannels_->append(channel_);
                } else if (warnLogTag_) {
                    qWarning() << warnLogTag_ << "dongle 通道异常 channel=" << channel_;
                }
                resetState();
                outInnerPackets_ = &outInnerPackets;
            }
            break;
        default:
            resetState();
            outInnerPackets_ = &outInnerPackets;
            break;
        }
    }
    outInnerPackets_ = nullptr;
}

QByteArray wrapDonglePhyTxPacket(const QByteArray& innerPacket, quint8 channel) {
    if (innerPacket.isEmpty() || innerPacket.size() > 0xFF)
        return {};
    QByteArray phy;
    phy.reserve(kDonglePhyHeaderSize + 2 + innerPacket.size());
    phy.append(QByteArray(kDonglePhyHeaderSize, static_cast<char>(kDonglePhyTxHeaderByte)));
    phy.append(static_cast<char>(innerPacket.size()));
    phy.append(static_cast<char>(channel));
    phy.append(innerPacket);
    return phy;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
