#ifndef PLATFORM_UNIVERSAL_FIXTURE_CMD_MANIFEST_H
#define PLATFORM_UNIVERSAL_FIXTURE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace UniversalFixtureCmdManifest {

struct Row {
    UniversalFixtureCmd cmd = UniversalFixtureCmd::CylinderDown;
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
const Row* findByCmd(UniversalFixtureCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace UniversalFixtureCmdManifest

#endif // PLATFORM_UNIVERSAL_FIXTURE_CMD_MANIFEST_H
