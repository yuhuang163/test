#ifndef IMODBUS_RTU_DEVICE_H
#define IMODBUS_RTU_DEVICE_H

#include <QByteArray>
#include <QString>
#include <QVariant>

/**
 * Modbus RTU 设备抽象接口：华勤/立讯等电流表实现此接口，
 * QModbusManager 经 activeRtuDevice() + 模板 exec 多态分发（与 IScpiDevice 同构）。
 */
class IModbusRtuDevice {
  public:
    virtual ~IModbusRtuDevice() = default;

    virtual QByteArray buildRequest(int cmd, const QVariant& param = {}) = 0;
    virtual bool parseResponse(const QByteArray& frame, QString* valueText) = 0;

    /** 发送新命令前清空粘包缓冲（立讯 RTU 等需要）。 */
    virtual void resetRxState() {
    }

    /**
     * 喂入串口字节；解析出完整测量时写 valueText 并返回 true。
     * 默认：单帧即 parse（华勤）；立讯等设备可覆写粘包逻辑。
     */
    virtual bool feedRx(const QByteArray& chunk, QString* valueText) {
        return parseResponse(chunk, valueText);
    }
};

#endif // IMODBUS_RTU_DEVICE_H
