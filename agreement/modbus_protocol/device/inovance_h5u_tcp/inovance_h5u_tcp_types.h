#ifndef INOVANCE_H5U_TCP_TYPES_H
#define INOVANCE_H5U_TCP_TYPES_H

#include <QMetaType>
#include <QVariant>
#include <QString>

/** 汇川 H5U PLC 指令（工站经 QModbusManager::exec 下发）。 */
enum class PlcCmd {
    Connect,
    Disconnect,
    IsConnected,
    ReadCoil,
    WriteCoil,
    ReadCoils,
    WaitCoilTrue,
    WaitCoilFalse,
    SendStepDone,
};

struct PlcCoilRequest {
    int m = 0;
    bool value = false;
};

struct PlcWaitCoilRequest {
    int m = 0;
    int timeoutMs = 8000;
    int pollMs = 50;
};

struct PlcReadCoilsRequest {
    int m = 0;
    int quantity = 1;
};

Q_DECLARE_METATYPE(PlcCoilRequest)
Q_DECLARE_METATYPE(PlcWaitCoilRequest)
Q_DECLARE_METATYPE(PlcReadCoilsRequest)

void registerPlcCmdMetaTypes();

#endif // INOVANCE_H5U_TCP_TYPES_H
