#ifndef PLATFORM_XWD_FIXTURE_CMD_MANIFEST_H
#define PLATFORM_XWD_FIXTURE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdRawFixtureCmdManifest {

struct Row {
    XwdRawFixtureCmd cmd = XwdRawFixtureCmd::SendRaw;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind = DeviceCmdParamKind::None;
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(XwdRawFixtureCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace XwdRawFixtureCmdManifest

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // PLATFORM_XWD_FIXTURE_CMD_MANIFEST_H
