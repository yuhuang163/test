#ifndef PLATFORM_DEVICE_CMD_MANIFEST_H
#define PLATFORM_DEVICE_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "qprotocol_types.h"
#include "test_case_types.h"

#include <QString>
#include <QVariant>

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

/** 编辑器指令下拉：enumName 须落在当前协议 set/get 的 DeviceCmd case 名单内（与 build/_device_cmd_by_protocol.json 一致，勿用前缀猜测）。 */
bool matchesProductProtocol(const QString& enumName, TestCaseProductProtocol protocol);

/** 将 case ini 中的 JsonMap 参数转换为协议 set/get 可接受的 QVariant。 */
QVariant normalizeSendParam(DeviceCmd cmd, const QVariant& param);

} // namespace DeviceCmdManifest

#endif // PLATFORM_DEVICE_CMD_MANIFEST_H
