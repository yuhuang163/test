#ifndef DJI_BULK_CODEC_H
#define DJI_BULK_CODEC_H

#include <QByteArray>
#include <functional>
#include <atomic>
#include "../access/bulk_types.h"

class DjiBulkCodec {
  public:
    using FrameHandler = std::function<void(const DjiBulkFrame& frame)>;

    void feed(QByteArray& buffer, const FrameHandler& onFrame);

    static QByteArray buildPacket(uint8_t receiver, uint8_t cmdType, uint8_t cmdSet,
                                  uint8_t cmdID, const QByteArray& data,
                                  uint8_t encryptionType = 0, uint8_t isResponse = 0);

    static uint16_t duss_util_crc16_calc(const uint8_t* data, uint32_t len, uint16_t init_crc);
    static uint8_t crc8_calc(const uint8_t* data, uint32_t len, uint8_t init_crc);

    static void md5_init(md5_state_t* pms);
    static void md5_append(md5_state_t* pms, const md5_byte_t* data, int nbytes);
    static void md5_finish(md5_state_t* pms, md5_byte_t digest[16]);

  private:
    static bool checkHeaderCRC(const QByteArray& packet);
    static bool checkDataCRC(const QByteArray& packet);

    static std::atomic<uint16_t> m_seqNum;
};

#endif // DJI_BULK_CODEC_H
