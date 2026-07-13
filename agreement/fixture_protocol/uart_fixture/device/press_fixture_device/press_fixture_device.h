#ifndef PRESS_FIXTURE_DEVICE_H
#define PRESS_FIXTURE_DEVICE_H

#include <functional>

#include <QByteArray>

#include "fixture_uart_types.h"
#include "press_uart_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 压感 / Y20Pro / U7 治具：文本命令与 0x55 短帧。 */
class PressFixtureDevice {
  public:
    using WriteFn = std::function<void(const QByteArray& data, bool logTx, bool startAction)>;
    using DelayFn = std::function<void(unsigned int msec)>;

    PressFixtureDevice(WriteFn writeFn, DelayFn delayFn);

    void sendFixtureState(FixtureState state);
    void sendCommand(int commandId, int numb);
    FixturePressUartEvent parseReceived(const QByteArray& data) const;

    qint64 lastCommidTimestamp() const;
    void setLastCommidTimestamp(qint64 timestamp);
    machine_command_id_e lastCommid() const;
    void setLastCommid(machine_command_id_e commandId);

  private:
    WriteFn write_;
    DelayFn delay_;
    FixturePressUartProtocol protocol_;
    qint64 last_sent_timestamp_ = 0;
    qint64 last_commid_timestamp_ = 0;
    machine_command_id_e last_commid_ = COMMAND_ID_MAX;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // PRESS_FIXTURE_DEVICE_H
