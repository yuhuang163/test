#ifndef MODBUS_TYPES_H
#define MODBUS_TYPES_H

/**
 * Modbus 域 access：工站可见设备列表（ModbusDeviceRoute）。
 * Tcp/Rtu、组帧等由 device/ 与 manager 内部处理；各设备 Cmd 在对应 device 头文件。
 */

/** 工站/自由工站测试项配置：只选设备，再选该设备下的 Cmd。 */
enum class ModbusDeviceRoute {
    None,
    InovanceH5uTcp, // device/inovance_h5u_tcp → PlcCmd（汇川，PLC/*）
    GcSeriesTcp,    // device/gc_series_tcp → GcPlcCmd（GC 系列，GC_PLC/*，M 偏移默认 4096）
    HqAmmeterRtu,   // device/hq_ammeter_rtu → HqAmmeterRtuCmd
    LxAmmeterRtu,   // device/lx_ammeter_rtu → LxAmmeterRtuCmd
};

#endif // MODBUS_TYPES_H
