
#include "qfreework.h"

void QFreeWork::createTestFunctions() {
    testFunctions = {
        {"禁止休眠", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN)); }},
        {"蓝牙升级",
         [&]() {
             // sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
         }},
        {"电机升级",
         [&]() {
             // sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
         }},
        {"压感升级",
         [&]() {
             // sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
         }},
        {"打开串口接收", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_uart_receive, pb, 1)); }},
        {"关闭串口接收", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_uart_receive, pb, 0)); }},
        {"设置屏幕颜色", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_screen_color, pb, 1)); }},
        {"获取尾盖SN码", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_sn, pb, FacDevInfoType_TAIL_SN)); }},
        {"获取基本信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb)); }},
        {"获取电量信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_battery, pb)); }},
        {"进入船运模式", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_ship_mode, pb, 1)); }},
        {"设置UART接收状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_uart_receive, pb, 1)); }},
        {"设置RGB颜色",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_rgb_color, pb, FacLedControl{/* params */})); }},
        {"设置电机校准状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_motor_cali, pb, 1)); }},
        {"设置电机阻尼状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_motor_damping_state, pb, 1)); }},
        {"设置电机测试状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_motor_test_state, pb, 1)); }},
        {"设置工厂结果状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_fac_result, pb, 1)); }},

        {"设置LED颜色", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_led_color, pb, 1, 1)); }},
        {"设置声波电机参数", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_motor_param, pb, 270, 60)); }},
        {"打开声波电机", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_motor_state, pb, 1)); }},
        {"设置电机校准结果参数",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_motor_cali_result_param, pb, 100)); }},
        {"连接WiFi",
         [&]() {
             sendCommandWithRetry(std::bind(&Qpb::set_connect_wifi, pb, QByteArray("SSID"), QByteArray("password")));
         }},
        {"设置音乐", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_music, pb, QByteArray("music data"))); }},
        {"设置老化测试模式",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_burning_mode, pb, 1, FacSwitch_START)); }},
        {"设置刷牙记录",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_brush_record, pb, FacSetBrushRecord{/* params */})); }},
        {"设置刷牙时间", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_brush_time, pb, 1625140800)); }},
        {"设置休眠状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_sleeep, pb, FacSwitch_START)); }},
        {"设置摄像头状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_camera_state, pb, 1)); }},
        {"设置屏幕摄像头状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_screen_camera_state, pb, 1)); }},
        {"设置摄像头灯光状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_camera_light_state, pb, 1)); }},
        {"设置摄像头支持状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_camera_support_state, pb, 1)); }},
        {"设置摄像头曝光时间", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_camera_exposure_time, pb, 1000)); }},
        {"设备复位", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_dev_reset, pb)); }},
        {"刷牙复位", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_brush_reset, pb)); }},
        {"设置压力校准结果",
         [&]() {
             // sendCommandWithRetry(std::bind(&Qpb::set_press_cali_result, pb, reinterpret_cast<unsigned
             // short*>(nullptr)));
         }},
        {"发送IMU校准结果",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_imu_cali_result, pb, ImuCalData{/* params */})); }},
        {"发送新的IMU校准结果",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_new_imu_cali_result, pb, NewImuCalData{/* params */})); }},
        {"设置舵机电机参数",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_sevor_motor_param, pb, 90, 30.0f, 50.0f, 100)); }},
        {"设置设备模式", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_device_mode, pb, 1)); }},
        {"设置亮白模式", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_device_mode, pb, 4)); }},
        {"设置刷牙控制状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_brush_control, pb, 1)); }},
        {"设置工厂模式", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_fac_mode, pb, 1)); }},
        {"绑定SN码", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_sn, pb, FacDevInfoType_TAIL_SN, sn)); }},
        {"设置摄像头图片状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_camera_picture_state, pb, 1)); }},
        {"设置本地OTA",
         [&]() {
             local_ota_data x[2] = {/* params */};
             sendCommandWithRetry(std::bind(&Qpb::set_local_ota, pb, x));
         }},
        {"启动OTA应用",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_start_ota_app, pb, RotasFileStatusReq{/* params */})); }},
        {"配置网络应用",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_config_network_app, pb, WifiInfo{/* params */})); }},
        {"断开WiFi", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_wifi_disconnect, pb)); }},
        {"设置新的WiFi连接",
         [&]() {
             sendCommandWithRetry(std::bind(&Qpb::set_new_connect_wifi, pb, QByteArray("SSID"), QByteArray("password"),
                                            QString("192.168.1.1"), QString("8080")));
         }},
        {"设置压力采集参数",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_press_collect_param, pb, FacSwitch_START)); }},
        {"设置IMU采集参数",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_imu_collect_param, pb, FacSwitch_START)); }},
        {"获取按钮状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_button_state, pb, 1)); }},

        {"获取校准结果", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_cali_result, pb)); }},
        {"获取IMU校准结果", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_imu_cali_result, pb)); }},
        {"获取设备信息(ota)", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_device_info, pb)); }},
        {"获取外围设备状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_periph_state, pb)); }},
        {"获取连接信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_connect_info, pb)); }},
        {"获取WiFi信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_wifi_info, pb)); }},
    };
}
