#include "usb_camera_cmd_manifest.h"

#include <QString>

namespace {

using UsbCameraCmdManifest::Row;

constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintScreenDeadPixel[] =
    u8"本步调用 USB 摄像头对屏幕拍照，再在主机侧分析对比（坏点扫描）\r\n"
    u8"通道请选「治具通信」，治具协议选「USB摄像头」\r\n"
    u8"Param_cameraIndex 摄像头序号（默认 0） Param_cameraName 可选按名称匹配\r\n"
    u8"Param_warmupMs 预热毫秒（默认 450）\r\n"
    u8"Param_expectedColor 期望纯色：测绿屏填 1（0蓝1绿2红3白4黑5灰；-1不判断是不是该颜色）\r\n"
    u8"Param_deadDiff 残差阈值（空则用设置页坏点残差）\r\n"
    u8"Param_saveCapture=1 保存拍摄图到 bin/screen_inspect/\r\n"
    u8"Param_roi 检测范围 x,y,w,h（空则用调试页划定的范围，再空则自动找屏）\r\n"
    u8"卡控勾「坏点数」0~8；测绿屏再勾「是否为期望纯色」范围 1~1";
constexpr const char kHintScreenDisplayAnomaly[] =
    u8"本步调用 USB 摄像头对屏幕拍照，再与参考图做相似度对比\r\n"
    u8"通道请选「治具通信」，治具协议选「USB摄像头」\r\n"
    u8"Param_referencePath 标准参考图：双击该参数值选择 png/jpg（必填，或放 screen_inspect/reference.png）\r\n"
    u8"Param_cameraIndex / cameraName / warmupMs 同坏点步骤\r\n"
    u8"Param_saveCapture=1 存图\r\n"
    u8"Param_roi 同坏点步骤（调试页拖拽划定后写入设置，步骤可不填）\r\n"
    u8"卡控勾「与参考图相似度」0.85~1。灰阶条纹不要勾亮度起伏";

const Row kRows[] = {
    {UsbCameraCmd::ScreenDeadPixelCheck, "ScreenDeadPixelCheck", u8"屏幕拍照·坏点分析", DeviceCmdParamKind::JsonMap,
     kHintScreenDeadPixel, kGet, "ProtocolScreenInspectData", "deadPixels"},
    {UsbCameraCmd::ScreenDisplayAnomalyCheck, "ScreenDisplayAnomalyCheck", u8"屏幕拍照·显示对比",
     DeviceCmdParamKind::JsonMap, kHintScreenDisplayAnomaly, kGet, "ProtocolScreenInspectData", "ssim"},
};

} // namespace

namespace UsbCameraCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(UsbCameraCmd cmd) {
    for (const Row& row : kRows) {
        if (row.cmd == cmd)
            return &row;
    }
    return nullptr;
}

const Row* findByEnumName(const QString& enumName) {
    const QString trimmed = enumName.trimmed();
    for (const Row& row : kRows) {
        if (QString::fromLatin1(row.enumName) == trimmed)
            return &row;
    }
    return nullptr;
}

} // namespace UsbCameraCmdManifest
