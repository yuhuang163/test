#include "test_case.h"

#include <QSet>

#include "Abini.h"
#include "asd9026a_cmd_manifest.h"
#include "common_utils.h"
#include "device_cmd_manifest.h"
#include "dongle_cmd_manifest.h"
#include "fixture_pcba_cmd_manifest.h"
#include "fixture_uart_types.h"
#include "jieli_bt_box_cmd_manifest.h"
#include "modbus_cmd_manifest.h"
#include "scpi_cmd_manifest.h"
#include "usb_camera_cmd_manifest.h"
#include "ves_light_cmd_manifest.h"
#include "screen_inspect_analyzer.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// ===================== GateRegistry =====================

namespace {

const QVector<GateTypeDescriptor> kTypes = {
    {QStringLiteral("ProtocolRssiData"), QStringLiteral("蓝牙信号强度"), {{QStringLiteral("dbm"), QStringLiteral("信号强度(分贝)")}}},
    {QStringLiteral("ProtocolBatteryData"), QStringLiteral("电量"), {{QStringLiteral("percent"), QStringLiteral("电量(%)")}, {QStringLiteral("chargeState"), QStringLiteral("充电状态")}, {QStringLiteral("voltageMv"), QStringLiteral("电压(mV)")}, {QStringLiteral("currentMa"), QStringLiteral("电流(mA)")}, {QStringLiteral("temperatureC"), QStringLiteral("温度(℃)")}}},
    {QStringLiteral("ProtocolKeyCapData"), QStringLiteral("按键电容"), {{QStringLiteral("capacitance"), QStringLiteral("电容值")}, {QStringLiteral("keyId"), QStringLiteral("按键编号")}}},
    {QStringLiteral("ProtocolChargeCurrentData"), QStringLiteral("充电电流"), {{QStringLiteral("currentMa"), QStringLiteral("电流(mA)")}}},
    {QStringLiteral("ProtocolFactoryDoneData"), QStringLiteral("产测完成标识"),
     {{QStringLiteral("done"), QStringLiteral("已完成(1=是)")}}},
    {QStringLiteral("ProtocolTrimData"), QStringLiteral("Trim微调值"), {{QStringLiteral("trim"), QStringLiteral("微调值")}}},
    {QStringLiteral("ProtocolLightCalibData"), QStringLiteral("光感校准值"), {{QStringLiteral("calibValue"), QStringLiteral("校准值")}}},
    {QStringLiteral("ProtocolPhotosensitiveData"), QStringLiteral("光感上报"), {{QStringLiteral("lightSensor"), QStringLiteral("光感值")}}},
    {QStringLiteral("ProtocolAiotImuCaliData"), QStringLiteral("Qaiot IMU校准"),
     {{QStringLiteral("kx"), QStringLiteral("kx")},
      {QStringLiteral("ky"), QStringLiteral("ky")},
      {QStringLiteral("kz"), QStringLiteral("kz")},
      {QStringLiteral("syx"), QStringLiteral("syx")},
      {QStringLiteral("szx"), QStringLiteral("szx")},
      {QStringLiteral("szy"), QStringLiteral("szy")},
      {QStringLiteral("bx"), QStringLiteral("bx")},
      {QStringLiteral("by"), QStringLiteral("by")},
      {QStringLiteral("bz"), QStringLiteral("bz")}}},
    {QStringLiteral("ProtocolAiotFsensorCaliData"), QStringLiteral("Qaiot电容/力传感校准"),
     {{QStringLiteral("calibrated"), QStringLiteral("校准标志")}}},
    {QStringLiteral("ProtocolAiotExceptionThresholdData"), QStringLiteral("Qaiot异常阈值"),
     {{QStringLiteral("type"), QStringLiteral("异常类型")},
      {QStringLiteral("value"), QStringLiteral("阈值主值")},
      {QStringLiteral("valueHigh"), QStringLiteral("阈值上限")}}},
    {QStringLiteral("ProtocolAiotPumpParamData"), QStringLiteral("Qaiot泵/阀参数"),
     {{QStringLiteral("circleNum"), QStringLiteral("循环次数")},
      {QStringLiteral("durationTime"), QStringLiteral("泵工作时长")},
      {QStringLiteral("intervalTime"), QStringLiteral("泵间隔时长")},
      {QStringLiteral("valveEnableTime"), QStringLiteral("阀使能时长")},
      {QStringLiteral("valveDisableTime"), QStringLiteral("阀关闭时长")},
      {QStringLiteral("pumpPwm"), QStringLiteral("泵PWM%")},
      {QStringLiteral("valvePwm"), QStringLiteral("阀PWM%")}}},
    {QStringLiteral("ProtocolAiotHeatTestData"), QStringLiteral("Qaiot自定义加热"),
     {{QStringLiteral("enable"), QStringLiteral("加热使能")},
      {QStringLiteral("driveStrength"), QStringLiteral("加热强度")},
      {QStringLiteral("durationTime"), QStringLiteral("加热时长")}}},
    {QStringLiteral("ProtocolAiotVibrationTestData"), QStringLiteral("Qaiot自定义振动"),
     {{QStringLiteral("enable"), QStringLiteral("振动使能")},
      {QStringLiteral("driveStrength"), QStringLiteral("振动强度")},
      {QStringLiteral("freq"), QStringLiteral("振动频率")},
      {QStringLiteral("durationTime"), QStringLiteral("振动时长")}}},
    {QStringLiteral("ProtocolAiotCycleReportConfigData"), QStringLiteral("Qaiot循环上报配置"),
     {{QStringLiteral("enable"), QStringLiteral("循环上报使能")},
      {QStringLiteral("dataType"), QStringLiteral("数据类型")},
      {QStringLiteral("intervalTime"), QStringLiteral("上报周期ms")}}},
    {QStringLiteral("ProtocolAiotCycleReportData"), QStringLiteral("Qaiot循环上报数据"),
     {{QStringLiteral("dataType"), QStringLiteral("数据类型")},
      {QStringLiteral("accX"), QStringLiteral("加速度X")},
      {QStringLiteral("accY"), QStringLiteral("加速度Y")},
      {QStringLiteral("accZ"), QStringLiteral("加速度Z")},
      {QStringLiteral("gyroX"), QStringLiteral("角速度X")},
      {QStringLiteral("gyroY"), QStringLiteral("角速度Y")},
      {QStringLiteral("gyroZ"), QStringLiteral("角速度Z")},
      {QStringLiteral("pressureOut"), QStringLiteral("压力出(0.1Pa)")},
      {QStringLiteral("pressureIn"), QStringLiteral("压力进(0.1Pa)")},
      {QStringLiteral("flowRate"), QStringLiteral("气流(0.01L/min)")},
      {QStringLiteral("distanceMm"), QStringLiteral("距离mm")},
      {QStringLiteral("adcRaw"), QStringLiteral("ADC原始值")},
      {QStringLiteral("irLevel"), QStringLiteral("红外强度")},
      {QStringLiteral("impedance"), QStringLiteral("阻抗(0.1Ω)")},
      {QStringLiteral("levelMm"), QStringLiteral("液位mm")},
      {QStringLiteral("temperatureC"), QStringLiteral("温度℃")},
      {QStringLiteral("humidity"), QStringLiteral("湿度%RH")},
      {QStringLiteral("currentMa"), QStringLiteral("电流mA")},
      {QStringLiteral("hallState"), QStringLiteral("霍尔状态")},
      {QStringLiteral("pulseCount"), QStringLiteral("编码器脉冲")}}},
    {QStringLiteral("ProtocolSnData"), QStringLiteral("序列号"), {{QStringLiteral("value"), QStringLiteral("序列号文本")}}},
    {QStringLiteral("ProtocolBaseInfoData"), QStringLiteral("基本信息"), {{QStringLiteral("soft_version"), QStringLiteral("软件版本")}, {QStringLiteral("res_version"), QStringLiteral("资源版本")}, {QStringLiteral("product_name"), QStringLiteral("产品名称")}, {QStringLiteral("hw_version"), QStringLiteral("硬件版本")}, {QStringLiteral("algo_version"), QStringLiteral("算法版本")}, {QStringLiteral("ageing_state"), QStringLiteral("老化状态")}}},
    {QStringLiteral("ProtocolPeriphStateData"), QStringLiteral("外设状态"), {{QStringLiteral("press0_state"), QStringLiteral("压感0状态")}, {QStringLiteral("press1_state"), QStringLiteral("压感1状态")}, {QStringLiteral("battery_ic_state"), QStringLiteral("电池IC状态")}, {QStringLiteral("touch_ic_state"), QStringLiteral("触摸IC状态")}, {QStringLiteral("led_ic_state"), QStringLiteral("LED IC状态")}, {QStringLiteral("pd_ic_state"), QStringLiteral("PD IC状态")}}},
    {QStringLiteral("ProtocolTupleData"), QStringLiteral("设备三元组"), {{QStringLiteral("productId"), QStringLiteral("产品密钥")}, {QStringLiteral("deviceId"), QStringLiteral("设备名")}, {QStringLiteral("key"), QStringLiteral("设备密钥")}}},
    {QStringLiteral("ProtocolButtonStateData"), QStringLiteral("按键状态"), {{QStringLiteral("modeButtonState"), QStringLiteral("模式键状态")}, {QStringLiteral("powerButtonState"), QStringLiteral("电源键状态")}, {QStringLiteral("keyButtonId"), QStringLiteral("按键编号")}}},
    {QStringLiteral("ProtocolAgingStatusData"), QStringLiteral("老化状态上报"), {{QStringLiteral("status"), QStringLiteral("状态码")}, {QStringLiteral("loops"), QStringLiteral("循环次数")}, {QStringLiteral("seconds"), QStringLiteral("秒数")}}},
    {QStringLiteral("ProtocolMusicStateData"), QStringLiteral("音乐状态"), {{QStringLiteral("musicState"), QStringLiteral("音乐状态码")}}},
    {QStringLiteral("ProtocolResultData"), QStringLiteral("通用结果码"), {{QStringLiteral("result"), QStringLiteral("结果码")}}},
    {QStringLiteral("ProtocolFixturePcbaData"), QStringLiteral("PCBA治具数据包"), {{QStringLiteral("machineNumber"), QStringLiteral("机号")}, {QStringLiteral("staticCurrent"), QStringLiteral("静态电流(uA)")}, {QStringLiteral("workingCurrent"), QStringLiteral("工作电流(mA)")}, {QStringLiteral("chargingCurrent"), QStringLiteral("充电电流(mA)")}, {QStringLiteral("musicCurrent"), QStringLiteral("音频IC电流(mA)")}, {QStringLiteral("standbyCurrentUa"), QStringLiteral("待机电流(uA)")}, {QStringLiteral("pumpVoltageMv"), QStringLiteral("泵电压(mV)")}, {QStringLiteral("mcuVoltageMv"), QStringLiteral("MCU电压(mV)")}, {QStringLiteral("valveVoltageMv"), QStringLiteral("阀电压(mV)")}, {QStringLiteral("button1"), QStringLiteral("按键1")}, {QStringLiteral("button2"), QStringLiteral("按键2")}, {QStringLiteral("overVoltageLight"), QStringLiteral("过压灯")}, {QStringLiteral("fixerro"), QStringLiteral("治具错误码")}}},
    {QStringLiteral("ProtocolJieliBtBoxData"), QStringLiteral("杰理蓝牙盒子RF"), {{QStringLiteral("rssi"), QStringLiteral("RSSI(dBm)")}, {QStringLiteral("freqOffset"), QStringLiteral("频偏")}, {QStringLiteral("mac"), QStringLiteral("MAC地址")}}},
    {QStringLiteral("ProtocolMacData"), QStringLiteral("MAC地址"), {{QStringLiteral("mac"), QStringLiteral("MAC文本")}}},
    {QStringLiteral("ProtocolTypeData"), QStringLiteral("状态码"), {{QStringLiteral("type"), QStringLiteral("状态值")}}},
    {QStringLiteral("ProtocolFlangeData"), QStringLiteral("法兰状态"), {{QStringLiteral("type"), QStringLiteral("法兰类型")}}},
    {QStringLiteral("ProtocolPumpStallCurrentData"), QStringLiteral("泵堵电流"), {{QStringLiteral("adcValue"), QStringLiteral("堵转ADC")}}},
    {QStringLiteral("ProtocolRootAgingHistoryData"), QStringLiteral("老化历史/老化模式"),
     {{QStringLiteral("status"), QStringLiteral("老化使能")},
      {QStringLiteral("finishedFlag"), QStringLiteral("老化完成标志")},
      {QStringLiteral("agingCount"), QStringLiteral("老化当前次数")},
      {QStringLiteral("batteryMaxTempC"), QStringLiteral("电池历史最高温℃")},
      {QStringLiteral("flangeMaxTempC"), QStringLiteral("法兰历史最高温℃")},
      {QStringLiteral("stallCount"), QStringLiteral("老化堵转次数")},
      {QStringLiteral("stallThreshold"), QStringLiteral("泵阀堵转阈值")},
      {QStringLiteral("stallCurrent0"), QStringLiteral("堵转电流1")},
      {QStringLiteral("stallCurrent1"), QStringLiteral("堵转电流2")},
      {QStringLiteral("stallCurrent2"), QStringLiteral("堵转电流3")},
      {QStringLiteral("stallCurrent3"), QStringLiteral("堵转电流4")},
      {QStringLiteral("stallCurrent4"), QStringLiteral("堵转电流5")}}},
    {QStringLiteral("ProtocolBatteryTempData"), QStringLiteral("电池温度"), {{QStringLiteral("type"), QStringLiteral("温度值")}}},
    {QStringLiteral("ProtocolHeatTempData"), QStringLiteral("加热温度"), {{QStringLiteral("type"), QStringLiteral("温度值")}}},
    {QStringLiteral("ProtocolMeasureData"), QStringLiteral("外设测量值"), {{QStringLiteral("value"), QStringLiteral("测量数值")}, {QStringLiteral("valueText"), QStringLiteral("测量文本值")}, {QStringLiteral("deviceName"), QStringLiteral("外设名称")}, {QStringLiteral("channel"), QStringLiteral("通道号")}, {QStringLiteral("type"), QStringLiteral("测量类型")}, {QStringLiteral("unit"), QStringLiteral("单位")}}},
    {QStringLiteral("ProtocolDongleSuctionData"),
     QStringLiteral("Dongle吸力实时"),
     {{QStringLiteral("ch1Kpa"), QStringLiteral("CH1(kPa)")},
      {QStringLiteral("ch2Kpa"), QStringLiteral("CH2(kPa)")},
      {QStringLiteral("ch3Kpa"), QStringLiteral("CH3(kPa)")}}},
    {QStringLiteral("ProtocolDongleSuctionPeakData"),
     QStringLiteral("Dongle吸力峰值"),
     {{QStringLiteral("peakKpa"), QStringLiteral("单通道最低峰值")},
      {QStringLiteral("highKpa"), QStringLiteral("单通道最弱峰值(kPa)")},
      {QStringLiteral("peakDiffKpa"), QStringLiteral("单通道峰值差(最大峰-最小峰)")},
      {QStringLiteral("ch1PeakKpa"), QStringLiteral("CH1最低峰值")},
      {QStringLiteral("ch2PeakKpa"), QStringLiteral("CH2最低峰值")},
      {QStringLiteral("sideDiffKpa"), QStringLiteral("CH1-CH2峰差(kPa)")},
      {QStringLiteral("peakCount"), QStringLiteral("完整周期峰个数")}}},
    {QStringLiteral("ProtocolScreenInspectData"),
     QStringLiteral("屏幕检测"),
     {      {QStringLiteral("deadPixels"), QStringLiteral("坏点数")},
      {QStringLiteral("detectedColor"), QStringLiteral("实测纯色")},
      {QStringLiteral("colorMatch"), QStringLiteral("是否为期望纯色")},
      {QStringLiteral("ssim"), QStringLiteral("与参考图相似度（0~1）")},
      {QStringLiteral("muraStd"), QStringLiteral("亮度起伏σ（灰阶勿勾）")}}},
};

double fieldValueFromVariant(const QString& reportType, const QString& field, const QVariant& payload, bool& ok) {
    ok = false;
    if (reportType == QLatin1String("ProtocolRssiData")) {
        const auto d = payload.value<ProtocolRssiData>();
        if (field == QLatin1String("dbm")) {
            ok = true;
            return d.dbm;
        }
    } else if (reportType == QLatin1String("ProtocolBatteryData")) {
        const auto d = payload.value<ProtocolBatteryData>();
        if (field == QLatin1String("percent")) {
            ok = true;
            return d.percent;
        }
        if (field == QLatin1String("chargeState")) {
            ok = true;
            return d.chargeState;
        }
        if (field == QLatin1String("voltageMv")) {
            ok = true;
            return d.voltageMv;
        }
        if (field == QLatin1String("currentMa")) {
            ok = true;
            return d.currentMa;
        }
        if (field == QLatin1String("temperatureC")) {
            ok = true;
            return d.temperatureC;
        }
    } else if (reportType == QLatin1String("ProtocolKeyCapData")) {
        const auto d = payload.value<ProtocolKeyCapData>();
        if (field == QLatin1String("capacitance")) {
            ok = true;
            return static_cast<double>(d.capacitance);
        }
        if (field == QLatin1String("keyId")) {
            ok = true;
            return d.keyId;
        }
    } else if (reportType == QLatin1String("ProtocolChargeCurrentData")) {
        const auto d = payload.value<ProtocolChargeCurrentData>();
        if (field == QLatin1String("currentMa")) {
            ok = true;
            return static_cast<double>(d.currentMa);
        }
    } else if (reportType == QLatin1String("ProtocolFactoryDoneData")) {
        const auto d = payload.value<ProtocolFactoryDoneData>();
        if (field == QLatin1String("done")) {
            ok = true;
            return d.done ? 1.0 : 0.0;
        }
    } else if (reportType == QLatin1String("ProtocolPumpStallCurrentData")) {
        const auto d = payload.value<ProtocolPumpStallCurrentData>();
        if (field == QLatin1String("adcValue")) {
            ok = true;
            return static_cast<double>(d.adcValue);
        }
    } else if (reportType == QLatin1String("ProtocolRootAgingHistoryData")) {
        const auto d = payload.value<ProtocolRootAgingHistoryData>();
        if (field == QLatin1String("status") || field == QLatin1String("enable")) {
            ok = true;
            return static_cast<double>(d.status);
        }
        if (field == QLatin1String("finishedFlag") || field == QLatin1String("finished")) {
            ok = true;
            return static_cast<double>(d.finishedFlag);
        }
        if (field == QLatin1String("agingCount") || field == QLatin1String("count")
            || field == QLatin1String("loops")) {
            ok = true;
            return static_cast<double>(d.agingCount);
        }
        if (field == QLatin1String("batteryMaxTempC") || field == QLatin1String("batteryMaxTemp")) {
            ok = true;
            return static_cast<double>(d.batteryMaxTempC);
        }
        if (field == QLatin1String("flangeMaxTempC") || field == QLatin1String("flangeMaxTemp")) {
            ok = true;
            return static_cast<double>(d.flangeMaxTempC);
        }
        if (field == QLatin1String("stallCount")) {
            ok = true;
            return static_cast<double>(d.stallCount);
        }
        if (field == QLatin1String("stallThreshold")) {
            ok = true;
            return static_cast<double>(d.stallThreshold);
        }
        if (field.startsWith(QLatin1String("stallCurrent"))) {
            bool idxOk = false;
            const int idx = field.mid(QStringLiteral("stallCurrent").size()).toInt(&idxOk);
            if (idxOk && idx >= 0 && idx < 5) {
                ok = true;
                return static_cast<double>(d.stallCurrents[idx]);
            }
        }
    } else if (reportType == QLatin1String("ProtocolTrimData")) {
        const auto d = payload.value<ProtocolTrimData>();
        if (field == QLatin1String("trim")) {
            ok = true;
            return static_cast<double>(d.trim);
        }
    } else if (reportType == QLatin1String("ProtocolLightCalibData")) {
        const auto d = payload.value<ProtocolLightCalibData>();
        if (field == QLatin1String("calibValue")) {
            ok = true;
            return static_cast<double>(d.calibValue);
        }
    } else if (reportType == QLatin1String("ProtocolPhotosensitiveData")) {
        const auto d = payload.value<ProtocolPhotosensitiveData>();
        if (field == QLatin1String("lightSensor") || field == QLatin1String("value")) {
            ok = true;
            return static_cast<double>(d.lightSensor);
        }
    } else if (reportType == QLatin1String("ProtocolAiotImuCaliData")) {
        const auto d = payload.value<ProtocolAiotImuCaliData>();
        if (field == QLatin1String("kx")) {
            ok = true;
            return d.kx;
        }
        if (field == QLatin1String("ky")) {
            ok = true;
            return d.ky;
        }
        if (field == QLatin1String("kz")) {
            ok = true;
            return d.kz;
        }
        if (field == QLatin1String("syx")) {
            ok = true;
            return d.syx;
        }
        if (field == QLatin1String("szx")) {
            ok = true;
            return d.szx;
        }
        if (field == QLatin1String("szy")) {
            ok = true;
            return d.szy;
        }
        if (field == QLatin1String("bx")) {
            ok = true;
            return d.bx;
        }
        if (field == QLatin1String("by")) {
            ok = true;
            return d.by;
        }
        if (field == QLatin1String("bz")) {
            ok = true;
            return d.bz;
        }
    } else if (reportType == QLatin1String("ProtocolAiotFsensorCaliData")) {
        const auto d = payload.value<ProtocolAiotFsensorCaliData>();
        if (field == QLatin1String("calibrated") || field == QLatin1String("flag")) {
            ok = true;
            return d.calibrated;
        }
    } else if (reportType == QLatin1String("ProtocolAiotExceptionThresholdData")) {
        const auto d = payload.value<ProtocolAiotExceptionThresholdData>();
        if (d.items.isEmpty())
            return 0.0;
        const auto& item = d.items.first();
        if (field == QLatin1String("type")) {
            ok = true;
            return item.type;
        }
        if (field == QLatin1String("value") || field == QLatin1String("percent") || field == QLatin1String("voltageMv")
            || field == QLatin1String("seconds") || field == QLatin1String("currentMa")
            || field == QLatin1String("tempLow") || field == QLatin1String("low")) {
            ok = true;
            return item.value;
        }
        if (field == QLatin1String("valueHigh") || field == QLatin1String("tempHigh") || field == QLatin1String("high")) {
            ok = true;
            return item.valueHigh;
        }
    } else if (reportType == QLatin1String("ProtocolAiotPumpParamData")) {
        const auto d = payload.value<ProtocolAiotPumpParamData>();
        if (field == QLatin1String("circleNum") || field == QLatin1String("loops")
            || field == QLatin1String("pump_run_time") || field == QLatin1String("pump_circle_num")) {
            ok = true;
            return d.circleNum;
        }
        if (field == QLatin1String("durationTime") || field == QLatin1String("pump_duration_time")) {
            ok = true;
            return d.durationTime;
        }
        if (field == QLatin1String("intervalTime") || field == QLatin1String("pump_interval_time")) {
            ok = true;
            return d.intervalTime;
        }
        if (field == QLatin1String("valveEnableTime") || field == QLatin1String("value_enable_time")
            || field == QLatin1String("valve_enable_time")) {
            ok = true;
            return d.valveEnableTime;
        }
        if (field == QLatin1String("valveDisableTime") || field == QLatin1String("value_disable_time")
            || field == QLatin1String("valve_disable_time")) {
            ok = true;
            return d.valveDisableTime;
        }
        if (field == QLatin1String("pumpPwm") || field == QLatin1String("pump_pwm_value")) {
            ok = true;
            return d.pumpPwm;
        }
        if (field == QLatin1String("valvePwm") || field == QLatin1String("value_pwm_value")
            || field == QLatin1String("valve_pwm_value")) {
            ok = true;
            return d.valvePwm;
        }
    } else if (reportType == QLatin1String("ProtocolAiotHeatTestData")) {
        const auto d = payload.value<ProtocolAiotHeatTestData>();
        if (field == QLatin1String("enable") || field == QLatin1String("heat_enable")) {
            ok = true;
            return d.enable;
        }
        if (field == QLatin1String("driveStrength") || field == QLatin1String("heat_drive_strength")
            || field == QLatin1String("strength") || field == QLatin1String("pwm")) {
            ok = true;
            return d.driveStrength;
        }
        if (field == QLatin1String("durationTime") || field == QLatin1String("heat_duration_time")
            || field == QLatin1String("duration")) {
            ok = true;
            return d.durationTime;
        }
    } else if (reportType == QLatin1String("ProtocolAiotVibrationTestData")) {
        const auto d = payload.value<ProtocolAiotVibrationTestData>();
        if (field == QLatin1String("enable") || field == QLatin1String("vibration_enable")) {
            ok = true;
            return d.enable;
        }
        if (field == QLatin1String("driveStrength") || field == QLatin1String("vibration_drive_strength")
            || field == QLatin1String("strength") || field == QLatin1String("pwm")) {
            ok = true;
            return d.driveStrength;
        }
        if (field == QLatin1String("freq") || field == QLatin1String("vibration_freq")
            || field == QLatin1String("frequency")) {
            ok = true;
            return d.freq;
        }
        if (field == QLatin1String("durationTime") || field == QLatin1String("vibration_duration_time")
            || field == QLatin1String("duration")) {
            ok = true;
            return d.durationTime;
        }
    } else if (reportType == QLatin1String("ProtocolAiotCycleReportConfigData")) {
        const auto d = payload.value<ProtocolAiotCycleReportConfigData>();
        if (field == QLatin1String("enable") || field == QLatin1String("cycle_report_enable")) {
            ok = true;
            return d.enable;
        }
        if (d.items.isEmpty())
            return 0;
        const auto& item = d.items.first();
        if (field == QLatin1String("dataType") || field == QLatin1String("type")
            || field == QLatin1String("report_data_type")) {
            ok = true;
            return item.dataType;
        }
        if (field == QLatin1String("intervalTime") || field == QLatin1String("report_interval_time")
            || field == QLatin1String("interval")) {
            ok = true;
            return item.intervalTime;
        }
    } else if (reportType == QLatin1String("ProtocolAiotCycleReportData")) {
        const auto d = payload.value<ProtocolAiotCycleReportData>();
        if (d.items.isEmpty())
            return 0;
        const auto& item = d.items.first();
        if (field == QLatin1String("dataType") || field == QLatin1String("type")
            || field == QLatin1String("report_data_type")) {
            ok = true;
            return item.dataType;
        }
        if (field == QLatin1String("accX")) {
            ok = true;
            return item.accX;
        }
        if (field == QLatin1String("accY")) {
            ok = true;
            return item.accY;
        }
        if (field == QLatin1String("accZ")) {
            ok = true;
            return item.accZ;
        }
        if (field == QLatin1String("gyroX")) {
            ok = true;
            return item.gyroX;
        }
        if (field == QLatin1String("gyroY")) {
            ok = true;
            return item.gyroY;
        }
        if (field == QLatin1String("gyroZ")) {
            ok = true;
            return item.gyroZ;
        }
        if (field == QLatin1String("pressureOut") || field == QLatin1String("p_out")) {
            ok = true;
            return item.pressureOut;
        }
        if (field == QLatin1String("pressureIn") || field == QLatin1String("p_in")) {
            ok = true;
            return item.pressureIn;
        }
        if (field == QLatin1String("flowRate") || field == QLatin1String("flow_rate")) {
            ok = true;
            return item.flowRate;
        }
        if (field == QLatin1String("distanceMm") || field == QLatin1String("distance")) {
            ok = true;
            return item.distanceMm;
        }
        if (field == QLatin1String("adcRaw") || field == QLatin1String("adc")) {
            ok = true;
            return item.adcRaw;
        }
        if (field == QLatin1String("irLevel") || field == QLatin1String("ir_level")) {
            ok = true;
            return item.irLevel;
        }
        if (field == QLatin1String("impedance")) {
            ok = true;
            return item.impedance;
        }
        if (field == QLatin1String("levelMm") || field == QLatin1String("level")) {
            ok = true;
            return item.levelMm;
        }
        if (field == QLatin1String("temperatureC") || field == QLatin1String("temperature")) {
            ok = true;
            return item.temperatureC;
        }
        if (field == QLatin1String("humidity")) {
            ok = true;
            return item.humidity;
        }
        if (field == QLatin1String("currentMa") || field == QLatin1String("current")) {
            ok = true;
            return item.currentMa;
        }
        if (field == QLatin1String("hallState") || field == QLatin1String("hall")) {
            ok = true;
            return item.hallState;
        }
        if (field == QLatin1String("pulseCount") || field == QLatin1String("pulse")) {
            ok = true;
            return item.pulseCount;
        }
    } else if (reportType == QLatin1String("ProtocolBaseInfoData")) {
        const auto d = payload.value<ProtocolBaseInfoData>();
        if (field == QLatin1String("ageing_state")) {
            ok = true;
            return d.ageing_state;
        }
    } else if (reportType == QLatin1String("ProtocolPeriphStateData")) {
        const auto d = payload.value<ProtocolPeriphStateData>();
        if (field == QLatin1String("press0_state")) {
            ok = true;
            return d.press0_state;
        }
        if (field == QLatin1String("press1_state")) {
            ok = true;
            return d.press1_state;
        }
        if (field == QLatin1String("battery_ic_state")) {
            ok = true;
            return d.battery_ic_state;
        }
        if (field == QLatin1String("touch_ic_state")) {
            ok = true;
            return d.touch_ic_state;
        }
        if (field == QLatin1String("led_ic_state")) {
            ok = true;
            return d.led_ic_state;
        }
        if (field == QLatin1String("pd_ic_state")) {
            ok = true;
            return d.pd_ic_state;
        }
    } else if (reportType == QLatin1String("ProtocolButtonStateData")) {
        const auto d = payload.value<ProtocolButtonStateData>();
        if (field == QLatin1String("modeButtonState")) {
            ok = true;
            return d.modeButtonState;
        }
        if (field == QLatin1String("powerButtonState")) {
            ok = true;
            return d.powerButtonState;
        }
        if (field == QLatin1String("keyButtonId")) {
            ok = true;
            return d.keyButtonId;
        }
    } else if (reportType == QLatin1String("ProtocolAgingStatusData")) {
        const auto d = payload.value<ProtocolAgingStatusData>();
        if (field == QLatin1String("status")) {
            ok = true;
            return d.status;
        }
        if (field == QLatin1String("loops")) {
            ok = true;
            return d.loops;
        }
        if (field == QLatin1String("seconds")) {
            ok = true;
            return static_cast<double>(d.seconds);
        }
    } else if (reportType == QLatin1String("ProtocolMusicStateData")) {
        const auto d = payload.value<ProtocolMusicStateData>();
        if (field == QLatin1String("musicState")) {
            ok = true;
            return d.musicState;
        }
    } else if (reportType == QLatin1String("ProtocolResultData")) {
        const auto d = payload.value<ProtocolResultData>();
        if (field == QLatin1String("result")) {
            ok = true;
            return d.result;
        }
    } else if (reportType == QLatin1String("ProtocolTypeData") || reportType == QLatin1String("ProtocolBatteryTempData")
               || reportType == QLatin1String("ProtocolFlangeData")
               || reportType == QLatin1String("ProtocolHeatTempData")) {
        const auto d = payload.value<ProtocolTypeData>();
        if (field == QLatin1String("type")) {
            ok = true;
            return d.type;
        }
    } else if (reportType == QLatin1String("ProtocolFixturePcbaData")) {
        QVariantMap m = payload.toMap();
        if (m.isEmpty()) {
            const FixturePacketData pack = payload.value<FixturePacketData>();
            m.insert(QStringLiteral("machineNumber"), pack.machineNumber);
            m.insert(QStringLiteral("staticCurrent"), pack.staticCurrent);
            m.insert(QStringLiteral("workingCurrent"), pack.workingCurrent);
            m.insert(QStringLiteral("chargingCurrent"), pack.chargingCurrent);
            m.insert(QStringLiteral("musicCurrent"), pack.musicCurrent);
            m.insert(QStringLiteral("standbyCurrentUa"), pack.standbyCurrentUa);
            m.insert(QStringLiteral("pumpVoltageMv"), pack.pumpVoltageMv);
            m.insert(QStringLiteral("mcuVoltageMv"), pack.mcuVoltageMv);
            m.insert(QStringLiteral("valveVoltageMv"), pack.valveVoltageMv);
            m.insert(QStringLiteral("button1"), pack.button1);
            m.insert(QStringLiteral("button2"), pack.button2);
            m.insert(QStringLiteral("overVoltageLight"), pack.overVoltageLight);
            m.insert(QStringLiteral("fixerro"), pack.fixerro);
        }
        if (m.contains(field)) {
            ok = true;
            return m.value(field).toDouble();
        }
    } else if (reportType == QLatin1String("ProtocolJieliBtBoxData")) {
        QVariantMap m = payload.toMap();
        if (m.isEmpty() && payload.canConvert<ProtocolJieliBtBoxData>()) {
            const auto d = payload.value<ProtocolJieliBtBoxData>();
            m.insert(QStringLiteral("rssi"), d.rssi);
            m.insert(QStringLiteral("freqOffset"), d.freqOffset);
            m.insert(QStringLiteral("mac"), d.mac);
        }
        // mac 为文本字段，走 fieldStringFromVariant；数值路径不处理
        if (field == QLatin1String("mac")) {
            ok = false;
            return 0;
        }
        if (m.contains(field)) {
            ok = true;
            return m.value(field).toDouble();
        }
    } else if (reportType == QLatin1String("ProtocolMeasureData")) {
        const auto d = payload.value<ProtocolMeasureData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return d.value;
        }
    } else if (reportType == QLatin1String("ProtocolDongleSuctionData")) {
        const auto d = payload.value<ProtocolDongleSuctionData>();
        if (field == QLatin1String("ch1Kpa") || field == QLatin1String("leftKpa")) {
            ok = true;
            return d.ch1Kpa;
        }
        if (field == QLatin1String("ch2Kpa") || field == QLatin1String("rightKpa")) {
            ok = true;
            return d.ch2Kpa;
        }
        if (field == QLatin1String("ch3Kpa") || field == QLatin1String("thirdKpa")) {
            ok = true;
            return d.ch3Kpa;
        }
    } else if (reportType == QLatin1String("ProtocolDongleSuctionPeakData")) {
        const auto d = payload.value<ProtocolDongleSuctionPeakData>();
        if (field == QLatin1String("peakKpa")) {
            ok = true;
            return d.peakKpa;
        }
        if (field == QLatin1String("highKpa")) {
            ok = true;
            return d.highKpa;
        }
        if (field == QLatin1String("peakDiffKpa")) {
            ok = true;
            return d.peakDiffKpa;
        }
        if (field == QLatin1String("ch1PeakKpa") || field == QLatin1String("leftPeakKpa")) {
            ok = true;
            return d.ch1PeakKpa;
        }
        if (field == QLatin1String("ch2PeakKpa") || field == QLatin1String("rightPeakKpa")) {
            ok = true;
            return d.ch2PeakKpa;
        }
        if (field == QLatin1String("sideDiffKpa") || field == QLatin1String("peakSpanKpa")) {
            ok = true;
            return d.sideDiffKpa;
        }
        if (field == QLatin1String("peakCount")) {
            ok = true;
            return d.peakCount;
        }
    } else if (reportType == QLatin1String("ProtocolScreenInspectData")) {
        const auto d = payload.value<ProtocolScreenInspectData>();
        if (field == QLatin1String("deadPixels")) {
            ok = true;
            return d.deadPixels;
        }
        if (field == QLatin1String("muraStd")) {
            ok = true;
            return d.muraStd;
        }
        if (field == QLatin1String("ssim") || field == QLatin1String("similarity")) {
            ok = true;
            return d.ssim;
        }
        if (field == QLatin1String("detectedColor")) {
            ok = true;
            return d.detectedColor;
        }
        if (field == QLatin1String("colorMatch")) {
            ok = true;
            return d.colorMatch;
        }
    }
    return 0.0;
}

QString fieldStringFromVariant(const QString& reportType, const QString& field, const QVariant& payload, bool& ok) {
    ok = false;
    if (reportType == QLatin1String("ProtocolRssiData")) {
        const auto d = payload.value<ProtocolRssiData>();
        if (field == QLatin1String("dbm")) {
            ok = true;
            return QString::number(d.dbm);
        }
    } else if (reportType == QLatin1String("ProtocolBatteryData")) {
        const auto d = payload.value<ProtocolBatteryData>();
        if (field == QLatin1String("percent")) {
            ok = true;
            return QString::number(d.percent);
        }
        if (field == QLatin1String("voltageMv")) {
            ok = true;
            return QString::number(d.voltageMv);
        }
        if (field == QLatin1String("currentMa")) {
            ok = true;
            return QString::number(d.currentMa);
        }
        if (field == QLatin1String("temperatureC")) {
            ok = true;
            return QString::number(d.temperatureC);
        }
    } else if (reportType == QLatin1String("ProtocolAiotImuCaliData")) {
        const auto d = payload.value<ProtocolAiotImuCaliData>();
        auto num = [&](float v) {
            ok = true;
            return QString::number(v, 'g', 8);
        };
        if (field == QLatin1String("kx"))
            return num(d.kx);
        if (field == QLatin1String("ky"))
            return num(d.ky);
        if (field == QLatin1String("kz"))
            return num(d.kz);
        if (field == QLatin1String("syx"))
            return num(d.syx);
        if (field == QLatin1String("szx"))
            return num(d.szx);
        if (field == QLatin1String("szy"))
            return num(d.szy);
        if (field == QLatin1String("bx"))
            return num(d.bx);
        if (field == QLatin1String("by"))
            return num(d.by);
        if (field == QLatin1String("bz"))
            return num(d.bz);
    } else if (reportType == QLatin1String("ProtocolAiotFsensorCaliData")) {
        const auto d = payload.value<ProtocolAiotFsensorCaliData>();
        if (field == QLatin1String("calibrated") || field == QLatin1String("flag")) {
            ok = true;
            return QString::number(d.calibrated);
        }
    } else if (reportType == QLatin1String("ProtocolAiotExceptionThresholdData")) {
        const auto d = payload.value<ProtocolAiotExceptionThresholdData>();
        if (d.items.isEmpty())
            return {};
        const auto& item = d.items.first();
        if (field == QLatin1String("type")) {
            ok = true;
            return QString::number(item.type);
        }
        if (field == QLatin1String("value") || field == QLatin1String("percent") || field == QLatin1String("voltageMv")
            || field == QLatin1String("seconds") || field == QLatin1String("currentMa")
            || field == QLatin1String("tempLow") || field == QLatin1String("low")) {
            ok = true;
            return QString::number(item.value);
        }
        if (field == QLatin1String("valueHigh") || field == QLatin1String("tempHigh") || field == QLatin1String("high")) {
            ok = true;
            return QString::number(item.valueHigh);
        }
    } else if (reportType == QLatin1String("ProtocolAiotPumpParamData")) {
        const auto d = payload.value<ProtocolAiotPumpParamData>();
        if (field == QLatin1String("circleNum") || field == QLatin1String("loops")
            || field == QLatin1String("pump_run_time") || field == QLatin1String("pump_circle_num")) {
            ok = true;
            return QString::number(d.circleNum);
        }
        if (field == QLatin1String("durationTime") || field == QLatin1String("pump_duration_time")) {
            ok = true;
            return QString::number(d.durationTime);
        }
        if (field == QLatin1String("intervalTime") || field == QLatin1String("pump_interval_time")) {
            ok = true;
            return QString::number(d.intervalTime);
        }
        if (field == QLatin1String("valveEnableTime") || field == QLatin1String("value_enable_time")
            || field == QLatin1String("valve_enable_time")) {
            ok = true;
            return QString::number(d.valveEnableTime);
        }
        if (field == QLatin1String("valveDisableTime") || field == QLatin1String("value_disable_time")
            || field == QLatin1String("valve_disable_time")) {
            ok = true;
            return QString::number(d.valveDisableTime);
        }
        if (field == QLatin1String("pumpPwm") || field == QLatin1String("pump_pwm_value")) {
            ok = true;
            return QString::number(d.pumpPwm);
        }
        if (field == QLatin1String("valvePwm") || field == QLatin1String("value_pwm_value")
            || field == QLatin1String("valve_pwm_value")) {
            ok = true;
            return QString::number(d.valvePwm);
        }
    } else if (reportType == QLatin1String("ProtocolAiotHeatTestData")) {
        const auto d = payload.value<ProtocolAiotHeatTestData>();
        if (field == QLatin1String("enable") || field == QLatin1String("heat_enable")) {
            ok = true;
            return QString::number(d.enable);
        }
        if (field == QLatin1String("driveStrength") || field == QLatin1String("heat_drive_strength")
            || field == QLatin1String("strength") || field == QLatin1String("pwm")) {
            ok = true;
            return QString::number(d.driveStrength);
        }
        if (field == QLatin1String("durationTime") || field == QLatin1String("heat_duration_time")
            || field == QLatin1String("duration")) {
            ok = true;
            return QString::number(d.durationTime);
        }
    } else if (reportType == QLatin1String("ProtocolAiotVibrationTestData")) {
        const auto d = payload.value<ProtocolAiotVibrationTestData>();
        if (field == QLatin1String("enable") || field == QLatin1String("vibration_enable")) {
            ok = true;
            return QString::number(d.enable);
        }
        if (field == QLatin1String("driveStrength") || field == QLatin1String("vibration_drive_strength")
            || field == QLatin1String("strength") || field == QLatin1String("pwm")) {
            ok = true;
            return QString::number(d.driveStrength);
        }
        if (field == QLatin1String("freq") || field == QLatin1String("vibration_freq")
            || field == QLatin1String("frequency")) {
            ok = true;
            return QString::number(d.freq);
        }
        if (field == QLatin1String("durationTime") || field == QLatin1String("vibration_duration_time")
            || field == QLatin1String("duration")) {
            ok = true;
            return QString::number(d.durationTime);
        }
    } else if (reportType == QLatin1String("ProtocolAiotCycleReportConfigData")) {
        const auto d = payload.value<ProtocolAiotCycleReportConfigData>();
        if (field == QLatin1String("enable") || field == QLatin1String("cycle_report_enable")) {
            ok = true;
            return QString::number(d.enable);
        }
        if (d.items.isEmpty())
            return {};
        const auto& item = d.items.first();
        if (field == QLatin1String("dataType") || field == QLatin1String("type")
            || field == QLatin1String("report_data_type")) {
            ok = true;
            return QString::number(item.dataType);
        }
        if (field == QLatin1String("intervalTime") || field == QLatin1String("report_interval_time")
            || field == QLatin1String("interval")) {
            ok = true;
            return QString::number(item.intervalTime);
        }
    } else if (reportType == QLatin1String("ProtocolAiotCycleReportData")) {
        const auto d = payload.value<ProtocolAiotCycleReportData>();
        if (d.items.isEmpty())
            return {};
        const auto& item = d.items.first();
        if (field == QLatin1String("dataType") || field == QLatin1String("type")
            || field == QLatin1String("report_data_type")) {
            ok = true;
            return QString::number(item.dataType);
        }
        if (field == QLatin1String("accX")) {
            ok = true;
            return QString::number(item.accX);
        }
        if (field == QLatin1String("accY")) {
            ok = true;
            return QString::number(item.accY);
        }
        if (field == QLatin1String("accZ")) {
            ok = true;
            return QString::number(item.accZ);
        }
        if (field == QLatin1String("gyroX")) {
            ok = true;
            return QString::number(item.gyroX);
        }
        if (field == QLatin1String("gyroY")) {
            ok = true;
            return QString::number(item.gyroY);
        }
        if (field == QLatin1String("gyroZ")) {
            ok = true;
            return QString::number(item.gyroZ);
        }
        if (field == QLatin1String("pressureOut") || field == QLatin1String("p_out")) {
            ok = true;
            return QString::number(item.pressureOut);
        }
        if (field == QLatin1String("pressureIn") || field == QLatin1String("p_in")) {
            ok = true;
            return QString::number(item.pressureIn);
        }
        if (field == QLatin1String("flowRate") || field == QLatin1String("flow_rate")) {
            ok = true;
            return QString::number(item.flowRate);
        }
        if (field == QLatin1String("distanceMm") || field == QLatin1String("distance")) {
            ok = true;
            return QString::number(item.distanceMm);
        }
        if (field == QLatin1String("adcRaw") || field == QLatin1String("adc")) {
            ok = true;
            return QString::number(item.adcRaw);
        }
        if (field == QLatin1String("irLevel") || field == QLatin1String("ir_level")) {
            ok = true;
            return QString::number(item.irLevel);
        }
        if (field == QLatin1String("impedance")) {
            ok = true;
            return QString::number(item.impedance);
        }
        if (field == QLatin1String("levelMm") || field == QLatin1String("level")) {
            ok = true;
            return QString::number(item.levelMm);
        }
        if (field == QLatin1String("temperatureC") || field == QLatin1String("temperature")) {
            ok = true;
            return QString::number(item.temperatureC);
        }
        if (field == QLatin1String("humidity")) {
            ok = true;
            return QString::number(item.humidity);
        }
        if (field == QLatin1String("currentMa") || field == QLatin1String("current")) {
            ok = true;
            return QString::number(item.currentMa);
        }
        if (field == QLatin1String("hallState") || field == QLatin1String("hall")) {
            ok = true;
            return QString::number(item.hallState);
        }
        if (field == QLatin1String("pulseCount") || field == QLatin1String("pulse")) {
            ok = true;
            return QString::number(item.pulseCount);
        }
    } else if (reportType == QLatin1String("ProtocolSnData")) {
        const auto d = payload.value<ProtocolSnData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return d.value.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolBaseInfoData")) {
        const auto d = payload.value<ProtocolBaseInfoData>();
        if (field == QLatin1String("soft_version")) {
            ok = true;
            return d.soft_version.trimmed();
        }
        if (field == QLatin1String("res_version")) {
            ok = true;
            return d.res_version.trimmed();
        }
        if (field == QLatin1String("product_name")) {
            ok = true;
            return d.product_name.trimmed();
        }
        if (field == QLatin1String("hw_version")) {
            ok = true;
            return d.hw_version.trimmed();
        }
        if (field == QLatin1String("algo_version")) {
            ok = true;
            return d.algo_version.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolTupleData")) {
        const auto d = payload.value<ProtocolTupleData>();
        if (field == QLatin1String("productId")) {
            ok = true;
            return d.productId.trimmed();
        }
        if (field == QLatin1String("deviceId")) {
            ok = true;
            return d.deviceId.trimmed();
        }
        if (field == QLatin1String("key")) {
            ok = true;
            return d.key.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolMacData")) {
        const auto d = payload.value<ProtocolMacData>();
        if (field == QLatin1String("mac")) {
            ok = true;
            return d.mac.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolJieliBtBoxData")) {
        QVariantMap m = payload.toMap();
        if (m.isEmpty() && payload.canConvert<ProtocolJieliBtBoxData>()) {
            const auto d = payload.value<ProtocolJieliBtBoxData>();
            m.insert(QStringLiteral("rssi"), d.rssi);
            m.insert(QStringLiteral("freqOffset"), d.freqOffset);
            m.insert(QStringLiteral("mac"), d.mac);
        }
        if (field == QLatin1String("mac")) {
            ok = true;
            return m.value(QStringLiteral("mac")).toString().trimmed();
        }
        if (field == QLatin1String("rssi") || field == QLatin1String("freqOffset")) {
            ok = true;
            return QString::number(m.value(field).toLongLong());
        }
    } else if (reportType == QLatin1String("ProtocolTypeData") || reportType == QLatin1String("ProtocolBatteryTempData")
               || reportType == QLatin1String("ProtocolFlangeData")
               || reportType == QLatin1String("ProtocolHeatTempData")) {
        const auto d = payload.value<ProtocolTypeData>();
        if (field == QLatin1String("type")) {
            ok = true;
            return QString::number(d.type);
        }
    } else if (reportType == QLatin1String("ProtocolPumpStallCurrentData")) {
        const auto d = payload.value<ProtocolPumpStallCurrentData>();
        if (field == QLatin1String("adcValue")) {
            ok = true;
            return QString::number(d.adcValue);
        }
    } else if (reportType == QLatin1String("ProtocolRootAgingHistoryData")) {
        const auto d = payload.value<ProtocolRootAgingHistoryData>();
        if (field == QLatin1String("status") || field == QLatin1String("enable")) {
            ok = true;
            return QString::number(d.status);
        }
        if (field == QLatin1String("finishedFlag") || field == QLatin1String("finished")) {
            ok = true;
            return QString::number(d.finishedFlag);
        }
        if (field == QLatin1String("agingCount") || field == QLatin1String("count")
            || field == QLatin1String("loops")) {
            ok = true;
            return QString::number(d.agingCount);
        }
        if (field == QLatin1String("batteryMaxTempC") || field == QLatin1String("batteryMaxTemp")) {
            ok = true;
            return QString::number(d.batteryMaxTempC);
        }
        if (field == QLatin1String("flangeMaxTempC") || field == QLatin1String("flangeMaxTemp")) {
            ok = true;
            return QString::number(d.flangeMaxTempC);
        }
        if (field == QLatin1String("stallCount")) {
            ok = true;
            return QString::number(d.stallCount);
        }
        if (field == QLatin1String("stallThreshold")) {
            ok = true;
            return QString::number(d.stallThreshold);
        }
        if (field.startsWith(QLatin1String("stallCurrent"))) {
            bool idxOk = false;
            const int idx = field.mid(QStringLiteral("stallCurrent").size()).toInt(&idxOk);
            if (idxOk && idx >= 0 && idx < 5) {
                ok = true;
                return QString::number(d.stallCurrents[idx]);
            }
        }
    } else if (reportType == QLatin1String("ProtocolPeriphStateData")) {
        const auto d = payload.value<ProtocolPeriphStateData>();
        if (field == QLatin1String("press0_state")) {
            ok = true;
            return QString::number(d.press0_state);
        }
        if (field == QLatin1String("press1_state")) {
            ok = true;
            return QString::number(d.press1_state);
        }
        if (field == QLatin1String("battery_ic_state")) {
            ok = true;
            return QString::number(d.battery_ic_state);
        }
        if (field == QLatin1String("touch_ic_state")) {
            ok = true;
            return QString::number(d.touch_ic_state);
        }
        if (field == QLatin1String("led_ic_state")) {
            ok = true;
            return QString::number(d.led_ic_state);
        }
        if (field == QLatin1String("pd_ic_state")) {
            ok = true;
            return QString::number(d.pd_ic_state);
        }
    } else if (reportType == QLatin1String("ProtocolButtonStateData")) {
        const auto d = payload.value<ProtocolButtonStateData>();
        if (field == QLatin1String("modeButtonState")) {
            ok = true;
            return QString::number(d.modeButtonState);
        }
        if (field == QLatin1String("powerButtonState")) {
            ok = true;
            return QString::number(d.powerButtonState);
        }
        if (field == QLatin1String("keyButtonId")) {
            ok = true;
            return QString::number(d.keyButtonId);
        }
    } else if (reportType == QLatin1String("ProtocolMeasureData")) {
        const auto d = payload.value<ProtocolMeasureData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return QString::number(d.value);
        }
        if (field == QLatin1String("valueText")) {
            ok = true;
            return d.valueText.trimmed();
        }
        if (field == QLatin1String("deviceName")) {
            ok = true;
            return d.deviceName.trimmed();
        }
        if (field == QLatin1String("channel")) {
            ok = true;
            return d.channel.trimmed();
        }
        if (field == QLatin1String("type")) {
            ok = true;
            return d.type.trimmed();
        }
        if (field == QLatin1String("unit")) {
            ok = true;
            return d.unit.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolDongleSuctionData")) {
        const auto d = payload.value<ProtocolDongleSuctionData>();
        if (field == QLatin1String("ch1Kpa") || field == QLatin1String("leftKpa")) {
            ok = true;
            return QString::number(d.ch1Kpa, 'f', 2);
        }
        if (field == QLatin1String("ch2Kpa") || field == QLatin1String("rightKpa")) {
            ok = true;
            return QString::number(d.ch2Kpa, 'f', 2);
        }
        if (field == QLatin1String("ch3Kpa") || field == QLatin1String("thirdKpa")) {
            ok = true;
            return QString::number(d.ch3Kpa, 'f', 2);
        }
    } else if (reportType == QLatin1String("ProtocolDongleSuctionPeakData")) {
        const auto d = payload.value<ProtocolDongleSuctionPeakData>();
        if (field == QLatin1String("peakKpa")) {
            ok = true;
            return QString::number(d.peakKpa, 'f', 3);
        }
        if (field == QLatin1String("highKpa")) {
            ok = true;
            return QString::number(d.highKpa, 'f', 3);
        }
        if (field == QLatin1String("peakDiffKpa")) {
            ok = true;
            return QString::number(d.peakDiffKpa, 'f', 3);
        }
        if (field == QLatin1String("ch1PeakKpa") || field == QLatin1String("leftPeakKpa")) {
            ok = true;
            return QString::number(d.ch1PeakKpa, 'f', 3);
        }
        if (field == QLatin1String("ch2PeakKpa") || field == QLatin1String("rightPeakKpa")) {
            ok = true;
            return QString::number(d.ch2PeakKpa, 'f', 3);
        }
        if (field == QLatin1String("sideDiffKpa") || field == QLatin1String("peakSpanKpa")) {
            ok = true;
            return QString::number(d.sideDiffKpa, 'f', 3);
        }
        if (field == QLatin1String("peakCount")) {
            ok = true;
            return QString::number(d.peakCount);
        }
    } else if (reportType == QLatin1String("ProtocolScreenInspectData")) {
        const auto d = payload.value<ProtocolScreenInspectData>();
        if (field == QLatin1String("deadPixels")) {
            ok = true;
            return QString::number(d.deadPixels);
        }
        if (field == QLatin1String("muraStd")) {
            ok = true;
            return QString::number(d.muraStd, 'f', 1);
        }
        if (field == QLatin1String("ssim") || field == QLatin1String("similarity")) {
            ok = true;
            return QString::number(d.ssim, 'f', 3);
        }
        if (field == QLatin1String("detectedColor")) {
            ok = true;
            return ScreenInspectAnalyzer::colorName(d.detectedColor);
        }
        if (field == QLatin1String("colorMatch")) {
            ok = true;
            if (d.colorMatch < 0)
                return QStringLiteral("未指定");
            return d.colorMatch == 1 ? QStringLiteral("是") : QStringLiteral("否");
        }
    } else if (reportType == QLatin1String("ProtocolFactoryDoneData")) {
        const auto d = payload.value<ProtocolFactoryDoneData>();
        if (field == QLatin1String("done")) {
            ok = true;
            return d.done ? QStringLiteral("已完成") : QStringLiteral("未完成");
        }
    }
    return {};
}

/** 吸力峰判定字段展示用「最低峰值」，其余 Gate 仍用「当前值」。 */
QString gateActualValueLabel(const QString& reportType, const QString& field) {
    if (reportType == QLatin1String("ProtocolDongleSuctionPeakData")
        && (field == QLatin1String("peakKpa") || field == QLatin1String("ch1PeakKpa")
            || field == QLatin1String("leftPeakKpa") || field == QLatin1String("ch2PeakKpa")
            || field == QLatin1String("rightPeakKpa")))
        return QStringLiteral("最低峰值");
    return QStringLiteral("当前值");
}

} // namespace

QStringList GateRegistry::reportTypes() {
    QStringList list;
    for (const auto& t : kTypes)
        list.append(t.reportType);
    return list;
}

QVector<GateTypeDescriptor> GateRegistry::allTypeDescriptors() {
    return kTypes;
}

GateSendBinding GateRegistry::bindingForSend(TestCaseSendChannel channel, const QString& protocolOrDevice,
                                             const QString& deviceCmd) {
    GateSendBinding out;
    const QString cmd = deviceCmd.trimmed();
    if (cmd.isEmpty())
        return out;

    const auto applyRow = [&out](const char* typesCsv, const char* defaultField) {
        if (typesCsv && typesCsv[0] != '\0') {
            const QStringList parts = QString::fromLatin1(typesCsv).split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (QString t : parts) {
                t = t.trimmed();
                if (!t.isEmpty())
                    out.reportTypes.append(t);
            }
        }
        if (defaultField && defaultField[0] != '\0')
            out.defaultField = QString::fromLatin1(defaultField);
    };

    switch (channel) {
    case TestCaseSendChannel::Product:
        if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::Dongle:
        if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::Fixture: {
        const TestCaseFixtureProtocol proto = FixturePcbaCmdCatalog::fixtureProtocolFromIni(protocolOrDevice);
        if (proto == TestCaseFixtureProtocol::JieliBtBox) {
            if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::Asd9026a) {
            if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::Pcba) {
            if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::UsbCamera) {
            if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::VesLight) {
            if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        }
        break;
    }
    case TestCaseSendChannel::Scpi:
        if (const ScpiCmdManifest::Row* row =
                ScpiCmdManifest::findByDeviceAndName(ScpiPeriphCmdCatalog::deviceFromIni(protocolOrDevice), cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::Modbus:
        if (const ModbusCmdManifest::Row* row =
                ModbusCmdManifest::findByDeviceAndName(ModbusPeriphCmdCatalog::deviceFromIni(protocolOrDevice), cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::ProductSerial:
    case TestCaseSendChannel::Cloud:
        break;
    }
    return out;
}

bool GateRegistry::descriptorFor(const QString& reportType, GateTypeDescriptor& out) {
    for (const auto& t : kTypes) {
        if (t.reportType == reportType) {
            out = t;
            return true;
        }
    }
    return false;
}

QStringList GateRegistry::fieldsFor(const QString& reportType) {
    GateTypeDescriptor d;
    if (!descriptorFor(reportType, d))
        return {};
    QStringList fields;
    for (const auto& f : d.fields)
        fields.append(f.field);
    return fields;
}

bool GateRegistry::isAllFieldsGateField(const QString& field) {
    const QString f = field.trimmed();
    return f.isEmpty() || f == QLatin1String("*") || f.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0;
}

QString GateRegistry::fieldDisplayName(const QString& reportType, const QString& field) {
    GateTypeDescriptor desc;
    if (!descriptorFor(reportType, desc))
        return field;
    for (const GateFieldDescriptor& fd : desc.fields) {
        if (fd.field == field)
            return fd.displayName;
    }
    return field;
}

namespace {

/** 卡控数值展示：屏幕纯色等字段用中文，其它保持数字。 */
QString formatGateFieldValue(const QString& reportType, const QString& field, double value) {
    if (reportType == QLatin1String("ProtocolScreenInspectData")) {
        if (field == QLatin1String("detectedColor"))
            return ScreenInspectAnalyzer::colorName(qRound(value));
        if (field == QLatin1String("colorMatch"))
            return qAbs(value + 1.0) < 0.0001
                       ? QStringLiteral("未指定")
                       : (qAbs(value - 1.0) < 0.0001 ? QStringLiteral("是") : QStringLiteral("否"));
    }
    if (reportType == QLatin1String("ProtocolFactoryDoneData") && field == QLatin1String("done"))
        return qAbs(value - 1.0) < 0.0001 ? QStringLiteral("已完成") : QStringLiteral("未完成");
    if (qAbs(value - qRound(value)) < 1e-9)
        return QString::number(qRound(value));
    return QString::number(value);
}

/** 大于/小于/等于：优先期望值，兼容旧 ini 只写了 Low。 */
double gateCompareThreshold(const TestCaseGate& gate) {
    const QString expected = gate.expected.trimmed();
    if (!expected.isEmpty()) {
        bool ok = false;
        const double parsed = expected.toDouble(&ok);
        if (ok)
            return parsed;
    }
    return gate.low;
}

/**
 * 「是否为期望纯色」阈值：支持 1/0、是/否。
 * Expected 非数字（如「是」）时绝不能回退成 Low=0，否则会误报「期望=否」。
 * Expected 空时默认期望匹配（1）。
 */
double colorMatchThreshold(const TestCaseGate& gate) {
    const QString e = gate.expected.trimmed();
    if (!e.isEmpty()) {
        if (e == QStringLiteral("是") || e.compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0
            || e.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
            return 1.0;
        if (e == QStringLiteral("否") || e.compare(QLatin1String("no"), Qt::CaseInsensitive) == 0
            || e.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0)
            return 0.0;
        bool ok = false;
        const double parsed = e.toDouble(&ok);
        if (ok)
            return parsed;
    }
    if (qAbs(gate.low - gate.high) < 0.0001 && (qAbs(gate.low) < 0.0001 || qAbs(gate.low - 1.0) < 0.0001))
        return gate.low;
    if (qAbs(gate.low - 1.0) < 0.0001 || qAbs(gate.high - 1.0) < 0.0001)
        return 1.0;
    return 1.0;
}

/** 产测完成标志：支持 1/0、已完成/未完成；空则默认期望已完成(1)。 */
double factoryDoneThreshold(const TestCaseGate& gate) {
    const QString e = gate.expected.trimmed();
    if (!e.isEmpty()) {
        if (e == QStringLiteral("已完成") || e == QStringLiteral("是")
            || e.compare(QLatin1String("yes"), Qt::CaseInsensitive) == 0
            || e.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0)
            return 1.0;
        if (e == QStringLiteral("未完成") || e == QStringLiteral("否")
            || e.compare(QLatin1String("no"), Qt::CaseInsensitive) == 0
            || e.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0)
            return 0.0;
        bool ok = false;
        const double parsed = e.toDouble(&ok);
        if (ok)
            return parsed;
    }
    return 1.0;
}

} // namespace

bool GateRegistry::evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                            QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (!gate.enabled)
        return true;

    if (isAllFieldsGateField(gate.field)) {
        const QStringList fields = fieldsFor(reportType);
        if (fields.isEmpty()) {
            passOut = false;
            detailOut = QStringLiteral("回传类型无可用判定字段");
            return true;
        }
        bool allPass = true;
        QStringList parts;
        for (const QString& subField : fields) {
            TestCaseGate subGate = gate;
            subGate.field = subField;
            bool subPass = true;
            QString subDetail;
            evaluate(subGate, reportType, payload, subPass, subDetail);
            if (!subPass)
                allPass = false;
            parts.append(QStringLiteral("%1(%2)").arg(fieldDisplayName(reportType, subField), subDetail));
        }
        passOut = allPass;
        detailOut = parts.join(QStringLiteral("; "));
        return true;
    }

    // multi 仅为 ini 占位，须用 Gate/ItemN_Field；误评会得到「无法从上报数据读取字段」
    if (gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0) {
        passOut = false;
        detailOut = QStringLiteral("多项卡控未加载分项(请检查 Gate/Count 与 ItemN_Field)");
        return true;
    }

    bool ok = false;
    double value = fieldValueFromVariant(reportType, gate.field, payload, ok);
    // 步骤未写 Param_expectedColor 时 colorMatch=-1；展示曾误成「否」且 -1!=0 导致卡控失败
    if (ok && reportType == QLatin1String("ProtocolScreenInspectData")
        && gate.field == QLatin1String("colorMatch") && qAbs(value + 1.0) < 0.0001) {
        passOut = true;
        detailOut = QStringLiteral("当前=未指定期望色, 本项跳过");
        return true;
    }
    if (gate.op == TestCaseGateOp::CompareVersions) {
        bool strOk = false;
        QString actual = fieldStringFromVariant(reportType, gate.field, payload, strOk);
        if (!strOk) {
            passOut = false;
            detailOut = QStringLiteral("无法从上报数据读取文本字段");
            return true;
        }
        QString expected = gate.expected.trimmed();
        if (expected.isEmpty() && !gate.expectedSettingsKey.isEmpty())
            expected = SETTINGS.value(gate.expectedSettingsKey).toString().trimmed();
        // 未在 case ini 配置期望时跳过比对（不再回退 上位机设置.ini 的 ProductInfo/Software_Version）
        if (expected.isEmpty()) {
            passOut = true;
            detailOut = QStringLiteral("当前=%1, case 未配置 Gate/Expected").arg(actual);
            return true;
        }
        if (reportType == QLatin1String("ProtocolBaseInfoData") && gate.field == QLatin1String("soft_version") && gate.expected.isEmpty() && !gate.expectedSettingsKey.isEmpty() && !SETTINGS.value(QStringLiteral("ProductInfo/SoftwareVersion_checkBox"), true).toBool()) {
            passOut = true;
            detailOut = QStringLiteral("当前=%1, 未启用软件版本校验").arg(actual);
            return true;
        }
        passOut = CommonUtils::compareVersions(expected, actual);
        detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
        return true;
    }

    if (gate.op == TestCaseGateOp::Eq) {
        bool strOk = false;
        const QString actual = fieldStringFromVariant(reportType, gate.field, payload, strOk);
        if (strOk) {
            QString expected = gate.expected.trimmed();
            if (expected.isEmpty() && !gate.expectedSettingsKey.isEmpty())
                expected = SETTINGS.value(gate.expectedSettingsKey).toString().trimmed();
            if (expected.isEmpty()
                && !(reportType == QLatin1String("ProtocolFactoryDoneData")
                     && gate.field == QLatin1String("done"))) {
                passOut = false;
                detailOut = QStringLiteral("当前=%1, 未配置期望( Gate/Expected 或 MES/UI SN)").arg(actual.isEmpty() ? QStringLiteral("-") : actual);
            } else if ((reportType == QLatin1String("ProtocolMacData")
                        || reportType == QLatin1String("ProtocolJieliBtBoxData"))
                       && gate.field == QLatin1String("mac")) {
                auto normalizeMac = [](QString s) {
                    s.remove(QLatin1Char(':'));
                    s.remove(QLatin1Char('-'));
                    s.remove(QLatin1Char(' '));
                    return s.toUpper();
                };
                passOut = (normalizeMac(actual) == normalizeMac(expected));
                detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
            } else if (reportType == QLatin1String("ProtocolScreenInspectData")
                       && (gate.field == QLatin1String("detectedColor")
                           || gate.field == QLatin1String("colorMatch"))) {
                // 实测 fieldString 已是「红」等中文；ini 期望可为 "2" 或「红」
                double threshold = 0.0;
                if (gate.field == QLatin1String("colorMatch")) {
                    threshold = colorMatchThreshold(gate);
                } else {
                    bool colorOk = false;
                    const int colorIdx =
                        ScreenInspectAnalyzer::parseColorIndex(gate.expected.trimmed(), &colorOk);
                    if (colorOk && !gate.expected.trimmed().isEmpty())
                        threshold = colorIdx;
                    else
                        threshold = gateCompareThreshold(gate);
                }
                const QString expectText = formatGateFieldValue(reportType, gate.field, threshold);
                if (ok) {
                    passOut = qAbs(value - threshold) < 0.0001;
                    detailOut = QStringLiteral("当前=%1, 期望=%2")
                                    .arg(formatGateFieldValue(reportType, gate.field, value), expectText);
                } else {
                    // 数值读不到时退化为中文名比对（期望侧仍转文字，避免弹窗出现「期望=2」）
                    passOut = (actual == expectText);
                    detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expectText);
                }
            } else if (reportType == QLatin1String("ProtocolFactoryDoneData")
                       && gate.field == QLatin1String("done")) {
                // 实测文案为「已完成/未完成」；ini 期望可为 1/0 或中文，不能裸字符串比
                const double threshold = factoryDoneThreshold(gate);
                const QString expectText = formatGateFieldValue(reportType, gate.field, threshold);
                if (ok) {
                    passOut = qAbs(value - threshold) < 0.0001;
                    detailOut = QStringLiteral("当前=%1, 期望=%2")
                                    .arg(formatGateFieldValue(reportType, gate.field, value), expectText);
                } else {
                    passOut = (actual == expectText);
                    detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expectText);
                }
            } else {
                passOut = (actual == expected);
                detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
            }
            return true;
        }
    }

    if (!ok) {
        passOut = false;
        detailOut = QStringLiteral("无法从上报数据读取字段");
        return true;
    }

    double low = gate.low;
    double high = gate.high;
    resolveRangeBounds(gate, low, high);
    const QString valueText = formatGateFieldValue(reportType, gate.field, value);
    const double threshold = gateCompareThreshold(gate);
    const QString thresholdText = formatGateFieldValue(reportType, gate.field, threshold);

    switch (gate.op) {
    case TestCaseGateOp::Gt:
        passOut = value > threshold;
        detailOut = QStringLiteral("%1=%2, 要求>%3")
                        .arg(gateActualValueLabel(reportType, gate.field), valueText, thresholdText);
        return true;
    case TestCaseGateOp::Lt:
        passOut = value < threshold;
        detailOut = QStringLiteral("%1=%2, 要求<%3")
                        .arg(gateActualValueLabel(reportType, gate.field), valueText, thresholdText);
        return true;
    case TestCaseGateOp::Eq: {
        passOut = qAbs(value - threshold) < 0.0001;
        detailOut = QStringLiteral("%1=%2, 期望=%3")
                        .arg(gateActualValueLabel(reportType, gate.field), valueText, thresholdText);
        return true;
    }
    default: {
        const bool openRange = CommonUtils::isRssiOpenRangeGate(reportType, gate.field);
        passOut = openRange ? (value > low && value < high) : (value >= low && value <= high);
        detailOut = QStringLiteral("%1=%2, 允许%3%4,%5%6")
                        .arg(gateActualValueLabel(reportType, gate.field), valueText,
                             openRange ? QStringLiteral("(") : QStringLiteral("["),
                             formatGateFieldValue(reportType, gate.field, low),
                             formatGateFieldValue(reportType, gate.field, high),
                             openRange ? QStringLiteral(")") : QStringLiteral("]"));
        return true;
    }
    }
}

bool GateRegistry::evaluateAll(const QVector<TestCaseGate>& gates, const QString& reportType,
                               const QVariant& payload, bool& passOut, QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (gates.isEmpty())
        return true;
    bool allPass = true;
    QStringList parts;
    for (const TestCaseGate& g : gates) {
        TestCaseGate ge = g;
        ge.enabled = true;
        ge.reportType = reportType;
        bool subPass = true;
        QString subDetail;
        evaluate(ge, reportType, payload, subPass, subDetail);
        if (!subPass)
            allPass = false;
        parts.append(QStringLiteral("%1(%2)")
                         .arg(fieldDisplayName(reportType, ge.field), subDetail));
    }
    passOut = allPass;
    detailOut = parts.join(QStringLiteral("; "));
    return true;
}

void GateRegistry::resolveRangeBounds(const TestCaseGate& gate, double& lowOut, double& highOut) {
    lowOut = gate.low;
    highOut = gate.high;
    if (!gate.lowSettingsKey.isEmpty())
        lowOut = SETTINGS.value(gate.lowSettingsKey, lowOut).toDouble();
    if (!gate.highSettingsKey.isEmpty())
        highOut = SETTINGS.value(gate.highSettingsKey, highOut).toDouble();
}

namespace {

QString withDisplayUnit(const QString& text, const QString& unit) {
    const QString t = text.trimmed();
    const QString u = unit.trimmed();
    if (t.isEmpty() || u.isEmpty() || t == QLatin1String("-") || t == QLatin1String("通过")
        || t == QLatin1String("失败"))
        return text;
    if (t.endsWith(u) && (t.size() == u.size() || t.at(t.size() - u.size() - 1).isSpace()
                          || !t.at(t.size() - u.size() - 1).isLetterOrNumber()))
        return text;
    return t + QLatin1Char(' ') + u;
}

QString defaultUnitForField(const QString& reportType, const QString& field) {
    if (reportType == QLatin1String("ProtocolRssiData") && field == QLatin1String("dbm"))
        return QStringLiteral("dBm");
    if (reportType == QLatin1String("ProtocolBatteryData")) {
        if (field == QLatin1String("percent"))
            return QStringLiteral("%");
        if (field == QLatin1String("voltageMv"))
            return QStringLiteral("mV");
        if (field == QLatin1String("currentMa"))
            return QStringLiteral("mA");
        if (field == QLatin1String("temperatureC"))
            return QStringLiteral("℃");
    }
    if (reportType == QLatin1String("ProtocolChargeCurrentData") && field == QLatin1String("currentMa"))
        return QStringLiteral("mA");
    if (reportType == QLatin1String("ProtocolFixturePcbaData")) {
        if (field == QLatin1String("staticCurrent") || field == QLatin1String("standbyCurrentUa"))
            return QStringLiteral("uA");
        if (field == QLatin1String("workingCurrent") || field == QLatin1String("chargingCurrent")
            || field == QLatin1String("musicCurrent"))
            return QStringLiteral("mA");
        if (field == QLatin1String("pumpVoltageMv") || field == QLatin1String("mcuVoltageMv")
            || field == QLatin1String("valveVoltageMv"))
            return QStringLiteral("mV");
    }
    if (reportType == QLatin1String("ProtocolJieliBtBoxData")) {
        if (field == QLatin1String("rssi"))
            return QStringLiteral("dBm");
        if (field == QLatin1String("freqOffset"))
            return QStringLiteral("Hz");
    }
    if (reportType == QLatin1String("ProtocolDongleSuctionData")
        || reportType == QLatin1String("ProtocolDongleSuctionPeakData")) {
        if (field.endsWith(QLatin1String("Kpa")) || field.endsWith(QLatin1String("kPa")))
            return QStringLiteral("kPa");
    }
    if (reportType == QLatin1String("ProtocolScreenInspectData")) {
        if (field == QLatin1String("deadPixels"))
            return QStringLiteral("个");
        if (field == QLatin1String("ssim"))
            return QStringLiteral("0~1");
        if (field == QLatin1String("muraStd"))
            return QStringLiteral("σ");
        if (field == QLatin1String("colorMatch") || field == QLatin1String("detectedColor"))
            return {};
    }
    if ((reportType == QLatin1String("ProtocolBatteryTempData")
         || reportType == QLatin1String("ProtocolHeatTempData"))
        && field == QLatin1String("type"))
        return QString::fromUtf8("℃");
    if (reportType == QLatin1String("ProtocolAgingStatusData") && field == QLatin1String("seconds"))
        return QStringLiteral("s");
    return {};
}

} // namespace

QString GateRegistry::unitFor(const QString& reportType, const QString& field, const QVariant& payload) {
    if (reportType == QLatin1String("ProtocolMeasureData") && payload.canConvert<ProtocolMeasureData>()) {
        const QString runtimeUnit = payload.value<ProtocolMeasureData>().unit.trimmed();
        if (!runtimeUnit.isEmpty())
            return runtimeUnit;
    }
    return defaultUnitForField(reportType, field);
}

QString GateRegistry::formatFieldDisplayValue(const QString& reportType, const QString& field, double value) {
    return formatGateFieldValue(reportType, field, value);
}

QString GateRegistry::formatGateAsk(const TestCaseGate& gate, const QString& reportType, const QVariant& payload) {
    QString ask;
    if (gate.op == TestCaseGateOp::Range) {
        double low = gate.low;
        double high = gate.high;
        resolveRangeBounds(gate, low, high);
        // RSSI 开区间展示为 (low,high)，与判定一致
        if (CommonUtils::isRssiOpenRangeGate(reportType, gate.field)) {
            ask = QStringLiteral("(%1,%2)")
                      .arg(formatGateFieldValue(reportType, gate.field, low),
                           formatGateFieldValue(reportType, gate.field, high));
        } else {
            ask = QStringLiteral("[%1,%2]")
                      .arg(formatGateFieldValue(reportType, gate.field, low),
                           formatGateFieldValue(reportType, gate.field, high));
        }
    } else if (gate.op == TestCaseGateOp::Gt) {
        ask = QStringLiteral(">%1").arg(formatGateFieldValue(reportType, gate.field, gateCompareThreshold(gate)));
    } else if (gate.op == TestCaseGateOp::Lt) {
        ask = QStringLiteral("<%1").arg(formatGateFieldValue(reportType, gate.field, gateCompareThreshold(gate)));
    } else if (gate.op == TestCaseGateOp::Eq) {
        const QString expected = gate.expected.trimmed();
        if (reportType == QLatin1String("ProtocolScreenInspectData")
            && gate.field == QLatin1String("colorMatch")) {
            ask = formatGateFieldValue(reportType, gate.field, colorMatchThreshold(gate));
        } else if (reportType == QLatin1String("ProtocolFactoryDoneData")
                   && gate.field == QLatin1String("done")) {
            ask = formatGateFieldValue(reportType, gate.field, factoryDoneThreshold(gate));
        } else if (!expected.isEmpty()) {
            bool ok = false;
            const double parsed = expected.toDouble(&ok);
            ask = ok ? formatGateFieldValue(reportType, gate.field, parsed) : expected;
        } else {
            ask = formatGateFieldValue(reportType, gate.field, gateCompareThreshold(gate));
        }
    } else {
        ask = gate.expected.trimmed();
    }
    return withDisplayUnit(ask, unitFor(reportType, gate.field, payload));
}

QString GateRegistry::formatMultiFieldAsk(const QVector<TestCaseGate>& gates, const QString& reportType,
                                          const QVariant& payload) {
    QStringList expectedParts;
    expectedParts.reserve(gates.size());
    for (const TestCaseGate& g : gates) {
        const QString name = fieldDisplayName(reportType, g.field);
        QString part;
        if (g.op == TestCaseGateOp::Range) {
            double low = g.low;
            double high = g.high;
            resolveRangeBounds(g, low, high);
            part = QStringLiteral("%1=[%2,%3]")
                       .arg(name, formatGateFieldValue(reportType, g.field, low),
                            formatGateFieldValue(reportType, g.field, high));
        } else if (g.op == TestCaseGateOp::Eq) {
            const QString expected = g.expected.trimmed();
            QString valText;
            if (!expected.isEmpty()) {
                bool ok = false;
                const double parsed = expected.toDouble(&ok);
                valText = ok ? formatGateFieldValue(reportType, g.field, parsed) : expected;
            } else {
                valText = formatGateFieldValue(reportType, g.field, gateCompareThreshold(g));
            }
            part = QStringLiteral("%1=%2").arg(name, valText);
        } else if (g.op == TestCaseGateOp::Gt) {
            part = QStringLiteral("%1>%2")
                       .arg(name, formatGateFieldValue(reportType, g.field, gateCompareThreshold(g)));
        } else if (g.op == TestCaseGateOp::Lt) {
            part = QStringLiteral("%1<%2")
                       .arg(name, formatGateFieldValue(reportType, g.field, gateCompareThreshold(g)));
        } else {
            part = QStringLiteral("%1:%2").arg(name, g.expected);
        }
        expectedParts.append(withDisplayUnit(part, unitFor(reportType, g.field, payload)));
    }
    return expectedParts.join(QLatin1Char(';'));
}

namespace {

QString fixturePacketSummary(const FixturePacketData& pack) {
    return QStringLiteral("机号=%1 静态=%2 工作=%3 充电=%4 泵=%5 MCU=%6 阀=%7")
        .arg(pack.machineNumber)
        .arg(pack.staticCurrent)
        .arg(pack.workingCurrent)
        .arg(pack.chargingCurrent)
        .arg(pack.pumpVoltageMv)
        .arg(pack.mcuVoltageMv)
        .arg(pack.valveVoltageMv);
}

QString periphStateSummary(const ProtocolPeriphStateData& periph) {
    return QStringLiteral("press0=%1;press1=%2;battery=%3;touch=%4;led=%5;pd=%6")
        .arg(periph.press0_state)
        .arg(periph.press1_state)
        .arg(periph.battery_ic_state)
        .arg(periph.touch_ic_state)
        .arg(periph.led_ic_state)
        .arg(periph.pd_ic_state);
}

QString agingHistorySummary(const ProtocolRootAgingHistoryData& hist) {
    QString head;
    if (hist.status >= 0 || hist.finishedFlag >= 0) {
        head = QStringLiteral("使能=%1 完成=%2 ")
                   .arg(hist.status < 0 ? 0 : hist.status)
                   .arg(hist.finishedFlag < 0 ? 0 : hist.finishedFlag);
    } else {
        head = QStringLiteral("次数=%1 ").arg(hist.agingCount);
    }
    return head
           + QStringLiteral("电池最高温=%1℃ 法兰最高温=%2℃ 堵转次数=%3 泵阀堵转阈值=%4 电流=[%5,%6,%7,%8,%9]")
                 .arg(hist.batteryMaxTempC)
                 .arg(hist.flangeMaxTempC)
                 .arg(hist.stallCount)
                 .arg(hist.stallThreshold)
                 .arg(hist.stallCurrents[0])
                 .arg(hist.stallCurrents[1])
                 .arg(hist.stallCurrents[2])
                 .arg(hist.stallCurrents[3])
                 .arg(hist.stallCurrents[4]);
}

QString primaryFieldTestData(const TestCaseGate& primaryGate, const QString& reportType, const QVariant& payload) {
    // 优先数值路径：纯色等字段由 formatGateFieldValue 转成文字（避免 fieldString 仍返回 "2"）
    bool numOk = false;
    const double fromNum = fieldValueFromVariant(reportType, primaryGate.field, payload, numOk);
    if (numOk)
        return formatGateFieldValue(reportType, primaryGate.field, fromNum);
    bool strOk = false;
    const QString fromField = fieldStringFromVariant(reportType, primaryGate.field, payload, strOk);
    if (strOk && !fromField.isEmpty())
        return fromField;
    return {};
}

} // namespace

GateStepDisplay GateRegistry::formatStepDisplay(const TestCaseGate& primaryGate, const QVector<TestCaseGate>& allGates,
                                                const QString& reportType, const QVariant& payload,
                                                bool multiFieldMode) {
    GateStepDisplay out;
    const QString unit = unitFor(reportType, primaryGate.field, payload);
    if (reportType == QLatin1String("ProtocolFixturePcbaData") && payload.canConvert<FixturePacketData>()) {
        out.testData = fixturePacketSummary(payload.value<FixturePacketData>());
    } else if (reportType == QLatin1String("ProtocolPeriphStateData") && payload.canConvert<ProtocolPeriphStateData>()) {
        out.testData = periphStateSummary(payload.value<ProtocolPeriphStateData>());
    } else if (reportType == QLatin1String("ProtocolRootAgingHistoryData")
               && payload.canConvert<ProtocolRootAgingHistoryData>()) {
        out.testData = agingHistorySummary(payload.value<ProtocolRootAgingHistoryData>());
    } else if (reportType == QLatin1String("ProtocolMeasureData")
               && primaryGate.field == QLatin1String("value")
               && payload.canConvert<ProtocolMeasureData>()) {
        const ProtocolMeasureData data = payload.value<ProtocolMeasureData>();
        out.testData = withDisplayUnit(QString::number(data.value), unit);
    } else {
        out.testData = withDisplayUnit(primaryFieldTestData(primaryGate, reportType, payload), unit);
    }

    if (multiFieldMode && allGates.size() > 1)
        out.ask = formatMultiFieldAsk(allGates, reportType, payload);
    else
        out.ask = formatGateAsk(primaryGate, reportType, payload);
    return out;
}
