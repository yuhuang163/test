#ifndef PLC_V3_FACADE_H
#define PLC_V3_FACADE_H

#include "plc_v3_fixture.h"

class QModbusManager;

/**
 * V3 治具 PLC 工站门面（business/，与 CmwGprfFacade 同级）。
 * 自由工站：plcFacade_.setModbusManager(&modbusManager) 后 run()。
 */
class PlcV3Facade {
  public:
    void setModbusManager(QModbusManager* modbus);
    QModbusManager* modbusManager() const;

    PlcV3RunResult run(PlcV3Command command, const PlcV3RunParams& params);
    void disconnect();

  private:
    QModbusManager* modbus_ = nullptr;
    PlcV3Fixture fixture_;
};

#endif // PLC_V3_FACADE_H
