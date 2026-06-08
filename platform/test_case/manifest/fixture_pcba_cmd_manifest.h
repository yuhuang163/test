#ifndef PLATFORM_FIXTURE_PCBA_CMD_MANIFEST_H
#define PLATFORM_FIXTURE_PCBA_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace FixturePcbaCmdManifest {

struct Row {
    FixturePcbaCmd cmd = FixturePcbaCmd::StartTest;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(FixturePcbaCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace FixturePcbaCmdManifest

#endif // PLATFORM_FIXTURE_PCBA_CMD_MANIFEST_H
