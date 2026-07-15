#ifndef XWD_AMPLITUDE_CODEC_H
#define XWD_AMPLITUDE_CODEC_H

#include <QByteArray>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 摆幅仪应答解析（纯协议，不含 SETTINGS 误差）。 */
namespace XwdAmplitudeCodec {

bool parsePeakAmplitude(const QByteArray& byte, int* peakAmplitude);

} // namespace XwdAmplitudeCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_AMPLITUDE_CODEC_H
