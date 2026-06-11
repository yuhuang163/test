#ifndef IMODBUS_RTU_DEVICE_H
#define IMODBUS_RTU_DEVICE_H

#include <QByteArray>
#include <QString>
#include <QVariant>

/**
 * Modbus RTU 设备抽象接口：所有 RTU 电流表（华勤、立讯等）实现此接口，
 * 使 QModbusManager 通过多态分发，无需为每个设备新增 exec 重载（OCP）。
 *
 * buildRequest: 根据 cmd 枚举（int 化）组帧。
 * parseResponse: 解析回包，返回测量文本。
 */
class IModbusRtuDevice {
  public:
    virtual ~IModbusRtuDevice() = default;

    /** 根据命令枚举（int 化）和参数构建发送帧。 */
    virtual QByteArray buildRequest(int cmd, const QVariant& param = {}) = 0;

    /** 解析设备回包帧，成功时将测量值写入 valueText 并返回 true。 */
    virtual bool parseResponse(const QByteArray& frame, QString* valueText) = 0;
};

#endif // IMODBUS_RTU_DEVICE_H
