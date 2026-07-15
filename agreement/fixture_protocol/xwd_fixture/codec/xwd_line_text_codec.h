#ifndef XWD_LINE_TEXT_CODEC_H
#define XWD_LINE_TEXT_CODEC_H

#include <QByteArray>

#include "xwd_fixture_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 产线文本协议（ctl_down / tray_middle 等）。 */
namespace XwdLineTextCodec {

QByteArray buildTxFrame(XwdFixtureState state);

} // namespace XwdLineTextCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_LINE_TEXT_CODEC_H
