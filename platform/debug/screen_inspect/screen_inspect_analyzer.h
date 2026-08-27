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
 * 坏点：圆屏内再内缩约 82%。相似度：识别到的完整圆屏外接方框整块对比（背景黑，四角可忽略）。
 */
namespace ScreenInspectAnalyzer {

struct Params {
    int deadDiff = 35;
    int expectedColor = -1;
    QRect manualRoi; // 有效则只在此矩形内判定；空则自动找 ROI
    bool enableDeadPixels = true; // 步骤未启用坏点卡控时可跳过扫描
    bool enableSsim = true;       // 步骤未启用相似度卡控时可跳过 SSIM
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

/**
 * 仅画检测范围框 + 圆屏轮廓（无坏点红圈），供摄像头位置校准对照。
 * roi 无效时自动找屏；circleFrom 非空时圆轮廓按该图尺寸映射到 rgb（左右同框对照）。
 * outRoi 可选，回写实际使用的检测框。
 */
QImage drawGuides(const QImage& rgb, const QRect& roi, const QImage* circleFrom = nullptr,
                  QRect* outRoi = nullptr);

/**
 * 清理 screen_inspect 历史抓拍，避免目录无限累积。
 * 保留 reference* / last_*；带时间戳的 capture/mark/reference 只留最近 keepNewest 份
 *（默认 48，覆盖一次红蓝绿黑白灰+灰阶各拍 2~3 张），且超过 maxAgeDays 的一律删。
 */
void cleanupStoredImages(const QString& dirPath, int keepNewest = 48, int maxAgeDays = 1);

/** 解析期望/卡控颜色：支持 -1/0~5、不判断/自动、蓝绿红白黑灰（及英文）。失败返回 -1。 */
inline int parseColorIndex(const QString& text, bool* ok = nullptr) {
    const QString t = text.trimmed();
    if (t.isEmpty() || t.startsWith(QStringLiteral("不判断")) || t == QStringLiteral("自动")
        || t == QStringLiteral("未识别") || t == QStringLiteral("未指定")
        || t.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0 || t == QLatin1String("-1")) {
        if (ok)
            *ok = true;
        return -1;
    }
    auto hit = [&](const QString& cn, const char* en) -> bool {
        if (t == cn || t.compare(QLatin1String(en), Qt::CaseInsensitive) == 0) {
            if (ok)
                *ok = true;
            return true;
        }
        return false;
    };
    if (hit(QStringLiteral("蓝"), "blue"))
        return 0;
    if (hit(QStringLiteral("绿"), "green"))
        return 1;
    if (hit(QStringLiteral("红"), "red"))
        return 2;
    if (hit(QStringLiteral("白"), "white"))
        return 3;
    if (hit(QStringLiteral("黑"), "black"))
        return 4;
    if (hit(QStringLiteral("灰"), "gray") || hit(QStringLiteral("灰"), "grey"))
        return 5;
    bool numOk = false;
    const int v = t.toInt(&numOk);
    if (numOk && v >= -1 && v <= 5) {
        if (ok)
            *ok = true;
        return v;
    }
    if (ok)
        *ok = false;
    return -1;
}

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
