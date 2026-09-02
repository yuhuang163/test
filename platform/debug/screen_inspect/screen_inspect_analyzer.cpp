#include "screen_inspect_analyzer.h"

#include <QColor>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
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

/** 圆屏掩膜：正方形 ROI 四角黑框不参与颜色/坏点。 */
struct ScreenCircle {
    int cx = 0;
    int cy = 0;
    int r = 0;
    bool contains(int x, int y) const {
        const qint64 dx = x - cx;
        const qint64 dy = y - cy;
        return dx * dx + dy * dy <= qint64(r) * qint64(r);
    }
};

/**
 * 在矩形 ROI 内估计圆屏半径：亮屏用径向亮度跌落找边；全黑则用内接圆略内缩。
 */
ScreenCircle detectScreenCircle(const QImage& rgb, const QRect& roi) {
    const QImage img = toRgb888(rgb);
    const QRect r = roi.intersected(img.rect());
    ScreenCircle c;
    
    int rMax = qMin(r.width(), r.height()) / 2;
    if (rMax < 8) {
        c.cx = r.center().x();
        c.cy = r.center().y();
        c.r = qMax(0, rMax);
        return c;
    }

    // 动态找准真实圆心：在固定的小 ROI 框内，快速找出亮色区域的包围盒重心
    int x0 = r.right(), y0 = r.bottom(), x1 = r.left(), y1 = r.top();
    int maxVal = 0;
    
    // 第一遍极速抽样扫描，找到 ROI 内的最高亮度（步长=4，速度极快）
    for (int y = r.top(); y <= r.bottom(); y += 4) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); x += 4) {
            const int i = x * 3;
            const int val = qMax(line[i], qMax(line[i + 1], line[i + 2]));
            if (val > maxVal) maxVal = val;
        }
    }
    
    // 只有当画面有足够亮的内容时，才去找真实中心，避免全黑屏干扰
    if (maxVal > 30) {
        const int thr = qMax(30, maxVal * 60 / 100); 
        for (int y = r.top(); y <= r.bottom(); y += 4) {
            const uchar* line = img.constScanLine(y);
            for (int x = r.left(); x <= r.right(); x += 4) {
                const int i = x * 3;
                if (qMax(line[i], qMax(line[i + 1], line[i + 2])) >= thr) {
                    if (x < x0) x0 = x;
                    if (x > x1) x1 = x;
                    if (y < y0) y0 = y;
                    if (y > y1) y1 = y;
                }
            }
        }
        if (x1 >= x0 && y1 >= y0) {
            c.cx = (x0 + x1) / 2;
            c.cy = (y0 + y1) / 2;
            int dynamicRMax = qMin(x1 - x0, y1 - y0) / 2;
            if (dynamicRMax >= 8) {
                rMax = dynamicRMax;
            }
        } else {
            c.cx = r.center().x();
            c.cy = r.center().y();
        }
    } else {
        c.cx = r.center().x();
        c.cy = r.center().y();
    }

    QVector<qint64> sum(rMax + 1, 0);
    QVector<int> cnt(rMax + 1, 0);
    for (int y = r.top(); y <= r.bottom(); ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); ++x) {
            const int dx = x - c.cx;
            const int dy = y - c.cy;
            const int rad = static_cast<int>(qSqrt(static_cast<qreal>(dx) * dx + static_cast<qreal>(dy) * dy) + 0.5);
            if (rad > rMax)
                continue;
            const int i = x * 3;
            sum[rad] += qMax(line[i], qMax(line[i + 1], line[i + 2]));
            ++cnt[rad];
        }
    }

    QVector<int> mean(rMax + 1, 0);
    for (int i = 0; i <= rMax; ++i)
        mean[i] = cnt[i] > 0 ? static_cast<int>(sum[i] / cnt[i]) : 0;

    int innerSum = 0;
    int innerN = 0;
    const int i0 = qMax(1, rMax / 10);
    const int i1 = qMax(i0 + 1, rMax * 35 / 100);
    for (int i = i0; i <= i1; ++i) {
        if (cnt[i] <= 0)
            continue;
        innerSum += mean[i];
        ++innerN;
    }
    const int innerMean = innerN > 0 ? innerSum / innerN : 0;
    // 黑屏/暗屏：亮边检测不可靠，用 ROI 内接圆略内缩避开四角
    if (innerMean < 55) {
        c.r = qMax(8, rMax * 92 / 100);
        return c;
    }

    const int thr = qMax(40, innerMean * 55 / 100);
    int edge = rMax * 92 / 100;
    for (int i = rMax; i >= rMax / 5; --i) {
        if (mean[i] >= thr) {
            edge = i;
            break;
        }
    }
    // 内缩到较亮实心区，避开圆屏边缘亮暗过渡带
    c.r = qMax(8, edge * 90 / 100);
    return c;
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

/** 与坏点一致：只统计圆屏内（圆心/半径相对裁剪图坐标系）的 8x8 块。 */
double ssimOnGrayInCircle(const QVector<quint8>& a, const QVector<quint8>& b, int w, int h, int cx, int cy,
                          int radius) {
    if (a.size() != b.size() || w < 8 || h < 8 || radius < 8)
        return 0.0;
    const int block = 8;
    const double c1 = 6.5025;
    const double c2 = 58.5225;
    const qint64 r2 = qint64(radius) * qint64(radius);
    double sum = 0.0;
    int n = 0;
    for (int y = 0; y + block <= h; y += block) {
        for (int x = 0; x + block <= w; x += block) {
            const int bx = x + block / 2;
            const int by = y + block / 2;
            const qint64 dx = bx - cx;
            const qint64 dy = by - cy;
            if (dx * dx + dy * dy > r2)
                continue;
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
                    const double ddx = pa[i] - mx;
                    const double ddy = pb[i] - my;
                    vx += ddx * ddx;
                    vy += ddy * ddy;
                    cov += ddx * ddy;
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

int guessExpectedColor(const QImage& rgb, const QRect& roi, const ScreenCircle& circle) {
    const QImage img = toRgb888(rgb);
    const QRect r = roi.intersected(img.rect());
    int matchCount[6] = {0, 0, 0, 0, 0, 0};
    int n = 0;
    // 大 ROI 逐像素扫圆内极慢（千万级）；抽稀到约数万点即可判纯色
    const int area = qMax(1, r.width() * r.height());
    const int step = qBound(1, static_cast<int>(qCeil(qSqrt(area / 40000.0))), 16);
    QVector<int> grays;
    grays.reserve(qMax(1, area / (step * step * 2)));
    qint64 sr = 0, sg = 0, sb = 0;
    for (int y = r.top(); y <= r.bottom(); y += step) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left(); x <= r.right(); x += step) {
            if (!circle.contains(x, y))
                continue;
            const int i = x * 3;
            const int pr = line[i];
            const int pg = line[i + 1];
            const int pb = line[i + 2];
            const int gray = (77 * pr + 150 * pg + 29 * pb) >> 8;
            ++n;
            grays.append(gray);
            sr += pr;
            sg += pg;
            sb += pb;
            for (int c = 0; c <= 5; ++c) {
                if (pixelMatchesSolidColor(pr, pg, pb, gray, c))
                    ++matchCount[c];
            }
        }
    }
    if (n <= 0)
        return -1;
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
        // 圆内若仍偏暗却混有高亮：黑票领先时纠正为白
        if (best == 4 && matchCount[3] >= n * 30 / 100 && p75 > 140)
            return 3;
        return best;
    }
    if (p25 < 70 && matchCount[4] >= n * 25 / 100)
        return 4;
    if (p75 > 130 && matchCount[3] >= n * 25 / 100)
        return 3;

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

/** 坏点识别上限：达到后不再继续扫/涨连通域，避免花屏时卡死。 */
constexpr int kMaxDeadPixelsDetect = 500;

/** 灰度积分图，用于大半径盒式模糊（抑制摩尔纹/拍摄纹路）。 */
QVector<qint64> buildGrayIntegral(const QVector<quint8>& gray, int w, int h) {
    QVector<qint64> integ((w + 1) * (h + 1), 0);
    for (int y = 0; y < h; ++y) {
        qint64 row = 0;
        for (int x = 0; x < w; ++x) {
            row += gray[y * w + x];
            integ[(y + 1) * (w + 1) + (x + 1)] = integ[y * (w + 1) + (x + 1)] + row;
        }
    }
    return integ;
}

int boxBlurGrayAt(const QVector<qint64>& integ, int w, int h, int x, int y, int rad) {
    const int x0 = qMax(0, x - rad);
    const int y0 = qMax(0, y - rad);
    const int x1 = qMin(w - 1, x + rad);
    const int y1 = qMin(h - 1, y + rad);
    const int stride = w + 1;
    const qint64 sum = integ[(y1 + 1) * stride + (x1 + 1)] - integ[y0 * stride + (x1 + 1)]
                       - integ[(y1 + 1) * stride + x0] + integ[y0 * stride + x0];
    const int area = (x1 - x0 + 1) * (y1 - y0 + 1);
    return static_cast<int>(sum / qMax(1, area));
}

/**
 * mark：1=弱候选（可能是纹路），2=强候选（尖点/真坏点）。
 * 小连通域全部保留；大连通域只保留强点，避免摩尔条带灌进坏点数，同时不丢真坏点。
 * 计入达到 maxDead 后提前结束。
 */
void finalizeDeadMarks(QVector<quint8>& mark, int w, int h, const QRect& r, int maxBlob, int maxDead,
                       DeadScan& out) {
    out.count = 0;
    out.points.clear();
    QVector<quint8> keep(mark.size(), 0);
    QVector<QPoint> stack;
    stack.reserve(256);
    for (int y = r.top(); y <= r.bottom() && out.count < maxDead; ++y) {
        for (int x = r.left(); x <= r.right() && out.count < maxDead; ++x) {
            const int start = y * w + x;
            if (!mark[start] || keep[start])
                continue;
            stack.clear();
            stack.append(QPoint(x, y));
            const quint8 startStr = mark[start];
            mark[start] = 0;
            QVector<QPoint> comp;
            QVector<quint8> compStr;
            comp.reserve(64);
            compStr.reserve(64);
            comp.append(QPoint(x, y));
            compStr.append(startStr);
            while (!stack.isEmpty()) {
                const QPoint p = stack.takeLast();
                const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (const auto& d : dirs) {
                    const int nx = p.x() + d[0];
                    const int ny = p.y() + d[1];
                    if (nx < r.left() || nx > r.right() || ny < r.top() || ny > r.bottom())
                        continue;
                    const int ni = ny * w + nx;
                    if (!mark[ni])
                        continue;
                    compStr.append(mark[ni]);
                    mark[ni] = 0;
                    const QPoint np(nx, ny);
                    comp.append(np);
                    stack.append(np);
                }
            }
            if (comp.isEmpty())
                continue;
            const bool smallBlob = comp.size() <= maxBlob;
            for (int i = 0; i < comp.size() && out.count < maxDead; ++i) {
                // 小斑全留；大片纹路里只留强尖点（真坏点）
                if (!smallBlob && compStr.at(i) < 2)
                    continue;
                const QPoint& p = comp.at(i);
                keep[p.y() * w + p.x()] = 1;
                ++out.count;
                if (out.points.size() < 80)
                    out.points.append(p);
            }
        }
    }
    mark.swap(keep);
}

DeadScan scanDeadPixels(const QImage& rgb, const QRect& roi, int deadDiff, int expectedColor,
                        const ScreenCircle& circle, int deadRadiusPercent) {
    DeadScan out;
    const QImage img = toRgb888(rgb);
    const QRect r = roi.intersected(img.rect());
    if (r.width() < 10 || r.height() < 10 || circle.r < 8)
        return out;

    // 坏点再内缩：边缘过渡环被圆裁切后易成大量小连通域，全部计入坏点
    ScreenCircle dead = circle;
    dead.r = qMax(8, circle.r * deadRadiusPercent / 100);

    const QVector<quint8> gray = toGrayBytes(img);
    const int w = img.width();
    const int h = img.height();
    // 模糊只用来压缓变摩尔纹；真坏点靠尖残差，不强行抬高门槛
    const int blurRad = qBound(6, qMin(r.width(), r.height()) / 100, 20);
    const QVector<qint64> integ = buildGrayIntegral(gray, w, h);
    // 小连通域上限：略放宽，避免几个相邻真坏点被当成纹路丢掉
    const int maxBlob = qBound(40, qMin(r.width(), r.height()) / 80, 120);
    const int rad = 2;
    const int strongExtra = qMax(12, deadDiff / 2); // 强点：比普通阈值再高一截
    QVector<quint8> mark(gray.size(), 0);
    qint64 sum = 0;
    qint64 sum2 = 0;
    int n = 0;
    int markedN = 0; // 已标记候选像素数，达上限则跳过后续坏点扫描

    auto markHit = [&](int x, int y, int strength) {
        if (strength <= 0 || markedN >= kMaxDeadPixelsDetect)
            return;
        const int idx = y * w + x;
        const quint8 old = mark[idx];
        if (old < static_cast<quint8>(strength)) {
            if (old == 0)
                ++markedN;
            mark[idx] = static_cast<quint8>(strength);
        }
    };

    // 仅扫描圆屏内缩区；正方形 ROI 四角黑框与边缘过渡环跳过
    for (int y = r.top() + rad; y <= r.bottom() - rad; ++y) {
        const uchar* line = img.constScanLine(y);
        for (int x = r.left() + rad; x <= r.right() - rad; ++x) {
            if (!dead.contains(x, y))
                continue;
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

            if (markedN >= kMaxDeadPixelsDetect)
                continue;

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

            const int bg = boxBlurGrayAt(integ, w, h, x, y, blurRad);
            const int residual = qAbs(g - bg);
            const int localDiff = qAbs(g - mean);
            // 弱：模糊残差达阈值（缓变纹路残差通常不够）
            if (residual >= deadDiff)
                markHit(x, y, 1);
            // 强：尖点明显高于普通阈值，或局部也明显跳变 → 大片纹路里也保留
            if (residual >= deadDiff + strongExtra || (residual >= deadDiff && localDiff >= deadDiff + 8))
                markHit(x, y, 2);

            if (expectedColor == 4) {
                if (g >= bg + deadDiff && g >= 40)
                    markHit(x, y, g >= bg + deadDiff + strongExtra ? 2 : 1);
            } else if (expectedColor == 3) {
                if (g <= bg - deadDiff && g <= 210)
                    markHit(x, y, g <= bg - deadDiff - strongExtra ? 2 : 1);
            }
        }
    }

    if (markedN < kMaxDeadPixelsDetect && expectedColor >= 0 && expectedColor <= 2) {
        const int impurityThr = qMax(70, deadDiff + 30);
        for (int y = r.top() + rad; y <= r.bottom() - rad && markedN < kMaxDeadPixelsDetect; ++y) {
            const uchar* line = img.constScanLine(y);
            for (int x = r.left() + rad; x <= r.right() - rad && markedN < kMaxDeadPixelsDetect; ++x) {
                if (!dead.contains(x, y))
                    continue;
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
                const int bg = boxBlurGrayAt(integ, w, h, x, y, blurRad);
                // 恢复对明显暗点/异色的敏感；极暗点标强，避免被大连通域滤掉
                if (mainCh < 40 && other > 80) {
                    markHit(x, y, 2);
                } else if (mainCh + deadDiff < other) {
                    const int res = qAbs(gy - bg);
                    markHit(x, y, res >= deadDiff + strongExtra ? 2 : (res >= deadDiff / 2 ? 1 : 0));
                }
                if (mainCh >= 200 && o1 >= impurityThr && o2 >= impurityThr)
                    markHit(x, y, 2);
            }
        }
    }

    // 异色黑点/白斑：仅在坏点内缩圆内；大片洪水只留强种子邻域
    if (markedN < kMaxDeadPixelsDetect && expectedColor >= 0) {
        const int inset = qMax(8, dead.r / 10);
        ScreenCircle core = dead;
        core.r = qMax(8, dead.r - inset);
        if (core.r >= 12) {
            QVector<QPoint> foreignSeeds;
            QVector<int> foreignSeedStr;
            for (int y = r.top() + 1; y <= r.bottom() - 1 && markedN < kMaxDeadPixelsDetect; ++y) {
                const uchar* line = img.constScanLine(y);
                for (int x = r.left() + 1; x <= r.right() - 1 && markedN < kMaxDeadPixelsDetect; ++x) {
                    if (!core.contains(x, y))
                        continue;
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
                    if (expectedColor <= 2 && solidMean < 50)
                        continue;
                    // 真黑点通常很暗或与邻域差很大；摩尔暗带一般达不到
                    const bool strongBlack = g <= 25 || (solidMean - g) >= deadDiff + 50;
                    const bool softBlack = g <= 40 || (solidMean - g) >= deadDiff + 25;
                    const bool whiteSpot = (g >= 175 && spread <= 55) || (pr >= 200 && pg >= 100 && pb >= 100);
                    if (!softBlack && !whiteSpot)
                        continue;
                    foreignSeeds.append(QPoint(x, y));
                    foreignSeedStr.append(strongBlack || whiteSpot ? 2 : 1);
                    // 种子过多时不必再扫整圆，后续涨点也会很快触顶
                    if (foreignSeeds.size() >= kMaxDeadPixelsDetect)
                        break;
                }
                if (foreignSeeds.size() >= kMaxDeadPixelsDetect)
                    break;
            }

            for (int si = 0; si < foreignSeeds.size() && markedN < kMaxDeadPixelsDetect; ++si) {
                const QPoint seed = foreignSeeds.at(si);
                const int seedStrength = foreignSeedStr.at(si);
                const int sg = gray[seed.y() * w + seed.x()];
                const bool seedDark = sg <= 80;
                QVector<QPoint> stack;
                QVector<quint8> visited(mark.size(), 0);
                int grown = 0;
                stack.append(seed);
                while (!stack.isEmpty() && markedN < kMaxDeadPixelsDetect) {
                    const QPoint p = stack.takeLast();
                    if (!core.contains(p.x(), p.y()))
                        continue;
                    const int idx = p.y() * w + p.x();
                    if (visited[idx])
                        continue;
                    visited[idx] = 1;
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
                        seedDark ? (g <= 55)
                                 : ((g >= 170 && spread <= 60) || (pr >= 190 && pg >= 95 && pb >= 95));
                    if (!sameKind)
                        continue;
                    const int str = (p == seed) ? seedStrength : 1;
                    markHit(p.x(), p.y(), str);
                    ++grown;
                    // 纹路连片过大则停止扩散，种子强点已写入
                    if (grown > maxBlob * 3)
                        break;
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
    finalizeDeadMarks(mark, w, h, r, maxBlob, kMaxDeadPixelsDetect, out);
    return out;
}

QImage drawAnnotated(const QImage& rgb, const QRect& roi, const ScreenCircle& circle, const QVector<QPoint>& pts) {
    // 标注仅供预览/归档，全分辨率转 RGB32 很慢；长边压到 1600
    constexpr int kAnnoMaxSide = 1600;
    QImage src = rgb;
    QRect drawRoi = roi;
    ScreenCircle drawCircle = circle;
    QVector<QPoint> drawPts = pts;
    const int maxSide = qMax(src.width(), src.height());
    if (maxSide > kAnnoMaxSide && src.width() > 0 && src.height() > 0) {
        const QImage scaled =
            src.scaled(kAnnoMaxSide, kAnnoMaxSide, Qt::KeepAspectRatio, Qt::FastTransformation);
        const int sw = scaled.width();
        const int sh = scaled.height();
        drawRoi = QRect(roi.x() * sw / src.width(), roi.y() * sh / src.height(),
                        qMax(1, roi.width() * sw / src.width()), qMax(1, roi.height() * sh / src.height()));
        drawCircle.cx = circle.cx * sw / src.width();
        drawCircle.cy = circle.cy * sh / src.height();
        drawCircle.r = qMax(1, circle.r * qMin(sw, sh) / qMin(src.width(), src.height()));
        for (QPoint& pt : drawPts) {
            pt.setX(pt.x() * sw / src.width());
            pt.setY(pt.y() * sh / src.height());
        }
        src = scaled;
    }
    QImage out = src.convertToFormat(QImage::Format_RGB32);
    QPainter p(&out);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(QColor(0, 200, 0), 2));
    p.drawRect(drawRoi.adjusted(0, 0, -1, -1));
    if (drawCircle.r >= 8)
        p.drawEllipse(QPoint(drawCircle.cx, drawCircle.cy), drawCircle.r, drawCircle.r);
    p.setPen(QPen(QColor(220, 0, 0), 2));
    for (const QPoint& pt : drawPts)
        p.drawEllipse(pt, 6, 6);
    p.end();
    return out;
}

QRect scaleRoiRect(const QRect& roi, const QSize& from, const QSize& to) {
    if (from.width() <= 0 || from.height() <= 0 || to.width() <= 0 || to.height() <= 0)
        return roi;
    return QRect(roi.x() * to.width() / from.width(), roi.y() * to.height() / from.height(),
                 qMax(1, roi.width() * to.width() / from.width()),
                 qMax(1, roi.height() * to.height() / from.height()));
}

} // namespace

namespace ScreenInspectAnalyzer {

QImage drawGuides(const QImage& rgb, const QRect& roiIn, const QImage* circleFrom, QRect* outRoi) {
    const QImage img = toRgb888(rgb);
    QRect roi = roiIn.intersected(img.rect());
    if (roi.width() < 10 || roi.height() < 10)
        roi = detectScreenRoi(img);
    if (outRoi)
        *outRoi = roi;

    ScreenCircle circle;
    if (circleFrom && !circleFrom->isNull()) {
        const QImage src = toRgb888(*circleFrom);
        QRect srcRoi = scaleRoiRect(roi, img.size(), src.size()).intersected(src.rect());
        if (srcRoi.width() < 10 || srcRoi.height() < 10)
            srcRoi = detectScreenRoi(src);
        const ScreenCircle sc = detectScreenCircle(src, srcRoi);
        // 把源图圆映射到当前图，保证左右对照是同一套校准线
        if (sc.r >= 8 && src.width() > 0 && src.height() > 0) {
            circle.cx = sc.cx * img.width() / src.width();
            circle.cy = sc.cy * img.height() / src.height();
            circle.r = qMax(8, sc.r * qMin(img.width(), img.height()) / qMin(src.width(), src.height()));
        } else {
            circle = detectScreenCircle(img, roi);
        }
    } else {
        circle = detectScreenCircle(img, roi);
    }
    return drawAnnotated(img, roi, circle, {});
}

Report analyze(const QImage& currRgb, const QImage& refRgb, const Params& p) {
    QElapsedTimer totalT;
    totalT.start();
    QElapsedTimer stepT;
    Report report;
    stepT.start();
    const QImage curr = toRgb888(currRgb);
    const qint64 msRgb = stepT.elapsed();

    stepT.start();
    // 已划定 ROI 时跳过全图自动框（千万像素直方图很慢）
    QRect roi = p.manualRoi.intersected(curr.rect());
    if (roi.width() < 10 || roi.height() < 10)
        roi = detectScreenRoi(curr);
    report.roi = roi;
    const ScreenCircle circle = detectScreenCircle(curr, roi);
    const qint64 msRoi = stepT.elapsed();

    stepT.start();
    int expected = p.expectedColor;
    const int detected = guessExpectedColor(curr, roi, circle);
    report.detectedColor = detected;
    if (expected < 0)
        expected = detected;
    report.expectedColorUsed = expected;
    report.colorMatch = p.expectedColor < 0 ? -1 : (detected == p.expectedColor ? 1 : 0);
    const qint64 msColor = stepT.elapsed();

    DeadScan dead;
    qint64 msDead = 0;
    if (p.enableDeadPixels) {
        stepT.start();
        dead = scanDeadPixels(curr, roi, p.deadDiff, expected, circle, p.deadRadiusPercent);
        report.deadPixels = dead.count;
        report.muraStd = dead.muraStd;
        msDead = stepT.elapsed();
    }

    stepT.start();
    report.annotated = drawAnnotated(curr, roi, circle, dead.points);
    const qint64 msAnno = stepT.elapsed();

    qint64 msSsim = 0;
    if (p.enableSsim && !refRgb.isNull()) {
        stepT.start();
        QImage ref = toRgb888(refRgb);
        if (ref.size() != curr.size())
            ref = ref.scaled(curr.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        // 完整识别圆屏：以外接方框裁切对比（四角黑底一致，不再内缩、不按块跳过）
        QRect circleBox(circle.cx - circle.r, circle.cy - circle.r, circle.r * 2, circle.r * 2);
        circleBox = circleBox.intersected(curr.rect());
        if (circleBox.width() < 8 || circleBox.height() < 8)
            circleBox = roi;
        QImage a = curr.copy(circleBox);
        QImage b = ref.copy(circleBox);
        const int maxSide = 256;
        if (a.width() > maxSide || a.height() > maxSide) {
            a = a.scaled(maxSide, maxSide, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            b = b.scaled(a.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        report.ssim = ssimOnGray(toGrayBytes(a), toGrayBytes(b), a.width(), a.height());
        msSsim = stepT.elapsed();
    }
    qDebug().noquote() << QStringLiteral("[ScreenInspectAnalyze]")
                       << QStringLiteral("size=%1x%2 rgb=%3 roi=%4 circle=%5,%6 r=%7 color=%8 dead=%9 anno=%10 ssim=%11 total=%12")
                              .arg(curr.width())
                              .arg(curr.height())
                              .arg(msRgb)
                              .arg(msRoi)
                              .arg(circle.cx)
                              .arg(circle.cy)
                              .arg(circle.r)
                              .arg(msColor)
                              .arg(msDead)
                              .arg(msAnno)
                              .arg(msSsim)
                              .arg(totalT.elapsed());
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

void cleanupStoredImages(const QString& dirPath, int keepNewest, int maxAgeDays) {
    QDir dir(dirPath);
    if (!dir.exists())
        return;
    if (keepNewest < 0)
        keepNewest = 0;
    if (maxAgeDays < 1)
        maxAgeDays = 1;

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 maxAgeMs = static_cast<qint64>(maxAgeDays) * 24 * 3600 * 1000;
    // 仅枚举根目录文件，不递归；参考图目录内文件不会出现在此列表
    const QString refDirName = QStringLiteral("参考图");

    struct Entry {
        QString path;
        qint64 mtime = 0;
    };
    QVector<Entry> stamped;
    stamped.reserve(64);

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : files) {
        const QString name = fi.fileName();
        // 防御：根目录旧版单张参考图、最近一次预览
        if (name.compare(QStringLiteral("reference.png"), Qt::CaseInsensitive) == 0
            || name.startsWith(QStringLiteral("last_"), Qt::CaseInsensitive))
            continue;
        // 双保险：路径落在参考图目录下绝不删
        const QString abs = QDir::cleanPath(fi.absoluteFilePath());
        if (abs.contains(QLatin1Char('/') + refDirName + QLatin1Char('/'))
            || abs.contains(QLatin1Char('\\') + refDirName + QLatin1Char('\\'))
            || abs.endsWith(QLatin1Char('/') + refDirName) || abs.endsWith(QLatin1Char('\\') + refDirName))
            continue;

        const bool isStamp = name.contains(QStringLiteral("_capture"), Qt::CaseInsensitive)
                             || name.contains(QStringLiteral("_mark"), Qt::CaseInsensitive)
                             || name.contains(QStringLiteral("_reference"), Qt::CaseInsensitive);
        if (!isStamp)
            continue;

        const qint64 mt = fi.lastModified().toMSecsSinceEpoch();
        if (nowMs - mt > maxAgeMs) {
            QFile::remove(fi.absoluteFilePath());
            continue;
        }
        stamped.append({fi.absoluteFilePath(), mt});
    }

    std::sort(stamped.begin(), stamped.end(),
              [](const Entry& a, const Entry& b) { return a.mtime > b.mtime; });
    for (int i = keepNewest; i < stamped.size(); ++i)
        QFile::remove(stamped.at(i).path);
}

QString storageRootDir() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
}

QString referenceLibraryDir() {
    return QDir(storageRootDir()).filePath(QStringLiteral("参考图"));
}

QString importReferenceToLibrary(const QString& sourcePath, QString* errorOut) {
    const QString src = sourcePath.trimmed();
    if (src.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("参考图路径为空");
        return {};
    }
    QFileInfo srcInfo(src);
    if (!srcInfo.isAbsolute())
        srcInfo.setFile(QDir(QCoreApplication::applicationDirPath()).filePath(src));
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        if (errorOut)
            *errorOut = QStringLiteral("参考图文件不存在：") + srcInfo.absoluteFilePath();
        return {};
    }

    const QString libDir = referenceLibraryDir();
    if (!QDir().mkpath(libDir)) {
        if (errorOut)
            *errorOut = QStringLiteral("无法创建参考图目录：") + libDir;
        return {};
    }

    const QString srcAbs = QDir::cleanPath(srcInfo.absoluteFilePath());
    const QString libAbs = QDir::cleanPath(libDir);
    // 已在参考图目录内：直接返回相对路径，不再复制
    if (srcAbs.startsWith(libAbs + QLatin1Char('/'), Qt::CaseInsensitive)
        || srcAbs.startsWith(libAbs + QLatin1Char('\\'), Qt::CaseInsensitive)
        || srcAbs.compare(libAbs, Qt::CaseInsensitive) == 0) {
        const QString app = QDir::cleanPath(QCoreApplication::applicationDirPath());
        if (srcAbs.startsWith(app, Qt::CaseInsensitive))
            return QDir(app).relativeFilePath(srcAbs).replace(QLatin1Char('\\'), QLatin1Char('/'));
        return srcAbs;
    }

    QString baseName = srcInfo.fileName();
    if (baseName.isEmpty())
        baseName = QStringLiteral("reference.png");
    QString destAbs = QDir(libDir).filePath(baseName);
    if (QFileInfo::exists(destAbs)) {
        // 同名已存在：内容相同则复用；不同则加时间戳避免覆盖
        QFile fa(srcAbs), fb(destAbs);
        bool same = false;
        if (fa.open(QIODevice::ReadOnly) && fb.open(QIODevice::ReadOnly))
            same = (fa.readAll() == fb.readAll());
        if (!same) {
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
            destAbs = QDir(libDir).filePath(srcInfo.completeBaseName() + QLatin1Char('_') + stamp + QLatin1Char('.')
                                            + srcInfo.suffix());
        }
    }
    if (!QFileInfo::exists(destAbs)) {
        if (QFile::exists(destAbs))
            QFile::remove(destAbs);
        if (!QFile::copy(srcAbs, destAbs)) {
            if (errorOut)
                *errorOut = QStringLiteral("复制参考图失败：") + destAbs;
            return {};
        }
    }

    const QString app = QDir::cleanPath(QCoreApplication::applicationDirPath());
    const QString cleanDest = QDir::cleanPath(destAbs);
    if (cleanDest.startsWith(app, Qt::CaseInsensitive))
        return QDir(app).relativeFilePath(cleanDest).replace(QLatin1Char('\\'), QLatin1Char('/'));
    return cleanDest;
}

} // namespace ScreenInspectAnalyzer

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
