#ifndef UNIVERSAL_FIXTURE_CODEC_H
#define UNIVERSAL_FIXTURE_CODEC_H

#include <QByteArray>
#include <QString>
#include <QVector>

#include "universal_fixture_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace UniversalFixtureCodec {

QByteArray buildControlFrame(quint8 slot, quint8 ch, quint8 st);
QByteArray buildMeasureFrame(quint8 slot, quint8 ch);

/** 从缓冲中解析控制到位应答；匹配 expectSlot（广播时 expectSlot=0x00 表示接受任一 1~15） */
bool takeControlAck(QByteArray* buffer, quint8 expectSlot, quint8 expectCh, quint8 expectSt,
                    UniversalFixture::ControlAck* out, QString* errorMessage = nullptr);

/** 从缓冲取出一帧测量应答；expectSlot 必须为 1~15 */
bool takeMeasureRsp(QByteArray* buffer, quint8 expectSlot, UniversalFixture::MeasureRsp* out,
                    QString* errorMessage = nullptr);

/** 解析缓冲中全部测量应答（CH=0 全通道多帧） */
QVector<UniversalFixture::MeasureRsp> takeAllMeasureRsp(QByteArray* buffer, quint8 expectSlot);

QString controlStatusText(quint8 status);
QString measureStatusText(quint8 status);

} // namespace UniversalFixtureCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // UNIVERSAL_FIXTURE_CODEC_H
