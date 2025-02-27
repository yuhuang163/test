#include "qsetting.h"

#include "qevent.h"
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
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonPressTest"), 9);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonFreeWorkstation"), 10);

    // 如果需要从某个数据源添加项，可以使用循环来添加
    QStringList productList = {"U7",    "U7P", "F20",   "Q20", "Q20P",  "Y20",   "Y20P", "Y30",
                               "Y30PS", "Y21", "Y20PO", "T10", "P20PS", "Y25SE", "P20P"};
    ui->comboBox_productName->addItems(productList);

    QStringList pressFunctionSwitch = {"无效选项", "单校准", "单测试", "校准加测试"};
    ui->comboBox_pressFunctionSwitch->addItems(pressFunctionSwitch);

    QStringList individualMode = {"无效选项", "单独刷头", "单独按键", "都校准"};
    ui->comboBox_individualMode->addItems(individualMode);

    QStringList displayImageType = {"无效选项", "关闭所有图像", "ADC图像",     "压力值图像",
                                    "刷头图像", "按键图像",     "显示全部图像"};
    ui->comboBox_displayImageType->addItems(displayImageType);

    QStringList factoryList = {"lx", "xwd", "hq", "wks", "ydm", "无mes厂"};
    ui->comboBox_factory->addItems(factoryList);

    loadConfig();
    RestoreFacDefaultSetting();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
}

void qsetting::readSubPIDAndFilter() {
    SETTINGS.beginGroup("SUBPID");
    // 读取 [SUBPID] 部分的所有键值对
    QStringList subPIDKeys = SETTINGS.childKeys();  // 获取所有键
    SETTINGS.endGroup();
    // 用于存储不同值的变量
    QString subPID_00;
    QString subPID_01;
    QString subPID_02;
    QString subPID_03;
    // qDebug() << "subPIDKeys###########: " << subPIDKeys;
    // 遍历所有键，并读取对应的值
    for (const QString& key : subPIDKeys) {
        QString value = SETTINGS.value("SUBPID/" + key).toString();

        // 根据值分类，将键名加到相应的变量中
        if (value == "00") {
            if (!subPID_00.isEmpty()) {
                subPID_00 += "=";  // 如果不是第一个键，添加 "=" 连接符
            }
            subPID_00 += key;
        } else if (value == "01") {
            if (!subPID_01.isEmpty()) {
                subPID_01 += "=";
            }
            subPID_01 += key;
        } else if (value == "02") {
            if (!subPID_02.isEmpty()) {
                subPID_02 += "=";
            }
            subPID_02 += key;
        } else if (value == "03") {
            if (!subPID_03.isEmpty()) {
                subPID_03 += "=";
            }
            subPID_03 += key;
        }
    }

    // 输出结果
    // qDebug() << "subPID_00: " << subPID_00;
    // qDebug() << "subPID_01: " << subPID_01;
    // qDebug() << "subPID_02: " << subPID_02;
    // qDebug() << "subPID_03: " << subPID_03;

    // 将数据填充到 QLineEdit 控件
    ui->lineEdit_SubPID_00->setText(subPID_00);
    ui->lineEdit_SubPID_01->setText(subPID_01);
    ui->lineEdit_SubPID_02->setText(subPID_02);
    ui->lineEdit_SubPID_03->setText(subPID_03);
}
void qsetting::loadConfig() {
    // 假设 SETTINGS 是一个 QSettings 实例
    QString station = SETTINGS.value("SYSTEM/station").toString();  // 读取 station 的值
    const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
    QVariant windowSize(availableSize / 4 * 3);
    this->resize(SETTINGS.value("Window/SettingSize", windowSize).toSize());

    // 使用映射将 station 和 QRadioButton 关联
    QMap<QString, QRadioButton*> stationMap = {{"IMU_CALI", ui->radioButtonImuCalibration},
                                               {"MOTOR_TEST", ui->radioButtonMotorCalibration},
                                               {"QUIESCENT_CURRENT", ui->radioButtonStaticCurrent},
                                               {"SCREEN_TEST", ui->radioButtonScreenTest},
                                               {"CAMERA_TEST", ui->radioButtonCameraTest},
                                               {"WIFIBLE_TEST", ui->radioButtonSignalTest},
                                               {"AGE_TEST", ui->radioButtonAgingTest},
                                               {"PCBA_TEST", ui->radioButtonBoardFactoryTest},
                                               {"FREE_WORK", ui->radioButtonFreeWorkstation},
                                               {"MAIN_TEST", ui->radioButtonDebug},
                                               {"PRESS_TEST", ui->radioButtonPressTest}};

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
    ui->checkBox_LockProductUI->setChecked(SETTINGS.value("SYSTEM/LockProductUI").toBool());

    ui->checkBox_SimplePcbaTest->setChecked(SETTINGS.value("SYSTEM/SimplePcbaTest").toBool());
    ui->checkBox_NeedWriteSubpid->setChecked(SETTINGS.value("SYSTEM/NeedWriteSubpid").toBool());
    ui->checkBox_NeedWriteSkuid->setChecked(SETTINGS.value("SYSTEM/NeedWriteSkuid").toBool());

    ui->checkBox_BluetoothImageTransfer->setChecked(SETTINGS.value("SYSTEM/BluetoothImageTransfer").toBool());
    ui->checkBox_IMUCalibrationWakeup->setChecked(SETTINGS.value("SYSTEM/IMUCalibrationWakeup").toBool());
    ui->checkBox_DisableSerialPortRx->setChecked(SETTINGS.value("SYSTEM/DisableSerialPortRx").toBool());
    ui->checkBox_ShipModeResponse->setChecked(SETTINGS.value("SYSTEM/ShipModeResponse").toBool());
    ui->checkBox_SerialPortMAC->setChecked(SETTINGS.value("SYSTEM/SerialPortMAC").toBool());
    ui->checkBox_MagneticReuseMotorStatus->setChecked(SETTINGS.value("SYSTEM/MagneticReuseMotorStatus").toBool());
    ui->checkBox_TestAudioCurrent->setChecked(SETTINGS.value("SYSTEM/TestAudioCurrent").toBool());
    ui->checkBox_TestShippingCurrent->setChecked(SETTINGS.value("SYSTEM/TestShippingCurrent").toBool());
    ui->checkBox_PressIndependent->setChecked(SETTINGS.value("SYSTEM/PressIndependent").toBool());
    ui->checkBox_PressWindow->setChecked(SETTINGS.value("SYSTEM/PressWindow").toBool());

    ui->checkBox_SendMotorCalibration->setChecked(SETTINGS.value("SYSTEM/SendMotorCalibration").toBool());
    ui->checkBox_LightTest->setChecked(SETTINGS.value("SYSTEM/LightTest").toBool());
    ui->checkBox_uperMotor->setChecked(SETTINGS.value("SYSTEM/uperMotor").toBool());

    ui->checkBox_ServoMotorStart->setChecked(SETTINGS.value("SYSTEM/ServoMotorStart").toBool());
    ui->checkBox_TestWifiSignal->setChecked(SETTINGS.value("SYSTEM/TestWifiSignal").toBool());
    ui->checkBox_IMULastEnterStartTest->setChecked(SETTINGS.value("SYSTEM/IMULastEnterStartTest").toBool());
    ui->lineEdit_CurrentMechine->setText(SETTINGS.value("SYSTEM/CurrentMechine").toString());
    // 加载 SN
    ui->snLineEdit->setText(SETTINGS.value("Regex/SNPattern").toString());
    ui->lineEdit_music_state->setText(SETTINGS.value("Music/MusicState").toString());
    ui->checkBox_MusicState->setChecked(SETTINGS.value("Music/MusicState_checkBox").toBool());

    readSubPIDAndFilter();

    // 加载 WIFI 信息
    ui->wifiAccountLineEdit->setText(SETTINGS.value("WIFI/Name").toString());
    ui->wifiPasswordLineEdit->setText(SETTINGS.value("WIFI/Password").toString());
    ui->wifiUpperLimitLineEdit->setText(SETTINGS.value("WIFI/HighRssi").toString());
    ui->wifiLowerLimitLineEdit->setText(SETTINGS.value("WIFI/LowRssi").toString());
    ui->wifiIPLineEdit->setText(SETTINGS.value("WIFI/IP").toString());

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

    // 加载压感参数

    ui->lineEdit_testWaitTime->setText(SETTINGS.value("PRESSURE/TestTime").toString());  // 压感测试时间
    ui->comboBox_pressFunctionSwitch->setCurrentIndex(SETTINGS.value("PRESSURE/functionSwitch", 0).toInt());
    ui->comboBox_displayImageType->setCurrentIndex(SETTINGS.value("PRESSURE/Use_graph", 0).toInt());
    ui->comboBox_individualMode->setCurrentIndex(SETTINGS.value("PRESSURE/Module", 0).toInt());
    ui->lineEdit_PressMechine->setText(SETTINGS.value("PRESSURE/PressMechine").toString());  //选择第几套治具
    ui->lineEdit_ButtonThreshold->setText(SETTINGS.value("PRESSURE/ButtonThreshold").toString());
    ui->lineEdit_ButtonThresholdCount->setText(SETTINGS.value("PRESSURE/ButtonThresholdCount").toString());
    ui->lineEdit_BrushThreshold->setText(SETTINGS.value("PRESSURE/BrushThreshold").toString());
    ui->lineEdit_BrushThresholdCount->setText(SETTINGS.value("PRESSURE/BrushThresholdCount").toString());

    ui->bthPressUpperLimitLineEdit->setText(SETTINGS.value("PRESSURE/bth_upper").toString());
    ui->bthPressLowerLimitLineEdit->setText(SETTINGS.value("PRESSURE/bth_lower").toString());

    ui->modelPressUpperLimitLineEdit->setText(SETTINGS.value("PRESSURE/model_button_upper").toString());
    ui->modelPressLowerLimitLineEdit->setText(SETTINGS.value("PRESSURE/model_button_lower").toString());

    ui->powerPressUpperLimitLineEdit->setText(SETTINGS.value("PRESSURE/power_button_upper").toString());
    ui->powerPressLowerLimitLineEdit->setText(SETTINGS.value("PRESSURE/power_button_lower").toString());

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

    ui->checkBox_ProductName->setChecked(SETTINGS.value("ProductInfo/ProductName_checkBox").toBool());
    ui->checkBox_HardwareVersion->setChecked(SETTINGS.value("ProductInfo/HardwareVersion_checkBox").toBool());
    ui->checkBox_SoftwareVersion->setChecked(SETTINGS.value("ProductInfo/SoftwareVersion_checkBox").toBool());
    ui->checkBox_ResourceVersion->setChecked(SETTINGS.value("ProductInfo/ResourceVersion_checkBox").toBool());
    ui->checkBox_AgingStatus->setChecked(SETTINGS.value("ProductInfo/AgingStatus_checkBox").toBool());
    ui->checkBox_MotorVersion->setChecked(SETTINGS.value("ProductInfo/MotorVersion_checkBox").toBool());
    ui->checkBox_BluetoothVersion->setChecked(SETTINGS.value("ProductInfo/BluetoothVersion_checkBox").toBool());
    ui->checkBox_AppPB->setChecked(SETTINGS.value("ProductInfo/AppPB_checkBox").toBool());
    ui->checkBox_FactoryPB->setChecked(SETTINGS.value("ProductInfo/FactoryPB_checkBox").toBool());
    ui->checkBox_AlgorithmVersion->setChecked(SETTINGS.value("ProductInfo/AlgorithmVersion_checkBox").toBool());
    ui->checkBox_PressureVersion->setChecked(SETTINGS.value("ProductInfo/PressureVersion_checkBox").toBool());
    ui->checkBox_ImuID->setChecked(SETTINGS.value("ProductInfo/ImuID_checkBox").toBool());
    ui->checkBox_CameraID->setChecked(SETTINGS.value("ProductInfo/CameraID_checkBox").toBool());

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

    ui->checkBox_imu_status->setChecked(SETTINGS.value("PeripheralStatus/IMUStatus_checkBox").toBool());
    ui->checkBox_flash_status->setChecked(SETTINGS.value("PeripheralStatus/FlashStatus_checkBox").toBool());
    ui->checkBox_magnetic_status->setChecked(SETTINGS.value("PeripheralStatus/MagneticStatus_checkBox").toBool());
    ui->checkBox_pressure_status->setChecked(SETTINGS.value("PeripheralStatus/PressureStatus_checkBox").toBool());
    ui->checkBox_audio_status->setChecked(SETTINGS.value("PeripheralStatus/AudioStatus_checkBox").toBool());

    // Battery Control
    ui->lineEdit_battery_voltage_control->setText(SETTINGS.value("BATTARY/standbattary").toString());  // 电池电压

    // Camera Position
    ui->lineEdit_camera_x->setText(SETTINGS.value("CAMERA/Rect1_X").toString());  // 摄像头偏位标准x坐标
    ui->lineEdit_camera_y->setText(SETTINGS.value("CAMERA/Rect1_Y").toString());  // 摄像头偏位标准y坐标
    ui->lineEdit_camera_width->setText(SETTINGS.value("CAMERA/Rect1_Width").toString());  // 摄像头偏位标准矩形宽度
    ui->lineEdit_camera_height->setText(SETTINGS.value("CAMERA/Rect1_Height").toString());  // 摄像头偏位标准矩形高度
    ui->lineEdit_image_interval->setText(SETTINGS.value("CAMERA/CameraGetTime").toString());  // 重新获取图片时间间隔
    ui->lineEdit_start_dirty_time->setText(SETTINGS.value("CAMERA/startDirtyTime").toString());

    // MES 相关
    ui->comboBox_factory->setCurrentText(SETTINGS.value("MES/factory").toString());
    ui->comboBox_productName->setCurrentText(SETTINGS.value("MES/Product_Name").toString());
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
qsetting::~qsetting() {
    qDebug() << "已经删除页面";
    delete ui;
}

void qsetting::saveSubPIDAndFilter() {
    // 获取 QLineEdit 控件中的文本内容
    QString subPID_00 = ui->lineEdit_SubPID_00->text();
    QString subPID_01 = ui->lineEdit_SubPID_01->text();
    QString subPID_02 = ui->lineEdit_SubPID_02->text();
    QString subPID_03 = ui->lineEdit_SubPID_03->text();

    // 输出当前内容
    // qDebug() << "subPID_00 (from UI): " << subPID_00;
    // qDebug() << "subPID_01 (from UI): " << subPID_01;
    // qDebug() << "subPID_02 (from UI): " << subPID_02;
    // qDebug() << "subPID_03 (from UI): " << subPID_03;

    // 进入 SUBPID 组
    SETTINGS.beginGroup("SUBPID");

    // 清空原有的配置
    for (const QString& key : SETTINGS.childKeys()) {
        SETTINGS.remove(key);
    }

    // 解析并保存每个 subPID 组的键值对
    // 将 subPID_00 中的键值对存入文件
    QStringList keys_00 = subPID_00.split("=");
    for (const QString& key : keys_00) {
        SETTINGS.setValue(key, "00");
    }

    // 将 subPID_01 中的键值对存入文件
    QStringList keys_01 = subPID_01.split("=");
    for (const QString& key : keys_01) {
        SETTINGS.setValue(key, "01");
    }

    // 将 subPID_02 中的键值对存入文件
    QStringList keys_02 = subPID_02.split("=");
    for (const QString& key : keys_02) {
        SETTINGS.setValue(key, "02");
    }

    // 将 subPID_03 中的键值对存入文件
    QStringList keys_03 = subPID_03.split("=");
    for (const QString& key : keys_03) {
        SETTINGS.setValue(key, "03");
    }

    // 结束 SUBPID 组
    SETTINGS.endGroup();

    // 输出保存后的结果
    qDebug() << "Saved SUBPID data to file!";
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
                                        ui->radioButtonPressTest->isChecked()        ? "PRESS_TEST" :
                                        ui->radioButtonBoardFactoryTest->isChecked() ? "PCBA_TEST" :
                                        ui->radioButtonFreeWorkstation->isChecked()  ? "FREE_WORK" :
                                                                                       "MAIN_TEST");

    // 保存复选框状态到配置文件
    SETTINGS.setValue("SYSTEM/ShowLocalOTAFunc", ui->checkBox_ShowLocalOTAFunc->isChecked());
    SETTINGS.setValue("SYSTEM/ShowUpperComputerOTAFunc", ui->checkBox_ShowUpperComputerOTAFunc->isChecked());
    SETTINGS.setValue("SYSTEM/SaveToothbrushLog", ui->checkBox_SaveToothbrushLog->isChecked());
    SETTINGS.setValue("SYSTEM/LockProductUI", ui->checkBox_LockProductUI->isChecked());

    SETTINGS.setValue("SYSTEM/SimplePcbaTest", ui->checkBox_SimplePcbaTest->isChecked());
    SETTINGS.setValue("SYSTEM/NeedWriteSubpid", ui->checkBox_NeedWriteSubpid->isChecked());
    SETTINGS.setValue("SYSTEM/NeedWriteSkuid", ui->checkBox_NeedWriteSkuid->isChecked());

    SETTINGS.setValue("SYSTEM/BluetoothImageTransfer", ui->checkBox_BluetoothImageTransfer->isChecked());
    SETTINGS.setValue("SYSTEM/IMUCalibrationWakeup", ui->checkBox_IMUCalibrationWakeup->isChecked());
    SETTINGS.setValue("SYSTEM/DisableSerialPortRx", ui->checkBox_DisableSerialPortRx->isChecked());
    SETTINGS.setValue("SYSTEM/ShipModeResponse", ui->checkBox_ShipModeResponse->isChecked());
    SETTINGS.setValue("SYSTEM/SerialPortMAC", ui->checkBox_SerialPortMAC->isChecked());
    SETTINGS.setValue("SYSTEM/MagneticReuseMotorStatus", ui->checkBox_MagneticReuseMotorStatus->isChecked());
    SETTINGS.setValue("SYSTEM/TestAudioCurrent", ui->checkBox_TestAudioCurrent->isChecked());
    SETTINGS.setValue("SYSTEM/TestShippingCurrent", ui->checkBox_TestShippingCurrent->isChecked());
    SETTINGS.setValue("SYSTEM/PressIndependent", ui->checkBox_PressIndependent->isChecked());
    SETTINGS.setValue("SYSTEM/PressWindow", ui->checkBox_PressWindow->isChecked());

    SETTINGS.setValue("SYSTEM/SendMotorCalibration", ui->checkBox_SendMotorCalibration->isChecked());
    SETTINGS.setValue("SYSTEM/LightTest", ui->checkBox_LightTest->isChecked());
    SETTINGS.setValue("SYSTEM/ServoMotorStart", ui->checkBox_ServoMotorStart->isChecked());
    SETTINGS.setValue("SYSTEM/uperMotor", ui->checkBox_uperMotor->isChecked());

    SETTINGS.setValue("SYSTEM/TestWifiSignal", ui->checkBox_TestWifiSignal->isChecked());
    SETTINGS.setValue("SYSTEM/IMULastEnterStartTest", ui->checkBox_IMULastEnterStartTest->isChecked());
    SETTINGS.setValue("SYSTEM/CurrentMechine", ui->lineEdit_CurrentMechine->text());

    // 保存 SN
    SETTINGS.setValue("Regex/SNPattern", ui->snLineEdit->text());
    SETTINGS.setValue("Music/MusicState", ui->lineEdit_music_state->text());
    SETTINGS.setValue("Music/MusicState_checkBox", ui->checkBox_MusicState->isChecked());

    saveSubPIDAndFilter();
    // 保存 WIFI 信息
    SETTINGS.setValue("WIFI/Name", ui->wifiAccountLineEdit->text());
    SETTINGS.setValue("WIFI/Password", ui->wifiPasswordLineEdit->text());
    SETTINGS.setValue("WIFI/HighRssi", ui->wifiUpperLimitLineEdit->text());
    SETTINGS.setValue("WIFI/LowRssi", ui->wifiLowerLimitLineEdit->text());
    SETTINGS.setValue("WIFI/IP", ui->wifiIPLineEdit->text());

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

    // 加载压感参数

    SETTINGS.setValue("PRESSURE/TestTime", ui->lineEdit_testWaitTime->text());
    SETTINGS.setValue("PRESSURE/functionSwitch", ui->comboBox_pressFunctionSwitch->currentIndex());
    SETTINGS.setValue("PRESSURE/Use_graph", ui->comboBox_displayImageType->currentIndex());
    SETTINGS.setValue("PRESSURE/Module", ui->comboBox_individualMode->currentIndex());

    SETTINGS.setValue("PRESSURE/PressMechine", ui->lineEdit_PressMechine->text());
    SETTINGS.setValue("PRESSURE/ButtonThreshold", ui->lineEdit_ButtonThreshold->text());
    SETTINGS.setValue("PRESSURE/ButtonThresholdCount", ui->lineEdit_ButtonThresholdCount->text());
    SETTINGS.setValue("PRESSURE/BrushThreshold", ui->lineEdit_BrushThreshold->text());
    SETTINGS.setValue("PRESSURE/BrushThresholdCount", ui->lineEdit_BrushThresholdCount->text());

    SETTINGS.setValue("PRESSURE/bth_upper", ui->bthPressUpperLimitLineEdit->text());
    SETTINGS.setValue("PRESSURE/bth_lower", ui->bthPressLowerLimitLineEdit->text());

    SETTINGS.setValue("PRESSURE/model_button_upper", ui->modelPressUpperLimitLineEdit->text());
    SETTINGS.setValue("PRESSURE/model_button_lower", ui->modelPressLowerLimitLineEdit->text());

    SETTINGS.setValue("PRESSURE/power_button_upper", ui->powerPressUpperLimitLineEdit->text());
    SETTINGS.setValue("PRESSURE/power_button_lower", ui->powerPressLowerLimitLineEdit->text());

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

    SETTINGS.setValue("ProductInfo/ProductName_checkBox", ui->checkBox_ProductName->isChecked());
    SETTINGS.setValue("ProductInfo/HardwareVersion_checkBox", ui->checkBox_HardwareVersion->isChecked());
    SETTINGS.setValue("ProductInfo/SoftwareVersion_checkBox", ui->checkBox_SoftwareVersion->isChecked());
    SETTINGS.setValue("ProductInfo/ResourceVersion_checkBox", ui->checkBox_ResourceVersion->isChecked());
    SETTINGS.setValue("ProductInfo/AgingStatus_checkBox", ui->checkBox_AgingStatus->isChecked());
    SETTINGS.setValue("ProductInfo/MotorVersion_checkBox", ui->checkBox_MotorVersion->isChecked());
    SETTINGS.setValue("ProductInfo/BluetoothVersion_checkBox", ui->checkBox_BluetoothVersion->isChecked());
    SETTINGS.setValue("ProductInfo/AppPB_checkBox", ui->checkBox_AppPB->isChecked());
    SETTINGS.setValue("ProductInfo/FactoryPB_checkBox", ui->checkBox_FactoryPB->isChecked());
    SETTINGS.setValue("ProductInfo/AlgorithmVersion_checkBox", ui->checkBox_AlgorithmVersion->isChecked());
    SETTINGS.setValue("ProductInfo/PressureVersion_checkBox", ui->checkBox_PressureVersion->isChecked());
    SETTINGS.setValue("ProductInfo/ImuID_checkBox", ui->checkBox_ImuID->isChecked());
    SETTINGS.setValue("ProductInfo/CameraID_checkBox", ui->checkBox_CameraID->isChecked());

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

    // 检查 QCheckBox 的状态并保存到配置文件
    SETTINGS.setValue("PeripheralStatus/IMUStatus_checkBox", ui->checkBox_imu_status->isChecked());
    SETTINGS.setValue("PeripheralStatus/FlashStatus_checkBox", ui->checkBox_flash_status->isChecked());
    SETTINGS.setValue("PeripheralStatus/MagneticStatus_checkBox", ui->checkBox_magnetic_status->isChecked());
    SETTINGS.setValue("PeripheralStatus/PressureStatus_checkBox", ui->checkBox_pressure_status->isChecked());
    SETTINGS.setValue("PeripheralStatus/AudioStatus_checkBox", ui->checkBox_audio_status->isChecked());

    // Battery Control
    SETTINGS.setValue("BATTARY/standbattary", ui->lineEdit_battery_voltage_control->text());

    // Camera Position
    SETTINGS.setValue("CAMERA/Rect1_X", ui->lineEdit_camera_x->text());
    SETTINGS.setValue("CAMERA/Rect1_Y", ui->lineEdit_camera_y->text());
    SETTINGS.setValue("CAMERA/Rect1_Width", ui->lineEdit_camera_width->text());
    SETTINGS.setValue("CAMERA/Rect1_Height", ui->lineEdit_camera_height->text());
    SETTINGS.setValue("CAMERA/CameraGetTime", ui->lineEdit_image_interval->text());
    SETTINGS.setValue("CAMERA/startDirtyTime", ui->lineEdit_start_dirty_time->text());

    // MES 相关
    SETTINGS.setValue("MES/factory", ui->comboBox_factory->currentText());
    SETTINGS.setValue("MES/Product_Name", ui->comboBox_productName->currentText());
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
    event->accept();
}

void qsetting::RestoreProductDefaultSetting() {
    ui->checkBox_PressIndependent->setChecked(true);  //默认所有产品都直接开始
    ui->checkBox_NeedWriteSkuid->setChecked(false);
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
    ui->checkBox_uperMotor->setChecked(false);
    ui->checkBox_PressWindow->setChecked(false);

    ui->checkBox_TestWifiSignal->setChecked(false);
    ui->checkBox_IMULastEnterStartTest->setChecked(false);

    //立讯的p20p要回车开始，木星是按键开始
    //立讯：imu需要晃动唤醒（加快蓝牙连接），全扫码再测试
    if (ui->comboBox_productName->currentText() == "U7") {
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
    if (ui->comboBox_productName->currentText() == "U7P") {
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
    if (ui->comboBox_productName->currentText() == "F20") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
    }
    //立讯：imu需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Q20P") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
    }
    //欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Q20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
    }
    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20P") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
    }
    if (ui->comboBox_productName->currentText() == "T10") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }

    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20PO") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
    }

    //欣旺达：依次扫标签码连接，需要唤醒
    if (ui->comboBox_productName->currentText() == "Y21") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    //华勤：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "P20P") {
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_MagneticReuseMotorStatus->setChecked(true);
        ui->checkBox_SendMotorCalibration->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_ServoMotorStart->setChecked(true);

        if (ui->comboBox_factory->currentText() == "lx")
            ui->checkBox_IMULastEnterStartTest->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试//原p30ps
    if (ui->comboBox_productName->currentText() == "Y25SE") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30PS") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }

    if (ui->comboBox_productName->currentText() == "P20PS") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_LightTest->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
    }
}

void qsetting::RestoreFacDefaultSetting() {
    // 隐藏 QLabel 和 QLineEdit 控件
    ui->label_weikesen_order->hide();
    ui->lineEdit_weikesen_order->hide();
    ui->label_78->hide();
    ui->lineEdit_mesUrl->hide();
    ui->label_74->hide();
    ui->lineEdit_macStation->hide();
    ui->label_79->hide();
    ui->lineEdit_processName->hide();
    ui->label_80->hide();
    ui->lineEdit_modelName->hide();
    ui->label_mac_field->hide();
    ui->lineEdit_mac_field->hide();
    ui->label_79->hide();
    ui->label_action_huaqin->hide();
    ui->lineEdit_action_huaqin->hide();
    ui->label_line_huaqin->hide();
    ui->lineEdit_line_huaqin->hide();
    ui->label_mes_login_account->hide();
    ui->lineEdit_mes_login_account->hide();
    ui->label_mes_login_password->hide();
    ui->lineEdit_mes_login_password->hide();
    ui->label_mes_operator->hide();
    ui->lineEdit_mes_operator->hide();
    ui->label_mes_login_station->hide();
    ui->lineEdit_mes_login_station->hide();

    if (ui->comboBox_factory->currentText() == "wks") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
    }
    if (ui->comboBox_factory->currentText() == "ydm") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_80->show();
        ui->lineEdit_modelName->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
    }
    if (ui->comboBox_factory->currentText() == "xwd") {
        ui->label_74->show();
        ui->lineEdit_macStation->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
        ui->label_mes_operator->show();
        ui->lineEdit_mes_operator->show();
        ui->label_mes_login_station->show();
        ui->lineEdit_mes_login_station->show();
    }
    if (ui->comboBox_factory->currentText() == "hq") {
        ui->label_action_huaqin->show();
        ui->lineEdit_action_huaqin->show();
        ui->label_line_huaqin->show();
        ui->lineEdit_line_huaqin->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
    }
    if (ui->comboBox_factory->currentText() == "lx") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_80->show();
        ui->lineEdit_modelName->show();
        ui->label_mac_field->show();
        ui->lineEdit_mac_field->show();
    }
    if (ui->comboBox_factory->currentText() == "无mes厂") {
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_74->show();
        ui->lineEdit_macStation->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_80->show();
        ui->lineEdit_modelName->show();
        ui->label_mac_field->show();
        ui->lineEdit_mac_field->show();
        ui->label_79->show();
        ui->label_action_huaqin->show();
        ui->lineEdit_action_huaqin->show();
        ui->label_line_huaqin->show();
        ui->lineEdit_line_huaqin->show();
        ui->label_mes_login_account->show();
        ui->lineEdit_mes_login_account->show();
        ui->label_mes_login_password->show();
        ui->lineEdit_mes_login_password->show();
        ui->label_mes_operator->show();
        ui->lineEdit_mes_operator->show();
        ui->label_mes_login_station->show();
        ui->lineEdit_mes_login_station->show();
    }
}
void qsetting::on_comboBox_productName_textActivated(const QString& arg1) {
    qDebug() << "选择的产品" << arg1;
    RestoreProductDefaultSetting();
}

void qsetting::on_comboBox_factory_textActivated(const QString& arg1) {
    qDebug() << "选择的工厂" << arg1;
    RestoreFacDefaultSetting();
}
