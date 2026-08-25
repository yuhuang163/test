#include "aiot_link_codec.h"

#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

uint16_t AiotLinkCodec::crc16CcittFalse(const uint8_t* data, int length) {
    if (!data || length <= 0)
        return 0;
    uint16_t crc = 0xFFFF;
    for (int i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            if (crc & 0x8000)
                crc = static_cast<uint16_t>((crc << 1) ^ 0x1021);
            else
                crc = static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

uint16_t AiotLinkCodec::crc16CcittFalse(const QByteArray& data) {
    return crc16CcittFalse(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
}

QByteArray AiotLinkCodec::buildFrame(const QByteArray& payload, uint8_t control, uint8_t fsn, uint8_t fmn,
                                     uint8_t psn, uint8_t version) {
    const uint8_t fra = static_cast<uint8_t>(control & AiotLink::kCtrlFsnMask);
    const bool needVersion = (control & AiotLink::kCtrlVersion) != 0;
    const bool needFsn = fra != AiotLink::kCtrlFsnNone;
    // Version 启用且分帧：Header 再带 FMN+PSN（规范 Version=0）
    const bool needFmnPsn = needVersion && needFsn;

    // Length = Control(+Version)(+FSN)(+FMN+PSN)+Payload，不含 SOF/Length/CRC
    const int midLen = 1 + (needVersion ? 1 : 0) + (needFsn ? 1 : 0) + (needFmnPsn ? 2 : 0) + payload.size();
    if (midLen > 0xFFFF)
        return {};

    QByteArray mid;
    mid.reserve(midLen);
    mid.append(static_cast<char>(control));
    if (needVersion)
        mid.append(static_cast<char>(version));
    if (needFsn)
        mid.append(static_cast<char>(fsn));
    if (needFmnPsn) {
        mid.append(static_cast<char>(fmn));
        mid.append(static_cast<char>(psn));
    }
    mid.append(payload);

    // CRC 覆盖 Length + Control(+Version/FSN/FMN/PSN) + Payload
    QByteArray crcInput;
    crcInput.reserve(2 + mid.size());
    crcInput.append(static_cast<char>((midLen >> 8) & 0xFF));
    crcInput.append(static_cast<char>(midLen & 0xFF));
    crcInput.append(mid);
    const uint16_t crc = crc16CcittFalse(crcInput);

    QByteArray frame;
    frame.reserve(1 + 2 + mid.size() + 2);
    frame.append(static_cast<char>(AiotLink::kSof));
    frame.append(static_cast<char>((midLen >> 8) & 0xFF));
    frame.append(static_cast<char>(midLen & 0xFF));
    frame.append(mid);
    frame.append(static_cast<char>((crc >> 8) & 0xFF));
    frame.append(static_cast<char>(crc & 0xFF));
    return frame;
}

QVector<QByteArray> AiotLinkCodec::buildFramesForPdu(const QByteArray& pdu, int maxPayload) {
    QVector<QByteArray> frames;
    if (maxPayload < 1)
        maxPayload = 512;
    if (pdu.size() <= maxPayload) {
        // 本机发送仍用 v1.0 单帧（不启 Version）；设备应答可能带 Version=0
        frames.append(buildFrame(pdu, AiotLink::kCtrlFsnNone));
        return frames;
    }

    int offset = 0;
    uint8_t fsn = 0;
    const int totalFrames = (pdu.size() + maxPayload - 1) / maxPayload;
    while (offset < pdu.size()) {
        const int remain = pdu.size() - offset;
        const int chunk = qMin(remain, maxPayload);
        uint8_t ctrl = AiotLink::kCtrlFsnMiddle;
        if (offset == 0)
            ctrl = AiotLink::kCtrlFsnStart;
        if (chunk >= remain)
            ctrl = (offset == 0) ? AiotLink::kCtrlFsnNone : AiotLink::kCtrlFsnEnd;
        // 多分片时首帧也必须带 FSN
        if (offset == 0 && chunk < pdu.size())
            ctrl = AiotLink::kCtrlFsnStart;
        frames.append(buildFrame(pdu.mid(offset, chunk), ctrl, fsn, static_cast<uint8_t>(totalFrames), 0));
        ++fsn;
        offset += chunk;
    }
    return frames;
}

void AiotLinkCodec::reset() {
    state_ = State::WaitSof;
    expectedBody_ = 0;
    body_.clear();
}

bool AiotLinkCodec::feed(const QByteArray& chunk, QVector<Frame>* outFrames) {
    if (!outFrames)
        return false;
    bool got = false;
    for (unsigned char ch : chunk) {
        switch (state_) {
        case State::WaitSof:
            if (ch == AiotLink::kSof)
                state_ = State::WaitLen0;
            break;
        case State::WaitLen0:
            expectedBody_ = static_cast<uint16_t>(ch) << 8;
            state_ = State::WaitLen1;
            break;
        case State::WaitLen1: {
            const uint16_t midLen = static_cast<uint16_t>(expectedBody_ | ch);
            // body = Control(+Version/FSN/FMN/PSN)+Payload+CRC(2)
            if (midLen < 1) {
                reset();
                break;
            }
            const int bodyLen = static_cast<int>(midLen) + 2;
            body_.clear();
            body_.reserve(bodyLen);
            expectedBody_ = static_cast<uint16_t>(bodyLen);
            state_ = State::WaitBody;
            break;
        }
        case State::WaitBody:
            body_.append(static_cast<char>(ch));
            if (body_.size() < expectedBody_)
                break;
            {
                // 校验：Length(2)+mid，CRC 为大端落在末 2 字节
                const int midLen = body_.size() - 2;
                if (midLen < 1) {
                    reset();
                    break;
                }
                QByteArray crcInput;
                crcInput.append(static_cast<char>((midLen >> 8) & 0xFF));
                crcInput.append(static_cast<char>(midLen & 0xFF));
                crcInput.append(body_.constData(), midLen);
                const uint16_t expect = crc16CcittFalse(crcInput);
                const uint16_t gotCrc = static_cast<uint16_t>(
                    (static_cast<uint8_t>(body_.at(midLen)) << 8) | static_cast<uint8_t>(body_.at(midLen + 1)));
                if (expect == gotCrc) {
                    Frame fr;
                    fr.control = static_cast<uint8_t>(body_.at(0));
                    const uint8_t fra = static_cast<uint8_t>(fr.control & AiotLink::kCtrlFsnMask);
                    const bool needVersion = (fr.control & AiotLink::kCtrlVersion) != 0;
                    const bool needFsn = fra != AiotLink::kCtrlFsnNone;
                    const bool needFmnPsn = needVersion && needFsn;

                    int payloadOff = 1;
                    if (needVersion) {
                        if (midLen < payloadOff + 1) {
                            reset();
                            break;
                        }
                        fr.hasVersion = true;
                        fr.version = static_cast<uint8_t>(body_.at(payloadOff));
                        ++payloadOff;
                    }
                    if (needFsn) {
                        if (midLen < payloadOff + 1) {
                            reset();
                            break;
                        }
                        fr.hasFsn = true;
                        fr.fsn = static_cast<uint8_t>(body_.at(payloadOff));
                        ++payloadOff;
                    }
                    if (needFmnPsn) {
                        if (midLen < payloadOff + 2) {
                            reset();
                            break;
                        }
                        fr.hasFmnPsn = true;
                        fr.fmn = static_cast<uint8_t>(body_.at(payloadOff));
                        fr.psn = static_cast<uint8_t>(body_.at(payloadOff + 1));
                        payloadOff += 2;
                    }
                    fr.payload = body_.mid(payloadOff, midLen - payloadOff);
                    outFrames->append(fr);
                    got = true;
                }
                reset();
            }
            break;
        }
    }
    return got;
}
