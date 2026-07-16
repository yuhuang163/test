#include "jieli_bt_box_device.h"

#include "serial_channel.h"

#include <QDebug>
#include <QElapsedTimer>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

bool JieliBtBoxDevice::waitForRfInfo(SerialChannel* channel, int timeoutMs, JieliBtBoxRfInfo* out,
                                     QString* errorMessage) {
    if (!out) {
        if (errorMessage)
            *errorMessage = QStringLiteral("输出为空");
        return false;
    }
    *out = JieliBtBoxRfInfo{};
    if (!channel || !channel->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("产品串口(仪器)未打开");
        return false;
    }

    const int waitMs = qMax(1, timeoutMs);
    // 不先清缓冲：上电步骤后盒子可能已发出 RF 帧，清掉会丢掉刚到的包
    QByteArray stream;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < waitMs) {
        const int remainMs = waitMs - static_cast<int>(timer.elapsed());
        if (remainMs <= 0)
            break;
        QByteArray chunk;
        // 已有流时短切片继续拼；首包用剩余超时
        const int sliceMs = stream.isEmpty() ? remainMs : qMin(remainMs, 80);
        if (!channel->waitForFrame(&chunk, sliceMs)) {
            if (!stream.isEmpty())
                break;
            continue;
        }
        if (chunk.isEmpty())
            continue;
        stream += chunk;
        qDebug().noquote() << "JIELI_BT_BOX RX:" << QString::fromLatin1(chunk.toHex(' ').toUpper());

        QByteArray frame;
        int consumed = 0;
        if (!JieliBtBoxCodec::tryExtractFrame(stream, &frame, &consumed))
            continue;
        if (consumed > 0)
            stream.remove(0, consumed);

        QString parseErr;
        if (!JieliBtBoxCodec::parseFrame(frame, out, &parseErr)) {
            if (errorMessage)
                *errorMessage = parseErr;
            qDebug().noquote() << "JIELI_BT_BOX parse fail:" << parseErr;
            continue;
        }
        if (!out->hasFreqOffset || !out->hasRssi) {
            if (errorMessage)
                *errorMessage = QStringLiteral("帧内频偏或 RSSI 字段缺失");
            continue;
        }
        qDebug().noquote() << QStringLiteral("JIELI_BT_BOX OK freq=%1 rssi=%2")
                                  .arg(out->freqOffset)
                                  .arg(out->rssi);
        return true;
    }

    if (errorMessage && (errorMessage->isEmpty() || *errorMessage == QStringLiteral("等待杰理频偏/RSSI 上报超时")))
        *errorMessage = QStringLiteral("等待杰理频偏/RSSI 上报超时");
    return false;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
