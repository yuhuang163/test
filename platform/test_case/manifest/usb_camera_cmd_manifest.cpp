#include "usb_camera_cmd_manifest.h"

#include <QString>

namespace {

using UsbCameraCmdManifest::Row;

constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintScreenDeadPixel[] =
    u8"本步对屏幕拍照后在主机侧分析对比（坏点扫描）\r\n"
    u8"通道请选「治具通信」，治具协议选「USB摄像头」\r\n"
    u8"自由工站默认 GigE：Param_cameraIp（如 169.254.64.10）或设置 ScreenInspect/GigEIp\r\n"
    u8"Param_cameraSerial 可选按序列号；Param_cameraSource=usb 时改用 USB 摄像头\r\n"
    u8"Param_cameraIndex / cameraName 仅 USB 时有效；Param_warmupMs 预热毫秒（默认 450）\r\n"
    u8"Param_expectedColor 期望纯色：下拉选「蓝/绿/红/白/黑/灰」，或「不判断」；也兼容数字 0~5/-1\r\n"
    u8"Param_deadDiff 残差阈值（空则用设置页坏点残差）\r\n"
    u8"Param_deadRadiusPercent 坏点检测区域缩放比例（默认 82%）\r\n"
    u8"Param_saveCapture=1 保存拍摄图到 bin/screen_inspect/\r\n"
    u8"Param_roi 检测范围 x,y,w,h（空则用调试页划定的范围，再空则自动找屏）\r\n"
    u8"Param_reuseCircleRoi=1 沿用上一步(如红屏)识别的圆形区域，解决灰阶图无法找圆\r\n"
    u8"卡控勾「坏点数」0~8；测绿屏再勾「是否为期望纯色」范围 1~1";
constexpr const char kHintScreenDisplayAnomaly[] =
    u8"本步对屏幕拍照后与参考图做相似度对比\r\n"
    u8"通道请选「治具通信」，治具协议选「USB摄像头」\r\n"
    u8"Param_referencePath 标准参考图：双击选择后自动复制到 screen_inspect/参考图/（与测试抓拍分开，清理不删）\r\n"
    u8"自由工站默认 GigE：Param_cameraIp / ScreenInspect/GigEIp；Param_cameraSource=usb 用 USB\r\n"
    u8"Param_cameraIndex / cameraName / warmupMs 同坏点步骤\r\n"
    u8"Param_saveCapture=1 存图\r\n"
    u8"Param_roi 同坏点步骤（调试页拖拽划定后写入设置，步骤可不填）\r\n"
    u8"Param_reuseCircleRoi=1 沿用上一步识别的圆形区域，解决灰阶图找圆失败\r\n"
    u8"卡控勾「与参考图相似度」0.85~1。灰阶条纹不要勾亮度起伏";
constexpr const char kHintScreenCameraCalibration[] =
    u8"测试前摄像头位置校准：拍一张当前图，与参考图并排显示，两侧叠加同一套校准框/圆屏线\r\n"
    u8"通道请选「治具通信」，治具协议选「USB摄像头」；默认 GigE\r\n"
    u8"Param_referencePath 标准参考图（双击选择后自动存入 screen_inspect/参考图/）\r\n"
    u8"Param_cameraIp / ScreenInspect/GigEIp；Param_cameraSource=usb 用 USB\r\n"
    u8"Param_roi 检测范围（建议先在调试页拖拽划定）；Param_warmupMs 预热\r\n"
    u8"弹窗请操作员目视对照：是=位置OK继续，否=未对准失败。本步不做坏点/相似度卡控";

const Row kRows[] = {
    {UsbCameraCmd::ScreenCameraCalibration, "ScreenCameraCalibration", u8"屏幕拍照·位置校准", DeviceCmdParamKind::JsonMap,
     kHintScreenCameraCalibration, kGet, "ProtocolScreenInspectData", "ssim"},
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
