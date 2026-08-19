#ifndef AT_SUCTION_FRAME_CODEC_H
#define AT_SUCTION_FRAME_CODEC_H
#include <QByteArray>
#include <functional>
#include "dongle_phy.h"
#include "dongle_phy_codec.h"
#include "qprotocol_types.h"

/** Dongle AT 行：AT+SUCTION_DATA=左,右,...；Pico 兼容：$左 右 ...; */
bool parseAtSuctionDataLine(const QString& line, double* leftKpa, double* rightKpa, double* thirdKpa = nullptr);
bool parseDualChannelSuctionFrame(const QString& data, double* leftKpa, double* rightKpa, double* thirdKpa = nullptr);

/** Dongle 吸力上行：Pico $...; 文本 + PHY channel=4 二进制（统一在 QAT 层解析） */

class AtSuctionFrameCodec {
  public:
    using FrameHandler = std::function<void(const ProtocolDongleSuctionData& data)>;
    void reset();
    void feed(const QByteArray& chunk, const FrameHandler& onFrame);
  private:
    void appendTextByte(char c);
    void flushTextFrames(const FrameHandler& onFrame);
    QString textBuffer_;
    /** 仅解析 channel=4 吸力二进制；产测通道 1/2/3 由各自协议 phyRx_ 处理 */
    DonglePhyRxCodec phyRx_{0, nullptr};
};
#endif // AT_SUCTION_FRAME_CODEC_H
