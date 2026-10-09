#include "universal_fixture_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace UniversalFixtureCodec {

using namespace UniversalFixture;

QByteArray buildControlFrame(quint8 slot, quint8 ch, quint8 st) {
    QByteArray frame;
    frame.reserve(6);
    frame.append(char(kSof));
    frame.append(char(kTypeControl));
    frame.append(char(slot));
    frame.append(char(ch));
    frame.append(char(0x00));
    frame.append(char(st));
    return frame;
}

QByteArray buildMeasureFrame(quint8 slot, quint8 ch) {
    QByteArray frame;
    frame.reserve(6);
    frame.append(char(kSof));
    frame.append(char(kTypeMeasure));
    frame.append(char(slot));
    frame.append(char(ch));
    frame.append(char(0x00));
    frame.append(char(kOpReadOnce));
    return frame;
}

static int findSof(const QByteArray& buffer, int from) {
    for (int i = from; i < buffer.size(); ++i) {
        if (static_cast<quint8>(buffer.at(i)) == kSof)
            return i;
    }
    return -1;
}

bool takeControlAck(QByteArray* buffer, quint8 expectSlot, quint8 expectCh, quint8 expectSt, ControlAck* out,
                    QString* errorMessage) {
    if (!buffer || !out)
        return false;
    while (true) {
        const int sof = findSof(*buffer, 0);
        if (sof < 0) {
            buffer->clear();
            if (errorMessage)
                *errorMessage = QStringLiteral("未找到控制应答帧头");
            return false;
        }
        if (sof > 0)
            buffer->remove(0, sof);
        if (buffer->size() < 7) {
            if (errorMessage)
                *errorMessage = QStringLiteral("控制应答长度不足");
            return false;
        }
        const quint8 type = static_cast<quint8>(buffer->at(1));
        if (type != kTypeControlAck) {
            buffer->remove(0, 1);
            continue;
        }
        if (static_cast<quint8>(buffer->at(6)) != kEof) {
            buffer->remove(0, 1);
            continue;
        }
        const quint8 slot = static_cast<quint8>(buffer->at(2));
        const quint8 ch = static_cast<quint8>(buffer->at(3));
        const quint8 st = static_cast<quint8>(buffer->at(4));
        const quint8 status = static_cast<quint8>(buffer->at(5));
        const bool slotOk = (expectSlot == kSlotBroadcast) ? (slot >= 0x01 && slot <= 0x0F) : (slot == expectSlot);
        if (!slotOk || ch != expectCh || st != expectSt) {
            buffer->remove(0, 7);
            continue;
        }
        out->slot = slot;
        out->ch = ch;
        out->st = st;
        out->status = status;
        buffer->remove(0, 7);
        return true;
    }
}

bool takeMeasureRsp(QByteArray* buffer, quint8 expectSlot, MeasureRsp* out, QString* errorMessage) {
    if (!buffer || !out)
        return false;
    while (true) {
        const int sof = findSof(*buffer, 0);
        if (sof < 0) {
            buffer->clear();
            if (errorMessage)
                *errorMessage = QStringLiteral("未找到测量应答帧头");
            return false;
        }
        if (sof > 0)
            buffer->remove(0, sof);
        if (buffer->size() < 3) {
            if (errorMessage)
                *errorMessage = QStringLiteral("测量应答长度不足");
            return false;
        }
        const quint8 type = static_cast<quint8>(buffer->at(1));
        const quint8 len = static_cast<quint8>(buffer->at(2));
        if (type != kTypeMeasureRsp) {
            buffer->remove(0, 1);
            continue;
        }
        // 文档示例为 10 字节（EOF 在偏移 9），LEN 字段写 0x0B；两者都接受
        int frameSize = static_cast<int>(len);
        if (frameSize < 10)
            frameSize = 10;
        if (buffer->size() < frameSize) {
            if (errorMessage)
                *errorMessage = QStringLiteral("测量应答未收齐");
            return false;
        }
        if (static_cast<quint8>(buffer->at(frameSize - 1)) != kEof) {
            if (len == kMeasureLen && buffer->size() >= 10 && static_cast<quint8>(buffer->at(9)) == kEof)
                frameSize = 10;
            else {
                buffer->remove(0, 1);
                continue;
            }
        }
        const quint8 slot = static_cast<quint8>(buffer->at(3));
        if (slot != expectSlot) {
            buffer->remove(0, frameSize);
            continue;
        }
        out->slot = slot;
        out->ch = static_cast<quint8>(buffer->at(4));
        const quint8 hi = static_cast<quint8>(buffer->at(5));
        const quint8 lo = static_cast<quint8>(buffer->at(6));
        out->currentRaw = static_cast<qint16>((hi << 8) | lo);
        out->status = static_cast<quint8>(buffer->at(7));
        buffer->remove(0, frameSize);
        return true;
    }
}

QVector<MeasureRsp> takeAllMeasureRsp(QByteArray* buffer, quint8 expectSlot) {
    QVector<MeasureRsp> list;
    while (true) {
        MeasureRsp one;
        QString err;
        if (!takeMeasureRsp(buffer, expectSlot, &one, &err))
            break;
        list.append(one);
    }
    return list;
}

QString controlStatusText(quint8 status) {
    switch (status) {
    case 0x00:
        return QStringLiteral("到位/完成");
    case 0x01:
        return QStringLiteral("SLOT/CH无效");
    case 0x03:
        return QStringLiteral("硬件故障");
    case 0x04:
        return QStringLiteral("工位忙");
    case 0x05:
        return QStringLiteral("到位超时");
    case 0x06:
        return QStringLiteral("气源/互锁不允许");
    default:
        return QStringLiteral("未知错误(0x%1)").arg(status, 2, 16, QChar('0'));
    }
}

QString measureStatusText(quint8 status) {
    switch (status) {
    case 0x00:
        return QStringLiteral("成功");
    case 0x01:
        return QStringLiteral("SLOT/CH无效");
    case 0x02:
        return QStringLiteral("测量未就绪");
    case 0x03:
        return QStringLiteral("硬件故障");
    case 0x04:
        return QStringLiteral("工位忙");
    default:
        return QStringLiteral("未知错误(0x%1)").arg(status, 2, 16, QChar('0'));
    }
}

} // namespace UniversalFixtureCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
