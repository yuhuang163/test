#ifndef XINJIE_PLC_RTU_TYPES_H
#define XINJIE_PLC_RTU_TYPES_H

#include <QMetaType>
#include <QtGlobal>
#include <QString>

/** 信捷 PLC Modbus RTU 指令（串口默认 19200 8N1，地址串如 M100/D100/X0/Y0）。 */
enum class XinjePlcCmd {
    Connect,
    Disconnect,
    IsConnected,
    WriteCoil,
    ReadCoils,
    WriteRegister,
    ReadHoldingRegisters,
    ReadDiscreteInputs,
};

struct XinjePlcCoilRequest {
    QString address;
    bool value = false;
    int quantity = 1;
};

struct XinjePlcRegisterRequest {
    QString address;
    quint16 value = 0;
    int quantity = 1;
};

Q_DECLARE_METATYPE(XinjePlcCoilRequest)
Q_DECLARE_METATYPE(XinjePlcRegisterRequest)

void registerXinjePlcCmdMetaTypes();

#endif // XINJIE_PLC_RTU_TYPES_H
