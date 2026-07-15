#include "hz_pcba_fixture_device.h"

#include "qdebug.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

HzPcbaFixtureDevice::HzPcbaFixtureDevice(WriteFn writeFn) : write_(std::move(writeFn)) {
    ringBuf_ = new RingBuf(&p_ringBuffer_, ring_buffer_, 1, sizeof(ring_buffer_));
    protocol_ = new FixturePcbaUartProtocol(ringBuf_, &p_ringBuffer_, frame_buf_, sizeof(frame_buf_));
}

HzPcbaFixtureDevice::~HzPcbaFixtureDevice() {
    delete protocol_;
    delete ringBuf_;
}

void HzPcbaFixtureDevice::sendStartTest(int machineIndex) {
    const QByteArray frame = FixturePcbaUartProtocol::buildStartTestCommand(machineIndex);
    if (frame.isEmpty()) {
        qDebug() << "Invalid hz PCBA start command index";
        return;
    }
    write_(frame, true, false);
    qDebug() << "已发送华庄 PCBA 开始命令" << machineIndex;
}

void HzPcbaFixtureDevice::sendSleep(int machineIndex) {
    const QByteArray frame = FixturePcbaUartProtocol::buildSleepCommand(machineIndex);
    if (frame.isEmpty()) {
        qDebug() << "Invalid hz PCBA sleep command index";
        return;
    }
    write_(frame, true, false);
    qDebug() << "已发送华庄 PCBA 休眠命令" << machineIndex;
}

void HzPcbaFixtureDevice::sendWhiteMode(int machineIndex) {
    const QByteArray frame = FixturePcbaUartProtocol::buildWhiteModeCommand(machineIndex);
    if (frame.isEmpty()) {
        qDebug() << "Invalid hz PCBA white-mode command index";
        return;
    }
    write_(frame, true, false);
    qDebug() << "已发送华庄 PCBA 亮白模式命令" << machineIndex;
}

void HzPcbaFixtureDevice::sendFrame(const QByteArray& frame) {
    if (frame.isEmpty())
        return;
    write_(frame, true, false);
}

void HzPcbaFixtureDevice::onRx(const QByteArray& data) {
    if (data.isEmpty() || !ringBuf_)
        return;
    const int writeLen = ringBuf_->usmile_ring_buffer_write(
        &p_ringBuffer_, reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())), data.size());
    if (writeLen < data.size())
        qDebug() << "hz PCBA ring write_len:" << writeLen << "len:" << data.size();
}

void HzPcbaFixtureDevice::pollFrames(const std::function<void(const FixturePcbaUartEvent&)>& handler) {
    if (protocol_)
        protocol_->pollFrames(handler);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
