#ifndef HQ_AMMETER_RTU_H
#define HQ_AMMETER_RTU_H

#include <QByteArray>

#include "qmodbus_rtu_codec.h"

/**
 * 华勤产线 Modbus RTU 电流表（`hq_ammeter_rtu/`，厂商_型号_协议）。
 * 铭牌确认后目录改为 <品牌>_<型号>_rtu；hq 仅作产线路由过渡。
 */
class HqAmmeterModbusRtu {
  public:
    static QByteArray buildReadMeasurementRequest();
    static QByteArray buildSetBaud115200Request();
    static ModbusRtuCodec::AmmeterReading parseResponse(const QByteArray& frame);
};

#endif // HQ_AMMETER_RTU_H
