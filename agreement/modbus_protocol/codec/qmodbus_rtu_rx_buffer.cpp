#include "qmodbus_rtu_rx_buffer.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

ModbusRtuRxBuffer::ModbusRtuRxBuffer(int maxBufferedLen) : maxLen_(maxBufferedLen) {
}

void ModbusRtuRxBuffer::reset() {
    buffer_.clear();
}

void ModbusRtuRxBuffer::feed(const QByteArray& chunk) {
    buffer_.append(chunk);
    if (buffer_.size() > maxLen_) {
        buffer_.clear();
    }
}

const QByteArray& ModbusRtuRxBuffer::buffer() const {
    return buffer_;
}
