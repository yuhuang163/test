#include "xwd_ble_uart_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace XwdBleUartCodec {

QByteArray encodeRawText(const QString& text) {
    return text.toUtf8();
}

} // namespace XwdBleUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
