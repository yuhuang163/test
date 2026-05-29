#include "root_ble_ota.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

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

static const uint32_t kCrc32Table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D,
};

quint32 crc32Update(quint32 crc, const char* data, int len) {
    while (len-- > 0) {
        crc = (crc >> 8) ^ kCrc32Table[(crc ^ static_cast<quint8>(*data++)) & 0xFF];
    }
    return crc;
}

void appendLe16(QByteArray* b, quint16 v) {
    b->append(static_cast<char>(v & 0xFF));
    b->append(static_cast<char>((v >> 8) & 0xFF));
}

void appendLe32(QByteArray* b, quint32 v) {
    b->append(static_cast<char>(v & 0xFF));
    b->append(static_cast<char>((v >> 8) & 0xFF));
    b->append(static_cast<char>((v >> 16) & 0xFF));
    b->append(static_cast<char>((v >> 24) & 0xFF));
}

quint16 readLe16(const QByteArray& b, int off) {
    return static_cast<quint16>(static_cast<quint8>(b[off]) | (static_cast<quint8>(b[off + 1]) << 8));
}

quint32 readLe32(const QByteArray& b, int off) {
    return static_cast<quint32>(static_cast<quint8>(b[off]) | (static_cast<quint8>(b[off + 1]) << 8)
                                | (static_cast<quint8>(b[off + 2]) << 16)
                                | (static_cast<quint8>(b[off + 3]) << 24));
}

int progressFromBytes(int sentBytes, int totalBytes) {
    if (totalBytes <= 0)
        return 0;
    return qMin(100, static_cast<int>(static_cast<qint64>(sentBytes) * 100 / totalBytes));
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
    quint32 crc = 0xFFFFFFFFu;
    crc = crc32Update(crc, imageData.constData(), imageData.size());
    return crc ^ 0xFFFFFFFFu;
}

QByteArray RootBleOtaClient::buildOtaPayload(uint8_t tlvType, const QByteArray& tlvValue) {
    QByteArray payload;
    payload.append(static_cast<char>(kServerId));
    payload.append(static_cast<char>(kServerIdLen));
    payload.append(static_cast<char>(tlvType));
    appendLe16(&payload, static_cast<quint16>(tlvValue.size()));
    payload.append(tlvValue);
    return payload;
}

QByteArray RootBleOtaClient::buildFrame(uint8_t& seq, uint8_t frameType, const QByteArray& payload) {
    QByteArray header;
    appendLe16(&header, kFrameSof);
    header.append(static_cast<char>(seq++));
    header.append(static_cast<char>(frameType));
    appendLe16(&header, static_cast<quint16>(payload.size()));

    QByteArray frame = header;
    frame.append(payload);

    const quint16 crc = CRC16(frame.constData(), static_cast<unsigned int>(frame.size()));
    appendLe16(&frame, crc);
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

        const quint16 payloadLen = readLe16(rxBuffer_, 4);
        const int frameLen = 6 + payloadLen + 2;
        if (rxBuffer_.size() < frameLen)
            return;

        const QByteArray frame = rxBuffer_.left(frameLen);
        rxBuffer_.remove(0, frameLen);

        const quint16 expectCrc = readLe16(frame, 6 + payloadLen);
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

    const int payloadLen = static_cast<int>(readLe16(frame, 4));
    const QByteArray payload = frame.mid(6, payloadLen);
    if (payload.size() < 4)
        return false;
    if (static_cast<uint8_t>(payload[0]) != kServerId)
        return false;

    int off = 2;
    while (off + 3 <= payload.size()) {
        const uint8_t tlvType = static_cast<uint8_t>(payload[off]);
        const quint16 tlvLen = readLe16(payload, off + 1);
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

bool RootBleOtaClient::negotiateBlockSize(int* outBlockSize, CancelPredicate cancelled, QString* errorOut) {
    QByteArray req;
    appendLe16(&req, static_cast<quint16>(kDefaultSuggestBlockSize));
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
    const int deviceMax = static_cast<int>(readLe16(resp, 0));
    int blockSize = kDefaultSuggestBlockSize;
    if (deviceMax > 0)
        blockSize = qMin(blockSize, deviceMax);
    if (blockSize <= 0)
        blockSize = kDefaultFragmentSize;
    *outBlockSize = blockSize;
    return true;
}

bool RootBleOtaClient::startOtaSession(uint32_t imageId, uint32_t version, int blockSize, const QByteArray& imageData,
                                       int* outNextBlock, CancelPredicate cancelled, QString* errorOut) {
    const int totalSize = imageData.size();
    const int totalBlocks = (totalSize + blockSize - 1) / blockSize;
    const uint32_t imageCrc32 = calculateImageCrc32(imageData);

    QByteArray req;
    appendLe32(&req, imageId);
    appendLe16(&req, static_cast<quint16>(totalBlocks));
    appendLe16(&req, static_cast<quint16>(blockSize));
    appendLe32(&req, static_cast<quint32>(totalSize));
    appendLe32(&req, version);
    appendLe32(&req, imageCrc32);

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
    *outNextBlock = static_cast<int>(readLe16(resp, 1));
    return true;
}

RootBleOtaClient::BlockSendResult RootBleOtaClient::sendBlock(int blockNumber, int blockSize,
                                                              const QByteArray& imageData, int intervalMs,
                                                              CancelPredicate cancelled, QString* errorOut,
                                                              ProgressFunc onProgress) {
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

        const quint16 nackBlock = readLe16(nack, 0);
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

    for (int fragOff = 0; fragOff < blockLen; fragOff += kDefaultFragmentSize) {
        if (cancelled && cancelled())
            return BlockSendResult::Failed;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        const BlockSendResult pendingNackResult = checkPendingNack();
        if (pendingNackResult != BlockSendResult::Success)
            return pendingNackResult;
        const int fragLen = qMin(kDefaultFragmentSize, blockLen - fragOff);
        QByteArray tlvValue;
        appendLe16(&tlvValue, static_cast<quint16>(blockNumber));
        appendLe16(&tlvValue, static_cast<quint16>(fragOff));
        tlvValue.append(imageData.constData() + blockOffset + fragOff, fragLen);
        if (!sendTlvRequest(BlockData, tlvValue)) {
            if (errorOut)
                *errorOut = QStringLiteral("发送 BLOCK_DATA 失败");
            return BlockSendResult::Failed;
        }
        if (onProgress)
            onProgress(progressFromBytes(qMin(totalSize, blockOffset + fragOff + fragLen), totalSize));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        const BlockSendResult nackResult = checkPendingNack();
        if (nackResult != BlockSendResult::Success)
            return nackResult;
        QThread::msleep(intervalMs);
    }

    QByteArray complete;
    appendLe16(&complete, static_cast<quint16>(blockNumber));

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
        if (ackOrNack.size() >= 2 && static_cast<int>(readLe16(ackOrNack, 0)) == blockNumber)
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
    appendLe32(&req, imageId);
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
                                   int blockBusyWaitMs, CancelPredicate cancelled, QString* errorOut,
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

    reset();

    int blockSize = 0;
    if (!negotiateBlockSize(&blockSize, cancelled, errorOut))
        return false;

    int nextBlock = 0;
    if (!startOtaSession(imageId, version, blockSize, imageData, &nextBlock, cancelled, errorOut))
        return false;

    const int totalBlocks = (imageData.size() + blockSize - 1) / blockSize;
    if (onProgress)
        onProgress(progressFromBytes(qMin(imageData.size(), nextBlock * blockSize), imageData.size()));
    for (int block = nextBlock; block < totalBlocks; ++block) {
        if (cancelled && cancelled())
            return false;
        bool blockDone = false;
        for (int retry = 0; retry < kMaxRetry && !blockDone; ++retry) {
            if (retry > 0 && logFunc_)
                logFunc_(QStringLiteral("BLE OTA 重发整块 %1，第 %2 次").arg(block).arg(retry + 1));
            const BlockSendResult result = sendBlock(block, blockSize, imageData, intervalMs, cancelled, errorOut, onProgress);
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
