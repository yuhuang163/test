
#include "qfreework.h"
// 定义一个函数，用于执行具有特定名称的函数
void QFreeWork::executeFunctionByName(const QString functionName) {
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
void QFreeWork::createTestFunctions() {
    testFunctions = {
        {"禁止休眠", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN)); }); }},
        {"蓝牙升级",
         [&]() {
             // sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN)); });
         }},
        {"电机升级",
         [&]() {
             // sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN)); });
         }},
        {"压感升级",
         [&]() {
             // sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN)); });
         }},
        {"打开串口接收", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 1); }); }},
        {"关闭串口接收", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 0); }); }},
        {"设置屏幕颜色", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ScreenColor, 1); }); }},
        {"获取尾盖SN码", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetSn, static_cast<int>(FacDevInfoType_TAIL_SN)); }); }},
        {"获取基本信息", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::BaseInfo); }); }},
        {"获取电量信息", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::Battery); }); }},
        {"进入船运模式", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ShipMode, 1); }); }},
        {"设置UART接收状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 1); }); }},
        {"设置RGB颜色",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::RgbColor, QVariant::fromValue(FacLedControl{/* params */})); }); }},
        {"设置电机校准状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorCali, 1); }); }},
        {"设置电机阻尼状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorDampingState, 1); }); }},
        {"设置电机测试状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorTestState, 1); }); }},
        {"设置工厂结果状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacResult, 1); }); }},

        {"设置LED颜色", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::LedColor, QVariantList{1, 1}); }); }},
        {"设置声波电机参数", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorParam, QVariantList{270, 60}); }); }},
        {"打开声波电机", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorState, 1); }); }},
        {"设置电机校准结果参数",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorCaliResultParam, 100); }); }},
        {"连接WiFi",
         [&]() {
             sendCommandWithRetry([&]() {
                 protocolManager.set(DeviceCmd::WifiConnect, QVariant::fromValue(WifiConnectPayload{QByteArray("SSID"), QByteArray("password")}));
             });
         }},
        {"设置音乐", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Music, QByteArray("music data")); }); }},
        {"设置老化测试模式",
         [&]() {
             sendCommandWithRetry([&]() {
                 QVariantMap m;
                 m["mode"] = 1;
                 m["switch"] = static_cast<int>(FacSwitch_START);
                 protocolManager.set(DeviceCmd::BurningMode, m);
             });
         }},
        {"设置使用记录",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushRecord, QVariant::fromValue(FacSetBrushRecord{/* params */})); }); }},
        {"设置使用时间", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushTime, 1625140800); }); }},
        {"设置休眠状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sleep, static_cast<int>(FacSwitch_START)); }); }},
        {"设置摄像头状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraState, 1); }); }},
        {"设置屏幕摄像头状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ScreenCameraState, 1); }); }},
        {"设置摄像头灯光状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraLightState, 1); }); }},
        {"设置摄像头支持状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraSupportState, 1); }); }},
        {"设置摄像头曝光时间", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraExposureTime, 1000); }); }},
        {"设备复位", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::DevReset); }); }},
        {"使用复位", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushReset); }); }},
        {"设置压力校准结果",
         [&]() {
             // sendCommandWithRetry(std::bind(&Qpb::set_press_cali_result, pb, reinterpret_cast<unsigned
             // short*>(nullptr)));
         }},
        {"发送IMU校准结果",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ImuCaliResult, QVariant::fromValue(ImuCalData{/* params */})); }); }},
        {"发送新的IMU校准结果",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::NewImuCaliResult, QVariant::fromValue(NewImuCalData{/* params */})); }); }},
        {"设置舵机电机参数",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::SevorMotorParam, QVariant::fromValue(SevorMotorParamPayload{90, 30.0f, 50.0f, 100})); }); }},
        {"设置设备模式", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::DeviceMode, 1); }); }},
        {"设置亮白模式", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::DeviceMode, 4); }); }},
        {"设置使用控制状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushControl, 1); }); }},
        {"设置工厂模式", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacMode, 1); }); }},
        {"绑定SN码", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, sn})); }); }},
        {"设置摄像头图片状态", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraPictureState, 1); }); }},
        {"设置本地OTA",
         [&]() {
             local_ota_data x[2] = {/* params */};
            sendCommandWithRetry(
                    [&]() { protocolManager.set(DeviceCmd::LocalOta, QVariant::fromValue(LocalOtaPayload{x[0], x[1]})); });
         }},
        {"启动OTA应用",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::StartOtaApp, QVariant::fromValue(RotasFileStatusReq{/* params */})); }); }},
        {"配置网络应用",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ConfigNetworkApp, QVariant::fromValue(WifiInfo{/* params */})); }); }},
        {"断开WiFi", [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::WifiDisconnect); }); }},
        {"设置新的WiFi连接",
         [&]() {
            sendCommandWithRetry([&]() {
                protocolManager.set(DeviceCmd::NewWifiConnect, QVariant::fromValue(NewWifiConnectPayload{QByteArray("SSID"), QByteArray("password"), QString("192.168.1.1"), QString("8080")}));
            });
         }},
        {"设置压力采集参数",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_START)); }); }},
        {"设置IMU采集参数",
         [&]() { sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ImuCollect, static_cast<int>(FacSwitch_START)); }); }},
        {"获取按钮状态", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::ButtonState, 1); }); }},

        {"获取IMU校准结果", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetImuCaliResult); }); }},
        {"获取设备信息(ota)", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::DeviceInfo); }); }},
        {"获取外围设备状态", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::PeriphState); }); }},
        {"获取连接信息", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::ConnectInfo); }); }},
        {"获取WiFi信息", [&]() { sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::WifiInfo); }); }},
    };
}



