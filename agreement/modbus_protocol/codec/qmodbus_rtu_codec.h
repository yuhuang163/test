#ifndef QMODBUS_RTU_CODEC_H
#define QMODBUS_RTU_CODEC_H

#include <QByteArray>
#include <QString>

/**
 * Modbus RTU 帧级 codec（CRC/PDU 校验用 qmodbus_pdu；粘包缓冲见 qmodbus_rtu_rx_buffer）。
 * 产线电流表寄存器语义解析供 device 调用，不含 SETTINGS/路由。
 */
namespace ModbusRtuCodec {

struct AmmeterReading {
    bool ok = false;
    QString valueText;
};

quint16 readBigEndianU16(const QByteArray& buffer, int highIndex, int lowIndex);

AmmeterReading parseHqHoldRegisterFrame(const QByteArray& frame);
AmmeterReading parseLxHoldRegisterFrame(const QByteArray& frame);

} // namespace ModbusRtuCodec

#endif // QMODBUS_RTU_CODEC_H
