#ifndef HQ_AMMETER_RTU_H
#define HQ_AMMETER_RTU_H

#include <QByteArray>

#include "imodbus_rtu_device.h"
#include "qmodbus_rtu_codec.h"

/** 华勤 RTU 电流表指令（工站经 QModbusManager::exec 下发）。 */
enum class HqAmmeterRtuCmd {
    ReadMeasurement,
    SetBaud115200,
};

/**
 * 华勤产线 Modbus RTU 电流表（`hq_ammeter_rtu/`，厂商_型号_协议）。
 * 铭牌确认后目录改为 <品牌>_<型号>_rtu；hq 仅作产线路由过渡。
 */
class HqAmmeterModbusRtu : public IModbusRtuDevice {
  public:
    static QByteArray buildReadMeasurementRequest();
    static QByteArray buildSetBaud115200Request();
    static ModbusRtuCodec::AmmeterReading parseResponseFrame(const QByteArray& frame);

    // --- IModbusRtuDevice interface ---
    QByteArray buildRequest(int cmd, const QVariant& param = {}) override;
    bool parseResponse(const QByteArray& frame, QString* valueText) override;
};

#endif // HQ_AMMETER_RTU_H
