#ifndef PLATFORM_TUPLE_CMD_MANIFEST_H
#define PLATFORM_TUPLE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "qtupleservice.h"
#include "test_case_types.h"

#include <QString>

namespace TupleCmdManifest {

struct Row {
    TupleCmd cmd = TupleCmd::Login;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(TupleCmd cmd);
const Row* findByEnumName(const QString& enumName);

}  // namespace TupleCmdManifest

#endif  // PLATFORM_TUPLE_CMD_MANIFEST_H
