#include "aa55_ammeter_rtu.h"

#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static quint16 aa55ChecksumBe(const QByteArray& content) {
    quint16 sum = 0;
    for (int i = 0; i < content.size(); ++i) {
        sum = static_cast<quint16>(sum + static_cast<quint8>(content.at(i)));
    }
    return sum;
}

static QByteArray aa55BuildFrame(const QByteArray& content) {
    QByteArray frame;
    frame.append(char(0xAA));
    frame.append(char(0x55));
    frame.append(content);
    const quint16 cs = aa55ChecksumBe(content);
    frame.append(char(quint8(cs >> 8)));
    frame.append(char(quint8(cs & 0xFF)));
    return frame;
}

QByteArray Aa55AmmeterRtu::buildReadMeasurementRequest() {
    // AA 55 02 FE 01 00：content=02 FE，校验=0x02+0xFE=0x0100
    QByteArray content;
    content.append(char(0x02));
    content.append(char(0xFE));
    return aa55BuildFrame(content);
}

bool Aa55AmmeterRtu::parseResponseFrame(const QByteArray& frame, QString* valueText) {
    // 最小应答：AA 55 | len cmd d0 d1 | cs_hi cs_lo → 至少 8 字节（len=4）
    if (frame.size() < 6) {
        return false;
    }
    if (static_cast<quint8>(frame.at(0)) != 0xAA || static_cast<quint8>(frame.at(1)) != 0x55) {
        return false;
    }
    const int contentLen = static_cast<quint8>(frame.at(2));
    if (contentLen < 2) {
        return false;
    }
    const int totalLen = 2 + contentLen + 2;
    if (frame.size() < totalLen) {
        return false;
    }
    const QByteArray content = frame.mid(2, contentLen);
    const quint16 expectCs = aa55ChecksumBe(content);
    const quint16 gotCs = (static_cast<quint16>(static_cast<quint8>(frame.at(2 + contentLen))) << 8)
                          | static_cast<quint8>(frame.at(2 + contentLen + 1));
    if (gotCs != expectCs) {
        return false;
    }
    // data：length 之后 cmd 1 字节，再跟 LE uint16 电流 mA（例 E8 03 → 1000）
    if (contentLen < 4) {
        return false;
    }
    const quint16 milliAmp = static_cast<quint16>(static_cast<quint8>(content.at(2)))
                             | (static_cast<quint16>(static_cast<quint8>(content.at(3))) << 8);
    if (valueText) {
        *valueText = QString::number(milliAmp);
    }
    return true;
}

QByteArray Aa55AmmeterRtu::buildRequest(int cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (static_cast<Aa55AmmeterRtuCmd>(cmd)) {
    case Aa55AmmeterRtuCmd::ReadMeasurement:
        return buildReadMeasurementRequest();
    default:
        return {};
    }
}

bool Aa55AmmeterRtu::parseResponse(const QByteArray& frame, QString* valueText) {
    return parseResponseFrame(frame, valueText);
}

void Aa55AmmeterRtu::resetRxState() {
    rtuRxBuffer_.reset();
}

bool Aa55AmmeterRtu::feedRx(const QByteArray& chunk, QString* valueText) {
    if (chunk.isEmpty()) {
        return false;
    }
    rtuRxBuffer_.feed(chunk);
    QByteArray buf = rtuRxBuffer_.buffer();
    // 丢掉帧头前的噪声，保留 AA 55 起的粘包缓冲
    while (buf.size() >= 2) {
        const int idx = buf.indexOf(QByteArray::fromHex("AA55"));
        if (idx < 0) {
            // 可能只剩半个 AA，保留末字节
            if (static_cast<quint8>(buf.at(buf.size() - 1)) == 0xAA) {
                rtuRxBuffer_.reset();
                rtuRxBuffer_.feed(QByteArray(1, char(0xAA)));
            } else {
                rtuRxBuffer_.reset();
            }
            return false;
        }
        if (idx > 0) {
            buf.remove(0, idx);
            rtuRxBuffer_.reset();
            rtuRxBuffer_.feed(buf);
        }
        if (buf.size() < 3) {
            return false;
        }
        const int contentLen = static_cast<quint8>(buf.at(2));
        const int totalLen = 2 + contentLen + 2;
        if (contentLen < 2 || totalLen > 64) {
            // 非法长度：丢弃当前 AA55，继续扫
            buf.remove(0, 2);
            rtuRxBuffer_.reset();
            rtuRxBuffer_.feed(buf);
            continue;
        }
        if (buf.size() < totalLen) {
            return false;
        }
        const QByteArray frame = buf.left(totalLen);
        if (parseResponseFrame(frame, valueText)) {
            rtuRxBuffer_.reset();
            return true;
        }
        // 校验失败：跳过本 AA55，继续找下一帧
        buf.remove(0, 2);
        rtuRxBuffer_.reset();
        rtuRxBuffer_.feed(buf);
    }
    return false;
}
