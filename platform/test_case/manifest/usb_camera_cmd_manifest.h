#ifndef PLATFORM_USB_CAMERA_CMD_MANIFEST_H
#define PLATFORM_USB_CAMERA_CMD_MANIFEST_H

#include "cmd_manifest_common.h"
#include "test_case_types.h"

#include <QString>

namespace UsbCameraCmdManifest {

struct Row {
    UsbCameraCmd cmd = UsbCameraCmd::ScreenDeadPixelCheck;
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
const Row* findByCmd(UsbCameraCmd cmd);
const Row* findByEnumName(const QString& enumName);

} // namespace UsbCameraCmdManifest

#endif // PLATFORM_USB_CAMERA_CMD_MANIFEST_H
