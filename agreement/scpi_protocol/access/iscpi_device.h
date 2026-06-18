#ifndef ISCPI_DEVICE_H
#define ISCPI_DEVICE_H

#include <QVariant>
#include <QString>

/**
 * SCPI 设备抽象接口：所有 SCPI 设备（会凌 WFP60H、罗德 CMW100 等）实现此接口，
 * 使 QScpiManager 通过多态分发，无需为每个设备新增 exec 重载（OCP）。
 *
 * set / get 分离是因为 SCPI 协议天然区分写命令（*RST, CONF:...）和查询命令（MEAS:CURR?），
 * 且现有设备类已按此拆分。
 */
class IScpiDevice {
  public:
    virtual ~IScpiDevice() = default;

    /** 下发写命令（Set），cmd 为具体设备 enum 的 int 值。 */
    virtual bool set(int cmd, const QVariant& param) = 0;

    /** 下发查询命令（Get），cmd 为具体设备 enum 的 int 值。 */
    virtual bool get(int cmd, const QVariant& param) = 0;

    /** 判断 cmd 是否为查询类命令（Manager 需据此选择调用 set 还是 get）。 */
    virtual bool isQueryCmd(int cmd) const = 0;

    /** 设备最后一次查询的文本应答（如有）。 */
    virtual QString lastQueryResponse() const {
        return {};
    }
};

#endif // ISCPI_DEVICE_H
