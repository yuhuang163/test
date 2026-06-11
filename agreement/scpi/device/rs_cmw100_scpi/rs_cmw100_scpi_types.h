#ifndef RS_CMW100_SCPI_TYPES_H
#define RS_CMW100_SCPI_TYPES_H

/** 罗德 CMW100 GPRF SCPI 指令（工站经 QScpiManager::exec 下发）。 */
enum class CmwScpiCmd {
    ClearStatus,
    GenOff,
    GenOn,
    ListOff,
    BbModeArb,
    ArbFile,
    ArbRepetition,
    ArbCycles,
    TxLevelDbm,
    FrequencyMhz,
    ManualArbTrigger,
    WriteLine,
    Identity,
    ArbFilePath,
    ArbScount,
    GenState,
    SystemError,
    QueryLine,
};

#endif // RS_CMW100_SCPI_TYPES_H
