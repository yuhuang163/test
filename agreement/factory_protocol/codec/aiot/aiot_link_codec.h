#ifndef AIOT_LINK_CODEC_H
#define AIOT_LINK_CODEC_H

#include <QByteArray>
#include <QVector>
#include <cstdint>

#include "aiot_link_defs.h"

/** AIOT 链路层单帧编解码与流式拆帧（SOF=0x5A，CRC16/CCITT-FALSE）。 */
class AiotLinkCodec {
  public:
    struct Frame {
        uint8_t control = 0;
        uint8_t fsn = 0;
        bool hasFsn = false;
        QByteArray payload;
    };

    static uint16_t crc16CcittFalse(const uint8_t* data, int length);
    static uint16_t crc16CcittFalse(const QByteArray& data);

    /** 组装单帧；fsnBits=kCtrlFsnNone 时不带 FSN。checksum 为大端。 */
    static QByteArray buildFrame(const QByteArray& payload, uint8_t control = AiotLink::kCtrlFsnNone,
                                 uint8_t fsn = 0);

    /** 将应用层 PDU 按 maxPayload 切分为若干链路帧（默认不分片）。 */
    static QVector<QByteArray> buildFramesForPdu(const QByteArray& pdu, int maxPayload = 512);

    /** 喂入字节流，解析出完整帧；失败帧丢弃并继续同步 SOF。 */
    bool feed(const QByteArray& chunk, QVector<Frame>* outFrames);

    void reset();

  private:
    enum class State { WaitSof, WaitLen0, WaitLen1, WaitBody };
    State state_ = State::WaitSof;
    uint16_t expectedBody_ = 0; // Control(+FSN)+Payload+CRC
    QByteArray body_;
};

#endif // AIOT_LINK_CODEC_H
