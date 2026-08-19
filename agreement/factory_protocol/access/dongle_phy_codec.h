#ifndef DONGLE_PHY_CODEC_H
#define DONGLE_PHY_CODEC_H

#include <QByteArray>
#include <QList>
#include <functional>

#include "dongle_phy.h"
#include "qprotocol_types.h"

/** Dongle 吸力二进制上行 payload 长度（channel=4） */
constexpr int kDongleSuctionUplinkPayloadLen = 22;
constexpr int kDongleSuctionUplinkFrameLen = 32; // 8 magic + channel + length + payload

/** Dongle 二进制上行 payload（22B，压力 0.01 kPa）→ kPa */
bool parseDongleSuctionUplinkPayload(const QByteArray& payload, ProtocolDongleSuctionData* out);

/**
 * Dongle 串口 PHY 收包（8×0xAA → channel → len → payload）。
 * 通道 4（吸力）可设 suctionHandler_ 解析（QAT 层）；其它通道按 acceptedChannelMask_ 输出内层包。
 */
class DonglePhyRxCodec {
  public:
    using SuctionHandler = std::function<void(const ProtocolDongleSuctionData& data)>;

    explicit DonglePhyRxCodec(quint8 acceptedChannelMask = kDonglePhyRxAcceptFacAppMain,
                              const char* warnLogTag = nullptr);

    void reset();
    void setAcceptedChannelMask(quint8 mask);
    quint8 acceptedChannelMask() const;
    void setSuctionHandler(SuctionHandler handler);

    /** 流式喂入；完整内层包追加到 outInnerPackets；outChannels 与内层包一一对应 */
    void feed(const QByteArray& chunk, QList<QByteArray>& outInnerPackets, QList<quint8>* outChannels = nullptr);

  private:
    enum State { Idle, Header, Channel, Len, Payload };

    void resetState();

    quint8 acceptedChannelMask_ = 0;
    const char* warnLogTag_ = nullptr;
    SuctionHandler suctionHandler_;
    QList<QByteArray>* outInnerPackets_ = nullptr;
    State state_ = Idle;
    int headerHits_ = 0;
    int expectedLen_ = 0;
    quint8 channel_ = 0;
    QByteArray payload_;
};

/** 上位机 → Dongle：8×0xCC + len + channel + inner */
QByteArray wrapDonglePhyTxPacket(const QByteArray& innerPacket, quint8 channel = kDonglePhyChannelFac);

#endif // DONGLE_PHY_CODEC_H
