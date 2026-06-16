#include "root_ble_ota2.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>

#include "common_utils.h"
#include "common_protocl/comm_protocol_defs.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr uint16_t kFrameSof = 0x5CC5u;
constexpr uint8_t kFrameTypeReq = 0x00;
constexpr uint8_t kFrameTypeResp = 0x01;
constexpr uint8_t kFrameTypeNotify = 0x02;
constexpr uint8_t kObjectTypeImageResource = 0x02;

void waitWork(int ms) {
    if (ms <= 0)
        return;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents);
}

} // namespace

void RootBleOta2Client::reset() {
    rxBuffer_.clear();
    responses_.clear();
    statusNotifies_.clear();
    seq_ = 0;
}

void RootBleOta2Client::onRx(const QByteArray& data) {
    if (data.isEmpty())
        return;
    rxBuffer_.append(data);
    drainRxBuffer();
}

uint32_t RootBleOta2Client::calculateImageCrc32(const QByteArray& imageData) {
    return CommonUtils::crc32(imageData);
}

QByteArray RootBleOta2Client::appendTlvU32(uint16_t type, uint32_t value) {
    QByteArray tlv;
    CommonUtils::appendLe16(&tlv, type);
    CommonUtils::appendLe16(&tlv, 4);
    CommonUtils::appendLe32(&tlv, value);
    return tlv;
}

QByteArray RootBleOta2Client::appendTlvU8(uint16_t type, uint8_t value) {
    QByteArray tlv;
    CommonUtils::appendLe16(&tlv, type);
    CommonUtils::appendLe16(&tlv, 1);
    tlv.append(static_cast<char>(value));
    return tlv;
}

QByteArray RootBleOta2Client::appendTlvU16(uint16_t type, uint16_t value) {
    QByteArray tlv;
    CommonUtils::appendLe16(&tlv, type);
    CommonUtils::appendLe16(&tlv, 2);
    CommonUtils::appendLe16(&tlv, value);
    return tlv;
}

QByteArray RootBleOta2Client::appendTlvEmpty(uint16_t type) {
    QByteArray tlv;
    CommonUtils::appendLe16(&tlv, type);
    CommonUtils::appendLe16(&tlv, 0);
    return tlv;
}

QByteArray RootBleOta2Client::appendTlvBytes(uint16_t type, const QByteArray& value) {
    QByteArray tlv;
    CommonUtils::appendLe16(&tlv, type);
    CommonUtils::appendLe16(&tlv, static_cast<quint16>(value.size()));
    tlv.append(value);
    return tlv;
}

QByteArray RootBleOta2Client::buildOtaServicePayload(const QByteArray& tlvBlob) {
    QByteArray payload;
    CommonUtils::appendLe16(&payload, kOtaServiceId);
    CommonUtils::appendLe16(&payload, static_cast<quint16>(tlvBlob.size()));
    payload.append(tlvBlob);
    return payload;
}

QByteArray RootBleOta2Client::buildFrame(uint8_t& seq, uint8_t frameType, const QByteArray& payload) {
    QByteArray header;
    CommonUtils::appendLe16(&header, kFrameSof);
    header.append(static_cast<char>(seq++));
    header.append(static_cast<char>(frameType));
    CommonUtils::appendLe16(&header, static_cast<quint16>(payload.size()));

    QByteArray frame = header;
    frame.append(payload);

    const quint16 crc = CRC16(frame.constData(), static_cast<unsigned int>(frame.size()));
    CommonUtils::appendLe16(&frame, crc);
    return frame;
}

uint16_t RootBleOta2Client::readTlvU16(const FrameResponse& resp, uint16_t type, uint16_t defaultValue) {
    const QByteArray value = resp.tlvs.value(type);
    if (value.size() < 2)
        return defaultValue;
    return CommonUtils::readLe16(value, 0);
}

uint32_t RootBleOta2Client::readTlvU32(const FrameResponse& resp, uint16_t type, uint32_t defaultValue) {
    const QByteArray value = resp.tlvs.value(type);
    if (value.size() < 4)
        return defaultValue;
    return CommonUtils::readLe32(value, 0);
}

void RootBleOta2Client::drainRxBuffer() {
    while (rxBuffer_.size() >= 8) {
        int sofPos = -1;
        for (int i = 0; i + 1 < rxBuffer_.size(); ++i) {
            if (static_cast<quint8>(rxBuffer_[i]) == 0xC5 && static_cast<quint8>(rxBuffer_[i + 1]) == 0x5C) {
                sofPos = i;
                break;
            }
        }
        if (sofPos < 0) {
            rxBuffer_.clear();
            return;
        }
        if (sofPos > 0)
            rxBuffer_.remove(0, sofPos);
        if (rxBuffer_.size() < 8)
            return;

        const quint16 payloadLen = CommonUtils::readLe16(rxBuffer_, 4);
        const int frameLen = 6 + payloadLen + 2;
        if (rxBuffer_.size() < frameLen)
            return;

        const QByteArray frame = rxBuffer_.left(frameLen);
        rxBuffer_.remove(0, frameLen);

        const quint16 expectCrc = CommonUtils::readLe16(frame, 6 + payloadLen);
        const quint16 actualCrc = CRC16(frame.constData(), static_cast<unsigned int>(6 + payloadLen));
        if (expectCrc != actualCrc)
            continue;

        const uint8_t seq = static_cast<uint8_t>(frame[2]);
        const uint8_t frameType = static_cast<uint8_t>(frame[3]);
        const QString rxLog =
            QStringLiteral("BLE OTA2 接收 seq=%1 frameType=0x%2 len=%3 data=%4")
                .arg(seq)
                .arg(frameType, 2, 16, QLatin1Char('0'))
                .arg(frame.size())
                .arg(QString::fromLatin1(frame.toHex(' ').toUpper()));
        qDebug() << rxLog;
        if (logFunc_)
            logFunc_(rxLog);

        parseOneFrame(frame);
    }
}

void RootBleOta2Client::parseServiceBody(uint16_t serviceId, const QByteArray& body, uint8_t seq,
                                         uint8_t frameType) {
    QHash<uint16_t, QByteArray> tlvs;
    int off = 0;
    while (off + 4 <= body.size()) {
        const uint16_t tlvType = CommonUtils::readLe16(body, off);
        const quint16 tlvLen = CommonUtils::readLe16(body, off + 2);
        off += 4;
        if (off + tlvLen > body.size())
            break;
        tlvs.insert(tlvType, body.mid(off, tlvLen));
        off += tlvLen;
    }

    if (serviceId == kTestServiceId && frameType == kFrameTypeNotify) {
        if (tlvs.contains(UpgradeStatusNotify)) {
            const QByteArray notifyValue = tlvs.value(UpgradeStatusNotify);
            const uint16_t code =
                notifyValue.size() >= 2 ? CommonUtils::readLe16(notifyValue, 0) : static_cast<uint16_t>(0xFFFF);
            statusNotifies_.enqueue(code);
        }
        return;
    }

    if (serviceId != kOtaServiceId)
        return;
    if (frameType != kFrameTypeResp && frameType != kFrameTypeNotify)
        return;

    FrameResponse resp;
    resp.seq = seq;
    resp.tlvs = tlvs;
    responses_.enqueue(resp);
}

void RootBleOta2Client::parseOneFrame(const QByteArray& frame) {
    if (frame.size() < 8)
        return;

    const uint8_t seq = static_cast<uint8_t>(frame[2]);
    const uint8_t frameType = static_cast<uint8_t>(frame[3]);
    const int payloadLen = static_cast<int>(CommonUtils::readLe16(frame, 4));
    const QByteArray payload = frame.mid(6, payloadLen);
    if (payload.size() < 4)
        return;

    int off = 0;
    while (off + 4 <= payload.size()) {
        const uint16_t serviceId = CommonUtils::readLe16(payload, off);
        const quint16 serviceLen = CommonUtils::readLe16(payload, off + 2);
        off += 4;
        if (off + serviceLen > payload.size())
            break;
        parseServiceBody(serviceId, payload.mid(off, serviceLen), seq, frameType);
        off += serviceLen;
    }
}

bool RootBleOta2Client::tryTakeResponse(uint8_t seq, FrameResponse* out) {
    for (int i = 0; i < responses_.size(); ++i) {
        if (responses_.at(i).seq != seq)
            continue;
        if (out)
            *out = responses_.at(i);
        responses_.removeAt(i);
        return true;
    }
    return false;
}

bool RootBleOta2Client::tryTakeStatusNotify(uint16_t* errorCode) {
    if (statusNotifies_.isEmpty())
        return false;
    if (errorCode)
        *errorCode = statusNotifies_.dequeue();
    else
        statusNotifies_.dequeue();
    return true;
}

void RootBleOta2Client::sendFrameChunked(const QByteArray& frame) {
    if (!sendFunc_ || frame.isEmpty())
        return;
    const int step = qMax(1, uartChunkSize_);
    for (int off = 0; off < frame.size(); off += step) {
        const int pieceLen = qMin(step, frame.size() - off);
        sendFunc_(frame.mid(off, pieceLen));
        if (off + pieceLen < frame.size())
            waitWork(transferIntervalMs_);
    }
}

bool RootBleOta2Client::sendOtaRequest(const QByteArray& tlvBlob, uint8_t* outSeq) {
    if (!sendFunc_)
        return false;
    const QByteArray payload = buildOtaServicePayload(tlvBlob);
    const uint8_t sendSeq = seq_;
    const QByteArray frame = buildFrame(seq_, kFrameTypeReq, payload);
    // qDebug() << QStringLiteral("BLE OTA2 发送 seq=%1 len=%2 data=%3")
    //                 .arg(sendSeq)
    //                 .arg(frame.size())
    //                 .arg(QString::fromLatin1(frame.toHex(' ').toUpper()));
    sendFrameChunked(frame);
    if (outSeq)
        *outSeq = sendSeq;
    return true;
}

bool RootBleOta2Client::waitResponse(uint8_t expectSeq, FrameResponse* out, int timeoutMs,
                                     CancelPredicate cancelled) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cancelled && cancelled())
            return false;

        uint16_t notifyCode = 0;
        if (tryTakeStatusNotify(&notifyCode)) {
            if (logFunc_)
                logFunc_(QStringLiteral("收到升级异常通知 0x%1").arg(notifyCode, 4, 16, QLatin1Char('0')));
            return false;
        }

        if (tryTakeResponse(expectSeq, out))
            return true;

        // 勿用 QThread::msleep：会阻塞事件循环，串口 onRx 延迟导致块间多出 ~10–30ms
        waitWork(1);
    }
    return false;
}

bool RootBleOta2Client::startUpgrade(uint32_t imageId, uint32_t version, const QByteArray& imageData,
                                     uint32_t* outOffset, CancelPredicate cancelled, QString* errorOut) {
    const uint32_t totalSize = static_cast<uint32_t>(imageData.size());
    const uint32_t imageCrc32 = calculateImageCrc32(imageData);

    QByteArray tlvs;
    tlvs.append(appendTlvU32(ImgId, imageId));
    tlvs.append(appendTlvU32(Version, version));
    tlvs.append(appendTlvU32(Crc32, imageCrc32));
    tlvs.append(appendTlvU32(TotalSize, totalSize));
    tlvs.append(appendTlvU8(Type, kObjectTypeImageResource));
    tlvs.append(appendTlvEmpty(StartUpgradeRequest));

    uint8_t sendSeq = 0;
    if (!sendOtaRequest(tlvs, &sendSeq)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送开始升级请求失败");
        return false;
    }

    FrameResponse resp;
    if (!waitResponse(sendSeq, &resp, kUpgradeControlTimeoutMs, cancelled)) {
        if (errorOut)
            *errorOut = QStringLiteral("等待开始升级应答超时或收到异常通知");
        return false;
    }

    const uint16_t serviceErr = readTlvU16(resp, ServiceErrorCode, 0xFFFF);
    if (serviceErr != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("开始升级 F000=0x%1").arg(serviceErr, 4, 16, QLatin1Char('0'));
        return false;
    }

    const uint32_t offset = readTlvU32(resp, ImgOffsetAddr, 0);
    const uint16_t blockSize = readTlvU16(resp, BlockSize, 0);
    if (blockSize != 0 && blockSize != kBlockSize && logFunc_) {
        logFunc_(QStringLiteral("设备返回块大小 %1，仍按 %2 发送").arg(blockSize).arg(kBlockSize));
    }
    *outOffset = offset;
    if (logFunc_) {
        logFunc_(QStringLiteral("开始升级成功，续传偏移=%1，块大小=%2").arg(offset).arg(blockSize ? blockSize : kBlockSize));
    }
    return true;
}

bool RootBleOta2Client::writeBlock(uint32_t offset, const QByteArray& chunk, CancelPredicate cancelled,
                                   QString* errorOut) {
    QByteArray tlvs;
    tlvs.append(appendTlvU32(ImgOffsetAddr, offset));
    tlvs.append(appendTlvBytes(WriteImgData, chunk));

    uint8_t sendSeq = 0;
    if (!sendOtaRequest(tlvs, &sendSeq)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送写数据请求失败 offset=%1").arg(offset);
        return false;
    }

    QElapsedTimer blockRtt;
    blockRtt.start(); // 整帧串口发完后才开始计 RTT
    FrameResponse resp;
    if (!waitResponse(sendSeq, &resp, kBlockResponseTimeoutMs, cancelled)) {
        if (errorOut)
            *errorOut = QStringLiteral("等待写数据应答超时 offset=%1").arg(offset);
        return false;
    }

    const uint16_t serviceErr = readTlvU16(resp, ServiceErrorCode, 0xFFFF);
    if (serviceErr != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("写数据 F000=0x%1 offset=%2").arg(serviceErr, 4, 16, QLatin1Char('0')).arg(offset);
        return false;
    }
    // 单调时钟：整帧串口发完 → 收到应答，与 dongle 内部戳 38585→38614 同语义
    if (logFunc_) {
        logFunc_(QStringLiteral("写块 offset=%1 seq=%2 发完→应答 %3ms")
                     .arg(offset)
                     .arg(sendSeq)
                     .arg(blockRtt.elapsed()));
    }
    return true;
}

bool RootBleOta2Client::endUpgrade(CancelPredicate cancelled, QString* errorOut) {
    const QByteArray tlvs = appendTlvEmpty(EndUpgradeRequest);

    uint8_t sendSeq = 0;
    if (!sendOtaRequest(tlvs, &sendSeq)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送结束升级请求失败");
        return false;
    }

    FrameResponse resp;
    if (!waitResponse(sendSeq, &resp, kUpgradeControlTimeoutMs, cancelled)) {
        if (errorOut)
            *errorOut = QStringLiteral("等待结束升级应答超时");
        return false;
    }

    const uint16_t serviceErr = readTlvU16(resp, ServiceErrorCode, 0xFFFF);
    const uint16_t upgradeResult = readTlvU16(resp, UpgradeResult, 0xFFFF);
    if (serviceErr != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("结束升级 F000=0x%1").arg(serviceErr, 4, 16, QLatin1Char('0'));
        return false;
    }
    if (upgradeResult != 0) {
        if (errorOut)
            *errorOut = QStringLiteral("结束升级失败 UPGRADE_RESULT=0x%1").arg(upgradeResult, 4, 16, QLatin1Char('0'));
        return false;
    }
    return true;
}

bool RootBleOta2Client::runTransfer(const QByteArray& imageData, uint32_t imageId, uint32_t version, int intervalMs,
                                    int uartChunkSize, CancelPredicate cancelled, QString* errorOut,
                                    ProgressFunc onProgress) {
    if (imageData.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("镜像数据为空");
        return false;
    }
    if (!sendFunc_) {
        if (errorOut)
            *errorOut = QStringLiteral("未设置发送回调");
        return false;
    }

    transferIntervalMs_ = qMax(0, intervalMs);
    uartChunkSize_ = qMax(1, uartChunkSize);

    reset();

    uint32_t offset = 0;
    if (!startUpgrade(imageId, version, imageData, &offset, cancelled, errorOut))
        return false;

    const int totalSize = imageData.size();
    if (onProgress)
        onProgress(CommonUtils::progressPercent(static_cast<int>(offset), totalSize));

    while (static_cast<int>(offset) < totalSize) {
        if (cancelled && cancelled())
            return false;

        const int chunkLen = qMin(kBlockSize, totalSize - static_cast<int>(offset));
        const QByteArray chunk = imageData.mid(static_cast<int>(offset), chunkLen);

        bool blockOk = false;
        for (int retry = 0; retry < kMaxWriteRetry && !blockOk; ++retry) {
            if (retry > 0 && logFunc_) {
                logFunc_(QStringLiteral("重发数据块 offset=%1，第 %2 次").arg(offset).arg(retry + 1));
            }
            QString blockErr;
            blockOk = writeBlock(offset, chunk, cancelled, &blockErr);
            if (!blockOk && errorOut)
                *errorOut = blockErr;
        }
        if (!blockOk)
            return false;

        offset += static_cast<uint32_t>(chunkLen);
        if (onProgress)
            onProgress(CommonUtils::progressPercent(static_cast<int>(offset), totalSize));
        // 块间收到设备应答后立即发下一块；transferIntervalMs_ 仅用于 sendFrameChunked 串口分片间隔
    }

    if (onProgress)
        onProgress(100);
    return endUpgrade(cancelled, errorOut);
}
