#include "device_cmd_manifest.h"

namespace {

using DeviceCmdManifest::Row;

constexpr uint8_t kSet = DeviceCmdManifest::kSendActionSet;
constexpr uint8_t kGet = DeviceCmdManifest::kSendActionGet;
constexpr uint8_t kBoth = DeviceCmdManifest::kSendActionBoth;

// hint 单独定义，避免 MSVC 宏参数中的括号/换行问题
constexpr const char kHintForbidSleep[] =
    u8"禁止休眠：value=1 开启，0 关闭\r\n示例：{\"value\":1} 或 value=1";
constexpr const char kHintSn[] =
    u8"读写 SN：which_sn 类型\r\n"
    u8"  1=整机SN(TAIL)  6=deviceName(SUB_PID)  7=productKey(SKUID)\r\n"
    u8"写 productKey：which_sn=7，sn=$TUPLE_PRODUCT_KEY\r\n"
    u8"写 deviceName：which_sn=6，sn=$TUPLE_DEVICE_NAME\r\n"
    u8"写整机 SN：which_sn=1，sn=$WHOLE_MACHINE_SN（本地整机SN）或具体值\r\n"
    u8"  PCBA SN：$PCBA_SN / $SN（扫码/MES，解析 MAC 等，勿用于写整机）\r\n"
    u8"Qaiot device_side_id：Param_side=0 Left / 1 Right / 2 Independent（默认2）\r\n"
    u8"  也可用 left/right/independent 或 Param_position=L/R\r\n"
    u8"Qroot：读 0xA0 / 写 0xA1(≤40)；which_sn=7→0xF8；which_sn=6→0xF9；读 6/7 兼容 0xF7";
constexpr const char kHintRootHeatLevelControl[] =
    u8"Qroot：Req 0x83，CAL=0x02，body：switch(0关1开)+level(0=L1/1=L2/2=L3)\r\n"
    u8"示例：Param_switch=1 Param_level=1 → 开 L2\r\n"
    u8"卡控（可选）：ReportType=ProtocolResultData，Field=result，Expected=1";
constexpr const char kHintTupleRead[] =
    u8"Qaiot：Param_dataType=2/3/4 只读单项；Param_side=0/1/2（Left/Right/Independent，默认2）\r\n"
    u8"比对在步骤逻辑中与云端三元组比较\r\n"
    u8"Qroot：Req 0xF7，Ack 30B=ProductID(6)+DeviceID(16)+KeyTail(8 RC4密文)\r\n"
    u8"密钥校验：用完整 16B deviceSecret 对 KeyTail 做 RC4 解密，应等于密钥后 8 字节";
constexpr const char kHintWriteKey[] =
    u8"写 deviceSecret：value=$TUPLE_DEVICE_SECRET（默认取云端已获取三元组）\r\n"
    u8"Qaiot：Param_side=0 Left / 1 Right / 2 Independent（默认2）\r\n"
    u8"Qroot：Req 0xFA，密钥 ≤16 字节截断";
constexpr const char kHintSoftVersionRead[] =
    u8"Qroot：Req 0x91，回包 soft_version(2B)+hw_version(1B)\r\n"
    u8"Qaiot：Param_field=soft_version|hw_version|res_version（默认固件）\r\n"
    u8"卡控：ReportType=ProtocolBaseInfoData，Field 与上对应，Op=compareVersions";
constexpr const char kHintPeriphState[] =
    u8"Qaiot 读传感器 CID=0x08：Param_type=0x00~0x0D\r\n"
    u8"00 IMU(回 36B=9×float LE: kx..bz) / 04 电容fsensor(回 1B 0/1)\r\n"
    u8"01 压力 / 02 气流 / 03 TOF / 05 红外 / 06 生物阻抗\r\n"
    u8"07 液位 / 08 温度 / 09 湿度 / 0A 接近 / 0B 电流 / 0C 霍尔 / 0D 编码器\r\n"
    u8"卡控：ProtocolAiotImuCaliData Field=kx..bz；ProtocolAiotFsensorCaliData Field=calibrated";
constexpr const char kHintLightCalibWrite[] =
    u8"Qfctp 测试服务 TLV 0x001E：Param_index=0~19 Param_value=校准值(int32 小端)\r\n"
    u8"请求 Length 必须为 5（1 字节索引 + 4 字节校准值）\r\n"
    u8"示例：Param_index=0 Param_value=1\r\n"
    u8"Qaiot 写传感器校准 CID=0x09：Param_type + 校准数据\r\n"
    u8"IMU(type=0)：Param_kx..Param_bz 九个 float，或 Param_data=72位hex(36B LE)\r\n"
    u8"电容/fsensor(type=4)：Param_calibrated=0|1 或 Param_data=00/01\r\n"
    u8"其它类型：Param_data=hex（最长64B）";
constexpr const char kHintLightCalibRead[] =
    u8"Qfctp 测试服务 TLV 0x001F：Param_index=0~19\r\n"
    u8"应答 4 字节 int32 小端；索引非法时固件返回全 0\r\n"
    u8"示例：Param_index=0\r\n"
    u8"卡控：ReportType=ProtocolLightCalibData Field=calibValue";
constexpr const char kHintExceptionThresholdRead[] =
    u8"Qaiot CID=0x0A 读异常阈值：Param_type 可选（缺省读全部）\r\n"
    u8"01低电告警% 02低电关机% 03充电过压mV 04充电超时s 05电池温度(低+高)\r\n"
    u8"11电机堵转mA 12电机开路mA 22负压过高\r\n"
    u8"卡控：ReportType=ProtocolAiotExceptionThresholdData Field=value/valueHigh";
constexpr const char kHintExceptionThresholdWrite[] =
    u8"Qaiot CID=0x0B 写异常阈值（掉电不消失）：Param_type + 数值\r\n"
    u8"01/02：Param_value=电量%  03：Param_value/voltageMv  04：Param_value/seconds\r\n"
    u8"05：Param_low+Param_high（°C）  11/12：Param_value/currentMa  22：Param_value";
constexpr const char kHintPumpParamRead[] =
    u8"Qaiot CID=0x10 读泵运行参数（与阀分开）\r\n"
    u8"回包字段：circleNum / durationTime / intervalTime / pumpPwm\r\n"
    u8"卡控：ReportType=ProtocolAiotPumpParamData Field=durationTime/pumpPwm…";
constexpr const char kHintPumpParamWrite[] =
    u8"Qaiot CID=0x0F 写泵运行参数（不写阀）\r\n"
    u8"Param_circleNum / durationTime / intervalTime / pumpPwm(0~100)";
constexpr const char kHintValveParamRead[] =
    u8"Qaiot CID=0x10 读阀运行参数（与泵分开）\r\n"
    u8"回包字段：valveEnableTime / valveDisableTime / valvePwm\r\n"
    u8"卡控：ReportType=ProtocolAiotPumpParamData Field=valveEnableTime/valvePwm…";
constexpr const char kHintValveParamWrite[] =
    u8"Qaiot CID=0x0F 写阀运行参数（不写泵）\r\n"
    u8"Param_valveEnableTime / valveDisableTime / valvePwm(0~100)\r\n"
    u8"兼容：value_enable_time / value_pwm_value";
constexpr const char kHintHeatTestWrite[] =
    u8"Qaiot CID=0x14 自定义加热测试（回包结果反馈）\r\n"
    u8"Param_enable=0|1（必选）  Param_driveStrength=强度/PWM（必选）\r\n"
    u8"Param_durationTime=加热时长（可选 uint16）\r\n"
    u8"卡控：ReportType=ProtocolAiotHeatTestData Field=enable/driveStrength/durationTime";
constexpr const char kHintVibrationTestWrite[] =
    u8"Qaiot CID=0x15 自定义振动测试（回包结果反馈）\r\n"
    u8"Param_enable=0|1  Param_driveStrength=强度  Param_freq=频率  Param_durationTime=时长\r\n"
    u8"（均为必选）\r\n"
    u8"卡控：ReportType=ProtocolAiotVibrationTestData Field=enable/driveStrength/freq/durationTime";
constexpr const char kHintCycleReportWrite[] =
    u8"Qaiot CID=0x18 设置数据采集循环上报\r\n"
    u8"Param_enable=0|1（必选）\r\n"
    u8"单条：Param_type=0x00~0x0D Param_intervalTime=周期ms\r\n"
    u8"多条：Param_types=0,1,8 Param_intervals=100,200,500\r\n"
    u8"或 Param_items=[{\"type\":0,\"intervalTime\":100},...]\r\n"
    u8"类型：00IMU 01压力 02气流 03TOF 04电容 05红外 06生物阻抗 07液位\r\n"
    u8"      08温度 09湿度 0A接近 0B电流 0C霍尔 0D编码器\r\n"
    u8"被动上报 CID=0x19：ReportType=ProtocolAiotCycleReportData Field=dataType/accX/…\r\n"
    u8"卡控配置回包：ReportType=ProtocolAiotCycleReportConfigData Field=enable/dataType/intervalTime";
constexpr const char kHintAgingStatusRead[] =
    u8"Qaiot CID=0x01 读工厂模式：Param_mode=0~5（默认 2 老化）\r\n"
    u8"0 idle / 1 factory / 2 aging / 3 suction / 4 compensate / 5 ate\r\n"
    u8"老化(mode=2) Ack 0x23 可为 18B：使能+完成+双温+堵转次数(2)+阈值(2)+电流×5\r\n"
    u8"卡控/显示：ReportType=ProtocolRootAgingHistoryData\r\n"
    u8"Field=status/finishedFlag/batteryMaxTempC/flangeMaxTempC/stallCount/stallThreshold/stallCurrent0~4";
constexpr const char kHintGetBattery[] =
    u8"Qroot：Notify 0xE0+0x01 查询；回包 percent(1B)+voltage 大端 0.01V\r\n"
    u8"Qaiot CID=0x0E：Param_field=percent|voltage|current|temperature（默认四项）\r\n"
    u8"字段 Type：01 percent%  02 voltage mV  03 current mA  04 temperature °C\r\n"
    u8"卡控：ReportType=ProtocolBatteryData，Field=percent/voltageMv/currentMa/temperatureC";
constexpr const char kHintRootBatteryTemp[] =
    u8"Qroot：Notify 0x80+0x01 查询电池温度\r\n"
    u8"卡控：ReportType=ProtocolBatteryTempData，Field=type";
constexpr const char kHintRootHeatTemp[] =
    u8"Qroot：Notify 0x98+0x01 查询加热温度；设备 Notify 回 1B 温度\r\n"
    u8"卡控（可选）：ReportType=ProtocolHeatTempData，Field=type";
constexpr const char kHintFactoryReset[] =
    u8"Qroot：Notify 0xFC+0x04 恢复出厂；应答 0xE0/0xFC 返回 0x04\r\n"
    u8"Qaiot CID=0x0C：side + type=0x01(factory_reset) + data\r\n"
    u8"可选 Param_side=0/1/2；Param_data=附加数据 hex\r\n"
    u8"卡控：ReportType=ProtocolResultData，Field=result，Expected=1";
constexpr const char kHintShipMode[] =
    u8"Qfctp：关机 TLV；Qroot：Req 0xFC+0x01 关机\r\n"
    u8"Qaiot CID=0x0C：side + type=0x02(power_off) + data\r\n"
    u8"可选 Param_side=0/1/2；Param_data=附加数据 hex\r\n"
    u8"卡控（可选）：ReportType=ProtocolResultData，Field=result，Expected=1";
constexpr const char kHintDevReset[] =
    u8"Qpb：设备复位；Qroot：Req 0xFC+0x02 重启\r\n"
    u8"Qaiot CID=0x0C：side + type=0x04(device_reset 重启) + data\r\n"
    u8"可选 Param_side=0/1/2；Param_data=附加数据 hex\r\n"
    u8"卡控（可选）：ReportType=ProtocolResultData，Field=result，Expected=1";
constexpr const char kHintTravelLock[] =
    u8"Qaiot CID=0x0C：side + type=0x03(travel_lock) + data\r\n"
    u8"可选 Param_side=0/1/2；Param_data=附加数据 hex";
constexpr const char kHintRootAgingHistory[] =
    u8"Qroot：Req 0x9C 读取老化历史信息（无参）\r\n"
    u8"Ack 16B：次数(1)+电池最高温℃(1)+法兰最高温℃(1)+堵转次数(1)+泵阀堵转阈值(2 BE)+最新堵转电流×5(10 BE)\r\n"
    u8"卡控：ReportType=ProtocolRootAgingHistoryData，Field=multi\r\n"
    u8"子字段 agingCount / batteryMaxTempC / flangeMaxTempC（stallCount/stallThreshold/stallCurrent0~4 仅显示）";
constexpr const char kHintRootEnterOta[] =
    u8"Qroot：Req 0xFC+0x03 进入 OTA\r\n"
    u8"卡控（可选）：ReportType=ProtocolResultData，Field=result，Expected=1";
constexpr const char kHintRootSystemControl[] =
    u8"【兼容旧配置】Qroot Req 0xFC，Param_command=1关机 2重启 3OTA 4复位\r\n"
    u8"新步骤请改用：ShipMode / DevReset / RootEnterOta / FactoryReset\r\n"
    u8"卡控（可选）：ReportType=ProtocolResultData，Field=result，Expected=1";
constexpr const char kHintLedTest[] =
    u8"Qroot：Req 0x93，on=1 全亮 / on=0 全灭\r\n"
    u8"Qfctp：系统配置 TLV 0x0005，on=1 全亮 / on=0 全灭\r\n"
    u8"示例：Param_on=1 或 {\"on\":1}";
constexpr const char kHintLcdBacklight[] =
    u8"Qfctp：测试服务 TLV 0x001C，on=1 开背光 / on=0 关背光\r\n"
    u8"示例：Param_on=1 或 {\"on\":0}";
constexpr const char kHintButtonState[] =
    u8"Qaiot：模拟按键 CID=0x10，Param_int/Param_key=0x01~0x0B\r\n"
    u8"01电源 02开始 03模式 04频率 05母乳 06左控 07右控 08恢复出厂 09旅行锁 0A旋钮左 0B旋钮右\r\n"
    u8"Qroot：Notify 0x9A，1=开启按键上报 / 0=关闭；Ack body=0xFF 表示已接收\r\n"
    u8"Qpb：Param_int 为按键索引";
constexpr const char kHintRootSuctionTest[] =
    u8"Qroot：Req 0x81，CAL=0x03，body：switch(0关1开)+mode(00按摩/01吸乳/02混合)+level(强度)\r\n"
    u8"示例：Param_switch=1 Param_mode=1 Param_level=8 → 开吸乳强度8\r\n"
    u8"卡控（可选）：ReportType=ProtocolResultData，Field=result，Expected=1\r\n"
    u8"注意：FCTP「进入/退出吸力测试模式」请改用「吸力模式(进/退)」+ Param_enter";
constexpr const char kHintRootPumpControl[] =
    u8"Qroot：Req 0xC0 吸奶器控制，CAL=0x0A\r\n"
    u8"Param_status=0停止1运行；mode=吸奶模式1~7；level=档位1~15\r\n"
    u8"Param_customMode=自定义模式；customLevel=自定义档位；customFreq=1低2中3高\r\n"
    u8"Param_controlType=0左1右2双边；pumpFreq=0低1中2高\r\n"
    u8"卡控（可选）：ReportType=ProtocolTypeData，Field=type，Expected=controlType";
constexpr const char kHintBaseInfo[] =
    u8"QPB：读写基础信息（含软件/资源版本等）\r\nFCTP 请改用 SoftVersionRead（读取版本号）";
constexpr const char kHintFacResult[] =
    u8"产测结果：done=1 通过(留空等同1)，done=0 失败\r\n示例：done=1 或 {\"done\":1}";
constexpr const char kHintBurningMode[] =
    u8"老化：mode、seconds，可选 switch/enter\r\nQroot 进入老化：Notify 0xAF+0x01\r\n示例：{\"mode\":1,\"seconds\":3600}";
constexpr const char kHintSleep[] = u8"休眠：switch=1 进入，0 退出\r\n示例：{\"switch\":1}";
constexpr const char kHintFacMode[] =
    u8"工厂模式：Param_on/value=1 进入 0 退出；Param_mode 模式类型\r\n"
    u8"0 idle / 1 factory_test / 2 aging / 3 suction / 4 suction_compensate / 5 ate\r\n"
    u8"示例：Param_mode=1 Param_on=1";
constexpr const char kHintSuctionMode[] =
    u8"FCTP 吸力测试模式：enter=1 进入，0 退出\r\n示例：Param_enter=1 或 {\"enter\":1}\r\n"
    u8"注意：Qroot 开泵档位请改用「吸力测试(档位)」+ switch/mode/level";
constexpr const char kHintBtRfMode[] =
    u8"蓝牙 RF 测试模式开关：Param_enter 或 Param_on，1=开/进入，0=关/退出\r\n"
    u8"示例：进入 Param_enter=1；退出 Param_enter=0（或 Param_on=1/0）\r\n"
    u8"进非信令后设备可能关机：步骤 Timing/WaitReply=false（发完即过，不等回包）\r\n"
    u8"Qfctp：TLV 单字节；Qaiot：CID 射频测试 enable 字节";
constexpr const char kHintWifiConnect[] =
    u8"WiFi：name=SSID，password=密码\r\n示例：name=TestAP\r\npassword=12345678";
constexpr const char kHintRssiRead[] = u8"RSSI：mode=0 读 BLE，mode=1 读 BT\r\n示例：{\"mode\":0}";
constexpr const char kHintMacWrite[] =
    u8"写 MAC：value=MAC 或 $MAC\r\n"
    u8"Qaiot：CID=0x04 device_data_type=0x05；Param_side=0/1/2（Left/Right/Independent，默认2）\r\n"
    u8"示例：Param_value=$MAC 或 {\"value\":\"$MAC\"}";
constexpr const char kHintMacRead[] =
    u8"读 MAC\r\n"
    u8"Qaiot：CID=0x03 device_data_type=0x05；Param_side=0/1/2（Left/Right/Independent，默认2）";

// 新增指令：在此表增加一行；协议实现见 qfctp.cpp / qpb.cpp 的 set/get。
const Row kRows[] = {
    {DeviceCmd::ForbidSleep, "ForbidSleep", u8"禁止休眠", DeviceCmdParamKind::JsonMap, kHintForbidSleep, kSet},
    {DeviceCmd::Sn, "Sn", u8"序列号", DeviceCmdParamKind::JsonMap, kHintSn, kBoth, "ProtocolSnData", "value"},
    {DeviceCmd::SoftVersionRead, "SoftVersionRead", u8"版本号", DeviceCmdParamKind::JsonMap, kHintSoftVersionRead, kGet,
     "ProtocolBaseInfoData", "soft_version"},
    {DeviceCmd::BaseInfo, "BaseInfo", u8"基本信息", DeviceCmdParamKind::None, kHintBaseInfo, kGet, "ProtocolBaseInfoData",
     "soft_version"},
    {DeviceCmd::GetBattery, "GetBattery", u8"电量", DeviceCmdParamKind::JsonMap, kHintGetBattery, kGet,
     "ProtocolBatteryData", "percent"},
    {DeviceCmd::SetBattery, "SetBattery", u8"设置电池/模拟电量", DeviceCmdParamKind::JsonMap,
     u8"Qpb：0=两节电池 1=单节电池\r\n"
     u8"Qaiot CID=0x13（可选 TLV，0=该通道真实值）：\r\n"
     u8"  Param_percent=0~100（Type=0x01）\r\n"
     u8"  Param_voltageMv=mV（Type=0x02 uint16）\r\n"
     u8"  Param_currentMa=mA（Type=0x03 uint16）\r\n"
     u8"  Param_temperatureC=°C（Type=0x04 uint8）\r\n"
     u8"示例：Param_percent=50；恢复百分比：Param_percent=0",
     kSet},
    {DeviceCmd::FacResult, "FacResult", u8"产测结果", DeviceCmdParamKind::JsonMap, kHintFacResult, kSet},
    {DeviceCmd::BurningMode, "BurningMode", u8"老化模式", DeviceCmdParamKind::JsonMap, kHintBurningMode, kSet},
    {DeviceCmd::Sleep, "Sleep", u8"休眠", DeviceCmdParamKind::JsonMap, kHintSleep, kSet},
    {DeviceCmd::ShipMode, "ShipMode", u8"关机", DeviceCmdParamKind::JsonMap, kHintShipMode, kSet, "ProtocolResultData",
     "result"},
    {DeviceCmd::FacMode, "FacMode", u8"工厂模式", DeviceCmdParamKind::JsonMap, kHintFacMode, kSet},
    {DeviceCmd::DevReset, "DevReset", u8"设备重启", DeviceCmdParamKind::JsonMap, kHintDevReset, kSet, "ProtocolResultData",
     "result"},
    {DeviceCmd::TravelLock, "TravelLock", u8"旅行锁", DeviceCmdParamKind::JsonMap, kHintTravelLock, kSet},
    {DeviceCmd::WifiDisconnect, "WifiDisconnect", u8"断开无线网络", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::WifiConnect, "WifiConnect", u8"连接无线网络", DeviceCmdParamKind::JsonMap, kHintWifiConnect, kSet},
    {DeviceCmd::RssiRead, "RssiRead", u8"信号强度", DeviceCmdParamKind::JsonMap, kHintRssiRead, kGet, "ProtocolRssiData",
     "dbm"},
    {DeviceCmd::ChargeCurrentRead, "ChargeCurrentRead", u8"读取充电电流", DeviceCmdParamKind::None,
     u8"FCTP TLV 0x0020，应答电流单位 mA；无电池节点或查询失败时为 0", kGet, "ProtocolChargeCurrentData", "currentMa"},
    {DeviceCmd::ChargeCurrentSet, "ChargeCurrentSet", u8"设置充电电流", DeviceCmdParamKind::JsonMap,
     u8"FCTP TLV 0x0022，单位 mA（uint16 小端）\r\n示例：{\"currentMa\":2000} 或 {\"value\":2000}", kSet},
    {DeviceCmd::TupleRead, "TupleRead", u8"三元组", DeviceCmdParamKind::JsonMap, kHintTupleRead, kGet, "ProtocolTupleData",
     "productId"},
    {DeviceCmd::PeriphState, "PeriphState", u8"外设状态", DeviceCmdParamKind::JsonMap, kHintPeriphState, kGet,
     "ProtocolPeriphStateData,ProtocolAiotImuCaliData,ProtocolAiotFsensorCaliData", "press0_state"},
    {DeviceCmd::FactoryReset, "FactoryReset", u8"恢复出厂", DeviceCmdParamKind::JsonMap, kHintFactoryReset, kSet,
     "ProtocolResultData", "result"},
    {DeviceCmd::RootEnterOta, "RootEnterOta", u8"进入OTA", DeviceCmdParamKind::None, kHintRootEnterOta, kSet,
     "ProtocolResultData", "result"},
    {DeviceCmd::RootSystemControl, "RootSystemControl", u8"系统控制(兼容)", DeviceCmdParamKind::JsonMap,
     kHintRootSystemControl, kSet, "ProtocolResultData", "result"},
    {DeviceCmd::RootBatteryTempQuery, "RootBatteryTempQuery", u8"电池温度", DeviceCmdParamKind::None, kHintRootBatteryTemp,
     kGet, "ProtocolBatteryTempData", "type"},
    {DeviceCmd::RootVibration, "RootVibration", u8"振子控制", DeviceCmdParamKind::JsonMap, u8"0=停止 1=震动\r\n示例：{\"value\":1}", kSet},
    {DeviceCmd::RootFlangeQuery, "RootFlangeQuery", u8"法兰状态", DeviceCmdParamKind::None,
     u8"0=无法兰 1=加热法兰 2=震动法兰 0xA0=二合一\r\n卡控（可选）：ReportType=ProtocolFlangeData，Field=type", kGet,
     "ProtocolFlangeData", "type"},
    {DeviceCmd::RootNtcQuery, "RootNtcQuery", u8"加热NTC", DeviceCmdParamKind::None, nullptr, kGet, "ProtocolTypeData",
     "type"},
    {DeviceCmd::RootHeatTempQuery, "RootHeatTempQuery", u8"加热温度", DeviceCmdParamKind::None, kHintRootHeatTemp, kGet,
     "ProtocolHeatTempData", "type"},
    {DeviceCmd::RootVibStatusQuery, "RootVibStatusQuery", u8"振子状态", DeviceCmdParamKind::None, nullptr, kGet,
     "ProtocolTypeData", "type"},
    {DeviceCmd::RootPumpTestEnter, "RootPumpTestEnter", u8"主机泵测试进入", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::RootPumpTestExit, "RootPumpTestExit", u8"主机泵测试退出", DeviceCmdParamKind::None, nullptr, kSet},
    // 勿与 SuctionMode（FCTP 进/退吸力模式）混淆：本项为 Qroot 开泵+模式+档位
    {DeviceCmd::RootSuctionTest, "RootSuctionTest", u8"吸力测试(档位)", DeviceCmdParamKind::JsonMap, kHintRootSuctionTest, kSet},
    {DeviceCmd::RootPumpStallCurrentQuery, "RootPumpStallCurrentQuery", u8"泵堵电流", DeviceCmdParamKind::None,
     u8"Ack 2B 堵转 ADC（大端 last_adc_value）\r\n卡控（可选）：ReportType=ProtocolPumpStallCurrentData，Field=adcValue",
     kGet, "ProtocolPumpStallCurrentData", "adcValue"},
    {DeviceCmd::RootAgingHistoryQuery, "RootAgingHistoryQuery", u8"老化历史信息", DeviceCmdParamKind::None,
     kHintRootAgingHistory, kGet, "ProtocolRootAgingHistoryData", "agingCount"},
    {DeviceCmd::RootHeatLevelControl, "RootHeatLevelControl", u8"加热档位", DeviceCmdParamKind::JsonMap,
     kHintRootHeatLevelControl, kSet, "ProtocolResultData", "result"},
    {DeviceCmd::RootPumpControl, "RootPumpControl", u8"吸奶器控制", DeviceCmdParamKind::JsonMap, kHintRootPumpControl, kSet},
    {DeviceCmd::PressSensorTemp, "PressSensorTemp", u8"压力传感器温度", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::UartReceive, "UartReceive", u8"串口接收开关", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::RgbColor, "RgbColor", u8"RGB颜色", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorCali, "MotorCali", u8"电机校准", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorDampingState, "MotorDampingState", u8"电机阻尼状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorTestState, "MotorTestState", u8"电机测试状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorCaliState, "MotorCaliState", u8"电机校准状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::ScreenColor, "ScreenColor", u8"屏幕颜色", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LedColor, "LedColor", u8"指示灯颜色", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorAdcSwitch, "MotorAdcSwitch", u8"电机ADC开关", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorParam, "MotorParam", u8"电机参数", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorState, "MotorState", u8"电机运行状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MotorCaliResultParam, "MotorCaliResultParam", u8"电机校准结果参数", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::Music, "Music", u8"音乐", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BrushRecord, "BrushRecord", u8"刷牙记录", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BrushTime, "BrushTime", u8"刷牙时间", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CameraState, "CameraState", u8"摄像头状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::ScreenCameraState, "ScreenCameraState", u8"屏幕摄像头状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CameraLightState, "CameraLightState", u8"摄像头补光状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CameraSupportState, "CameraSupportState", u8"摄像头支持状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CameraExposureTime, "CameraExposureTime", u8"摄像头曝光时间", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BrushReset, "BrushReset", u8"刷牙复位", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::PressCaliResult, "PressCaliResult", u8"压力校准结果", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::ImuCaliResult, "ImuCaliResult", u8"惯性校准结果", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::NewImuCaliResult, "NewImuCaliResult", u8"新惯性校准结果", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::DeviceMode, "DeviceMode", u8"设备模式", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BrushControl, "BrushControl", u8"刷牙控制", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CameraPictureState, "CameraPictureState", u8"摄像头拍照状态", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LocalOta, "LocalOta", u8"本地固件升级", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::StartOtaApp, "StartOtaApp", u8"升级应用", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::IAmApp, "IAmApp", u8"应用身份声明", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::ConfigNetworkApp, "ConfigNetworkApp", u8"配网应用", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::StartMultiBleOtaApp, "StartMultiBleOtaApp", u8"多设备蓝牙升级", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::PressCollect, "PressCollect", u8"压力采集", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::ImuCollect, "ImuCollect", u8"惯性传感器采集", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CameraFaultDataPacket, "CameraFaultDataPacket", u8"摄像头故障数据", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::ServoMotorInfo, "ServoMotorInfo", u8"舵机信息", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MicControl, "MicControl", u8"麦克风控制", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::UploadRecordData, "UploadRecordData", u8"记录数据上传", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::NewWifiConnect, "NewWifiConnect", u8"无线网络（新协议）", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::SevorMotorParam, "SevorMotorParam", u8"舵机参数", DeviceCmdParamKind::None, nullptr, kSet},
    // FCTP TLV 进/退吸力测试模式；「进入吸力测试模式」步骤应选本项 + Param_enter=1
    {DeviceCmd::SuctionMode, "SuctionMode", u8"吸力模式(进/退)", DeviceCmdParamKind::JsonMap, kHintSuctionMode, kSet},
    {DeviceCmd::BtSignalMode, "BtSignalMode", u8"蓝牙信号模式", DeviceCmdParamKind::JsonMap, kHintBtRfMode, kSet},
    {DeviceCmd::BtNoSignalMode, "BtNoSignalMode", u8"蓝牙无信号模式", DeviceCmdParamKind::JsonMap, kHintBtRfMode, kSet},
    {DeviceCmd::BtFreqMode, "BtFreqMode", u8"蓝牙定频模式", DeviceCmdParamKind::JsonMap, kHintBtRfMode, kSet},
    {DeviceCmd::WriteKey, "WriteKey", u8"密钥", DeviceCmdParamKind::JsonMap, kHintWriteKey, kSet},
    {DeviceCmd::TrimSet, "TrimSet", u8"微调值", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MacWrite, "MacWrite", u8"蓝牙mac地址", DeviceCmdParamKind::JsonMap, kHintMacWrite, kSet},
    {DeviceCmd::NightLightSet, "NightLightSet", u8"夜灯", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LedTest, "LedTest", u8"指示灯测试", DeviceCmdParamKind::JsonMap, kHintLedTest, kSet},
    {DeviceCmd::LcdBacklight, "LcdBacklight", u8"屏幕背光", DeviceCmdParamKind::JsonMap, kHintLcdBacklight, kSet},
    {DeviceCmd::LcdColorTestMode, "LcdColorTestMode", u8"LCD颜色测试模式(进/退)", DeviceCmdParamKind::JsonMap,
     u8"TLV 0x0021：enter=1 进入，0 退出\r\n示例：Param_enter=1", kSet},
    {DeviceCmd::SetLcdColor, "SetLcdColor", u8"设置LCD颜色", DeviceCmdParamKind::JsonMap,
     u8"TLV 0x0028：color=1红 2绿 3蓝 4黑 5白 6灰度条 7灰\r\n需先进入 LcdColorTestMode\r\n示例：Param_color=1", kSet},
    {DeviceCmd::LightReportControl, "LightReportControl", u8"光感上报控制", DeviceCmdParamKind::JsonMap,
     u8"Qfctp 测试服务 TLV 0x001D：start=1 开启上报 / start=0 关闭\r\n示例：Param_start=1", kSet},
    {DeviceCmd::LightCalibWrite, "LightCalibWrite", u8"传感器校准写入", DeviceCmdParamKind::JsonMap, kHintLightCalibWrite, kSet},
    {DeviceCmd::CompensationSet, "CompensationSet", u8"补偿参数", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::NowMusicInfo, "NowMusicInfo", u8"当前音乐信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::SdCardInfo, "SdCardInfo", u8"存储卡信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::LightSensorInfo, "LightSensorInfo", u8"环境光传感器信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::ButtonState, "ButtonState", u8"按键状态/模拟按键", DeviceCmdParamKind::Int, kHintButtonState, kBoth,
     "ProtocolButtonStateData", "modeButtonState"},
    {DeviceCmd::GetPressCaliResult, "GetPressCaliResult", u8"压力校准结果", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::GetImuCaliResult, "GetImuCaliResult", u8"惯性校准结果", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::DeviceInfo, "DeviceInfo", u8"设备信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::ConnectInfo, "ConnectInfo", u8"连接信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::WifiInfo, "WifiInfo", u8"无线网络信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::GetServoMotorInfo, "GetServoMotorInfo", u8"舵机信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::BurshBacklog, "BurshBacklog", u8"刷牙积压数据", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::TrimRead, "TrimRead", u8"微调值", DeviceCmdParamKind::None, nullptr, kGet, "ProtocolTrimData", "trim"},
    {DeviceCmd::MacRead, "MacRead", u8"蓝牙mac地址", DeviceCmdParamKind::JsonMap, kHintMacRead, kGet, "ProtocolMacData",
     "mac"},
    {DeviceCmd::KeySignalRead, "KeySignalRead", u8"按键信号", DeviceCmdParamKind::None, nullptr, kGet,
     "ProtocolKeyCapData", "capacitance"},
    {DeviceCmd::LightCalibRead, "LightCalibRead", u8"光感校准读取", DeviceCmdParamKind::JsonMap, kHintLightCalibRead, kGet,
     "ProtocolLightCalibData", "calibValue"},
    {DeviceCmd::AgingStatusRead, "AgingStatusRead", u8"老化状态", DeviceCmdParamKind::JsonMap, kHintAgingStatusRead, kGet,
     "ProtocolRootAgingHistoryData", "status"},
    {DeviceCmd::FactoryDoneRead, "FactoryDoneRead", u8"产测完成标志", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::DeviceExceptionRead, "DeviceExceptionRead", u8"设备异常", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::ExceptionThresholdRead, "ExceptionThresholdRead", u8"异常阈值读取", DeviceCmdParamKind::JsonMap,
     kHintExceptionThresholdRead, kGet, "ProtocolAiotExceptionThresholdData", "value"},
    {DeviceCmd::ExceptionThresholdWrite, "ExceptionThresholdWrite", u8"异常阈值写入", DeviceCmdParamKind::JsonMap,
     kHintExceptionThresholdWrite, kSet},
    {DeviceCmd::PumpParamRead, "PumpParamRead", u8"泵参数读取", DeviceCmdParamKind::None, kHintPumpParamRead, kGet,
     "ProtocolAiotPumpParamData", "durationTime"},
    {DeviceCmd::PumpParamWrite, "PumpParamWrite", u8"泵参数写入", DeviceCmdParamKind::JsonMap, kHintPumpParamWrite,
     kSet},
    {DeviceCmd::ValveParamRead, "ValveParamRead", u8"阀参数读取", DeviceCmdParamKind::None, kHintValveParamRead, kGet,
     "ProtocolAiotPumpParamData", "valveEnableTime"},
    {DeviceCmd::ValveParamWrite, "ValveParamWrite", u8"阀参数写入", DeviceCmdParamKind::JsonMap, kHintValveParamWrite,
     kSet},
    {DeviceCmd::HeatTestWrite, "HeatTestWrite", u8"自定义加热测试", DeviceCmdParamKind::JsonMap, kHintHeatTestWrite,
     kSet, "ProtocolAiotHeatTestData", "enable"},
    {DeviceCmd::VibrationTestWrite, "VibrationTestWrite", u8"自定义振动测试", DeviceCmdParamKind::JsonMap,
     kHintVibrationTestWrite, kSet, "ProtocolAiotVibrationTestData", "enable"},
    {DeviceCmd::CycleReportWrite, "CycleReportWrite", u8"设置数据采集上报", DeviceCmdParamKind::JsonMap,
     kHintCycleReportWrite, kSet, "ProtocolAiotCycleReportConfigData,ProtocolAiotCycleReportData", "enable"},
};

} // namespace

namespace DeviceCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(DeviceCmd cmd) {
    for (const Row& row : kRows) {
        if (row.cmd == cmd)
            return &row;
    }
    return nullptr;
}

const Row* findByEnumName(const QString& enumName) {
    const QString trimmed = enumName.trimmed();
    for (const Row& row : kRows) {
        if (QString::fromLatin1(row.enumName) == trimmed)
            return &row;
    }
    return nullptr;
}

} // namespace DeviceCmdManifest
