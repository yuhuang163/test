#include "plc_v3_facade.h"

#include "qmodbusmanager.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

void PlcV3Facade::setModbusManager(QModbusManager* modbus) {
    modbus_ = modbus;
}

QModbusManager* PlcV3Facade::modbusManager() const {
    return modbus_;
}

PlcV3RunResult PlcV3Facade::run(PlcV3Command command, const PlcV3RunParams& params) {
    PlcV3RunParams runParams = params;
    if (!runParams.modbus) {
        runParams.modbus = modbus_;
    }
    if (command == PlcV3Command::Disconnect) {
        if (runParams.modbus) {
            runParams.modbus->disconnectPlc();
        }
        PlcV3RunResult ok;
        ok.ok = true;
        return ok;
    }
    return fixture_.run(command, runParams);
}

void PlcV3Facade::disconnect() {
    if (modbus_) {
        modbus_->disconnectPlc();
    }
}
