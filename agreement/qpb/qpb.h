#ifndef QPB_H
#define QPB_H

#include <QObject>
#include <QSerialPort>

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

// 待整理
typedef enum
{
    MODULE_INVALID,
    MODULE_BTH,
    MODULE_MODE_BUTTON,
    MODULE_POWER_BUTTON,
    MODULE_MAX,
} press_module_e;

typedef struct {
    short calib_factor[MODULE_MAX];
    short temperature[MODULE_MAX];
}press_calib_data_t;
// 待整理

class Qpb : public QSerialPort {
    Q_OBJECT
public:
    QString APP_VERSION;
    int shipCount = 0;
    explicit Qpb(QSerialPort* parent = nullptr);
    void parseCmd(const QByteArray& byte);
    void sendShortPack(const FactoryDataPackage& pack);
    void sendShortPack(const DataPackage& pack);

    void sendMainPack(const DataPackage& pack);
    uint32_t sendlongPack(const FactoryDataPackage& pack);
    void waitWork(int ms);
    void setPbMode(int p) { pb_mode = (PB_MODE)p; }
    DataPackage getBlePack() const;
    void setBlePack(const DataPackage& newBlePack);
    bool getisDevintowhitemode() { return is_dev_into_white_mode; }
    bool getisOtaStart() { return is_ota_start; }
    bool getisSetIamApp() { return is_set_i_am_app; }

    int is_motor_cali_data_set = 0;
    int getisHallCali() { return is_hall_cali; }
    int getis_camera_control() { return is_camera_control; }
    int get_is_damping_state() { return is_damping_state; }
    int get_is_wif_set() { return is_wif_set; }
    int get_is_motor_test_state() { return is_motor_test_state; }
    int get_is_motor_cali_data_set() { return is_motor_cali_data_set; }
    int get_is_get_battery_data() { return is_get_battery_data; }
    int get_is_stop_motor_cali() { return is_stop_motor_cali; }
    int getisZeroCali() { return is_zero_cali; }
    bool getDisableSleep() { return is_disable_sleep; }
    bool getCollectParam() { return is_set_press_collect_param; }
    bool getis_imu_set_sta() { return is_imu_set_sta; }
    bool get_is_close_forbid_sleep() { return is_close_forbid_sleep; }
    bool get_is_banding_ok() { return is_banding_ok; }
    bool get_is_motor_param_set() { return is_motor_param_set; }
    int get_is_get_imu_cali_data() {
        return is_get_imu_cali_data;  // get的返回成功
    }

    bool get_isSetimuCollectParam() { return is_setimu_collect_param; }

    void reset_all_pb() {
        is_dev_into_white_mode = 0;
        is_ota_start = 0;
        is_set_i_am_app = 0;
        is_wif_set = 0;
        is_damping_state = 0;
        is_motor_test_state = 0;
        is_motor_cali_data_set = 0;
        is_get_battery_data = 0;
        is_stop_motor_cali = 0;
        is_hall_cali = 0;
        is_camera_control = 0;
        is_zero_cali = 0;
        is_banding_ok = 0;
        is_disable_sleep = 0;
        is_set_press_collect_param = 0;
        is_imu_set_sta = 0;
        is_setimu_collect_param = 0;
        is_close_forbid_sleep = 0;
        is_motor_param_set = 0;
        is_get_imu_cali_data = 0;
        is_save_press_cali_ok = 0;
    }  // 复位参数
private:
    typedef enum {
        STATE_IDLE,
        STATE_HEADER,
        STATE_CHANNEL,
        STATE_LEN,
        STATE_STAGE,
        STATE_PB_HEADER,
        STATE_UNPACK
    } State;
    enum PB_MODE {
        CLIENT,
        FACTORY,
    } pb_mode = FACTORY;
    typedef enum {
        PHY_CHANNEL_INVALID = 0,  // 无效值
        PHY_CHANNEL_FAC,          // 工厂命令通道
        PHY_CHANNEL_APP,          // app数据通道
        PHY_CHANNEL_MAIN,         // main数据通道
    } ext_ble_phy_channel_e;

    ext_ble_phy_channel_e pbChannel = PHY_CHANNEL_INVALID;
    State state = STATE_IDLE;
    int hitTimes = 0;
    int len = 1;

    QSerialPort* serialPort;
    std::vector<uint8_t> ibuffer, ipack;
    FactoryDataPackage recievePack = FactoryDataPackage_init_default;
    DataPackage blePack = DataPackage_init_default;
    typedef std::function<void(FactoryDataPackage&)> callback;
    typedef std::function<void(DataPackage&)> ble_callback;
    std::map<FactroyCmd, callback> factoryCommandList;
    std::map<CommandId, ble_callback> bleCommandList;
    uint16_t calCrc16(const std::vector<uint8_t>& d);
    void registerCommand();
    bool is_close_forbid_sleep = 0;
    bool is_motor_param_set = 0;
    int is_get_imu_cali_data = 0;
    bool is_save_imu_cali_ok = 0;
    bool is_save_press_cali_ok = 0;
    bool is_banding_ok = 0;
    bool is_dev_into_white_mode = 0;
    bool is_ota_start = 0;
    bool is_set_i_am_app = 0;

    bool is_disable_sleep = 0;
    int is_hall_cali = 0;
    int is_camera_control = 0;
    int is_zero_cali = 0;
    int is_damping_state = 0;
    int is_wif_set = 0;
    int is_motor_test_state = 0;
    int is_stop_motor_cali = 0;

    int is_get_battery_data = 0;
    bool is_imu_set_sta = 0;
    bool is_set_press_collect_param = 0;
    bool is_setimu_collect_param = 0;

public slots:
    void set_press_sensor_temp(int state);
    void set_solve_imu_collect_param(FacSwitch sta);  // 设置imu采集开关
    void set_uart_receive(int state);                 // 设置UART接收状态
    void set_rgb_color(FacLedControl data);           // 设置RGB颜色
    void set_motor_cali(int state);                   // 设置电机校准状态
    void set_motor_damping_state(int state);          // 设置电机阻尼状态
    void set_motor_test_state(int state);             // 设置电机测试状态
    void set_motor_cali_state(int state);             // 设置电机校准状态
    void set_fac_result(int state);                   // 设置工厂结果状态
    void set_screen_color(int state);                 // 设置屏幕颜色
    void set_led_color(int state, int lednumber);     // 设置LED颜色
    void set_ship_mode(int state);                    // 设置船运模式
    void set_motor_adc_switch(int state);
    void set_motor_param(uint32_t fre, float duty);                             // 设置电机参数
    void set_motor_state(int state);                                            // 设置电机状态
    void set_motor_cali_result_param(uint32_t data);                            // 设置电机校准结果参数
    void set_connect_wifi(const QByteArray& name, const QByteArray& password);  // 连接WiFi
    void set_music(const QByteArray& data);                                     // 设置音乐
    void set_burning_mode(int state, FacSwitch switchs);                        // 设置老化测试模式
    void set_brush_record(const FacSetBrushRecord& record);                     // 设置刷牙记录
    void set_brush_time(int timestamp);                                         // 设置刷牙时间
    void set_sleeep(FacSwitch state);                                           // 设置休眠状态
    void set_forbid_sleep(FacSwitch state);                                     // 禁止休眠
    void set_camera_state(int state);                                           // 设置摄像头状态
    void set_screen_camera_state(int state);                                    // 设置屏幕摄像头状态
    void set_camera_light_state(int state);                                     // 设置摄像头灯光状态
    void set_camera_support_state(int state);                                   // 设置摄像头支持状态
    void set_camera_exposure_time(uint32_t time);                               // 设置摄像头曝光时间
    void set_dev_reset();                                                       // 设备复位
    void set_brush_reset();                                                     // 刷牙复位
    void set_press_cali_result(unsigned short* cali_ok);                        // 设置压力校准结果
    void sendCaliResult(press_calib_data_t cali_result);// 待整理
    bool get_is_save_press_cali_data(){return is_save_press_cali_ok;}// 待整理
    void set_imu_cali_result(ImuCalData cali_ok);                               // 发送IMU校准结果
    void set_new_imu_cali_result(NewImuCalData cali_ok);                        // 发送新的IMU校准结果
    void set_sevor_motor_param(uint32_t sweeping_angle, float vibrate_angle, float sweeping_freq,
                               uint32_t vibrate_freq);           // 设置舵机电机参数
    void set_device_mode(int mode);                              // 设置设备模式
    void set_brush_control(int state);                           // 设置刷牙控制状态
    void set_fac_mode(int state);                                // 设置工厂模式
    void set_sn(FacDevInfoType which_sn, const QByteArray& sn);  // 绑定SN码
    void set_base_info(FacBasInfoType which_info, const FacGetDevBaseInfo& data);
    void set_camera_picture_state(int state);                  // 设置摄像头图片状态
    void set_local_ota(local_ota_data x[2]);                   // 设置本地OTA
    void set_start_ota_app(RotasFileStatusReq RotasFiledata);  // 启动OTA应用
    void set_i_am_app();                                       // 骗牙刷是app
    void set_config_network_app(WifiInfo info);                // 配置网络应用
    void set_wifi_disconnect();                                // 断开WiFi
    void set_new_connect_wifi(const QByteArray& name, const QByteArray& password, const QString& ip,
                              const QString& port);  // 设置新的WiFi连接
    void set_press_collect_param(FacSwitch sta);     // 设置压力采集参数
    void set_imu_collect_param(FacSwitch sta);       // 设置IMU采集参数
    void set_camera_fault_data_packet(int count, const QVector<int>& data);
    void set_battery(FacBatteryType type);  // 设置电池信息
    void set_servo_motor_info();
    void get_now_music_info();

public slots:
    void get_battery();                    // 获取电池信息
    void get_button_state(int state);      // 获取按钮状态
    void get_sn(FacDevInfoType which_sn);  // 获取SN码
    void get_cali_result();                // 获取校准结果
    void get_imu_cali_result();            // 获取IMU校准结果
    void get_device_info();                // 获取设备信息
    void get_base_info();                  // 获取基本信息
    void get_periph_state();               // 获取外围设备状态
    void get_connect_info();               // 获取连接信息
    void get_wifi_info();                  // 获取WiFi信息
    void get_servo_motor_info();           // 获取电机信息
    void get_bursh_backlog(int state);
private slots:

    void process_CommandId_ROTAS_RESULT_RSP(DataPackage& f);
    void process_CommandId_ROTAS_FILE_STATUS_REQ(DataPackage& f);
    void process_CommandId_GET_USER_INFO(DataPackage& f);
    void process_CommandId_CONNECT_PRO(DataPackage& f);
    void process_CommandId_ROTAS(DataPackage& f);
    void process_FactroyCmd_SET_COLLECT_PARAM(FactoryDataPackage& f);
    void process_FactroyCmd_SET_DEVICE_INFO(FactoryDataPackage& f);
    void process_FactroyCmd_MOTO_CONTROL(FactoryDataPackage& f);
    void process_FactroyCmd_LCD_CONTROL(FactoryDataPackage& f);
    void process_FactroyCmd_SET_AGEING_TEST(FactoryDataPackage& f);
    void process_FactroyCmd_GET_IMU_CALIB(FactoryDataPackage& f);
    void process_FactroyCmd_UPLOAD_PRESS_SENSOR(FactoryDataPackage& f);
    void process_FactroyCmd_UPLOAD_NINE_ALEX(FactoryDataPackage& f);
    void process_FactroyCmd_SET_DEVICE_STATE(FactoryDataPackage& f);
    void process_FactroyCmd_SET_PRESS_SENSOR_CALIB(FactoryDataPackage& f);
    void process_FactroyCmd_GET_PRESS_SENSOR_CALIB(FactoryDataPackage& f);
    void process_FactroyCmd_SET_IMU_CALIB(FactoryDataPackage& f);
    void process_FactroyCmd_GET_DEVICE_BASE_INFO(FactoryDataPackage& f);
    void process_FactroyCmd_GET_PERIPH_STATE(FactoryDataPackage& f);
    void process_FactroyCmd_GET_DEVICE_INFO(FactoryDataPackage& f);
    void process_FactroyCmd_UPLOAD_BUTTON_STATE(FactoryDataPackage& f);
    void process_FactroyCmd_LED_CONTROL(FactoryDataPackage& f);
    void process_FactroyCmd_BRUSH_CONTROL(FactoryDataPackage& f);
    void process_FactroyCmd_UPLOAD_MOTORCALI_DATA(FactoryDataPackage& f);
    void process_FactroyCmd_CAMERA_CONTROL(FactoryDataPackage& f);
    void process_FactroyCmd_INTERNET_OTA(FactoryDataPackage& f);
    void process_FactroyCmd_WIFI_DEMAND(FactoryDataPackage& f);
    void process_FactroyCmd_UPLOAD_PICTURE_DATA(FactoryDataPackage& f);
    void process_FactroyCmd_FAC_LOG(FactoryDataPackage& f);
signals:
    void send_press_data(FacUploadPresSensor);
    void send_base_data(FacGetDevBaseInfo get_dev_base_info);
    void send_periph_data(FacGetPeriphState state);
    void send_imu_data(FacUploadNineAlex);
    void send_battary(FacDevInfo);
    void send_wifi_State(FacDevInfo);
    void send_sn_data(FacDevInfo);
    void send_music_state(FacDevInfo);
    void send_button_state(FacButtonState);
    void send_BrushControl_state(FacBrushControl);
    void send_LED_CONTROL_state(FacLedControl);
    void send_Lcd_CONTROL_state(FacLcdControl);
    void send_IMU_CALIB_result(FacImuCalibResult);
    void send_FactroyCmd_INTERNET_OTA(FacInternetOta);
    void send_FactroyCmd_WIFI_DEMAND(FacWifiDemand);
    void send_camera_CONTROL_state(FacCameraControl);
    void send_servo_motor_info_msg(FacMotorCalibResult);

    void send_get_picture_send_over(FacPictureDataAck);

    void send_pb_date(QString data);
    void send_ota_flow_control(int state);

    void send_motor_cali_msg(QString data);
    void send_pb_info(QString info);
    void send_ota_progress(int progress);
    void send_ota_result(int result);

    void sendGetBrushResponse(int data);
    void sendpresscalidata(FacPreSensorCalibResult x);// 待整理
};

#endif  // QPB_H
