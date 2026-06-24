#include "scpi_line_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

void ScpiLineCodec::reset() {
    buffer_.clear();
}

void ScpiLineCodec::flushLine(const LineHandler& onLine) {
    const QString line = buffer_.trimmed();
    buffer_.clear();
    if (!line.isEmpty() && onLine) {
        onLine(line);
    }
}

void ScpiLineCodec::feed(const QByteArray& chunk, const LineHandler& onLine) {
    for (char c : chunk) {
        if (c == '\r' || c == '\n') {
            flushLine(onLine);
            continue;
        }
        buffer_ += QLatin1Char(c);
        if (c == ';') {
            flushLine(onLine);
        }
    }
}
