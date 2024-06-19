#ifndef QPB_H
#define QPB_H

#include "ble_protocol/fx_ble_msg.pb.h"
#include "factory_protocol/factory_msg.pb.h"
#include <QObject>
#include <QSerialPort>

typedef struct
{
    int magic;
    short gyro_offset[3];
} ImuCalData;

typedef struct
{
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

typedef struct
{
    short gyro[3];
    short acc[3];
} ImuDataT;

typedef struct
{
    int magic;
    ImuDataT imu_offset;
    float acc_scalar[3];
} ImuCalDataOld;

typedef struct
{
    int is_have_data;
    int file_size;
    int version_code;   // 版本号
    int version_type;   // 文件类型  201固件  202资源  303音频 304主题 305音乐文件
    QByteArray url;     //
    QByteArray md5;     //
} local_ota_data;

class Qpb : public QSerialPort
{
    Q_OBJECT
public:
    void set_rgb_color(FacLedControl data);
    void set_motor_cali(int state);
    void set_motor_damping_state(int state);
    void set_motor_test_state(int state);
    void set_motor_cali_state(int state);
    void set_fac_result(int state);
    explicit Qpb(QSerialPort *parent = nullptr);
    void parseCmd(const QByteArray &byte);
    void sendShortPack(const FactoryDataPackage &pack);
    void sendShortPack(const DataPackage &pack);
    uint32_t sendlongPack(const FactoryDataPackage &pack);
    void waitWork(int ms);
    void getCaliResult();      // 发送校准结果
    void getimuCaliResult();   // 获取校准结果
    void getDeviceInfo();
    void getBaseInfo();
    void set_camera_picture_state(int state);   // 开启补光灯

    // void getFacMotorCalibResult();
    void getPeriphState();
    void setPressCollectParam(FacSwitch sta);
    void setimuCollectParam(FacSwitch sta);   // 设置imu采集开关
    void setPbMode(int p)
    {
        pb_mode = (PB_MODE)p;
    }
    void set_device_mode(int mode);
    void getbattary();
    void getbottonState(int state);
    void getwifistate();
    void disconnect_wifi();   // 断开wifi
    void connect_wifi(const QByteArray &name, const QByteArray &password, const QString &ip,
                      const QString &port);
    void setbrushcontrol(int state);                               // 开始暂停刷牙
    void set_fac_mode(int state);                                  // 开始工厂
    void send_sn(FacDevInfoType which_sn, const QByteArray &sn);   // 绑定sn码
    void get_sn(FacDevInfoType which_sn);
    void set_screen_color(int state);   // 开始屏幕颜色
    void set_led_color(int state, int lednumber);
    void set_ship_mode(int state);   // 开始船运
    void set_motor_param(uint32_t fre, float duty);
    void set_motor_state(int state);
    void set_motor_cali_result_param(uint32_t data);
    void set_connect_wifi(const QByteArray &name, const QByteArray &password);   // 开始连接wifi
    void set_music(const QByteArray &data);
    void set_burning_mode(int state, FacSwitch switchs);   // 开始老化测试
    void set_brush_record(const FacSetBrushRecord &record);
    void set_brush_time(int timestamp);
    void set_sleeep(FacSwitch state);   // 进入休眠状态
    void setDevForbidSleepState(FacSwitch state);
    void set_camera_state(int state);   // 开启摄像头
    void set_screen_camera_state(int state);
    void set_camera_light_state(int state);
    void set_camera_support_state(int state);

    void set_camera_exposure_time(uint32_t time);
    void setDevRstState();   // 复位牙刷
    void set_brush_reset();
    void sendCaliResult(unsigned short *cali_ok);
    void sendimuCaliResult(ImuCalData cali_ok);   // 发送校准结果
    void send_new_imuCaliResult(NewImuCalData cali_ok);
    void set_sevor_motor_param(uint32_t sweeping_angle, float vibrate_angle, float sweeping_freq,
                               uint32_t vibrate_freq);
    int getisHallCali()
    {
        return isHallCali;
    }
    int getis_camera_control()
    {
        return is_camera_control;
    }

    int get_is_damping_state()
    {
        return is_damping_state;
    }
    int get_is_wif_set()
    {
        return is_wif_set;
    }

    int get_is_motor_test_state()
    {
        return is_motor_test_state;
    }
    int get_is_motor_cali_data_set()
    {
        return is_motor_cali_data_set;
    }
    int get_is_get_battery_data()
    {
        return is_get_battery_data;
    }
    int get_is_stop_motor_cali()
    {
        return is_stop_motor_cali;
    }

    int getisZeroCali()
    {
        return isZeroCali;
    }
    bool getDisableSleep()
    {
        return isDisableSleep;
    }
    bool get_is_set_age_test()
    {
        return is_set_age_test;
    }
    bool getCollectParam()
    {
        return isSetPressCollectParam;
    }

    bool getis_imu_set_sta()
    {
        return is_imu_set_sta;
    }

    bool getisDevintowhitemode()
    {
        return isDevintowhitemode;
    }
    bool getisGetBaseInfo()
    {
        return isGetBaseInfo;
    }

    bool get_is_save_press_cali_data()
    {
        return is_save_press_cali_data;
    }

    bool get_is_open_sleep()
    {
        return is_open_sleep;
    }
    bool get_is_close_forbid_sleep()
    {
        return is_close_forbid_sleep;
    }

    bool get_is_banding_ok()
    {
        return is_banding_ok;
    }
    bool get_is_wifi_set_ok()
    {
        return is_wifi_set_ok;
    }

    bool get_is_motor_param_set()
    {
        return is_motor_param_set;
    }

    int get_is_get_imu_cali_data()
    {
        return is_get_imu_cali_data;   // get的返回成功
    }
    bool get_is_save_imu_cali_ok()
    {
        return is_save_imu_cali_ok;   // save的返回成功
    }

    bool get_isSetimuCollectParam()
    {
        return isSetimuCollectParam;
    }
    void set_local_ota(local_ota_data x[2]);

    void reset_all_pb()
    {
        is_wif_set = 0;
        is_damping_state = 0;
        is_motor_test_state = 0;
        is_motor_cali_data_set = 0;
        is_get_battery_data = 0;
        is_stop_motor_cali = 0;
        isHallCali = 0;
        is_camera_control = 0;
        isZeroCali = 0;
        is_banding_ok = 0;
        is_wifi_set_ok = 0;
        is_set_age_test = 0;
        isDisableSleep = 0;
        isSetPressCollectParam = 0;
        is_imu_set_sta = 0;
        isSetimuCollectParam = 0;
        isDevintowhitemode = 0;
        isGetBaseInfo = 0;
        is_save_press_cali_data = 0;
        is_close_forbid_sleep = 0;
        is_motor_param_set = 0;
        is_open_sleep = 0;
        is_get_imu_cali_data = 0;
        is_save_imu_cali_ok = 0;
    }   // 复位参数
    void process_Dev_into_white_mode(FactoryDataPackage &f);
    void process_connect_info(DataPackage &f);
    void process_ota(DataPackage &f);
    void getConnectInfo();
    DataPackage getBlePack() const;
    void setBlePack(const DataPackage &newBlePack);
    void startOTA();
    void configNetwork(WifiInfo info);
    void processFactroyCmd_SET_COLLECT_PARAM(FactoryDataPackage &f);

signals:
    void parseFinished(FactoryDataPackage);
    void pressSensorDataReady(FacUploadPresSensor);
    void baseInfoReady(FacGetDevBaseInfo get_dev_base_info);
    void periphStateReady(FacGetPeriphState state);
    void imuDataReady(FacUploadNineAlex);
    void send_battary(FacDevInfo);
    void send_wifi_State(FacDevInfo);
    void send_sn_data(FacDevInfo);
    void send_button_state(FacButtonState);
    void send_BrushControl_state(FacBrushControl);
    void send_LED_CONTROL_state(FacLedControl);
    void send_Lcd_CONTROL_state(FacLcdControl);
    void send_IMU_CALIB_result(FacImuCalibResult);
    void send_FactroyCmd_INTERNET_OTA(FacInternetOta);
    void send_FactroyCmd_WIFI_DEMAND(FacWifiDemand);
    void send_camera_CONTROL_state(FacCameraControl);
    void send_pb_date(QString data);
    void send_motor_cali_msg(QString data);
    void sendInfo(QString info);
    void sendOTAProgress(int progress);
    void sendOTAResult(int result);
    void sendGetBrushResponse(int data);
private:
    typedef enum
    {
        STATE_IDLE,
        STATE_HEADER,
        STATE_LEN,
        STATE_STAGE,
        STATE_PB_HEADER,
        STATE_UNPACK
    } State;
    State state = STATE_IDLE;
    int hitTimes = 0;
    int len = 1;
    QSerialPort *serialPort;
    std::vector<uint8_t> ibuffer, ipack;
    FactoryDataPackage recievePack = FactoryDataPackage_init_default;
    DataPackage blePack = DataPackage_init_default;
    typedef std::function<void(FactoryDataPackage &)> callback;
    typedef std::function<void(DataPackage &)> ble_callback;
    std::map<FactroyCmd, callback> factoryCommandList;
    std::map<CommandId, ble_callback> bleCommandList;
    uint16_t calCrc16(const std::vector<uint8_t> &d);
    void registerCommand();
    bool is_close_forbid_sleep = 0;
    bool is_open_sleep = 0;
    bool is_motor_param_set = 0;
    bool is_save_press_cali_data = 0;
    int is_get_imu_cali_data = 0;
    bool is_save_imu_cali_ok = 0;
    bool is_set_age_test = 0;
    bool is_banding_ok = 0;
    bool is_wifi_set_ok = 0;

    bool isDisableSleep = 0;
    int isHallCali = 0;
    int is_camera_control = 0;

    int isZeroCali = 0;
    int is_damping_state = 0;
    int is_wif_set = 0;

    int is_motor_test_state = 0;
    int is_stop_motor_cali = 0;
    int is_motor_cali_data_set = 0;
    int is_get_battery_data = 0;

    bool is_imu_set_sta = 0;

    bool isSetPressCollectParam = 0;
    bool isSetimuCollectParam = 0;
    bool isDevintowhitemode = 0;
    bool isGetBaseInfo = 0;
    enum PB_MODE
    {
        CLIENT,
        FACTORY,
    } pb_mode = FACTORY;

private slots:
    void process_FactroyCmd_MOTO_CONTROL(FactoryDataPackage &f);
    void process_FactroyCmd_LCD_CONTROL(FactoryDataPackage &f);
    void process_FactroyCmd_SET_AGEING_TEST(FactoryDataPackage &f);
    void process_FactroyCmd_GET_IMU_CALIB(FactoryDataPackage &f);
    void processPressSensorData(FactoryDataPackage &f);
    void processimuData(FactoryDataPackage &f);
    void process_set_dev_state(FactoryDataPackage &f);
    void process_SET_PRESS_SENSOR_CALIB(FactoryDataPackage &f);
    void process_get_PRESS_SENSOR_CALIB(FactoryDataPackage &f);
    void process_SET_imu_SENSOR_CALIB(FactoryDataPackage &f);
    void process_DEVICE_BASE_INFO(FactoryDataPackage &f);
    void process_GET_PERIPH_STATE(FactoryDataPackage &f);
    void process_dev_info(FactoryDataPackage &f);
    void process_UPLOAD_BUTTON_STATE(FactoryDataPackage &f);
    void process_LED_CONTROL(FactoryDataPackage &f);
    void process_BRUSH_CONTROL(FactoryDataPackage &f);
    void process_FactroyCmd_UPLOAD_MOTORCALI_DATA(FactoryDataPackage &f);
    void process_FactroyCmd_CAMERA_CONTROL(FactoryDataPackage &f);
    void process_FactroyCmd_INTERNET_OTA(FactoryDataPackage &f);
    void process_FactroyCmd_WIFI_DEMAND(FactoryDataPackage &f);
};

#endif   // QPB_H
