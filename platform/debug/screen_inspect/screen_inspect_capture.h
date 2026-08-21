#ifndef SCREEN_INSPECT_CAPTURE_H
#define SCREEN_INSPECT_CAPTURE_H

#include <QImage>
#include <QString>

/** USB 摄像头预热后拍一张静图（须在 GUI 线程调用）。 */
namespace ScreenInspectCapture {

bool grabStill(int cameraIndex, const QString& cameraName, int warmupMs, QImage* out, QString* err);

} // namespace ScreenInspectCapture

#endif // SCREEN_INSPECT_CAPTURE_H
