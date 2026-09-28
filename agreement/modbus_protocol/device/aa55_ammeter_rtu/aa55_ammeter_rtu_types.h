#ifndef AA55_AMMETER_RTU_TYPES_H
#define AA55_AMMETER_RTU_TYPES_H

/** AA55 串口电流表指令（工站经 QModbusManager::exec 下发）。 */
enum class Aa55AmmeterRtuCmd {
    ReadMeasurement,
};
#endif // AA55_AMMETER_RTU_TYPES_H
