#ifndef HZ_PCBA_FIXTURE_DEVICE_H
#define HZ_PCBA_FIXTURE_DEVICE_H

#include <functional>

#include <QByteArray>

#include "hz_fixture_types.h"
#include "pcba_uart_codec.h"
#include "usmile_ring_buffer.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 华庄 hz PCBA 治具：组帧下发 + 长包/短包解析。 */
class HzPcbaFixtureDevice {
  public:
    using WriteFn = std::function<void(const QByteArray& data, bool logTx, bool startAction)>;

    explicit HzPcbaFixtureDevice(WriteFn writeFn);
    ~HzPcbaFixtureDevice();

    void sendStartTest(int machineIndex);
    void sendSleep(int machineIndex);
    void sendWhiteMode(int machineIndex);
    void sendFrame(const QByteArray& frame);
    void onRx(const QByteArray& data);
    void pollFrames(const std::function<void(const FixturePcbaUartEvent&)>& handler);

  private:
    WriteFn write_;
    usmile_ring_buffer_t p_ringBuffer_;
    uint8_t ring_buffer_[100 * 1024];
    uint8_t frame_buf_[2 * 1024];
    RingBuf* ringBuf_ = nullptr;
    FixturePcbaUartProtocol* protocol_ = nullptr;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // HZ_PCBA_FIXTURE_DEVICE_H
