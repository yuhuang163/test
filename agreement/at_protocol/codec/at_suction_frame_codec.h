#ifndef AT_SUCTION_FRAME_CODEC_H
#define AT_SUCTION_FRAME_CODEC_H

#include <QByteArray>
#include <functional>

#include "qprotocol_types.h"

/** Dongle AT 行：AT+SUCTION_DATA=左,右,...；Pico 兼容：$左 右 ...; */
bool parseAtSuctionDataLine(const QString& line, double* leftKpa, double* rightKpa);
bool parseDualChannelSuctionFrame(const QString& data, double* leftKpa, double* rightKpa);

/** Dongle 吸力原始帧流式组帧 */
class AtSuctionFrameCodec {
  public:
    using FrameHandler = std::function<void(const ProtocolDongleSuctionData& data)>;

    void reset();
    void feed(const QByteArray& chunk, const FrameHandler& onFrame);

  private:
    QString buffer_;
};

#endif // AT_SUCTION_FRAME_CODEC_H
