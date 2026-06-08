#ifndef PLC_V3_FIXTURE_H
#define PLC_V3_FIXTURE_H

#include "inovance_h5u_modbus_tcp.h"

#include <functional>

/** V3 治具 PLC 统一入口：连接测试 / 按键 / 旋钮 / 复位。 */
enum class PlcV3Command {
    ModbusConnectTest,
    TouchKey,
    TouchSwitch,
    SwitchDoneReset,
    Disconnect,
};

struct PlcV3RunParams {
    int stationIndex = 1;
    int keyIndex0To6 = 0;
    std::function<void(const QString& line)> log;
    std::function<bool()> isTestContinue;
    /** 按键下压期间读电容（仅 TouchKey 可选）。 */
    std::function<bool(QString* errOut, QString* capSummary)> pollCapDuringPress;
};

struct PlcV3RunResult {
    bool ok = false;
    QString summary;
};

class PlcV3Fixture {
public:
    PlcV3RunResult run(PlcV3Command command, const PlcV3RunParams& params);
    void disconnect();

private:
    InovanceH5uModbusTcp tcp_;
};

#endif  // PLC_V3_FIXTURE_H
