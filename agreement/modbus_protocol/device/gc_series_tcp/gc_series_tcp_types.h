#ifndef GC_SERIES_TCP_TYPES_H
#define GC_SERIES_TCP_TYPES_H

#include <QMetaType>

/** GC 系列 PLC（Modbus TCP）指令，与汇川 PlcCmd 独立，避免互相干扰。 */
enum class GcPlcCmd {
    Connect,
    Disconnect,
    IsConnected,
    WriteCoil,
};

struct GcPlcCoilRequest {
    int m = 0;
    bool value = false;
};

Q_DECLARE_METATYPE(GcPlcCoilRequest)

void registerGcPlcCmdMetaTypes();

#endif // GC_SERIES_TCP_TYPES_H
