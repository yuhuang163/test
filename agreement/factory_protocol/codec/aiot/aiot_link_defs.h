#ifndef AIOT_LINK_DEFS_H
#define AIOT_LINK_DEFS_H

#include <cstdint>

/** Momcozy AIOT 数据链路层（FCT&ATE 协议规范 v1.0.0） */
namespace AiotLink {

constexpr uint8_t kSof = 0x5A;

/** Control 位域 */
constexpr uint8_t kCtrlRsp = 0x08; // bit3
constexpr uint8_t kCtrlAck = 0x04; // bit2
constexpr uint8_t kCtrlFsnMask = 0x03; // bit1..0
constexpr uint8_t kCtrlFsnNone = 0x00; // 完整应用层包，无 FSN
constexpr uint8_t kCtrlFsnStart = 0x01;
constexpr uint8_t kCtrlFsnMiddle = 0x02;
constexpr uint8_t kCtrlFsnEnd = 0x03;

/** 应用层 Service ID：Qaiot 只走 FCT&ATE */
constexpr uint8_t kSvcFctAte = 0x04;

/** FCT&ATE Service CID */
constexpr uint8_t kFctCidGetFactoryStatus = 0x01;
constexpr uint8_t kFctCidSetFactoryStatus = 0x02;
constexpr uint8_t kFctCidGetDeviceData = 0x03;
constexpr uint8_t kFctCidSetDeviceData = 0x04;
constexpr uint8_t kFctCidSetRfTest = 0x05;
constexpr uint8_t kFctCidGetRfData = 0x06;
constexpr uint8_t kFctCidSetRfData = 0x07;
constexpr uint8_t kFctCidGetSensor = 0x08;
constexpr uint8_t kFctCidSetSensor = 0x09;
constexpr uint8_t kFctCidDeviceControl = 0x0C;
constexpr uint8_t kFctCidGetBatteryInfo = 0x0E;
constexpr uint8_t kFctCidSimulateKey = 0x10;

/** 获取产测状态 CID=0x01 的 TLV Type */
constexpr uint8_t kFctGetTlvDeviceName = 0x01;
constexpr uint8_t kFctGetTlvFwVersion = 0x02;
constexpr uint8_t kFctGetTlvMac = 0x03;
constexpr uint8_t kFctGetTlvFactoryComplete = 0x04;
constexpr uint8_t kFctGetTlvHwVersion = 0x05;
constexpr uint8_t kFctGetTlvResVersion = 0x06;
constexpr uint8_t kFctGetTlvModeList = 0x20;
constexpr uint8_t kFctGetTlvModeStruct = 0x21;
constexpr uint8_t kFctGetTlvModeType = 0x22;
constexpr uint8_t kFctGetTlvModeStatus = 0x23;

/** 设置产测状态 CID=0x02 的 TLV Type（与 GET 编号空间不同） */
constexpr uint8_t kFctSetTlvFactoryComplete = 0x01;
constexpr uint8_t kFctSetTlvModeList = 0x02;
constexpr uint8_t kFctSetTlvModeStruct = 0x03;
constexpr uint8_t kFctSetTlvModeType = 0x04;
constexpr uint8_t kFctSetTlvModeEnable = 0x05;

/** 工厂模式类型 factory_mode_type */
constexpr uint8_t kFctModeIdle = 0x00;
constexpr uint8_t kFctModeFactoryTest = 0x01;
constexpr uint8_t kFctModeAging = 0x02;
constexpr uint8_t kFctModeSuction = 0x03;
constexpr uint8_t kFctModeSuctionCompensate = 0x04;
constexpr uint8_t kFctModeAte = 0x05;

/** 通用错误码 Type */
constexpr uint8_t kTlvErrorCode = 127;

} // namespace AiotLink

#endif // AIOT_LINK_DEFS_H
