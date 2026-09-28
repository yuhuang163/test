#ifndef LED_INSPECT_ANALYZER_H
#define LED_INSPECT_ANALYZER_H
#include <QImage>
#include <QMetaType>
#include <QRect>
#include <QString>
#include <QStringList>
#include <QVector>

/**
 * 离散 LED 检测（与屏幕 ScreenInspect 独立）。
 * 灯数 1~4；灯框存本工站 flow.ini [LedInspect] Roi1~N（调试页按当前工站读写）。
 * 每灯可单独配 on（常亮）或 blink（闪烁）。
 */

namespace LedInspectAnalyzer {
constexpr int kMaxLedCount = 4;
inline QRect parseRoi(const QString& text) {
    const QStringList parts = text.split(QLatin1Char(','));
    if (parts.size() != 4)
        return QRect();
    bool okx = false, oky = false, okw = false, okh = false;
    const QRect r(parts.at(0).trimmed().toInt(&okx), parts.at(1).trimmed().toInt(&oky),
                  parts.at(2).trimmed().toInt(&okw), parts.at(3).trimmed().toInt(&okh));
    if (!okx || !oky || !okw || !okh || r.width() < 4 || r.height() < 4)
        return QRect();
    return r;
}
inline QString formatRoi(const QRect& r) {
    if (r.width() < 4 || r.height() < 4)
        return QString();
    return QStringLiteral("%1,%2,%3,%4").arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
}
inline QString settingsRoiKey(int index1Based) {
    return QStringLiteral("LedInspect/Roi%1").arg(index1Based);
}
inline int clampLedCount(int n) {
    if (n < 1)
        return 1;
    if (n > kMaxLedCount)
        return kMaxLedCount;
    return n;
}
inline bool isBlinkMode(const QString& mode) {
    const QString m = mode.trimmed().toLower();
    return m == QLatin1String("blink") || m == QLatin1String("flash") || m == QStringLiteral("闪烁");
}

/** 规范化为 on / blink */
inline QString normalizeLedMode(const QString& mode) {
    return isBlinkMode(mode) ? QStringLiteral("blink") : QStringLiteral("on");
}

struct Params {
    int ledCount = 4;
    QVector<QRect> rois;
    double minLuma = 80;

    /** 每灯模式，长度>=ledCount；缺省填 on */
    QVector<QString> modes;
};

struct LedResult {
    QRect roi;
    double luma = 0;
    bool on = false; // 结案：该灯卡控是否通过
    bool valid = false;
    bool blinkMode = false;
    int onFrames = 0;
    int offFrames = 0;
};

struct Report {
    QVector<LedResult> leds;
    int onCount = 0;
    int configuredCount = 0;
    int ledCount = 0;
    bool allConfiguredOn = false;
    bool needMultiFrame = false;
    int frameCount = 0;
    QImage annotated;
};
Report analyze(const QImage& rgb, const Params& p);
void accumulateFrame(QVector<LedResult>& tracks, const Report& frame);

/** 按每灯 mode 结案：blink=须亮灭都有；on=窗口内须全亮 */
Report finalizeByModes(const QVector<LedResult>& tracks, const Params& p, const QImage& lastRgb);
QImage drawAnnotated(const QImage& rgb, const QVector<LedResult>& leds);
QString storageRootDir();
void cleanupStoredImages(const QString& dirPath, int keepNewest = 30, int maxAgeDays = 1);
} // namespace LedInspectAnalyzer

struct ProtocolLedInspectData {
    int led1On = 0;
    int led2On = 0;
    int led3On = 0;
    int led4On = 0;
    int allOn = 0;
    double luma1 = 0;
    double luma2 = 0;
    double luma3 = 0;
    double luma4 = 0;
    int onCount = 0;
    int configuredCount = 0;
    int ledCount = 0;

    /** 每灯：0=常亮 1=闪烁 */
    int judgeMode1 = 0;
    int judgeMode2 = 0;
    int judgeMode3 = 0;
    int judgeMode4 = 0;
    int onFrames1 = 0;
    int offFrames1 = 0;
    int onFrames2 = 0;
    int offFrames2 = 0;
    int onFrames3 = 0;
    int offFrames3 = 0;
    int onFrames4 = 0;
    int offFrames4 = 0;
};
Q_DECLARE_METATYPE(ProtocolLedInspectData)
#endif // LED_INSPECT_ANALYZER_H
