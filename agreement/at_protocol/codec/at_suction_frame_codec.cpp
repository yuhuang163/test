#include "at_suction_frame_codec.h"



#include "qprotocol_types.h"



#include <QRegularExpression>



#if _MSC_VER >= 1600

#pragma execution_character_set(push, "utf-8")

#endif



bool parseAtSuctionDataLine(const QString& line, double* leftKpa, double* rightKpa) {

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

    return true;

}



bool parseDualChannelSuctionFrame(const QString& data, double* leftKpa, double* rightKpa) {

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

    return true;

}



void AtSuctionFrameCodec::reset() {

    buffer_.clear();

}



void AtSuctionFrameCodec::feed(const QByteArray& chunk, const FrameHandler& onFrame) {

    if (chunk.isEmpty())

        return;

    buffer_ += QString::fromUtf8(chunk);

    while (true) {

        int lineEnd = -1;

        int skipLen = 0;

        const int crlfIndex = buffer_.indexOf(QStringLiteral("\r\n"));

        const int lfIndex = buffer_.indexOf(QLatin1Char('\n'));

        const int crIndex = buffer_.indexOf(QLatin1Char('\r'));

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

            const QString line = buffer_.left(lineEnd).trimmed();

            buffer_.remove(0, lineEnd + skipLen);

            if (line.isEmpty())

                continue;

            if (line.startsWith(QStringLiteral("AT+TEMP_DATA"), Qt::CaseInsensitive))

                continue;

            // AT+SUCTION_DATA 走 DongleAtDevice::suction_data，此处仅处理 Pico $...; 帧

            if (line.startsWith(QStringLiteral("AT+SUCTION_DATA"), Qt::CaseInsensitive))

                continue;



            double left = 0.0;

            double right = 0.0;

            if (parseDualChannelSuctionFrame(line, &left, &right) && onFrame) {

                ProtocolDongleSuctionData data;

                data.leftKpa = left;

                data.rightKpa = right;

                onFrame(data);

            }

            continue;

        }



        const int semicolonIndex = buffer_.indexOf(QLatin1Char(';'));

        if (semicolonIndex < 0 || !buffer_.contains(QLatin1Char('$')))

            break;



        const QString frame = buffer_.left(semicolonIndex + 1).trimmed();

        buffer_.remove(0, semicolonIndex + 1);

        if (frame.isEmpty())

            continue;



        double left = 0.0;

        double right = 0.0;

        if (parseDualChannelSuctionFrame(frame, &left, &right) && onFrame) {

            ProtocolDongleSuctionData data;

            data.leftKpa = left;

            data.rightKpa = right;

            onFrame(data);

        }

    }

    if (buffer_.size() > 4096)

        buffer_ = buffer_.right(1024);

}



#if _MSC_VER >= 1600

#pragma execution_character_set(pop)

#endif

