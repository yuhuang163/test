#include "qsetting_bindings.h"

#include "my_set/my_typedef.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>
#include <QWidget>

#include <cstring>

namespace {

enum class K : uint8_t { Le,
                         Cb,
                         Pair,
                         Ci,
                         Ct,
                         Tip };

struct Row {
    K kind;
    const char* w1;
    const char* w2;
    const char* k1;
    const char* k2;
    const char* d1;
    const char* d2;
};

#define LE(line, key, def) {K::Le, line, nullptr, key, nullptr, def, nullptr}
#define CB(cb, key, def) {K::Cb, cb, nullptr, key, nullptr, def, nullptr}
#define P(line, cb, vk, ek, ld, cd) {K::Pair, line, cb, vk, ek, ld, cd}
#define CI(combo, key, def) {K::Ci, combo, nullptr, key, nullptr, def, nullptr}
#define CT(combo, key) {K::Ct, combo, nullptr, key, nullptr, nullptr, nullptr}
/** 仅悬停说明（不参与 load/save）；k1 为完整 tooltip 文案 */
#define TIP(widget, text) {K::Tip, widget, nullptr, text, nullptr, nullptr, nullptr}

static const Row kSettings[] = {
    CB("checkBox_ShowLocalOTAFunc", "SYSTEM/ShowLocalOTAFunc", nullptr),
    CB("checkBox_ShowUpperComputerOTAFunc", "SYSTEM/ShowUpperComputerOTAFunc", nullptr),
    CB("checkBox_SaveToothbrushLog", "SYSTEM/SaveToothbrushLog", nullptr),
    CB("checkBox_LockProductUI", "SYSTEM/LockProductUI", nullptr),
    CB("checkBox_SimplePcbaTest", "SYSTEM/SimplePcbaTest", nullptr),
    CB("checkBox_NeedWriteSubpid", "SYSTEM/NeedWriteSubpid", nullptr),
    CB("checkBox_NeedWriteSkuid", "SYSTEM/NeedWriteSkuid", nullptr),
    CB("checkBox_BluetoothImageTransfer", "SYSTEM/BluetoothImageTransfer", nullptr),
    CB("checkBox_IMUCalibrationWakeup", "SYSTEM/IMUCalibrationWakeup", nullptr),
    CB("checkBox_DisableSerialPortRx", "SYSTEM/DisableSerialPortRx", nullptr),
    CB("checkBox_ShipModeResponse", "SYSTEM/ShipModeResponse", nullptr),
    CB("checkBox_SerialPortMAC", "SYSTEM/SerialPortMAC", nullptr),
    CB("checkBox_MagneticReuseMotorStatus", "SYSTEM/MagneticReuseMotorStatus", nullptr),
    CB("checkBox_TestAudioCurrent", "SYSTEM/TestAudioCurrent", nullptr),
    CB("checkBox_TestShippingCurrent", "SYSTEM/TestShippingCurrent", nullptr),
    CB("checkBox_PressIndependent", "SYSTEM/PressIndependent", nullptr),
    CB("checkBox_PressWindow", "SYSTEM/PressWindow", nullptr),
    CB("checkBox_SendMotorCalibration", "SYSTEM/SendMotorCalibration", nullptr),
    CB("checkBox_LightTest", "SYSTEM/LightTest", nullptr),
    CB("checkBox_uperMotor", "SYSTEM/uperMotor", nullptr),
    CB("checkBox_ServoMotorStart", "SYSTEM/ServoMotorStart", nullptr),
    CB("checkBox_TestWifiSignal", "SYSTEM/TestWifiSignal", nullptr),
    CB("checkBox_IMULastEnterStartTest", "SYSTEM/IMULastEnterStartTest", nullptr),
    CB("checkBox_amplitudeLimit", "Press/AmplitudeLimit", "false"),
    CB("checkBox_freeInstrumentBleBrushCmwConcurrent", "FreeInstrument/BleBrushCmwConcurrent", "false"),
    CB("checkBox_freeInstrumentBleBrushCmwOnStopPer", "FreeInstrument/BleBrushCmwOnStopPer", "true"),
    CB("checkBox_plcModbusTrace", "PLC/ModbusTrace", "false"),
    CB("checkBox_plcConnectVerifyRead", "PLC/ConnectVerifyRead", "true"),
    CB("checkBox_freework_press0", "FreeWorkPeripheral/Press0_checkBox", "true"),
    CB("checkBox_freework_press1", "FreeWorkPeripheral/Press1_checkBox", "true"),
    CB("checkBox_freework_battery_ic", "FreeWorkPeripheral/BatteryIC_checkBox", "true"),
    CB("checkBox_freework_touch_ic", "FreeWorkPeripheral/TouchIC_checkBox", "true"),
    CB("checkBox_freework_led_ic", "FreeWorkPeripheral/LedIC_checkBox", "true"),
    CB("checkBox_freework_pd_ic", "FreeWorkPeripheral/PdIC_checkBox", "true"),
    CB("checkBox_mesRetest", "Mes/RETEST", "false"),

    LE("lineEdit_CurrentMechine", "SYSTEM/CurrentMechine", nullptr),
    LE("snLineEdit", "Regex/SNPattern", nullptr),
    LE("wifiAccountLineEdit", "WIFI/Name", nullptr),
    LE("wifiPasswordLineEdit", "WIFI/Password", nullptr),
    LE("wifiUpperLimitLineEdit", "WIFI/HighRssi", nullptr),
    LE("wifiLowerLimitLineEdit", "WIFI/LowRssi", nullptr),
    LE("wifiIPLineEdit", "WIFI/IP", nullptr),
    LE("bluetoothUpperLimitLineEdit", "BLE/HighRssi", nullptr),
    LE("bluetoothLowerLimitLineEdit", "BLE/LowRssi", nullptr),
    LE("signalTestCountLineEdit", "BLE/RssiCount", nullptr),
    LE("lineEdit_ageingBurningMode", "AGING/BurningMode", "1"),
    LE("lineEdit_ageingBurningSeconds", "AGING/BurningSeconds", "14400"),
    LE("lineEdit_brushInstrumentSendPacketCount", "BrushInstrument/InstrumentSendPacketCount", "1000"),
    LE("lineEdit_brushInstrumentMaxPer", "BrushInstrument/MaxPer", "0.05"),
    LE("lineEdit_brushInstrumentPacketPhaseWaitMs", "BrushInstrument/PacketPhaseWaitMs", "2000"),
    LE("lineEdit_brushInstrumentStopAckTimeoutMs", "BrushInstrument/StopAckTimeoutMs", "5000"),
    LE("lineEdit_plcIpAddress", "PLC/IpAddress", "192.168.1.88"),
    LE("lineEdit_plcPort", "PLC/Port", "502"),
    LE("lineEdit_plcUnitId", "PLC/UnitId", "1"),
    LE("lineEdit_plcMCoilOffset", "PLC/MCoilAddressOffset", "0"),
    LE("lineEdit_plcMBase", "PLC/MBase", "200"),
    LE("lineEdit_plcMBaseStationStep", "PLC/MBaseStationStep", "20"),
    LE("lineEdit_plcConnectTimeoutMs", "PLC/ConnectTimeoutMs", "3000"),
    LE("lineEdit_plcRequestTimeoutMs", "PLC/RequestTimeoutMs", "2000"),
    LE("lineEdit_plcCommandGapMs", "PLC/CommandGapMs", "80"),
    LE("lineEdit_plcSwitchDoneResetM", "PLC/SwitchTestDoneResetM", "211"),
    LE("lineEdit_plcSwitchDoneResetPulseMs", "PLC/SwitchTestDoneResetPulseMs", "0"),
    LE("lineEdit_tupleAuthUser", "Tuple/AuthUser", nullptr),
    LE("lineEdit_tupleAuthPassword", "Tuple/AuthPassword", nullptr),

    LE("lineEdit_factoryCloudStationKey", "FactoryCloud/StationKey", nullptr),
    CB("checkBox_factoryCloudLogUpload", "FactoryCloud/Feature/LogUpload", "true"),
    CB("checkBox_factoryCloudTestCaseSync", "FactoryCloud/Feature/TestCaseSync", "true"),
    CB("checkBox_factoryCloudTestDataUpload", "FactoryCloud/Feature/TestDataUpload", "true"),
    CB("checkBox_factoryCloudHostOta", "FactoryCloud/Feature/HostOtaCheck", "true"),
    CB("checkBox_factoryCloudOfflineBypass", "FactoryCloud/OfflineBypassEnabled", "false"),
    LE("lineEdit_factoryCloudOfflineUser", "FactoryCloud/OfflineBypassUser", "offline"),
    LE("lineEdit_factoryCloudOfflinePassword", "FactoryCloud/OfflineBypassPassword", "offline26"),
    LE("lineEdit_tupleSku", "Tuple/Sku", ""),
    LE("lineEdit_tuplePosition", "Tuple/Position", "L"),
    LE("rowLineEdit", "User/formRow", nullptr),
    LE("columnLineEdit", "User/formColumn", nullptr),
    LE("lineEdit_MusicStatus", "FIXTEST/MusicState", nullptr),
    LE("lineEdit_OvervoltageLightStatus", "FIXTEST/OverVoltageLight", nullptr),
    LE("lineEdit_Button1Status", "FIXTEST/Button1", nullptr),
    LE("lineEdit_Button2Status", "FIXTEST/Button2", nullptr),
    LE("lineEdit_testWaitTime", "PRESSURE/TestTime", nullptr),
    LE("lineEdit_caliWaitTime", "PRESSURE/CaliTime", nullptr),
    LE("lineEdit_PressMechine", "PRESSURE/PressMechine", nullptr),
    LE("lineEdit_ButtonThreshold", "PRESSURE/ButtonThreshold", nullptr),
    LE("lineEdit_ButtonThresholdCount", "PRESSURE/ButtonThresholdCount", nullptr),
    LE("lineEdit_BrushThreshold", "PRESSURE/BrushThreshold", nullptr),
    LE("lineEdit_BrushThresholdCount", "PRESSURE/BrushThresholdCount", nullptr),
    LE("lineEdit_press_adc_shake", "PRESSURE/ADCShakeValue", nullptr),
    LE("lineEdit_amplitudeLimitUpper", "Press/AmplitudeLimitUpper", "0"),
    LE("lineEdit_amplitudeLimitLower", "Press/AmplitudeLimitLower", "0"),
    LE("lineEdit_amplitudeError", "Pressure/AmplitudeError", "0"),
    LE("bthPressUpperLimitLineEdit", "PRESSURE/bth_upper", nullptr),
    LE("bthPressLowerLimitLineEdit", "PRESSURE/bth_lower", nullptr),
    LE("modelPressUpperLimitLineEdit", "PRESSURE/model_button_upper", nullptr),
    LE("modelPressLowerLimitLineEdit", "PRESSURE/model_button_lower", nullptr),
    LE("powerPressUpperLimitLineEdit", "PRESSURE/power_button_upper", nullptr),
    LE("powerPressLowerLimitLineEdit", "PRESSURE/power_button_lower", nullptr),
    LE("lineEdit_calibrationTime", "IMU/IMU_Wait_Time", nullptr),
    LE("lineEdit_comparisonValue", "IMU/ImuCompareData", nullptr),
    LE("lineEdit_singlePointTimeout", "IMU/imu_cali_wait_time", nullptr),
    LE("lineEdit_zAxisUpper", "IMU/acc_z_up", nullptr),
    LE("lineEdit_zAxisLower", "IMU/acc_z_down", nullptr),
    LE("lineEdit_xAxisUpper", "IMU/acc_x_up", nullptr),
    LE("lineEdit_xAxisLower", "IMU/acc_x_down", nullptr),
    LE("lineEdit_yAxisUpper", "IMU/acc_y_up", nullptr),
    LE("lineEdit_yAxisLower", "IMU/acc_y_down", nullptr),
    LE("lineEdit_threshold", "IMU/STATIC_CONV_VAR", nullptr),
    LE("lineEdit_delayFrames", "IMU/STATIC_CONV_DELAY", nullptr),
    LE("lineEdit_sampleFrames", "IMU/STATIC_CONV_COUNT", nullptr),
    LE("lineEdit_mac_sn_path", "MAC_SN/FilePath", "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt"),
    LE("lineEdit_KeyCapLow", "KeyCap/Low", "1"),
    LE("lineEdit_KeyCapHigh", "KeyCap/High", "65535"),
    LE("lineEdit_KeyCapReadTimeoutMs", "KeyCap/ReadTimeoutMs", "5000"),
    LE("lineEdit_KeyCapReadCount", "KeyCap/ReadCount", "3"),
    LE("lineEdit_KeyCapReadIntervalMs", "KeyCap/ReadIntervalMs", "80"),
    LE("lineEdit_KeyCapSingleReadTimeoutMs", "KeyCap/SingleReadTimeoutMs", "2000"),
    LE("lineEdit_CargoCurrentUpper", "Current/HighshipCurrent", nullptr),
    LE("lineEdit_CargoCurrentLower", "Current/LowshipCurrent", nullptr),
    LE("lineEdit_ChargingCurrentUpper", "Current/HighCharCurrent", nullptr),
    LE("lineEdit_ChargingCurrentLower", "Current/LowCharCurrent", nullptr),
    LE("lineEdit_WorkingCurrentUpper", "Current/HighworkCurrent", nullptr),
    LE("lineEdit_WorkingCurrentLower", "Current/LowworkCurrent", nullptr),
    LE("lineEdit_StaticCurrentUpper", "Current/HighstaticCurrent", nullptr),
    LE("lineEdit_StaticCurrentLower", "Current/LowstaticCurrent", nullptr),
    LE("lineEdit_AudioCurrentUpper", "Current/HighmusicCurrent", nullptr),
    LE("lineEdit_AudioCurrentLower", "Current/LowmusicCurrent", nullptr),
    LE("lineEdit_TestDuration", "Current/measure_wait_time", nullptr),
    LE("lineEdit_freework_press0_status", "PeripheralStatus/Press0_Status", nullptr),
    LE("lineEdit_freework_press1_status", "PeripheralStatus/Press1_Status", nullptr),
    LE("lineEdit_freework_battery_ic_status", "PeripheralStatus/BatteryIc_Status", nullptr),
    LE("lineEdit_freework_touch_ic_status", "PeripheralStatus/TouchIc_Status", nullptr),
    LE("lineEdit_freework_led_ic_status", "PeripheralStatus/LedIc_Status", nullptr),
    LE("lineEdit_freework_pd_ic_status", "PeripheralStatus/PdIc_Status", nullptr),
    LE("lineEdit_battery_voltage_control", "BATTARY/standbattary", nullptr),
    LE("lineEdit_camera_x", "CAMERA/Rect1_X", nullptr),
    LE("lineEdit_camera_y", "CAMERA/Rect1_Y", nullptr),
    LE("lineEdit_camera_width", "CAMERA/Rect1_Width", nullptr),
    LE("lineEdit_camera_height", "CAMERA/Rect1_Height", nullptr),
    LE("lineEdit_image_interval", "CAMERA/CameraGetTime", nullptr),
    LE("lineEdit_start_dirty_time", "CAMERA/startDirtyTime", nullptr),
    LE("lineEdit_macStation", "MES/xwdWpCode", nullptr),
    LE("lineEdit_station", "MES/machineNo", nullptr),
    LE("lineEdit_mesUrl", "MES/NET", nullptr),
    LE("lineEdit_mes_config_file_path", "MES/ConfigFilePath", nullptr),
    LE("lineEdit_processName", "MES/test_station", nullptr),
    LE("lineEdit_modelName", "MES/model", nullptr),
    LE("lineEdit_mac_field", "MES/FIELD", nullptr),
    LE("lineEdit_mes_operator", "MES/mUserno", nullptr),
    LE("lineEdit_weikesen_order", "MES/Work_Order", nullptr),
    LE("lineEdit_mes_login_account", "MES/M_USERNO", nullptr),
    LE("lineEdit_action_huaqin", "MES/Action", nullptr),
    LE("lineEdit_mes_login_station", "MES/M_MACHINENO", nullptr),
    LE("lineEdit_line_huaqin", "MES/Line", nullptr),
    LE("lineEdit_mes_login_password", "MES/M_PASSWORD", nullptr),

    P("lineEdit_music_state", "checkBox_MusicState", "Music/MusicState", "Music/MusicState_checkBox", nullptr, nullptr),
    P("lineEdit_ProductName", "checkBox_ProductName", "ProductInfo/Product_Name", "ProductInfo/ProductName_checkBox",
      nullptr, nullptr),
    P("lineEdit_HardwareVersion", "checkBox_HardwareVersion", "ProductInfo/Hardware_Version",
      "ProductInfo/HardwareVersion_checkBox", nullptr, nullptr),
    P("lineEdit_SoftwareVersion", "checkBox_SoftwareVersion", "ProductInfo/Software_Version",
      "ProductInfo/SoftwareVersion_checkBox", nullptr, nullptr),
    P("lineEdit_ResourceVersion", "checkBox_ResourceVersion", "ProductInfo/Resource_Version",
      "ProductInfo/ResourceVersion_checkBox", nullptr, nullptr),
    P("lineEdit_AgingStatus", "checkBox_AgingStatus", "ProductInfo/Age_State", "ProductInfo/AgingStatus_checkBox",
      nullptr, nullptr),
    P("lineEdit_MotorVersion", "checkBox_MotorVersion", "ProductInfo/Motor_Ver", "ProductInfo/MotorVersion_checkBox",
      nullptr, nullptr),
    P("lineEdit_BluetoothVersion", "checkBox_BluetoothVersion", "ProductInfo/Ble_Ver",
      "ProductInfo/BluetoothVersion_checkBox", nullptr, nullptr),
    P("lineEdit_AppPB", "checkBox_AppPB", "ProductInfo/App_Protocol_Version", "ProductInfo/AppPB_checkBox", nullptr,
      nullptr),
    P("lineEdit_FactoryPB", "checkBox_FactoryPB", "ProductInfo/Factory_Protocol_Version",
      "ProductInfo/FactoryPB_checkBox", nullptr, nullptr),
    P("lineEdit_AlgorithmVersion", "checkBox_AlgorithmVersion", "ProductInfo/Algorithm_Version",
      "ProductInfo/AlgorithmVersion_checkBox", nullptr, nullptr),
    P("lineEdit_PressureVersion", "checkBox_PressureVersion", "ProductInfo/Pressure_Sense_Version",
      "ProductInfo/PressureVersion_checkBox", nullptr, nullptr),
    P("lineEdit_FSensorVersion", "checkBox_FSensorVersion", "ProductInfo/FSensor_Version",
      "ProductInfo/FSensorVersion_checkBox", nullptr, nullptr),
    P("lineEdit_ImuID", "checkBox_ImuID", "ProductInfo/IMU_ID", "ProductInfo/ImuID_checkBox", nullptr, nullptr),
    P("lineEdit_CameraID", "checkBox_CameraID", "ProductInfo/Camera_Id", "ProductInfo/CameraID_checkBox", nullptr,
      nullptr),
    P("lineEdit_KeyIdPower", "checkBox_KeyIdPower", "ProductInfo/KeyIdPower", "ProductInfo/KeyIdPower_checkBox", "1",
      "true"),
    P("lineEdit_KeyIdStartPause", "checkBox_KeyIdStartPause", "ProductInfo/KeyIdStartPause",
      "ProductInfo/KeyIdStartPause_checkBox", "2", "true"),
    P("lineEdit_KeyIdMode", "checkBox_KeyIdMode", "ProductInfo/KeyIdMode", "ProductInfo/KeyIdMode_checkBox", "3",
      "true"),
    P("lineEdit_KeyIdSpeed", "checkBox_KeyIdSpeed", "ProductInfo/KeyIdSpeed", "ProductInfo/KeyIdSpeed_checkBox", "4",
      "true"),
    P("lineEdit_KeyIdProgram", "checkBox_KeyIdProgram", "ProductInfo/KeyIdProgram", "ProductInfo/KeyIdProgram_checkBox",
      "5", "true"),
    P("lineEdit_KeyIdLeft", "checkBox_KeyIdLeft", "ProductInfo/KeyIdLeft", "ProductInfo/KeyIdLeft_checkBox", "6",
      "true"),
    P("lineEdit_KeyIdRight", "checkBox_KeyIdRight", "ProductInfo/KeyIdRight", "ProductInfo/KeyIdRight_checkBox", "7",
      "true"),
    P("lineEdit_KeyIdLeftRotate", "checkBox_KeyIdLeftRotate", "ProductInfo/KeyIdLeftRotate",
      "ProductInfo/KeyIdLeftRotate_checkBox", "10", "true"),
    P("lineEdit_KeyIdRightRotate", "checkBox_KeyIdRightRotate", "ProductInfo/KeyIdRightRotate",
      "ProductInfo/KeyIdRightRotate_checkBox", "11", "true"),
    P("lineEdit_imu_status", "checkBox_imu_status", "PeripheralStatus/IMU_Status", "PeripheralStatus/IMUStatus_checkBox",
      nullptr, nullptr),
    P("lineEdit_flash_status", "checkBox_flash_status", "PeripheralStatus/Flash_Status",
      "PeripheralStatus/FlashStatus_checkBox", nullptr, nullptr),
    P("lineEdit_magnetic_status", "checkBox_magnetic_status", "PeripheralStatus/Magnetic_Status",
      "PeripheralStatus/MagneticStatus_checkBox", nullptr, nullptr),
    P("lineEdit_pressure_status", "checkBox_pressure_status", "PeripheralStatus/Pressure_Status",
      "PeripheralStatus/PressureStatus_checkBox", nullptr, nullptr),
    P("lineEdit_audio_status", "checkBox_audio_status", "PeripheralStatus/Audio_Status",
      "PeripheralStatus/AudioStatus_checkBox", nullptr, nullptr),

    CI("comboBox_pressFunctionSwitch", "PRESSURE/functionSwitch", "0"),
    CI("comboBox_displayImageType", "PRESSURE/Use_graph", "0"),
    CI("comboBox_individualMode", "PRESSURE/Module", "0"),
    CT("comboBox_factory", "Mes/FACTORY"),
    CT("comboBox_productName", "MES/Product_Name"),

    TIP("radioButtonDebug", "MAIN_TEST 调试上位机。"),
    TIP("radioButtonFreeWorkstation", "FREE_WORK 自由工站。"),
    TIP("radioButtonStaticCurrent", "QUIESCENT_CURRENT 静态电流工站。"),
    TIP("radioButtonMotorCalibration", "MOTOR_TEST 电机工站。"),
    TIP("radioButtonImuCalibration", "IMU_CALI IMU 校准工站。"),
    TIP("radioButtonScreenTest", "SCREEN_TEST 屏幕工站。"),
    TIP("radioButtonCameraTest", "CAMERA_TEST 摄像头工站。"),
    TIP("radioButtonSignalTest", "WIFIBLE_TEST 蓝牙/WiFi 工站。"),
    TIP("radioButtonAgingTest", "AGE_TEST 老化工站。"),
    TIP("radioButtonPressTest", "PRESS_TEST 压感工站。"),
    TIP("radioButtonBoardFactoryTest", "PCBA_TEST 板厂工站。"),
    TIP("radioButtonKeyTest", "KEY_TEST 按键工站。"),
    TIP("radioButtonSuctionTest", "SUCTION_TEST 吸力工站。"),

    TIP("groupBox_3", "MES：工厂、产品名、工站、URL 等；工厂下拉联动显示字段。"),
    TIP("radioButtonGroupWidget", "工站类型 SYSTEM/station；切换后重启程序。"),
    TIP("groupBox_2", "1 拖多：主界面分窗行/列（User/formRow、formColumn）。"),
    TIP("groupBox_4", "电流阈值：船运/充电/工作/静态/音频（Current/*）。"),
    TIP("groupBox_15", "外设状态：期望值 +「启用」；读回与期望值比对。"),
    TIP("groupBox_TestRequirements", "治具状态期望值（FIXTEST/*）：音乐、过压灯、按键等。"),
    TIP("groupBox_camera_position", "摄像头 ROI、脏污检测延时等。"),
    TIP("imuCalibrationWidget", "IMU 校准时间、轴限、静态收敛参数（IMU/*）。"),
    TIP("signalStrengthSettingWidget", "WiFi/BLE RSSI 卡控与测试次数（WIFI/*、BLE/*）。"),
    TIP("pressureSettingsWidget", "压感测试/校准、治具套号、阈值（PRESSURE/*）。"),
    TIP("mainWidget", "基础信息期望值；勾选后参与 compareVersions 比对。"),
    TIP("groupBox_1", "产品差异化流程开关（SYSTEM/*），按产品可 Restore 默认。"),
    TIP("groupBox_SubPIDSettings", "SUBPID 规则（SUBPID/*）。"),
    TIP("groupBox_ageingConfig", "老化 AGING/BurningMode、BurningSeconds。"),
    TIP("groupBox_tupleConfig", "三元组 Tuple/*：环境、URL、鉴权、SKU。"),
    TIP("lineEdit_factoryCloudStationKey", "FactoryCloud/StationKey；留空则使用设置页工站类型（SYSTEM/station，如 FREE_WORK）。"),
    TIP("checkBox_factoryCloudTestDataUpload", "过站 PASS 后自动 JSON 上报分项测试数据至云端。"),
    TIP("checkBox_factoryCloudOfflineBypass",
        "服务器不可用时允许用离线账号进入上位机做本地测试；也可直接在 上位机设置.ini 写 "
        "FactoryCloud/OfflineBypassEnabled=true。"),
    TIP("lineEdit_factoryCloudOfflineUser", "离线测试账号（FactoryCloud/OfflineBypassUser），默认 offline。"),
    TIP("lineEdit_factoryCloudOfflinePassword", "离线测试密码（FactoryCloud/OfflineBypassPassword），默认 offline26。"),
    TIP("pushButton_factoryCloudUploadLogs", "打包 exe 目录下「所有log」并上传到云平台日志接口。"),
    TIP("pushButton_uploadTestCase", "打包本地 test_case（不含 .backup）上传云端并发布；需 engineer/admin 账号。"),
    TIP("pushButton_syncTestCase", "从云端下载最新 test_case bundle 覆盖本地（先比对 bundle 版本）。"),
    TIP("groupBox_keyConfig", "按键 KeyId（ProductInfo/KeyId*）。"),
    TIP("groupBox_plcModbusDebug", "PLC Modbus（PLC/*），治具 PLC 步骤。"),
    TIP("groupBox_brushInstrument",
        "BrushInstrument/* 与并联 CMW：BleBrushCmwConcurrent、BleBrushCmwOnStopPer、BlePer/Cmw*（含 "
        "CmwQueryCurrentArbFile 查询 SOURce:GPRF:GEN:ARB:FILE?；另见 CmwBurstPollArbScount…、CmwVisaTrace、"
        "CmwWaitArbScount）。"),
    TIP("groupBox_systemProtocol", "SYSTEM/ProtocolType：qpb、qfctp 或 qaiot。"),
    TIP("config", "已选步骤，自上而下执行；可拖拽排序。"),
    TIP("can_use", "可选步骤，勾选后拖入左侧配置区。"),
    TIP("freeWorkOptionalTabs", "分类：产品 / 治具 / dongle / 云端。"),
    TIP("configScrollArea", "配置区列表，内容多时可滚动。"),
    TIP("optionalScroll_product", "产品功能类可选步骤。"),
    TIP("optionalScroll_dongle", "dongle/蓝牙连接类可选步骤。"),
    TIP("optionalScroll_fixture", "治具/PLC/串口仪器类可选步骤。"),
    TIP("optionalScroll_cloud", "三元组云端类可选步骤。"),
    TIP("comboBox_factory", "MES 工厂代码（lx 立讯精密/hq 华勤技术/wks 伟克森/ydm 亚达明/hz 华庄等）；选 hz 显示华庄专用项。"),
    TIP("checkBox_mesRetest", "华庄 Mes/RETEST：站前检查 checkRoute 的 retest 参数。"),
    TIP("comboBox_productName", "产品型号 MES/Product_Name；切换恢复默认差异化。"),
    TIP("lineEdit_station", "MES 工站。"),
    TIP("pushButton_mesConfigFileBrowse", "仅 BYD：浏览外部 mes_config.ini（MES/ConfigFilePath）。"),
    TIP("label_mes_config_file_path", "仅 BYD：外部 MES 配置文件路径。"),
    TIP("comboBox_testFlowStation", "TestOrderMeta/SelectedStation；切换工站编排 test_case 流程。"),
    TIP("pushButton_testFlowClear", "清空当前工站已配置测试流程。"),
    TIP("comboBox_systemProtocolType", "设备协议 qpb / qfctp / qaiot / qroot（吸奶器 PCBA，经蓝牙透传）。"),
    TIP("comboBox_tupleEnvironment", "三元组环境预设。"),
    TIP("lineEdit_tupleBaseUrl", "Tuple/BaseUrl。"),
    TIP("lineEdit_imu_status", "外设 imu 状态期望值。"),
    TIP("checkBox_imu_status", "启用 imu 状态比对。"),
    TIP("lineEdit_ageingBurningSeconds", "AGING/BurningSeconds（秒）。"),
    TIP("checkBox_freeInstrumentBleBrushCmwConcurrent",
        "FreeInstrument/BleBrushCmwConcurrent：启用 CMW GPRF；“停止接收/PER…”项见另一勾选（"
        "BleBrushCmwOnStopPer）。需 BlePer/CmwVisaAddress；可选 BlePer/CmwVisaTrace；BlePer/"
        "CmwQueryCurrentArbFile（默认 true，查询 SOURce:GPRF:GEN:ARB:FILE? 打日志）；BlePer/CmwBurstPollArbScount、"
        "BlePer/CmwWaitArbScount；详见 docs/cmw100rx第四节。"),
    TIP("checkBox_freeInstrumentBleBrushCmwOnStopPer",
        "FreeInstrument/BleBrushCmwOnStopPer（默认勾选）：勾选时 PER 步内按上个「开始接收」频点再打一发；若前序已执行六个"
        "「并联CMW播放*MHz」步，可取消勾选以免 PER 重复打射频。"),
    TIP("lineEdit_plcSwitchDoneResetM",
        "PLC/SwitchTestDoneResetM：旋钮测试完成复位线圈 M 默认值（211）；双工位时工站2可单独设 ini 键 "
        "SwitchTestDoneResetM_Station2（如231），工位N 用 SwitchTestDoneResetM_StationN，有则优先于本项。"),
    TIP("lineEdit_plcSwitchDoneResetPulseMs",
        "PLC/SwitchTestDoneResetPulseMs：复位脉冲毫秒；>0 时置1后经此时长再置0，0 表示仅置1由 PLC 清零。"),
};

#undef LE
#undef CB
#undef P
#undef CI
#undef CT
#undef TIP

QString key1(const Row& r) {
    return QString::fromUtf8(r.k1);
}
QString key2(const Row& r) {
    return QString::fromUtf8(r.k2);
}
bool defBool(const char* s) {
    return s && (qstrcmp(s, "true") == 0 || qstrcmp(s, "1") == 0);
}

void applyRowTooltip(QWidget* root, const Row& r) {
    const auto tip = [&](const char* key, const QString& suffix) {
        if (QWidget* w = root->findChild<QWidget*>(r.w1))
            w->setToolTip(QString::fromUtf8(key) + suffix);
    };
    switch (r.kind) {
    case K::Tip:
        if (QWidget* w = root->findChild<QWidget*>(r.w1))
            w->setToolTip(QString::fromUtf8(r.k1));
        break;
    case K::Le:
    case K::Ci:
    case K::Ct:
    case K::Cb:
        tip(r.k1, QStringLiteral("。"));
        break;
    case K::Pair:
        if (QWidget* w = root->findChild<QWidget*>(r.w1))
            w->setToolTip(QString::fromUtf8(r.k1) + QStringLiteral(" 期望值。"));
        if (QWidget* w = root->findChild<QWidget*>(r.w2))
            w->setToolTip(QString::fromUtf8(r.k2) + QStringLiteral("。"));
        break;
    }
}

void loadRow(QWidget* root, const Row& r) {
    switch (r.kind) {
    case K::Tip:
        break;
    case K::Le:
        if (auto* le = root->findChild<QLineEdit*>(r.w1)) {
            const QString k = key1(r);
            le->setText(r.d1 ? SETTINGS.value(k, QString::fromUtf8(r.d1)).toString() : SETTINGS.value(k).toString());
        }
        break;
    case K::Cb:
        if (auto* cb = root->findChild<QCheckBox*>(r.w1)) {
            const QString k = key1(r);
            cb->setChecked(r.d1 ? SETTINGS.value(k, defBool(r.d1)).toBool() : SETTINGS.value(k).toBool());
        }
        break;
    case K::Pair:
        if (auto* le = root->findChild<QLineEdit*>(r.w1)) {
            const QString vk = key1(r);
            le->setText(r.d1 ? SETTINGS.value(vk, QString::fromUtf8(r.d1)).toString() : SETTINGS.value(vk).toString());
        }
        if (auto* cb = root->findChild<QCheckBox*>(r.w2)) {
            const QString ek = key2(r);
            cb->setChecked(r.d2 ? SETTINGS.value(ek, defBool(r.d2)).toBool() : SETTINGS.value(ek).toBool());
        }
        break;
    case K::Ci:
        if (auto* c = root->findChild<QComboBox*>(r.w1)) {
            const QString k = key1(r);
            c->setCurrentIndex(r.d1 ? SETTINGS.value(k, QString::fromUtf8(r.d1).toInt()).toInt()
                                    : SETTINGS.value(k).toInt());
        }
        break;
    case K::Ct:
        if (auto* c = root->findChild<QComboBox*>(r.w1))
            c->setCurrentText(SETTINGS.value(key1(r)).toString());
        break;
    }
}

void saveRow(QWidget* root, const Row& r) {
    switch (r.kind) {
    case K::Tip:
        break;
    case K::Le:
        if (auto* le = root->findChild<QLineEdit*>(r.w1))
            SETTINGS.setValue(key1(r), le->text());
        break;
    case K::Cb:
        if (auto* cb = root->findChild<QCheckBox*>(r.w1))
            SETTINGS.setValue(key1(r), cb->isChecked());
        break;
    case K::Pair:
        if (auto* le = root->findChild<QLineEdit*>(r.w1))
            SETTINGS.setValue(key1(r), le->text());
        if (auto* cb = root->findChild<QCheckBox*>(r.w2))
            SETTINGS.setValue(key2(r), cb->isChecked());
        break;
    case K::Ci:
        if (auto* c = root->findChild<QComboBox*>(r.w1))
            SETTINGS.setValue(key1(r), c->currentIndex());
        break;
    case K::Ct:
        if (auto* c = root->findChild<QComboBox*>(r.w1))
            SETTINGS.setValue(key1(r), c->currentText());
        break;
    }
}

} // namespace

void applyQSettingTableBindingTooltips(QWidget* root) {
    if (!root) {
        return;
    }
    for (const Row& r : kSettings) {
        applyRowTooltip(root, r);
    }
}

void loadQSettingTableBindings(QWidget* root) {
    if (!root) {
        return;
    }
    for (const Row& r : kSettings) {
        loadRow(root, r);
    }
}

void saveQSettingTableBindings(QWidget* root) {
    if (!root) {
        return;
    }
    for (const Row& r : kSettings) {
        saveRow(root, r);
    }
}
