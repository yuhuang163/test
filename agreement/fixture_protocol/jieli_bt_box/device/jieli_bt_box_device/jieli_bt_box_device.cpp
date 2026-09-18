#include "jieli_bt_box_device.h"

#include "qfixturemanager.h"
#include "serial_channel.h"

#include <QDebug>
#include <QElapsedTimer>
#include <functional>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static bool waitForRfInfoWithChunk(const std::function<bool(QByteArray*, int)>& waitChunk, bool isOpen,
                                   const QString& notOpenMsg, int timeoutMs, JieliBtBoxRfInfo* out,
                                   QString* errorMessage, int expectedSide, const QByteArray& expectedMac,
                                   const std::function<bool()>& shouldContinue) {
    if (!out) {
        if (errorMessage)
            *errorMessage = QStringLiteral("输出为空");
        return false;
    }
    *out = JieliBtBoxRfInfo{};
    if (!isOpen) {
        if (errorMessage)
            *errorMessage = notOpenMsg;
        return false;
    }

    const bool filterMac = expectedMac.size() == 6;
    const bool filterSide = expectedSide == 0 || expectedSide == 1;
    const int waitMs = qMax(1, timeoutMs);
    // 不先清缓冲：上电步骤后盒子可能已发出 RF 帧，清掉会丢掉刚到的包
    QByteArray stream;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < waitMs) {
        if (shouldContinue && !shouldContinue()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("测试已停止");
            return false;
        }
        for (;;) {
            QByteArray frame;
            int consumed = 0;
            if (!JieliBtBoxCodec::tryExtractFrame(stream, &frame, &consumed))
                break;
            if (consumed > 0)
                stream.remove(0, consumed);

            QString parseErr;
            JieliBtBoxRfInfo info;
            if (!JieliBtBoxCodec::parseFrame(frame, &info, &parseErr)) {
                if (errorMessage)
                    *errorMessage = parseErr;
                qDebug().noquote() << "JIELI_BT_BOX parse fail:" << parseErr;
                continue;
            }
            if (!info.hasFreqOffset || !info.hasRssi) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("帧内频偏或 RSSI 字段缺失");
                continue;
            }

            bool accept = true;
            if (filterMac)
                accept = info.hasMac && info.mac == expectedMac;
            else if (filterSide)
                accept = ((info.isRight ? 1 : 0) == expectedSide);
            if (!accept) {
                QString reason;
                if (filterMac)
                    reason = QStringLiteral("本窗期望MAC %1")
                                 .arg(QString::fromLatin1(expectedMac.toHex('-')).toUpper());
                else if (filterSide)
                    reason = QStringLiteral("本窗期望%1").arg(expectedSide == 1 ? QStringLiteral("右耳")
                                                                               : QStringLiteral("左耳"));
                qDebug().noquote() << QStringLiteral("JIELI_BT_BOX skip %1 mac=%2 freq=%3 rssi=%4 (%5)")
                                          .arg(info.isRight ? QStringLiteral("右耳") : QStringLiteral("左耳"))
                                          .arg(QString::fromLatin1(info.mac.toHex('-')).toUpper())
                                          .arg(info.freqOffset)
                                          .arg(info.rssi)
                                          .arg(reason);
                continue;
            }

            *out = info;
            qDebug().noquote() << QStringLiteral("JIELI_BT_BOX OK %1 freq=%2 rssi=%3")
                                      .arg(out->isRight ? QStringLiteral("右耳") : QStringLiteral("左耳"))
                                      .arg(out->freqOffset)
                                      .arg(out->rssi);
            return true;
        }

        const int remainMs = waitMs - static_cast<int>(timer.elapsed());
        if (remainMs <= 0)
            break;
        QByteArray chunk;
        const int chunkWaitMs = qMin(remainMs, 200);
        if (!waitChunk(&chunk, chunkWaitMs)) {
            if (shouldContinue && !shouldContinue()) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("测试已停止");
                return false;
            }
            continue;
        }
        if (chunk.isEmpty())
            continue;
        stream += chunk;
        qDebug().noquote() << "JIELI_BT_BOX RX:" << QString::fromLatin1(chunk.toHex(' ').toUpper());
    }

    if (errorMessage) {
        if (filterMac)
            *errorMessage = QStringLiteral("等待本窗 MAC %1 的频偏/RSSI 上报超时")
                                .arg(QString::fromLatin1(expectedMac.toHex('-')).toUpper());
        else if (filterSide)
            *errorMessage = QStringLiteral("等待%1频偏/RSSI 上报超时")
                                .arg(expectedSide == 1 ? QStringLiteral("右耳") : QStringLiteral("左耳"));
        else if (errorMessage->isEmpty() || *errorMessage == QStringLiteral("等待杰理频偏/RSSI 上报超时"))
            *errorMessage = QStringLiteral("等待杰理频偏/RSSI 上报超时");
    }
    return false;
}

bool JieliBtBoxDevice::waitForRfInfo(SerialChannel* channel, int timeoutMs, JieliBtBoxRfInfo* out,
                                     QString* errorMessage, int expectedSide, const QByteArray& expectedMac,
                                     const std::function<bool()>& shouldContinue) {
    return waitForRfInfoWithChunk(
        [channel](QByteArray* chunk, int remainMs) {
            return channel && channel->waitForFrame(chunk, remainMs);
        },
        channel && channel->isOpen(), QStringLiteral("产品串口(仪器)未打开"), timeoutMs, out, errorMessage,
        expectedSide, expectedMac, shouldContinue);
}

bool JieliBtBoxDevice::waitForRfInfo(QFixtureManager* fixture, int timeoutMs, JieliBtBoxRfInfo* out,
                                     QString* errorMessage, int expectedSide, const QByteArray& expectedMac,
                                     const std::function<bool()>& shouldContinue) {
    return waitForRfInfoWithChunk(
        [fixture](QByteArray* chunk, int remainMs) {
            return fixture && fixture->waitForRawFrame(chunk, remainMs);
        },
        fixture && fixture->isOpen(), QStringLiteral("请先用顶部菜单「连接治具串口」连接杰理盒子"), timeoutMs, out,
        errorMessage, expectedSide, expectedMac, shouldContinue);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
