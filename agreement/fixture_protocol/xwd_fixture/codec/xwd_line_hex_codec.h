#ifndef XWD_LINE_HEX_CODEC_H
#define XWD_LINE_HEX_CODEC_H

#include <QByteArray>

#include "xwd_fixture_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 传统 0x55 十六进制帧协议。 */
namespace XwdLineHexCodec {

QByteArray buildTxFrame(XwdFixtureState state);

} // namespace XwdLineHexCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_LINE_HEX_CODEC_H
