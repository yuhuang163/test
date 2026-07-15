#include "xwd_line_text_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdLineTextCodec {

QByteArray buildTxFrame(XwdFixtureState state) {
    switch (state) {
    case XWD_STATE_CYLINDER_OPEN:
        return QByteArray("ctl_down\r\n");
    case XWD_STATE_CYLINDER_RESET:
        return QByteArray("ctl_up\r\n");
    case XWD_STATE_RELAY1_OPEN:
        return QByteArray("pen1_down\r\n");
    case XWD_STATE_RELAY_RESET:
        return QByteArray("pen_up\r\n");
    case XWD_STATE_RELAY_OPEN:
        return QByteArray("pen_down\r\n");
    case XWD_STATE_RELAY1_RESET:
        return QByteArray("pen1_up\r\n");
    case XWD_STATE_RELAY2_OPEN:
        return QByteArray("pen2_down\r\n");
    case XWD_STATE_RELAY2_RESET:
        return QByteArray("pen2_up\r\n");
    case XWD_STATE_PEN_PRESS:
        return QByteArray("pen_press\r\n");
    case XWD_STATE_RESET_ALL:
        return QByteArray("reset\r\n");
    case XWD_STATE_TRAY_MIDDLE:
        return QByteArray("tray_middle\r\n");
    case XWD_STATE_TRAY_REAR:
        return QByteArray("tray_rear\r\n");
    case XWD_STATE_FRONT:
        return QByteArray("tray_front\r\n");
    default:
        return QByteArray();
    }
}

} // namespace XwdLineTextCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
