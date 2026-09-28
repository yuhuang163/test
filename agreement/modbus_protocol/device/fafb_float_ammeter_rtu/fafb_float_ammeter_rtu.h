#ifndef FAFB_FLOAT_AMMETER_RTU_H
#define FAFB_FLOAT_AMMETER_RTU_H

#include <QByteArray>
#include <QVariant>

#include "fafb_float_ammeter_rtu_types.h"
#include "imodbus_rtu_device.h"
#include "qmodbus_rtu_rx_buffer.h"

/**
 * Air1 灯显独立串口万用表（非 XWD 治具），走工位万用表口。
 * 电流：发 88 AE 00 11，收 FA FB + 大端 float(A)，上报 mA。
 * 电压：Modbus RTU FC03 读 1 保持寄存器，raw/100=V（抓包例寄存器 1/4）。
 */
class FaFbFloatAmmeterRtu : public IModbusRtuDevice {
  public:
    static QByteArray buildReadMeasurementRequest();
    static QByteArray buildReadVoltageRequest(quint8 slaveAddr, quint16 registerAddress);
    /** 解析 FA FB 电流帧；valueText=mA。 */
    static bool parseCurrentResponseFrame(const QByteArray& buf, QString* valueText, int* frameOffset = nullptr);
    /** 解析 FC03 电压帧；valueText=V。 */
    static bool parseVoltageResponseFrame(const QByteArray& buf, quint8 slaveAddr, QString* valueText,
                                          int* frameOffset = nullptr);

    QByteArray buildRequest(int cmd, const QVariant& param = {}) override;
    bool parseResponse(const QByteArray& frame, QString* valueText) override;
    void resetRxState() override;
    bool feedRx(const QByteArray& chunk, QString* valueText) override;

  private:
    ModbusRtuRxBuffer rtuRxBuffer_{64};
    FaFbFloatAmmeterRtuCmd pendingCmd_ = FaFbFloatAmmeterRtuCmd::ReadMeasurement;
    quint8 pendingSlaveAddr_ = 1;
};

#endif // FAFB_FLOAT_AMMETER_RTU_H
