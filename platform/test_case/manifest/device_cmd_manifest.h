#ifndef PLATFORM_DEVICE_CMD_MANIFEST_H
#define PLATFORM_DEVICE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "qprotocol_types.h"
#include "test_case_types.h"

#include <QString>

/** 产测 DeviceCmd 元数据（枚举名、中文、参数、Set/Get）；设置页下拉仅据此表与操作方式过滤。 */
namespace DeviceCmdManifest {

using TestCaseCmdManifest::kSendActionBoth;
using TestCaseCmdManifest::kSendActionGet;
using TestCaseCmdManifest::kSendActionSet;

struct Row {
    DeviceCmd cmd = DeviceCmd::FacMode;
    const char* enumName = nullptr;
    const char* uiLabel = nullptr;
    DeviceCmdParamKind paramKind{};
    const char* paramHint = nullptr;
    /** kSendActionSet / kSendActionGet / kSendActionBoth */
    uint8_t sendActions = kSendActionSet;
    /** 卡控回包类型（逗号分隔）；空则编辑器列出全部类型。 */
    const char* gateReportType = nullptr;
    const char* gateDefaultField = nullptr;
};

const Row* rows();
int rowCount();
const Row* findByCmd(DeviceCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace DeviceCmdManifest

#endif // PLATFORM_DEVICE_CMD_MANIFEST_H
