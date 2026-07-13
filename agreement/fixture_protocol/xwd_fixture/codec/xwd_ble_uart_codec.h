#ifndef XWD_BLE_UART_CODEC_H
#define XWD_BLE_UART_CODEC_H

#include <QByteArray>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 蓝牙工站治具：原文组帧（不做转义或追加换行）。 */
namespace XwdBleUartCodec {

QByteArray encodeRawText(const QString& text);

} // namespace XwdBleUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_BLE_UART_CODEC_H
