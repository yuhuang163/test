#ifndef PLATFORM_MODBUS_CMD_MANIFEST_H
#define PLATFORM_MODBUS_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "modbus_types.h"

#include <QString>

/** 各 Modbus 设备 Cmd 元数据；工站只选 ModbusDeviceRoute + 枚举名。 */
namespace ModbusCmdManifest {

struct Row {
    ModbusDeviceRoute device = ModbusDeviceRoute::None;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByDeviceAndName(ModbusDeviceRoute device, const QString& enumName);

} // namespace ModbusCmdManifest

#endif // PLATFORM_MODBUS_CMD_MANIFEST_H
