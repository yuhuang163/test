#ifndef QCHANNEL_H
#define QCHANNEL_H

#include <QByteArray>

class QSerialPort;

/** 传输层抽象：只负责字节收发，不含协议组帧 */
class IChannel {
public:
    virtual ~IChannel() = default;

    virtual bool isOpen() const = 0;
    virtual qint64 write(const QByteArray& data) = 0;
};

/** 串口写入口（协议层统一调用，避免各处直接 serialPort->write） */
namespace QSerialChannel {

bool isPortOpen(const QSerialPort* port);
qint64 write(QSerialPort* port, const QByteArray& data);

}  // namespace QSerialChannel

#endif  // QCHANNEL_H
