#ifndef JIELI_BT_BOX_CODEC_H
#define JIELI_BT_BOX_CODEC_H

#include <QByteArray>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 杰理蓝牙盒子串口 TLV 上报：CRC16-XMODEM(LEN+body) | LEN | MASK | {L,T,V}* */
struct JieliBtBoxRfInfo {
    QByteArray mac;          // T=1，6 字节
    qint32 freqOffset = 0;   // T=7，小端 int32；例 1B 00 00 00 → 27
    qint32 rssi = 0;         // T=8，小端 int32；例 CD FF FF FF → -51
    quint32 mask = 0;
    bool hasMac = false;
    bool hasFreqOffset = false;
    bool hasRssi = false;
};

namespace JieliBtBoxCodec {

enum TlvType : quint8 {
    TlvMac = 1,
    TlvSdkVersion = 2,
    TlvCrc32 = 3,
    TlvBattery = 4,
    TlvPairState = 5,
    TlvPairedMac = 6,
    TlvFreqOffset = 7,
    TlvRssi = 8,
    TlvFreqCalib = 9,
};

/** 从流里抠出一帧（CRC 校验通过），consumed 为从 buf 起始消耗字节数。 */
bool tryExtractFrame(const QByteArray& buf, QByteArray* frame, int* consumed = nullptr);

/** 解析完整一帧，至少需要 T=7 或 T=8 之一成功才返回 true（调用方可再要求两者齐全）。 */
bool parseFrame(const QByteArray& frame, JieliBtBoxRfInfo* out, QString* errorMessage = nullptr);

} // namespace JieliBtBoxCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // JIELI_BT_BOX_CODEC_H
