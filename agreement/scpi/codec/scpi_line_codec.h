#ifndef SCPI_LINE_CODEC_H
#define SCPI_LINE_CODEC_H

#include <QByteArray>
#include <QString>
#include <functional>

/** SCPI 文本行缓冲：按 \\r\\n 或 ';' 切分，与传输层（COM/VISA）无关。 */
class ScpiLineCodec {
  public:
    using LineHandler = std::function<void(const QString& line)>;

    void reset();
    void feed(const QByteArray& chunk, const LineHandler& onLine);

  private:
    void flushLine(const LineHandler& onLine);

    QString buffer_;
};

#endif // SCPI_LINE_CODEC_H
