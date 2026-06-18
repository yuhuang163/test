#ifndef LX_AMMETER_RTU_H
#define LX_AMMETER_RTU_H

#include <QByteArray>
#include <QVariant>

#include "imodbus_rtu_device.h"
#include "lx_ammeter_rtu_types.h"
#include "qmodbus_rtu_codec.h"
#include "qmodbus_rtu_rx_buffer.h"

/**
 * 立讯产线 Modbus RTU 电流表（`lx_ammeter_rtu/`，机台号 1..6 读帧）。
 * 铭牌确认后目录改为 <品牌>_<型号>_rtu；lx 仅作产线路由过渡。
 */
class LxAmmeterModbusRtu : public IModbusRtuDevice {
  public:
    void setMachineId(int machineId1Based);
    int machineId() const;

    static QByteArray buildReadMeasurementRequest(int machineId1Based);
    static ModbusRtuCodec::AmmeterReading parseResponseFrame(const QByteArray& frame);

    QByteArray buildRequest(int cmd, const QVariant& param = {}) override;
    bool parseResponse(const QByteArray& frame, QString* valueText) override;
    void resetRxState() override;
    bool feedRx(const QByteArray& chunk, QString* valueText) override;

  private:
    int machineId1Based_ = 1;
    ModbusRtuRxBuffer rtuRxBuffer_;
};

#endif // LX_AMMETER_RTU_H
