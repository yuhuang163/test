// =============================================================================
// 设置页 ↔ 上位机设置.ini 绑定表（由 qsetting.cpp include，勿单独编译）
// =============================================================================
// 新增项：在 settingPageBinds() 里找到对应 [分区号] 注释块，按行格式追加一行即可。
// 特殊项（工站单选、PLC 第二工位、三元组 Environment、KeyCap 大小端）见文件末尾「非表驱动」。
//
// 目录（Ctrl+F 搜分区号）：
//   [01] SYSTEM 开关    [02] SYSTEM 杂项   [03] WIFI          [04] BLE
//   [05] 老化           [06] 刷机仪器      [07] 自由工站仪器  [08] PLC 主站
//   [09] 三元组 URL 等  [10] 分窗行列      [11] 治具 FIXTEST  [12] 压感
//   [13] IMU            [14] 基础信息/KeyId [15] KeyCap 数值   [16] 电流
//   [17] 外设状态       [18] 电池           [19] 摄像头        [20] MES
// =============================================================================

// 行宏：统一列含义 →  INI键 | ui控件 | 默认值 | (可选)类型/trim/数值格式
#define SB_LINE(k, w, d) SettingBind{QStringLiteral(k), w, d}
#define SB_CHK(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::CheckBox}
#define SB_CMB_TXT(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::ComboText}
#define SB_CMB_IDX(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::ComboIndex}
#define SB_TRIM(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::LineEdit, true}
#define SB_INT(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::LineEdit, false, LineEditStore::AsInt}
#define SB_INT_T(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::LineEdit, true, LineEditStore::AsInt}
#define SB_D4_T(k, w, d) SettingBind{QStringLiteral(k), w, d, BindKind::LineEdit, true, LineEditStore::AsDouble4}

// -----------------------------------------------------------------------------
// 主表：loadBoundSettings / saveBoundSettings 只遍历本函数返回的列表
// -----------------------------------------------------------------------------
QVector<SettingBind> settingPageBinds(Ui::qsetting* ui) {
    QVector<SettingBind> b;
    b.reserve(220);

    // ----- [01] SYSTEM 流程开关（tab 产品差异化） -----
    appendSettingBinds(b, {
        SB_CHK("SYSTEM/ShowLocalOTAFunc", ui->checkBox_ShowLocalOTAFunc, false),
        SB_CHK("SYSTEM/ShowUpperComputerOTAFunc", ui->checkBox_ShowUpperComputerOTAFunc, false),
        SB_CHK("SYSTEM/SaveToothbrushLog", ui->checkBox_SaveToothbrushLog, false),
        SB_CHK("SYSTEM/LockProductUI", ui->checkBox_LockProductUI, false),
        SB_CHK("SYSTEM/SimplePcbaTest", ui->checkBox_SimplePcbaTest, false),
        SB_CHK("SYSTEM/NeedWriteSubpid", ui->checkBox_NeedWriteSubpid, false),
        SB_CHK("SYSTEM/NeedWriteSkuid", ui->checkBox_NeedWriteSkuid, false),
        SB_CHK("SYSTEM/BluetoothImageTransfer", ui->checkBox_BluetoothImageTransfer, false),
        SB_CHK("SYSTEM/IMUCalibrationWakeup", ui->checkBox_IMUCalibrationWakeup, false),
        SB_CHK("SYSTEM/DisableSerialPortRx", ui->checkBox_DisableSerialPortRx, false),
        SB_CHK("SYSTEM/ShipModeResponse", ui->checkBox_ShipModeResponse, false),
        SB_CHK("SYSTEM/SerialPortMAC", ui->checkBox_SerialPortMAC, false),
        SB_CHK("SYSTEM/MagneticReuseMotorStatus", ui->checkBox_MagneticReuseMotorStatus, false),
        SB_CHK("SYSTEM/TestAudioCurrent", ui->checkBox_TestAudioCurrent, false),
        SB_CHK("SYSTEM/TestShippingCurrent", ui->checkBox_TestShippingCurrent, false),
        SB_CHK("SYSTEM/PressIndependent", ui->checkBox_PressIndependent, false),
        SB_CHK("SYSTEM/PressWindow", ui->checkBox_PressWindow, false),
        SB_CHK("SYSTEM/SendMotorCalibration", ui->checkBox_SendMotorCalibration, false),
        SB_CHK("SYSTEM/LightTest", ui->checkBox_LightTest, false),
        SB_CHK("SYSTEM/uperMotor", ui->checkBox_uperMotor, false),
        SB_CHK("SYSTEM/ServoMotorStart", ui->checkBox_ServoMotorStart, false),
        SB_CHK("SYSTEM/TestWifiSignal", ui->checkBox_TestWifiSignal, false),
        SB_CHK("SYSTEM/IMULastEnterStartTest", ui->checkBox_IMULastEnterStartTest, false),
    });

    // ----- [02] SYSTEM 杂项 / SN / 音乐状态 -----
    appendSettingBinds(b, {
        SB_LINE("SYSTEM/CurrentMechine", ui->lineEdit_CurrentMechine, QString()),
        SB_LINE("Regex/SNPattern", ui->snLineEdit, QString()),
        SB_LINE("Music/MusicState", ui->lineEdit_music_state, QString()),
        SB_CHK("Music/MusicState_checkBox", ui->checkBox_MusicState, false),
    });

    // ----- [03] WIFI -----
    appendSettingBinds(b, {
        SB_LINE("WIFI/Name", ui->wifiAccountLineEdit, QString()),
        SB_LINE("WIFI/Password", ui->wifiPasswordLineEdit, QString()),
        SB_LINE("WIFI/HighRssi", ui->wifiUpperLimitLineEdit, QString()),
        SB_LINE("WIFI/LowRssi", ui->wifiLowerLimitLineEdit, QString()),
        SB_LINE("WIFI/IP", ui->wifiIPLineEdit, QString()),
    });

    // ----- [04] BLE / 信号测试次数 -----
    appendSettingBinds(b, {
        SB_LINE("BLE/HighRssi", ui->bluetoothUpperLimitLineEdit, QString()),
        SB_LINE("BLE/LowRssi", ui->bluetoothLowerLimitLineEdit, QString()),
        SB_LINE("BLE/RssiCount", ui->signalTestCountLineEdit, QString()),
    });

    // ----- [05] 老化 AGING -----
    appendSettingBinds(b, {
        SB_INT("AGING/BurningMode", ui->lineEdit_ageingBurningMode, 1),
        SB_INT("AGING/BurningSeconds", ui->lineEdit_ageingBurningSeconds, 60 * 60 * 4),
    });

    // ----- [06] 刷机 / CMW 仪器 BrushInstrument -----
    appendSettingBinds(b, {
        SB_INT_T("BrushInstrument/InstrumentSendPacketCount", ui->lineEdit_brushInstrumentSendPacketCount, 1000),
        SB_D4_T("BrushInstrument/MaxPer", ui->lineEdit_brushInstrumentMaxPer, 0.05),
        SB_INT_T("BrushInstrument/PacketPhaseWaitMs", ui->lineEdit_brushInstrumentPacketPhaseWaitMs, 2000),
        SB_INT_T("BrushInstrument/StopAckTimeoutMs", ui->lineEdit_brushInstrumentStopAckTimeoutMs, 5000),
    });

    // ----- [07] 自由工站仪器 FreeInstrument -----
    appendSettingBinds(b, {
        SB_CHK("FreeInstrument/BleBrushCmwConcurrent", ui->checkBox_freeInstrumentBleBrushCmwConcurrent, false),
        SB_CHK("FreeInstrument/BleBrushCmwOnStopPer", ui->checkBox_freeInstrumentBleBrushCmwOnStopPer, true),
    });

    // ----- [08] PLC Modbus 主站（第二工位见文件末尾 loadPlcStation2） -----
    appendSettingBinds(b, {
        SB_CHK("PLC/ModbusTrace", ui->checkBox_plcModbusTrace, false),
        SB_TRIM("PLC/IpAddress", ui->lineEdit_plcIpAddress, QStringLiteral("192.168.1.88")),
        SB_INT_T("PLC/Port", ui->lineEdit_plcPort, 502),
        SB_INT_T("PLC/UnitId", ui->lineEdit_plcUnitId, 1),
        SB_INT_T("PLC/MCoilAddressOffset", ui->lineEdit_plcMCoilOffset, 0),
        SB_INT_T("PLC/MBase", ui->lineEdit_plcMBase, 200),
        SB_INT_T("PLC/MBaseStationStep", ui->lineEdit_plcMBaseStationStep, 20),
        SB_INT_T("PLC/ConnectTimeoutMs", ui->lineEdit_plcConnectTimeoutMs, 3000),
        SB_INT_T("PLC/RequestTimeoutMs", ui->lineEdit_plcRequestTimeoutMs, 2000),
        SB_INT_T("PLC/CommandGapMs", ui->lineEdit_plcCommandGapMs, 80),
        SB_INT_T("PLC/SwitchTestDoneResetM", ui->lineEdit_plcSwitchDoneResetM, 211),
        SB_INT_T("PLC/SwitchTestDoneResetPulseMs", ui->lineEdit_plcSwitchDoneResetPulseMs, 0),
        SB_CHK("PLC/ConnectVerifyRead", ui->checkBox_plcConnectVerifyRead, true),
    });

    // ----- [09] 三元组 Tuple（Environment 见 loadTupleEnvironment） -----
    appendSettingBinds(b, {
        SB_LINE("Tuple/BaseUrl", ui->lineEdit_tupleBaseUrl, kTupleEnvPresets.first().baseUrl),
        SB_LINE("Tuple/AuthUser", ui->lineEdit_tupleAuthUser, QString()),
        SB_LINE("Tuple/AuthPassword", ui->lineEdit_tupleAuthPassword, QString()),
        SB_LINE("Tuple/Sku", ui->lineEdit_tupleSku, QString()),
        SB_LINE("Tuple/Position", ui->lineEdit_tuplePosition, QStringLiteral("L")),
    });

    // ----- [10] 主界面分窗 User -----
    appendSettingBinds(b, {
        SB_LINE("User/formRow", ui->rowLineEdit, QString()),
        SB_LINE("User/formColumn", ui->columnLineEdit, QString()),
    });

    // ----- [11] 治具状态 FIXTEST -----
    appendSettingBinds(b, {
        SB_LINE("FIXTEST/MusicState", ui->lineEdit_MusicStatus, QString()),
        SB_LINE("FIXTEST/OverVoltageLight", ui->lineEdit_OvervoltageLightStatus, QString()),
        SB_LINE("FIXTEST/Button1", ui->lineEdit_Button1Status, QString()),
        SB_LINE("FIXTEST/Button2", ui->lineEdit_Button2Status, QString()),
    });

    // ----- [12] 压感 PRESSURE / Press -----
    appendSettingBinds(b, {
        SB_LINE("PRESSURE/CaliTime", ui->lineEdit_caliWaitTime, QString()),
        SB_LINE("PRESSURE/TestTime", ui->lineEdit_testWaitTime, QString()),
        SB_CMB_IDX("PRESSURE/functionSwitch", ui->comboBox_pressFunctionSwitch, 0),
        SB_CMB_IDX("PRESSURE/Use_graph", ui->comboBox_displayImageType, 0),
        SB_CMB_IDX("PRESSURE/Module", ui->comboBox_individualMode, 0),
        SB_LINE("PRESSURE/PressMechine", ui->lineEdit_PressMechine, QString()),
        SB_LINE("PRESSURE/ButtonThreshold", ui->lineEdit_ButtonThreshold, QString()),
        SB_LINE("PRESSURE/ButtonThresholdCount", ui->lineEdit_ButtonThresholdCount, QString()),
        SB_LINE("PRESSURE/BrushThreshold", ui->lineEdit_BrushThreshold, QString()),
        SB_LINE("PRESSURE/BrushThresholdCount", ui->lineEdit_BrushThresholdCount, QString()),
        SB_LINE("PRESSURE/ADCShakeValue", ui->lineEdit_press_adc_shake, QString()),
        SB_CHK("Press/AmplitudeLimit", ui->checkBox_amplitudeLimit, false),
        SB_LINE("Press/AmplitudeLimitUpper", ui->lineEdit_amplitudeLimitUpper, QStringLiteral("0")),
        SB_LINE("Press/AmplitudeLimitLower", ui->lineEdit_amplitudeLimitLower, QStringLiteral("0")),
        SB_LINE("Pressure/AmplitudeError", ui->lineEdit_amplitudeError, QStringLiteral("0")),
        SB_LINE("PRESSURE/bth_upper", ui->bthPressUpperLimitLineEdit, QString()),
        SB_LINE("PRESSURE/bth_lower", ui->bthPressLowerLimitLineEdit, QString()),
        SB_LINE("PRESSURE/model_button_upper", ui->modelPressUpperLimitLineEdit, QString()),
        SB_LINE("PRESSURE/model_button_lower", ui->modelPressLowerLimitLineEdit, QString()),
        SB_LINE("PRESSURE/power_button_upper", ui->powerPressUpperLimitLineEdit, QString()),
        SB_LINE("PRESSURE/power_button_lower", ui->powerPressLowerLimitLineEdit, QString()),
    });

    // ----- [13] IMU 校准 / 静态收敛 -----
    appendSettingBinds(b, {
        SB_LINE("IMU/IMU_Wait_Time", ui->lineEdit_calibrationTime, QString()),
        SB_LINE("IMU/ImuCompareData", ui->lineEdit_comparisonValue, QString()),
        SB_LINE("IMU/imu_cali_wait_time", ui->lineEdit_singlePointTimeout, QString()),
        SB_LINE("IMU/acc_z_up", ui->lineEdit_zAxisUpper, QString()),
        SB_LINE("IMU/acc_z_down", ui->lineEdit_zAxisLower, QString()),
        SB_LINE("IMU/acc_x_up", ui->lineEdit_xAxisUpper, QString()),
        SB_LINE("IMU/acc_x_down", ui->lineEdit_xAxisLower, QString()),
        SB_LINE("IMU/acc_y_up", ui->lineEdit_yAxisUpper, QString()),
        SB_LINE("IMU/acc_y_down", ui->lineEdit_yAxisLower, QString()),
        SB_LINE("IMU/STATIC_CONV_VAR", ui->lineEdit_threshold, QString()),
        SB_LINE("IMU/STATIC_CONV_DELAY", ui->lineEdit_delayFrames, QString()),
        SB_LINE("IMU/STATIC_CONV_COUNT", ui->lineEdit_sampleFrames, QString()),
    });

    // ----- [14] 基础信息 ProductInfo / KeyId / MAC_SN -----
    appendSettingBinds(b, {
        SB_LINE("ProductInfo/Product_Name", ui->lineEdit_ProductName, QString()),
        SB_LINE("ProductInfo/Hardware_Version", ui->lineEdit_HardwareVersion, QString()),
        SB_LINE("ProductInfo/Software_Version", ui->lineEdit_SoftwareVersion, QString()),
        SB_LINE("ProductInfo/Resource_Version", ui->lineEdit_ResourceVersion, QString()),
        SB_LINE("ProductInfo/Age_State", ui->lineEdit_AgingStatus, QString()),
        SB_LINE("ProductInfo/Motor_Ver", ui->lineEdit_MotorVersion, QString()),
        SB_LINE("ProductInfo/Ble_Ver", ui->lineEdit_BluetoothVersion, QString()),
        SB_LINE("ProductInfo/App_Protocol_Version", ui->lineEdit_AppPB, QString()),
        SB_LINE("ProductInfo/Factory_Protocol_Version", ui->lineEdit_FactoryPB, QString()),
        SB_LINE("ProductInfo/Algorithm_Version", ui->lineEdit_AlgorithmVersion, QString()),
        SB_LINE("ProductInfo/Pressure_Sense_Version", ui->lineEdit_PressureVersion, QString()),
        SB_LINE("ProductInfo/FSensor_Version", ui->lineEdit_FSensorVersion, QString()),
        SB_LINE("ProductInfo/IMU_ID", ui->lineEdit_ImuID, QString()),
        SB_LINE("ProductInfo/Camera_Id", ui->lineEdit_CameraID, QString()),
        SB_LINE("MAC_SN/FilePath", ui->lineEdit_mac_sn_path,
                QStringLiteral("\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt")),
        SB_CHK("ProductInfo/ProductName_checkBox", ui->checkBox_ProductName, false),
        SB_CHK("ProductInfo/HardwareVersion_checkBox", ui->checkBox_HardwareVersion, false),
        SB_CHK("ProductInfo/SoftwareVersion_checkBox", ui->checkBox_SoftwareVersion, false),
        SB_CHK("ProductInfo/ResourceVersion_checkBox", ui->checkBox_ResourceVersion, false),
        SB_CHK("ProductInfo/AgingStatus_checkBox", ui->checkBox_AgingStatus, false),
        SB_CHK("ProductInfo/MotorVersion_checkBox", ui->checkBox_MotorVersion, false),
        SB_CHK("ProductInfo/BluetoothVersion_checkBox", ui->checkBox_BluetoothVersion, false),
        SB_CHK("ProductInfo/AppPB_checkBox", ui->checkBox_AppPB, false),
        SB_CHK("ProductInfo/FactoryPB_checkBox", ui->checkBox_FactoryPB, false),
        SB_CHK("ProductInfo/AlgorithmVersion_checkBox", ui->checkBox_AlgorithmVersion, false),
        SB_CHK("ProductInfo/PressureVersion_checkBox", ui->checkBox_PressureVersion, false),
        SB_CHK("ProductInfo/FSensorVersion_checkBox", ui->checkBox_FSensorVersion, false),
        SB_CHK("ProductInfo/ImuID_checkBox", ui->checkBox_ImuID, false),
        SB_CHK("ProductInfo/CameraID_checkBox", ui->checkBox_CameraID, false),
        SB_CHK("ProductInfo/KeyIdPower_checkBox", ui->checkBox_KeyIdPower, true),
        SB_LINE("ProductInfo/KeyIdPower", ui->lineEdit_KeyIdPower, QStringLiteral("1")),
        SB_CHK("ProductInfo/KeyIdStartPause_checkBox", ui->checkBox_KeyIdStartPause, true),
        SB_LINE("ProductInfo/KeyIdStartPause", ui->lineEdit_KeyIdStartPause, QStringLiteral("2")),
        SB_CHK("ProductInfo/KeyIdMode_checkBox", ui->checkBox_KeyIdMode, true),
        SB_LINE("ProductInfo/KeyIdMode", ui->lineEdit_KeyIdMode, QStringLiteral("3")),
        SB_CHK("ProductInfo/KeyIdSpeed_checkBox", ui->checkBox_KeyIdSpeed, true),
        SB_LINE("ProductInfo/KeyIdSpeed", ui->lineEdit_KeyIdSpeed, QStringLiteral("4")),
        SB_CHK("ProductInfo/KeyIdProgram_checkBox", ui->checkBox_KeyIdProgram, true),
        SB_LINE("ProductInfo/KeyIdProgram", ui->lineEdit_KeyIdProgram, QStringLiteral("5")),
        SB_CHK("ProductInfo/KeyIdLeft_checkBox", ui->checkBox_KeyIdLeft, true),
        SB_LINE("ProductInfo/KeyIdLeft", ui->lineEdit_KeyIdLeft, QStringLiteral("6")),
        SB_CHK("ProductInfo/KeyIdRight_checkBox", ui->checkBox_KeyIdRight, true),
        SB_LINE("ProductInfo/KeyIdRight", ui->lineEdit_KeyIdRight, QStringLiteral("7")),
        SB_CHK("ProductInfo/KeyIdLeftRotate_checkBox", ui->checkBox_KeyIdLeftRotate, true),
        SB_LINE("ProductInfo/KeyIdLeftRotate", ui->lineEdit_KeyIdLeftRotate, QStringLiteral("10")),
        SB_CHK("ProductInfo/KeyIdRightRotate_checkBox", ui->checkBox_KeyIdRightRotate, true),
        SB_LINE("ProductInfo/KeyIdRightRotate", ui->lineEdit_KeyIdRightRotate, QStringLiteral("11")),
    });

    // ----- [15] 按键 KeyCap 数值（ValueEndian 见 loadKeyCapEndian） -----
    appendSettingBinds(b, {
        SB_INT("KeyCap/Low", ui->lineEdit_KeyCapLow, 1),
        SB_INT("KeyCap/High", ui->lineEdit_KeyCapHigh, 65535),
        SB_INT("KeyCap/ReadTimeoutMs", ui->lineEdit_KeyCapReadTimeoutMs, 5000),
        SB_INT("KeyCap/ReadCount", ui->lineEdit_KeyCapReadCount, 3),
        SB_INT("KeyCap/ReadIntervalMs", ui->lineEdit_KeyCapReadIntervalMs, 80),
        SB_INT("KeyCap/SingleReadTimeoutMs", ui->lineEdit_KeyCapSingleReadTimeoutMs, 2000),
    });

    // ----- [16] 电流 Current -----
    appendSettingBinds(b, {
        SB_LINE("Current/HighshipCurrent", ui->lineEdit_CargoCurrentUpper, QString()),
        SB_LINE("Current/LowshipCurrent", ui->lineEdit_CargoCurrentLower, QString()),
        SB_LINE("Current/HighCharCurrent", ui->lineEdit_ChargingCurrentUpper, QString()),
        SB_LINE("Current/LowCharCurrent", ui->lineEdit_ChargingCurrentLower, QString()),
        SB_LINE("Current/HighworkCurrent", ui->lineEdit_WorkingCurrentUpper, QString()),
        SB_LINE("Current/LowworkCurrent", ui->lineEdit_WorkingCurrentLower, QString()),
        SB_LINE("Current/HighstaticCurrent", ui->lineEdit_StaticCurrentUpper, QString()),
        SB_LINE("Current/LowstaticCurrent", ui->lineEdit_StaticCurrentLower, QString()),
        SB_LINE("Current/HighmusicCurrent", ui->lineEdit_AudioCurrentUpper, QString()),
        SB_LINE("Current/LowmusicCurrent", ui->lineEdit_AudioCurrentLower, QString()),
        SB_LINE("Current/measure_wait_time", ui->lineEdit_TestDuration, QString()),
    });

    // ----- [17] 外设状态 PeripheralStatus / FreeWorkPeripheral -----
    appendSettingBinds(b, {
        SB_LINE("PeripheralStatus/IMU_Status", ui->lineEdit_imu_status, QString()),
        SB_LINE("PeripheralStatus/Flash_Status", ui->lineEdit_flash_status, QString()),
        SB_LINE("PeripheralStatus/Magnetic_Status", ui->lineEdit_magnetic_status, QString()),
        SB_LINE("PeripheralStatus/Pressure_Status", ui->lineEdit_pressure_status, QString()),
        SB_LINE("PeripheralStatus/Audio_Status", ui->lineEdit_audio_status, QString()),
        SB_CHK("PeripheralStatus/IMUStatus_checkBox", ui->checkBox_imu_status, false),
        SB_CHK("PeripheralStatus/FlashStatus_checkBox", ui->checkBox_flash_status, false),
        SB_CHK("PeripheralStatus/MagneticStatus_checkBox", ui->checkBox_magnetic_status, false),
        SB_CHK("PeripheralStatus/PressureStatus_checkBox", ui->checkBox_pressure_status, false),
        SB_CHK("PeripheralStatus/AudioStatus_checkBox", ui->checkBox_audio_status, false),
        SB_LINE("PeripheralStatus/Press0_Status", ui->lineEdit_freework_press0_status, QString()),
        SB_LINE("PeripheralStatus/Press1_Status", ui->lineEdit_freework_press1_status, QString()),
        SB_LINE("PeripheralStatus/BatteryIc_Status", ui->lineEdit_freework_battery_ic_status, QString()),
        SB_LINE("PeripheralStatus/TouchIc_Status", ui->lineEdit_freework_touch_ic_status, QString()),
        SB_LINE("PeripheralStatus/LedIc_Status", ui->lineEdit_freework_led_ic_status, QString()),
        SB_LINE("PeripheralStatus/PdIc_Status", ui->lineEdit_freework_pd_ic_status, QString()),
        SB_CHK("FreeWorkPeripheral/Press0_checkBox", ui->checkBox_freework_press0, true),
        SB_CHK("FreeWorkPeripheral/Press1_checkBox", ui->checkBox_freework_press1, true),
        SB_CHK("FreeWorkPeripheral/BatteryIC_checkBox", ui->checkBox_freework_battery_ic, true),
        SB_CHK("FreeWorkPeripheral/TouchIC_checkBox", ui->checkBox_freework_touch_ic, true),
        SB_CHK("FreeWorkPeripheral/LedIC_checkBox", ui->checkBox_freework_led_ic, true),
        SB_CHK("FreeWorkPeripheral/PdIC_checkBox", ui->checkBox_freework_pd_ic, true),
    });

    // ----- [18] 电池 BATTARY -----
    appendSettingBinds(b, {
        SB_LINE("BATTARY/standbattary", ui->lineEdit_battery_voltage_control, QString()),
    });

    // ----- [19] 摄像头 CAMERA -----
    appendSettingBinds(b, {
        SB_LINE("CAMERA/Rect1_X", ui->lineEdit_camera_x, QString()),
        SB_LINE("CAMERA/Rect1_Y", ui->lineEdit_camera_y, QString()),
        SB_LINE("CAMERA/Rect1_Width", ui->lineEdit_camera_width, QString()),
        SB_LINE("CAMERA/Rect1_Height", ui->lineEdit_camera_height, QString()),
        SB_LINE("CAMERA/CameraGetTime", ui->lineEdit_image_interval, QString()),
        SB_LINE("CAMERA/startDirtyTime", ui->lineEdit_start_dirty_time, QString()),
    });

    // ----- [20] MES -----
    appendSettingBinds(b, {
        SB_CMB_TXT("MES/factory", ui->comboBox_factory, QString()),
        SB_CMB_TXT("MES/Product_Name", ui->comboBox_productName, QString()),
        SB_LINE("MES/xwdWpCode", ui->lineEdit_macStation, QString()),
        SB_LINE("MES/machineNo", ui->lineEdit_station, QString()),
        SB_LINE("MES/NET", ui->lineEdit_mesUrl, QString()),
        SB_LINE("MES/ConfigFilePath", ui->lineEdit_mes_config_file_path, QString()),
        SB_LINE("MES/test_station", ui->lineEdit_processName, QString()),
        SB_LINE("MES/model", ui->lineEdit_modelName, QString()),
        SB_LINE("MES/FIELD", ui->lineEdit_mac_field, QString()),
        SB_LINE("MES/mUserno", ui->lineEdit_mes_operator, QString()),
        SB_LINE("MES/Work_Order", ui->lineEdit_weikesen_order, QString()),
        SB_LINE("MES/M_USERNO", ui->lineEdit_mes_login_account, QString()),
        SB_LINE("MES/Action", ui->lineEdit_action_huaqin, QString()),
        SB_LINE("MES/M_MACHINENO", ui->lineEdit_mes_login_station, QString()),
        SB_LINE("MES/Line", ui->lineEdit_line_huaqin, QString()),
        SB_LINE("MES/M_PASSWORD", ui->lineEdit_mes_login_password, QString()),
    });

    return b;
}

// =============================================================================
// 非表驱动：无法写成「键 + 单控件」的项（loadBoundSettings / saveBoundSettings 额外调用）
// =============================================================================

void loadSystemStation(Ui::qsetting* ui) {
    const QString station = SETTINGS.value(QStringLiteral("SYSTEM/station")).toString();
    const QMap<QString, QRadioButton*> stationMap = {
        {QStringLiteral("IMU_CALI"), ui->radioButtonImuCalibration},
        {QStringLiteral("MOTOR_TEST"), ui->radioButtonMotorCalibration},
        {QStringLiteral("QUIESCENT_CURRENT"), ui->radioButtonStaticCurrent},
        {QStringLiteral("SCREEN_TEST"), ui->radioButtonScreenTest},
        {QStringLiteral("CAMERA_TEST"), ui->radioButtonCameraTest},
        {QStringLiteral("WIFIBLE_TEST"), ui->radioButtonSignalTest},
        {QStringLiteral("AGE_TEST"), ui->radioButtonAgingTest},
        {QStringLiteral("PCBA_TEST"), ui->radioButtonBoardFactoryTest},
        {QStringLiteral("FREE_WORK"), ui->radioButtonFreeWorkstation},
        {QStringLiteral("KEY_TEST"), ui->radioButtonKeyTest},
        {QStringLiteral("SUCTION_TEST"), ui->radioButtonSuctionTest},
        {QStringLiteral("MAIN_TEST"), ui->radioButtonDebug},
        {QStringLiteral("PRESS_TEST"), ui->radioButtonPressTest},
    };
    for (QRadioButton* button : stationMap) {
        button->setChecked(false);
    }
    if (QRadioButton* button = stationMap.value(station)) {
        button->setChecked(true);
    }
}

void saveSystemStation(Ui::qsetting* ui) {
    struct StationRadio {
        QRadioButton* button;
        const char* key;
    };
    const StationRadio items[] = {
        {ui->radioButtonImuCalibration, "IMU_CALI"},
        {ui->radioButtonMotorCalibration, "MOTOR_TEST"},
        {ui->radioButtonStaticCurrent, "QUIESCENT_CURRENT"},
        {ui->radioButtonScreenTest, "SCREEN_TEST"},
        {ui->radioButtonCameraTest, "CAMERA_TEST"},
        {ui->radioButtonSignalTest, "WIFIBLE_TEST"},
        {ui->radioButtonAgingTest, "AGE_TEST"},
        {ui->radioButtonPressTest, "PRESS_TEST"},
        {ui->radioButtonBoardFactoryTest, "PCBA_TEST"},
        {ui->radioButtonKeyTest, "KEY_TEST"},
        {ui->radioButtonSuctionTest, "SUCTION_TEST"},
        {ui->radioButtonFreeWorkstation, "FREE_WORK"},
    };
    for (const auto& item : items) {
        if (item.button && item.button->isChecked()) {
            SETTINGS.setValue(QStringLiteral("SYSTEM/station"), QString::fromUtf8(item.key));
            return;
        }
    }
    SETTINGS.setValue(QStringLiteral("SYSTEM/station"), QStringLiteral("MAIN_TEST"));
}

void ensureSystemProtocolComboItems(Ui::qsetting* ui) {
    if (ui->comboBox_systemProtocolType->count() > 0) {
        return;
    }
    ui->comboBox_systemProtocolType->addItem(QStringLiteral("qpb（产测 PB）"), QStringLiteral("qpb"));
    ui->comboBox_systemProtocolType->addItem(QStringLiteral("qfctp（FCTP）"), QStringLiteral("qfctp"));
    ui->comboBox_systemProtocolType->addItem(QStringLiteral("qaiot（AIOT TLV）"), QStringLiteral("qaiot"));
}

void loadSystemProtocolType(Ui::qsetting* ui) {
    ensureSystemProtocolComboItems(ui);
    const QString proto =
        SETTINGS.value(QStringLiteral("SYSTEM/ProtocolType"), QStringLiteral("qpb")).toString().trimmed().toLower();
    const int idx = ui->comboBox_systemProtocolType->findData(proto);
    ui->comboBox_systemProtocolType->setCurrentIndex(idx < 0 ? 0 : idx);
}

void saveSystemProtocolType(Ui::qsetting* ui) {
    SETTINGS.setValue(QStringLiteral("SYSTEM/ProtocolType"),
                      ui->comboBox_systemProtocolType->currentData().toString().trimmed());
}

void loadPlcStation2(Ui::qsetting* ui) {
    ui->lineEdit_plcIpAddressStation2->setText(
        SETTINGS.value(QStringLiteral("PLC/IpAddress_Station2"), QString()).toString());
    ui->lineEdit_plcPortStation2->setText(SETTINGS.value(QStringLiteral("PLC/Port_Station2"), QString()).toString());
    ui->lineEdit_plcUnitIdStation2->setText(SETTINGS.value(QStringLiteral("PLC/UnitId_Station2"), QString()).toString());
    const int mb2 = SETTINGS.value(QStringLiteral("PLC/MBase_Station2"), -1).toInt();
    ui->lineEdit_plcMBaseStation2->setText(mb2 < 0 ? QString() : QString::number(mb2));
    ui->lineEdit_plcMCoilOffsetStation2->setText(
        SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station2"), QString()).toString());
}

void savePlcStation2(Ui::qsetting* ui) {
    auto savePlcOptional = [](const QString& key, const QString& raw) {
        const QString t = raw.trimmed();
        if (t.isEmpty()) {
            SETTINGS.remove(key);
        } else {
            SETTINGS.setValue(key, t);
        }
    };
    savePlcOptional(QStringLiteral("PLC/IpAddress_Station2"), ui->lineEdit_plcIpAddressStation2->text());
    savePlcOptional(QStringLiteral("PLC/Port_Station2"), ui->lineEdit_plcPortStation2->text());
    savePlcOptional(QStringLiteral("PLC/UnitId_Station2"), ui->lineEdit_plcUnitIdStation2->text());
    const QString mBase2 = ui->lineEdit_plcMBaseStation2->text().trimmed();
    if (mBase2.isEmpty()) {
        SETTINGS.remove(QStringLiteral("PLC/MBase_Station2"));
    } else {
        SETTINGS.setValue(QStringLiteral("PLC/MBase_Station2"), mBase2.toInt());
    }
    savePlcOptional(QStringLiteral("PLC/MCoilAddressOffset_Station2"), ui->lineEdit_plcMCoilOffsetStation2->text());
}

void loadTupleEnvironment(Ui::qsetting* ui) {
    const QString savedTupleUrl =
        SETTINGS.value(QStringLiteral("Tuple/BaseUrl"), kTupleEnvPresets.first().baseUrl).toString();
    QSignalBlocker blocker(ui->comboBox_tupleEnvironment);
    const QString envKey = SETTINGS.value(QStringLiteral("Tuple/Environment")).toString().trimmed();
    int idx = envKey.isEmpty() ? -1 : tupleEnvIndexForKey(ui->comboBox_tupleEnvironment, envKey);
    if (idx < 0) {
        idx = tupleEnvIndexForUrl(ui->comboBox_tupleEnvironment, savedTupleUrl);
    }
    if (idx < 0) {
        idx = ui->comboBox_tupleEnvironment->count() - 1;
    }
    ui->comboBox_tupleEnvironment->setCurrentIndex(idx);
}

void saveTupleEnvironment(Ui::qsetting* ui) {
    SETTINGS.setValue(QStringLiteral("Tuple/Environment"), ui->comboBox_tupleEnvironment->currentData().toString());
}

void loadKeyCapEndian(Ui::qsetting* ui) {
    const QString capEndian =
        SETTINGS.value(QStringLiteral("KeyCap/ValueEndian"), QStringLiteral("big")).toString();
    ui->comboBox_KeyCapValueEndian->setCurrentIndex(
        capEndian.compare(QStringLiteral("little"), Qt::CaseInsensitive) == 0 ? 1 : 0);
}

void saveKeyCapEndian(Ui::qsetting* ui) {
    SETTINGS.setValue(QStringLiteral("KeyCap/ValueEndian"),
                      ui->comboBox_KeyCapValueEndian->currentIndex() == 1 ? QStringLiteral("little")
                                                                          : QStringLiteral("big"));
}

void loadBoundSettings(Ui::qsetting* ui) {
    ensureSystemProtocolComboItems(ui);
    loadSettingBinds(settingPageBinds(ui));
    loadSystemProtocolType(ui);
    loadPlcStation2(ui);
    loadTupleEnvironment(ui);
    loadKeyCapEndian(ui);
}

void saveBoundSettings(Ui::qsetting* ui) {
    saveSettingBinds(settingPageBinds(ui));
    saveSystemProtocolType(ui);
    savePlcStation2(ui);
    saveTupleEnvironment(ui);
    saveKeyCapEndian(ui);
    bydmes::loadExternalMesConfig(nullptr);
}
