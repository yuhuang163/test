#ifndef FIXTURE_CAMERA_UART_PROTOCOL_H
#define FIXTURE_CAMERA_UART_PROTOCOL_H

#include <QByteArray>

#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

// 摄像工站治具：CTL_INx / CTL_OUTx
class FixtureCameraUartProtocol {
public:
    static QByteArray buildCommand(camreaFixtureState state);
};

#if _MSC_VER >= 1600
#    pragma execution_character_set(pop)
#endif

#endif  // FIXTURE_CAMERA_UART_PROTOCOL_H
