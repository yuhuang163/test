#ifndef LX_AMMETER_RTU_H
#define LX_AMMETER_RTU_H

#include <QByteArray>

#include "imodbus_rtu_device.h"
#include "qmodbus_rtu_codec.h"

/** 立讯 RTU 电流表指令（工站经 QModbusManager::exec 下发）。 */
enum class LxAmmeterRtuCmd {
    ReadMeasurement,
};

/**
 * 立讯产线 Modbus RTU 电流表（`lx_ammeter_rtu/`，机台号 1..6 读帧）。
 * 铭牌确认后目录改为 <品牌>_<型号>_rtu；lx 仅作产线路由过渡。
 */
class LxAmmeterModbusRtu : public IModbusRtuDevice {
  public:
    static QByteArray buildReadMeasurementRequest(int machineId1Based);
    static ModbusRtuCodec::AmmeterReading parseResponseFrame(const QByteArray& frame);

    // --- IModbusRtuDevice interface ---
    QByteArray buildRequest(int cmd, const QVariant& param = {}) override;
    bool parseResponse(const QByteArray& frame, QString* valueText) override;
};

#endif // LX_AMMETER_RTU_H
