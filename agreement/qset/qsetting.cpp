#include "qsetting.h"

#include "ui_qsetting.h"

qsetting::qsetting(QWidget* parent) : QWidget(parent), ui(new Ui::qsetting) {
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    // 假设你已经定义了 QButtonGroup* StationGroup;
    // 假设 StationGroup 已经被定义并初始化
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonDebug"), 0);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonStaticCurrent"), 1);  // 更新为静态电流
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonMotorCalibration"), 2);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonImuCalibration"), 3);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonScreenTest"), 4);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonCameraTest"), 5);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonSignalTest"), 6);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonAgingTest"), 7);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonBoardFactoryTest"), 8);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonFreeWorkstation"), 9);

    loadConfig();
}
void qsetting::loadConfig() {
    // 假设 SETTINGS 是一个 QSettings 实例
    QString station = SETTINGS.value("SYSTEM/station").toString();  // 读取 station 的值
    const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
    QVariant windowSize(availableSize / 4 * 3);
    this->resize(SETTINGS.value("Window/SettingSize", windowSize).toSize());

    // 使用映射将 station 和 QRadioButton 关联
    QMap<QString, QRadioButton*> stationMap = {
        {"IMU_CALI", ui->radioButtonImuCalibration},      {"MOTOR_TEST", ui->radioButtonMotorCalibration},
        {"STATIC_CURRENT", ui->radioButtonStaticCurrent}, {"SCREEN_TEST", ui->radioButtonScreenTest},
        {"CAMERA_TEST", ui->radioButtonCameraTest},       {"WIFIBLE_TEST", ui->radioButtonSignalTest},
        {"AGE_TEST", ui->radioButtonAgingTest},           {"PCBA_TEST", ui->radioButtonBoardFactoryTest},
        {"FREE_WORK", ui->radioButtonFreeWorkstation},    {"MAIN_TEST", ui->radioButtonDebug}};

    // 清除所有 QRadioButton 的选中状态
    for (auto button : stationMap) {
        button->setChecked(false);
    }

    // 根据 station 的值设置对应的 QRadioButton 为选中状态
    if (stationMap.contains(station)) {
        stationMap[station]->setChecked(true);
    }

    // 读取配置文件信息并设置复选框状态
    ui->checkBox_ShowLocalOTAFunc->setChecked(SETTINGS.value("SYSTEM/ShowLocalOTAFunc").toBool());
    ui->checkBox_ShowUpperComputerOTAFunc->setChecked(SETTINGS.value("SYSTEM/ShowUpperComputerOTAFunc").toBool());
    ui->checkBox_SaveToothbrushLog->setChecked(SETTINGS.value("SYSTEM/SaveToothbrushLog").toBool());
    ui->checkBox_SimplPcbaTest->setChecked(SETTINGS.value("SYSTEM/SimplPcbaTest").toBool());
    ui->checkBox_NeedWriteSubpid->setChecked(SETTINGS.value("SYSTEM/NeedWriteSubpid").toBool());
    ui->checkBox_BluetoothImageTransfer->setChecked(SETTINGS.value("SYSTEM/BluetoothImageTransfer").toBool());
    ui->checkBox_IMUCalibrationWakeup->setChecked(SETTINGS.value("SYSTEM/IMUCalibrationWakeup").toBool());
    ui->checkBox_DisableSerialPortRx->setChecked(SETTINGS.value("SYSTEM/DisableSerialPortRx").toBool());
    ui->checkBox_ShipModeResponse->setChecked(SETTINGS.value("SYSTEM/ShipModeResponse").toBool());
    ui->checkBox_SerialPortMAC->setChecked(SETTINGS.value("SYSTEM/SerialPortMAC").toBool());
    ui->checkBox_MagneticReuseMotorStatus->setChecked(SETTINGS.value("SYSTEM/MagneticReuseMotorStatus").toBool());
    ui->checkBox_TestAudioCurrent->setChecked(SETTINGS.value("SYSTEM/TestAudioCurrent").toBool());
    ui->checkBox_TestShippingCurrent->setChecked(SETTINGS.value("SYSTEM/TestShippingCurrent").toBool());
    ui->checkBox_SendMotorCalibration->setChecked(SETTINGS.value("SYSTEM/SendMotorCalibration").toBool());
    ui->checkBox_LightTest->setChecked(SETTINGS.value("SYSTEM/LightTest").toBool());
    ui->checkBox_ServoMotorStart->setChecked(SETTINGS.value("SYSTEM/ServoMotorStart").toBool());
    ui->checkBox_TestWifiSignal->setChecked(SETTINGS.value("SYSTEM/TestWifiSignal").toBool());

    ui->checkBox_IMULastEnterStartTest->setChecked(SETTINGS.value("SYSTEM/IMULastEnterStartTest").toBool());

    // 加载 SN
    ui->snLineEdit->setText(SETTINGS.value("Regex/SNPattern").toString());

    // 加载 WIFI 信息
    ui->wifiAccountLineEdit->setText(SETTINGS.value("WIFI/Name").toString());
    ui->wifiPasswordLineEdit->setText(SETTINGS.value("WIFI/Password").toString());
    ui->wifiUpperLimitLineEdit->setText(SETTINGS.value("WIFI/HighRssi").toString());
    ui->wifiLowerLimitLineEdit->setText(SETTINGS.value("WIFI/LowRssi").toString());

    // 加载 BLE 信息
    ui->bluetoothUpperLimitLineEdit->setText(SETTINGS.value("BLE/HighRssi").toString());
    ui->bluetoothLowerLimitLineEdit->setText(SETTINGS.value("BLE/LowRssi").toString());

    // 加载信号测试次数
    ui->signalTestCountLineEdit->setText(SETTINGS.value("BLE/RssiCount").toString());

    // 加载行和列
    ui->rowLineEdit->setText(SETTINGS.value("User/formRow").toString());
    ui->columnLineEdit->setText(SETTINGS.value("User/formColumn").toString());

    // 设置控件的值
    ui->lineEdit_MusicStatus->setText(SETTINGS.value("FIXTEST/MusicState").toString());
    ui->lineEdit_OvervoltageLightStatus->setText(SETTINGS.value("FIXTEST/OverVoltageLight").toString());
    ui->lineEdit_Button1Status->setText(SETTINGS.value("FIXTEST/Button1").toString());
    ui->lineEdit_Button2Status->setText(SETTINGS.value("FIXTEST/Button2").toString());

    // 加载 IMU 校准参数
    ui->lineEdit_calibrationTime->setText(SETTINGS.value("IMU/IMU_Wait_Time").toString());   // imu校准总时间
    ui->lineEdit_comparisonValue->setText(SETTINGS.value("IMU/ImuCompareData").toString());  // imu测试比较值
    ui->lineEdit_singlePointTimeout->setText(SETTINGS.value("IMU/imu_cali_wait_time").toString());  // imu单点位超时时间
    ui->lineEdit_zAxisUpper->setText(SETTINGS.value("IMU/acc_z_up").toString());                    // imu的z轴上限
    ui->lineEdit_zAxisLower->setText(SETTINGS.value("IMU/acc_z_down").toString());                  // imu的z轴下限
    ui->lineEdit_xAxisUpper->setText(SETTINGS.value("IMU/acc_x_up").toString());                    // imu的x轴上限
    ui->lineEdit_xAxisLower->setText(SETTINGS.value("IMU/acc_x_down").toString());                  // imu的x轴下限
    ui->lineEdit_yAxisUpper->setText(SETTINGS.value("IMU/acc_y_up").toString());                    // imu的y轴上限
    ui->lineEdit_yAxisLower->setText(SETTINGS.value("IMU/acc_y_down").toString());                  // imu的y轴下限
    // 加载 IMU 参数
    ui->lineEdit_threshold->setText(SETTINGS.value("IMU/STATIC_CONV_VAR").toString());       // 数据波动阈值
    ui->lineEdit_delayFrames->setText(SETTINGS.value("IMU/STATIC_CONV_DELAY").toString());   // 延时帧数
    ui->lineEdit_sampleFrames->setText(SETTINGS.value("IMU/STATIC_CONV_COUNT").toString());  // 样本帧数

    // 基础信息
    ui->lineEdit_ProductName->setText(SETTINGS.value("ProductInfo/Product_Name").toString());          // 产品名字
    ui->lineEdit_HardwareVersion->setText(SETTINGS.value("ProductInfo/Hardware_Version").toString());  // 硬件版本号
    ui->lineEdit_SoftwareVersion->setText(SETTINGS.value("ProductInfo/Software_Version").toString());    // 软件版本
    ui->lineEdit_ResourceVersion->setText(SETTINGS.value("ProductInfo/Resource_Version").toString());    // 资源版本
    ui->lineEdit_AgingStatus->setText(SETTINGS.value("ProductInfo/Age_State").toString());               // 老化状态
    ui->lineEdit_MotorVersion->setText(SETTINGS.value("ProductInfo/Motor_Ver").toString());              // 电机版本
    ui->lineEdit_BluetoothVersion->setText(SETTINGS.value("ProductInfo/Ble_Ver").toString());            // 蓝牙版本
    ui->lineEdit_AppPB->setText(SETTINGS.value("ProductInfo/App_Protocol_Version").toString());          // app的pb
    ui->lineEdit_FactoryPB->setText(SETTINGS.value("ProductInfo/Factory_Protocol_Version").toString());  // 厂测的pb
    ui->lineEdit_AlgorithmVersion->setText(SETTINGS.value("ProductInfo/Algorithm_Version").toString());  // 算法版本
    ui->lineEdit_PressureVersion->setText(SETTINGS.value("ProductInfo/Pressure_Sense_Version").toString());  // 压感版本
    ui->lineEdit_ImuID->setText(SETTINGS.value("ProductInfo/IMU_ID").toString());                            // imu的id
    ui->lineEdit_CameraID->setText(SETTINGS.value("ProductInfo/Camera_Id").toString());  // 摄像头的id

    // 船运电流
    ui->lineEdit_CargoCurrentUpper->setText(SETTINGS.value("Current/HighshipCurrent").toString());
    ui->lineEdit_CargoCurrentLower->setText(SETTINGS.value("Current/LowshipCurrent").toString());

    // 充电电流
    ui->lineEdit_ChargingCurrentUpper->setText(SETTINGS.value("Current/HighCharCurrent").toString());
    ui->lineEdit_ChargingCurrentLower->setText(SETTINGS.value("Current/LowCharCurrent").toString());

    // 工作电流
    ui->lineEdit_WorkingCurrentUpper->setText(SETTINGS.value("Current/HighworkCurrent").toString());
    ui->lineEdit_WorkingCurrentLower->setText(SETTINGS.value("Current/LowworkCurrent").toString());

    // 静态电流
    ui->lineEdit_StaticCurrentUpper->setText(SETTINGS.value("Current/HighstaticCurrent").toString());
    ui->lineEdit_StaticCurrentLower->setText(SETTINGS.value("Current/LowstaticCurrent").toString());

    // 音频电流
    ui->lineEdit_AudioCurrentUpper->setText(SETTINGS.value("Current/HighmusicCurrent").toString());
    ui->lineEdit_AudioCurrentLower->setText(SETTINGS.value("Current/LowmusicCurrent").toString());

    // 测试限制时长
    ui->lineEdit_TestDuration->setText(SETTINGS.value("Current/measure_wait_time").toString());

    // Peripheral Status
    ui->lineEdit_imu_status->setText(SETTINGS.value("PeripheralStatus/IMU_Status").toString());      // imu状态
    ui->lineEdit_flash_status->setText(SETTINGS.value("PeripheralStatus/Flash_Status").toString());  // flash状态
    ui->lineEdit_magnetic_status->setText(SETTINGS.value("PeripheralStatus/Magnetic_Status").toString());  // 地磁状态
    ui->lineEdit_pressure_status->setText(SETTINGS.value("PeripheralStatus/Pressure_Status").toString());  // 压感状态
    ui->lineEdit_audio_status->setText(SETTINGS.value("PeripheralStatus/Audio_Status").toString());  // 音频状态

    // Battery Control
    ui->lineEdit_battery_voltage_control->setText(SETTINGS.value("BATTARY/standbattary").toString());  // 电池电压

    // Camera Position
    ui->lineEdit_camera_x->setText(SETTINGS.value("CAMERA/Rect1_X").toString());  // 摄像头偏位标准x坐标
    ui->lineEdit_camera_y->setText(SETTINGS.value("CAMERA/Rect1_Y").toString());  // 摄像头偏位标准y坐标
    ui->lineEdit_camera_width->setText(SETTINGS.value("CAMERA/Rect1_Width").toString());  // 摄像头偏位标准矩形宽度
    ui->lineEdit_camera_height->setText(SETTINGS.value("CAMERA/Rect1_Height").toString());  // 摄像头偏位标准矩形高度
    ui->lineEdit_image_interval->setText(SETTINGS.value("CAMERA/CameraGetTime").toString());  // 重新获取图片时间间隔

    // MES 相关
    ui->lineEdit_factory->setText(SETTINGS.value("MES/factory").toString());
    ui->lineEdit_productName->setText(SETTINGS.value("MES/Product_Name").toString());
    ui->lineEdit_macStation->setText(SETTINGS.value("MES/xwdWpCode").toString());
    ui->lineEdit_station->setText(SETTINGS.value("MES/machineNo").toString());
    ui->lineEdit_mesUrl->setText(SETTINGS.value("MES/NET").toString());
    ui->lineEdit_processName->setText(SETTINGS.value("MES/test_station").toString());
    ui->lineEdit_modelName->setText(SETTINGS.value("MES/model").toString());
    ui->lineEdit_mac_field->setText(SETTINGS.value("MES/FIELD").toString());
    ui->lineEdit_mes_operator->setText(SETTINGS.value("MES/mUserno").toString());
    ui->lineEdit_weikesen_order->setText(SETTINGS.value("MES/Work_Order").toString());
    ui->lineEdit_mes_login_account->setText(SETTINGS.value("MES/M_USERNO").toString());
    ui->lineEdit_action_huaqin->setText(SETTINGS.value("MES/Action").toString());
    ui->lineEdit_mes_login_station->setText(SETTINGS.value("MES/M_MACHINENO").toString());
    ui->lineEdit_line_huaqin->setText(SETTINGS.value("MES/Line").toString());
    ui->lineEdit_mes_login_password->setText(SETTINGS.value("MES/M_PASSWORD").toString());
}
qsetting::~qsetting() {
    qDebug() << "已经删除页面";
    delete ui;
}
void qsetting::saveConfig() {
    SETTINGS.setValue("Window/SettingSize", this->size());
    // 假设 SETTINGS 是一个 QSettings 实例
    SETTINGS.setValue("SYSTEM/station", ui->radioButtonImuCalibration->isChecked()   ? "IMU_CALI" :
                                        ui->radioButtonMotorCalibration->isChecked() ? "MOTOR_TEST" :
                                        ui->radioButtonStaticCurrent->isChecked()    ? "QUIESCENT_CURRENT" :
                                        ui->radioButtonScreenTest->isChecked()       ? "SCREEN_TEST" :
                                        ui->radioButtonCameraTest->isChecked()       ? "CAMERA_TEST" :
                                        ui->radioButtonSignalTest->isChecked()       ? "WIFIBLE_TEST" :
                                        ui->radioButtonAgingTest->isChecked()        ? "AGE_TEST" :
                                        ui->radioButtonBoardFactoryTest->isChecked() ? "PCBA_TEST" :
                                        ui->radioButtonFreeWorkstation->isChecked()  ? "FREE_WORK" :
                                                                                       "MAIN_TEST");

    // 保存复选框状态到配置文件
    SETTINGS.setValue("SYSTEM/ShowLocalOTAFunc", ui->checkBox_ShowLocalOTAFunc->isChecked());
    SETTINGS.setValue("SYSTEM/ShowUpperComputerOTAFunc", ui->checkBox_ShowUpperComputerOTAFunc->isChecked());
    SETTINGS.setValue("SYSTEM/SaveToothbrushLog", ui->checkBox_SaveToothbrushLog->isChecked());
    SETTINGS.setValue("SYSTEM/SimplPcbaTest", ui->checkBox_SimplPcbaTest->isChecked());
    SETTINGS.setValue("SYSTEM/NeedWriteSubpid", ui->checkBox_NeedWriteSubpid->isChecked());
    SETTINGS.setValue("SYSTEM/BluetoothImageTransfer", ui->checkBox_BluetoothImageTransfer->isChecked());
    SETTINGS.setValue("SYSTEM/IMUCalibrationWakeup", ui->checkBox_IMUCalibrationWakeup->isChecked());
    SETTINGS.setValue("SYSTEM/DisableSerialPortRx", ui->checkBox_DisableSerialPortRx->isChecked());
    SETTINGS.setValue("SYSTEM/ShipModeResponse", ui->checkBox_ShipModeResponse->isChecked());
    SETTINGS.setValue("SYSTEM/SerialPortMAC", ui->checkBox_SerialPortMAC->isChecked());
    SETTINGS.setValue("SYSTEM/MagneticReuseMotorStatus", ui->checkBox_MagneticReuseMotorStatus->isChecked());
    SETTINGS.setValue("SYSTEM/TestAudioCurrent", ui->checkBox_TestAudioCurrent->isChecked());
    SETTINGS.setValue("SYSTEM/TestShippingCurrent", ui->checkBox_TestShippingCurrent->isChecked());
    SETTINGS.setValue("SYSTEM/SendMotorCalibration", ui->checkBox_SendMotorCalibration->isChecked());
    SETTINGS.setValue("SYSTEM/LightTest", ui->checkBox_LightTest->isChecked());
    SETTINGS.setValue("SYSTEM/ServoMotorStart", ui->checkBox_ServoMotorStart->isChecked());
    SETTINGS.setValue("SYSTEM/TestWifiSignal", ui->checkBox_TestWifiSignal->isChecked());
    SETTINGS.setValue("SYSTEM/IMULastEnterStartTest", ui->checkBox_IMULastEnterStartTest->isChecked());

    // 保存 SN
    SETTINGS.setValue("Regex/SNPattern", ui->snLineEdit->text());

    // 保存 WIFI 信息
    SETTINGS.setValue("WIFI/Name", ui->wifiAccountLineEdit->text());
    SETTINGS.setValue("WIFI/Password", ui->wifiPasswordLineEdit->text());
    SETTINGS.setValue("WIFI/HighRssi", ui->wifiUpperLimitLineEdit->text());
    SETTINGS.setValue("WIFI/LowRssi", ui->wifiLowerLimitLineEdit->text());

    // 保存 BLE 信息
    SETTINGS.setValue("BLE/HighRssi", ui->bluetoothUpperLimitLineEdit->text());
    SETTINGS.setValue("BLE/LowRssi", ui->bluetoothLowerLimitLineEdit->text());

    // 保存信号测试次数
    SETTINGS.setValue("BLE/RssiCount", ui->signalTestCountLineEdit->text());

    // 保存行和列
    SETTINGS.setValue("User/formRow", ui->rowLineEdit->text());
    SETTINGS.setValue("User/formColumn", ui->columnLineEdit->text());

    // 将控件的值设置回配置文件
    SETTINGS.setValue("FIXTEST/MusicState", ui->lineEdit_MusicStatus->text());
    SETTINGS.setValue("FIXTEST/OverVoltageLight", ui->lineEdit_OvervoltageLightStatus->text());
    SETTINGS.setValue("FIXTEST/Button1", ui->lineEdit_Button1Status->text());
    SETTINGS.setValue("FIXTEST/Button2", ui->lineEdit_Button2Status->text());

    // 保存 IMU 校准参数
    SETTINGS.setValue("IMU/IMU_Wait_Time", ui->lineEdit_calibrationTime->text());
    SETTINGS.setValue("IMU/ImuCompareData", ui->lineEdit_comparisonValue->text());
    SETTINGS.setValue("IMU/imu_cali_wait_time", ui->lineEdit_singlePointTimeout->text());
    SETTINGS.setValue("IMU/acc_z_up", ui->lineEdit_zAxisUpper->text());
    SETTINGS.setValue("IMU/acc_z_down", ui->lineEdit_zAxisLower->text());
    SETTINGS.setValue("IMU/acc_x_up", ui->lineEdit_xAxisUpper->text());
    SETTINGS.setValue("IMU/acc_x_down", ui->lineEdit_xAxisLower->text());
    SETTINGS.setValue("IMU/acc_y_up", ui->lineEdit_yAxisUpper->text());
    SETTINGS.setValue("IMU/acc_y_down", ui->lineEdit_yAxisLower->text());

    // 保存 IMU 参数
    SETTINGS.setValue("IMU/STATIC_CONV_VAR", ui->lineEdit_threshold->text());
    SETTINGS.setValue("IMU/STATIC_CONV_DELAY", ui->lineEdit_delayFrames->text());
    SETTINGS.setValue("IMU/STATIC_CONV_COUNT", ui->lineEdit_sampleFrames->text());

    // 基础信息
    SETTINGS.setValue("ProductInfo/Product_Name", ui->lineEdit_ProductName->text());
    SETTINGS.setValue("ProductInfo/Hardware_Version", ui->lineEdit_HardwareVersion->text());
    SETTINGS.setValue("ProductInfo/Software_Version", ui->lineEdit_SoftwareVersion->text());
    SETTINGS.setValue("ProductInfo/Resource_Version", ui->lineEdit_ResourceVersion->text());
    SETTINGS.setValue("ProductInfo/Age_State", ui->lineEdit_AgingStatus->text());
    SETTINGS.setValue("ProductInfo/Motor_Ver", ui->lineEdit_MotorVersion->text());
    SETTINGS.setValue("ProductInfo/Ble_Ver", ui->lineEdit_BluetoothVersion->text());
    SETTINGS.setValue("ProductInfo/App_Protocol_Version", ui->lineEdit_AppPB->text());
    SETTINGS.setValue("ProductInfo/Factory_Protocol_Version", ui->lineEdit_FactoryPB->text());
    SETTINGS.setValue("ProductInfo/Algorithm_Version", ui->lineEdit_AlgorithmVersion->text());
    SETTINGS.setValue("ProductInfo/Pressure_Sense_Version", ui->lineEdit_PressureVersion->text());
    SETTINGS.setValue("ProductInfo/IMU_ID", ui->lineEdit_ImuID->text());
    SETTINGS.setValue("ProductInfo/Camera_Id", ui->lineEdit_CameraID->text());

    // 船运电流
    SETTINGS.setValue("Current/HighshipCurrent", ui->lineEdit_CargoCurrentUpper->text());
    SETTINGS.setValue("Current/LowshipCurrent", ui->lineEdit_CargoCurrentLower->text());

    // 充电电流
    SETTINGS.setValue("Current/HighCharCurrent", ui->lineEdit_ChargingCurrentUpper->text());
    SETTINGS.setValue("Current/LowCharCurrent", ui->lineEdit_ChargingCurrentLower->text());

    // 工作电流
    SETTINGS.setValue("Current/HighworkCurrent", ui->lineEdit_WorkingCurrentUpper->text());
    SETTINGS.setValue("Current/LowworkCurrent", ui->lineEdit_WorkingCurrentLower->text());

    // 静态电流
    SETTINGS.setValue("Current/HighstaticCurrent", ui->lineEdit_StaticCurrentUpper->text());
    SETTINGS.setValue("Current/LowstaticCurrent", ui->lineEdit_StaticCurrentLower->text());

    // 音频电流
    SETTINGS.setValue("Current/HighmusicCurrent", ui->lineEdit_AudioCurrentUpper->text());
    SETTINGS.setValue("Current/LowmusicCurrent", ui->lineEdit_AudioCurrentLower->text());

    // 测试限制时长
    SETTINGS.setValue("Current/measure_wait_time", ui->lineEdit_TestDuration->text());

    // Peripheral Status
    SETTINGS.setValue("PeripheralStatus/IMU_Status", ui->lineEdit_imu_status->text());
    SETTINGS.setValue("PeripheralStatus/Flash_Status", ui->lineEdit_flash_status->text());
    SETTINGS.setValue("PeripheralStatus/Magnetic_Status", ui->lineEdit_magnetic_status->text());
    SETTINGS.setValue("PeripheralStatus/Pressure_Status", ui->lineEdit_pressure_status->text());
    SETTINGS.setValue("PeripheralStatus/Audio_Status", ui->lineEdit_audio_status->text());

    // Battery Control
    SETTINGS.setValue("BATTARY/standbattary", ui->lineEdit_battery_voltage_control->text());

    // Camera Position
    SETTINGS.setValue("CAMERA/Rect1_X", ui->lineEdit_camera_x->text());
    SETTINGS.setValue("CAMERA/Rect1_Y", ui->lineEdit_camera_y->text());
    SETTINGS.setValue("CAMERA/Rect1_Width", ui->lineEdit_camera_width->text());
    SETTINGS.setValue("CAMERA/Rect1_Height", ui->lineEdit_camera_height->text());
    SETTINGS.setValue("CAMERA/CameraGetTime", ui->lineEdit_image_interval->text());

    // MES 相关
    SETTINGS.setValue("MES/factory", ui->lineEdit_factory->text());
    SETTINGS.setValue("MES/Product_Name", ui->lineEdit_productName->text());
    SETTINGS.setValue("MES/xwdWpCode", ui->lineEdit_macStation->text());
    SETTINGS.setValue("MES/machineNo", ui->lineEdit_station->text());
    SETTINGS.setValue("MES/NET", ui->lineEdit_mesUrl->text());
    SETTINGS.setValue("MES/test_station", ui->lineEdit_processName->text());
    SETTINGS.setValue("MES/model", ui->lineEdit_modelName->text());
    SETTINGS.setValue("MES/FIELD", ui->lineEdit_mac_field->text());
    SETTINGS.setValue("MES/mUserno", ui->lineEdit_mes_operator->text());
    SETTINGS.setValue("MES/Work_Order", ui->lineEdit_weikesen_order->text());
    SETTINGS.setValue("MES/M_USERNO", ui->lineEdit_mes_login_account->text());
    SETTINGS.setValue("MES/Action", ui->lineEdit_action_huaqin->text());
    SETTINGS.setValue("MES/M_MACHINENO", ui->lineEdit_mes_login_station->text());
    SETTINGS.setValue("MES/Line", ui->lineEdit_line_huaqin->text());
    SETTINGS.setValue("MES/M_PASSWORD", ui->lineEdit_mes_login_password->text());
}

void qsetting::closeEvent(QCloseEvent* event) {
    saveConfig();
    qDebug() << "已经保存配置信息";
}

void qsetting::on_Restore_default_setting_clicked() {
    ui->checkBox_NeedWriteSubpid->setChecked(false);
    ui->checkBox_BluetoothImageTransfer->setChecked(false);
    ui->checkBox_IMUCalibrationWakeup->setChecked(false);
    ui->checkBox_DisableSerialPortRx->setChecked(false);
    ui->checkBox_ShipModeResponse->setChecked(false);
    ui->checkBox_SerialPortMAC->setChecked(false);
    ui->checkBox_MagneticReuseMotorStatus->setChecked(false);
    ui->checkBox_TestAudioCurrent->setChecked(false);
    ui->checkBox_TestShippingCurrent->setChecked(false);
    ui->checkBox_SendMotorCalibration->setChecked(false);
    ui->checkBox_LightTest->setChecked(false);
    ui->checkBox_ServoMotorStart->setChecked(false);
    ui->checkBox_TestWifiSignal->setChecked(false);
    ui->checkBox_IMULastEnterStartTest->setChecked(false);

    //立讯的p20p要回车开始，木星是按键开始
    //立讯：imu需要晃动唤醒（加快蓝牙连接），全扫码再测试
    if (ui->lineEdit_productName->text() == "U7") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_DisableSerialPortRx->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
    }
    //立讯：imu需要晃动唤醒（加快蓝牙连接），全扫码再测试
    if (ui->lineEdit_productName->text() == "U7P") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_BluetoothImageTransfer->setChecked(true);
        ui->checkBox_DisableSerialPortRx->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
    }
    //立讯：imu需要晃动唤醒，全扫码再测试
    if (ui->lineEdit_productName->text() == "F20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
    }
    //欣旺达：依次扫码连接，不需要唤醒
    if (ui->lineEdit_productName->text() == "Q20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->lineEdit_productName->text() == "Y20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->lineEdit_productName->text() == "Y20P") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //欣旺达：依次扫标签码连接，需要唤醒
    if (ui->lineEdit_productName->text() == "Y21") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    //华勤：依次扫码连接，不需要唤醒
    if (ui->lineEdit_productName->text() == "P20P") {
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_MagneticReuseMotorStatus->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);
        if (ui->lineEdit_factory->text() == "lx")
            ui->checkBox_IMULastEnterStartTest->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->lineEdit_productName->text() == "P30P") {
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
    }
}
void qsetting::updateMainStyle(QString style) {
    // QSS文件初始化界面样式
    QString stylesheet;
    QFile qss(style);
    if (qss.open(QFile::ReadOnly)) {
        qDebug() << "qss load";
        QTextStream filetext(&qss);
        stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    } else {
        qDebug() << "qss not load";
        qss.setFileName("/qss/" + style);
        if (qss.open(QFile::ReadOnly)) {
            qDebug() << "qss load";
            QTextStream filetext(&qss);
            stylesheet = filetext.readAll();
            this->setStyleSheet(stylesheet);
            qss.close();
        }
    }
}
