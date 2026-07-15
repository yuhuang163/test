#ifndef XWD_SUCTION_UART_CODEC_H
#define XWD_SUCTION_UART_CODEC_H

#include <QByteArray>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 吸力工站治具：纯 hex 按十六进制字节下发，否则 UTF-8 原文。 */
namespace XwdSuctionUartCodec {

QByteArray encodeRawText(const QString& text, bool* parsedAsHex = nullptr);

/** 解析 readonce 文本回包，例如：
 *  CH1:        12.222V        2403.75mA        29370.00mW
 *  CH2:        12.204V        2406.25mA        29370.00mW
 *  hasCh1/hasCh2 标明对应通道是否解析到；至少解析到一通道电流时返回 true。
 */
bool parseReadOnceReply(const QString& text, double* ch1Ma, double* ch2Ma, bool* hasCh1 = nullptr,
                        bool* hasCh2 = nullptr);

} // namespace XwdSuctionUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_SUCTION_UART_CODEC_H
