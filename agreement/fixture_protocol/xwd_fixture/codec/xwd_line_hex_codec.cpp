#include "xwd_line_hex_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdLineHexCodec {

QByteArray buildTxFrame(XwdFixtureState state) {
    switch (state) {
    case XWD_STATE_CYLINDER_RESET:
        return QByteArray::fromHex("5501050000");
    case XWD_STATE_RELAY1_OPEN:
        return QByteArray::fromHex("5501010001");
    case XWD_STATE_RELAY_RESET:
        return QByteArray::fromHex("5501010000");
    case XWD_STATE_RELAY_OPEN:
        return QByteArray::fromHex("5501020001");
    case XWD_STATE_RELAY1_RESET:
        return QByteArray::fromHex("5501010000");
    case XWD_STATE_RELAY2_OPEN:
        return QByteArray::fromHex("5501020001");
    case XWD_STATE_RELAY2_RESET:
        return QByteArray::fromHex("5501020000");
    case XWD_STATE_RELAY4_OPEN:
        return QByteArray::fromHex("5501040001");
    case XWD_STATE_RELAY4_RESET:
        return QByteArray::fromHex("5501040000");
    default:
        return QByteArray();
    }
}

} // namespace XwdLineHexCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
