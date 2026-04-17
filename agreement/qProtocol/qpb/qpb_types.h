#ifndef QPB_TYPES_H
#define QPB_TYPES_H

#include <QByteArray>
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
    int version_code;  // 版本号
    int version_type;  // 文件类型  201固件  202资源  303音频 304主题 305音乐文件
    QByteArray url;    //
    QByteArray md5;    //
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
    PressSensorTemp,
    UartReceive,
    RgbColor,
    MotorCali,
    MotorDampingState,
    MotorTestState,
    MotorCaliState,
    FacResult,
    ScreenColor,
    LedColor,
    ShipMode,
    MotorAdcSwitch,
    MotorParam,
    MotorState,
    MotorCaliResultParam,
    WifiConnect,
    Music,
    BurningMode,
    BrushRecord,
    BrushTime,
    Sleep,
    ForbidSleep,
    CameraState,
    ScreenCameraState,
    CameraLightState,
    CameraSupportState,
    CameraExposureTime,
    DevReset,
    BrushReset,
    PressCaliResult,
    ImuCaliResult,
    NewImuCaliResult,
    DeviceMode,
    BrushControl,
    FacMode,
    Sn,
    BaseInfo,
    CameraPictureState,
    LocalOta,
    StartOtaApp,
    IAmApp,
    ConfigNetworkApp,
    WifiDisconnect,
    StartMultiBleOtaApp,
    PressCollect,
    ImuCollect,
    CameraFaultDataPacket,
    Battery,
    ServoMotorInfo,
    MicControl,
    UploadRecordData,
    NewWifiConnect,
    SevorMotorParam,

    // get commands
    NowMusicInfo,
    SdCardInfo,
    LightSensorInfo,
    GetBattery,
    ButtonState,
    GetSn,
    GetPressCaliResult,
    GetImuCaliResult,
    DeviceInfo,
    GetBaseInfo,
    PeriphState,
    ConnectInfo,
    WifiInfo,
    GetServoMotorInfo,
    BurshBacklog,
};

class IDevice {
public:
    virtual ~IDevice() = default;

    virtual void set(DeviceCmd cmd, const QVariant& data = {}) = 0;
    virtual void get(DeviceCmd cmd, const QVariant& param = {}) = 0;
};

#endif  // QPB_TYPES_H
