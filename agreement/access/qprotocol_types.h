#ifndef QPB_TYPES_H
#define QPB_TYPES_H

#include <QByteArray>
#include <QVector>
#include <QString>
#include <QVariant>
#include "ble_protocol/fx_ble_msg.pb.h"
#include "factory_protocol/factory_msg.pb.h"

typedef struct {
    int magic;
    short gyro_offset[3];
} ImuCalData;

typedef struct {
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
} NewImuCalData;

typedef struct {
    short gyro[3];
    short acc[3];
} ImuDataT;

typedef struct {
    int magic;
    ImuDataT imu_offset;
    float acc_scalar[3];
} ImuCalDataOld;

typedef struct {
    int is_have_data;
    int file_size;
    int version_code; // 版本号
    int version_type; // 文件类型  201固件  202资源  303音频 304主题 305音乐文件
    QByteArray url;   //
    QByteArray md5;   //
} local_ota_data;

typedef enum {
    MODULE_INVALID,
    MODULE_BTH,
    MODULE_MODE_BUTTON,
    MODULE_POWER_BUTTON,
    MODULE_ASSISTANT_COMPONENT,
    MODULE_MAX,
} press_module_e;

typedef struct {
    unsigned short calib_factor[MODULE_MAX];
    short temperature[MODULE_MAX];
} press_calib_data_t;

typedef struct {
    QByteArray name;
    QByteArray password;
} WifiConnectPayload;

typedef struct {
    FacDevInfoType which_sn;
    QByteArray sn;
} DeviceSnPayload;

enum class ProtocolSnType {
    Unknown = 0,
    BoardSn, //板子sn
    TailSn,  //整机sn
    SubPid,  //app颜色id
    SkuId,   //产品id
};

typedef struct {
    ProtocolSnType type = ProtocolSnType::Unknown;
    QString value;
} ProtocolSnData;

typedef struct {
    int chargeState = 0;
    int percent = 0;
    int voltageMv = 0;
} ProtocolBatteryData;

typedef struct {
    QString wifiName;
    QString wifiPassword;
} ProtocolWifiStateData;

typedef struct {
    int musicState = 0;
} ProtocolMusicStateData;

typedef struct {
    int size = 0;
    uint8_t bytes[6] = {0};
} ProtocolMacBytesData;

typedef struct {
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
} ProtocolBaseInfoData;

typedef struct {
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
} ProtocolPeriphStateData;

typedef struct {
    int modeButtonState = 0;
    int powerButtonState = 0;
    int keyButtonId = 0;
} ProtocolButtonStateData;

typedef struct {
    int brushStart = 0;
} ProtocolBrushControlData;

typedef struct {
    int switchState = 0;
    int ledStateCount = 0;
} ProtocolLedControlData;

typedef struct {
    int type = 0;
} ProtocolTypeData;

typedef ProtocolTypeData ProtocolLcdControlData;

typedef struct {
    int timeStamp = 0;
    QVector<int> adcValues;
    QVector<int> valueValues;
} ProtocolPressSampleData;

typedef struct {
    int timeStamp = 0;
    QVector<int> accelValues;
    QVector<int> gyroValues;
} ProtocolImuSampleData;

typedef struct {
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
} ProtocolImuCalibResultData;

typedef struct {
    int brushHeadAdc = 0;
    int modeButtonAdc = 0;
    int powerButtonAdc = 0;
    int assistantComponent = 0;
    int temperature = 0;
} ProtocolPressCalibResultData;

typedef struct {
    int result = 0;
} ProtocolResultData;

typedef ProtocolResultData ProtocolInternetOtaData;
typedef ProtocolResultData ProtocolWifiDemandData;
typedef ProtocolTypeData ProtocolCameraControlData;

typedef struct {
    int uploadType = 0;
    int whichValue = 0;
    int motorCaliMark = 0;
    int motorCurrent = 0;
    int motorState = 0;
    int motorVoltage = 0;
    int faultCode = 0;
    QString hallInfo;
    QString zeroInfo;
} ProtocolServoMotorInfoData;

typedef ProtocolResultData ProtocolPictureSendOverData;

typedef struct {
    int lightSensor = 0;
} ProtocolPhotosensitiveData;

typedef struct {
    int cmd = 0;
    QString data;
} ProtocolSdInfoData;

typedef struct {
    QString productId;
    QString deviceId;
    QString key;
} ProtocolTupleData;

typedef struct {
    int status = 0;
    int loops = 0;
    uint32_t seconds = 0;
} ProtocolAgingStatusData;

typedef struct {
    int status = 0;
    QString statusText;
} ProtocolDeviceExceptionData;

typedef struct {
    uint32_t capacitance = 0;
    int keyId = -1; // 按键编号 KK，无则 -1
} ProtocolKeyCapData;

typedef struct {
    uint32_t currentMa = 0;
} ProtocolChargeCurrentData;

typedef struct {
    uint32_t trim = 0;
} ProtocolTrimData;

typedef struct {
    uint32_t calibValue = 0;
} ProtocolLightCalibData;

/** Dongle AT 上行 */
typedef struct {
    int connected = 0;
} ProtocolDongleBleStateData;

typedef struct {
    QString rssi;
} ProtocolDongleBleRssiData;

typedef struct {
    QString text;
} ProtocolDongleWifiMsgData;

typedef struct {
    QString version;
} ProtocolDongleVersionData;

typedef struct {
    QString ssid;
} ProtocolDongleWifiSsidData;

typedef struct {
    QString rssi;
} ProtocolDongleWifiRssiData;

typedef struct {
    int connected = 0;
} ProtocolDongleWifiStateData;

typedef struct {
    QString ip;
} ProtocolDongleWifiIpData;

/** USB 电流表 / 治具振幅仪上行 */
typedef struct {
    QString value;
} ProtocolAmmeterReadingData;

typedef struct {
    QString value;
} ProtocolJigAmplitudeData;

typedef struct {
    bool done = false;
} ProtocolFactoryDoneData;

typedef struct {
    int dbm = 0;
} ProtocolRssiData;

typedef struct {
    QString mac;
} ProtocolMacData;

typedef struct {
    int ack = 0;
} ProtocolAckData;

typedef struct {
    QByteArray name;
    QByteArray password;
    QString ip;
    QString port;
} NewWifiConnectPayload;

typedef struct {
    uint32_t sweeping_angle;
    float vibrate_angle;
    float sweeping_freq;
    uint32_t vibrate_freq;
} SevorMotorParamPayload;

typedef struct {
    local_ota_data file0;
    local_ota_data file1;
} LocalOtaPayload;

typedef struct {
    RotasFileStatusReq req0;
    RotasFileStatusReq req1;
} StartMultiBleOtaPayload;

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
    ShipMode,              // 【Qpb】船运模式（int，set_ship_mode）；【Qfctp】复用枚举名→关机 TLV（与 Qpb 语义不同）
    MotorAdcSwitch,        // 【Qpb】电机 ADC 采样开关（int，set_motor_adc_switch）
    MotorParam,            // 【Qpb】电机运行参数（QVariantList{频率 uint, 占空比 float}，set_motor_param）
    MotorState,            // 【Qpb】电机工作状态（int，set_motor_state）
    MotorCaliResultParam,  // 【Qpb】电机校准结果参数（uint，set_motor_cali_result_param）
    WifiConnect,           // 【Qpb】连 WiFi（WifiConnectPayload / QVariantMap{name,password} / WifiInfo，set_connect_wifi）
    Music,                 // 【Qpb】音乐控制/曲目数据（QByteArray，set_music）
    BurningMode,           // 【主入口】老化模式统一入口（推荐 QVariantMap{mode,seconds,switch?/enter?}）；Qpb/Qfctp 均兼容
    BrushRecord,           // 【Qpb】使用记录写入（FacSetBrushRecord，set_brush_record）
    BrushTime,             // 【Qpb】使用计时相关（int，set_brush_time）
    Sleep,                 // 【主入口】休眠/待机控制；Qpb 走 set_sleeep，Qfctp 映射待机 TLV
    ForbidSleep,           // 【Qpb】禁止休眠开关（FacSwitch，set_forbid_sleep）
    CameraState,           // 【Qpb】摄像头总开关/状态（int，set_camera_state）
    ScreenCameraState,     // 【Qpb】屏侧相机状态（int，set_screen_camera_state）
    CameraLightState,      // 【Qpb】相机补光状态（int，set_camera_light_state）
    CameraSupportState,    // 【Qpb】相机能力/支持项状态（int，set_camera_support_state）
    CameraExposureTime,    // 【Qpb】曝光时间（uint，set_camera_exposure_time）
    DevReset,              // 【Qpb】设备复位（无参/可空 QVariant，set_dev_reset）
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
    LedTest,               // 【Qfctp】显示测试 LED（QVariantMap，setCaseLedTest）
    FactoryReset,          // 【Qfctp】恢复出厂（无参，setCaseFactoryReset）

    LcdBacklight,       // 【Qfctp】LCD 背光（QVariantMap，setCaseLcdBacklight）
    LightReportControl, // 【Qfctp】光感上报开关（QVariantMap，setCaseLightReportControl）
    LightCalibWrite,    // 【Qfctp】光感校准写（QVariantMap，setCaseLightCalibWrite）
    CompensationSet,    // 【Qfctp】吸力补偿开关（QVariantMap，setCaseCompensationSet）

    // 【Qroot】吸奶器 PCBA 串口协议
    RootVibration,       // 0x94 振子控制
    RootFlangeQuery,   // 0x96 法兰状态查询
    RootNtcQuery,        // 0x97 加热 NTC 查询
    RootHeatTempQuery,   // 0x98 加热温度查询
    RootVibStatusQuery,  // 0x99 振子状态查询
    RootPumpTestEnter,   // 0x9D 主机泵测试进入
    RootPumpTestExit,    // 0x9E 主机泵测试退出

    // get commands
    NowMusicInfo,       // 【Qpb】当前播放音乐信息（无参/可空 param，get_now_music_info）
    SdCardInfo,         // 【Qpb】SD 卡信息（无参，get_sd_card_info）
    LightSensorInfo,    // 【主入口】传感类读取入口；Qpb 读光感，Qfctp 兼容映射充电电流
    GetBattery,         // 【Qpb】读电量（无参，get_battery）；【Qfctp】电量 TLV（getCaseBatteryRead）
    ButtonState,        // 【Qpb】按键状态（param.toInt() 为按键索引/掩码，get_button_state）
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
    ChargeCurrentRead,   // 【Qfctp】读充电电流（无参，getCaseChargeCurrentRead）
    AgingStatusRead,     // 【兼容别名】Qfctp 老化状态查询（保留兼容）
    FactoryDoneRead,     // 【兼容别名】Qfctp 产测标识读取（保留兼容）
    DeviceExceptionRead, // 【Qfctp】读设备异常状态（独立入口）
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
Q_DECLARE_METATYPE(ProtocolAmmeterReadingData)
Q_DECLARE_METATYPE(ProtocolJigAmplitudeData)
Q_DECLARE_METATYPE(ProtocolFactoryDoneData)
Q_DECLARE_METATYPE(ProtocolRssiData)
Q_DECLARE_METATYPE(ProtocolMacData)
Q_DECLARE_METATYPE(ProtocolAckData)

#endif // QPB_TYPES_H
