#ifndef PLATFORM_AUDIO_EXPERT_CMD_MANIFEST_H
#define PLATFORM_AUDIO_EXPERT_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case_types.h"

#include <QString>

namespace AudioExpertCmdManifest {

struct Row {
    AudioExpertCmd cmd = AudioExpertCmd::RunTest;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionGet;
    const char* gateReportType = nullptr;
    const char* gateDefaultField = nullptr;
};

const Row* rows();
int rowCount();
const Row* findByCmd(AudioExpertCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace AudioExpertCmdManifest

#endif // PLATFORM_AUDIO_EXPERT_CMD_MANIFEST_H
