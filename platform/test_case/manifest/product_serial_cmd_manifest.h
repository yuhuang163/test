#ifndef PLATFORM_PRODUCT_SERIAL_CMD_MANIFEST_H
#define PLATFORM_PRODUCT_SERIAL_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case.h"
#include "test_case_types.h"

#include <QString>

namespace ProductSerialCmdManifest {

struct Row {
    ProductSerialCmd cmd = ProductSerialCmd::InstrumentReset;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    const char* paramHint = nullptr;
    uint8_t sendActions = TestCaseCmdManifest::kSendActionSet;
};

const Row* rows();
int rowCount();
const Row* findByCmd(ProductSerialCmd cmd);
const Row* findByEnumName(const QString& enumName);

}  // namespace ProductSerialCmdManifest

#endif  // PLATFORM_PRODUCT_SERIAL_CMD_MANIFEST_H
