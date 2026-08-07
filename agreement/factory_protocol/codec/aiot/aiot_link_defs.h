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
constexpr uint8_t kFctCidGetExceptionThreshold = 0x0A; // 获取设备阈值
constexpr uint8_t kFctCidSetExceptionThreshold = 0x0B; // 设置设备阈值（掉电不消失）
constexpr uint8_t kFctCidDeviceControl = 0x0C;
/** CID=0x0C 设备控制 TLV（另复用 kFctDeviceSideId=0x01） */
constexpr uint8_t kFctDutControlDataType = 0x02; // dut_control_data_type
constexpr uint8_t kFctDutControlData = 0x03;     // dut_control_data 附加数据
constexpr uint8_t kFctDutCtrlFactoryReset = 0x01;
constexpr uint8_t kFctDutCtrlPowerOff = 0x02;
constexpr uint8_t kFctDutCtrlTravelLock = 0x03;
constexpr uint8_t kFctDutCtrlReset = 0x04; // device_reset 重启
constexpr uint8_t kFctCidGetBatteryInfo = 0x0E;
constexpr uint8_t kFctCidSetPumpParam = 0x0F; // 设置泵阀运行参数
constexpr uint8_t kFctCidGetPumpParam = 0x10; // 获取泵阀运行参数
constexpr uint8_t kFctCidSimulateKey = 0x11;  // 按键模拟测试
constexpr uint8_t kFctCidVirtualBattery = 0x13; // 电量模拟测试，字段置 0=真实
constexpr uint8_t kFctCidHeatTest = 0x14;       // 自定义加热测试
constexpr uint8_t kFctCidVibrationTest = 0x15;  // 自定义振动测试
constexpr uint8_t kFctCidSetCycleReport = 0x18;  // 设置数据采集上报
constexpr uint8_t kFctCidCycleReportNotify = 0x19; // 数据采集被动上报
constexpr uint8_t kFctCidDutNotify = 0x1A;      // 测试数据主动上报（产测模式）

/** CID=0x0E 获取电量 / CID=0x13 电量模拟：battery_* 字段 Type（0x13 置 0=该通道恢复真实值） */
constexpr uint8_t kFctBattPercent = 0x01;     // battery_percent uint8 [0,100]
constexpr uint8_t kFctBattVoltage = 0x02;     // battery_voltage uint16 mV BE
constexpr uint8_t kFctBattCurrent = 0x03;     // battery_current uint16 mA BE
constexpr uint8_t kFctBattTemperature = 0x04; // battery_temperature uint8 °C
/** 兼容旧常量名 */
constexpr uint8_t kFctVirtualBattPercent = kFctBattPercent;
constexpr uint8_t kFctVirtualBattVoltageMv = kFctBattVoltage;
constexpr uint8_t kFctVirtualBattCurrentMa = kFctBattCurrent;
constexpr uint8_t kFctVirtualBattTempC = kFctBattTemperature;

/** CID=0x0F/0x10 泵阀运行参数 */
constexpr uint8_t kFctPumpCircleNum = 0x01;     // SET 循环次数 / GET 已测循环次数
constexpr uint8_t kFctPumpParamStruct = 0x02;   // pump_param_info
constexpr uint8_t kFctPumpDurationTime = 0x03;  // uint16 泵工作时长
constexpr uint8_t kFctPumpIntervalTime = 0x04;  // uint16 泵间隔
constexpr uint8_t kFctValveEnableTime = 0x05;   // uint16 阀使能（规范 value_/valve_ 混用）
constexpr uint8_t kFctValveDisableTime = 0x06;  // uint16 阀关闭
constexpr uint8_t kFctPumpPwmValue = 0x07;      // uint8 泵 PWM 0~100
constexpr uint8_t kFctValvePwmValue = 0x08;     // uint8 阀 PWM 0~100

/** CID=0x14 自定义加热测试 */
constexpr uint8_t kFctHeatStatusStruct = 0x01;    // heat_status_struct
constexpr uint8_t kFctHeatEnable = 0x02;          // uint8 0关/1开
constexpr uint8_t kFctHeatDriveStrength = 0x03;   // uint8 加热强度
constexpr uint8_t kFctHeatDurationTime = 0x04;    // uint16 加热时长（可选）

/** CID=0x15 自定义振动测试 */
constexpr uint8_t kFctVibrationStatusStruct = 0x01;  // vibration_status_struct
constexpr uint8_t kFctVibrationEnable = 0x02;        // uint8 0关/1开
constexpr uint8_t kFctVibrationDriveStrength = 0x03; // uint8 强度
constexpr uint8_t kFctVibrationFreq = 0x04;          // uint8 振动频率
constexpr uint8_t kFctVibrationDurationTime = 0x05;  // uint16 振动时长

/** CID=0x18 设置数据采集上报 */
constexpr uint8_t kFctCycleReportEnable = 0x01;       // cycle_report_enable 0关/1开
constexpr uint8_t kFctCycleReportTypeList = 0x02;     // report_data_type_list
constexpr uint8_t kFctCycleReportConfigStruct = 0x03; // report_data_config_struct
constexpr uint8_t kFctCycleReportCfgDataType = 0x04;  // report_data_type
constexpr uint8_t kFctCycleReportIntervalTime = 0x05; // report_interval_time uint16 ms BE

/** CID=0x19 数据采集被动上报 */
constexpr uint8_t kFctCycleReportDataList = 0x01;     // report_data_list
constexpr uint8_t kFctCycleReportDataStruct = 0x02;   // report_data_struct
constexpr uint8_t kFctCycleReportSampleType = 0x03;   // report_data_type
constexpr uint8_t kFctCycleReportSampleData = 0x04;   // report_data 原始载荷

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

/** 传感器类型 dut_sensor_type（CID=0x08/0x09）/ report_data_type（0x18/0x19） */
constexpr uint8_t kFctSensorTypeImu = 0x00;
constexpr uint8_t kFctSensorTypePressure = 0x01;
constexpr uint8_t kFctSensorTypeAirflow = 0x02;
constexpr uint8_t kFctSensorTypeTof = 0x03;
constexpr uint8_t kFctSensorTypeCapacitive = 0x04; // fsensor 电容/力传感
constexpr uint8_t kFctSensorTypeInfrared = 0x05;
constexpr uint8_t kFctSensorTypeBioimpedance = 0x06;
constexpr uint8_t kFctSensorTypeLiquidLevel = 0x07;
constexpr uint8_t kFctSensorTypeTemperature = 0x08;
constexpr uint8_t kFctSensorTypeHumidity = 0x09;
constexpr uint8_t kFctSensorTypeProximity = 0x0A;
constexpr uint8_t kFctSensorTypeCurrent = 0x0B;
constexpr uint8_t kFctSensorTypeHall = 0x0C;
constexpr uint8_t kFctSensorTypeEncoder = 0x0D;

/** IMU imu_cali_data_t：9×float LE = 36 字节 */
constexpr int kFctImuCaliFloatCount = 9;
constexpr int kFctImuCaliBytes = 36;

/** 异常阈值 exception_type（CID=0x0A/0x0B） */
constexpr uint8_t kFctExTypeBatLowAlarm = 0x01;
constexpr uint8_t kFctExTypeBatLowShutdown = 0x02;
constexpr uint8_t kFctExTypeChargeOvervolt = 0x03;
constexpr uint8_t kFctExTypeChargeTimeout = 0x04;
constexpr uint8_t kFctExTypeBatTempAbnormal = 0x05;
constexpr uint8_t kFctExTypeMotorStallOvercurrent = 0x11;
constexpr uint8_t kFctExTypeMotorOpenCircuit = 0x12;
constexpr uint8_t kFctExTypeNegPressureHigh = 0x22;

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
