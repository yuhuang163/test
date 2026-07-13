#ifndef FIXTURE_UART_CODEC_H
#define FIXTURE_UART_CODEC_H

#include <QByteArray>

class QSerialPort;
class Qlog;

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 治具串口物理层写帧（日志 + write）。 */
namespace FixtureUartCodec {

void writeFrame(QSerialPort* serialPort, Qlog& log, const QByteArray& dataToSend);
QByteArray amplitudeQueryCommand();

} // namespace FixtureUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_UART_CODEC_H
