#include "test_case_gate_accessors.h"

#include "fixture_uart_types.h"
#include "qprotocol_types.h"
#include "screen_inspect_analyzer.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// ===================== unpack（兼容结构体与 QVariantMap 两种 payload）=====================

/** 杰理蓝牙盒子：治具侧既可能直接装箱结构体，也可能给 QVariantMap。 */
static ProtocolJieliBtBoxData unpackJieli(const QVariant& payload, bool* ok) {
    ProtocolJieliBtBoxData d;
    const QVariantMap m = payload.toMap();
    if (!m.isEmpty()) {
        d.rssi = m.value(QStringLiteral("rssi")).toInt();
        d.freqOffset = m.value(QStringLiteral("freqOffset")).toInt();
        d.mac = m.value(QStringLiteral("mac")).toString();
        if (ok)
            *ok = true;
        return d;
    }
    if (!payload.canConvert<ProtocolJieliBtBoxData>()) {
        if (ok)
            *ok = false;
        return d;
    }
    if (ok)
        *ok = true;
    return payload.value<ProtocolJieliBtBoxData>();
}

/** PCBA 治具长包：同上，QVariantMap 走键名回填。 */
static FixturePacketData unpackFixture(const QVariant& payload, bool* ok) {
    FixturePacketData pack;
    const QVariantMap m = payload.toMap();
    if (!m.isEmpty()) {
        pack.machineNumber = static_cast<uchar>(m.value(QStringLiteral("machineNumber")).toUInt());
        pack.staticCurrent = m.value(QStringLiteral("staticCurrent")).toUInt();
        pack.workingCurrent = m.value(QStringLiteral("workingCurrent")).toUInt();
        pack.chargingCurrent = m.value(QStringLiteral("chargingCurrent")).toUInt();
        pack.musicCurrent = m.value(QStringLiteral("musicCurrent")).toUInt();
        pack.standbyCurrentUa = m.value(QStringLiteral("standbyCurrentUa")).toUInt();
        pack.pumpVoltageMv = m.value(QStringLiteral("pumpVoltageMv")).toUInt();
        pack.mcuVoltageMv = m.value(QStringLiteral("mcuVoltageMv")).toUInt();
        pack.valveVoltageMv = m.value(QStringLiteral("valveVoltageMv")).toUInt();
        pack.button1 = static_cast<uchar>(m.value(QStringLiteral("button1")).toUInt());
        pack.button2 = static_cast<uchar>(m.value(QStringLiteral("button2")).toUInt());
        pack.overVoltageLight = static_cast<uchar>(m.value(QStringLiteral("overVoltageLight")).toUInt());
        pack.fixerro = static_cast<uint8_t>(m.value(QStringLiteral("fixerro")).toUInt());
        if (ok)
            *ok = true;
        return pack;
    }
    if (!payload.canConvert<FixturePacketData>()) {
        if (ok)
            *ok = false;
        return pack;
    }
    if (ok)
        *ok = true;
    return payload.value<FixturePacketData>();
}

// ===================== Qaiot 数组型回包：卡控只看首项 =====================

static std::function<bool(const QVariant&, double*)>
aiotThresholdItemReader(int ProtocolAiotExceptionThresholdItem::*mem) {
    return [mem](const QVariant& payload, double* out) -> bool {
        if (!out || !payload.canConvert<ProtocolAiotExceptionThresholdData>())
            return false;
        const ProtocolAiotExceptionThresholdData d = payload.value<ProtocolAiotExceptionThresholdData>();
        if (d.items.isEmpty())
            return false;
        *out = static_cast<double>(d.items.first().*mem);
        return true;
    };
}

static std::function<bool(const QVariant&, double*)>
aiotCycleConfigItemReader(int ProtocolAiotCycleReportConfigItem::*mem) {
    return [mem](const QVariant& payload, double* out) -> bool {
        if (!out || !payload.canConvert<ProtocolAiotCycleReportConfigData>())
            return false;
        const ProtocolAiotCycleReportConfigData d = payload.value<ProtocolAiotCycleReportConfigData>();
        if (d.items.isEmpty())
            return false;
        *out = static_cast<double>(d.items.first().*mem);
        return true;
    };
}

static std::function<bool(const QVariant&, double*)> aiotCycleItemReader(int ProtocolAiotCycleReportItem::*mem) {
    return [mem](const QVariant& payload, double* out) -> bool {
        if (!out || !payload.canConvert<ProtocolAiotCycleReportData>())
            return false;
        const ProtocolAiotCycleReportData d = payload.value<ProtocolAiotCycleReportData>();
        if (d.items.isEmpty())
            return false;
        *out = static_cast<double>(d.items.first().*mem);
        return true;
    };
}

// ===================== 基础量测 / 校准 =====================

static void registerBasicMeasureGates() {
    {
        using D = ProtocolRssiData;
        GateType<D>("ProtocolRssiData", "蓝牙信号强度")
            .number("dbm", &D::dbm, "信号强度(分贝)", "dBm")
            .commit();
    }
    {
        using D = ProtocolBatteryData;
        GateType<D>("ProtocolBatteryData", "电量")
            .number("percent", &D::percent, "电量(%)", "%")
            .number("chargeState", &D::chargeState, "充电状态", "")
            .number("voltageMv", &D::voltageMv, "电压(mV)", "mV")
            .number("currentMa", &D::currentMa, "电流(mA)", "mA")
            .number("temperatureC", &D::temperatureC, "温度(℃)", "℃")
            .commit();
    }
    {
        using D = ProtocolKeyCapData;
        GateType<D>("ProtocolKeyCapData", "按键电容")
            .number("capacitance", &D::capacitance, "电容值", "")
            .number("keyId", &D::keyId, "按键编号", "")
            .commit();
    }
    {
        using D = ProtocolChargeCurrentData;
        GateType<D>("ProtocolChargeCurrentData", "充电电流")
            .number("currentMa", &D::currentMa, "电流(mA)", "mA")
            .commit();
    }
    {
        using D = ProtocolTrimData;
        GateType<D>("ProtocolTrimData", "Trim微调值")
            .number("trim", &D::trim, "微调值", "")
            .commit();
    }
    {
        using D = ProtocolLightCalibData;
        GateType<D>("ProtocolLightCalibData", "光感校准值")
            .number("calibValue", &D::calibValue, "校准值", "")
            .commit();
    }
    {
        using D = ProtocolPhotosensitiveData;
        GateType<D>("ProtocolPhotosensitiveData", "光感上报")
            .number("lightSensor", &D::lightSensor, "光感值", "")
            .alias("value")
            .commit();
    }
}

// ===================== Qaiot 校准 / 参数 / 循环上报 =====================

static void registerAiotGates() {
    {
        using D = ProtocolAiotImuCaliData;
        GateType<D>("ProtocolAiotImuCaliData", "Qaiot IMU校准")
            .number("kx", &D::kx, "kx", "").textFormat('g', 8)
            .number("ky", &D::ky, "ky", "").textFormat('g', 8)
            .number("kz", &D::kz, "kz", "").textFormat('g', 8)
            .number("syx", &D::syx, "syx", "").textFormat('g', 8)
            .number("szx", &D::szx, "szx", "").textFormat('g', 8)
            .number("szy", &D::szy, "szy", "").textFormat('g', 8)
            .number("bx", &D::bx, "bx", "").textFormat('g', 8)
            .number("by", &D::by, "by", "").textFormat('g', 8)
            .number("bz", &D::bz, "bz", "").textFormat('g', 8)
            .commit();
    }
    {
        using D = ProtocolAiotFsensorCaliData;
        GateType<D>("ProtocolAiotFsensorCaliData", "Qaiot电容/力传感校准")
            .number("calibrated", &D::calibrated, "校准标志", "")
            .alias("flag")
            .commit();
    }
    // 阈值/循环上报回包是数组，取值绕 items.first()，用 GateTypeRaw
    GateTypeRaw("ProtocolAiotExceptionThresholdData", "Qaiot异常阈值")
        .numberFn("type", "异常类型", "", aiotThresholdItemReader(&ProtocolAiotExceptionThresholdItem::type))
        .numberFn("value", "阈值主值", "", aiotThresholdItemReader(&ProtocolAiotExceptionThresholdItem::value))
        .alias("percent")
        .alias("voltageMv")
        .alias("seconds")
        .alias("currentMa")
        .alias("tempLow")
        .alias("low")
        .numberFn("valueHigh", "阈值上限", "",
                  aiotThresholdItemReader(&ProtocolAiotExceptionThresholdItem::valueHigh))
        .alias("tempHigh")
        .alias("high")
        .commit();
    {
        using D = ProtocolAiotPumpParamData;
        GateType<D>("ProtocolAiotPumpParamData", "Qaiot泵/阀参数")
            .number("circleNum", &D::circleNum, "循环次数", "")
            .alias("loops")
            .alias("pump_run_time")
            .alias("pump_circle_num")
            .number("durationTime", &D::durationTime, "泵工作时长", "")
            .alias("pump_duration_time")
            .number("intervalTime", &D::intervalTime, "泵间隔时长", "")
            .alias("pump_interval_time")
            .number("valveEnableTime", &D::valveEnableTime, "阀使能时长", "")
            .alias("value_enable_time")
            .alias("valve_enable_time")
            .number("valveDisableTime", &D::valveDisableTime, "阀关闭时长", "")
            .alias("value_disable_time")
            .alias("valve_disable_time")
            .number("pumpPwm", &D::pumpPwm, "泵PWM%", "")
            .alias("pump_pwm_value")
            .number("valvePwm", &D::valvePwm, "阀PWM%", "")
            .alias("value_pwm_value")
            .alias("valve_pwm_value")
            .commit();
    }
    {
        using D = ProtocolAiotHeatTestData;
        GateType<D>("ProtocolAiotHeatTestData", "Qaiot自定义加热")
            .number("enable", &D::enable, "加热使能", "")
            .alias("heat_enable")
            .number("driveStrength", &D::driveStrength, "加热强度", "")
            .alias("heat_drive_strength")
            .alias("strength")
            .alias("pwm")
            .number("durationTime", &D::durationTime, "加热时长", "")
            .alias("heat_duration_time")
            .alias("duration")
            .commit();
    }
    {
        using D = ProtocolAiotVibrationTestData;
        GateType<D>("ProtocolAiotVibrationTestData", "Qaiot自定义振动")
            .number("enable", &D::enable, "振动使能", "")
            .alias("vibration_enable")
            .number("driveStrength", &D::driveStrength, "振动强度", "")
            .alias("vibration_drive_strength")
            .alias("strength")
            .alias("pwm")
            .number("freq", &D::freq, "振动频率", "")
            .alias("vibration_freq")
            .alias("frequency")
            .number("durationTime", &D::durationTime, "振动时长", "")
            .alias("vibration_duration_time")
            .alias("duration")
            .commit();
    }
    GateTypeRaw("ProtocolAiotCycleReportConfigData", "Qaiot循环上报配置")
        .numberFn("enable", "循环上报使能", "",
                  [](const QVariant& payload, double* out) -> bool {
                      if (!out || !payload.canConvert<ProtocolAiotCycleReportConfigData>())
                          return false;
                      *out = payload.value<ProtocolAiotCycleReportConfigData>().enable;
                      return true;
                  })
        .alias("cycle_report_enable")
        .numberFn("dataType", "数据类型", "",
                  aiotCycleConfigItemReader(&ProtocolAiotCycleReportConfigItem::dataType))
        .alias("type")
        .alias("report_data_type")
        .numberFn("intervalTime", "上报周期ms", "",
                  aiotCycleConfigItemReader(&ProtocolAiotCycleReportConfigItem::intervalTime))
        .alias("report_interval_time")
        .alias("interval")
        .commit();
    GateTypeRaw("ProtocolAiotCycleReportData", "Qaiot循环上报数据")
        .numberFn("dataType", "数据类型", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::dataType))
        .alias("type")
        .alias("report_data_type")
        .numberFn("accX", "加速度X", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::accX))
        .numberFn("accY", "加速度Y", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::accY))
        .numberFn("accZ", "加速度Z", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::accZ))
        .numberFn("gyroX", "角速度X", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::gyroX))
        .numberFn("gyroY", "角速度Y", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::gyroY))
        .numberFn("gyroZ", "角速度Z", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::gyroZ))
        .numberFn("pressureOut", "压力出(0.1Pa)", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::pressureOut))
        .alias("p_out")
        .numberFn("pressureIn", "压力进(0.1Pa)", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::pressureIn))
        .alias("p_in")
        .numberFn("flowRate", "气流(0.01L/min)", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::flowRate))
        .alias("flow_rate")
        .numberFn("distanceMm", "距离mm", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::distanceMm))
        .alias("distance")
        .numberFn("adcRaw", "ADC原始值", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::adcRaw))
        .alias("adc")
        .numberFn("irLevel", "红外强度", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::irLevel))
        .alias("ir_level")
        .numberFn("impedance", "阻抗(0.1Ω)", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::impedance))
        .numberFn("levelMm", "液位mm", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::levelMm))
        .alias("level")
        .numberFn("temperatureC", "温度℃", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::temperatureC))
        .alias("temperature")
        .numberFn("humidity", "湿度%RH", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::humidity))
        .numberFn("currentMa", "电流mA", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::currentMa))
        .alias("current")
        .numberFn("hallState", "霍尔状态", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::hallState))
        .alias("hall")
        .numberFn("pulseCount", "编码器脉冲", "", aiotCycleItemReader(&ProtocolAiotCycleReportItem::pulseCount))
        .alias("pulse")
        .commit();
}

// ===================== 设备信息 / 状态 =====================

static void registerDeviceInfoGates() {
    {
        using D = ProtocolSnData;
        GateType<D>("ProtocolSnData", "序列号")
            .text("value", &D::value, "序列号文本")
            .commit();
    }
    {
        using D = ProtocolBaseInfoData;
        GateType<D>("ProtocolBaseInfoData", "基本信息")
            .version("soft_version", &D::soft_version, "软件版本")
            .text("res_version", &D::res_version, "资源版本")
            .text("product_name", &D::product_name, "产品名称")
            .text("hw_version", &D::hw_version, "硬件版本")
            .text("algo_version", &D::algo_version, "算法版本")
            .number("ageing_state", &D::ageing_state, "老化状态", "")
            .commit();
    }
    {
        using D = ProtocolPeriphStateData;
        GateType<D>("ProtocolPeriphStateData", "外设状态")
            .number("press0_state", &D::press0_state, "压感0状态", "")
            .number("press1_state", &D::press1_state, "压感1状态", "")
            .number("battery_ic_state", &D::battery_ic_state, "电池IC状态", "")
            .number("touch_ic_state", &D::touch_ic_state, "触摸IC状态", "")
            .number("led_ic_state", &D::led_ic_state, "LED IC状态", "")
            .number("pd_ic_state", &D::pd_ic_state, "PD IC状态", "")
            .summary([](const D& d) {
                return QStringLiteral("press0=%1;press1=%2;battery=%3;touch=%4;led=%5;pd=%6")
                    .arg(d.press0_state)
                    .arg(d.press1_state)
                    .arg(d.battery_ic_state)
                    .arg(d.touch_ic_state)
                    .arg(d.led_ic_state)
                    .arg(d.pd_ic_state);
            })
            .commit();
    }
    {
        using D = ProtocolTupleData;
        GateType<D>("ProtocolTupleData", "设备三元组")
            .text("productId", &D::productId, "产品密钥")
            .text("deviceId", &D::deviceId, "设备名")
            .text("key", &D::key, "设备密钥")
            .commit();
    }
    {
        using D = ProtocolButtonStateData;
        GateType<D>("ProtocolButtonStateData", "按键状态")
            .number("modeButtonState", &D::modeButtonState, "模式键状态", "")
            .number("powerButtonState", &D::powerButtonState, "电源键状态", "")
            .number("keyButtonId", &D::keyButtonId, "按键编号", "")
            .commit();
    }
    {
        using D = ProtocolAgingStatusData;
        GateType<D>("ProtocolAgingStatusData", "老化状态上报")
            .number("status", &D::status, "状态码", "")
            .number("loops", &D::loops, "循环次数", "")
            .number("seconds", &D::seconds, "秒数", "s")
            .commit();
    }
    {
        using D = ProtocolMusicStateData;
        GateType<D>("ProtocolMusicStateData", "音乐状态")
            .number("musicState", &D::musicState, "音乐状态码", "")
            .commit();
    }
    {
        using D = ProtocolResultData;
        GateType<D>("ProtocolResultData", "通用结果码")
            .number("result", &D::result, "结果码", "")
            .commit();
    }
}

// ===================== 治具 / 射频 =====================

static void registerFixtureAndRfGates() {
    {
        using D = FixturePacketData;
        GateType<D>("ProtocolFixturePcbaData", "PCBA治具数据包")
            .withUnpack(unpackFixture)
            .number("machineNumber", &D::machineNumber, "机号", "")
            .number("staticCurrent", &D::staticCurrent, "静态电流(uA)", "uA")
            .number("workingCurrent", &D::workingCurrent, "工作电流(mA)", "mA")
            .number("chargingCurrent", &D::chargingCurrent, "充电电流(mA)", "mA")
            .number("musicCurrent", &D::musicCurrent, "音频IC电流(mA)", "mA")
            .number("standbyCurrentUa", &D::standbyCurrentUa, "待机电流(uA)", "uA")
            .number("pumpVoltageMv", &D::pumpVoltageMv, "泵电压(mV)", "mV")
            .number("mcuVoltageMv", &D::mcuVoltageMv, "MCU电压(mV)", "mV")
            .number("valveVoltageMv", &D::valveVoltageMv, "阀电压(mV)", "mV")
            .number("button1", &D::button1, "按键1", "")
            .number("button2", &D::button2, "按键2", "")
            .number("overVoltageLight", &D::overVoltageLight, "过压灯", "")
            .number("fixerro", &D::fixerro, "治具错误码", "")
            .summary([](const D& d) {
                return QStringLiteral("机号=%1 静态=%2 工作=%3 充电=%4 泵=%5 MCU=%6 阀=%7")
                    .arg(d.machineNumber)
                    .arg(d.staticCurrent)
                    .arg(d.workingCurrent)
                    .arg(d.chargingCurrent)
                    .arg(d.pumpVoltageMv)
                    .arg(d.mcuVoltageMv)
                    .arg(d.valveVoltageMv);
            })
            .commit();
    }
    {
        using D = ProtocolJieliBtBoxData;
        GateType<D>("ProtocolJieliBtBoxData", "杰理蓝牙盒子RF")
            .withUnpack(unpackJieli)
            .number("rssi", &D::rssi, "RSSI(dBm)", "dBm")
            .number("freqOffset", &D::freqOffset, "频偏", "Hz")
            .mac("mac", &D::mac, "MAC地址")
            .commit();
    }
    {
        using D = ProtocolMacData;
        GateType<D>("ProtocolMacData", "MAC地址")
            .mac("mac", &D::mac, "MAC文本")
            .commit();
    }
}

// ===================== Qroot 状态码 / 温度 / 老化历史 =====================

static void registerRootGates() {
    using D = ProtocolTypeData;
    // 法兰/电池温度/加热温度均为 ProtocolTypeData 的 typedef，仅 ReportType 与显示不同
    GateType<D>("ProtocolTypeData", "状态码").number("type", &D::type, "状态值", "").commit();
    GateType<D>("ProtocolFlangeData", "法兰状态").number("type", &D::type, "法兰类型", "").commit();
    {
        using S = ProtocolPumpStallCurrentData;
        GateType<S>("ProtocolPumpStallCurrentData", "泵堵电流")
            .number("adcValue", &S::adcValue, "堵转ADC", "")
            .commit();
    }
    {
        using S = ProtocolRootAgingHistoryData;
        GateType<S>("ProtocolRootAgingHistoryData", "老化历史/老化模式")
            .number("status", &S::status, "老化使能", "")
            .alias("enable")
            .number("finishedFlag", &S::finishedFlag, "老化完成标志", "")
            .alias("finished")
            .number("agingCount", &S::agingCount, "老化当前次数", "")
            .alias("count")
            .alias("loops")
            .number("batteryMaxTempC", &S::batteryMaxTempC, "电池历史最高温℃", "")
            .alias("batteryMaxTemp")
            .number("flangeMaxTempC", &S::flangeMaxTempC, "法兰历史最高温℃", "")
            .alias("flangeMaxTemp")
            .number("stallCount", &S::stallCount, "老化堵转次数", "")
            .number("stallThreshold", &S::stallThreshold, "泵阀堵转阈值", "")
            .numberFn("stallCurrent0", "堵转电流1", "",
                      [](const S& d, double* out) {
                          if (!out)
                              return false;
                          *out = d.stallCurrents[0];
                          return true;
                      })
            .numberFn("stallCurrent1", "堵转电流2", "",
                      [](const S& d, double* out) {
                          if (!out)
                              return false;
                          *out = d.stallCurrents[1];
                          return true;
                      })
            .numberFn("stallCurrent2", "堵转电流3", "",
                      [](const S& d, double* out) {
                          if (!out)
                              return false;
                          *out = d.stallCurrents[2];
                          return true;
                      })
            .numberFn("stallCurrent3", "堵转电流4", "",
                      [](const S& d, double* out) {
                          if (!out)
                              return false;
                          *out = d.stallCurrents[3];
                          return true;
                      })
            .numberFn("stallCurrent4", "堵转电流5", "",
                      [](const S& d, double* out) {
                          if (!out)
                              return false;
                          *out = d.stallCurrents[4];
                          return true;
                      })
            .summary([](const S& d) {
                QString head;
                if (d.status >= 0 || d.finishedFlag >= 0) {
                    head = QStringLiteral("使能=%1 完成=%2 ")
                               .arg(d.status < 0 ? 0 : d.status)
                               .arg(d.finishedFlag < 0 ? 0 : d.finishedFlag);
                } else {
                    head = QStringLiteral("次数=%1 ").arg(d.agingCount);
                }
                return head
                       + QStringLiteral("电池最高温=%1℃ 法兰最高温=%2℃ 堵转次数=%3 泵阀堵转阈值=%4 电流=[%5,%6,%7,%8,%9]")
                             .arg(d.batteryMaxTempC)
                             .arg(d.flangeMaxTempC)
                             .arg(d.stallCount)
                             .arg(d.stallThreshold)
                             .arg(d.stallCurrents[0])
                             .arg(d.stallCurrents[1])
                             .arg(d.stallCurrents[2])
                             .arg(d.stallCurrents[3])
                             .arg(d.stallCurrents[4]);
            })
            .commit();
    }
    GateType<D>("ProtocolBatteryTempData", "电池温度").number("type", &D::type, "温度值", "℃").commit();
    GateType<D>("ProtocolHeatTempData", "加热温度").number("type", &D::type, "温度值", "℃").commit();
}

// ===================== 外设测量 / Dongle 吸力 / 屏幕检测 =====================

static void registerPeriphAndScreenGates() {
    {
        using D = ProtocolMeasureData;
        // 单位随运行时 payload.unit 变化，登记留空由 unitFor 兜底
        GateType<D>("ProtocolMeasureData", "外设测量值")
            .number("value", &D::value, "测量数值", "")
            .text("valueText", &D::valueText, "测量文本值")
            .text("deviceName", &D::deviceName, "外设名称")
            .text("channel", &D::channel, "通道号")
            .text("type", &D::type, "测量类型")
            .text("unit", &D::unit, "单位")
            .commit();
    }
    {
        using D = ProtocolDongleSuctionData;
        GateType<D>("ProtocolDongleSuctionData", "Dongle吸力实时")
            .number("ch1Kpa", &D::ch1Kpa, "CH1(kPa)", "kPa").textFormat('f', 2).alias("leftKpa")
            .number("ch2Kpa", &D::ch2Kpa, "CH2(kPa)", "kPa").textFormat('f', 2).alias("rightKpa")
            .number("ch3Kpa", &D::ch3Kpa, "CH3(kPa)", "kPa").textFormat('f', 2).alias("thirdKpa")
            .commit();
    }
    {
        using D = ProtocolDongleSuctionPeakData;
        GateType<D>("ProtocolDongleSuctionPeakData", "Dongle吸力峰值")
            .number("peakKpa", &D::peakKpa, "单通道最低峰值", "kPa").textFormat('f', 3).valueLabel("最低峰值")
            .number("highKpa", &D::highKpa, "单通道最弱峰值(kPa)", "kPa").textFormat('f', 3)
            .number("peakDiffKpa", &D::peakDiffKpa, "单通道峰值差(最大峰-最小峰)", "kPa").textFormat('f', 3)
            .number("ch1PeakKpa", &D::ch1PeakKpa, "CH1最低峰值", "kPa").textFormat('f', 3).alias("leftPeakKpa").valueLabel("最低峰值")
            .number("ch2PeakKpa", &D::ch2PeakKpa, "CH2最低峰值", "kPa").textFormat('f', 3).alias("rightPeakKpa").valueLabel("最低峰值")
            .number("sideDiffKpa", &D::sideDiffKpa, "CH1-CH2峰差(kPa)", "kPa").textFormat('f', 3).alias("peakSpanKpa")
            .number("peakCount", &D::peakCount, "完整周期峰个数", "")
            .commit();
    }
    {
        using D = ProtocolScreenInspectData;
        // 纯色/是否匹配：ini 期望写数字，判定走数值；展示用中文
        GateType<D>("ProtocolScreenInspectData", "屏幕检测")
            .number("deadPixels", &D::deadPixels, "坏点数", "个")
            .number("detectedColor", &D::detectedColor, "实测纯色", "")
            .screenColor()
            .formatValue([](double v) { return ScreenInspectAnalyzer::colorName(qRound(v)); })
            .overrideText([](const D& d, QString* out) {
                if (!out)
                    return false;
                *out = ScreenInspectAnalyzer::colorName(d.detectedColor);
                return true;
            })
            .number("colorMatch", &D::colorMatch, "是否为期望纯色", "")
            .screenColor()
            .formatValue([](double v) {
                return qAbs(v - 1.0) < 0.0001 ? QStringLiteral("是") : QStringLiteral("否");
            })
            .overrideText([](const D& d, QString* out) {
                if (!out)
                    return false;
                *out = d.colorMatch == 1 ? QStringLiteral("是") : QStringLiteral("否");
                return true;
            })
            .number("ssim", &D::ssim, "与参考图相似度（0~1）", "0~1").textFormat('f', 3).alias("similarity")
            .number("muraStd", &D::muraStd, "亮度起伏σ（灰阶勿勾）", "σ").textFormat('f', 1)
            .commit();
    }
}

void registerAllGateAccessorTypes() {
    registerBasicMeasureGates();
    registerAiotGates();
    registerDeviceInfoGates();
    registerFixtureAndRfGates();
    registerRootGates();
    registerPeriphAndScreenGates();
}
