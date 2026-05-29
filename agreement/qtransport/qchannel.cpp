#include "qchannel.h"

#include <QSerialPort>

namespace QSerialChannel {

bool isPortOpen(const QSerialPort* port) {
    return port != nullptr && port->isOpen();
}

qint64 write(QSerialPort* port, const QByteArray& data) {
    if (!isPortOpen(port) || data.isEmpty()) {
        return -1;
    }
    const qint64 n = port->write(data);
    if (n > 0) {
        port->flush();
    }
    return n;
}

}  // namespace QSerialChannel
