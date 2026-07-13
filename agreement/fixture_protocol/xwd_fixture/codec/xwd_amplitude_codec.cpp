#include "xwd_amplitude_codec.h"

#include "qdebug.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdAmplitudeCodec {

bool parsePeakAmplitude(const QByteArray& byte, int* peakAmplitude) {
    if (byte.isEmpty())
        return false;

    qDebug().noquote() << "FIXTURE RX:" << QString::fromLatin1(byte.toHex(' ').toUpper());

    QList<int> allValues;
    bool foundData = false;

    const QList<QByteArray> lines = byte.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty())
            continue;

        const QByteArray searchPattern = QByteArray::fromHex("CAFDBEDD");
        if (!line.contains(searchPattern))
            continue;

        foundData = true;
        const QByteArray colon = QByteArray::fromHex("A3BA");
        const int colonPos = line.indexOf(colon);
        if (colonPos == -1)
            continue;

        const QByteArray valuePart = line.mid(colonPos + colon.length());
        const QString dataStr = QString::fromUtf8(valuePart.trimmed());
        bool ok = false;
        const int value = dataStr.toInt(&ok);
        if (ok)
            allValues.append(value);
    }

    if (!foundData || allValues.isEmpty())
        return false;

    int maxVal = 0;
    int minVal = 99999;
    bool hasNonZeroValue = false;

    for (int value : allValues) {
        if (value <= 0)
            continue;
        hasNonZeroValue = true;
        maxVal = qMax(maxVal, value);
        minVal = qMin(minVal, value);
    }

    if (!hasNonZeroValue)
        return false;

    if (peakAmplitude)
        *peakAmplitude = maxVal - minVal + 1;
    return true;
}

} // namespace XwdAmplitudeCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
