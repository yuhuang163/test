#ifndef SCREEN_INSPECT_ANALYZER_H
#define SCREEN_INSPECT_ANALYZER_H

#include <QImage>
#include <QMetaType>
#include <QRect>
#include <QString>
#include <QStringList>

/**
 * 屏幕检测算法（调试页与自由工站步骤共用）。
 * expectedColor：-1 自动；0蓝 1绿 2红 3白 4黑 5灰。
 */
namespace ScreenInspectAnalyzer {

struct Params {
    int deadDiff = 35;
    int expectedColor = -1;
    QRect manualRoi; // 有效则只在此矩形内判定；空则自动找 ROI
};

inline QRect parseManualRoi(const QString& text) {
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != 4)
        return QRect();
    bool okx = false, oky = false, okw = false, okh = false;
    const QRect r(parts.at(0).trimmed().toInt(&okx), parts.at(1).trimmed().toInt(&oky),
                  parts.at(2).trimmed().toInt(&okw), parts.at(3).trimmed().toInt(&okh));
    if (!okx || !oky || !okw || !okh || r.width() < 8 || r.height() < 8)
        return QRect();
    return r;
}

inline QString formatManualRoi(const QRect& r) {
    if (r.width() < 8 || r.height() < 8)
        return QString();
    return QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}

struct Report {
    double ssim = -1.0;
    int deadPixels = 0;
    double muraStd = 0.0;
    int expectedColorUsed = -1;
    int detectedColor = -1;
    int colorMatch = -1; // -1未指定期望色；1匹配；0不匹配
    QRect roi;
    QImage annotated;
};

Report analyze(const QImage& currRgb, const QImage& refRgb, const Params& p);
QString colorName(int colorIndex);

} // namespace ScreenInspectAnalyzer

/**
 * 步骤 Gate 用的屏幕检测结果（USB 摄像头 + 主机分析）。
 * 不是工厂/产品协议帧；ReportType 仍写 ProtocolScreenInspectData，兼容已保存步骤 ini。
 */
struct ProtocolScreenInspectData {
    int deadPixels = 0;
    double muraStd = 0.0;
    double ssim = -1.0; // 无参考图为 -1
    int detectedColor = -1;
    int colorMatch = -1;
};

Q_DECLARE_METATYPE(ProtocolScreenInspectData)

#endif // SCREEN_INSPECT_ANALYZER_H
