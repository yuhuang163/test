#ifndef LX_AMMETER_RTU_H
#define LX_AMMETER_RTU_H

#include <QByteArray>

#include "qmodbus_rtu_codec.h"

/** 立讯 RTU 电流表指令（工站经 QModbusManager::exec 下发）。 */
enum class LxAmmeterRtuCmd {
    ReadMeasurement,
};

/**
 * 立讯产线 Modbus RTU 电流表（`lx_ammeter_rtu/`，机台号 1..6 读帧）。
 * 铭牌确认后目录改为 <品牌>_<型号>_rtu；lx 仅作产线路由过渡。
 */
class LxAmmeterModbusRtu {
  public:
    static QByteArray buildReadMeasurementRequest(int machineId1Based);
    static ModbusRtuCodec::AmmeterReading parseResponse(const QByteArray& frame);
};

#endif // LX_AMMETER_RTU_H
