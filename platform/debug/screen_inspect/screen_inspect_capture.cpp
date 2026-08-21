#include "screen_inspect_capture.h"

#include <QCamera>
#include <QCameraImageCapture>
#include <QCameraInfo>
#include <QEventLoop>
#include <QObject>
#include <QTimer>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace ScreenInspectCapture {

bool grabStill(int cameraIndex, const QString& cameraName, int warmupMs, QImage* out, QString* err) {
    if (!out) {
        if (err)
            *err = QStringLiteral("内部错误：输出图为空");
        return false;
    }
    out->fill(Qt::black);
    const QList<QCameraInfo> cams = QCameraInfo::availableCameras();
    if (cams.isEmpty()) {
        if (err)
            *err = QStringLiteral("未找到 USB 摄像头");
        return false;
    }

    int idx = cameraIndex;
    const QString name = cameraName.trimmed();
    if (!name.isEmpty()) {
        idx = -1;
        for (int i = 0; i < cams.size(); ++i) {
            if (cams.at(i).deviceName() == name || cams.at(i).description().contains(name, Qt::CaseInsensitive)) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            if (err)
                *err = QStringLiteral("未匹配到摄像头：%1").arg(name);
            return false;
        }
    }
    if (idx < 0 || idx >= cams.size()) {
        if (err)
            *err = QStringLiteral("摄像头序号无效：%1（共 %2 台）").arg(cameraIndex).arg(cams.size());
        return false;
    }

    warmupMs = qBound(0, warmupMs, 8000);
    QCamera camera(cams.at(idx));
    QCameraImageCapture capture(&camera);
    camera.setCaptureMode(QCamera::CaptureStillImage);
    if (capture.isCaptureDestinationSupported(QCameraImageCapture::CaptureToBuffer))
        capture.setCaptureDestination(QCameraImageCapture::CaptureToBuffer);
    else
        capture.setCaptureDestination(QCameraImageCapture::CaptureToFile);

    QEventLoop loop;
    QImage grabbed;
    QString failMsg;
    bool done = false;
    auto finish = [&]() {
        if (done)
            return;
        done = true;
        loop.quit();
    };

    QObject::connect(&capture, &QCameraImageCapture::imageCaptured, &loop,
                     [&](int, const QImage& image) {
                         grabbed = image;
                         finish();
                     });
    QObject::connect(&capture,
                     static_cast<void (QCameraImageCapture::*)(int, QCameraImageCapture::Error, const QString&)>(
                         &QCameraImageCapture::error),
                     &loop, [&](int, QCameraImageCapture::Error, const QString& message) {
                         failMsg = message;
                         finish();
                     });

    bool captureArmed = false;
    auto armCapture = [&]() {
        if (captureArmed)
            return;
        captureArmed = true;
        QTimer::singleShot(warmupMs, &loop, [&]() {
            if (done)
                return;
            if (!capture.isReadyForCapture()) {
                failMsg = QStringLiteral("摄像头未就绪，无法拍照");
                finish();
                return;
            }
            capture.capture();
        });
    };

    QObject::connect(&camera, &QCamera::statusChanged, &loop, [&](QCamera::Status status) {
        if (status == QCamera::ActiveStatus)
            armCapture();
    });

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        failMsg = QStringLiteral("摄像头采集超时");
        finish();
    });
    timeout.start(warmupMs + 8000);

    camera.start();
    if (camera.status() == QCamera::ActiveStatus)
        armCapture();
    loop.exec();
    camera.stop();

    if (grabbed.isNull()) {
        if (err)
            *err = failMsg.isEmpty() ? QStringLiteral("采集图像为空") : failMsg;
        return false;
    }
    *out = grabbed.convertToFormat(QImage::Format_RGB888);
    return true;
}

} // namespace ScreenInspectCapture

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
