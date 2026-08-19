#include "screen_inspect_analyzer.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPoint>
#include <QVector>
#include <QtMath>

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

int guessExpectedColor(const QImage& rgb, const QRect& roi) {
    const QImage img = toRgb888(rgb);
    qint64 sr = 0, sg = 0, sb = 0;
    int n = 0;
    const QRect r = roi.intersected(img.rect());
    for (int y = r.top(); y <= r.bottom(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); ++x) {
            const int i = x * 3;
            sr += line[i];
            sg += line[i + 1];
            sb += line[i + 2];
            ++n;
        }
    }
    if (n <= 0)
        return -1;
    const int mr = static_cast<int>(sr / n);
    const int mg = static_cast<int>(sg / n);
    const int mb = static_cast<int>(sb / n);
    const int mx = qMax(mr, qMax(mg, mb));
    const int mn = qMin(mr, qMin(mg, mb));
    if (mx < 45)
        return 4; // 黑
    if (mn > 170)
        return 3; // 白
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
        for (int x = r.left() + rad; x <= r.right() - rad; ++x) {
            const int idx = y * w + x;
            const int g = gray[idx];
            sum += g;
            sum2 += g * g;
            ++n;

            int local = 0;
            int gmin = 255;
            int gmax = 0;
            int cells = 0;
            for (int j = -rad; j <= rad; ++j) {
                for (int i = -rad; i <= rad; ++i) {
                    if (i == 0 && j == 0)
                        continue;
                    const int v = gray[(y + j) * w + (x + i)];
                    local += v;
                    gmin = qMin(gmin, v);
                    gmax = qMax(gmax, v);
                    ++cells;
                }
            }
            const int mean = local / qMax(1, cells);
            // 边沿/图标对比度大，不当坏点
            if (gmax - gmin > deadDiff * 2 + 10)
                continue;
            consider(x, y, qAbs(g - mean) >= deadDiff);

            if (expectedColor == 4)
                consider(x, y, g >= mean + deadDiff && g >= 40);
            else if (expectedColor == 3)
                consider(x, y, g <= mean - deadDiff && g <= 210);
        }
    }

    if (expectedColor >= 0 && expectedColor <= 2) {
        const int ch = expectedColor == 0 ? 2 : (expectedColor == 1 ? 1 : 0); // B/G/R 在 RGB888 下标
        for (int y = r.top() + rad; y <= r.bottom() - rad; ++y) {
            const uchar* line = img.constScanLine(y);
            for (int x = r.left() + rad; x <= r.right() - rad; ++x) {
                const int i = x * 3;
                const int mainCh = line[i + ch];
                const int other = (line[i] + line[i + 1] + line[i + 2] - mainCh) / 2;
                consider(x, y, mainCh + deadDiff < other || (mainCh < 40 && other > 80));
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
    const QRect roi = detectScreenRoi(curr);
    report.roi = roi;

    int expected = p.expectedColor;
    if (expected < 0)
        expected = guessExpectedColor(curr, roi);
    report.expectedColorUsed = expected;

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

} // namespace ScreenInspectAnalyzer

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
