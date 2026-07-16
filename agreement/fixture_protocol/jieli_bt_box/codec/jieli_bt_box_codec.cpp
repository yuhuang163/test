#include "jieli_bt_box_codec.h"

#include <QDebug>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

quint16 crc16Xmodem(const uchar* data, int len) {
    quint16 crc = 0;
    for (int i = 0; i < len; ++i) {
        crc ^= static_cast<quint16>(data[i]) << 8;
        for (int b = 0; b < 8; ++b) {
            if (crc & 0x8000)
                crc = static_cast<quint16>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<quint16>(crc << 1);
        }
    }
    return crc;
}

quint16 readLe16(const QByteArray& buf, int offset) {
    return static_cast<quint16>((static_cast<quint8>(buf.at(offset))) |
                               (static_cast<quint8>(buf.at(offset + 1)) << 8));
}

quint32 readLe32(const QByteArray& buf, int offset) {
    return (static_cast<quint32>(static_cast<quint8>(buf.at(offset)))) |
           (static_cast<quint32>(static_cast<quint8>(buf.at(offset + 1))) << 8) |
           (static_cast<quint32>(static_cast<quint8>(buf.at(offset + 2))) << 16) |
           (static_cast<quint32>(static_cast<quint8>(buf.at(offset + 3))) << 24);
}

qint32 readLeSignedFromV(const QByteArray& v) {
    quint32 u = 0;
    const int n = qMin(4, v.size());
    for (int i = 0; i < n; ++i)
        u |= static_cast<quint32>(static_cast<quint8>(v.at(i))) << (8 * i);
    return static_cast<qint32>(u);
}

/** 校验 LEN 后 body 的 TLV 是否可完整走完且无越界 */
bool tlvBodyLooksValid(const QByteArray& body) {
    if (body.size() < 4)
        return false;
    int pos = 4; // 跳过 MASK
    bool sawFreq = false;
    bool sawRssi = false;
    while (pos + 2 <= body.size()) {
        const int L = static_cast<quint8>(body.at(pos));
        const int T = static_cast<quint8>(body.at(pos + 1));
        if (pos + 2 + L > body.size())
            return false;
        if (T == JieliBtBoxCodec::TlvFreqOffset && L >= 1)
            sawFreq = true;
        if (T == JieliBtBoxCodec::TlvRssi && L >= 1)
            sawRssi = true;
        pos += 2 + L;
    }
    // 要求正好吃完 body，且至少有频偏或 RSSI
    return pos == body.size() && (sawFreq || sawRssi);
}

bool fillFromBody(const QByteArray& body, JieliBtBoxRfInfo* out) {
    out->mask = readLe32(body, 0);
    int pos = 4;
    while (pos + 2 <= body.size()) {
        const int L = static_cast<quint8>(body.at(pos));
        const int T = static_cast<quint8>(body.at(pos + 1));
        if (pos + 2 + L > body.size())
            return false;
        const QByteArray V = body.mid(pos + 2, L);
        switch (T) {
        case JieliBtBoxCodec::TlvMac:
        case JieliBtBoxCodec::TlvPairedMac:
            if (L >= 6) {
                out->mac = V.left(6);
                out->hasMac = true;
            }
            break;
        case JieliBtBoxCodec::TlvFreqOffset:
            if (L >= 1) {
                out->freqOffset = readLeSignedFromV(V);
                out->hasFreqOffset = true;
            }
            break;
        case JieliBtBoxCodec::TlvRssi:
            if (L >= 1) {
                out->rssi = readLeSignedFromV(V);
                out->hasRssi = true;
            }
            break;
        default:
            break;
        }
        pos += 2 + L;
    }
    return pos == body.size() && (out->hasFreqOffset || out->hasRssi);
}

} // namespace

namespace JieliBtBoxCodec {

bool tryExtractFrame(const QByteArray& buf, QByteArray* frame, int* consumed) {
    if (frame)
        frame->clear();
    if (consumed)
        *consumed = 0;
    if (buf.size() < 8)
        return false;

    // 先按 LEN+TLV 结构搜帧；CRC 对不上也不拒（实测多帧算法不一致）
    for (int i = 0; i + 4 <= buf.size(); ++i) {
        const int bodyLen = static_cast<int>(readLe16(buf, i + 2));
        if (bodyLen < 4 || bodyLen > 1024)
            continue;
        const int total = 4 + bodyLen; // CRC(2)+LEN(2)+body
        if (i + total > buf.size())
            continue;
        const QByteArray body = buf.mid(i + 4, bodyLen);
        if (!tlvBodyLooksValid(body))
            continue;

        const quint16 expectCrc = readLe16(buf, i);
        const uchar* crcStart = reinterpret_cast<const uchar*>(buf.constData() + i + 2);
        const quint16 calcCrc = crc16Xmodem(crcStart, 2 + bodyLen);
        if (calcCrc != expectCrc) {
            qDebug().noquote() << QStringLiteral("JIELI_BT_BOX CRC 未匹配(忽略)：期望=0x%1 计算XMODEM=0x%2")
                                     .arg(expectCrc, 4, 16, QLatin1Char('0'))
                                     .arg(calcCrc, 4, 16, QLatin1Char('0'));
        }

        if (frame)
            *frame = buf.mid(i, total);
        if (consumed)
            *consumed = i + total;
        return true;
    }
    return false;
}

bool parseFrame(const QByteArray& frame, JieliBtBoxRfInfo* out, QString* errorMessage) {
    if (!out) {
        if (errorMessage)
            *errorMessage = QStringLiteral("输出为空");
        return false;
    }
    *out = JieliBtBoxRfInfo{};
    if (frame.size() < 8) {
        if (errorMessage)
            *errorMessage = QStringLiteral("帧过短");
        return false;
    }

    const int bodyLen = static_cast<int>(readLe16(frame, 2));
    if (frame.size() != 4 + bodyLen) {
        if (errorMessage)
            *errorMessage = QStringLiteral("帧长度与 LEN 不符");
        return false;
    }

    const QByteArray body = frame.mid(4, bodyLen);
    if (!fillFromBody(body, out)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("TLV 解析失败或未找到频偏/RSSI");
        return false;
    }
    return true;
}

} // namespace JieliBtBoxCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
