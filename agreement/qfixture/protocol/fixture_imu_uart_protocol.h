#ifndef FIXTURE_IMU_UART_PROTOCOL_H
#define FIXTURE_IMU_UART_PROTOCOL_H

#include <QByteArray>

#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// 欣旺达六轴治具：ASCII 收发
struct FixtureImuUartEvent {
    bool ok = false;
    bool error = false;
};

class FixtureImuUartProtocol {
  public:
    static QByteArray buildCommand(imuFixtureState state);
    static FixtureImuUartEvent parseReceived(const QByteArray& data);
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_IMU_UART_PROTOCOL_H
