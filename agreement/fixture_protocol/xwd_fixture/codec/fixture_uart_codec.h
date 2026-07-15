#ifndef FIXTURE_UART_CODEC_H
#define FIXTURE_UART_CODEC_H

#include <QByteArray>
#include <QString>

class QSerialPort;
class Qlog;

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 治具串口物理层写帧（日志 + write）。 */
namespace FixtureUartCodec {

void writeFrame(QSerialPort* serialPort, Qlog& log, const QByteArray& dataToSend);
QByteArray amplitudeQueryCommand();

/**
 * 原始下发内容解析：
 * - 形如 "11 11 22" / "111122" / "11:11:22" → 十六进制字节（parsedAsHex=true）
 * - 否则按 UTF-8 原文（parsedAsHex=false，如 readonce）
 */
QByteArray encodeRawOrHexText(const QString& text, bool* parsedAsHex = nullptr);

} // namespace FixtureUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_UART_CODEC_H
