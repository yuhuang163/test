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

/** 配置里的字面量 \\r \\n \\t \\\\ → 真实控制符；未写则不加换行 */
QString unescapeParamEscapes(const QString& text) {
    QString out;
    out.reserve(text.size());
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i) == QLatin1Char('\\') && i + 1 < text.size()) {
            const QChar next = text.at(i + 1);
            if (next == QLatin1Char('r')) {
                out.append(QLatin1Char('\r'));
                ++i;
                continue;
            }
            if (next == QLatin1Char('n')) {
                out.append(QLatin1Char('\n'));
                ++i;
                continue;
            }
            if (next == QLatin1Char('t')) {
                out.append(QLatin1Char('\t'));
                ++i;
                continue;
            }
            if (next == QLatin1Char('\\')) {
                out.append(QLatin1Char('\\'));
                ++i;
                continue;
            }
        }
        out.append(text.at(i));
    }
    return out;
}

QByteArray encodeRawOrHexText(const QString& text, bool* parsedAsHex) {
    if (parsedAsHex)
        *parsedAsHex = false;

    // 先 trim 空白，再解析 \\r\\n（避免真实换行被 trim 掉后无法配置）
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
    // 原文：配置写 readonce\\r\\n 才带换行；只写 readonce 则不加
    return unescapeParamEscapes(trimmed).toUtf8();
}

} // namespace FixtureUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
