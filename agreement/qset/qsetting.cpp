#include "qsetting.h"

#include "qevent.h"
#include <algorithm>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QSet>
#include <QSignalBlocker>
#include <QString>
#include <QTabWidget>
#include <QVector>
#include "qpainter.h"
#include "ui_qsetting.h"
#include "bydmes.h"

struct FreeWorkTestCatalogItem {
    int id;
    QString name;
    QString categoryKey;
};

QVector<FreeWorkTestCatalogItem> getFreeWorkTestCatalog();

namespace {
constexpr int kRowSpacing = 10;
constexpr int kColumnSpacing = 10;
constexpr int kMargin = 10;

/** 可选区 Tab 顺序与标题（key 与 testFunction.cpp 中 freeWorkTestCategoryForItem 一致） */
const QVector<QPair<QString, QString>> kFreeWorkOptionalCategoryTabs = {
    {QStringLiteral("product"), QStringLiteral("产品功能")},
    {QStringLiteral("fixture"), QStringLiteral("治具功能")},
    {QStringLiteral("dongle"), QStringLiteral("dongle功能")},
    {QStringLiteral("cloud"), QStringLiteral("云端交互")},
};

QString categoryDisplayNameForKey(const QString& categoryKey) {
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        if (tab.first == categoryKey) {
            return tab.second;
        }
    }
    return categoryKey;
}

/** 按 objectName 批量设置悬停说明（未列出的控件保持无提示） */
void applyNamedTooltips(QWidget* root, const QVector<QPair<QString, QString>>& tips) {
    if (!root) {
        return;
    }
    for (const auto& item : tips) {
        if (QWidget* w = root->findChild<QWidget*>(item.first)) {
            w->setToolTip(item.second);
        }
    }
}
const QString kSelectedStationKey = "TestOrderMeta/SelectedStation";
const QString kSelectedStationNameKey = "TestOrderMeta/SelectedStationName";

struct StationItem {
    QString name;
    QString key;
};

const QVector<StationItem> kPresetStations = {
    {"组装电流测试工站", "ASSEMBLY_CURRENT_TEST"},
    {"老化测试工站", "AGING_TEST"},
    {"按键测试工站", "BUTTON_TEST"},
    {"屏幕测试工站", "SCREEN_TEST"},
    {"蓝牙测试工站", "BLUETOOTH_TEST"},
    {"吸力测试工站", "SUCTION_TEST"},
};

struct TupleEnvPreset {
    QString key;
    QString baseUrl;
};

const QVector<TupleEnvPreset> kTupleEnvPresets = {
    {QStringLiteral("dev"), QStringLiteral("http://192.168.200.140:8080")},
    {QStringLiteral("prod"), QStringLiteral("https://lute-aiot-mac.luteos.com")},
    {QStringLiteral("build-dev-inner"), QStringLiteral("http://192.168.200.140:8080")},
    {QStringLiteral("build-dev"), QStringLiteral("https://983ug2va5885.vicp.fun")},
    {QStringLiteral("build-prod"), QStringLiteral("https://lute-aiot-mac.luteos.com")},
};

const QString kTupleEnvCustomKey = QStringLiteral("custom");

QString normalizeTupleBaseUrl(const QString& u) {
    QString s = u.trimmed();
    while (s.endsWith('/')) {
        s.chop(1);
    }
    return s;
}

int tupleEnvIndexForKey(QComboBox* combo, const QString& key) {
    if (!combo) {
        return -1;
    }
    for (int i = 0; i < combo->count(); ++i) {
        if (combo->itemData(i).toString() == key) {
            return i;
        }
    }
    return -1;
}

int tupleEnvIndexForUrl(QComboBox* combo, const QString& url) {
    const QString n = normalizeTupleBaseUrl(url);
    for (const auto& p : kTupleEnvPresets) {
        if (normalizeTupleBaseUrl(p.baseUrl) == n) {
            return tupleEnvIndexForKey(combo, p.key);
        }
    }
    return -1;
}

QString tupleBaseUrlForKey(const QString& key) {
    for (const auto& p : kTupleEnvPresets) {
        if (p.key == key) {
            return p.baseUrl;
        }
    }
    return {};
}

QString orderGroupName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    return key.isEmpty() ? "TestOrder_default" : QString("TestOrder_%1").arg(key);
}

QString stationKeyFromDisplayName(const QString& name) {
    const QString trimmedName = name.trimmed();
    for (const auto& item : kPresetStations) {
        if (item.name == trimmedName) {
            return item.key;
        }
    }
    return trimmedName;
}

QString stationDisplayNameFromKey(const QString& key) {
    const QString trimmedKey = key.trimmed();
    for (const auto& item : kPresetStations) {
        if (item.key == trimmedKey) {
            return item.name;
        }
    }
    return trimmedKey;
}

}

qsetting::qsetting(QWidget* parent) : QWidget(parent), ui(new Ui::qsetting) {
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    setAcceptDrops(true);
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
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonKeyTest"), 11);
    StationGroup->addButton(findChild<QRadioButton*>("radioButtonSuctionTest"), 12);

    // 如果需要从某个数据源添加项，可以使用循环来添加
    QStringList productList = {"V3", "V3Pro"};
    ui->comboBox_productName->addItems(productList);

    QStringList pressFunctionSwitch = {"无效选项", "单校准", "单测试", "校准加测试"};
    ui->comboBox_pressFunctionSwitch->addItems(pressFunctionSwitch);

    QStringList individualMode = {"无效选项", "单独电机", "单独按键", "都校准"};
    ui->comboBox_individualMode->addItems(individualMode);

    QStringList displayImageType = {"无效选项", "关闭所有图像", "ADC图像",     "压力值图像",
                                    "电机图像", "按键图像",     "显示全部图像"};
    ui->comboBox_displayImageType->addItems(displayImageType);

    QStringList factoryList = {"lx", "xwd", "hq", "wks", "ydm", "byd", "无mes厂"};
    ui->comboBox_factory->addItems(factoryList);

    initTupleEnvironmentCombo();
    loadConfig();
    originalStation_ = SETTINGS.value("SYSTEM/station").toString();
    connect(StationGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, [this](int) {
        const QString oldStation = originalStation_;
        saveCurrentTestOrder();
        saveConfig();
        SETTINGS.sync();
        const QString newStation = SETTINGS.value("SYSTEM/station").toString();
        if (oldStation.isEmpty() || oldStation == newStation) {
            return;
        }
        originalStation_ = newStation;
        stationReloading_ = true;
        qApp->setProperty("StationRestartRequested", true);
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget* widget : widgets) {
            widget->close();
        }
        QCoreApplication::exit(0);
    });
    initSettingTooltips();
    initFreeWorkTestOrderUi();
    RestoreFacDefaultSetting();
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页


    ui->groupBox_SubPIDSettings->hide();
}

void qsetting::initSettingTooltips() {
    setToolTip(QStringLiteral("上位机全局参数；修改工站类型后需重启程序生效。保存后写入「上位机设置.ini」。"));
    const int tabFixture = ui->tabWidget->indexOf(ui->tab_fixture_setting);
    const int tabKey = ui->tabWidget->indexOf(ui->tab_key_setting);
    const int tabDetail = ui->tabWidget->indexOf(ui->tab_2);
    const int tabFlow = ui->tabWidget->indexOf(ui->tab_3);
    if (tabFixture >= 0) {
        ui->tabWidget->setTabToolTip(tabFixture, QStringLiteral("治具/压感/外设/摄像头及治具测试要求等。"));
    }
    if (tabKey >= 0) {
        ui->tabWidget->setTabToolTip(tabKey, QStringLiteral("MES、工站、电流/IMU/信号、基础信息与产品差异化等。"));
    }
    if (tabDetail >= 0) {
        ui->tabWidget->setTabToolTip(tabDetail, QStringLiteral("老化、三元组、按键 KeyId、PLC、串口仪器、协议类型等工站专用项。"));
    }
    if (tabFlow >= 0) {
        ui->tabWidget->setTabToolTip(tabFlow, QStringLiteral("自由工站测试流程：左侧为执行顺序，右侧按分类勾选后拖入配置区。"));
    }

    static const QVector<QPair<QString, QString>> kTips = {
        // —— 分区 GroupBox ——
        {QStringLiteral("groupBox_3"), QStringLiteral("MES：工厂、产品名、工站、URL 等；工厂下拉联动显示字段。")},
        {QStringLiteral("radioButtonGroupWidget"), QStringLiteral("工站类型 SYSTEM/station；切换后重启程序。")},
        {QStringLiteral("groupBox_2"), QStringLiteral("1 拖多：主界面分窗行/列（User/formRow、formColumn）。")},
        {QStringLiteral("groupBox_4"), QStringLiteral("电流阈值：船运/充电/工作/静态/音频（Current/*）。")},
        {QStringLiteral("groupBox_15"), QStringLiteral("外设状态：期望值 +「启用」；读回与期望值比对。")},
        {QStringLiteral("groupBox_TestRequirements"), QStringLiteral("治具状态期望值（FIXTEST/*）：音乐、过压灯、按键等。")},
        {QStringLiteral("groupBox_camera_position"), QStringLiteral("摄像头 ROI、脏污检测延时等。")},
        {QStringLiteral("imuCalibrationWidget"), QStringLiteral("IMU 校准时间、轴限、静态收敛参数（IMU/*）。")},
        {QStringLiteral("signalStrengthSettingWidget"), QStringLiteral("WiFi/BLE RSSI 卡控与测试次数（WIFI/*、BLE/*）。")},
        {QStringLiteral("pressureSettingsWidget"), QStringLiteral("压感测试/校准、治具套号、阈值（PRESSURE/*）。")},
        {QStringLiteral("mainWidget"), QStringLiteral("基础信息期望值；勾选后参与 compareVersions 比对。")},
        {QStringLiteral("groupBox_1"), QStringLiteral("产品差异化流程开关（SYSTEM/*），按产品可 Restore 默认。")},
        {QStringLiteral("groupBox_SubPIDSettings"), QStringLiteral("SUBPID 规则（SUBPID/*）。")},
        {QStringLiteral("groupBox_ageingConfig"), QStringLiteral("老化 AGING/BurningMode、BurningSeconds。")},
        {QStringLiteral("groupBox_tupleConfig"), QStringLiteral("三元组 Tuple/*：环境、URL、鉴权、SKU。")},
        {QStringLiteral("groupBox_keyConfig"), QStringLiteral("按键 KeyId（ProductInfo/KeyId*）。")},
        {QStringLiteral("groupBox_plcModbusDebug"), QStringLiteral("PLC Modbus（PLC/*），治具 PLC 步骤。")},
        {QStringLiteral("groupBox_brushInstrument"),
         QStringLiteral(
             "BrushInstrument/* 与并联 CMW：BleBrushCmwConcurrent、BleBrushCmwOnStopPer、BlePer/Cmw*（含 CmwQueryCurrentArbFile 查询 SOURce:GPRF:GEN:ARB:FILE?；另见 CmwBurstPollArbScount…、CmwVisaTrace、CmwWaitArbScount）。")},
        {QStringLiteral("groupBox_systemProtocol"), QStringLiteral("SYSTEM/ProtocolType：qpb 或 qfctp。")},
        {QStringLiteral("config"), QStringLiteral("已选步骤，自上而下执行；可拖拽排序。")},
        {QStringLiteral("can_use"), QStringLiteral("可选步骤，勾选后拖入左侧配置区。")},
        {QStringLiteral("freeWorkOptionalTabs"), QStringLiteral("分类：产品 / 治具 / dongle / 云端。")},
        // —— 常用控件 ——
        {QStringLiteral("comboBox_factory"), QStringLiteral("MES 工厂（lx/byd/wks/无mes厂等）。")},
        {QStringLiteral("comboBox_productName"), QStringLiteral("产品型号 MES/Product_Name；切换恢复默认差异化。")},
        {QStringLiteral("lineEdit_station"), QStringLiteral("MES 工站。")},
        {QStringLiteral("lineEdit_mes_config_file_path"), QStringLiteral("MES/ConfigFilePath。")},
        {QStringLiteral("pushButton_mesConfigFileBrowse"), QStringLiteral("浏览 MES 配置文件。")},
        {QStringLiteral("comboBox_testOrderStation"), QStringLiteral("TestOrderMeta/SelectedStation。")},
        {QStringLiteral("pushButton_clearConfiguredTestOrder"), QStringLiteral("清空当前工站配置区步骤。")},
        {QStringLiteral("comboBox_systemProtocolType"), QStringLiteral("设备协议 qpb / qfctp。")},
        {QStringLiteral("comboBox_tupleEnvironment"), QStringLiteral("三元组环境预设。")},
        {QStringLiteral("lineEdit_tupleBaseUrl"), QStringLiteral("Tuple/BaseUrl。")},
        {QStringLiteral("rowLineEdit"), QStringLiteral("User/formRow。")},
        {QStringLiteral("columnLineEdit"), QStringLiteral("User/formColumn。")},
        {QStringLiteral("snLineEdit"), QStringLiteral("Regex/SNPattern。")},
        {QStringLiteral("wifiAccountLineEdit"), QStringLiteral("WIFI/Name。")},
        {QStringLiteral("wifiPasswordLineEdit"), QStringLiteral("WIFI/Password。")},
        {QStringLiteral("wifiUpperLimitLineEdit"), QStringLiteral("WIFI/HighRssi。")},
        {QStringLiteral("wifiLowerLimitLineEdit"), QStringLiteral("WIFI/LowRssi。")},
        {QStringLiteral("wifiIPLineEdit"), QStringLiteral("WIFI/IP。")},
        {QStringLiteral("bluetoothUpperLimitLineEdit"), QStringLiteral("BLE/HighRssi。")},
        {QStringLiteral("bluetoothLowerLimitLineEdit"), QStringLiteral("BLE/LowRssi。")},
        {QStringLiteral("signalTestCountLineEdit"), QStringLiteral("BLE/RssiCount。")},
        {QStringLiteral("lineEdit_CargoCurrentUpper"), QStringLiteral("Current/HighshipCurrent。")},
        {QStringLiteral("lineEdit_CargoCurrentLower"), QStringLiteral("Current/LowshipCurrent。")},
        {QStringLiteral("lineEdit_ChargingCurrentUpper"), QStringLiteral("Current/HighCharCurrent。")},
        {QStringLiteral("lineEdit_ChargingCurrentLower"), QStringLiteral("Current/LowCharCurrent。")},
        {QStringLiteral("lineEdit_WorkingCurrentUpper"), QStringLiteral("Current/HighworkCurrent。")},
        {QStringLiteral("lineEdit_WorkingCurrentLower"), QStringLiteral("Current/LowworkCurrent。")},
        {QStringLiteral("lineEdit_StaticCurrentUpper"), QStringLiteral("Current/HighStaticCurrent。")},
        {QStringLiteral("lineEdit_StaticCurrentLower"), QStringLiteral("Current/LowStaticCurrent。")},
        {QStringLiteral("lineEdit_AudioCurrentUpper"), QStringLiteral("Current/HighAudioCurrent。")},
        {QStringLiteral("lineEdit_AudioCurrentLower"), QStringLiteral("Current/LowAudioCurrent。")},
        {QStringLiteral("lineEdit_imu_status"), QStringLiteral("外设 imu 状态期望值。")},
        {QStringLiteral("checkBox_imu_status"), QStringLiteral("启用 imu 状态比对。")},
        {QStringLiteral("lineEdit_ageingBurningMode"), QStringLiteral("AGING/BurningMode。")},
        {QStringLiteral("lineEdit_ageingBurningSeconds"), QStringLiteral("AGING/BurningSeconds（秒）。")},
        {QStringLiteral("lineEdit_brushInstrumentSendPacketCount"), QStringLiteral("BrushInstrument/InstrumentSendPacketCount。")},
        {QStringLiteral("lineEdit_brushInstrumentMaxPer"), QStringLiteral("BrushInstrument/MaxPer。")},
        {QStringLiteral("lineEdit_brushInstrumentPacketPhaseWaitMs"), QStringLiteral("BrushInstrument/PacketPhaseWaitMs。")},
        {QStringLiteral("lineEdit_brushInstrumentStopAckTimeoutMs"), QStringLiteral("BrushInstrument/StopAckTimeoutMs。")},
        {QStringLiteral("checkBox_freeInstrumentBleBrushCmwConcurrent"),
         QStringLiteral(
             "FreeInstrument/BleBrushCmwConcurrent：启用 CMW GPRF；“停止接收/PER…”项见另一勾选（BleBrushCmwOnStopPer）。需 BlePer/CmwVisaAddress；可选 BlePer/CmwVisaTrace；BlePer/CmwQueryCurrentArbFile（默认 true，查询 SOURce:GPRF:GEN:ARB:FILE? 打日志）；BlePer/CmwBurstPollArbScount、BlePer/CmwWaitArbScount；详见 docs/cmw100rx第四节。")},
        {QStringLiteral("checkBox_freeInstrumentBleBrushCmwOnStopPer"),
         QStringLiteral(
             "FreeInstrument/BleBrushCmwOnStopPer（默认勾选）：勾选时 PER 步内按上个「开始接收」profile 再打一发；“并联CMW播放Profile0～5”六个前序步后若不想重复触发可取消勾选。")},
        {QStringLiteral("lineEdit_plcSwitchDoneResetM"),
         QStringLiteral("PLC/SwitchTestDoneResetM：旋钮测试完成复位线圈 M 默认值（211）；双工位时工位2可单独设 ini 键 SwitchTestDoneResetM_Station2（如231），工位N 用 SwitchTestDoneResetM_StationN，有则优先于本项。")},
        {QStringLiteral("lineEdit_plcSwitchDoneResetPulseMs"), QStringLiteral("PLC/SwitchTestDoneResetPulseMs：复位脉冲毫秒；>0 时置1后经此时长再置0，0 表示仅置1由 PLC 清零。")},
        {QStringLiteral("lineEdit_plcIpAddress"), QStringLiteral("PLC/IpAddress。")},
        {QStringLiteral("lineEdit_plcPort"), QStringLiteral("PLC/Port。")},
        {QStringLiteral("radioButtonDebug"), QStringLiteral("MAIN_TEST 调试上位机。")},
        {QStringLiteral("radioButtonFreeWorkstation"), QStringLiteral("FREE_WORK 自由工站。")},
        {QStringLiteral("radioButtonStaticCurrent"), QStringLiteral("QUIESCENT_CURRENT。")},
        {QStringLiteral("radioButtonMotorCalibration"), QStringLiteral("MOTOR_TEST。")},
        {QStringLiteral("radioButtonImuCalibration"), QStringLiteral("IMU_CALI。")},
        {QStringLiteral("radioButtonScreenTest"), QStringLiteral("SCREEN_TEST。")},
        {QStringLiteral("radioButtonCameraTest"), QStringLiteral("CAMERA_TEST。")},
        {QStringLiteral("radioButtonSignalTest"), QStringLiteral("WIFIBLE_TEST。")},
        {QStringLiteral("radioButtonAgingTest"), QStringLiteral("AGE_TEST。")},
        {QStringLiteral("radioButtonPressTest"), QStringLiteral("PRESS_TEST。")},
        {QStringLiteral("radioButtonBoardFactoryTest"), QStringLiteral("PCBA_TEST。")},
        {QStringLiteral("radioButtonKeyTest"), QStringLiteral("KEY_TEST。")},
        {QStringLiteral("radioButtonSuctionTest"), QStringLiteral("SUCTION_TEST。")},
        {QStringLiteral("checkBox_NeedWriteSubpid"), QStringLiteral("SYSTEM/NeedWriteSubpid。")},
        {QStringLiteral("checkBox_NeedWriteSkuid"), QStringLiteral("SYSTEM/NeedWriteSkuid。")},
        {QStringLiteral("checkBox_TestWifiSignal"), QStringLiteral("SYSTEM/TestWifiSignal。")},
        {QStringLiteral("checkBox_IMUCalibrationWakeup"), QStringLiteral("SYSTEM/IMUCalibrationWakeup。")},
        {QStringLiteral("checkBox_SerialPortMAC"), QStringLiteral("SYSTEM/SerialPortMAC。")},
        {QStringLiteral("checkBox_TestShippingCurrent"), QStringLiteral("SYSTEM/TestShippingCurrent。")},
        {QStringLiteral("checkBox_ShowLocalOTAFunc"), QStringLiteral("SYSTEM/ShowLocalOTAFunc。")},
        {QStringLiteral("checkBox_ShowUpperComputerOTAFunc"), QStringLiteral("SYSTEM/ShowUpperComputerOTAFunc。")},
        {QStringLiteral("checkBox_SaveToothbrushLog"), QStringLiteral("SYSTEM/SaveToothbrushLog。")},
        {QStringLiteral("checkBox_LockProductUI"), QStringLiteral("SYSTEM/LockProductUI。")},
        {QStringLiteral("checkBox_SimplePcbaTest"), QStringLiteral("SYSTEM/SimplePcbaTest。")},
        {QStringLiteral("checkBox_BluetoothImageTransfer"), QStringLiteral("SYSTEM/BluetoothImageTransfer。")},
        {QStringLiteral("checkBox_DisableSerialPortRx"), QStringLiteral("SYSTEM/DisableSerialPortRx。")},
        {QStringLiteral("checkBox_ShipModeResponse"), QStringLiteral("SYSTEM/ShipModeResponse。")},
        {QStringLiteral("checkBox_ProductName"), QStringLiteral("ProductInfo/ProductName_checkBox。")},
        {QStringLiteral("checkBox_SoftwareVersion"), QStringLiteral("ProductInfo/SoftwareVersion_checkBox。")},
        {QStringLiteral("checkBox_HardwareVersion"), QStringLiteral("ProductInfo/HardwareVersion_checkBox。")},
        {QStringLiteral("lineEdit_ProductName"), QStringLiteral("ProductInfo/Product_Name 期望值。")},
        {QStringLiteral("lineEdit_SoftwareVersion"), QStringLiteral("ProductInfo/Software_Version。")},
        {QStringLiteral("lineEdit_HardwareVersion"), QStringLiteral("ProductInfo/Hardware_Version。")},
        {QStringLiteral("configScrollArea"), QStringLiteral("配置区列表，内容多时可滚动。")},
        {QStringLiteral("optionalScroll_product"), QStringLiteral("产品功能类可选步骤。")},
        {QStringLiteral("optionalScroll_dongle"), QStringLiteral("dongle/蓝牙连接类可选步骤。")},
        {QStringLiteral("optionalScroll_fixture"), QStringLiteral("治具/PLC/串口仪器类可选步骤。")},
        {QStringLiteral("optionalScroll_cloud"), QStringLiteral("三元组云端类可选步骤。")},
    };
    applyNamedTooltips(this, kTips);

    // 外设区：行内标签与输入框同步提示
    const QVector<QPair<QString, QString>> kPeriphRowTips = {
        {QStringLiteral("lineEdit_flash_status"), QStringLiteral("内存/Flash 状态期望值。")},
        {QStringLiteral("lineEdit_magnetic_status"), QStringLiteral("地磁状态期望值。")},
        {QStringLiteral("lineEdit_pressure_status"), QStringLiteral("压感状态期望值。")},
        {QStringLiteral("lineEdit_audio_status"), QStringLiteral("功放状态期望值。")},
        {QStringLiteral("lineEdit_freework_press0_status"), QStringLiteral("自由工站压感0状态期望值。")},
        {QStringLiteral("lineEdit_freework_press1_status"), QStringLiteral("自由工站压感1状态期望值。")},
        {QStringLiteral("lineEdit_freework_battery_ic_status"), QStringLiteral("电池IC状态期望值。")},
        {QStringLiteral("lineEdit_freework_touch_ic_status"), QStringLiteral("触摸IC状态期望值。")},
        {QStringLiteral("lineEdit_freework_led_ic_status"), QStringLiteral("LED IC状态期望值。")},
        {QStringLiteral("lineEdit_freework_pd_ic_status"), QStringLiteral("PD IC状态期望值。")},
    };
    applyNamedTooltips(this, kPeriphRowTips);
}

void qsetting::initFreeWorkTestOrderUi() {
    freeWorkConfigLayout_ = qobject_cast<QVBoxLayout*>(ui->config_areas);
    if (!freeWorkConfigLayout_) {
        return;
    }

    // 可选区 Tab + 各页 ScrollArea 见 qsetting.ui；此处仅绑定网格并统一间距
    freeWorkOptionalLayouts_.clear();
    const QHash<QString, QGridLayout*> optionalGrids = {
        {QStringLiteral("product"), ui->optional_areas_product},
        {QStringLiteral("fixture"), ui->optional_areas_fixture},
        {QStringLiteral("dongle"), ui->optional_areas_dongle},
        {QStringLiteral("cloud"), ui->optional_areas_cloud},
    };
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        QGridLayout* grid = optionalGrids.value(tab.first);
        if (!grid) {
            continue;
        }
        grid->setVerticalSpacing(kRowSpacing);
        grid->setHorizontalSpacing(kColumnSpacing);
        grid->setContentsMargins(kMargin, kMargin, kMargin, kMargin);
        freeWorkOptionalLayouts_.insert(tab.first, grid);
    }

    const auto catalog = getFreeWorkTestCatalog();
    freeWorkRows_ = (catalog.size() + freeWorkCols_ - 1) / freeWorkCols_;
    freeWorkCheckBoxes_.clear();
    freeWorkCheckBoxes_.reserve(catalog.size());

    QHash<QString, int> optionalPosByCategory;
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        optionalPosByCategory.insert(tab.first, 0);
    }

    for (const auto& item : catalog) {
        auto* checkBox = new DraggableCheckBox(item.name, item.id, this);
        connect(checkBox, &QCheckBox::stateChanged, this, [this](int) {
            testOrderDirty_ = true;
            saveCurrentTestOrder();
        });
        checkBox->setToolTip(QStringLiteral("【%1】%2（id=%3）\n勾选后可拖入左侧「配置区域」，按自上而下顺序执行。")
                                 .arg(categoryDisplayNameForKey(item.categoryKey), item.name)
                                 .arg(item.id));
        freeWorkCheckBoxes_.append(checkBox);
        QString cat = item.categoryKey;
        if (!freeWorkOptionalLayouts_.contains(cat)) {
            cat = QStringLiteral("product");
        }
        QGridLayout* const grid = freeWorkOptionalLayouts_.value(cat);
        const int pos = optionalPosByCategory.value(cat);
        grid->addWidget(checkBox, pos / freeWorkCols_, pos % freeWorkCols_);
        optionalPosByCategory[cat] = pos + 1;
    }

    initTestOrderStationSelector();
    reorderFreeWorkCheckBoxes();
}

QString qsetting::categoryKeyForCheckBox(const DraggableCheckBox* checkBox) const {
    if (!checkBox) {
        return QStringLiteral("product");
    }
    const auto catalog = getFreeWorkTestCatalog();
    for (const auto& item : catalog) {
        if (item.id == checkBox->getIndex()) {
            return item.categoryKey;
        }
    }
    return QStringLiteral("product");
}

QGridLayout* qsetting::optionalLayoutForCategory(const QString& categoryKey) const {
    return freeWorkOptionalLayouts_.value(categoryKey, nullptr);
}

QGridLayout* qsetting::optionalLayoutAtGlobalPos(const QPoint& globalPos, QRect* areaOut) const {
    if (!ui->freeWorkOptionalTabs) {
        return nullptr;
    }
    for (int t = 0; t < ui->freeWorkOptionalTabs->count(); ++t) {
        QWidget* const pageHost = ui->freeWorkOptionalTabs->widget(t);
        if (!pageHost) {
            continue;
        }
        const QRect globalRect(pageHost->mapToGlobal(QPoint(0, 0)), pageHost->size());
        if (!globalRect.contains(globalPos)) {
            continue;
        }
        const QString key = kFreeWorkOptionalCategoryTabs.at(t).first;
        if (areaOut) {
            *areaOut = globalRect;
        }
        return freeWorkOptionalLayouts_.value(key, nullptr);
    }
    return nullptr;
}

void qsetting::removeCheckBoxFromAllOptionalLayouts(DraggableCheckBox* checkBox) {
    if (!checkBox) {
        return;
    }
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (layout && layout->indexOf(checkBox) >= 0) {
            layout->removeWidget(checkBox);
        }
    }
    if (freeWorkConfigLayout_ && freeWorkConfigLayout_->indexOf(checkBox) >= 0) {
        freeWorkConfigLayout_->removeWidget(checkBox);
    }
}

int qsetting::optionalWidgetCount(const QGridLayout* layout) const {
    if (!layout) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < layout->count(); ++i) {
        if (layout->itemAt(i) && layout->itemAt(i)->widget()) {
            ++count;
        }
    }
    return count;
}

int qsetting::optionalRowCount(const QGridLayout* layout) const {
    const int count = optionalWidgetCount(layout);
    return qMax(1, (count + freeWorkCols_ - 1) / freeWorkCols_);
}

void qsetting::initTupleEnvironmentCombo() {
    QComboBox* c = ui->comboBox_tupleEnvironment;
    if (!c) {
        return;
    }
    c->clear();
    for (const auto& p : kTupleEnvPresets) {
        c->addItem(p.key, p.key);
    }
    c->addItem(QStringLiteral("自定义"), kTupleEnvCustomKey);
    connect(c, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &qsetting::on_comboBox_tupleEnvironment_currentIndexChanged);
}

void qsetting::on_comboBox_tupleEnvironment_currentIndexChanged(int index) {
    Q_UNUSED(index);
    QComboBox* c = ui->comboBox_tupleEnvironment;
    if (!c) {
        return;
    }
    const QString key = c->currentData().toString();
    if (key == kTupleEnvCustomKey || key.isEmpty()) {
        return;
    }
    const QString url = tupleBaseUrlForKey(key);
    if (!url.isEmpty()) {
        ui->lineEdit_tupleBaseUrl->setText(url);
    }
}

void qsetting::initTestOrderStationSelector() {
    if (!ui->comboBox_testOrderStation) {
        return;
    }

    QVector<StationItem> stationCandidates = kPresetStations;
    auto appendCandidate = [&stationCandidates](const QString& rawValue) {
        const QString key = stationKeyFromDisplayName(rawValue);
        if (key.isEmpty()) {
            return;
        }
        stationCandidates.append({stationDisplayNameFromKey(key), key});
    };
    appendCandidate(SETTINGS.value(kSelectedStationKey).toString());
    appendCandidate(SETTINGS.value(kSelectedStationNameKey).toString());



    SETTINGS.beginGroup("TestOrder");
    const QStringList legacyGroupKeys = SETTINGS.childGroups();
    SETTINGS.endGroup();
    for (const QString& groupKey : legacyGroupKeys) {
        appendCandidate(groupKey);
    }

    const QStringList rootGroups = SETTINGS.childGroups();
    for (const QString& group : rootGroups) {
        if (group.startsWith("TestOrder_")) {
            appendCandidate(group.mid(QString("TestOrder_").size()));
        }
    }

    QSet<QString> uniqueStations;
    QVector<StationItem> stations;
    for (const auto& station : stationCandidates) {
        const QString key = station.key.trimmed();

        if (!key.isEmpty() && !uniqueStations.contains(key)) {
            uniqueStations.insert(key);
            stations.append({station.name.trimmed(), key});
        }
    }
    if (stations.isEmpty()) {
        stations.append({"default", "default"});
    }

    switchingTestOrderStation_ = true;
    ui->comboBox_testOrderStation->clear();
    for (const auto& station : stations) {
        ui->comboBox_testOrderStation->addItem(station.name, station.key);
    }

    activeTestOrderStation_ = stationKeyFromDisplayName(
        SETTINGS.value(kSelectedStationKey, SETTINGS.value("MES/test_station", "default")).toString());
    if (activeTestOrderStation_.isEmpty()) {
        activeTestOrderStation_ = "default";
    }
    int idx = ui->comboBox_testOrderStation->findData(activeTestOrderStation_);
    if (idx < 0) {
        idx = ui->comboBox_testOrderStation->findText(stationDisplayNameFromKey(activeTestOrderStation_));
    }
    if (idx >= 0) {
        ui->comboBox_testOrderStation->setCurrentIndex(idx);
    } else {
        ui->comboBox_testOrderStation->setCurrentIndex(0);
        activeTestOrderStation_ = ui->comboBox_testOrderStation->currentData().toString().trimmed();
    }
    switchingTestOrderStation_ = false;
    SETTINGS.setValue(kSelectedStationKey, activeTestOrderStation_);
    SETTINGS.setValue(kSelectedStationNameKey, stationDisplayNameFromKey(activeTestOrderStation_));
    applyStationUiState(activeTestOrderStation_);

}

void qsetting::applyStationUiState(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    if (ui->config) {
        ui->config->setTitle(QString("配置区域（%1）").arg(stationDisplayNameFromKey(key)));
    }
}

QString qsetting::currentTestOrderStation() const {
    if (!activeTestOrderStation_.isEmpty()) {
        return activeTestOrderStation_;
    }
    if (ui->comboBox_testOrderStation) {
        const QString currentStation = ui->comboBox_testOrderStation->currentData().toString().trimmed();
        if (!currentStation.isEmpty()) {
            return currentStation;
        }
    }
    return SETTINGS.value("MES/test_station", "default").toString().trimmed();
}

QVector<int> qsetting::loadTestOrderIndexes(const QString& station) const {
    QVector<int> indexes;
    auto loadIndexesFromGroup = [&indexes](const QString& groupPath) {
        SETTINGS.beginGroup(groupPath);
        QStringList keys = SETTINGS.childKeys();
        std::sort(keys.begin(), keys.end(), [](const QString& left, const QString& right) { return left.toInt() < right.toInt(); });
        for (const QString& key : keys) {
            indexes.append(SETTINGS.value(key).toInt());
        }
        SETTINGS.endGroup();
    };

    const QString trimmedStation = station.trimmed();
    if (!trimmedStation.isEmpty()) {
        loadIndexesFromGroup(orderGroupName(trimmedStation));
    }
    // 兼容旧版写法: [TestOrder] 下的 station\index
    if (indexes.isEmpty() && !trimmedStation.isEmpty()) {
        loadIndexesFromGroup(QString("TestOrder/%1").arg(trimmedStation));
    }
    // if (indexes.isEmpty()) {
    //     loadIndexesFromGroup("TestOrder");
    // }
    // if (indexes.isEmpty()) {
    //     loadIndexesFromGroup(orderGroupName("default"));
    // }
    // if (indexes.isEmpty()) {
    //     loadIndexesFromGroup("TestOrder/default");
    // }
    return indexes;
}

void qsetting::saveTestOrderIndexes(const QString& station, const QVector<int>& indexes) const {
    QString groupPath = "TestOrder";
    const QString trimmedStation = station.trimmed();
    if (!trimmedStation.isEmpty()) {
        groupPath = orderGroupName(trimmedStation);
    }
    SETTINGS.beginGroup(groupPath);
    SETTINGS.remove("");
    for (int i = 0; i < indexes.size(); ++i) {
        SETTINGS.setValue(QString::number(i), indexes.at(i));
    }
    SETTINGS.endGroup();
}

DraggableCheckBox* qsetting::getConfiguredCheckBoxByIndex(int index) const {
    if (!freeWorkConfigLayout_) {
        return nullptr;
    }
    for (int i = 0; i < freeWorkConfigLayout_->count(); ++i) {
        auto* checkBox = qobject_cast<DraggableCheckBox*>(freeWorkConfigLayout_->itemAt(i)->widget());
        if (checkBox && checkBox->getIndex() == index) {
            return checkBox;
        }
    }
    return nullptr;
}

DraggableCheckBox* qsetting::getOptionalCheckBoxByIndex(int index) const {
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (!layout) {
            continue;
        }
        for (int i = 0; i < layout->count(); ++i) {
            auto* checkBox = qobject_cast<DraggableCheckBox*>(layout->itemAt(i)->widget());
            if (checkBox && checkBox->getIndex() == index) {
                return checkBox;
            }
        }
    }
    return nullptr;
}

void qsetting::reorderFreeWorkCheckBoxes() {
    if (!freeWorkConfigLayout_ || freeWorkOptionalLayouts_.isEmpty()) {
        qDebug() << "[TestOrder] reorder skipped, layout null";
        return;
    }

    const QString stationKey = currentTestOrderStation();
    const QVector<int> indexes = loadTestOrderIndexes(stationKey);
    qDebug() << "[TestOrder] reorder begin, station =" << stationKey << ", indexes =" << indexes;

    while (QLayoutItem* item = freeWorkConfigLayout_->takeAt(0)) {
        delete item;
    }
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (!layout) {
            continue;
        }
        while (QLayoutItem* item = layout->takeAt(0)) {
            delete item;
        }
    }

    QVector<QPair<DraggableCheckBox*, bool>> blockedStates;
    blockedStates.reserve(freeWorkCheckBoxes_.size());
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (checkBox) {
            blockedStates.append(qMakePair(checkBox, checkBox->blockSignals(true)));
            removeCheckBoxFromAllOptionalLayouts(checkBox);
        }
    }

    QSet<int> selectedIds;
    for (int index : indexes) {
        if (selectedIds.contains(index)) {
            qDebug() << "[TestOrder] duplicate index ignored:" << index;
            continue;
        }
        DraggableCheckBox* checkBox = nullptr;
        for (DraggableCheckBox* item : freeWorkCheckBoxes_) {
            if (item && item->getIndex() == index) {
                checkBox = item;
                break;
            }
        }
        if (!checkBox) {
            qDebug() << "[TestOrder] index has no checkbox, ignored:" << index;
            continue;
        }
        selectedIds.insert(index);

        freeWorkConfigLayout_->addWidget(checkBox);
        checkBox->show();
    }

    QHash<QString, int> optionalPosByCategory;
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        optionalPosByCategory.insert(tab.first, 0);
    }
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (!checkBox || selectedIds.contains(checkBox->getIndex())) {
            continue;
        }
        QString cat = categoryKeyForCheckBox(checkBox);
        QGridLayout* grid = freeWorkOptionalLayouts_.value(cat);
        if (!grid) {
            cat = QStringLiteral("product");
            grid = freeWorkOptionalLayouts_.value(cat);
        }
        if (!grid) {
            continue;
        }
        const int pos = optionalPosByCategory.value(cat);
        grid->addWidget(checkBox, pos / freeWorkCols_, pos % freeWorkCols_);
        checkBox->show();
        optionalPosByCategory[cat] = pos + 1;
    }

    for (const auto& state : blockedStates) {
        if (state.first) {
            state.first->blockSignals(state.second);
        }
    }

    freeWorkConfigLayout_->invalidate();
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (layout) {
            layout->invalidate();
        }
    }
    if (ui->config) {
        ui->config->updateGeometry();
        ui->config->update();
    }
    if (ui->can_use) {
        ui->can_use->updateGeometry();
        ui->can_use->update();
    }
    updateGeometry();
    update();
    int optionalTotal = 0;
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        optionalTotal += optionalWidgetCount(layout);
    }
    qDebug() << "[TestOrder] after rebuild, config count =" << freeWorkConfigLayout_->count()
             << ", optional count =" << optionalTotal << ", selected ids =" << selectedIds.values();

    savedTestOrderSnapshot_ = indexes;
    testOrderDirty_ = false;
}

void qsetting::saveCurrentTestOrder() {
    if (!freeWorkConfigLayout_) {
        return;
    }
    QVector<int> indexes;
    for (int i = 0; i < freeWorkConfigLayout_->count(); ++i) {
        auto* checkBox = qobject_cast<DraggableCheckBox*>(freeWorkConfigLayout_->itemAt(i)->widget());
        if (checkBox && checkBox->isChecked()) {
            indexes.append(checkBox->getIndex());
        }
    }
    saveTestOrderIndexes(currentTestOrderStation(), indexes);
}

void qsetting::moveToLayout(QLayout* fromLayout, QLayout* toLayout, QWidget* widget) {
    if (!widget || !toLayout) {
        return;
    }
    if (fromLayout) {
        fromLayout->removeWidget(widget);
    }
    toLayout->addWidget(widget);
}

void qsetting::moveToGrid(QGridLayout* layout, QWidget* widget, int row, int col) {
    if (!layout || !widget) {
        return;
    }
    if (layout->indexOf(widget) != -1) {
        layout->removeWidget(widget);
    }
    layout->addWidget(widget, row, col);
}

void qsetting::moveToOptionalByPosition(DraggableCheckBox* checkBox, QGridLayout* optionalLayout, int row, int col) {
    if (!optionalLayout || !checkBox) {
        return;
    }

    const int targetIndex = qMax(0, row * freeWorkCols_ + col);
    QVector<DraggableCheckBox*> optionalWidgets;
    optionalWidgets.reserve(optionalLayout->count() + 1);

    for (int i = 0; i < optionalLayout->count(); ++i) {
        auto* widget = qobject_cast<DraggableCheckBox*>(optionalLayout->itemAt(i)->widget());
        if (widget && widget != checkBox) {
            optionalWidgets.append(widget);
        }
    }

    const int insertIndex = qBound(0, targetIndex, optionalWidgets.size());
    optionalWidgets.insert(insertIndex, checkBox);

    removeCheckBoxFromAllOptionalLayouts(checkBox);

    for (DraggableCheckBox* widget : optionalWidgets) {
        optionalLayout->removeWidget(widget);
    }
    for (int i = 0; i < optionalWidgets.size(); ++i) {
        optionalLayout->addWidget(optionalWidgets.at(i), i / freeWorkCols_, i % freeWorkCols_);
    }
}

void qsetting::calculateGridPosition(const QPoint& globalPos, const QRect& area, int& row, int& col,
                                     const QGridLayout* optionalLayout) const {
    if (!optionalLayout) {
        row = 0;
        col = 0;
        return;
    }
    const int rows = optionalRowCount(optionalLayout);
    const int singleHeight = qMax(1, (area.height() - 2 * kMargin + kRowSpacing) / rows);
    const int singleWidth = qMax(1, (area.width() - 2 * kMargin + kColumnSpacing) / qMax(1, freeWorkCols_));
    row = qBound(0, (globalPos.y() - area.y() - kMargin) / singleHeight, qMax(0, rows - 1));
    col = qBound(0, (globalPos.x() - area.x() - kMargin) / singleWidth, qMax(0, freeWorkCols_ - 1));
}

int qsetting::getIndexAt(const QPoint& globalPos) const {
    if (!freeWorkConfigLayout_) {
        return -1;
    }
    for (int i = 0; i < freeWorkConfigLayout_->count(); ++i) {
        QWidget* widget = freeWorkConfigLayout_->itemAt(i)->widget();
        if (!widget) {
            continue;
        }
        const QRect globalRect(widget->mapToGlobal(QPoint(0, 0)), widget->size());
        if (globalRect.contains(globalPos)) {
            return i;
        }
    }
    return -1;
}

void qsetting::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText()) {
        event->acceptProposedAction();
    }
}

void qsetting::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasText()) {
        dragPos_ = event->pos();
        update();
        event->acceptProposedAction();
    }
}

void qsetting::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (!mimeData->hasText()) {
        return;
    }

    const int sourceIndex = mimeData->text().toInt();
    DraggableCheckBox* sourceCheckBox = getConfiguredCheckBoxByIndex(sourceIndex);
    if (!sourceCheckBox) {
        sourceCheckBox = getOptionalCheckBoxByIndex(sourceIndex);
    }
    if (!sourceCheckBox || !freeWorkConfigLayout_ || freeWorkOptionalLayouts_.isEmpty()) {
        return;
    }

    const QPoint mouseGlobalPos = mapToGlobal(event->pos());
    const QRect configGlobalArea(ui->config->mapToGlobal(QPoint(0, 0)), ui->config->size());
    QRect optionalGlobalArea;
    QGridLayout* targetOptionalLayout = optionalLayoutAtGlobalPos(mouseGlobalPos, &optionalGlobalArea);

    if (configGlobalArea.contains(mouseGlobalPos)) {
        removeCheckBoxFromAllOptionalLayouts(sourceCheckBox);
        freeWorkConfigLayout_->addWidget(sourceCheckBox);
        const int destIndex = getIndexAt(mouseGlobalPos);
        if (destIndex >= 0 && destIndex != freeWorkConfigLayout_->indexOf(sourceCheckBox)) {
            freeWorkConfigLayout_->removeWidget(sourceCheckBox);
            freeWorkConfigLayout_->insertWidget(destIndex, sourceCheckBox);
        }
        event->acceptProposedAction();
    } else if (targetOptionalLayout && !optionalGlobalArea.isNull()) {
        int row = 0;
        int col = 0;
        calculateGridPosition(mouseGlobalPos, optionalGlobalArea, row, col, targetOptionalLayout);
        moveToOptionalByPosition(sourceCheckBox, targetOptionalLayout, row, col);
        event->acceptProposedAction();
    }

    dragPos_ = QPoint();
    testOrderDirty_ = true;
    saveCurrentTestOrder();
}

void qsetting::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    if (dragPos_.isNull() || !ui->freeWorkOptionalTabs) {
        return;
    }

    const QPoint globalDrag = mapToGlobal(dragPos_);
    QRect optionalArea;
    QGridLayout* layout = optionalLayoutAtGlobalPos(globalDrag, &optionalArea);
    if (!layout || optionalArea.isNull()) {
        return;
    }

    const int rows = optionalRowCount(layout);
    const int singleHeight = qMax(1, (optionalArea.height() - (2 * kMargin) + kRowSpacing) / rows);
    const int singleWidth = qMax(1, (optionalArea.width() - (2 * kMargin) + kColumnSpacing) / qMax(1, freeWorkCols_));

    QPainter painter(this);
    painter.fillRect(QRect(dragPos_, QSize(singleWidth - kColumnSpacing, singleHeight - kRowSpacing)), Qt::green);
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
                                               {"KEY_TEST", ui->radioButtonKeyTest},
                                               {"SUCTION_TEST", ui->radioButtonSuctionTest},
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

    // 加载老化工站配置
    ui->lineEdit_ageingBurningMode->setText(SETTINGS.value("AGING/BurningMode", 1).toString());
    ui->lineEdit_ageingBurningSeconds->setText(SETTINGS.value("AGING/BurningSeconds", 60 * 60 * 4).toString());
    if (ui->comboBox_systemProtocolType->count() == 0) {
        ui->comboBox_systemProtocolType->addItem(QStringLiteral("qpb（产测 PB）"), QStringLiteral("qpb"));
        ui->comboBox_systemProtocolType->addItem(QStringLiteral("qfctp（FCTP）"), QStringLiteral("qfctp"));
    }
    {
        const QString proto = SETTINGS.value(QStringLiteral("SYSTEM/ProtocolType"), QStringLiteral("qpb"))
                                  .toString()
                                  .trimmed()
                                  .toLower();
        int idx = ui->comboBox_systemProtocolType->findData(proto);
        if (idx < 0) {
            idx = 0;
        }
        ui->comboBox_systemProtocolType->setCurrentIndex(idx);
    }
    ui->lineEdit_brushInstrumentSendPacketCount->setText(
        QString::number(SETTINGS.value(QStringLiteral("BrushInstrument/InstrumentSendPacketCount"), 1000).toInt()));
    ui->lineEdit_brushInstrumentMaxPer->setText(
        QString::number(SETTINGS.value(QStringLiteral("BrushInstrument/MaxPer"), 0.05).toDouble(), 'f', 4));
    ui->lineEdit_brushInstrumentPacketPhaseWaitMs->setText(
        QString::number(SETTINGS.value(QStringLiteral("BrushInstrument/PacketPhaseWaitMs"), 2000).toInt()));
    ui->lineEdit_brushInstrumentStopAckTimeoutMs->setText(
        QString::number(SETTINGS.value(QStringLiteral("BrushInstrument/StopAckTimeoutMs"), 5000).toInt()));
    ui->checkBox_freeInstrumentBleBrushCmwConcurrent->setChecked(
        SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool());
    ui->checkBox_freeInstrumentBleBrushCmwOnStopPer->setChecked(
        SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwOnStopPer"), true).toBool());
    ui->checkBox_plcModbusTrace->setChecked(SETTINGS.value(QStringLiteral("PLC/ModbusTrace"), false).toBool());
    ui->lineEdit_plcIpAddress->setText(
        SETTINGS.value(QStringLiteral("PLC/IpAddress"), QStringLiteral("192.168.1.88")).toString());
    ui->lineEdit_plcPort->setText(QString::number(SETTINGS.value(QStringLiteral("PLC/Port"), 502).toInt()));
    ui->lineEdit_plcUnitId->setText(QString::number(SETTINGS.value(QStringLiteral("PLC/UnitId"), 1).toInt()));
    ui->lineEdit_plcMCoilOffset->setText(QString::number(SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset"), 0).toInt()));
    ui->lineEdit_plcMBase->setText(QString::number(SETTINGS.value(QStringLiteral("PLC/MBase"), 200).toInt()));
    ui->lineEdit_plcMBaseStationStep->setText(
        QString::number(SETTINGS.value(QStringLiteral("PLC/MBaseStationStep"), 20).toInt()));
    ui->lineEdit_plcConnectTimeoutMs->setText(
        QString::number(SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt()));
    ui->lineEdit_plcRequestTimeoutMs->setText(
        QString::number(SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt()));
    ui->lineEdit_plcCommandGapMs->setText(QString::number(SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt()));
    ui->lineEdit_plcSwitchDoneResetM->setText(
        QString::number(SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM"), 211).toInt()));
    ui->lineEdit_plcSwitchDoneResetPulseMs->setText(
        QString::number(SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetPulseMs"), 0).toInt()));
    ui->checkBox_plcConnectVerifyRead->setChecked(
        SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool());
    ui->lineEdit_plcIpAddressStation2->setText(SETTINGS.value(QStringLiteral("PLC/IpAddress_Station2"), QString()).toString());
    ui->lineEdit_plcPortStation2->setText(SETTINGS.value(QStringLiteral("PLC/Port_Station2"), QString()).toString());
    ui->lineEdit_plcUnitIdStation2->setText(SETTINGS.value(QStringLiteral("PLC/UnitId_Station2"), QString()).toString());
    {
        const int mb2 = SETTINGS.value(QStringLiteral("PLC/MBase_Station2"), -1).toInt();
        ui->lineEdit_plcMBaseStation2->setText(mb2 < 0 ? QString() : QString::number(mb2));
    }
    ui->lineEdit_plcMCoilOffsetStation2->setText(
        SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station2"), QString()).toString());

    // 加载三元组配置
    const QString defaultTupleUrl = kTupleEnvPresets.first().baseUrl;
    const QString savedTupleUrl = SETTINGS.value("Tuple/BaseUrl", defaultTupleUrl).toString();
    ui->lineEdit_tupleBaseUrl->setText(savedTupleUrl);
    ui->lineEdit_tupleAuthUser->setText(SETTINGS.value("Tuple/AuthUser").toString());
    ui->lineEdit_tupleAuthPassword->setText(SETTINGS.value("Tuple/AuthPassword").toString());
    ui->lineEdit_tupleSku->setText(SETTINGS.value("Tuple/Sku", "").toString());
    ui->lineEdit_tuplePosition->setText(SETTINGS.value("Tuple/Position", "L").toString());

    {
        QSignalBlocker blocker(ui->comboBox_tupleEnvironment);
        QString envKey = SETTINGS.value("Tuple/Environment").toString().trimmed();
        int idx = envKey.isEmpty() ? -1 : tupleEnvIndexForKey(ui->comboBox_tupleEnvironment, envKey);
        if (idx < 0) {
            idx = tupleEnvIndexForUrl(ui->comboBox_tupleEnvironment, savedTupleUrl);
        }
        if (idx < 0) {
            idx = ui->comboBox_tupleEnvironment->count() - 1;
        }
        ui->comboBox_tupleEnvironment->setCurrentIndex(idx);
    }

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
    ui->lineEdit_caliWaitTime->setText(SETTINGS.value("PRESSURE/CaliTime").toString());  // 压感校准时间

    ui->comboBox_pressFunctionSwitch->setCurrentIndex(SETTINGS.value("PRESSURE/functionSwitch", 0).toInt());
    ui->comboBox_displayImageType->setCurrentIndex(SETTINGS.value("PRESSURE/Use_graph", 0).toInt());
    ui->comboBox_individualMode->setCurrentIndex(SETTINGS.value("PRESSURE/Module", 0).toInt());
    ui->lineEdit_PressMechine->setText(SETTINGS.value("PRESSURE/PressMechine").toString());  //选择第几套治具
    ui->lineEdit_ButtonThreshold->setText(SETTINGS.value("PRESSURE/ButtonThreshold").toString());
    ui->lineEdit_ButtonThresholdCount->setText(SETTINGS.value("PRESSURE/ButtonThresholdCount").toString());
    ui->lineEdit_BrushThreshold->setText(SETTINGS.value("PRESSURE/BrushThreshold").toString());
    ui->lineEdit_BrushThresholdCount->setText(SETTINGS.value("PRESSURE/BrushThresholdCount").toString());
    ui->lineEdit_press_adc_shake->setText(SETTINGS.value("PRESSURE/ADCShakeValue").toString());

    // 加载摆幅限制设置
    ui->checkBox_amplitudeLimit->setChecked(SETTINGS.value("Press/AmplitudeLimit", false).toBool());
    ui->lineEdit_amplitudeLimitUpper->setText(SETTINGS.value("Press/AmplitudeLimitUpper", "0").toString());
    ui->lineEdit_amplitudeLimitLower->setText(SETTINGS.value("Press/AmplitudeLimitLower", "0").toString());
    // 加载摆幅误差值
    ui->lineEdit_amplitudeError->setText(SETTINGS.value("Pressure/AmplitudeError", "0").toString());

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
    ui->lineEdit_FSensorVersion->setText(SETTINGS.value("ProductInfo/FSensor_Version").toString());  // 电机压感版本
    ui->lineEdit_ImuID->setText(SETTINGS.value("ProductInfo/IMU_ID").toString());                    // imu的id
    ui->lineEdit_CameraID->setText(SETTINGS.value("ProductInfo/Camera_Id").toString());              // 摄像头的id

    // 加载 MAC-SN 文件路径
    ui->lineEdit_mac_sn_path->setText(
        SETTINGS.value("MAC_SN/FilePath", "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt").toString());

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
    ui->checkBox_FSensorVersion->setChecked(SETTINGS.value("ProductInfo/FSensorVersion_checkBox").toBool());
    ui->checkBox_ImuID->setChecked(SETTINGS.value("ProductInfo/ImuID_checkBox").toBool());
    ui->checkBox_CameraID->setChecked(SETTINGS.value("ProductInfo/CameraID_checkBox").toBool());

    // 按键测试 KeyId 配置
    ui->checkBox_KeyIdPower->setChecked(SETTINGS.value("ProductInfo/KeyIdPower_checkBox", true).toBool());
    ui->lineEdit_KeyIdPower->setText(SETTINGS.value("ProductInfo/KeyIdPower", "1").toString());
    ui->checkBox_KeyIdStartPause->setChecked(SETTINGS.value("ProductInfo/KeyIdStartPause_checkBox", true).toBool());
    ui->lineEdit_KeyIdStartPause->setText(SETTINGS.value("ProductInfo/KeyIdStartPause", "2").toString());
    ui->checkBox_KeyIdMode->setChecked(SETTINGS.value("ProductInfo/KeyIdMode_checkBox", true).toBool());
    ui->lineEdit_KeyIdMode->setText(SETTINGS.value("ProductInfo/KeyIdMode", "3").toString());
    ui->checkBox_KeyIdSpeed->setChecked(SETTINGS.value("ProductInfo/KeyIdSpeed_checkBox", true).toBool());
    ui->lineEdit_KeyIdSpeed->setText(SETTINGS.value("ProductInfo/KeyIdSpeed", "4").toString());
    ui->checkBox_KeyIdProgram->setChecked(SETTINGS.value("ProductInfo/KeyIdProgram_checkBox", true).toBool());
    ui->lineEdit_KeyIdProgram->setText(SETTINGS.value("ProductInfo/KeyIdProgram", "5").toString());
    ui->checkBox_KeyIdLeft->setChecked(SETTINGS.value("ProductInfo/KeyIdLeft_checkBox", true).toBool());
    ui->lineEdit_KeyIdLeft->setText(SETTINGS.value("ProductInfo/KeyIdLeft", "6").toString());
    ui->checkBox_KeyIdRight->setChecked(SETTINGS.value("ProductInfo/KeyIdRight_checkBox", true).toBool());
    ui->lineEdit_KeyIdRight->setText(SETTINGS.value("ProductInfo/KeyIdRight", "7").toString());
    ui->checkBox_KeyIdLeftRotate->setChecked(SETTINGS.value("ProductInfo/KeyIdLeftRotate_checkBox", true).toBool());
    ui->lineEdit_KeyIdLeftRotate->setText(SETTINGS.value("ProductInfo/KeyIdLeftRotate", "10").toString());
    ui->checkBox_KeyIdRightRotate->setChecked(SETTINGS.value("ProductInfo/KeyIdRightRotate_checkBox", true).toBool());
    ui->lineEdit_KeyIdRightRotate->setText(SETTINGS.value("ProductInfo/KeyIdRightRotate", "11").toString());
    ui->lineEdit_KeyCapLow->setText(SETTINGS.value(QStringLiteral("KeyCap/Low"), 1).toString());
    ui->lineEdit_KeyCapHigh->setText(SETTINGS.value(QStringLiteral("KeyCap/High"), 65535).toString());
    ui->lineEdit_KeyCapReadTimeoutMs->setText(SETTINGS.value(QStringLiteral("KeyCap/ReadTimeoutMs"), 5000).toString());
    ui->lineEdit_KeyCapReadCount->setText(SETTINGS.value(QStringLiteral("KeyCap/ReadCount"), 3).toString());
    ui->lineEdit_KeyCapReadIntervalMs->setText(SETTINGS.value(QStringLiteral("KeyCap/ReadIntervalMs"), 80).toString());
    ui->lineEdit_KeyCapSingleReadTimeoutMs->setText(SETTINGS.value(QStringLiteral("KeyCap/SingleReadTimeoutMs"), 2000).toString());

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
    ui->lineEdit_freework_press0_status->setText(SETTINGS.value("PeripheralStatus/Press0_Status").toString());
    ui->lineEdit_freework_press1_status->setText(SETTINGS.value("PeripheralStatus/Press1_Status").toString());
    ui->lineEdit_freework_battery_ic_status->setText(SETTINGS.value("PeripheralStatus/BatteryIc_Status").toString());
    ui->lineEdit_freework_touch_ic_status->setText(SETTINGS.value("PeripheralStatus/TouchIc_Status").toString());
    ui->lineEdit_freework_led_ic_status->setText(SETTINGS.value("PeripheralStatus/LedIc_Status").toString());
    ui->lineEdit_freework_pd_ic_status->setText(SETTINGS.value("PeripheralStatus/PdIc_Status").toString());
    ui->checkBox_freework_press0->setChecked(SETTINGS.value("FreeWorkPeripheral/Press0_checkBox", true).toBool());
    ui->checkBox_freework_press1->setChecked(SETTINGS.value("FreeWorkPeripheral/Press1_checkBox", true).toBool());
    ui->checkBox_freework_battery_ic->setChecked(SETTINGS.value("FreeWorkPeripheral/BatteryIC_checkBox", true).toBool());
    ui->checkBox_freework_touch_ic->setChecked(SETTINGS.value("FreeWorkPeripheral/TouchIC_checkBox", true).toBool());
    ui->checkBox_freework_led_ic->setChecked(SETTINGS.value("FreeWorkPeripheral/LedIC_checkBox", true).toBool());
    ui->checkBox_freework_pd_ic->setChecked(SETTINGS.value("FreeWorkPeripheral/PdIC_checkBox", true).toBool());

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
    ui->lineEdit_mes_config_file_path->setText(SETTINGS.value("MES/ConfigFilePath").toString());
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
                                        ui->radioButtonKeyTest->isChecked()          ? "KEY_TEST" :
                                        ui->radioButtonSuctionTest->isChecked()      ? "SUCTION_TEST" :
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

    // 保存老化工站配置
    SETTINGS.setValue("AGING/BurningMode", ui->lineEdit_ageingBurningMode->text());
    SETTINGS.setValue("AGING/BurningSeconds", ui->lineEdit_ageingBurningSeconds->text());
    SETTINGS.setValue(QStringLiteral("SYSTEM/ProtocolType"),
                      ui->comboBox_systemProtocolType->currentData().toString().trimmed());
    SETTINGS.setValue(QStringLiteral("BrushInstrument/InstrumentSendPacketCount"),
                      ui->lineEdit_brushInstrumentSendPacketCount->text().trimmed());
    SETTINGS.setValue(QStringLiteral("BrushInstrument/MaxPer"), ui->lineEdit_brushInstrumentMaxPer->text().trimmed());
    SETTINGS.setValue(QStringLiteral("BrushInstrument/PacketPhaseWaitMs"),
                      ui->lineEdit_brushInstrumentPacketPhaseWaitMs->text().trimmed());
    SETTINGS.setValue(QStringLiteral("BrushInstrument/StopAckTimeoutMs"),
                      ui->lineEdit_brushInstrumentStopAckTimeoutMs->text().trimmed());
    SETTINGS.setValue(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"),
                      ui->checkBox_freeInstrumentBleBrushCmwConcurrent->isChecked());
    SETTINGS.setValue(QStringLiteral("FreeInstrument/BleBrushCmwOnStopPer"),
                      ui->checkBox_freeInstrumentBleBrushCmwOnStopPer->isChecked());
    SETTINGS.setValue(QStringLiteral("PLC/ModbusTrace"), ui->checkBox_plcModbusTrace->isChecked());
    SETTINGS.setValue(QStringLiteral("PLC/IpAddress"), ui->lineEdit_plcIpAddress->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/Port"), ui->lineEdit_plcPort->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/UnitId"), ui->lineEdit_plcUnitId->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/MCoilAddressOffset"), ui->lineEdit_plcMCoilOffset->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/MBase"), ui->lineEdit_plcMBase->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/MBaseStationStep"), ui->lineEdit_plcMBaseStationStep->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/ConnectTimeoutMs"), ui->lineEdit_plcConnectTimeoutMs->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/RequestTimeoutMs"), ui->lineEdit_plcRequestTimeoutMs->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/CommandGapMs"), ui->lineEdit_plcCommandGapMs->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/SwitchTestDoneResetM"), ui->lineEdit_plcSwitchDoneResetM->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/SwitchTestDoneResetPulseMs"),
                      ui->lineEdit_plcSwitchDoneResetPulseMs->text().trimmed());
    SETTINGS.setValue(QStringLiteral("PLC/ConnectVerifyRead"), ui->checkBox_plcConnectVerifyRead->isChecked());

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
    {
        const QString t = ui->lineEdit_plcMBaseStation2->text().trimmed();
        if (t.isEmpty()) {
            SETTINGS.remove(QStringLiteral("PLC/MBase_Station2"));
        } else {
            SETTINGS.setValue(QStringLiteral("PLC/MBase_Station2"), t.toInt());
        }
    }
    savePlcOptional(QStringLiteral("PLC/MCoilAddressOffset_Station2"), ui->lineEdit_plcMCoilOffsetStation2->text());

    // 保存三元组配置
    SETTINGS.setValue("Tuple/Environment", ui->comboBox_tupleEnvironment->currentData().toString());
    SETTINGS.setValue("Tuple/BaseUrl", ui->lineEdit_tupleBaseUrl->text());
    SETTINGS.setValue("Tuple/AuthUser", ui->lineEdit_tupleAuthUser->text());
    SETTINGS.setValue("Tuple/AuthPassword", ui->lineEdit_tupleAuthPassword->text());
    SETTINGS.setValue("Tuple/Sku", ui->lineEdit_tupleSku->text());
    SETTINGS.setValue("Tuple/Position", ui->lineEdit_tuplePosition->text());

    // 保存行和列
    SETTINGS.setValue("User/formRow", ui->rowLineEdit->text());
    SETTINGS.setValue("User/formColumn", ui->columnLineEdit->text());

    // 将控件的值设置回配置文件
    SETTINGS.setValue("FIXTEST/MusicState", ui->lineEdit_MusicStatus->text());
    SETTINGS.setValue("FIXTEST/OverVoltageLight", ui->lineEdit_OvervoltageLightStatus->text());
    SETTINGS.setValue("FIXTEST/Button1", ui->lineEdit_Button1Status->text());
    SETTINGS.setValue("FIXTEST/Button2", ui->lineEdit_Button2Status->text());

    // 加载压感参数

    SETTINGS.setValue("PRESSURE/CaliTime", ui->lineEdit_caliWaitTime->text());
    SETTINGS.setValue("PRESSURE/TestTime", ui->lineEdit_testWaitTime->text());
    SETTINGS.setValue("PRESSURE/functionSwitch", ui->comboBox_pressFunctionSwitch->currentIndex());
    SETTINGS.setValue("PRESSURE/Use_graph", ui->comboBox_displayImageType->currentIndex());
    SETTINGS.setValue("PRESSURE/Module", ui->comboBox_individualMode->currentIndex());

    SETTINGS.setValue("PRESSURE/PressMechine", ui->lineEdit_PressMechine->text());
    SETTINGS.setValue("PRESSURE/ButtonThreshold", ui->lineEdit_ButtonThreshold->text());
    SETTINGS.setValue("PRESSURE/ButtonThresholdCount", ui->lineEdit_ButtonThresholdCount->text());
    SETTINGS.setValue("PRESSURE/BrushThreshold", ui->lineEdit_BrushThreshold->text());
    SETTINGS.setValue("PRESSURE/BrushThresholdCount", ui->lineEdit_BrushThresholdCount->text());
    SETTINGS.setValue("PRESSURE/ADCShakeValue", ui->lineEdit_press_adc_shake->text());

    // 保存摆幅限制设置
    SETTINGS.setValue("Press/AmplitudeLimit", ui->checkBox_amplitudeLimit->isChecked());
    SETTINGS.setValue("Press/AmplitudeLimitUpper", ui->lineEdit_amplitudeLimitUpper->text());
    SETTINGS.setValue("Press/AmplitudeLimitLower", ui->lineEdit_amplitudeLimitLower->text());
    // 保存摆幅误差值
    SETTINGS.setValue("Pressure/AmplitudeError", ui->lineEdit_amplitudeError->text());

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
    SETTINGS.setValue("ProductInfo/FSensor_Version", ui->lineEdit_FSensorVersion->text());
    SETTINGS.setValue("ProductInfo/IMU_ID", ui->lineEdit_ImuID->text());
    SETTINGS.setValue("ProductInfo/Camera_Id", ui->lineEdit_CameraID->text());

    // 保存 MAC-SN 文件路径
    SETTINGS.setValue("MAC_SN/FilePath", ui->lineEdit_mac_sn_path->text());

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
    SETTINGS.setValue("ProductInfo/FSensorVersion_checkBox", ui->checkBox_FSensorVersion->isChecked());
    SETTINGS.setValue("ProductInfo/ImuID_checkBox", ui->checkBox_ImuID->isChecked());
    SETTINGS.setValue("ProductInfo/CameraID_checkBox", ui->checkBox_CameraID->isChecked());

    SETTINGS.setValue("ProductInfo/KeyIdPower_checkBox", ui->checkBox_KeyIdPower->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdPower", ui->lineEdit_KeyIdPower->text());
    SETTINGS.setValue("ProductInfo/KeyIdStartPause_checkBox", ui->checkBox_KeyIdStartPause->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdStartPause", ui->lineEdit_KeyIdStartPause->text());
    SETTINGS.setValue("ProductInfo/KeyIdMode_checkBox", ui->checkBox_KeyIdMode->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdMode", ui->lineEdit_KeyIdMode->text());
    SETTINGS.setValue("ProductInfo/KeyIdSpeed_checkBox", ui->checkBox_KeyIdSpeed->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdSpeed", ui->lineEdit_KeyIdSpeed->text());
    SETTINGS.setValue("ProductInfo/KeyIdProgram_checkBox", ui->checkBox_KeyIdProgram->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdProgram", ui->lineEdit_KeyIdProgram->text());
    SETTINGS.setValue("ProductInfo/KeyIdLeft_checkBox", ui->checkBox_KeyIdLeft->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdLeft", ui->lineEdit_KeyIdLeft->text());
    SETTINGS.setValue("ProductInfo/KeyIdRight_checkBox", ui->checkBox_KeyIdRight->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdRight", ui->lineEdit_KeyIdRight->text());
    SETTINGS.setValue("ProductInfo/KeyIdLeftRotate_checkBox", ui->checkBox_KeyIdLeftRotate->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdLeftRotate", ui->lineEdit_KeyIdLeftRotate->text());
    SETTINGS.setValue("ProductInfo/KeyIdRightRotate_checkBox", ui->checkBox_KeyIdRightRotate->isChecked());
    SETTINGS.setValue("ProductInfo/KeyIdRightRotate", ui->lineEdit_KeyIdRightRotate->text());
    SETTINGS.setValue(QStringLiteral("KeyCap/Low"), ui->lineEdit_KeyCapLow->text());
    SETTINGS.setValue(QStringLiteral("KeyCap/High"), ui->lineEdit_KeyCapHigh->text());
    SETTINGS.setValue(QStringLiteral("KeyCap/ReadTimeoutMs"), ui->lineEdit_KeyCapReadTimeoutMs->text());
    SETTINGS.setValue(QStringLiteral("KeyCap/ReadCount"), ui->lineEdit_KeyCapReadCount->text());
    SETTINGS.setValue(QStringLiteral("KeyCap/ReadIntervalMs"), ui->lineEdit_KeyCapReadIntervalMs->text());
    SETTINGS.setValue(QStringLiteral("KeyCap/SingleReadTimeoutMs"), ui->lineEdit_KeyCapSingleReadTimeoutMs->text());

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
    SETTINGS.setValue("PeripheralStatus/Press0_Status", ui->lineEdit_freework_press0_status->text());
    SETTINGS.setValue("PeripheralStatus/Press1_Status", ui->lineEdit_freework_press1_status->text());
    SETTINGS.setValue("PeripheralStatus/BatteryIc_Status", ui->lineEdit_freework_battery_ic_status->text());
    SETTINGS.setValue("PeripheralStatus/TouchIc_Status", ui->lineEdit_freework_touch_ic_status->text());
    SETTINGS.setValue("PeripheralStatus/LedIc_Status", ui->lineEdit_freework_led_ic_status->text());
    SETTINGS.setValue("PeripheralStatus/PdIc_Status", ui->lineEdit_freework_pd_ic_status->text());
    SETTINGS.setValue("FreeWorkPeripheral/Press0_checkBox", ui->checkBox_freework_press0->isChecked());
    SETTINGS.setValue("FreeWorkPeripheral/Press1_checkBox", ui->checkBox_freework_press1->isChecked());
    SETTINGS.setValue("FreeWorkPeripheral/BatteryIC_checkBox", ui->checkBox_freework_battery_ic->isChecked());
    SETTINGS.setValue("FreeWorkPeripheral/TouchIC_checkBox", ui->checkBox_freework_touch_ic->isChecked());
    SETTINGS.setValue("FreeWorkPeripheral/LedIC_checkBox", ui->checkBox_freework_led_ic->isChecked());
    SETTINGS.setValue("FreeWorkPeripheral/PdIC_checkBox", ui->checkBox_freework_pd_ic->isChecked());

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
    SETTINGS.setValue("MES/ConfigFilePath", ui->lineEdit_mes_config_file_path->text());
    bydmes::loadExternalMesConfig(nullptr);
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
    if (stationReloading_) {
        qDebug() << "切换工站中，跳过设置页关闭保存";
        event->accept();
        return;
    }
    saveCurrentTestOrder();
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

        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
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

        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "F20") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("60");
        ui->lineEdit_ButtonThresholdCount->setText("300");
        ui->lineEdit_BrushThreshold->setText("10");
        ui->lineEdit_BrushThresholdCount->setText("200");
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
        ui->lineEdit_ButtonThreshold->setText("30");
        ui->lineEdit_ButtonThresholdCount->setText("400");
        ui->lineEdit_BrushThreshold->setText("350");
        ui->lineEdit_BrushThresholdCount->setText("300");
    }
    if (ui->comboBox_productName->currentText() == "T10") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
    }

    //华勤：欣旺达：依次扫码连接，不需要唤醒
    if (ui->comboBox_productName->currentText() == "Y20PS") {
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("400");
    }

    //欣旺达：依次扫标签码连接，需要唤醒
    if (ui->comboBox_productName->currentText() == "Y21") {
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->checkBox_PressWindow->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("70");
        ui->lineEdit_ButtonThresholdCount->setText("300");
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
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30S") {
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }
    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30") {
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->lineEdit_ButtonThreshold->setText("40");
        ui->lineEdit_ButtonThresholdCount->setText("200");
    }

    //立讯：imu不需要晃动唤醒，全扫码再测试
    if (ui->comboBox_productName->currentText() == "Y30P") {
        ui->checkBox_TestAudioCurrent->setChecked(true);
        ui->checkBox_TestShippingCurrent->setChecked(true);
        ui->checkBox_NeedWriteSubpid->setChecked(true);
        // ui->checkBox_NeedWriteSkuid->setChecked(true);
        ui->checkBox_IMULastEnterStartTest->setChecked(true);
        ui->checkBox_SerialPortMAC->setChecked(true);
        ui->checkBox_uperMotor->setChecked(true);
        ui->checkBox_IMUCalibrationWakeup->setChecked(true);
        ui->checkBox_ShipModeResponse->setChecked(true);
        ui->checkBox_TestWifiSignal->setChecked(true);
        ui->lineEdit_BrushThreshold->setText("20");
        ui->lineEdit_BrushThresholdCount->setText("200");
    }

    if (ui->comboBox_productName->currentText() == "P20PS") {
        ui->checkBox_TestShippingCurrent->setChecked(true);
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
    if (ui->comboBox_factory->currentText() == "byd") {
        ui->label_78->show();
        ui->lineEdit_mesUrl->show();
        ui->label_79->show();
        ui->lineEdit_processName->show();
        ui->label_weikesen_order->show();
        ui->lineEdit_weikesen_order->show();
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

void qsetting::on_comboBox_testOrderStation_currentTextChanged(const QString& text) {
    Q_UNUSED(text);
    if (switchingTestOrderStation_ || !ui->comboBox_testOrderStation) {
        qDebug() << "[TestOrder] station changed ignored, switching =" << switchingTestOrderStation_;
        return;
    }
    const QString nextStation = ui->comboBox_testOrderStation->currentData().toString().trimmed();
    qDebug() << "[TestOrder] station changed, current =" << activeTestOrderStation_ << ", next =" << nextStation
             << ", display =" << ui->comboBox_testOrderStation->currentText();
    if (nextStation.isEmpty() || nextStation == activeTestOrderStation_) {
        qDebug() << "[TestOrder] station unchanged/empty, skip reorder";
        return;
    }
    if (testOrderDirty_) {
        const auto result = QMessageBox::question(this, "保存测试流程配置", "当前工站的测试流程已变化，是否保存？",
                                                  QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
        if (result == QMessageBox::Yes) {
            saveCurrentTestOrder();
        } else {
            saveTestOrderIndexes(activeTestOrderStation_, savedTestOrderSnapshot_);
        }
        testOrderDirty_ = false;
    }
    activeTestOrderStation_ = nextStation;
    SETTINGS.setValue(kSelectedStationKey, activeTestOrderStation_);
    SETTINGS.setValue(kSelectedStationNameKey, stationDisplayNameFromKey(activeTestOrderStation_));
    applyStationUiState(activeTestOrderStation_);
    reorderFreeWorkCheckBoxes();
}

void qsetting::on_pushButton_clearConfiguredTestOrder_clicked() {
    if (!freeWorkConfigLayout_ || freeWorkOptionalLayouts_.isEmpty()) {
        return;
    }
    const auto result = QMessageBox::question(this, "确认清空", "确定要清空当前工站的已配置测试流程吗？",
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    while (QLayoutItem* item = freeWorkConfigLayout_->takeAt(0)) {
        delete item;
    }
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (!layout) {
            continue;
        }
        while (QLayoutItem* item = layout->takeAt(0)) {
            delete item;
        }
    }

    QVector<QPair<DraggableCheckBox*, bool>> blockedStates;
    blockedStates.reserve(freeWorkCheckBoxes_.size());
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (checkBox) {
            blockedStates.append(qMakePair(checkBox, checkBox->blockSignals(true)));
        }
    }

    QHash<QString, int> optionalPosByCategory;
    for (const auto& tab : kFreeWorkOptionalCategoryTabs) {
        optionalPosByCategory.insert(tab.first, 0);
    }
    for (DraggableCheckBox* checkBox : freeWorkCheckBoxes_) {
        if (!checkBox) {
            continue;
        }
        QString cat = categoryKeyForCheckBox(checkBox);
        QGridLayout* grid = freeWorkOptionalLayouts_.value(cat);
        if (!grid) {
            cat = QStringLiteral("product");
            grid = freeWorkOptionalLayouts_.value(cat);
        }
        if (!grid) {
            continue;
        }
        const int pos = optionalPosByCategory.value(cat);
        grid->addWidget(checkBox, pos / freeWorkCols_, pos % freeWorkCols_);
        checkBox->show();
        optionalPosByCategory[cat] = pos + 1;
    }

    for (const auto& state : blockedStates) {
        if (state.first) {
            state.first->blockSignals(state.second);
        }
    }

    freeWorkConfigLayout_->invalidate();
    for (QGridLayout* layout : freeWorkOptionalLayouts_) {
        if (layout) {
            layout->invalidate();
        }
    }
    testOrderDirty_ = true;
    saveCurrentTestOrder();
}

void qsetting::on_pushButton_mesConfigFileBrowse_clicked() {
    QString startDir = QFileInfo(ui->lineEdit_mes_config_file_path->text().trimmed()).absolutePath();
    if (startDir.isEmpty() || !QFileInfo(startDir).isDir()) {
        startDir = QCoreApplication::applicationDirPath();
    }
    const QString path = QFileDialog::getOpenFileName(this, "选择MES配置文件", startDir,
                                                    "配置文件 (*.ini *.json *.xml);;所有文件 (*.*)");
    if (!path.isEmpty()) {
        ui->lineEdit_mes_config_file_path->setText(path);
    }
}
