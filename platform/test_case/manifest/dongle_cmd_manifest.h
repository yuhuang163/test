#ifndef PLATFORM_DONGLE_CMD_MANIFEST_H
#define PLATFORM_DONGLE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "qat.h"
#include "test_case_types.h"

#include <QString>

namespace DongleCmdManifest {

struct Row {
    DongleCmd cmd = DongleCmd::BleScanConnect;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(DongleCmd cmd);
const Row* findByEnumName(const QString& enumName);

}  // namespace DongleCmdManifest

#endif  // PLATFORM_DONGLE_CMD_MANIFEST_H
