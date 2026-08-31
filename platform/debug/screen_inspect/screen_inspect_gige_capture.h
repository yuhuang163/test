#ifndef SCREEN_INSPECT_GIGE_CAPTURE_H
#define SCREEN_INSPECT_GIGE_CAPTURE_H
#include <QImage>
#include <QString>

/** 海康 MVS / GigE Vision 静图采集（自由工站屏幕检测用）。 */
namespace ScreenInspectGigECapture {

/**
 * 枚举 GigE 设备后按 IP 或序列号打开并抓一帧，转成 RGB888。
 * @param cameraIp 例如 169.254.64.10；空则取枚举到的第一台 GigE
 * @param serial 可选，优先于 IP 匹配（机身序列号）
 * @param warmupMs 开始取流后丢弃/等待毫秒，稳定曝光
 * @param stageLog 可选，写入各阶段耗时（毫秒）便于排查卡顿
 * @param keepSession true：抓完后保持取流（工站多色连续拍）；false：抓完关闭（调试页单次拍）
 */
bool grabStill(const QString& cameraIp, const QString& serial, int warmupMs, QImage* out, QString* err,
               QString* stageLog = nullptr, bool keepSession = false);

/** 关闭复用中的 GigE 会话（测试结束 / 停止测试时调用）。 */
void releaseSession();

} // namespace ScreenInspectGigECapture
#endif // SCREEN_INSPECT_GIGE_CAPTURE_H
