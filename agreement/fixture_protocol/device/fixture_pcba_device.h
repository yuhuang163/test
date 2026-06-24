#ifndef FIXTURE_PCBA_UART_PROTOCOL_H
#define FIXTURE_PCBA_UART_PROTOCOL_H

#include <functional>

#include <QByteArray>

#include "fixture_uart_types.h"
#include "usmile_ring_buffer.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// 板厂 PCBA 治具：0x55 物理层帧（电流/按键等长包 + 休眠/启动短包）
struct FixturePcbaUartEvent {
    enum class Type {
        None,
        ShortSleep,
        ShortStart,
        FullPacket,
    };
    Type type = Type::None;
    FixturePacketData packet;
};

class FixturePcbaUartProtocol {
  public:
    FixturePcbaUartProtocol(RingBuf* ringBuf, usmile_ring_buffer_t* ring, uint8_t* frameBuf, int frameBufSize);

    void pollFrames(const std::function<void(const FixturePcbaUartEvent&)>& handler);
    int findNextFrame();

    static void dispatchShortFrame(const QByteArray& data,
                                   const std::function<void(const FixturePcbaUartEvent&)>& handler);
    static FixturePcbaUartEvent parseFullFrame(const QByteArray& data);

    /** @param machineIndex 机位 1..15（0x01..0x0F） */
    static QByteArray buildStartTestCommand(int machineIndex);
    static QByteArray buildSleepCommand(int machineIndex);
    static QByteArray buildWhiteModeCommand(int machineIndex);

  private:
    RingBuf* ringBuf_;
    usmile_ring_buffer_t* ring_;
    uint8_t* frameBuf_;
    int frameBufSize_;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_PCBA_UART_PROTOCOL_H
