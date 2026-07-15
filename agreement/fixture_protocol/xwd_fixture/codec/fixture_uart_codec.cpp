#include "fixture_uart_codec.h"

#include "qdebug.h"
#include "qlog.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace FixtureUartCodec {

void writeFrame(QSerialPort* serialPort, Qlog& log, const QByteArray& dataToSend) {
    if (!serialPort || dataToSend.isEmpty())
        return;
    qDebug().noquote() << "FIXTURE TX:" << QString::fromLatin1(dataToSend.toHex(' ').toUpper());
    serialPort->write(dataToSend);
    log.save_jig_uart_log(1, dataToSend);
}

QByteArray amplitudeQueryCommand() {
    return QByteArray("Test1\r\n");
}

QByteArray encodeRawOrHexText(const QString& text, bool* parsedAsHex) {
    if (parsedAsHex)
        *parsedAsHex = false;

    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return {};

    QString compact = trimmed;
    compact.remove(QLatin1Char(' '));
    compact.remove(QLatin1Char('\t'));
    compact.remove(QLatin1Char('\r'));
    compact.remove(QLatin1Char('\n'));
    compact.remove(QLatin1Char(':'));
    compact.remove(QLatin1Char('-'));
    compact.remove(QLatin1Char(','));
    compact.replace(QStringLiteral("0x"), QString(), Qt::CaseInsensitive);

    if (!compact.isEmpty() && (compact.size() % 2) == 0) {
        bool allHex = true;
        for (const QChar c : compact) {
            const ushort u = c.unicode();
            const bool isHexDigit = (u >= '0' && u <= '9') || (u >= 'a' && u <= 'f') || (u >= 'A' && u <= 'F');
            if (!isHexDigit) {
                allHex = false;
                break;
            }
        }
        if (allHex) {
            const QByteArray bytes = QByteArray::fromHex(compact.toLatin1());
            if (bytes.size() * 2 == compact.size()) {
                if (parsedAsHex)
                    *parsedAsHex = true;
                return bytes;
            }
        }
    }
    return trimmed.toUtf8();
}

} // namespace FixtureUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
