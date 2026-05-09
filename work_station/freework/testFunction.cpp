
#include "qfreework.h"

#include <QString>
#include <QVector>

struct FreeWorkTestCatalogItem {
    int id;
    QString name;
};

#define FREEWORK_TEST_LIST(X)                                                                                              \
    X(0, "禁止休眠", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN)); })) \
    /* X(4, "打开串口接收", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 1); })) */      \
    /* X(5, "关闭串口接收", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 0); })) */      \
    /* X(6, "设置屏幕颜色", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ScreenColor, 1); })) */      \
    X(7, "获取整机SN码", true, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::Sn, static_cast<int>(FacDevInfoType_TAIL_SN)); })) \
    X(8, "获取基本信息", true, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::BaseInfo); }))                \
    X(9, "获取电量信息", true, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetBattery); }))               \
    X(10, "关机", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ShipMode, 1); }))           \
    /* X(11, "设置UART接收状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::UartReceive, 1); })) */ \
    /* X(12, "设置RGB颜色", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::RgbColor, QVariant::fromValue(FacLedControl{params})); })) */ \
    /* X(13, "设置电机校准状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorCali, 1); })) */   \
    /* X(14, "设置电机阻尼状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorDampingState, 1); })) */ \
    /* X(15, "设置电机测试状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorTestState, 1); })) */ \
    X(16, "产测完成写入", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacResult, 1); }))      \
    /* X(17, "设置LED颜色", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::LedColor, QVariantList{1, 1}); })) */ \
    /* X(18, "设置声波电机参数", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorParam, QVariantList{270, 60}); })) */ \
    /* X(19, "打开声波电机", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorState, 1); })) */      \
    /* X(20, "设置电机校准结果参数", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::MotorCaliResultParam, 100); })) */ \
    /* X(21, "连接WiFi", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::WifiConnect, QVariant::fromValue(WifiConnectPayload{QByteArray("SSID"), QByteArray("password")})); })) */ \
    /* X(22, "设置音乐", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Music, QByteArray("music data")); })) */ \
    X(23, "设置老化测试模式", false, sendCommandWithRetry([&]() { QVariantMap m; m["mode"] = SETTINGS.value("AGING/BurningMode", 1).toInt(); m["seconds"] = SETTINGS.value("AGING/BurningSeconds", 60*60*4).toInt(); m["switch"] = static_cast<int>(FacSwitch_START); protocolManager.set(DeviceCmd::BurningMode, m); })) \
    /* X(24, "设置使用记录", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushRecord, QVariant::fromValue(FacSetBrushRecord{params})); })) */ \
    /* X(25, "设置使用时间", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushTime, 1625140800); })) */ \
    X(26, "设置休眠状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sleep, static_cast<int>(FacSwitch_START)); })) \
    /* X(27, "设置摄像头状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraState, 1); })) */   \
    /* X(28, "设置屏幕摄像头状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ScreenCameraState, 1); })) */ \
    /* X(29, "设置摄像头灯光状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraLightState, 1); })) */ \
    /* X(30, "设置摄像头支持状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraSupportState, 1); })) */ \
    /* X(31, "设置摄像头曝光时间", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraExposureTime, 1000); })) */ \
    /* X(32, "设备复位", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::DevReset); })) */               \
    /* X(33, "使用复位", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushReset); })) */             \
    /* X(34, "设置压力校准结果", false, sendCommandWithRetry([&]() { std::bind(&Qpb::set_press_cali_result, pb, reinterpret_cast<unsigned short*>(nullptr)); })) */ \
    /* X(35, "发送IMU校准结果", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ImuCaliResult, QVariant::fromValue(ImuCalData{params})); })) */ \
    /* X(36, "发送新的IMU校准结果", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::NewImuCaliResult, QVariant::fromValue(NewImuCalData{params})); })) */ \
    /* X(37, "设置舵机电机参数", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::SevorMotorParam, QVariant::fromValue(SevorMotorParamPayload{90, 30.0f, 50.0f, 100})); })) */ \
    /* X(38, "设置设备模式", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::DeviceMode, 1); })) */      \
    /* X(39, "设置亮白模式", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::DeviceMode, 4); })) */      \
    /* X(40, "设置使用控制状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::BrushControl, 1); })) */ \
    X(41, "设置工厂模式", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::FacMode, 1); }))            \
    X(42, "写入SN码", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_TAIL_SN, expectedTailSnFromUi})); })) \
    /* X(43, "设置摄像头图片状态", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::CameraPictureState, 1); })) */ \
    /* X(44, "设置本地OTA", false, local_ota_data x[2] = {params}; sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::LocalOta, QVariant::fromValue(LocalOtaPayload{x[0], x[1]})); })) */ \
    /* X(45, "启动OTA应用", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::StartOtaApp, QVariant::fromValue(RotasFileStatusReq{params})); })) */ \
    /* X(46, "配置网络应用", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ConfigNetworkApp, QVariant::fromValue(WifiInfo{params})); })) */ \
    /* X(47, "断开WiFi", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::WifiDisconnect); })) */          \
    /* X(48, "设置新的WiFi连接", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::NewWifiConnect, QVariant::fromValue(NewWifiConnectPayload{QByteArray("SSID"), QByteArray("password"), QString("192.168.1.1"), QString("8080")})); })) */ \
    /* X(49, "设置压力采集参数", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_START)); })) */ \
    /* X(50, "设置IMU采集参数", false, sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ImuCollect, static_cast<int>(FacSwitch_START)); })) */ \
    /* X(51, "获取按钮状态", false, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::ButtonState, 1); }))  */      \
    /* X(52, "获取IMU校准结果", false, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::GetImuCaliResult); })) */ \
    /* X(53, "获取设备信息(ota)", false, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::DeviceInfo); })) */    \
    X(54, "获取外围设备状态", true, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::PeriphState); }))    \
    /* X(55, "获取连接信息", false, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::ConnectInfo); })) */        \
    X(56, "获取WiFi信息", false, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::WifiInfo); })) \
    X(57, "读取治具电流测量值", true, sendCommandWithRetry([&]() { usb->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement); })) \
    X(58, "直连接蓝牙", false, sendCommandWithRetry([&]() { at->sendDcon(macAddress); }, 6 * 1000)) \
    X(59, "获取BT RSSI", true, sendCommandWithRetry([&]() { QVariantMap m; m["mode"] = 1; protocolManager.get(DeviceCmd::RssiRead, m); })) \
    X(60, "获取BLE RSSI", true, sendCommandWithRetry([&]() { QVariantMap m; m["mode"] = 0; protocolManager.get(DeviceCmd::RssiRead, m); })) \
    X(61, "读取充电电流", true, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::ChargeCurrentRead); })) \
    X(62, "获取云端三元组", true, applyTupleByMac()) \
    X(63, "写入productKey", false, if (failTupleWriteIfNoValidField(QStringLiteral("写入productKey"), !tupleData_.productKey.isEmpty(), QStringLiteral("productKey为空"))) return; sendCommandWithRetry([&]() { stepRuntime_.testData = tupleData_.productKey; protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SKUID, tupleData_.productKey.toUtf8()})); })) \
    X(64, "写入deviceName", false, if (failTupleWriteIfNoValidField(QStringLiteral("写入deviceName"), !tupleData_.deviceName.isEmpty(), QStringLiteral("deviceName为空"))) return; sendCommandWithRetry([&]() { stepRuntime_.testData = tupleData_.deviceName; protocolManager.set(DeviceCmd::Sn, QVariant::fromValue(DeviceSnPayload{FacDevInfoType_SUB_PID, tupleData_.deviceName.toUtf8()})); })) \
    X(65, "写入deviceSecret", false, if (failTupleWriteIfNoValidField(QStringLiteral("写入deviceSecret"), !tupleData_.deviceSecret.isEmpty(), QStringLiteral("deviceSecret为空"))) return; sendCommandWithRetry([&]() { stepRuntime_.testData = tupleData_.deviceSecret; QVariantMap map; map["value"] = tupleData_.deviceSecret.toUtf8(); protocolManager.set(DeviceCmd::WriteKey, map); })) \
    X(66, "读取设备三元组并比较", true, sendCommandWithRetry([&]() { protocolManager.get(DeviceCmd::TupleRead); })) \
    X(67, "上报三元组写入记录", true, reportTupleWriteRecord()) \
    X(68, "扫描连接蓝牙", false, sendCommandWithRetry([&]() { at->sendMac(macAddress); }, 6 * 1000)) \
    X(69, "电源键测试", true, startKeyButtonTest("电源键测试", "请短按下旋钮", "ProductInfo/KeyIdPower", "ProductInfo/KeyIdPower_checkBox")) \
    X(70, "开始/暂停键测试", true, startKeyButtonTest("开始/暂停键测试", "请短按下开始/暂停按钮", "ProductInfo/KeyIdStartPause", "ProductInfo/KeyIdStartPause_checkBox")) \
    X(71, "模式键测试", true, startKeyButtonTest("模式键测试", "请短按下模式按钮", "ProductInfo/KeyIdMode", "ProductInfo/KeyIdMode_checkBox")) \
    X(72, "速度键测试", true, startKeyButtonTest("速度键测试", "请短按下速度按钮", "ProductInfo/KeyIdSpeed", "ProductInfo/KeyIdSpeed_checkBox")) \
    X(73, "程序键测试", true, startKeyButtonTest("程序键测试", "请短按下程序按钮", "ProductInfo/KeyIdProgram", "ProductInfo/KeyIdProgram_checkBox")) \
    X(74, "左键测试", true, startKeyButtonTest("左键测试", "请短按下左按钮", "ProductInfo/KeyIdLeft", "ProductInfo/KeyIdLeft_checkBox")) \
    X(75, "右键测试", true, startKeyButtonTest("右键测试", "请短按下右按钮", "ProductInfo/KeyIdRight", "ProductInfo/KeyIdRight_checkBox")) \
    X(76, "左旋键测试", true, startKeyButtonTest("左旋键测试", "请左旋按钮", "ProductInfo/KeyIdLeftRotate", "ProductInfo/KeyIdLeftRotate_checkBox")) \
    X(77, "右旋键测试", true, startKeyButtonTest("右旋键测试", "请右旋按钮", "ProductInfo/KeyIdRightRotate", "ProductInfo/KeyIdRightRotate_checkBox")) \
    X(78, "进入吸力测试模式", false, sendCommandWithRetry([&]() { QVariantMap m; m["enter"] = 1; protocolManager.set(DeviceCmd::SuctionMode, m); })) \
    X(79, "退出吸力测试模式", false, sendCommandWithRetry([&]() { QVariantMap m; m["enter"] = 0; protocolManager.set(DeviceCmd::SuctionMode, m); })) \

QVector<FreeWorkTestCatalogItem> getFreeWorkTestCatalog() {
    return {
#define BUILD_CATALOG_ITEM(id, name, needCaseDone, actionExpr) {id, name},
        FREEWORK_TEST_LIST(BUILD_CATALOG_ITEM)
#undef BUILD_CATALOG_ITEM
    };
}

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
#define BUILD_TEST_FUNCTION(id, name, needCaseDone, actionExpr) {id, name, [&]() { actionExpr; }, needCaseDone},
        FREEWORK_TEST_LIST(BUILD_TEST_FUNCTION)
#undef BUILD_TEST_FUNCTION
    };
}

#undef FREEWORK_TEST_LIST
