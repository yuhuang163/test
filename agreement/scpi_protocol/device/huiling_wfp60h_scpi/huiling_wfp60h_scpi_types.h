#ifndef HUILING_WFP60H_SCPI_TYPES_H
#define HUILING_WFP60H_SCPI_TYPES_H

/** 会凌 WFP60H SCPI 指令（工站经 QScpiManager::exec 下发）。 */
enum class HuilingScpiCmd {
    ConfigureMeasure,
    ReadMeasureCurrent,
    ReadMeasureConfiguration,
    InitializeDevice,
    ConfigureProgrammablePower,
    ProgrammablePowerOutput,
    ReadProgrammableVoltage,
    ReadProgrammableCurrent,
    InitializeProgrammablePower,
    SendRawLine,
};

#endif // HUILING_WFP60H_SCPI_TYPES_H
