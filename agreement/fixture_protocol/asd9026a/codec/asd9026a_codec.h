#ifndef ASD9026A_CODEC_H
#define ASD9026A_CODEC_H

#include <QByteArray>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Asd9026aCodec {

constexpr quint8 kFuncRead = 0x10;
constexpr quint8 kFuncWrite = 0x11;
constexpr quint8 kFuncReadReply = 0x12;
constexpr quint8 kFuncAnalogRead = 0x20;
constexpr quint8 kFuncAnalogWrite = 0x21;
constexpr quint8 kFuncAnalogReply = 0x22;

constexpr quint8 kCmdQueryModuleInfo = 0x01;
constexpr quint8 kCmdQueryModuleStatus = 0x02;
constexpr quint8 kCmdOutputSwitch = 0x04;
constexpr quint8 kCmdAnalogConfigure = 0x0D;
constexpr quint8 kCmdAnalogStatus = 0x10;

quint16 crc16Modbus(const QByteArray& data);

QByteArray buildFrame(quint8 moduleAddr, quint8 funcCode, quint8 cmdCode, const QByteArray& payload = {});

bool parseFrame(const QByteArray& raw, quint8* moduleAddr, quint8* funcCode, quint8* cmdCode, QByteArray* payload,
                QString* errorMessage = nullptr);

QByteArray appendLe32(quint32 value);
quint32 readLe32(const QByteArray& data, int offset);
/** 状态回包中的电压/电流平均值按大端；配置下发仍用 appendLe32。 */
quint32 readBe32(const QByteArray& data, int offset);

} // namespace Asd9026aCodec

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // ASD9026A_CODEC_H
