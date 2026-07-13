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

} // namespace FixtureUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
