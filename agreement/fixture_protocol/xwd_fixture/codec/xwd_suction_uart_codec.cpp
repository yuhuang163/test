#include "xwd_suction_uart_codec.h"

#include "fixture_uart_codec.h"

#include <QRegularExpression>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdSuctionUartCodec {

QByteArray encodeRawText(const QString& text, bool* parsedAsHex) {
    // 纯十六进制（如 11 11 22）→ hex 字节；含非 hex 字母（如 readonce）→ UTF-8 原文
    return FixtureUartCodec::encodeRawOrHexText(text, parsedAsHex);
}

bool parseReadOnceReply(const QString& text, double* ch1Ma, double* ch2Ma, bool* hasCh1, bool* hasCh2) {
    if (ch1Ma)
        *ch1Ma = 0;
    if (ch2Ma)
        *ch2Ma = 0;
    if (hasCh1)
        *hasCh1 = false;
    if (hasCh2)
        *hasCh2 = false;

    static const QRegularExpression re(
        QStringLiteral(R"(CH\s*([12])\s*:\s*([-+]?\d+(?:\.\d+)?)\s*V\s*([-+]?\d+(?:\.\d+)?)\s*mA)"),
        QRegularExpression::CaseInsensitiveOption);

    bool any = false;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        bool ok = false;
        const int ch = m.captured(1).toInt(&ok);
        if (!ok)
            continue;
        const double ma = m.captured(3).toDouble(&ok);
        if (!ok)
            continue;
        if (ch == 1) {
            if (ch1Ma)
                *ch1Ma = ma;
            if (hasCh1)
                *hasCh1 = true;
        } else if (ch == 2) {
            if (ch2Ma)
                *ch2Ma = ma;
            if (hasCh2)
                *hasCh2 = true;
        }
        any = true;
    }
    return any;
}

} // namespace XwdSuctionUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
