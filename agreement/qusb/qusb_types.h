#ifndef QUSB_TYPES_H
#define QUSB_TYPES_H

/** COM 电流表链路（SCPI / Modbus RTU）；组帧分别在 scpi/、modbus/device。 */
enum class QusbProtocolRoute {
    Auto,
    Scpi,
    HqModbus,
    LxModbus,
};

enum class QusbPowerAction {
    ConfigurePowerSupply,
    ReadMeasurement,
    ReadsuctionData,
    ReadConfiguration,
    ReadProgrammablePowerVoltage,
    ReadProgrammablePowerCurrent,
    InitializeDevice,
};

enum class QusbCmd {
    PowerActionCmd,
    ProtocolConfigCmd,
    SendScpiLine,
    ConfigureProgrammablePower,
    ProgrammablePowerOutput,
    ReadProgrammablePowerVoltageCmd,
    ReadProgrammablePowerCurrentCmd,
    InitializeProgrammablePowerCmd,
};

struct QusbLinkConfig {
    QusbProtocolRoute protocol = QusbProtocolRoute::Auto;
    int luxshareMachineId = 1;
};

#endif // QUSB_TYPES_H
