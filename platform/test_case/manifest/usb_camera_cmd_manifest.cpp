#include "usb_camera_cmd_manifest.h"

#include <QString>

namespace {

using UsbCameraCmdManifest::Row;

constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintScreenDeadPixel[] =
    u8"本步对屏幕拍照后在主机侧分析对比（坏点扫描）\r\n"
    u8"通道选「治具」，目标设备选「USB摄像头」或「GigE工业摄像头」（指令共用）\r\n"
    u8"GigE：Param_cameraIp（如 169.254.64.10）或本工站 flow.ini [ScreenInspect] GigEIp；可选 cameraSerial\r\n"
    u8"USB：Param_cameraIndex / cameraName\r\n"
    u8"Param_warmupMs 预热毫秒（默认 450）\r\n"
    u8"Param_expectedColor 期望纯色：下拉选「蓝/绿/红/白/黑/灰」，或「不判断」；也兼容数字 0~5/-1\r\n"
    u8"Param_deadDiff 残差阈值（空则用设置页坏点残差）\r\n"
    u8"Param_deadRadiusPercent 坏点检测区域缩放比例（默认 82%）\r\n"
    u8"Param_saveCapture=1 保存拍摄图到 bin/screen_inspect/\r\n"
    u8"Param_roi 检测范围 x,y,w,h（空则用调试页划定的范围，再空则自动找屏）\r\n"
    u8"Param_reuseCircleRoi=1 沿用上一步(如红屏)识别的圆形区域，解决灰阶图无法找圆\r\n"
    u8"卡控勾「坏点数」0~8；测绿屏再勾「是否为期望纯色」范围 1~1";
constexpr const char kHintScreenDisplayAnomaly[] =
    u8"本步对屏幕拍照后与参考图做相似度对比\r\n"
    u8"通道选「治具」，目标设备选「USB摄像头」或「GigE工业摄像头」\r\n"
    u8"Param_referencePath 标准参考图：双击选择后自动复制到 screen_inspect/参考图/\r\n"
    u8"GigE：cameraIp / flow.ini GigEIp；USB：cameraIndex / cameraName；warmupMs 同坏点步骤\r\n"
    u8"Param_saveCapture=1 存图；Param_roi / reuseCircleRoi 同坏点步骤\r\n"
    u8"卡控勾「与参考图相似度」0.85~1。灰阶条纹不要勾亮度起伏";
constexpr const char kHintScreenCameraCalibration[] =
    u8"测试前摄像头位置校准：拍一张当前图，与参考图并排显示\r\n"
    u8"通道选「治具」，目标设备选「USB摄像头」或「GigE工业摄像头」\r\n"
    u8"Param_referencePath 标准参考图；GigE 填 cameraIp，USB 填 cameraIndex/cameraName\r\n"
    u8"Param_roi 检测范围（建议先在调试页拖拽划定）；Param_warmupMs 预热\r\n"
    u8"弹窗请操作员目视对照：是=位置OK继续，否=未对准失败。本步不做坏点/相似度卡控";
constexpr const char kHintLedOnOffCheck[] =
    u8"离散 LED：调试页划 Roi1~N（写入当前工站 flow.ini [LedInspect]）；同一步 1~4 灯，每灯单独卡控\r\n"
    u8"目标设备选 USB 或 GigE 摄像头（采集参数随设备）\r\n"
    u8"Param_mode1~mode4=on常亮 / blink闪烁（也可用 modes=on,blink,on,on）\r\n"
    u8"有任一只灯为 blink 则连拍；常亮灯要求窗口内全程亮，闪烁灯要求既亮又灭\r\n"
    u8"Param_ledCount / sampleDurationMs / sampleIntervalMs / minLuma / cameraIp 等";

const Row kRows[] = {
    {UsbCameraCmd::ScreenCameraCalibration, "ScreenCameraCalibration", u8"屏幕拍照·位置校准", DeviceCmdParamKind::JsonMap,
     kHintScreenCameraCalibration, kGet, "ProtocolScreenInspectData", "ssim"},
    {UsbCameraCmd::ScreenDeadPixelCheck, "ScreenDeadPixelCheck", u8"屏幕拍照·坏点分析", DeviceCmdParamKind::JsonMap,
     kHintScreenDeadPixel, kGet, "ProtocolScreenInspectData", "deadPixels"},
    {UsbCameraCmd::ScreenDisplayAnomalyCheck, "ScreenDisplayAnomalyCheck", u8"屏幕拍照·显示对比",
     DeviceCmdParamKind::JsonMap, kHintScreenDisplayAnomaly, kGet, "ProtocolScreenInspectData", "ssim"},
    {UsbCameraCmd::LedOnOffCheck, "LedOnOffCheck", u8"LED拍照·亮灭检测", DeviceCmdParamKind::JsonMap,
     kHintLedOnOffCheck, kGet, "ProtocolLedInspectData", "allOn"},
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
        if (trimmed.compare(QString::fromLatin1(row.enumName), Qt::CaseInsensitive) == 0)
            return &row;
    }
    return nullptr;
}

} // namespace UsbCameraCmdManifest
