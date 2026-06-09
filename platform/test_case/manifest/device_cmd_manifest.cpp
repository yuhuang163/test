#include "device_cmd_manifest.h"

namespace {

using DeviceCmdManifest::Row;

constexpr uint8_t kSet = DeviceCmdManifest::kSendActionSet;
constexpr uint8_t kGet = DeviceCmdManifest::kSendActionGet;
constexpr uint8_t kBoth = DeviceCmdManifest::kSendActionBoth;

// hint 单独定义，避免 MSVC 宏参数中的括号/换行问题
constexpr const char kHintForbidSleep[] =
    u8"禁止休眠：value=1 开启，0 关闭\n示例：{\"value\":1} 或 value=1";
constexpr const char kHintSn[] =
    u8"读写 SN：which_sn 类型\n"
    u8"  1=整机SN(TAIL)  6=deviceName(SUB_PID)  7=productKey(SKUID)\n"
    u8"写 productKey：which_sn=7，sn=$TUPLE_PRODUCT_KEY\n"
    u8"写 deviceName：which_sn=6，sn=$TUPLE_DEVICE_NAME\n"
    u8"写整机 SN：which_sn=1，sn=具体值或 $TAIL_SN";
constexpr const char kHintSoftVersionRead[] =
    u8"FCTP：Get SoftVersionRead 回包 soft_version（软件版本），无需参数\n"
    u8"卡控：ReportType=ProtocolBaseInfoData，Field=soft_version，Op=compareVersions，Expected=目标版本";
constexpr const char kHintBaseInfo[] =
    u8"QPB：读写基础信息（含软件/资源版本等）\nFCTP 请改用 SoftVersionRead（读取版本号）";
constexpr const char kHintFacResult[] =
    u8"产测结果：done=1 通过(留空等同1)，done=0 失败\n示例：done=1 或 {\"done\":1}";
constexpr const char kHintBurningMode[] =
    u8"老化：mode、seconds，可选 switch\n示例：{\"mode\":1,\"seconds\":3600}";
constexpr const char kHintSleep[] = u8"休眠：switch=1 进入，0 退出\n示例：{\"switch\":1}";
constexpr const char kHintFacMode[] = u8"工厂模式：value=1 进入，0 退出\n示例：{\"value\":1}";
constexpr const char kHintWifiConnect[] =
    u8"WiFi：name=SSID，password=密码\n示例：name=TestAP\npassword=12345678";
constexpr const char kHintRssiRead[] = u8"RSSI：mode=0 读 BLE，mode=1 读 BT\n示例：{\"mode\":0}";
constexpr const char kHintTupleRead[] = u8"无需参数；比对在步骤逻辑中与云端三元组比较";
constexpr const char kHintWriteKey[] =
    u8"写 deviceSecret：value=$TUPLE_DEVICE_SECRET（默认取云端已获取三元组）";

// 新增指令：在此表增加一行；协议实现见 qfctp.cpp / qpb.cpp 的 set/get。
const Row kRows[] = {
    {DeviceCmd::ForbidSleep, "ForbidSleep", u8"禁止休眠", DeviceCmdParamKind::JsonMap, kHintForbidSleep, kSet},
    {DeviceCmd::Sn, "Sn", u8"序列号", DeviceCmdParamKind::JsonMap, kHintSn, kBoth},
    {DeviceCmd::SoftVersionRead, "SoftVersionRead", u8"版本号", DeviceCmdParamKind::None, kHintSoftVersionRead, kGet},
    {DeviceCmd::BaseInfo, "BaseInfo", u8"基本信息", DeviceCmdParamKind::None, kHintBaseInfo, kGet},
    {DeviceCmd::GetBattery, "GetBattery", u8"电量", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::FacResult, "FacResult", u8"产测结果", DeviceCmdParamKind::JsonMap, kHintFacResult, kSet},
    {DeviceCmd::BurningMode, "BurningMode", u8"老化模式", DeviceCmdParamKind::JsonMap, kHintBurningMode, kSet},
    {DeviceCmd::Sleep, "Sleep", u8"休眠", DeviceCmdParamKind::JsonMap, kHintSleep, kSet},
    {DeviceCmd::ShipMode, "ShipMode", u8"关机", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::FacMode, "FacMode", u8"工厂模式", DeviceCmdParamKind::JsonMap, kHintFacMode, kSet},
    {DeviceCmd::DevReset, "DevReset", u8"设备复位", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::WifiDisconnect, "WifiDisconnect", u8"断开无线网络", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::WifiConnect, "WifiConnect", u8"连接无线网络", DeviceCmdParamKind::JsonMap, kHintWifiConnect, kSet},
    {DeviceCmd::RssiRead, "RssiRead", u8"信号强度", DeviceCmdParamKind::JsonMap, kHintRssiRead, kGet},
    {DeviceCmd::ChargeCurrentRead, "ChargeCurrentRead", u8"充电电流", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::TupleRead, "TupleRead", u8"三元组", DeviceCmdParamKind::None, kHintTupleRead, kGet},
    {DeviceCmd::PeriphState, "PeriphState", u8"外设状态", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::FactoryReset, "FactoryReset", u8"恢复出厂", DeviceCmdParamKind::None, nullptr, kSet},
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
    {DeviceCmd::SuctionMode, "SuctionMode", u8"吸力模式", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BtSignalMode, "BtSignalMode", u8"蓝牙信号模式", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BtNoSignalMode, "BtNoSignalMode", u8"蓝牙无信号模式", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::BtFreqMode, "BtFreqMode", u8"蓝牙定频模式", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::WriteKey, "WriteKey", u8"密钥", DeviceCmdParamKind::JsonMap, kHintWriteKey, kSet},
    {DeviceCmd::TrimSet, "TrimSet", u8"微调值", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::MacWrite, "MacWrite", u8"网卡地址", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::NightLightSet, "NightLightSet", u8"夜灯", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LedTest, "LedTest", u8"指示灯测试", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LcdBacklight, "LcdBacklight", u8"屏幕背光", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LightReportControl, "LightReportControl", u8"灯光上报控制", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::LightCalibWrite, "LightCalibWrite", u8"灯光校准", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::CompensationSet, "CompensationSet", u8"补偿参数", DeviceCmdParamKind::None, nullptr, kSet},
    {DeviceCmd::NowMusicInfo, "NowMusicInfo", u8"当前音乐信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::SdCardInfo, "SdCardInfo", u8"存储卡信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::LightSensorInfo, "LightSensorInfo", u8"环境光传感器信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::ButtonState, "ButtonState", u8"按键状态", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::GetPressCaliResult, "GetPressCaliResult", u8"压力校准结果", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::GetImuCaliResult, "GetImuCaliResult", u8"惯性校准结果", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::DeviceInfo, "DeviceInfo", u8"设备信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::ConnectInfo, "ConnectInfo", u8"连接信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::WifiInfo, "WifiInfo", u8"无线网络信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::GetServoMotorInfo, "GetServoMotorInfo", u8"舵机信息", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::BurshBacklog, "BurshBacklog", u8"刷牙积压数据", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::TrimRead, "TrimRead", u8"微调值", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::MacRead, "MacRead", u8"网卡地址", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::KeySignalRead, "KeySignalRead", u8"按键信号", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::LightCalibRead, "LightCalibRead", u8"灯光校准", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::AgingStatusRead, "AgingStatusRead", u8"老化状态", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::FactoryDoneRead, "FactoryDoneRead", u8"产测完成标志", DeviceCmdParamKind::None, nullptr, kGet},
    {DeviceCmd::DeviceExceptionRead, "DeviceExceptionRead", u8"设备异常", DeviceCmdParamKind::None, nullptr, kGet},
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
