#ifndef QMODBUS_RTU_RX_BUFFER_H
#define QMODBUS_RTU_RX_BUFFER_H

#include <QByteArray>

/** RTU 串口粘包缓冲（通用 codec，不含设备路由）。 */
class ModbusRtuRxBuffer {
  public:
    explicit ModbusRtuRxBuffer(int maxBufferedLen = 15);

    void reset();
    void feed(const QByteArray& chunk);
    const QByteArray& buffer() const;

  private:
    int maxLen_ = 15;
    QByteArray buffer_;
};

#endif // QMODBUS_RTU_RX_BUFFER_H
