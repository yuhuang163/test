#ifndef MULTI_TEMP_LOGGER_RTU_H
#define MULTI_TEMP_LOGGER_RTU_H

#include <QByteArray>
#include <QVariant>

#include "imodbus_rtu_device.h"
#include "multi_temp_logger_rtu_types.h"

/**
 * 多路温度记录仪（8~64 路）Modbus RTU。
 * 文档：docs/开发参考资料/多路温度记录仪Modbus通讯协议A版.pdf
 * 功能码仅 0x03 读保持寄存器、0x06 写单寄存器；波特率默认 115200。
 */
class MultiTempLoggerModbusRtu : public IModbusRtuDevice {
  public:
    static QByteArray buildReadChannelTempRequest(int slaveAddr, int channel1Based);
    static QByteArray buildSendRawRequest(const QVariant& param);
    /** 解析读保持寄存器回包中的 float（寄存器低字+高字 → IEEE754 大端）。 */
    static bool parseTemperatureFrame(const QByteArray& frame, double* outCelsius, QString* valueText);

    QByteArray buildRequest(int cmd, const QVariant& param = {}) override;
    bool parseResponse(const QByteArray& frame, QString* valueText) override;
};

#endif // MULTI_TEMP_LOGGER_RTU_H
