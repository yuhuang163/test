#ifndef SCREEN_INSPECT_ANALYZER_H
#define SCREEN_INSPECT_ANALYZER_H

#include <QImage>
#include <QRect>
#include <QString>

/**
 * 屏幕检测算法（调试页与自由工站步骤共用）。
 * expectedColor：-1 自动；0蓝 1绿 2红 3白 4黑（与 DeviceCmd::ScreenColor 一致）。
 */
namespace ScreenInspectAnalyzer {

struct Params {
    int deadDiff = 35;
    int expectedColor = -1;
};

struct Report {
    double ssim = -1.0;
    int deadPixels = 0;
    double muraStd = 0.0;
    int expectedColorUsed = -1;
    QRect roi;
    QImage annotated;
};

Report analyze(const QImage& currRgb, const QImage& refRgb, const Params& p);

} // namespace ScreenInspectAnalyzer

#endif // SCREEN_INSPECT_ANALYZER_H
