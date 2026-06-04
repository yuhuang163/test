#ifndef FIXTURE_PRESS_UART_PROTOCOL_H
#define FIXTURE_PRESS_UART_PROTOCOL_H

#include <QByteArray>

#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

// 压感 / Y20Pro / U7 治具：文本命令表 + 应答解析
struct FixturePressUartEvent {
    int pressNotifyState = 0;  // 0=无；1~4 对应原 send_data_to_mechine_press 参数
    bool requestCheckStatus = false;
};

class FixturePressUartProtocol {
public:
    void initCommands();
    bool commandBytes(int commandId, int numb, QByteArray* out) const;
    bool isValidCommand(int commandId, int numb) const;
    static FixturePressUartEvent parseReceived(const QByteArray& data);
    /** 气缸/继电器 0x55 短帧（非 PCBA 电流上报长包） */
    static QByteArray buildFixtureStateCommand(FixtureState state);

private:
    const char* press_commands_[COMMAND_ID_MAX][6] = {};
};

#if _MSC_VER >= 1600
#    pragma execution_character_set(pop)
#endif

#endif  // FIXTURE_PRESS_UART_PROTOCOL_H
