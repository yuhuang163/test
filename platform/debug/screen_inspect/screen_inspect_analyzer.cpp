#include "screen_inspect_analyzer.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QVector>
#include <QtMath>
#include <algorithm>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QImage toRgb888(const QImage& src) {
    return src.convertToFormat(QImage::Format_RGB888);
}

QVector<quint8> toGrayBytes(const QImage& rgb) {
    const QImage img = toRgb888(rgb);
    const int w = img.width();
    const int h = img.height();
    QVector<quint8> gray(w * h);
    for (int y = 0; y < h; ++y) {
        const uchar* line = img.constScanLine(y);
        quint8* dst = gray.data() + y * w;
        for (int x = 0; x < w; ++x) {
            const int i = x * 3;
            dst[x] = static_cast<quint8>((77 * line[i] + 150 * line[i + 1] + 29 * line[i + 2]) >> 8);
        }
    }
    return gray;
}

/** 屏幕 ROI：取偏亮区域包围盒，避免桌面背景把 SSIM 拉低。 */
QRect detectScreenRoi(const QImage& rgb) {
    const QImage img = toRgb888(rgb);
    const int w = img.width();
    const int h = img.height();
    if (w < 16 || h < 16)
        return QRect(0, 0, w, h);

    QVector<int> hist(256, 0);
    QVector<quint8> vmap(w * h);
    for (int y = 0; y < h; ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            const int i = x * 3;
            const quint8 v = static_cast<quint8>(qMax(line[i], qMax(line[i + 1], line[i + 2])));
            vmap[y * w + x] = v;
            ++hist[v];
        }
    }
    const int target = static_cast<int>(w * h * 0.62);
    int acc = 0;
    int thr = 80;
    for (int i = 0; i < 256; ++i) {
        acc += hist[i];
        if (acc >= target) {
            thr = i;
            break;
        }
    }

    int x0 = w, y0 = h, x1 = 0, y1 = 0;
    int count = 0;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (vmap[y * w + x] < thr)
                continue;
            ++count;
            x0 = qMin(x0, x);
            y0 = qMin(y0, y);
            x1 = qMax(x1, x);
            y1 = qMax(y1, y);
        }
    }
    if (count < w * h / 12 || x1 <= x0 || y1 <= y0)
        return QRect(0, 0, w, h);

    const int mx = qMax(2, (x1 - x0) / 25);
    const int my = qMax(2, (y1 - y0) / 25);
    return QRect(x0 + mx, y0 + my, (x1 - x0 + 1) - 2 * mx, (y1 - y0 + 1) - 2 * my).intersected(QRect(0, 0, w, h));
}

double ssimOnGray(const QVector<quint8>& a, const QVector<quint8>& b, int w, int h) {
    if (a.size() != b.size() || w < 8 || h < 8)
        return 0.0;
    const int block = 8;
    const double c1 = 6.5025;  // (0.01*255)^2
    const double c2 = 58.5225; // (0.03*255)^2
    double sum = 0.0;
    int n = 0;
    for (int y = 0; y + block <= h; y += block) {
        for (int x = 0; x + block <= w; x += block) {
            double mx = 0, my = 0;
            for (int j = 0; j < block; ++j) {
                const quint8* pa = a.constData() + (y + j) * w + x;
                const quint8* pb = b.constData() + (y + j) * w + x;
                for (int i = 0; i < block; ++i) {
                    mx += pa[i];
                    my += pb[i];
                }
            }
            const double inv = 1.0 / (block * block);
            mx *= inv;
            my *= inv;
            double vx = 0, vy = 0, cov = 0;
            for (int j = 0; j < block; ++j) {
                const quint8* pa = a.constData() + (y + j) * w + x;
                const quint8* pb = b.constData() + (y + j) * w + x;
                for (int i = 0; i < block; ++i) {
                    const double dx = pa[i] - mx;
                    const double dy = pb[i] - my;
                    vx += dx * dx;
                    vy += dy * dy;
                    cov += dx * dy;
                }
            }
            vx *= inv;
            vy *= inv;
            cov *= inv;
            const double num = (2.0 * mx * my + c1) * (2.0 * cov + c2);
            const double den = (mx * mx + my * my + c1) * (vx + vy + c2);
            if (den > 1e-9) {
                sum += num / den;
                ++n;
            }
        }
    }
    if (n <= 0)
        return 0.0;
    return qBound(0.0, sum / n, 1.0);
}

bool pixelMatchesSolidColor(int r, int g, int b, int gray, int colorIndex) {
    const int spread = qMax(r, qMax(g, b)) - qMin(r, qMin(g, b));
    switch (colorIndex) {
    case 4:
        return gray < 85 && spread < 55;
    case 5:
        return gray >= 85 && gray <= 165 && spread < 50;
    case 3:
        return gray > 165 && spread < 55;
    case 0:
        return b >= r + 20 && b >= g + 20;
    case 1:
        return g >= r + 20 && g >= b + 20;
    case 2:
        return r >= g + 20 && r >= b + 20;
    default:
        return true;
    }
}

int guessExpectedColor(const QImage& rgb, const QRect& roi) {
    const QImage img = toRgb888(rgb);
    const QRect r = roi.intersected(img.rect());
    int matchCount[6] = {0, 0, 0, 0, 0, 0};
    int n = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); ++x) {
            const int i = x * 3;
            const int pr = line[i];
            const int pg = line[i + 1];
            const int pb = line[i + 2];
            const int gray = (77 * pr + 150 * pg + 29 * pb) >> 8;
            ++n;
            for (int c = 0; c <= 5; ++c) {
                if (pixelMatchesSolidColor(pr, pg, pb, gray, c))
                    ++matchCount[c];
            }
        }
    }
    if (n <= 0)
        return -1;
    QVector<int> grays;
    grays.reserve(n);
    for (int y = r.top(); y <= r.bottom(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); ++x) {
            const int i = x * 3;
            grays.append((77 * line[i] + 150 * line[i + 1] + 29 * line[i + 2]) >> 8);
        }
    }
    std::sort(grays.begin(), grays.end());
    const int p25 = grays[grays.size() / 4];
    const int p75 = grays[grays.size() * 3 / 4];

    int best = -1;
    int bestN = 0;
    for (int c = 0; c <= 5; ++c) {
        if (matchCount[c] > bestN) {
            bestN = matchCount[c];
            best = c;
        }
    }
    if (best >= 0 && bestN >= n * 30 / 100) {
        // 圆屏+矩形 ROI 时灰角易把「白」票拉高，用暗部 p25 纠正黑屏
        if (best == 3 && matchCount[4] >= n * 30 / 100 && p25 < 70)
            return 4;
        if (best == 4 && matchCount[3] >= n * 30 / 100 && p75 > 140)
            return 3;
        return best;
    }
    if (p25 < 70 && matchCount[4] >= n * 25 / 100)
        return 4;
    if (p75 > 130 && matchCount[3] >= n * 25 / 100)
        return 3;

    qint64 sr = 0, sg = 0, sb = 0;
    for (int y = r.top(); y <= r.bottom(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); ++x) {
            const int i = x * 3;
            sr += line[i];
            sg += line[i + 1];
            sb += line[i + 2];
        }
    }
    const int mr = static_cast<int>(sr / n);
    const int mg = static_cast<int>(sg / n);
    const int mb = static_cast<int>(sb / n);
    const int spread = qMax(mr, qMax(mg, mb)) - qMin(mr, qMin(mg, mb));
    const int gray = (77 * mr + 150 * mg + 29 * mb) >> 8;
    if (spread < 50) {
        if (gray < 80)
            return 4;
        if (gray > 165)
            return 3;
        if (gray >= 85)
            return 5;
    }
    if (mb >= mr + 25 && mb >= mg + 25)
        return 0;
    if (mg >= mr + 25 && mg >= mb + 25)
        return 1;
    if (mr >= mg + 25 && mr >= mb + 25)
        return 2;
    return -1;
}

struct DeadScan {
    int count = 0;
    QVector<QPoint> points;
    double muraStd = 0.0;
};

DeadScan scanDeadPixels(const QImage& rgb, const QRect& roi, int deadDiff, int expectedColor) {
    DeadScan out;
    const QImage img = toRgb888(rgb);
    const QRect r = roi.intersected(img.rect());
    if (r.width() < 10 || r.height() < 10)
        return out;

    const QVector<quint8> gray = toGrayBytes(img);
    const int w = img.width();
    const int rad = 2;
    QVector<quint8> mark(gray.size(), 0);
    qint64 sum = 0;
    qint64 sum2 = 0;
    int n = 0;

    auto consider = [&](int x, int y, bool hit) {
        if (!hit)
            return;
        const int idx = y * w + x;
        if (mark[idx])
            return;
        mark[idx] = 1;
        ++out.count;
        if (out.points.size() < 80)
            out.points.append(QPoint(x, y));
    };

    for (int y = r.top() + rad; y <= r.bottom() - rad; ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left() + rad; x <= r.right() - rad; ++x) {
            const int i = x * 3;
            const int pr = line[i];
            const int pg = line[i + 1];
            const int pb = line[i + 2];
            const int idx = y * w + x;
            const int g = gray[idx];
            if (expectedColor >= 0
                && !pixelMatchesSolidColor(pr, pg, pb, g, expectedColor))
                continue;

            sum += g;
            sum2 += g * g;
            ++n;

            int local = 0;
            int gmin = 255;
            int gmax = 0;
            int cells = 0;
            for (int j = -rad; j <= rad; ++j) {
                for (int k = -rad; k <= rad; ++k) {
                    if (k == 0 && j == 0)
                        continue;
                    const int v = gray[(y + j) * w + (x + k)];
                    local += v;
                    gmin = qMin(gmin, v);
                    gmax = qMax(gmax, v);
                    ++cells;
                }
            }
            const int mean = local / qMax(1, cells);
            const int edgeThr = expectedColor >= 0 ? deadDiff + 15 : deadDiff * 2 + 10;
            if (gmax - gmin > edgeThr)
                continue;
            consider(x, y, qAbs(g - mean) >= deadDiff);

            if (expectedColor == 4)
                consider(x, y, g >= mean + deadDiff && g >= 40);
            else if (expectedColor == 3)
                consider(x, y, g <= mean - deadDiff && g <= 210);
        }
    }

    if (expectedColor >= 0 && expectedColor <= 2) {
        // 旁通道抬升阈值：纯色红/绿/蓝上的发白、粉斑（主通道仍高，灰阶对比易被边沿跳过）
        const int impurityThr = qMax(70, deadDiff + 30);
        for (int y = r.top() + rad; y <= r.bottom() - rad; ++y) {
            const uchar* line = img.constScanLine(y);
            for (int x = r.left() + rad; x <= r.right() - rad; ++x) {
                const int i = x * 3;
                const int pr = line[i];
                const int pg = line[i + 1];
                const int pb = line[i + 2];
                const int gy = gray[y * w + x];
                if (!pixelMatchesSolidColor(pr, pg, pb, gy, expectedColor))
                    continue;
                const int ch = expectedColor == 0 ? 2 : (expectedColor == 1 ? 1 : 0);
                const int mainCh = line[i + ch];
                const int o1 = line[i + ((ch + 1) % 3)];
                const int o2 = line[i + ((ch + 2) % 3)];
                const int other = (o1 + o2) / 2;
                // 暗坏点：主通道明显偏弱
                consider(x, y, mainCh + deadDiff < other || (mainCh < 40 && other > 80));
                // 异色/发白：主通道仍很亮，但两旁通道同时明显抬高（如红底上的白/粉斑）
                consider(x, y, mainCh >= 200 && o1 >= impurityThr && o2 >= impurityThr);
            }
        }
    }

    // 异色黑点/白斑：仅在 ROI 内缩区检测，避免矩形框里的黑边框连片误报
    if (expectedColor >= 0) {
        const int inset = qMax(8, qMin(r.width(), r.height()) / 10);
        const QRect core = r.adjusted(inset, inset, -inset, -inset);
        if (core.width() >= 12 && core.height() >= 12) {
            QVector<QPoint> foreignSeeds;
            for (int y = core.top() + 1; y <= core.bottom() - 1; ++y) {
                const uchar* line = img.constScanLine(y);
                for (int x = core.left() + 1; x <= core.right() - 1; ++x) {
                    const int i = x * 3;
                    const int pr = line[i];
                    const int pg = line[i + 1];
                    const int pb = line[i + 2];
                    const int g = gray[y * w + x];
                    if (pixelMatchesSolidColor(pr, pg, pb, g, expectedColor))
                        continue;

                    int solidN = 0;
                    int solidGraySum = 0;
                    for (int j = -1; j <= 1; ++j) {
                        for (int k = -1; k <= 1; ++k) {
                            if (j == 0 && k == 0)
                                continue;
                            const int nx = x + k;
                            const int ny = y + j;
                            const uchar* nl = img.constScanLine(ny);
                            const int ni = nx * 3;
                            const int ng = gray[ny * w + nx];
                            if (pixelMatchesSolidColor(nl[ni], nl[ni + 1], nl[ni + 2], ng, expectedColor)) {
                                ++solidN;
                                solidGraySum += ng;
                            }
                        }
                    }
                    if (solidN < 5)
                        continue;

                    const int spread = qMax(pr, qMax(pg, pb)) - qMin(pr, qMin(pg, pb));
                    const int solidMean = solidGraySum / solidN;
                    // 邻域纯色也偏暗时多半是边框过渡，不种黑点种子
                    if (expectedColor <= 2 && solidMean < 50)
                        continue;
                    const bool blackSpot = g <= 50 || (solidMean - g) >= deadDiff + 15;
                    const bool whiteSpot = (g >= 175 && spread <= 55) || (pr >= 200 && pg >= 100 && pb >= 100);
                    if (!blackSpot && !whiteSpot)
                        continue;
                    foreignSeeds.append(QPoint(x, y));
                }
            }

            for (const QPoint& seed : foreignSeeds) {
                const int sg = gray[seed.y() * w + seed.x()];
                const bool seedDark = sg <= 80;
                QVector<QPoint> stack;
                stack.append(seed);
                while (!stack.isEmpty()) {
                    const QPoint p = stack.takeLast();
                    if (!core.contains(p))
                        continue;
                    const int idx = p.y() * w + p.x();
                    if (mark[idx])
                        continue;
                    const uchar* line = img.constScanLine(p.y());
                    const int i = p.x() * 3;
                    const int pr = line[i];
                    const int pg = line[i + 1];
                    const int pb = line[i + 2];
                    const int g = gray[idx];
                    if (pixelMatchesSolidColor(pr, pg, pb, g, expectedColor))
                        continue;
                    const int spread = qMax(pr, qMax(pg, pb)) - qMin(pr, qMin(pg, pb));
                    const bool sameKind =
                        seedDark ? (g <= 70)
                                 : ((g >= 170 && spread <= 60) || (pr >= 190 && pg >= 95 && pb >= 95));
                    if (!sameKind)
                        continue;
                    consider(p.x(), p.y(), true);
                    stack.append(QPoint(p.x() + 1, p.y()));
                    stack.append(QPoint(p.x() - 1, p.y()));
                    stack.append(QPoint(p.x(), p.y() + 1));
                    stack.append(QPoint(p.x(), p.y() - 1));
                }
            }
        }
    }

    if (n > 1) {
        const double mean = static_cast<double>(sum) / n;
        const double var = static_cast<double>(sum2) / n - mean * mean;
        out.muraStd = var > 0 ? qSqrt(var) : 0.0;
    }
    return out;
}

QImage drawAnnotated(const QImage& rgb, const QRect& roi, const QVector<QPoint>& pts) {
    QImage out = rgb.convertToFormat(QImage::Format_RGB32);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 200, 0), 2));
    p.drawRect(roi.adjusted(0, 0, -1, -1));
    p.setPen(QPen(QColor(220, 0, 0), 2));
    for (const QPoint& pt : pts)
        p.drawEllipse(pt, 6, 6);
    p.end();
    return out;
}

} // namespace

namespace ScreenInspectAnalyzer {

Report analyze(const QImage& currRgb, const QImage& refRgb, const Params& p) {
    Report report;
    const QImage curr = toRgb888(currRgb);
    const QRect autoRoi = detectScreenRoi(curr);
    QRect roi = p.manualRoi.intersected(curr.rect());
    if (roi.width() < 10 || roi.height() < 10)
        roi = autoRoi;
    report.roi = roi;

    int expected = p.expectedColor;
    const int detected = guessExpectedColor(curr, roi);
    report.detectedColor = detected;
    if (expected < 0)
        expected = detected;
    report.expectedColorUsed = expected;
    report.colorMatch = p.expectedColor < 0 ? -1 : (detected == p.expectedColor ? 1 : 0);

    const DeadScan dead = scanDeadPixels(curr, roi, p.deadDiff, expected);
    report.deadPixels = dead.count;
    report.muraStd = dead.muraStd;
    report.annotated = drawAnnotated(curr, roi, dead.points);

    if (!refRgb.isNull()) {
        QImage ref = toRgb888(refRgb);
        if (ref.size() != curr.size())
            ref = ref.scaled(curr.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const int tw = 256;
        const int th = qMax(8, curr.height() * tw / qMax(1, curr.width()));
        const QImage a = curr.copy(roi).scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        const QImage b = ref.copy(roi).scaled(tw, th, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        report.ssim = ssimOnGray(toGrayBytes(a), toGrayBytes(b), a.width(), a.height());
    }
    return report;
}

QString colorName(int colorIndex) {
    switch (colorIndex) {
    case 0:
        return QStringLiteral("蓝");
    case 1:
        return QStringLiteral("绿");
    case 2:
        return QStringLiteral("红");
    case 3:
        return QStringLiteral("白");
    case 4:
        return QStringLiteral("黑");
    case 5:
        return QStringLiteral("灰");
    default:
        return QStringLiteral("未识别");
    }
}

} // namespace ScreenInspectAnalyzer

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
