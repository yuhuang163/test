#include "camera_uart_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QByteArray FixtureCameraUartProtocol::buildCommand(camreaFixtureState state) {
    switch (state) {
    case STATE_THOROUGHFARE1_IN:
        return QByteArray("CTL_IN1\r\n");
    case STATE_THOROUGHFARE1_OUT:
        return QByteArray("CTL_OUT1\r\n");
    case STATE_THOROUGHFARE2_IN:
        return QByteArray("CTL_IN2\r\n");
    case STATE_THOROUGHFARE2_OUT:
        return QByteArray("CTL_OUT2\r\n");
    case STATE_THOROUGHFARE3_IN:
        return QByteArray("CTL_IN3\r\n");
    case STATE_THOROUGHFARE3_OUT:
        return QByteArray("CTL_OUT3\r\n");
    default:
        return QByteArray();
    }
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
