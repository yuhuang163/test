#ifndef UNIVERSAL_FIXTURE_TYPES_H
#define UNIVERSAL_FIXTURE_TYPES_H

#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 通用治具串口协议 V1.2：通道 CH / 状态码（见 docs/协议文档/通用治具串口协议.md） */
namespace UniversalFixture {

constexpr quint8 kSof = 0x55;
constexpr quint8 kEof = 0xAA;
constexpr quint8 kTypeControl = 0x01;
constexpr quint8 kTypeMeasure = 0x02;
constexpr quint8 kTypeControlAck = 0x81;
constexpr quint8 kTypeMeasureRsp = 0x82;
constexpr quint8 kMeasureLen = 0x0B;

constexpr quint8 kChRelay1 = 0x01;
constexpr quint8 kChRelay2 = 0x02;
constexpr quint8 kChRelay3 = 0x03;
constexpr quint8 kChRelay4 = 0x04;
constexpr quint8 kChCylinder = 0x05;
constexpr quint8 kChCurrentAll = 0x00;
constexpr quint8 kChCurrent1 = 0x01;
constexpr quint8 kChCurrent2 = 0x02;

constexpr quint8 kStOff = 0x00;
constexpr quint8 kStOn = 0x01;
constexpr quint8 kOpReadOnce = 0x01;

constexpr quint8 kSlotBroadcast = 0x00;

enum class ControlStatus : quint8 {
    Ok = 0x00,
    InvalidSlotOrCh = 0x01,
    HardwareFault = 0x03,
    Busy = 0x04,
    PositionTimeout = 0x05,
    Interlock = 0x06,
    Unknown = 0xFF,
};

struct ControlAck {
    quint8 slot = 0;
    quint8 ch = 0;
    quint8 st = 0;
    quint8 status = 0xFF;
};

struct MeasureRsp {
    quint8 slot = 0;
    quint8 ch = 0;
    qint16 currentRaw = 0; // 单位 0.01 mA
    quint8 status = 0xFF;
    double currentMa() const { return static_cast<double>(currentRaw) / 100.0; }
};

} // namespace UniversalFixture

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // UNIVERSAL_FIXTURE_TYPES_H
