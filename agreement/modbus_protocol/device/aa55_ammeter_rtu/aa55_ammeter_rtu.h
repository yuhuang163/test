#ifndef AA55_AMMETER_RTU_H
#define AA55_AMMETER_RTU_H

#include <QByteArray>
#include <QVariant>

#include "aa55_ammeter_rtu_types.h"
#include "imodbus_rtu_device.h"
#include "qmodbus_rtu_rx_buffer.h"

/**
 * AA55 帧格式串口电流表（`aa55_ammeter_rtu/`）。
 * 帧：AA 55 | content | 2 字节 BE 校验（仅对 content 累加）；content[0]=长度（含自身）。
 */
class Aa55AmmeterRtu : public IModbusRtuDevice {
  public:
    static QByteArray buildReadMeasurementRequest();
    static bool parseResponseFrame(const QByteArray& frame, QString* valueText);

    QByteArray buildRequest(int cmd, const QVariant& param = {}) override;
    bool parseResponse(const QByteArray& frame, QString* valueText) override;
    void resetRxState() override;
    bool feedRx(const QByteArray& chunk, QString* valueText) override;

  private:
    ModbusRtuRxBuffer rtuRxBuffer_{64};
};

#endif // AA55_AMMETER_RTU_H
