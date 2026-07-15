#ifndef PLATFORM_XWD_SUCTION_FIXTURE_CMD_MANIFEST_H
#define PLATFORM_XWD_SUCTION_FIXTURE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace XwdSuctionFixtureCmdManifest {

struct Row {
    XwdSuctionFixtureCmd cmd = XwdSuctionFixtureCmd::SendRaw;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(XwdSuctionFixtureCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace XwdSuctionFixtureCmdManifest

#endif // PLATFORM_XWD_SUCTION_FIXTURE_CMD_MANIFEST_H
