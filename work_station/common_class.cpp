#include "common_class.h"

common_class::common_class() {}

TestFunctionExecutor::TestFunctionExecutor(Qpb* pb) : pb(pb) {
    createTestFunctions();
    connect(pb, SIGNAL(sendGetBrushResponse(int)), this, SLOT(solveGetBrushResponse(int)));
}

void TestFunctionExecutor::executeFunctionByName(const QString& functionName) {
    // 在 functions 向量中查找具有特定名称的函数对象
    for (const auto& namedFunction : testFunctions) {
        if (namedFunction.name == functionName) {
            // 执行找到的函数对象
            namedFunction.function();
            return;  // 执行完成后直接返回
        }
    }

    // 如果没有找到匹配的函数名，则输出错误信息
    qDebug() << "Error: Function with name " << functionName << " not found!";
}

void TestFunctionExecutor::createTestFunctions() {
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
        {"获取设备信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_device_info, pb)); }},
        {"获取基本信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb)); }},
        {"获取外围设备状态", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_periph_state, pb)); }},
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
        {"设置音乐",
         [&]() { sendCommandWithRetry(std::bind(&Qpb::set_music, pb, QByteArray("/data/audio/scan_f.bin"))); }},
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
        // {"绑定SN码", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_sn, pb, FacDevInfoType_TAIL_SN, sn)); }},
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

        {"获取IMU校准结果", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_imu_cali_result, pb)); }},

        {"获取连接信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_connect_info, pb)); }},
        {"获取WiFi信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_wifi_info, pb)); }},
    };
}

void TestFunctionExecutor::solveGetBrushResponse(int data) { getRespone = data; }

int TestFunctionExecutor::sendCommandWithRetry(std::function<void()> commandFunc) {
    static int retryCount = 0;
    canGoNext = false;
    sendRetryOver = false;
    if (commandFunc != nullptr) {
        qDebug() << "发送pb初始指令";
        commandFunc();  // 重新发送指令
    }

    // 启动定时器
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        qDebug() << "retryCount=" << retryCount
                 << QString("sendCommandWithRetry定时器触发时间: %1, timer 地址: %2")
                        .arg(currentTime)
                        .arg(reinterpret_cast<quintptr>(timer), 0, 16);  // 以16进制显示地址

        if (!getRespone) {          // 根据传递进来的条件判断是否未收到响应
            if (retryCount < 20) {  // 如果还有重试次数
                if (commandFunc != nullptr && !(retryCount % 5)) {
                    qDebug() << "重新发送指令发送pb指令";
                    commandFunc();  // 重新发送指令
                }
                retryCount++;
            } else {
                disconnect(timer, &QTimer::timeout, this, nullptr);
                getRespone = 0;
                retryCount = 0;
                sendRetryOver = 1;
                timer->stop();  // 达到最大重试次数，停止定时器

                qDebug() << "达到最大重试次数，停止定时器";
                delete timer;
                return 0;
            }
        } else {  // 如果收到响应
            disconnect(timer, &QTimer::timeout, this, nullptr);
            timer->stop();
            delete timer;
            retryCount = 0;
            getRespone = 0;
            canGoNext = 1;

            qDebug() << "sendCommandWithRetry完成，收到牙刷响应";

            return 1;
        }
        return 0;
    });

    timer->start(300);  // 启动定时器
    return 0;
}
