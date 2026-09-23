#ifndef XWD_RAW_UART_CODEC_H
#define XWD_RAW_UART_CODEC_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 XWD 治具（蓝牙盒/吸力等同物理层）：hex 或 UTF-8 原文；读电流解析。 */
namespace XwdRawUartCodec {

QByteArray encodeRawText(const QString& text, bool* parsedAsHex = nullptr);

/** 解析 readonce 文本回包，例如：
 *  CH1:        12.222V        2403.75mA        29370.00mW
 *  CH2:        12.204V        2406.25mA        29370.00mW
 *  hasCh1/hasCh2 标明对应通道是否解析到；至少解析到一通道电流时返回 true。
 */
bool parseReadOnceReply(const QString& text, double* ch1Ma, double* ch2Ma, bool* hasCh1 = nullptr,
                        bool* hasCh2 = nullptr);

/** Air1 输入灯显等治具：发 88 AE 00 11，收 FA FB ss + 大端 IEEE754 float(A) + 尾字节。 */
QByteArray buildAir1FloatCurrentReadRequest();
/** 在缓冲中定位 FA FB，解析电流（单位 A）；成功返回 true。 */
bool parseAir1FloatCurrentReply(const QByteArray& buf, double* amperesOut, int* frameOffset = nullptr);

/**
 * Air1 输出灯显等：Modbus RTU FC03 读 1 个保持寄存器，原始值 /100 = 电压(V)。
 * 抓包例：发 01 03 00 01 00 01 CRC → 收 01 03 02 01 EE CRC → 0x01EE=494 → 4.94V。
 */
QByteArray buildAir1ModbusVoltageReadRequest(quint8 slaveAddr, quint16 registerAddress);
bool parseAir1ModbusVoltageReply(const QByteArray& buf, quint8 slaveAddr, double* voltsOut,
                                 int* frameOffset = nullptr);

} // namespace XwdRawUartCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_RAW_UART_CODEC_H
