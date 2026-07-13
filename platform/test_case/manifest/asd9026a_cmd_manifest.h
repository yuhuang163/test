#ifndef PLATFORM_ASD9026A_CMD_MANIFEST_H
#define PLATFORM_ASD9026A_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace Asd9026aCmdManifest {

struct Row {
    Asd9026aCmd cmd = Asd9026aCmd::ConfigureProgrammablePower;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(Asd9026aCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace Asd9026aCmdManifest

#endif // PLATFORM_ASD9026A_CMD_MANIFEST_H
