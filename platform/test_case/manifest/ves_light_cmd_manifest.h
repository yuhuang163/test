#ifndef PLATFORM_VES_LIGHT_CMD_MANIFEST_H
#define PLATFORM_VES_LIGHT_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case_types.h"

#include <QString>

namespace VesLightCmdManifest {

struct Row {
    VesLightCmd cmd = VesLightCmd::SetBrightness;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
    const char* gateReportType = nullptr;
    const char* gateDefaultField = nullptr;
};

const Row* rows();
int rowCount();
const Row* findByCmd(VesLightCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace VesLightCmdManifest

#endif // PLATFORM_VES_LIGHT_CMD_MANIFEST_H
