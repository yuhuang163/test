#ifndef LX_AMMETER_RTU_TYPES_H
#define LX_AMMETER_RTU_TYPES_H

/** 立讯 RTU 电流表指令（工站经 QModbusManager::exec 下发）。 */
enum class LxAmmeterRtuCmd {
    ReadMeasurement,
};

#endif // LX_AMMETER_RTU_TYPES_H
