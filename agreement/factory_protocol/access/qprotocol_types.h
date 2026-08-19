#ifndef QPB_TYPES_H
#define QPB_TYPES_H

#include <QByteArray>
#include <QVector>
#include <QString>
#include <QVariant>
#include "ble_protocol/fx_ble_msg.pb.h"
#include "factory_protocol/factory_msg.pb.h"

struct ImuCalData {
    int magic;
    short gyro_offset[3];
};

struct NewImuCalData {
    short gyro_offset[3];
    float kx;
    float ky;
    float kz;
    float syx;
    float szx;
    float szy;
    float bx;
    float by;
    float bz;
};

struct ImuDataT {
    short gyro[3];
    short acc[3];
};

struct ImuCalDataOld {
    int magic;
    ImuDataT imu_offset;
    float acc_scalar[3];
};

struct local_ota_data {
    int is_have_data;
    int file_size;
    int version_code; // 版本号
    int version_type; // 文件类型  201固件  202资源  303音频 304主题 305音乐文件
    QByteArray url;   //
    QByteArray md5;   //
};

typedef enum {
    MODULE_INVALID,
    MODULE_BTH,
    MODULE_MODE_BUTTON,
    MODULE_POWER_BUTTON,
    MODULE_ASSISTANT_COMPONENT,
    MODULE_MAX,
} press_module_e;

struct press_calib_data_t {
    unsigned short calib_factor[MODULE_MAX];
    short temperature[MODULE_MAX];
};

struct WifiConnectPayload {
    QByteArray name;
    QByteArray password;
};

struct DeviceSnPayload {
    FacDevInfoType which_sn;
    QByteArray sn;
    /** Qaiot device_side_id：-1 未指定；0 Left / 1 Right / 2 Independent */
    int sideId = -1;
};

enum class ProtocolSnType {
    Unknown = 0,
    BoardSn, //板子sn
    TailSn,  //整机sn
    SubPid,  //app颜色id
    SkuId,   //产品id
};

struct ProtocolSnData {
    ProtocolSnType type = ProtocolSnType::Unknown;
    QString value;
};

struct ProtocolBatteryData {
    int chargeState = 0;
    int percent = 0;       // battery_percent uint8 [0,100]
    int voltageMv = 0;     // battery_voltage uint16 mV
    int currentMa = 0;     // battery_current uint16 mA
    int temperatureC = 0;  // battery_temperature uint8 °C
};

struct ProtocolWifiStateData {
    QString wifiName;
    QString wifiPassword;
};

struct ProtocolMusicStateData {
    int musicState = 0;
};

struct ProtocolMacBytesData {
    int size = 0;
    uint8_t bytes[6] = {0};
};

struct ProtocolBaseInfoData {
    QString product_name;
    int pb_phone_ver = 0;
    int pb_factory_ver = 0;
    QString hw_version;
    QString soft_version;
    QString res_version;
    QString algo_version;
    QString presure_version;
    QString motor_version;
    QString ble_version;
    QString camera_version;
    QString fsensor_version;
    ProtocolMacBytesData ble_mac;
    ProtocolMacBytesData wifi_mac;
    int imu_id = 0;
    int result = 0;
    int ageing_state = 0;
    int ageingState = 0;
};

struct ProtocolPeriphStateData {
    int imu_state = 0;
    int flash_state = 0;
    int magnet_state = 0;
    int press_state = 0;
    int press0_state = 0;
    int press1_state = 0;
    int audio_state = 0;
    // 新增明确语义字段：用于区分“IC状态”与历史复用字段
    int battery_ic_state = -1;
    int touch_ic_state = -1;
    int led_ic_state = -1;
    int pd_ic_state = -1;
    int result = 0;
};

struct ProtocolButtonStateData {
    int modeButtonState = 0;
    int powerButtonState = 0;
    int keyButtonId = 0;
};

struct ProtocolBrushControlData {
    int brushStart = 0;
};

struct ProtocolLedControlData {
    int switchState = 0;
    int ledStateCount = 0;
};

struct ProtocolTypeData {
    int type = 0;
};

typedef ProtocolTypeData ProtocolLcdControlData;
typedef ProtocolTypeData ProtocolBatteryTempData;
typedef ProtocolTypeData ProtocolFlangeData;
typedef ProtocolTypeData ProtocolHeatTempData;

struct ProtocolPressSampleData {
    int timeStamp = 0;
    QVector<int> adcValues;
    QVector<int> valueValues;
};

struct ProtocolImuSampleData {
    int timeStamp = 0;
    QVector<int> accelValues;
    QVector<int> gyroValues;
};

struct ProtocolImuCalibResultData {
    int result = 0;
    int gyroX = 0;
    int gyroY = 0;
    int gyroZ = 0;
    int bx = 0;
    int by = 0;
    int bz = 0;
    int kx = 0;
    int ky = 0;
    int kz = 0;
    int syx = 0;
    int szx = 0;
    int szy = 0;
};

/** Qaiot IMU 校准（CID=0x08/0x09，type=0x00）：imu_cali_data_t = 9×float LE，共 36 字节 */
struct ProtocolAiotImuCaliData {
    float kx = 0.f;
    float ky = 0.f;
    float kz = 0.f;
    float syx = 0.f;
    float szx = 0.f;
    float szy = 0.f;
    float bx = 0.f;
    float by = 0.f;
    float bz = 0.f;
};

/** Qaiot 电容/力传感校准标志（type=0x04 fsensor）：1 字节，0 未校准 / 1 已校准 */
struct ProtocolAiotFsensorCaliData {
    int calibrated = 0;
};

/** Qaiot 异常阈值单项（CID=0x0A/0x0B） */
struct ProtocolAiotExceptionThresholdItem {
    int type = 0;
    int value = 0;     // 主值：% / mV / 秒 / mA / 温度下限 / 负压
    int valueHigh = 0; // 仅 bat_temp_abnormal 上限
    QByteArray raw;
};

struct ProtocolAiotExceptionThresholdData {
    QVector<ProtocolAiotExceptionThresholdItem> items;
};

/** Qaiot 泵/阀运行参数（CID=0x0F 设置 / 0x10 获取；泵与阀分命令控制，共用回包结构） */
struct ProtocolAiotPumpParamData {
    int circleNum = 0;        // 泵侧：SET 目标循环 / GET 已测循环
    int durationTime = 0;     // 泵工作时长
    int intervalTime = 0;     // 泵间隔
    int valveEnableTime = 0;  // 阀使能时长
    int valveDisableTime = 0; // 阀关闭时长
    int pumpPwm = 0;          // 泵 PWM %
    int valvePwm = 0;         // 阀 PWM %
};

/** Qaiot 自定义加热测试（CID=0x14） */
struct ProtocolAiotHeatTestData {
    int enable = 0;       // 0 disable / 1 enable
    int driveStrength = 0;
    int durationTime = 0; // 可选；未带回则为 0
    bool hasDuration = false;
};

/** Qaiot 自定义振动测试（CID=0x15） */
struct ProtocolAiotVibrationTestData {
    int enable = 0;
    int driveStrength = 0;
    int freq = 0;
    int durationTime = 0;
};

/** Qaiot 循环上报配置单项（CID=0x18） */
struct ProtocolAiotCycleReportConfigItem {
    int dataType = 0;     // report_data_type 0x00~0x0D
    int intervalTime = 0; // ms
};

/** Qaiot 设置数据采集上报回包（CID=0x18） */
struct ProtocolAiotCycleReportConfigData {
    int enable = 0; // 0 Disabled / 1 Enabled
    QVector<ProtocolAiotCycleReportConfigItem> items;
};

/** Qaiot 循环上报采样单项（CID=0x19；按 dataType 填对应字段） */
struct ProtocolAiotCycleReportItem {
    int dataType = 0;
    QByteArray raw;
    // IMU 0x00：6×int16 BE
    int accX = 0;
    int accY = 0;
    int accZ = 0;
    int gyroX = 0;
    int gyroY = 0;
    int gyroZ = 0;
    // 压力 0x01：int16 p_out/p_in（0.1 Pa）
    int pressureOut = 0;
    int pressureIn = 0;
    // 气流 0x02：int32（0.01 L/min）
    int flowRate = 0;
    // TOF 0x03 / 接近 0x0A
    int distanceMm = 0;
    // 电容 0x04
    int adcRaw = 0;
    // 红外 0x05
    int irLevel = 0;
    // 生物阻抗 0x06：uint32（0.1 Ω）
    int impedance = 0;
    // 液位 0x07
    int levelMm = 0;
    // 温度 0x08 / 湿度 0x09 / 霍尔 0x0C
    int temperatureC = 0;
    int humidity = 0;
    int hallState = 0;
    // 电流 0x0B
    int currentMa = 0;
    // 编码器 0x0D
    int pulseCount = 0;
};

/** Qaiot 数据采集被动上报（CID=0x19） */
struct ProtocolAiotCycleReportData {
    QVector<ProtocolAiotCycleReportItem> items;
};

struct ProtocolPressCalibResultData {
    int brushHeadAdc = 0;
    int modeButtonAdc = 0;
    int powerButtonAdc = 0;
    int assistantComponent = 0;
    int temperature = 0;
};

struct ProtocolResultData {
    int result = 0;
};

typedef ProtocolResultData ProtocolInternetOtaData;
typedef ProtocolResultData ProtocolWifiDemandData;
typedef ProtocolTypeData ProtocolCameraControlData;

struct ProtocolServoMotorInfoData {
    int uploadType = 0;
    int whichValue = 0;
    int motorCaliMark = 0;
    int motorCurrent = 0;
    int motorState = 0;
    int motorVoltage = 0;
    int faultCode = 0;
    QString hallInfo;
    QString zeroInfo;
};

typedef ProtocolResultData ProtocolPictureSendOverData;

struct ProtocolPhotosensitiveData {
    int lightSensor = 0;
};

struct ProtocolSdInfoData {
    int cmd = 0;
    QString data;
};

struct ProtocolTupleData {
    QString productId;
    QString deviceId;
    QString key;           // 明文密钥或（qroot 解密后）密钥后 8 位明文
    QString keyCipherHex;  // qroot 0xF7 线上 KeyTail 密文 hex；非 qroot 为空
    bool keyDecrypted = false;
};

struct ProtocolAgingStatusData {
    int status = 0;
    int loops = 0;
    uint32_t seconds = 0;
};

struct ProtocolDeviceExceptionData {
    int status = 0;
    QString statusText;
};

struct ProtocolKeyCapData {
    uint32_t capacitance = 0;
    int keyId = -1; // 按键编号 KK，无则 -1
};

struct ProtocolChargeCurrentData {
    uint32_t currentMa = 0;
};

/** qroot 0x82 泵堵电流：堵转 ADC（last_adc_value，大端）。 */
struct ProtocolPumpStallCurrentData {
    int adcValue = 0;
};

/**
 * 老化历史/老化模式信息：
 * - Qroot 0x9C Ack 16B：次数 + 双温 + 堵转次数(1) + 阈值(2) + 电流×5
 * - Qaiot CID=0x01 老化模式 factory_mode_info(0x23) 18B：
 *   使能 + 完成标志 + 双温 + 堵转次数(2 BE) + 阈值(2) + 电流×5
 */
struct ProtocolRootAgingHistoryData {
    int status = -1;                // Qaiot 使能 0/1；Qroot 无此字段为 -1
    int finishedFlag = -1;          // Qaiot 完成标志；Qroot 为 -1
    int agingCount = 0;             // Qroot 老化当前次数 uint8
    int batteryMaxTempC = 0;        // 老化电池历史最高温 ℃
    int flangeMaxTempC = 0;         // 老化法兰历史最高温 ℃
    int stallCount = 0;             // 堵转次数（Qroot 1B / Qaiot 2B BE）
    int stallThreshold = 0;         // 泵阀堵转阈值 uint16 BE
    int stallCurrents[5] = {0, 0, 0, 0, 0}; // 最新 5 个堵转电流 mA，各 uint16 BE
};

struct ProtocolTrimData {
    uint32_t trim = 0;
};

struct ProtocolLightCalibData {
    uint32_t calibValue = 0;
};

/** Dongle AT 上行 */
struct ProtocolDongleBleStateData {
    int connected = 0;
};

struct ProtocolDongleBleRssiData {
    QString rssi;
};

struct ProtocolDongleWifiMsgData {
    QString text;
};

struct ProtocolDongleVersionData {
    QString version;
};

struct ProtocolDongleDeviceNameData {
    QString name;
};

struct ProtocolDongleWifiSsidData {
    QString ssid;
};

struct ProtocolDongleWifiRssiData {
    QString rssi;
};

struct ProtocolDongleWifiStateData {
    int connected = 0;
};

struct ProtocolDongleWifiIpData {
    QString ip;
};

struct ProtocolDongleScanResultData {
    QString deviceName;
    QString deviceAddress;
    QString deviceRssi;
};

/** Dongle AT+SUCTION=1 后上报：AT+SUCTION_DATA=CH1,CH2,CH3（kPa）或 PHY 通道 4 二进制帧 */
struct ProtocolDongleSuctionData {
    double ch1Kpa = 0.0;
    double ch2Kpa = 0.0;
    double ch3Kpa = 0.0;
    /** PHY 二进制 payload 前 4B 小端 ms；文本 AT 无此时为 -1 */
    qint32 dongleTimestampMs = -1;
};

/** Dongle 吸力采样窗口汇总（SampleSuctionDual/Single），供 Gate 卡控 */
struct ProtocolDongleSuctionPeakData {
    double peakKpa = 0.0;     // 单通道：各完整周期峰值中最强（数值最小）
    double highKpa = 0.0;     // 单通道：各完整周期峰值中最弱（数值最大）
    double peakDiffKpa = 0.0; // 单通道：峰值差=各周期峰值中最大-最小
    double ch1PeakKpa = 0.0;  // 双通道：CH1 各完整周期峰值中最强（最低 kPa）
    double ch2PeakKpa = 0.0;  // 双通道：CH2 各完整周期峰值中最强
    double sideDiffKpa = 0.0; // 双通道：|CH1峰值-CH2峰值|
    int peakCount = 0;        // 完整周期峰个数（双通道取两口较少者）
};

/** 自由工站屏幕检测（USB 摄像头采集，主机侧分析，不发产品协议） */
struct ProtocolScreenInspectData {
    int deadPixels = 0;
    double muraStd = 0.0;
    double ssim = -1.0; // 无参考图为 -1
};

/** USB 电流表 / 治具振幅仪上行 */
struct ProtocolAmmeterReadingData {
    QString value;
};

struct ProtocolJigAmplitudeData {
    QString value;
};

struct ProtocolFactoryDoneData {
    bool done = false;
};

struct ProtocolRssiData {
    int dbm = 0;
};

/** 杰理蓝牙盒子 TLV 上报（T=7 频偏、T=8 RSSI，小端 int32） */
struct ProtocolJieliBtBoxData {
    qint32 freqOffset = 0;
    qint32 rssi = 0;
    QString mac;
};

struct ProtocolMacData {
    QString mac;
};

struct ProtocolAckData {
    int ack = 0;
};

struct ProtocolMeasureData {
    QString deviceName; // 设备名称，如 "HuilingWfp60h"
    QString channel;    // 通道号（如有，如 "CH1"）
    QString type;       // 测量类型：Current(电流), Voltage(电压), Power(功率), Temp(温度)
    double value = 0.0; // 测量数值
    QString valueText;  // 测量文本结果（用于非数值匹配，如 CMW IDN、错误文本）
    QString unit;       // 单位，如 "mA", "V", "W"
    bool isOk = true;   // 状态是否有效
};

struct NewWifiConnectPayload {
    QByteArray name;
    QByteArray password;
    QString ip;
    QString port;
};

struct SevorMotorParamPayload {
    uint32_t sweeping_angle;
    float vibrate_angle;
    float sweeping_freq;
    uint32_t vibrate_freq;
};

struct LocalOtaPayload {
    local_ota_data file0;
    local_ota_data file1;
};

struct StartMultiBleOtaPayload {
    RotasFileStatusReq req0;
    RotasFileStatusReq req1;
};

enum class DeviceCmd {
    // set commands
    PressSensorTemp,       // 【预留】枚举保留，当前 Qpb/Qfctp 均未在 switch 中实现
    UartReceive,           // 【上层常用】Qpb 未单独 case 时走 default；Qfctp 未实现（发告警）
    RgbColor,              // 【Qpb】RGB 灯效（FacLedControl，set_rgb_color）
    MotorCali,             // 【Qpb】电机校准流程控制（int，set_motor_cali）
    MotorDampingState,     // 【Qpb】电机阻尼状态（int，set_motor_damping_state）
    MotorTestState,        // 【Qpb】电机测试状态（int，set_motor_test_state）
    MotorCaliState,        // 【Qpb】电机校准状态（int，set_motor_cali_state）
    FacResult,             // 【统一入口】产测结果/完成标识；Qpb: set_fac_result(int)，Qfctp: FactoryDoneWrite(done)
    ScreenColor,           // 【Qpb】屏幕纯色/显示测试（int，set_screen_color）
    LedColor,              // 【Qpb】LED 颜色两参数（QVariantList{区/路, 色值}，set_led_color）
    ShipMode,              // 【Qpb】船运模式（int，set_ship_mode）；【Qfctp/Qroot】关机（无参）
    MotorAdcSwitch,        // 【Qpb】电机 ADC 采样开关（int，set_motor_adc_switch）
    MotorParam,            // 【Qpb】电机运行参数（QVariantList{频率 uint, 占空比 float}，set_motor_param）
    MotorState,            // 【Qpb】电机工作状态（int，set_motor_state）
    MotorCaliResultParam,  // 【Qpb】电机校准结果参数（uint，set_motor_cali_result_param）
    WifiConnect,           // 【Qpb】连 WiFi（WifiConnectPayload / QVariantMap{name,password} / WifiInfo，set_connect_wifi）
    Music,                 // 【Qpb】音乐控制/曲目数据（QByteArray，set_music）
    BurningMode,           // 【主入口】老化模式统一入口（推荐 QVariantMap{mode,seconds,switch?/enter?}）；Qpb/Qfctp/Qroot(0xAF) 均兼容
    BrushRecord,           // 【Qpb】使用记录写入（FacSetBrushRecord，set_brush_record）
    BrushTime,             // 【Qpb】使用计时相关（int，set_brush_time）
    Sleep,                 // 【主入口】休眠/待机控制；Qpb 走 set_sleeep，Qfctp 映射待机 TLV
    ForbidSleep,           // 【Qpb】禁止休眠开关（FacSwitch，set_forbid_sleep）
    CameraState,           // 【Qpb】摄像头总开关/状态（int，set_camera_state）
    ScreenCameraState,     // 【Qpb】屏侧相机状态（int，set_screen_camera_state）
    CameraLightState,      // 【Qpb】相机补光状态（int，set_camera_light_state）
    CameraSupportState,    // 【Qpb】相机能力/支持项状态（int，set_camera_support_state）
    CameraExposureTime,    // 【Qpb】曝光时间（uint，set_camera_exposure_time）
    DevReset,              // 【Qpb】设备复位；【Qroot】0xFC+0x02；【Qaiot】CID=0x0C type=0x04 重启
    BrushReset,            // 【Qpb】使用相关复位（无参，set_brush_reset）
    PressCaliResult,       // 【Qpb】压力标定结果下发（press_calib_data_t，set_press_cali_result）；get 见 GetPressCaliResult
    ImuCaliResult,         // 【Qpb】IMU 标定结果下发（ImuCalData，set_imu_cali_result）；get 见 GetImuCaliResult
    NewImuCaliResult,      // 【Qpb】新 IMU 校准结果下发（NewImuCalData，set_new_imu_cali_result）
    DeviceMode,            // 【Qpb】设备运行模式（int，set_device_mode）
    BrushControl,          // 【Qpb】电机/动作控制（int，set_brush_control）
    FacMode,               // 【Qpb】工厂模式开关（int，set_fac_mode）；【Qfctp】非 0 进入产测模式（sendFactoryTestMode）
    Sn,                    // 【主入口】SN/三元组写统一入口；Qpb 兼容 set_sn，Qfctp 内部映射 TLV 写入
    SoftVersionRead,       // 【Qfctp】读软件版本（getCaseFwVersionRead）；产测 test_case「读取版本号」
    BaseInfo,              // 【Qpb】基础信息读写（get/set）；FCTP 请用 SoftVersionRead
    CameraPictureState,    // 【Qpb】相机拍照/成像状态（int，set_camera_picture_state）
    LocalOta,              // 【Qpb】本地 OTA（载荷 LocalOtaPayload）
    StartOtaApp,           // 【Qpb】通过 App 通道启动 OTA（RotasFileStatusReq，走 set_start_ota_app）
    IAmApp,                // 【Qpb】向设备声明「当前连接端是 App」（无参，set_i_am_app）
    ConfigNetworkApp,      // 【Qpb】配网：把 WiFi 信息下发给设备上的配网 App（WifiInfo，set_config_network_app）
    WifiDisconnect,        // 【Qpb】通知设备断开当前 WiFi（无参，set_wifi_disconnect）
    StartMultiBleOtaApp,   // 【Qpb】双路 BLE OTA：同时下发两路 RotasFileStatusReq（StartMultiBleOtaPayload 或 QVariantList）
    PressCollect,          // 【Qpb】压力传感器产测采集开关（FacSwitch，set_press_collect_param）
    ImuCollect,            // 【Qpb】IMU 产测采集开关（FacSwitch，set_imu_collect_param）
    CameraFaultDataPacket, // 【预留】当前 Qpb/Qfctp 均未在 switch 中实现
    ServoMotorInfo,        // 【Qpb】舵机信息查询触发/下发（无参 set，set_servo_motor_info）
    MicControl,            // 【Qpb】麦克风开关或增益类控制（int，set_mic_control）
    UploadRecordData,      // 【Qpb】上传记录类数据控制（int，set_upload_record_data）
    NewWifiConnect,        // 【Qpb】新协议连 WiFi（NewWifiConnectPayload 或 QVariantList{name,pwd,ip,port}，set_new_connect_wifi）
    SevorMotorParam,       // 【Qpb】扫振电机参数（SevorMotorParamPayload 或 QVariantList 4 元组，set_sevor_motor_param）
    SuctionMode,           // 【兼容别名】Qfctp 吸力测试入口（保留兼容）
    BtSignalMode,          // 【兼容别名】Qfctp 蓝牙信令入口（保留兼容）
    BtNoSignalMode,        // 【兼容别名】Qfctp 蓝牙非信令入口（保留兼容）
    BtFreqMode,            // 【兼容别名】Qfctp 蓝牙校频入口（保留兼容）
    WriteKey,              // 【兼容别名】Qfctp 写密钥入口（建议优先走 Sn 主入口）
    TrimSet,               // 【Qfctp】写 trim（QVariantMap，setCaseTrimSet）
    MacWrite,              // 【Qfctp】写 MAC（QVariantMap，setCaseMacWrite）
    NightLightSet,         // 【Qfctp】夜灯亮度（QVariantMap，setCaseNightLightSet）
    LedTest,               // 【主入口】LED 测试开关（QVariantMap{on:0|1}）；Qfctp/Qroot/Qpb 均兼容
    FactoryReset,          // 【Qfctp】恢复出厂；【Qroot】0xFC+0x04；【Qaiot】CID=0x0C type=0x01
    TravelLock,            // 【Qaiot】CID=0x0C type=0x03 旅行锁

    LcdBacklight,       // 【Qfctp】LCD 背光（QVariantMap，setCaseLcdBacklight）
    LightReportControl, // 【Qfctp】光感上报开关（QVariantMap，setCaseLightReportControl）
    LightCalibWrite,    // 【Qfctp】光感校准写（QVariantMap，setCaseLightCalibWrite）
    ChargeCurrentSet,   // 【Qfctp】设置充电电流（QVariantMap{currentMa|value}，uint16 mA，setCaseChargeCurrentSet）
    CompensationSet,    // 【Qfctp】吸力补偿开关（QVariantMap，setCaseCompensationSet）
    LcdColorTestMode,   // 【Qfctp】LCD 颜色测试模式进/退（TLV 0x0021，enter=1 进入，0 退出）
    SetLcdColor,        // 【Qfctp】设置 LCD 颜色（TLV 0x0028，需先进入 LcdColorTestMode）

    // 【Qroot】吸奶器 PCBA 串口协议
    RootBatteryTempQuery, // 0x80 电池温度查询
    RootVibration,        // 0x94 振子控制
    RootFlangeQuery,      // 0x96 法兰状态查询
    RootNtcQuery,         // 0x97 加热 NTC 查询
    RootHeatTempQuery,    // 0x98 加热温度查询
    RootVibStatusQuery,   // 0x99 振子状态查询
    RootPumpTestEnter,    // 0x9D 主机泵测试进入
    RootPumpTestExit,     // 0x9E 主机泵测试退出
    RootSuctionTest,           // 0x81 吸力测试（REQ 3B：switch+mode+level）
    RootPumpStallCurrentQuery, // 0x82 读取泵堵电流（Req 无参；Ack 2B 堵转 ADC 大端）
    RootAgingHistoryQuery,     // 0x9C 读取老化历史信息（Req 无参；Ack 16B）
    RootHeatLevelControl,      // 0x83 加热档位控制（REQ 2B：switch+level L1~L3）
    RootPumpControl,           // 0xC0 吸奶器控制（REQ 10B，ACK 回显 controlType）
    RootEnterOta,         // 【Qroot】Req 0xFC+0x03 进入 OTA
    RootSystemControl,    // 【已拆分/兼容】旧「系统控制」聚合命令；新步骤请用 ShipMode/DevReset/RootEnterOta/FactoryReset

    // get commands
    NowMusicInfo,       // 【Qpb】当前播放音乐信息（无参/可空 param，get_now_music_info）
    SdCardInfo,         // 【Qpb】SD 卡信息（无参，get_sd_card_info）
    LightSensorInfo,    // 【主入口】传感类读取入口；Qpb 读光感，Qfctp 兼容映射充电电流
    GetBattery,         // 【Qpb】读电量（无参，get_battery）；【Qfctp】电量 TLV；【Qroot】Notify 0xE0 查询
    SetBattery,         // 【Qpb】电池类型；【Qaiot】CID=0x13 模拟电量(百分比/电压/电流/温度，0=真实)
    ButtonState,        // 【主入口】按键状态/上报开关；Qpb 读按键，Qroot 9A 开关（param 0|1）
    GetPressCaliResult, // 【Qpb】读压力标定结果（无参，get_press_cali_result）
    GetImuCaliResult,   // 【Qpb】读 IMU 标定结果（无参，get_imu_cali_result）
    DeviceInfo,         // 【主入口】设备信息查询（Qpb）

    TupleRead, // 【主入口】Qfctp 兼容映射三元组读取

    PeriphState,         // 【主入口】外设状态读取；Qfctp 映射 sensor 状态 TLV
    ConnectInfo,         // 【Qpb】连接信息（无参，get_connect_info）
    WifiInfo,            // 【Qpb】WiFi 信息（无参，get_wifi_info）
    GetServoMotorInfo,   // 【Qpb】读舵机信息（无参，get_servo_motor_info）
    BurshBacklog,        // 【Qpb】使用积压/日志类（param.toInt()，get_bursh_backlog）
    TrimRead,            // 【Qfctp】读 trim（无参，getCaseTrimRead）
    MacRead,             // 【Qfctp】读 MAC（无参，getCaseMacRead）
    RssiRead,            // 【Qfctp】读 RSSI（param 为 QVariantMap，含 mode，getCaseRssiRead）
    KeySignalRead,       // 【Qfctp】读按键电容（param 为 QVariantMap，含 key，getCaseKeySignalRead）
    LightCalibRead,      // 【Qfctp】读光感校准（param 为 QVariantMap，含 index，getCaseLightCalibRead）
    ChargeCurrentRead,   // 【Qfctp】读充电电流 TLV 0x0020（无参，应答 uint32 mA，getCaseChargeCurrentRead）
    AgingStatusRead,     // 【兼容别名】Qfctp 老化状态查询（保留兼容）
    FactoryDoneRead,     // 【兼容别名】Qfctp 产测标识读取（保留兼容）
    DeviceExceptionRead, // 【Qfctp】读设备异常状态（独立入口）
    ExceptionThresholdRead,  // 【Qaiot】CID=0x0A 获取异常阈值
    ExceptionThresholdWrite, // 【Qaiot】CID=0x0B 设置异常阈值（掉电不消失）
    PumpParamRead,           // 【Qaiot】CID=0x10 读泵运行参数（循环/时长/间隔/泵PWM）
    PumpParamWrite,          // 【Qaiot】CID=0x0F 写泵运行参数
    ValveParamRead,          // 【Qaiot】CID=0x10 读阀运行参数（使能/关闭/阀PWM）
    ValveParamWrite,         // 【Qaiot】CID=0x0F 写阀运行参数
    HeatTestWrite,           // 【Qaiot】CID=0x14 自定义加热测试
    VibrationTestWrite,      // 【Qaiot】CID=0x15 自定义振动测试
    CycleReportWrite,        // 【Qaiot】CID=0x18 设置数据采集上报
};

class IDevice {
  public:
    virtual ~IDevice() = default;

    virtual void set(DeviceCmd cmd, const QVariant& data = {}) = 0;
    virtual void get(DeviceCmd cmd, const QVariant& param = {}) = 0;
};

/** 协议上行统一信封：reportType 与 GateRegistry::kTypes 一致，payload 为具体结构或调试文本 */
struct ProtocolReport {
    QString reportType;
    QVariant payload;

    ProtocolReport() = default;
    ProtocolReport(const QString& type, const QVariant& data = {}) : reportType(type), payload(data) {
    }
};
Q_DECLARE_METATYPE(ProtocolReport)

// payload 内 QVariant 装箱所需（信封层仅 ProtocolReport，各 Protocol*Data 仍须单独声明）
Q_DECLARE_METATYPE(ProtocolSnData)
Q_DECLARE_METATYPE(ProtocolBatteryData)
Q_DECLARE_METATYPE(ProtocolWifiStateData)
Q_DECLARE_METATYPE(ProtocolMusicStateData)
Q_DECLARE_METATYPE(ProtocolBaseInfoData)
Q_DECLARE_METATYPE(ProtocolPeriphStateData)
Q_DECLARE_METATYPE(ProtocolButtonStateData)
Q_DECLARE_METATYPE(ProtocolBrushControlData)
Q_DECLARE_METATYPE(ProtocolLedControlData)
Q_DECLARE_METATYPE(ProtocolTypeData)
Q_DECLARE_METATYPE(ProtocolPressSampleData)
Q_DECLARE_METATYPE(ProtocolImuSampleData)
Q_DECLARE_METATYPE(ProtocolImuCalibResultData)
Q_DECLARE_METATYPE(ProtocolAiotImuCaliData)
Q_DECLARE_METATYPE(ProtocolAiotFsensorCaliData)
Q_DECLARE_METATYPE(ProtocolAiotExceptionThresholdData)
Q_DECLARE_METATYPE(ProtocolAiotPumpParamData)
Q_DECLARE_METATYPE(ProtocolAiotHeatTestData)
Q_DECLARE_METATYPE(ProtocolAiotVibrationTestData)
Q_DECLARE_METATYPE(ProtocolAiotCycleReportConfigData)
Q_DECLARE_METATYPE(ProtocolAiotCycleReportData)
Q_DECLARE_METATYPE(ProtocolPressCalibResultData)
Q_DECLARE_METATYPE(ProtocolResultData)
Q_DECLARE_METATYPE(ProtocolServoMotorInfoData)
Q_DECLARE_METATYPE(ProtocolPhotosensitiveData)
Q_DECLARE_METATYPE(ProtocolSdInfoData)
Q_DECLARE_METATYPE(ProtocolTupleData)
Q_DECLARE_METATYPE(ProtocolAgingStatusData)
Q_DECLARE_METATYPE(ProtocolDeviceExceptionData)
Q_DECLARE_METATYPE(ProtocolKeyCapData)
Q_DECLARE_METATYPE(ProtocolChargeCurrentData)
Q_DECLARE_METATYPE(ProtocolPumpStallCurrentData)
Q_DECLARE_METATYPE(ProtocolRootAgingHistoryData)
Q_DECLARE_METATYPE(ProtocolTrimData)
Q_DECLARE_METATYPE(ProtocolLightCalibData)
Q_DECLARE_METATYPE(ProtocolDongleBleStateData)
Q_DECLARE_METATYPE(ProtocolDongleBleRssiData)
Q_DECLARE_METATYPE(ProtocolDongleWifiMsgData)
Q_DECLARE_METATYPE(ProtocolDongleVersionData)
Q_DECLARE_METATYPE(ProtocolDongleWifiSsidData)
Q_DECLARE_METATYPE(ProtocolDongleWifiRssiData)
Q_DECLARE_METATYPE(ProtocolDongleWifiStateData)
Q_DECLARE_METATYPE(ProtocolDongleWifiIpData)
Q_DECLARE_METATYPE(ProtocolDongleScanResultData)
Q_DECLARE_METATYPE(ProtocolDongleSuctionData)
Q_DECLARE_METATYPE(ProtocolDongleSuctionPeakData)
Q_DECLARE_METATYPE(ProtocolScreenInspectData)
Q_DECLARE_METATYPE(ProtocolAmmeterReadingData)
Q_DECLARE_METATYPE(ProtocolJigAmplitudeData)
Q_DECLARE_METATYPE(ProtocolFactoryDoneData)
Q_DECLARE_METATYPE(ProtocolRssiData)
Q_DECLARE_METATYPE(ProtocolJieliBtBoxData)
Q_DECLARE_METATYPE(ProtocolMacData)
Q_DECLARE_METATYPE(ProtocolAckData)
Q_DECLARE_METATYPE(ProtocolMeasureData)
Q_DECLARE_METATYPE(ProtocolDongleDeviceNameData)

#endif // QPB_TYPES_H
