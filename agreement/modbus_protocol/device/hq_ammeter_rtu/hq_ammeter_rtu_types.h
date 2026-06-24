#ifndef HQ_AMMETER_RTU_TYPES_H
#define HQ_AMMETER_RTU_TYPES_H

/** 华勤 RTU 电流表指令（工站经 QModbusManager::exec 下发）。 */
enum class HqAmmeterRtuCmd {
    ReadMeasurement,
    SetBaud115200,
};

#endif // HQ_AMMETER_RTU_TYPES_H
