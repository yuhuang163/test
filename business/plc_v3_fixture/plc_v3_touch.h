#ifndef PLC_V3_TOUCH_H
#define PLC_V3_TOUCH_H

#include "inovance_h5u_tcp.h"

#include <functional>

struct PlcV3TouchKeyOptions {
    /** 下压稳定后、StepDone 前读电容；返回 false 则整步失败。 */
    std::function<bool(QString* errOut, QString* capSummary)> pollCapDuringPress;
};

struct PlcV3TouchResult {
    bool ok = false;
    QString summary;
};

/** V3 治具单键整步（Modbus 握手 + 可选电容读取）。 */
PlcV3TouchResult runPlcV3TouchKey(PlcModbusSession& session, int keyIndex0To6,
                                  const PlcV3TouchKeyOptions& options = {});

/** V3 治具旋钮整步（前推 + 按压 Modbus 握手）。 */
PlcV3TouchResult runPlcV3TouchSwitch(PlcModbusSession& session);

#endif // PLC_V3_TOUCH_H
