#include "root_ble_ota.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

#include "common_utils.h"
#include "common_protocl/comm_protocol_defs.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr uint16_t kFrameSof = 0x5CC5u;
constexpr uint8_t kFrameTypeReq = 0x00;
constexpr uint8_t kFrameTypeResp = 0x01;
constexpr uint8_t kFrameTypeNotify = 0x02;
constexpr uint16_t kNackBusyBlock = 0xFFFFu;
constexpr uint8_t kNackReasonBusy = 0x01;
constexpr uint8_t kNackReasonReassembleFailed = 0x02;

/** 与 MainWindow::waitWork 相同：按毫秒等待并处理事件，避免 Windows 上 QThread::msleep(2) 被量化到 ~15ms */
void waitWork(int ms) {
    if (ms <= 0)
        return;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < ms)
        QCoreApplication::processEvents(QEventLoop::AllEvents);
}

}  // namespace

void RootBleOtaClient::reset() {
    rxBuffer_.clear();
    responses_.clear();
    seq_ = 0;
}

void RootBleOtaClient::onRx(const QByteArray& data) {
    if (data.isEmpty())
        return;
    rxBuffer_.append(data);
    drainRxBuffer();
}

bool RootBleOtaClient::tryTakeResponse(uint8_t type, QByteArray* outValue) {
    for (int i = 0; i < responses_.size(); ++i) {
        if (responses_.at(i).type != type)
            continue;
        if (outValue)
            *outValue = responses_.at(i).value;
        responses_.removeAt(i);
        return true;
    }
    return false;
}

uint32_t RootBleOtaClient::calculateImageCrc32(const QByteArray& imageData) {
    return CommonUtils::crc32(imageData);
}

QByteArray RootBleOtaClient::buildOtaPayload(uint8_t tlvType, const QByteArray& tlvValue) {
    QByteArray payload;
    payload.append(static_cast<char>(kServerId));
    payload.append(static_cast<char>(kServerIdLen));
    payload.append(static_cast<char>(tlvType));
    CommonUtils::appendLe16(&payload, static_cast<quint16>(tlvValue.size()));
    payload.append(tlvValue);
    return payload;
}

QByteArray RootBleOtaClient::buildFrame(uint8_t& seq, uint8_t frameType, const QByteArray& payload) {
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

void RootBleOtaClient::drainRxBuffer() {
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
        const int tlvType = frame.size() > 8 ? static_cast<uint8_t>(frame[8]) : -1;
        const QString rxLog =
            QStringLiteral("BLE OTA 接收数据包 type=0x%1 frameType=0x%2 seq=%3 len=%4 data=%5")
                .arg(tlvType, 2, 16, QLatin1Char('0'))
                .arg(frameType, 2, 16, QLatin1Char('0'))
                .arg(seq)
                .arg(frame.size())
                .arg(QString::fromLatin1(frame.toHex(' ').toUpper()));
        qDebug() << rxLog;
        if (logFunc_)
            logFunc_(rxLog);

        parseOneFrame(frame);
    }
}

bool RootBleOtaClient::parseOneFrame(const QByteArray& frame) {
    if (frame.size() < 8)
        return false;
    const uint8_t frameType = static_cast<uint8_t>(frame[3]);
    if (frameType != kFrameTypeResp && frameType != kFrameTypeNotify)
        return false;

    const int payloadLen = static_cast<int>(CommonUtils::readLe16(frame, 4));
    const QByteArray payload = frame.mid(6, payloadLen);
    if (payload.size() < 4)
        return false;
    if (static_cast<uint8_t>(payload[0]) != kServerId)
        return false;

    int off = 2;
    while (off + 3 <= payload.size()) {
        const uint8_t tlvType = static_cast<uint8_t>(payload[off]);
        const quint16 tlvLen = CommonUtils::readLe16(payload, off + 1);
        off += 3;
        if (off + tlvLen > payload.size())
            break;
        TlvMessage msg;
        msg.type = tlvType;
        msg.value = payload.mid(off, tlvLen);
        off += tlvLen;
        responses_.enqueue(msg);
    }
    return true;
}

bool RootBleOtaClient::sendTlvRequest(uint8_t tlvType, const QByteArray& tlvValue) {
    if (!sendFunc_)
        return false;
    const QByteArray payload = buildOtaPayload(tlvType, tlvValue);
    const uint8_t sendSeq = seq_;
    const QByteArray frame = buildFrame(seq_, kFrameTypeReq, payload);
    qDebug() << QStringLiteral("BLE OTA 发送数据包 type=0x%1 seq=%2 len=%3 data=%4")
                    .arg(tlvType, 2, 16, QLatin1Char('0'))
                    .arg(sendSeq)
                    .arg(frame.size())
                    .arg(QString::fromLatin1(frame.toHex(' ').toUpper()));

                    // qDebug() << "BLE OTA 发送数据包";
    sendFunc_(frame);
    return true;
}

bool RootBleOtaClient::waitTlv(uint8_t tlvType, int timeoutMs, QByteArray* outValue, CancelPredicate cancelled) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < timeoutMs) {
        if (cancelled && cancelled())
            return false;
        if (tryTakeResponse(tlvType, outValue))
            return true;
        if (tlvType != Nack && tryTakeResponse(Nack, outValue))
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
        QThread::msleep(10);
    }
    return false;
}

bool RootBleOtaClient::negotiateBlockSize(int* outBlockSize, int fragmentSize, CancelPredicate cancelled,
                                          QString* errorOut) {
    QByteArray req;
    CommonUtils::appendLe16(&req, static_cast<quint16>(kDefaultSuggestBlockSize));
    if (!sendTlvRequest(NegotiateBsReq, req)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送块大小协商请求失败");
        return false;
    }
    QByteArray resp;
    if (!waitTlv(NegotiateBsResp, kResponseTimeoutMs, &resp, cancelled)) {
        if (errorOut)
            *errorOut = QStringLiteral("等待 NEGOTIATE_BS_RESP 超时");
        return false;
    }
    if (resp.size() < 2) {
        if (errorOut)
            *errorOut = QStringLiteral("NEGOTIATE_BS_RESP 长度非法");
        return false;
    }
    const int deviceMax = static_cast<int>(CommonUtils::readLe16(resp, 0));
    int blockSize = kDefaultSuggestBlockSize;
    if (deviceMax > 0)
        blockSize = qMin(blockSize, deviceMax);
    if (blockSize <= 0)
        blockSize = qMax(1, fragmentSize);
    *outBlockSize = blockSize;
    return true;
}

bool RootBleOtaClient::startOtaSession(uint32_t imageId, uint32_t version, int blockSize, const QByteArray& imageData,
                                       int* outNextBlock, CancelPredicate cancelled, QString* errorOut) {
    const int totalSize = imageData.size();
    const int totalBlocks = (totalSize + blockSize - 1) / blockSize;
    const uint32_t imageCrc32 = calculateImageCrc32(imageData);

    QByteArray req;
    CommonUtils::appendLe32(&req, imageId);
    CommonUtils::appendLe16(&req, static_cast<quint16>(totalBlocks));
    CommonUtils::appendLe16(&req, static_cast<quint16>(blockSize));
    CommonUtils::appendLe32(&req, static_cast<quint32>(totalSize));
    CommonUtils::appendLe32(&req, version);
    CommonUtils::appendLe32(&req, imageCrc32);

    if (!sendTlvRequest(StartOtaReq, req)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送 START_OTA_REQ 失败");
        return false;
    }

    QByteArray resp;
    if (!waitTlv(StartOtaResp, kResponseTimeoutMs, &resp, cancelled)) {
        if (errorOut)
            *errorOut = QStringLiteral("等待 START_OTA_RESP 超时或被 NACK");
        return false;
    }
    if (resp.size() < 3) {
        if (errorOut)
            *errorOut = QStringLiteral("START_OTA_RESP 长度非法");
        return false;
    }
    if (static_cast<uint8_t>(resp[0]) != 0x00) {
        if (errorOut)
            *errorOut = QStringLiteral("设备拒绝 START_OTA_REQ");
        return false;
    }
    *outNextBlock = static_cast<int>(CommonUtils::readLe16(resp, 1));
    return true;
}

RootBleOtaClient::BlockSendResult RootBleOtaClient::sendBlock(int blockNumber, int blockSize,
                                                              const QByteArray& imageData, int intervalMs,
                                                              int fragmentSize, CancelPredicate cancelled,
                                                              QString* errorOut, ProgressFunc onProgress) {
    const int fragStep = qMax(1, fragmentSize);
    const int totalSize = imageData.size();
    const int blockOffset = blockNumber * blockSize;
    const int blockLen = qMin(blockSize, totalSize - blockOffset);
    if (blockLen <= 0)
        return BlockSendResult::Success;

    auto handleNack = [&](const QByteArray& nack) {
        if (nack.size() < 3) {
            if (errorOut)
                *errorOut = QStringLiteral("设备 NACK 长度非法");
            return BlockSendResult::Failed;
        }

        const quint16 nackBlock = CommonUtils::readLe16(nack, 0);
        const quint8 reason = static_cast<quint8>(nack[2]);
        if (nackBlock == static_cast<quint16>(kNackBusyBlock) && reason == static_cast<quint8>(kNackReasonBusy)) {
            if (logFunc_)
                logFunc_(QStringLiteral("收到 NACK(blk=0xFFFF, reason=0x01)，设备超时，停止当前块发送并准备退避重发"));
            return BlockSendResult::RetryBlockAfterBackoff;
        }
        if (nackBlock == blockNumber && reason == kNackReasonReassembleFailed) {
            if (logFunc_)
                logFunc_(QStringLiteral("收到 NACK(blk=%1, reason=0x02)，重新组装失败，准备重发整块").arg(blockNumber));
            return BlockSendResult::RetryBlock;
        }

        if (errorOut)
            *errorOut = QStringLiteral("设备 NACK 块 %1，原因 0x%2")
                            .arg(static_cast<int>(nackBlock))
                            .arg(reason, 2, 16, QLatin1Char('0'));
        return BlockSendResult::Failed;
    };

    auto checkPendingNack = [&]() {
        QByteArray nack;
        if (!tryTakeResponse(Nack, &nack))
            return BlockSendResult::Success;
        return handleNack(nack);
    };

    for (int fragOff = 0; fragOff < blockLen; fragOff += fragStep) {
        if (cancelled && cancelled())
            return BlockSendResult::Failed;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        const BlockSendResult pendingNackResult = checkPendingNack();
        if (pendingNackResult != BlockSendResult::Success)
            return pendingNackResult;
        const int fragLen = qMin(fragStep, blockLen - fragOff);
        QByteArray tlvValue;
        CommonUtils::appendLe16(&tlvValue, static_cast<quint16>(blockNumber));
        CommonUtils::appendLe16(&tlvValue, static_cast<quint16>(fragOff));
        tlvValue.append(imageData.constData() + blockOffset + fragOff, fragLen);
        if (!sendTlvRequest(BlockData, tlvValue)) {
            if (errorOut)
                *errorOut = QStringLiteral("发送 BLOCK_DATA 失败");
            return BlockSendResult::Failed;
        }
        if (onProgress)
            onProgress(CommonUtils::progressPercent(qMin(totalSize, blockOffset + fragOff + fragLen), totalSize));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        const BlockSendResult nackResult = checkPendingNack();
        if (nackResult != BlockSendResult::Success)
            return nackResult;
        waitWork(intervalMs);
    }

    QByteArray complete;
    CommonUtils::appendLe16(&complete, static_cast<quint16>(blockNumber));

    if (cancelled && cancelled())
        return BlockSendResult::Failed;
    const BlockSendResult pendingNackResult = checkPendingNack();
    if (pendingNackResult != BlockSendResult::Success)
        return pendingNackResult;
    if (!sendTlvRequest(BlockComplete, complete)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送 BLOCK_COMPLETE 失败");
        return BlockSendResult::Failed;
    }

    QByteArray ackOrNack;
    if (waitTlv(BlockCompleteAck, kBlockCompleteTimeoutMs, &ackOrNack, cancelled)) {
        if (ackOrNack.size() >= 2 && static_cast<int>(CommonUtils::readLe16(ackOrNack, 0)) == blockNumber)
            return BlockSendResult::Success;
        if (errorOut)
            *errorOut = QStringLiteral("BLOCK_COMPLETE_ACK 块号不匹配，当前块 %1").arg(blockNumber);
        return BlockSendResult::Failed;
    }
    if (cancelled && cancelled())
        return BlockSendResult::Failed;

    if (ackOrNack.size() >= 3)
        return handleNack(ackOrNack);

    if (errorOut)
        *errorOut = QStringLiteral("等待 BLOCK_COMPLETE_ACK 超时，块 %1").arg(blockNumber);
    return BlockSendResult::RetryBlock;
}

bool RootBleOtaClient::endOtaSession(uint32_t imageId, CancelPredicate cancelled, QString* errorOut) {
    QByteArray req;
    CommonUtils::appendLe32(&req, imageId);
    if (!sendTlvRequest(EndOtaReq, req)) {
        if (errorOut)
            *errorOut = QStringLiteral("发送 END_OTA_REQ 失败");
        return false;
    }
    QByteArray resp;
    if (!waitTlv(EndOtaResp, kResponseTimeoutMs, &resp, cancelled)) {
        if (errorOut)
            *errorOut = QStringLiteral("等待 END_OTA_RESP 超时");
        return false;
    }
    if (resp.isEmpty() || static_cast<uint8_t>(resp[0]) != 0x00) {
        if (errorOut)
            *errorOut = QStringLiteral("END_OTA_RESP 失败");
        return false;
    }
    return true;
}

bool RootBleOtaClient::runTransfer(const QByteArray& imageData, uint32_t imageId, uint32_t version, int intervalMs,
                                   int blockBusyWaitMs, int fragmentSize, CancelPredicate cancelled, QString* errorOut,
                                   ProgressFunc onProgress) {
    const int fragSize = qMax(1, fragmentSize);
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

    reset();

    int blockSize = 0;
    if (!negotiateBlockSize(&blockSize, fragSize, cancelled, errorOut))
        return false;

    int nextBlock = 0;
    if (!startOtaSession(imageId, version, blockSize, imageData, &nextBlock, cancelled, errorOut))
        return false;

    const int totalBlocks = (imageData.size() + blockSize - 1) / blockSize;
    if (onProgress)
        onProgress(CommonUtils::progressPercent(qMin(imageData.size(), nextBlock * blockSize), imageData.size()));
    for (int block = nextBlock; block < totalBlocks; ++block) {
        if (cancelled && cancelled())
            return false;
        bool blockDone = false;
        for (int retry = 0; retry < kMaxRetry && !blockDone; ++retry) {
            if (retry > 0 && logFunc_)
                logFunc_(QStringLiteral("BLE OTA 重发整块 %1，第 %2 次").arg(block).arg(retry + 1));
            const BlockSendResult result =
                sendBlock(block, blockSize, imageData, intervalMs, fragSize, cancelled, errorOut, onProgress);
            if (result == BlockSendResult::Success) {
                blockDone = true;
            } else if (result == BlockSendResult::Failed) {
                return false;
            } else if (result == BlockSendResult::RetryBlockAfterBackoff) {
                const int waitMs = qMax(0, blockBusyWaitMs);
                if (logFunc_)
                    logFunc_(QStringLiteral("设备忙，块忙等待 %1 ms 后从块 %2 第一个 fragment 重发").arg(waitMs).arg(block));
                QElapsedTimer backoffTimer;
                backoffTimer.start();
                while (backoffTimer.elapsed() < waitMs) {
                    if (cancelled && cancelled())
                        return false;
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
                    QThread::msleep(10);
                }
            }
        }
        if (!blockDone) {
            if (errorOut)
                *errorOut = QStringLiteral("块 %1 重发 %2 次后仍未确认").arg(block).arg(kMaxRetry);
            return false;
        }
    }

    if (onProgress)
        onProgress(100);
    return endOtaSession(imageId, cancelled, errorOut);
}
