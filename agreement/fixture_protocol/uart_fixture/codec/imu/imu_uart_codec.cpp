#include "imu_uart_codec.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QByteArray FixtureImuUartProtocol::buildCommand(imuFixtureState state) {
    switch (state) {
    case STATE_START:
        return QByteArray("START");
    case STATE_END:
        return QByteArray("END");
    case STATE_RESET:
        return QByteArray("RESET");
    case STATE_RETURN:
        return QByteArray("S0180");
    case STATE_40:
        return QByteArray("S0040");
    case STATE_FU40:
        return QByteArray("S1040");
    case STATE_BRUSH_UP:
        return QByteArray("B0000");
    case STATE_BRUSH_DOWN:
        return QByteArray("B0180");
    case STATE_BRUSH_LEFT:
        return QByteArray("B0090");
    case STATE_BRUSH_RIGHT:
        return QByteArray("B0270");
    case STATE_HOME:
        return QByteArray("HOME");
    default:
        return QByteArray();
    }
}

FixtureImuUartEvent FixtureImuUartProtocol::parseReceived(const QByteArray& data) {
    FixtureImuUartEvent ev;
    const QString text = QString::fromUtf8(data);

    if (text.contains(QStringLiteral("OK"))) {
        ev.ok = true;
        qDebug() << "收到治具ok指令";
    }
    if (text.contains(QStringLiteral("ERROR"))) {
        ev.error = true;
    }
    return ev;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
