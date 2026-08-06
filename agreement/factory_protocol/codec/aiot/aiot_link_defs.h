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
constexpr uint8_t kFctCidVirtualBattery = 0x13; // 电量模拟测试，字段置 0=真实
constexpr uint8_t kFctCidDutNotify = 0x1A;      // 测试数据主动上报（产测模式）

/** CID=0x13 电量模拟 TLV（均可选；置 0 表示该通道恢复真实值） */
constexpr uint8_t kFctVirtualBattPercent = 0x01;    // uint8 0–100
constexpr uint8_t kFctVirtualBattVoltageMv = 0x02;  // uint16 mV
constexpr uint8_t kFctVirtualBattCurrentMa = 0x03;  // int16 mA（放电为负）
constexpr uint8_t kFctVirtualBattTempC = 0x04;      // int8 °C

/** CID=0x1A 主动上报 TLV */
constexpr uint8_t kFctDutNotifyList = 0x01;
constexpr uint8_t kFctDutNotifyStruct = 0x02;
constexpr uint8_t kFctDutNotifyType = 0x03;
constexpr uint8_t kFctDutNotifyValue = 0x04;
constexpr uint8_t kFctDutNotifyTypeVirtualKey = 0x00; // virtual_key_value 按键

/** 获取产测状态 CID=0x01 的 TLV Type（MAC 已迁至通用设备数据 Type=0x05） */
constexpr uint8_t kFctGetTlvDeviceName = 0x01;
constexpr uint8_t kFctGetTlvFwVersion = 0x02;
constexpr uint8_t kFctGetTlvResVersion = 0x03;
constexpr uint8_t kFctGetTlvFactoryComplete = 0x04;
constexpr uint8_t kFctGetTlvHwVersion = 0x05;
/** GET/SET 共用：工厂模式 list/struct/type/status */
constexpr uint8_t kFctGetTlvModeList = 0x20;
constexpr uint8_t kFctGetTlvModeStruct = 0x21;
constexpr uint8_t kFctGetTlvModeType = 0x22;
constexpr uint8_t kFctGetTlvModeStatus = 0x23;

/** 设置产测状态 CID=0x02：完成标识 Type=0x01（GET 完成标识为 0x04） */
constexpr uint8_t kFctSetTlvFactoryComplete = 0x01;

/** 通用设备数据 CID=0x03/0x04：device_data_type */
/** CID=0x03/0x04 通用设备数据：device_side_id Type=0x01 */
constexpr uint8_t kFctDeviceSideId = 0x01;
constexpr uint8_t kFctDeviceSideLeft = 0x00;
constexpr uint8_t kFctDeviceSideRight = 0x01;
constexpr uint8_t kFctDeviceSideIndependent = 0x02;
/** device_data_timestap：UTC 秒，uint32 大端 */
constexpr uint8_t kFctDeviceDataTimestamp = 0x02;

constexpr uint8_t kFctDataTypeSn = 0x01;
constexpr uint8_t kFctDataTypeProductId = 0x02;
constexpr uint8_t kFctDataTypeDeviceId = 0x03;
constexpr uint8_t kFctDataTypeDeviceSecret = 0x04;
constexpr uint8_t kFctDataTypeMac = 0x05;

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
