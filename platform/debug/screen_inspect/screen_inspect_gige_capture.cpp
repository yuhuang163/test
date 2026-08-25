#include "screen_inspect_gige_capture.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "MvCameraControl.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace ScreenInspectGigECapture {
namespace {

QMutex g_mvsMutex;
bool g_sdkInited = false;

void ensureRuntimeDllSearchPath() {
    static bool done = false;
    if (done)
        return;
    done = true;
    // 优先 exe 旁 mvs_runtime；其次 exe 目录本身（打包扁平拷贝）
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString nested = QDir(appDir).filePath(QStringLiteral("mvs_runtime"));
    const QString useDir = QDir(nested).exists() ? nested : appDir;
    SetDllDirectoryW(reinterpret_cast<LPCWSTR>(useDir.utf16()));
}

bool ensureSdk(QString* err) {
    ensureRuntimeDllSearchPath();
    if (g_sdkInited)
        return true;
    const int nRet = MV_CC_Initialize();
    if (nRet != MV_OK) {
        if (err)
            *err = QStringLiteral("初始化海康 MVS SDK 失败：0x%1").arg(nRet, 0, 16);
        return false;
    }
    g_sdkInited = true;
    return true;
}

QString ipFromGigEInfo(const MV_GIGE_DEVICE_INFO& info) {
    const unsigned int ip = info.nCurrentIp;
    return QStringLiteral("%1.%2.%3.%4")
        .arg((ip >> 24) & 0xff)
        .arg((ip >> 16) & 0xff)
        .arg((ip >> 8) & 0xff)
        .arg(ip & 0xff);
}

bool deviceMatches(const MV_CC_DEVICE_INFO* info, const QString& cameraIp, const QString& serial) {
    if (!info)
        return false;
    const QString wantIp = cameraIp.trimmed();
    const QString wantSn = serial.trimmed();
    if (info->nTLayerType == MV_GIGE_DEVICE || info->nTLayerType == MV_GENTL_GIGE_DEVICE) {
        const auto& g = info->SpecialInfo.stGigEInfo;
        if (!wantSn.isEmpty()) {
            const QString sn = QString::fromLocal8Bit(reinterpret_cast<const char*>(g.chSerialNumber));
            if (sn.compare(wantSn, Qt::CaseInsensitive) == 0)
                return true;
            if (!wantIp.isEmpty())
                return false;
        }
        if (!wantIp.isEmpty())
            return ipFromGigEInfo(g) == wantIp;
        return true;
    }
    return false;
}

bool frameToQImage(void* handle, const MV_FRAME_OUT& frame, QImage* out, QString* err) {
    const unsigned int w = frame.stFrameInfo.nExtendWidth
                               ? frame.stFrameInfo.nExtendWidth
                               : frame.stFrameInfo.nWidth;
    const unsigned int h = frame.stFrameInfo.nExtendHeight
                               ? frame.stFrameInfo.nExtendHeight
                               : frame.stFrameInfo.nHeight;
    if (w == 0 || h == 0 || !frame.pBufAddr) {
        if (err)
            *err = QStringLiteral("GigE 帧无效");
        return false;
    }

    const int stride = static_cast<int>(w) * 3;
    QByteArray rgb(stride * static_cast<int>(h), Qt::Uninitialized);
    MV_CC_PIXEL_CONVERT_PARAM_EX cvt;
    memset(&cvt, 0, sizeof(cvt));
    cvt.nWidth = w;
    cvt.nHeight = h;
    cvt.enSrcPixelType = frame.stFrameInfo.enPixelType;
    cvt.pSrcData = frame.pBufAddr;
    cvt.nSrcDataLen = frame.stFrameInfo.nFrameLen;
    cvt.enDstPixelType = PixelType_Gvsp_RGB8_Packed;
    cvt.pDstBuffer = reinterpret_cast<unsigned char*>(rgb.data());
    cvt.nDstBufferSize = static_cast<unsigned int>(rgb.size());

    const int nRet = MV_CC_ConvertPixelTypeEx(handle, &cvt);
    if (nRet != MV_OK) {
        if (err)
            *err = QStringLiteral("像素格式转换失败：0x%1").arg(nRet, 0, 16);
        return false;
    }
    const QImage img(reinterpret_cast<const uchar*>(rgb.constData()), static_cast<int>(w), static_cast<int>(h),
                     stride, QImage::Format_RGB888);
    *out = img.copy();
    return true;
}

} // namespace

bool grabStill(const QString& cameraIp, const QString& serial, int warmupMs, QImage* out, QString* err,
               QString* stageLog) {
    if (!out) {
        if (err)
            *err = QStringLiteral("内部错误：输出图为空");
        return false;
    }
    out->fill(Qt::black);
    warmupMs = qBound(0, warmupMs, 8000);

    QElapsedTimer totalT;
    totalT.start();
    QElapsedTimer stepT;
    qint64 msEnum = 0, msOpen = 0, msStart = 0, msWarm = 0, msDiscard = 0, msGet = 0, msCvt = 0;
    auto finishLog = [&](bool ok) {
        const QString line =
            QStringLiteral("ok=%1 thread=%2 enum=%3 open=%4 start=%5 warm=%6 discard=%7 get=%8 cvt=%9 "
                           "total=%10 size=%11x%12")
                .arg(ok ? 1 : 0)
                .arg(quintptr(QThread::currentThreadId()), 0, 16)
                .arg(msEnum)
                .arg(msOpen)
                .arg(msStart)
                .arg(msWarm)
                .arg(msDiscard)
                .arg(msGet)
                .arg(msCvt)
                .arg(totalT.elapsed())
                .arg(out->width())
                .arg(out->height());
        qDebug().noquote() << QStringLiteral("[ScreenInspectGigE]") << line;
        if (stageLog)
            *stageLog = line;
    };

    QMutexLocker lock(&g_mvsMutex);
    if (!ensureSdk(err)) {
        finishLog(false);
        return false;
    }

    void* handle = nullptr;
    bool opened = false;
    bool grabbing = false;
    auto cleanup = [&]() {
        if (grabbing)
            MV_CC_StopGrabbing(handle);
        if (opened)
            MV_CC_CloseDevice(handle);
        if (handle) {
            MV_CC_DestroyHandle(handle);
            handle = nullptr;
        }
    };

    MV_CC_DEVICE_INFO_LIST deviceList;
    memset(&deviceList, 0, sizeof(deviceList));
    stepT.start();
    int nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE | MV_GENTL_GIGE_DEVICE, &deviceList);
    msEnum = stepT.elapsed();
    if (nRet != MV_OK) {
        if (err)
            *err = QStringLiteral("枚举 GigE 相机失败：0x%1").arg(nRet, 0, 16);
        finishLog(false);
        return false;
    }
    if (deviceList.nDeviceNum == 0) {
        if (err)
            *err = QStringLiteral("未找到 GigE 相机（请检查网线/同网段 IP，MVS 能出图后再试）");
        finishLog(false);
        return false;
    }

    int index = -1;
    for (unsigned int i = 0; i < deviceList.nDeviceNum; ++i) {
        if (deviceMatches(deviceList.pDeviceInfo[i], cameraIp, serial)) {
            index = static_cast<int>(i);
            break;
        }
    }
    if (index < 0) {
        if (err) {
            QStringList ips;
            for (unsigned int i = 0; i < deviceList.nDeviceNum; ++i) {
                const auto* p = deviceList.pDeviceInfo[i];
                if (p && (p->nTLayerType == MV_GIGE_DEVICE || p->nTLayerType == MV_GENTL_GIGE_DEVICE))
                    ips << ipFromGigEInfo(p->SpecialInfo.stGigEInfo);
            }
            *err = QStringLiteral("未匹配 GigE 相机 IP/序列号（当前=%1，已发现：%2）")
                       .arg(cameraIp.trimmed().isEmpty() ? QStringLiteral("(空)") : cameraIp.trimmed(),
                            ips.join(QLatin1Char(',')));
        }
        finishLog(false);
        return false;
    }

    stepT.start();
    nRet = MV_CC_CreateHandle(&handle, deviceList.pDeviceInfo[index]);
    if (nRet != MV_OK) {
        if (err)
            *err = QStringLiteral("创建 GigE 句柄失败：0x%1").arg(nRet, 0, 16);
        finishLog(false);
        return false;
    }

    nRet = MV_CC_OpenDevice(handle);
    msOpen = stepT.elapsed();
    if (nRet != MV_OK) {
        cleanup();
        if (err)
            *err = QStringLiteral("打开 GigE 相机失败：0x%1（是否被 MVS 客户端占用？）").arg(nRet, 0, 16);
        finishLog(false);
        return false;
    }
    opened = true;

    if (deviceList.pDeviceInfo[index]->nTLayerType == MV_GIGE_DEVICE) {
        const int packetSize = MV_CC_GetOptimalPacketSize(handle);
        if (packetSize > 0)
            MV_CC_SetIntValueEx(handle, "GevSCPSPacketSize", packetSize);
    }
    MV_CC_SetEnumValue(handle, "TriggerMode", 0);

    stepT.start();
    nRet = MV_CC_StartGrabbing(handle);
    msStart = stepT.elapsed();
    if (nRet != MV_OK) {
        cleanup();
        if (err)
            *err = QStringLiteral("GigE 开始取流失败：0x%1").arg(nRet, 0, 16);
        finishLog(false);
        return false;
    }
    grabbing = true;

    stepT.start();
    if (warmupMs > 0)
        QThread::msleep(static_cast<unsigned long>(warmupMs));
    msWarm = stepT.elapsed();

    // 丢掉若干帧再取稳定画面
    stepT.start();
    for (int i = 0; i < 3; ++i) {
        MV_FRAME_OUT discard;
        memset(&discard, 0, sizeof(discard));
        if (MV_CC_GetImageBuffer(handle, &discard, 1000) == MV_OK)
            MV_CC_FreeImageBuffer(handle, &discard);
    }
    msDiscard = stepT.elapsed();

    MV_FRAME_OUT frame;
    memset(&frame, 0, sizeof(frame));
    stepT.start();
    nRet = MV_CC_GetImageBuffer(handle, &frame, 3000);
    msGet = stepT.elapsed();
    if (nRet != MV_OK) {
        cleanup();
        if (err)
            *err = QStringLiteral("GigE 取图超时/失败：0x%1").arg(nRet, 0, 16);
        finishLog(false);
        return false;
    }

    QImage converted;
    stepT.start();
    const bool ok = frameToQImage(handle, frame, &converted, err);
    msCvt = stepT.elapsed();
    MV_CC_FreeImageBuffer(handle, &frame);
    cleanup();

    if (!ok) {
        finishLog(false);
        return false;
    }
    *out = converted;
    finishLog(true);
    return true;
}

} // namespace ScreenInspectGigECapture

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
