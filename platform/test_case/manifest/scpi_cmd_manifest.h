#ifndef PLATFORM_SCPI_CMD_MANIFEST_H
#define PLATFORM_SCPI_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "scpi_types.h"

#include <QString>

namespace ScpiCmdManifest {

struct Row {
    ScpiDeviceRoute device = ScpiDeviceRoute::None;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByDeviceAndName(ScpiDeviceRoute device, const QString& enumName);

} // namespace ScpiCmdManifest

#endif // PLATFORM_SCPI_CMD_MANIFEST_H
