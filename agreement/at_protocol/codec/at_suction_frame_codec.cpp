#include "at_suction_frame_codec.h"
#include <QRegularExpression>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

bool parseAtSuctionDataLine(const QString& line, double* leftKpa, double* rightKpa, double* thirdKpa) {
    if (!leftKpa || !rightKpa)
        return false;
    QString payload = line.trimmed();
    static const QString kPrefix = QStringLiteral("AT+SUCTION_DATA=");
    if (!payload.startsWith(kPrefix, Qt::CaseInsensitive))
        return false;
    payload = payload.mid(kPrefix.size()).trimmed();
    const QStringList parts = payload.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 2)
        return false;
    bool okLeft = false;
    bool okRight = false;
    const double left = parts.at(0).toDouble(&okLeft);
    const double right = parts.at(1).toDouble(&okRight);
    if (!okLeft || !okRight)
        return false;
    *leftKpa = left;
    *rightKpa = right;
    if (thirdKpa && parts.size() >= 3) {
        bool okThird = false;
        const double third = parts.at(2).toDouble(&okThird);
        *thirdKpa = okThird ? third : 0.0;
    }
    return true;
}

bool parseDualChannelSuctionFrame(const QString& data, double* leftKpa, double* rightKpa, double* thirdKpa) {
    if (!leftKpa || !rightKpa)
        return false;
    QString payload = data.trimmed();
    const int dollarIndex = payload.indexOf(QLatin1Char('$'));
    if (dollarIndex < 0)
        return false;
    payload = payload.mid(dollarIndex + 1);
    const int semicolonIndex = payload.indexOf(QLatin1Char(';'));
    if (semicolonIndex < 0)
        return false;
    payload = payload.left(semicolonIndex);
    const QRegularExpression numberRegex(QStringLiteral("-?\\d+(?:\\.\\d+)?"));
    QRegularExpressionMatchIterator it = numberRegex.globalMatch(payload);
    QVector<double> values;
    while (it.hasNext()) {
        bool ok = false;
        const double value = it.next().captured(0).toDouble(&ok);
        if (ok)
            values.append(value);
    }
    if (values.size() < 2)
        return false;
    *leftKpa = values.at(0);
    *rightKpa = values.at(1);
    if (thirdKpa)
        *thirdKpa = values.size() >= 3 ? values.at(2) : 0.0;
    return true;
}

void AtSuctionFrameCodec::reset() {
    textBuffer_.clear();
    phyRx_.reset();
}

void AtSuctionFrameCodec::appendTextByte(char c) {
    // 可打印 ASCII 与常见换行；channel=4 二进制由 phyRx_ 解析
    const uchar u = static_cast<uchar>(c);
    if (u == '\r' || u == '\n' || u == '\t' || (u >= 0x20 && u <= 0x7E))
        textBuffer_.append(QLatin1Char(c));
}

void AtSuctionFrameCodec::flushTextFrames(const FrameHandler& onFrame) {
    while (true) {
        int lineEnd = -1;
        int skipLen = 0;
        const int crlfIndex = textBuffer_.indexOf(QStringLiteral("\r\n"));
        const int lfIndex = textBuffer_.indexOf(QLatin1Char('\n'));
        const int crIndex = textBuffer_.indexOf(QLatin1Char('\r'));
        if (crlfIndex >= 0) {
            lineEnd = crlfIndex;
            skipLen = 2;
        } else if (lfIndex >= 0) {
            lineEnd = lfIndex;
            skipLen = 1;
        } else if (crIndex >= 0) {
            lineEnd = crIndex;
            skipLen = 1;
        }

        if (lineEnd >= 0) {
            const QString line = textBuffer_.left(lineEnd).trimmed();
            textBuffer_.remove(0, lineEnd + skipLen);
            if (line.isEmpty())
                continue;
            if (line.startsWith(QStringLiteral("AT+TEMP_DATA"), Qt::CaseInsensitive))
                continue;
            // AT+SUCTION_DATA 走 DongleAtDevice::suction_data，此处仅处理 Pico $...; 帧
            if (line.startsWith(QStringLiteral("AT+SUCTION_DATA"), Qt::CaseInsensitive))
                continue;

            double left = 0.0;
            double right = 0.0;
            double third = 0.0;
            if (parseDualChannelSuctionFrame(line, &left, &right, &third) && onFrame) {
                ProtocolDongleSuctionData data;
                data.ch1Kpa = left;
                data.ch2Kpa = right;
                data.ch3Kpa = third;
                onFrame(data);
            }
            continue;
        }

        const int semicolonIndex = textBuffer_.indexOf(QLatin1Char(';'));
        if (semicolonIndex < 0 || !textBuffer_.contains(QLatin1Char('$')))
            break;

        const QString frame = textBuffer_.left(semicolonIndex + 1).trimmed();
        textBuffer_.remove(0, semicolonIndex + 1);
        if (frame.isEmpty() || !frame.startsWith(QLatin1Char('$')))
            continue;

        double left = 0.0;
        double right = 0.0;
        double third = 0.0;
        if (parseDualChannelSuctionFrame(frame, &left, &right, &third) && onFrame) {
            ProtocolDongleSuctionData data;
            data.ch1Kpa = left;
            data.ch2Kpa = right;
            data.ch3Kpa = third;
            onFrame(data);
        }
    }

    if (textBuffer_.size() > 4096)
        textBuffer_ = textBuffer_.right(1024);
}

void AtSuctionFrameCodec::feed(const QByteArray& chunk, const FrameHandler& onFrame) {
    if (chunk.isEmpty())
        return;
    phyRx_.setSuctionHandler(onFrame);
    QList<QByteArray> unused;
    phyRx_.feed(chunk, unused);
    // 二进制 PHY chunk 里的可打印字节会污染 textBuffer_，遇帧头则清空
    if (chunk.size() >= kDonglePhyHeaderSize) {
        bool phyHeader = true;
        for (int i = 0; i < kDonglePhyHeaderSize; ++i) {
            if (static_cast<quint8>(chunk.at(i)) != kDonglePhyRxHeaderByte) {
                phyHeader = false;
                break;
            }
        }
        if (phyHeader)
            textBuffer_.clear();
    }
    for (char c : chunk)
        appendTextByte(c);
    flushTextFrames(onFrame);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
