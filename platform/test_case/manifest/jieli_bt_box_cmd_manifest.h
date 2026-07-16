#ifndef PLATFORM_JIELI_BT_BOX_CMD_MANIFEST_H
#define PLATFORM_JIELI_BT_BOX_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace JieliBtBoxCmdManifest {

struct Row {
    JieliBtBoxCmd cmd = JieliBtBoxCmd::WaitRfInfo;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionGet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(JieliBtBoxCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace JieliBtBoxCmdManifest

#endif // PLATFORM_JIELI_BT_BOX_CMD_MANIFEST_H
