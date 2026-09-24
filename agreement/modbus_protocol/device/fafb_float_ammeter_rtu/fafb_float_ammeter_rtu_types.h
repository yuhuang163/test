#ifndef FAFB_FLOAT_AMMETER_RTU_TYPES_H
#define FAFB_FLOAT_AMMETER_RTU_TYPES_H

/** FAFB 串口万用表：电流 88 AE / FA FB float；电压 Modbus FC03 raw/100=V。 */
enum class FaFbFloatAmmeterRtuCmd {
    ReadMeasurement,       // 88 AE 00 11 -> FA FB float(A) -> report mA
    ReadVoltageCentivolt,  // FC03 hold reg, raw/100=V；Param_slaveAddr / Param_registerAddress
};
#endif // FAFB_FLOAT_AMMETER_RTU_TYPES_H
