#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H

/**
 * Modbus 域 access：链路级通用类型（不含具体设备组帧/SETTINGS 线圈表）。
 * 设备命令：inovance_h5u_tcp（PlcCmd）、hq/lx_ammeter_rtu（经 manager 路由）。
 */

enum class ModbusLinkKind {
    Tcp,
    Rtu,
};

/** 串口 RTU 电流表产线路由（SETTINGS，非 device 目录名）。 */
enum class ModbusRtuRoute {
    None,
    Hq,
    Lx,
};

enum class ModbusRtuAmmeterCmd {
    ReadMeasurement,
    InitializeBaud115200,
};

#endif // MODBUS_TYPES_H
