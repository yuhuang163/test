#ifndef PLATFORM_XWD_BLE_FIXTURE_CMD_MANIFEST_H
#define PLATFORM_XWD_BLE_FIXTURE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace XwdBleFixtureCmdManifest {

struct Row {
    XwdBleFixtureCmd cmd = XwdBleFixtureCmd::SendRaw;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(XwdBleFixtureCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace XwdBleFixtureCmdManifest

#endif // PLATFORM_XWD_BLE_FIXTURE_CMD_MANIFEST_H
