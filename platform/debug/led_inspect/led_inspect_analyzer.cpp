#include "led_inspect_analyzer.h"



#include <QCoreApplication>

#include <QDateTime>

#include <QDir>

#include <QFile>

#include <QFileInfo>

#include <QPainter>

#include <QtGlobal>



#if _MSC_VER >= 1600

#pragma execution_character_set("utf-8")

#endif



namespace LedInspectAnalyzer {



static double averageLuma(const QImage& rgb, const QRect& roiIn) {

    const QImage img = rgb.convertToFormat(QImage::Format_RGB32);

    const QRect roi = roiIn.intersected(img.rect());

    if (roi.width() < 1 || roi.height() < 1)

        return 0;

    qint64 sum = 0;

    qint64 n = 0;

    for (int y = roi.top(); y <= roi.bottom(); ++y) {

        const QRgb* line = reinterpret_cast<const QRgb*>(img.constScanLine(y));

        for (int x = roi.left(); x <= roi.right(); ++x) {

            const QRgb c = line[x];

            sum += (299 * qRed(c) + 587 * qGreen(c) + 114 * qBlue(c)) / 1000;

            ++n;

        }

    }

    return n > 0 ? static_cast<double>(sum) / static_cast<double>(n) : 0;

}



static QString modeAt(const Params& p, int index0) {

    if (index0 >= 0 && index0 < p.modes.size())

        return normalizeLedMode(p.modes.at(index0));

    return QStringLiteral("on");

}



QImage drawAnnotated(const QImage& rgb, const QVector<LedResult>& leds) {

    QImage out = rgb.convertToFormat(QImage::Format_RGB32);

    if (out.isNull())

        return out;

    QPainter p(&out);

    p.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < leds.size(); ++i) {

        const LedResult& L = leds.at(i);

        if (!L.valid)

            continue;

        const QRect r = L.roi.intersected(out.rect());

        if (r.width() < 2 || r.height() < 2)

            continue;

        const QColor pen = L.on ? QColor(0, 220, 80) : QColor(220, 60, 60);

        p.setPen(QPen(pen, 3));

        p.drawRect(r.adjusted(0, 0, -1, -1));

        p.setPen(QPen(Qt::white, 1));

        if (L.blinkMode) {

            p.drawText(r.topLeft() + QPoint(4, 16),

                       QStringLiteral("L%1 %2 亮%3灭%4")

                           .arg(i + 1)

                           .arg(L.on ? QStringLiteral("闪烁OK") : QStringLiteral("未闪烁"))

                           .arg(L.onFrames)

                           .arg(L.offFrames));

        } else {

            p.drawText(r.topLeft() + QPoint(4, 16),

                       QStringLiteral("L%1 %2 luma=%3 亮%4灭%5")

                           .arg(i + 1)

                           .arg(L.on ? QStringLiteral("常亮OK") : QStringLiteral("非常亮"))

                           .arg(L.luma, 0, 'f', 1)

                           .arg(L.onFrames)

                           .arg(L.offFrames));

        }

    }

    p.end();

    return out;

}



Report analyze(const QImage& rgb, const Params& p) {

    Report report;

    report.ledCount = clampLedCount(p.ledCount);

    report.frameCount = 1;

    report.leds.resize(report.ledCount);

    if (rgb.isNull())

        return report;



    const double thr = qBound(0.0, p.minLuma, 255.0);

    for (int i = 0; i < report.ledCount; ++i) {

        LedResult& L = report.leds[i];

        L.blinkMode = isBlinkMode(modeAt(p, i));

        if (L.blinkMode)

            report.needMultiFrame = true;

        if (i >= p.rois.size())

            continue;

        const QRect r = p.rois.at(i).intersected(rgb.rect());

        if (r.width() < 4 || r.height() < 4)

            continue;

        L.roi = r;

        L.luma = averageLuma(rgb, r);

        const bool bright = L.luma >= thr;

        L.on = bright; // 单帧时：常亮=本帧亮；闪烁单帧无法结案，外层应走多帧

        L.onFrames = bright ? 1 : 0;

        L.offFrames = bright ? 0 : 1;

        L.valid = true;

        ++report.configuredCount;

        if (L.on)

            ++report.onCount;

    }

    report.allConfiguredOn =

        report.configuredCount == report.ledCount && report.onCount == report.ledCount;

    report.annotated = drawAnnotated(rgb, report.leds);

    return report;

}



void accumulateFrame(QVector<LedResult>& tracks, const Report& frame) {

    const int n = frame.leds.size();

    if (tracks.size() < n)

        tracks.resize(n);

    for (int i = 0; i < n; ++i) {

        const LedResult& F = frame.leds.at(i);

        LedResult& T = tracks[i];

        if (!F.valid)

            continue;

        T.roi = F.roi;

        T.luma = F.luma;

        T.valid = true;

        T.blinkMode = F.blinkMode;

        if (F.onFrames > 0)

            ++T.onFrames;

        else if (F.offFrames > 0)

            ++T.offFrames;

        else if (F.on)

            ++T.onFrames;

        else

            ++T.offFrames;

    }

}



Report finalizeByModes(const QVector<LedResult>& tracks, const Params& p, const QImage& lastRgb) {

    Report report;

    report.ledCount = clampLedCount(p.ledCount);

    report.leds.resize(report.ledCount);

    int frames = 0;

    for (int i = 0; i < report.ledCount; ++i) {

        LedResult& L = report.leds[i];

        const bool blink = isBlinkMode(modeAt(p, i));

        L.blinkMode = blink;

        if (blink)

            report.needMultiFrame = true;

        if (i >= tracks.size() || !tracks.at(i).valid)

            continue;

        const LedResult& T = tracks.at(i);

        L.roi = T.roi;

        L.luma = T.luma;

        L.onFrames = T.onFrames;

        L.offFrames = T.offFrames;

        L.valid = true;

        // 闪烁：须既亮又灭；常亮：窗口内不得灭、且至少一帧亮

        if (blink)

            L.on = (T.onFrames > 0 && T.offFrames > 0);

        else

            L.on = (T.onFrames > 0 && T.offFrames == 0);

        ++report.configuredCount;

        frames = qMax(frames, T.onFrames + T.offFrames);

        if (L.on)

            ++report.onCount;

    }

    report.frameCount = frames;

    report.allConfiguredOn =

        report.configuredCount == report.ledCount && report.onCount == report.ledCount;

    report.annotated = drawAnnotated(lastRgb, report.leds);

    return report;

}



QString storageRootDir() {

    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("led_inspect"));

}



void cleanupStoredImages(const QString& dirPath, int keepNewest, int maxAgeDays) {

    QDir dir(dirPath);

    if (!dir.exists())

        return;

    const QFileInfoList files =

        dir.entryInfoList({QStringLiteral("*.jpg"), QStringLiteral("*.png"), QStringLiteral("*.bmp")},

                          QDir::Files, QDir::Time);

    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-qMax(1, maxAgeDays));

    int kept = 0;

    for (const QFileInfo& fi : files) {

        const QString name = fi.fileName();

        if (name.startsWith(QLatin1String("last_")))

            continue;

        ++kept;

        if (kept > keepNewest || fi.lastModified() < cutoff)

            QFile::remove(fi.absoluteFilePath());

    }

}



} // namespace LedInspectAnalyzer


