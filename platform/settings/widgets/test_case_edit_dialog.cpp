#include "test_case_edit_dialog.h"
#include "ui_test_case_edit_dialog.h"

#include "test_case.h"
#include "manifest/modbus_cmd_manifest.h"
#include "manifest/scpi_cmd_manifest.h"
#include "qprotocol_types.h"
#include "screen_inspect_analyzer.h"

#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QAbstractItemView>
#include <QItemSelectionModel>

#include <QHash>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QColor>
#include <QBrush>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QLabel>

#include <algorithm>
#include <functional>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** 屏蔽下拉框滚轮改值：产线小屏滚动页面时，滚轮落到下拉框上会误改选中项。 */
class ComboBoxWheelGuard : public QObject {
  public:
    using QObject::QObject;

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::Wheel)
            return true; // 吞掉滚轮：下拉框不再因滚轮改值，只能点击选择
        return QObject::eventFilter(watched, event);
    }
};

QVariantMap sendParamAsJsonMap(const QVariant& param) {
    // DeviceSnPayload 必须先于 canConvert/toMap：payload 的 toMap() 恒为空，UI 会显示成 {}
    if (param.canConvert<DeviceSnPayload>()) {
        const DeviceSnPayload payload = param.value<DeviceSnPayload>();
        QVariantMap map;
        map.insert(QStringLiteral("which_sn"), static_cast<int>(payload.which_sn));
        map.insert(QStringLiteral("sn"), QString::fromUtf8(payload.sn));
        map.insert(QStringLiteral("length"), payload.snLength > 0 ? payload.snLength : 35);
        return map;
    }
    if (param.type() == QVariant::Map || param.type() == QVariant::Hash)
        return param.toMap();
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        if (!map.isEmpty())
            return map;
    }
    return {};
}

/** 步骤 Param 键 → 界面中文说明（保存仍用英文键）。 */
QString sendParamKeyZhLabel(const QString& key) {
    const QString k = key.trimmed();
    if (k.isEmpty())
        return {};
    static const QHash<QString, QString> kMap = {
        {QStringLiteral("visaAddress"), QStringLiteral("VISA 资源地址")},
        {QStringLiteral("voltage"), QStringLiteral("电压 (V)")},
        {QStringLiteral("current"), QStringLiteral("限流 (A)")},
        {QStringLiteral("currentRange"), QStringLiteral("电流量程")},
        {QStringLiteral("scpiSetVoltageCmd"), QStringLiteral("设电压 SCPI（含 %1）")},
        {QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("设限流 SCPI（含 %1）")},
        {QStringLiteral("scpiOutputOnCmd"), QStringLiteral("打开输出 SCPI")},
        {QStringLiteral("scpiOutputOffCmd"), QStringLiteral("关闭输出 SCPI")},
        {QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("读电压 SCPI")},
        {QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("读电流 SCPI")},
        {QStringLiteral("scpiSetCurrentRangeCmd"), QStringLiteral("设电流量程 SCPI（含 %1）")},
        {QStringLiteral("scpiChannelSelectCmd"), QStringLiteral("选通道 SCPI（含 %1，如 INST OUT%1）")},
        {QStringLiteral("sharedPair"), QStringLiteral("启用多工位共享外设（填 false 关闭二拖二）")},
        {QStringLiteral("shareInstrument"), QStringLiteral("启用多工位共享外设（同 sharedPair，填 false 可关闭）")},
        {QStringLiteral("stationsPerDevice"), QStringLiteral("每台设备对应工位数（如 2 或 3）")},
        {QStringLiteral("powerChannel"), QStringLiteral("电源通道号（一般自动填）")},
        {QStringLiteral("powerChannelLock"), QStringLiteral("锁定电源通道（不按工位改）")},
        {QStringLiteral("visaDeviceIndex"), QStringLiteral("电源设备序号（从 0，一般自动）")},
        {QStringLiteral("visaDeviceIndexLock"), QStringLiteral("锁定电源设备序号")},
        {QStringLiteral("enable"), QStringLiteral("输出开关（1开/0关）")},
        {QStringLiteral("sampleDurationMs"), QStringLiteral("连续采样窗口 (ms)")},
        {QStringLiteral("sampleIntervalMs"), QStringLiteral("连续采样间隔 (ms)")},
        {QStringLiteral("minPeakCount"), QStringLiteral("最少完整周期峰数")},
        {QStringLiteral("peakBaselineKpa"), QStringLiteral("回基线阈值 (kPa)")},
        {QStringLiteral("peakDipStartKpa"), QStringLiteral("入峰阈值 (kPa)")},
        {QStringLiteral("offsetKpa"), QStringLiteral("吸力修正值 (kPa)，最终值=原始值+本项（填负数为减）")},
        {QStringLiteral("showCurveInTable"), QStringLiteral("结果表格中嵌入吸力曲线图（true/false，默认true）")},
        {QStringLiteral("channel"), QStringLiteral("通道（气压:left/right/both；吸力:1/2/3）")},
        {QStringLiteral("sampleCount"), QStringLiteral("气压采样点数")},
        {QStringLiteral("intervalMs"), QStringLiteral("气压采样间隔 (ms)")},
        {QStringLiteral("channels"), QStringLiteral("通道列表（如 1,2,3 或 1-6）")},
        {QStringLiteral("channelsPerStation"), QStringLiteral("每工位占用通道数")},
        {QStringLiteral("tempPassMode"), QStringLiteral("多路温度卡控：all=同轮全部达标（默认）；any=任一路达标即过")},
        {QStringLiteral("seconds"), QStringLiteral("倒计时等待秒数（COUNTDOWN_WAIT）")},
        {QStringLiteral("waitSeconds"), QStringLiteral("倒计时等待秒数（同 seconds）")},
        {QStringLiteral("host"), QStringLiteral("AudioExpert TCP 主机（默认127.0.0.1）")},
        {QStringLiteral("port"), QStringLiteral("AudioExpert Remote 端口号")},
        {QStringLiteral("command"), QStringLiteral("发送指令（默认$test#）")},
        {QStringLiteral("connectTimeoutMs"), QStringLiteral("TCP连接超时ms")},
        {QStringLiteral("readTimeoutMs"), QStringLiteral("等待$test_over超时ms（0=用步骤超时）")},
        {QStringLiteral("requireOk"), QStringLiteral("要求Result/Status为OK（1/0）")},
        {QStringLiteral("prompt"), QStringLiteral("倒计时弹窗提示文字（可选）")},
        {QStringLiteral("spare"), QStringLiteral("整机SN备用4位（默认0000）")},
        {QStringLiteral("expected"), QStringLiteral("期望SN前四位")},
        {QStringLiteral("sku"), QStringLiteral("整机SN SKU 3位（附表二/MES）")},
        {QStringLiteral("reserved"), QStringLiteral("整机SN预留F：Air1 1左/2右/3单撑")},
        {QStringLiteral("version"), QStringLiteral("整机SN生产版本N（附表三）")},
        {QStringLiteral("serial"), QStringLiteral("整机SN当日流水4位")},
        {QStringLiteral("pcbaType"), QStringLiteral("期望PCBA类型（主板MB）")},
        {QStringLiteral("date"), QStringLiteral("整机SN日期3位（空则用PCBA日期）")},
        {QStringLiteral("factory"), QStringLiteral("工厂代码2位（空则用PCBA工厂）")},
        {QStringLiteral("pcbaSn"), QStringLiteral("PCBA主板SN（空则用界面SN栏）")},
        {QStringLiteral("brightness"), QStringLiteral("亮度（0~255）")},
        {QStringLiteral("index"), QStringLiteral("光感校准写入下标（0~19）")},
        {QStringLiteral("golden"), QStringLiteral("本点金样光感值")},
        {QStringLiteral("turnOff"), QStringLiteral("1=本步结束后关闭光源")},
        {QStringLiteral("port"), QStringLiteral("串口（如 COM8）")},
        {QStringLiteral("settleMs"), QStringLiteral("设电流后稳定等待毫秒")},
        {QStringLiteral("waitMs"), QStringLiteral("单次采集光感上报超时毫秒")},
        {QStringLiteral("readRepeat"), QStringLiteral("单点重复读取次数（取中位数）")},
        {QStringLiteral("stableTail"), QStringLiteral("取尾部稳定样本个数")},
        {QStringLiteral("writeCalib"), QStringLiteral("1=写入光感平均值 0=只读")},
        {QStringLiteral("verifyRead"), QStringLiteral("1=写后回读校验")},
        {QStringLiteral("verifyWrite"), QStringLiteral("1=回读值须与本轮写入值一致")},
        {QStringLiteral("enableFivePointJudge"), QStringLiteral("1=本步回读后对已采点做范围+差值卡控（不合格立即 FAIL；各读取点建议均开）")},
        {QStringLiteral("minSamples"), QStringLiteral("采集光感最少样本数")},
        {QStringLiteral("rangeLo"), QStringLiteral("本点回读下限（含）")},
        {QStringLiteral("rangeHi"), QStringLiteral("本点回读上限")},
        {QStringLiteral("rangeHiInc"), QStringLiteral("本点上界是否含：1=含 0=不含")},
        {QStringLiteral("diffMin"), QStringLiteral("本点相对上一点差值下限（判定 >；点1可不写）")},
        {QStringLiteral("channelLock"), QStringLiteral("锁定通道号（不按工位改）")},
        {QStringLiteral("slaveAddr"), QStringLiteral("Modbus 从站地址")},
        {QStringLiteral("addr"), QStringLiteral("Modbus 从站地址")},
        {QStringLiteral("registerAddress"), QStringLiteral("保持寄存器地址（Air1电压常用1/4）")},
        {QStringLiteral("tempDeviceIndex"), QStringLiteral("温度仪设备序号（从 0）")},
        {QStringLiteral("tempDeviceIndexLock"), QStringLiteral("锁定温度仪设备序号")},
        {QStringLiteral("tempBaudRate"), QStringLiteral("温度仪串口波特率")},
        {QStringLiteral("tempRtsMode"), QStringLiteral("RTS模式：默认 RS232；RS485 转换器填 rs485，无控制填 none")},
        {QStringLiteral("tempLowC"), QStringLiteral("温度下限 ℃（可覆盖 Gate）")},
        {QStringLiteral("tempHighC"), QStringLiteral("温度上限 ℃（可覆盖 Gate）")},
        {QStringLiteral("sharedComName"), QStringLiteral("共享串口名（运行时解析）")},
        {QStringLiteral("txHex"), QStringLiteral("发送十六进制报文")},
        {QStringLiteral("string"), QStringLiteral("原文/字符串参数")},
        {QStringLiteral("line"), QStringLiteral("整机SN产线P（附表二；SCPI步骤则为原始行）")},
        {QStringLiteral("mLeft"), QStringLiteral("PLC 左工位线圈")},
        {QStringLiteral("mRight"), QStringLiteral("PLC 右工位线圈")},
        {QStringLiteral("address"), QStringLiteral("地址（如 M20、D100、X0）")},
        {QStringLiteral("value"), QStringLiteral("参数值（true/false 或数字）")},
        {QStringLiteral("quantity"), QStringLiteral("读取数量")},
        {QStringLiteral("comPort"), QStringLiteral("串口（如 COM4，留空则用默认）")},
        {QStringLiteral("portName"), QStringLiteral("串口（如 COM4，留空则用默认）")},
        {QStringLiteral("baudRate"), QStringLiteral("波特率")},
        {QStringLiteral("slaveId"), QStringLiteral("Modbus 从站地址")},
        {QStringLiteral("requestTimeoutMs"), QStringLiteral("应答超时 (ms)")},
        {QStringLiteral("parity"), QStringLiteral("校验位（even / none / odd）")},
        {QStringLiteral("expectReply"), QStringLiteral("写入是否等待并校验回包（默认 false 只发不收）")},
        {QStringLiteral("rtsMode"), QStringLiteral("RTS 模式（rs485 / none / enable）")},
        {QStringLiteral("readChannel"), QStringLiteral("治具读电流通道 CH1/CH2")},
        {QStringLiteral("MachineIndex"), QStringLiteral("治具机号")},
        {QStringLiteral("machineIndex"), QStringLiteral("治具机号")},
        {QStringLiteral("baseUrl"), QStringLiteral("接口地址")},
        {QStringLiteral("userName"), QStringLiteral("用户名")},
        {QStringLiteral("password"), QStringLiteral("密码")},
        {QStringLiteral("cameraIndex"), QStringLiteral("摄像头序号（从 0，仅 USB）")},
        {QStringLiteral("cameraName"), QStringLiteral("摄像头名称（可选，仅 USB）")},
        {QStringLiteral("cameraSource"), QStringLiteral("旧兼容：usb/gige（优先用目标设备 USB摄像头/GigE工业摄像头）")},
        {QStringLiteral("cameraIp"), QStringLiteral("GigE 相机 IP（如 169.254.64.10）")},
        {QStringLiteral("gigeIp"), QStringLiteral("GigE 相机 IP（同 cameraIp）")},
        {QStringLiteral("cameraSerial"), QStringLiteral("GigE 序列号（可选，优先于 IP）")},
        {QStringLiteral("warmupMs"), QStringLiteral("采集预热 (ms)")},
        {QStringLiteral("expectedColor"), QStringLiteral("期望纯色（下拉选：不判断/蓝绿红白黑灰）")},
        {QStringLiteral("deadDiff"), QStringLiteral("坏点残差阈值")},
        {QStringLiteral("deadRadiusPercent"), QStringLiteral("坏点检测区域缩放比例 (%)")},
        {QStringLiteral("referencePath"), QStringLiteral("参考图路径（双击选择，自动存入 screen_inspect/参考图）")},
        {QStringLiteral("saveCapture"), QStringLiteral("保存拍摄图（1开/0关）")},
        {QStringLiteral("roi"), QStringLiteral("检测范围 x,y,w,h（空=调试页划定/自动）")},
        {QStringLiteral("ledCount"), QStringLiteral("检测灯数 1~4（同一步分项）")},
        {QStringLiteral("minLuma"), QStringLiteral("亮灭亮度阈值 0~255")},
        {QStringLiteral("mode"), QStringLiteral("模式（RSSI：0=BLE，1=经典BT；吸奶：1~7；灯检：on/blink）")},
        {QStringLiteral("modes"), QStringLiteral("每灯模式 CSV，如 on,blink,on,on")},
        {QStringLiteral("mode1"), QStringLiteral("灯1：on常亮 / blink闪烁")},
        {QStringLiteral("mode2"), QStringLiteral("灯2：on常亮 / blink闪烁")},
        {QStringLiteral("mode3"), QStringLiteral("灯3：on常亮 / blink闪烁")},
        {QStringLiteral("mode4"), QStringLiteral("灯4：on常亮 / blink闪烁")},
        {QStringLiteral("sampleDurationMs"), QStringLiteral("有闪烁灯时采样窗口 ms")},
        {QStringLiteral("sampleIntervalMs"), QStringLiteral("采样间隔 ms")},
        {QStringLiteral("status"), QStringLiteral("泵开关（0停止/1运行）")},
        {QStringLiteral("level"), QStringLiteral("档位（1~15）")},
        {QStringLiteral("switch"), QStringLiteral("开关（0关/1开）")},
        {QStringLiteral("enter"), QStringLiteral("进入/退出（1进/0退）")},
        {QStringLiteral("customMode"), QStringLiteral("自定义模式")},
        {QStringLiteral("customLevel"), QStringLiteral("自定义档位")},
        {QStringLiteral("customFreq"), QStringLiteral("自定义频率（1低/2中/3高）")},
        {QStringLiteral("controlType"), QStringLiteral("控制侧（0左/1右/2双边）")},
        {QStringLiteral("pumpFreq"), QStringLiteral("泵频率（0低/1中/2高）")},
        {QStringLiteral("on"), QStringLiteral("开关（1进入/开，0退出/关）")},
        {QStringLiteral("state"), QStringLiteral("状态（1运行/0暂停）")},
        {QStringLiteral("which_sn"), QStringLiteral("SN 类型（整机SN / deviceName / productKey）")},
        {QStringLiteral("length"), QStringLiteral("写入SN长度（仅Qroot2：35=M5；39=Air1补0）")},
        {QStringLiteral("snLength"), QStringLiteral("写入SN长度（同 length）")},
        {QStringLiteral("sn"), QStringLiteral("SN 内容（写时填；读时可空）")},
        {QStringLiteral("done"), QStringLiteral("产测结果（1=完成，0=未完成）")},
        {QStringLiteral("color"), QStringLiteral("LCD/灯光颜色（0红1白2绿等）")},
        {QStringLiteral("side"), QStringLiteral("设备侧（0左/1右/2独立，Qaiot 常用）")},
        {QStringLiteral("field"), QStringLiteral("读取字段（如 soft_version/percent）")},
        {QStringLiteral("start"), QStringLiteral("上报开关（1开/0关）")},
        {QStringLiteral("type"), QStringLiteral("类型/子类型（按指令 hint）")},
        {QStringLiteral("dataType"), QStringLiteral("三元组数据类型（2/3/4）")},
        {QStringLiteral("data"), QStringLiteral("附加数据（hex，可选）")},
        {QStringLiteral("name"), QStringLiteral("名称（WiFi SSID / 广播名等）")},
        {QStringLiteral("rssi"), QStringLiteral("RSSI 阈值 (dBm)")},
        {QStringLiteral("currentMa"), QStringLiteral("电流 (mA)")},
        {QStringLiteral("percent"), QStringLiteral("电量百分比 (0~100)")},
        {QStringLiteral("command"), QStringLiteral("系统控制命令（1关机2重启3OTA4复位）")},
        {QStringLiteral("circleNum"), QStringLiteral("泵循环次数")},
        {QStringLiteral("durationTime"), QStringLiteral("时长")},
        {QStringLiteral("intervalTime"), QStringLiteral("间隔时间 (ms)")},
        {QStringLiteral("pumpPwm"), QStringLiteral("泵 PWM（0~100）")},
        {QStringLiteral("valveEnableTime"), QStringLiteral("阀开启时间")},
        {QStringLiteral("valveDisableTime"), QStringLiteral("阀关闭时间")},
        {QStringLiteral("valvePwm"), QStringLiteral("阀 PWM（0~100）")},
        {QStringLiteral("driveStrength"), QStringLiteral("驱动强度/PWM")},
        {QStringLiteral("freq"), QStringLiteral("频率")},
        {QStringLiteral("types"), QStringLiteral("多类型 CSV（如 0,1,8）")},
        {QStringLiteral("intervals"), QStringLiteral("多周期 CSV（ms，与 types 对齐）")},
        {QStringLiteral("low"), QStringLiteral("下限")},
        {QStringLiteral("high"), QStringLiteral("上限")},
        {QStringLiteral("deviceName"), QStringLiteral("广播设备名")},
        {QStringLiteral("connectionInterval"), QStringLiteral("连接间隔")},
        {QStringLiteral("mac"), QStringLiteral("MAC（可用 $MAC）")},
        {QStringLiteral("position"), QStringLiteral("左右位置（1左/2右/3单只/F未指定）")},
        {QStringLiteral("sku"), QStringLiteral("SKU")},
    };
    if (kMap.contains(k))
        return kMap.value(k);
    {
        QRegularExpression re(QStringLiteral(R"(^visaAddress_?(\d+)$)"), QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(k);
        if (m.hasMatch())
            return QStringLiteral("第 %1 台程控电源 VISA 地址").arg(m.captured(1).toInt() + 1);
    }
    {
        QRegularExpression re(QStringLiteral(R"(^(?:tempComName|sharedComName)_?(\d+)$)"),
                              QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = re.match(k);
        if (m.hasMatch())
            return QStringLiteral("第 %1 台温度仪串口名").arg(m.captured(1).toInt() + 1);
    }
    // 未登记键：界面仍显示英文键本身，悬停同样提示
    return k;
}

/** Hook 步骤是否需在编辑对话框展示 Param_ 参数表（非 Send 指令）。 */
bool hookUsesSendParamUi(const QString& hookId) {
    return hookId == QLatin1String("COUNTDOWN_WAIT") || hookId == QLatin1String("LIGHT_SENSOR_GOLDEN_CALIB")
        || hookId == QLatin1String("LIGHT_SENSOR_CALIB_WRITE") || hookId == QLatin1String("LIGHT_SENSOR_CALIB_READ")
        || hookId == QLatin1String("VES_CH1_SET_BRIGHTNESS") || hookId == QLatin1String("AIR_PRESSURE_SAMPLE")
        || hookId == QLatin1String("GENERATE_WHOLE_MACHINE_SN") || hookId == QLatin1String("SN_PREFIX4_CHECK")
        || hookId.startsWith(QLatin1String("PLC_SYNC_"));
}

QVariantMap hookSendParamDefaultMap(const QString& hookId) {
    if (hookId == QLatin1String("COUNTDOWN_WAIT")) {
        QVariantMap map;
        map.insert(QStringLiteral("seconds"), QStringLiteral("5"));
        map.insert(QStringLiteral("prompt"), QStringLiteral("请等待"));
        return map;
    }
    if (hookId == QLatin1String("SN_PREFIX4_CHECK")) {
        QVariantMap map;
        map.insert(QStringLiteral("expected"), QString());
        return map;
    }
    if (hookId == QLatin1String("GENERATE_WHOLE_MACHINE_SN")) {
        QVariantMap map;
        map.insert(QStringLiteral("spare"), QStringLiteral("0000"));
        map.insert(QStringLiteral("sku"), QString());
        map.insert(QStringLiteral("reserved"), QString());
        map.insert(QStringLiteral("version"), QString());
        map.insert(QStringLiteral("line"), QString());
        map.insert(QStringLiteral("serial"), QString());
        map.insert(QStringLiteral("pcbaType"), QStringLiteral("MB"));
        return map;
    }
    if (hookId == QLatin1String("LIGHT_SENSOR_GOLDEN_CALIB")) {
        QVariantMap map;
        map.insert(QStringLiteral("index"), QStringLiteral("0"));
        map.insert(QStringLiteral("golden"), QStringLiteral("38"));
        map.insert(QStringLiteral("waitMs"), QStringLiteral("5000"));
        map.insert(QStringLiteral("minSamples"), QStringLiteral("10"));
        map.insert(QStringLiteral("verifyWrite"), QStringLiteral("1"));
        map.insert(QStringLiteral("enableFivePointJudge"), QStringLiteral("1"));
        return map;
    }
    if (hookId == QLatin1String("LIGHT_SENSOR_CALIB_WRITE")) {
        QVariantMap map;
        map.insert(QStringLiteral("index"), QStringLiteral("0"));
        map.insert(QStringLiteral("golden"), QStringLiteral("38"));
        map.insert(QStringLiteral("waitMs"), QStringLiteral("5000"));
        map.insert(QStringLiteral("minSamples"), QStringLiteral("10"));
        return map;
    }
    if (hookId == QLatin1String("LIGHT_SENSOR_CALIB_READ")) {
        QVariantMap map;
        map.insert(QStringLiteral("index"), QStringLiteral("0"));
        map.insert(QStringLiteral("verifyWrite"), QStringLiteral("1"));
        map.insert(QStringLiteral("enableFivePointJudge"), QStringLiteral("1"));
        map.insert(QStringLiteral("rangeLo"), QStringLiteral("0"));
        map.insert(QStringLiteral("rangeHi"), QStringLiteral("70"));
        map.insert(QStringLiteral("rangeHiInc"), QStringLiteral("0"));
        map.insert(QStringLiteral("diffMin"), QStringLiteral("20"));
        return map;
    }
    if (hookId == QLatin1String("VES_CH1_SET_BRIGHTNESS")) {
        QVariantMap map;
        map.insert(QStringLiteral("baud"), QStringLiteral("9600"));
        map.insert(QStringLiteral("brightness"), QStringLiteral("22"));
        return map;
    }
    if (hookId == QLatin1String("AIR_PRESSURE_SAMPLE")) {
        QVariantMap map;
        map.insert(QStringLiteral("sampleCount"), QStringLiteral("10"));
        map.insert(QStringLiteral("intervalMs"), QStringLiteral("100"));
        map.insert(QStringLiteral("waitMs"), QStringLiteral("15000"));
        map.insert(QStringLiteral("channel"), QStringLiteral("both"));
        return map;
    }
    if (hookId.startsWith(QLatin1String("PLC_SYNC_WRITE"))) {
        QVariantMap map;
        QString defAddr = QStringLiteral("M10");
        if (hookId == QLatin1String("PLC_SYNC_WRITE_M11"))
            defAddr = QStringLiteral("M11");
        map.insert(QStringLiteral("address"), defAddr);
        map.insert(QStringLiteral("pulse"), QStringLiteral("true"));
        map.insert(QStringLiteral("pulseHoldMs"), QStringLiteral("200"));
        map.insert(QStringLiteral("syncWaitMs"), QStringLiteral("120000"));
        map.insert(QStringLiteral("value"), QStringLiteral("true"));
        return map;
    }
    if (hookId.startsWith(QLatin1String("PLC_SYNC_READ"))) {
        QVariantMap map;
        QString defAddr = QStringLiteral("M0");
        if (hookId == QLatin1String("PLC_SYNC_READ_M1"))
            defAddr = QStringLiteral("M1");
        map.insert(QStringLiteral("address"), defAddr);
        map.insert(QStringLiteral("syncWaitMs"), QStringLiteral("120000"));
        return map;
    }
    return {};
}

void setSendCommandFieldRowsVisible(Ui::TestCaseEditDialog* ui, QLabel* stationTargetLabel,
                                    QComboBox* stationTargetCombo, bool visible, bool stationBindingsMode) {
    if (!ui)
        return;
    ui->label_action->setVisible(visible);
    ui->comboBox_action->setVisible(visible);
    ui->label_deviceCmd->setVisible(visible);
    ui->comboBox_deviceCmd->setVisible(visible);
    if (!visible) {
        ui->label_sendChannel->setVisible(false);
        ui->comboBox_sendChannel->setVisible(false);
        ui->label_productProtocol->setVisible(false);
        ui->comboBox_productProtocol->setVisible(false);
        if (stationTargetLabel)
            stationTargetLabel->setVisible(false);
        if (stationTargetCombo)
            stationTargetCombo->setVisible(false);
        return;
    }
    if (stationBindingsMode) {
        ui->label_sendChannel->setVisible(false);
        ui->comboBox_sendChannel->setVisible(false);
        ui->label_productProtocol->setVisible(false);
        ui->comboBox_productProtocol->setVisible(false);
        if (stationTargetLabel)
            stationTargetLabel->setVisible(true);
        if (stationTargetCombo)
            stationTargetCombo->setVisible(true);
    } else {
        ui->label_sendChannel->setVisible(true);
        ui->comboBox_sendChannel->setVisible(true);
        if (stationTargetLabel)
            stationTargetLabel->setVisible(false);
        if (stationTargetCombo)
            stationTargetCombo->setVisible(false);
    }
}

QString stationTargetKeyForSend(TestCaseSendChannel channel, const QString& protoOrDevice) {
    switch (channel) {
    case TestCaseSendChannel::Product:
        return QStringLiteral("product:%1").arg(protoOrDevice);
    case TestCaseSendChannel::ProductSerial:
        return QStringLiteral("product_serial:%1").arg(protoOrDevice);
    case TestCaseSendChannel::Dongle:
        return QStringLiteral("dongle");
    case TestCaseSendChannel::Cloud:
        return QStringLiteral("cloud");
    case TestCaseSendChannel::Fixture:
        return QStringLiteral("fixture:%1").arg(protoOrDevice);
    case TestCaseSendChannel::Modbus:
        return QStringLiteral("modbus:%1").arg(protoOrDevice);
    case TestCaseSendChannel::Scpi:
        return QStringLiteral("scpi:%1").arg(protoOrDevice);
    }
    return {};
}

bool parseStationTargetKey(const QString& targetKey, TestCaseSendChannel& channel, QString& device) {
    const QString t = targetKey.trimmed();
    if (t.isEmpty())
        return false;
    if (t == QLatin1String("dongle")) {
        channel = TestCaseSendChannel::Dongle;
        device.clear();
        return true;
    }
    if (t == QLatin1String("cloud")) {
        channel = TestCaseSendChannel::Cloud;
        device.clear();
        return true;
    }
    const int colon = t.indexOf(QLatin1Char(':'));
    if (colon <= 0)
        return false;
    const QString kind = t.left(colon);
    const QString rest = t.mid(colon + 1).trimmed();
    if (kind == QLatin1String("product")) {
        channel = TestCaseSendChannel::Product;
        device = rest;
        return !rest.isEmpty();
    }
    if (kind == QLatin1String("product_serial")) {
        channel = TestCaseSendChannel::ProductSerial;
        device = rest;
        return !rest.isEmpty();
    }
    if (kind == QLatin1String("fixture")) {
        channel = TestCaseSendChannel::Fixture;
        device = rest;
        return !rest.isEmpty();
    }
    if (kind == QLatin1String("modbus")) {
        channel = TestCaseSendChannel::Modbus;
        device = rest;
        return !rest.isEmpty();
    }
    if (kind == QLatin1String("scpi")) {
        channel = TestCaseSendChannel::Scpi;
        device = rest;
        return !rest.isEmpty();
    }
    return false;
}

/** 目标设备展示名（【类别】短名）；用于绑定列表与旧步骤回显。 */
QString stationTargetUiLabel(const QString& targetKey) {
    TestCaseSendChannel channel = TestCaseSendChannel::Product;
    QString device;
    if (!parseStationTargetKey(targetKey, channel, device))
        return targetKey.trimmed();
    switch (channel) {
    case TestCaseSendChannel::Product:
        return QStringLiteral("【产品】%1")
            .arg(DeviceCmdCatalog::productProtocolUiLabel(DeviceCmdCatalog::productProtocolFromIni(device)));
    case TestCaseSendChannel::ProductSerial:
        return QStringLiteral("【产品串口】%1")
            .arg(DeviceCmdCatalog::productProtocolUiLabel(DeviceCmdCatalog::productProtocolFromIni(device)));
    case TestCaseSendChannel::Dongle:
        return QStringLiteral("【蓝牙Dongle】");
    case TestCaseSendChannel::Cloud:
        return QStringLiteral("【云端三元组】");
    case TestCaseSendChannel::Fixture:
        return QStringLiteral("【治具】%1")
            .arg(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(
                FixturePcbaCmdCatalog::fixtureProtocolFromIni(device)));
    case TestCaseSendChannel::Modbus:
        return QStringLiteral("【外设】%1")
            .arg(ModbusDeviceCatalog::deviceRouteUiLabel(ModbusDeviceCatalog::deviceRouteFromIni(device)));
    case TestCaseSendChannel::Scpi:
        return QStringLiteral("【电源/仪表】%1")
            .arg(ScpiPeriphCmdCatalog::deviceUiLabel(ScpiPeriphCmdCatalog::deviceFromIni(device)));
    }
    return targetKey.trimmed();
}

enum {
    SendParamKeyRole = Qt::UserRole + 31,
    SendParamSavedRole = Qt::UserRole + 32,
    SendParamTouchedRole = Qt::UserRole + 33,
    SendParamPlaceholderRole = Qt::UserRole + 34,
};

bool isSendParamExplicitEmptySentinel(const QString& text) {
    const QString t = text.trimmed();
    return t == QLatin1String("-") || t == QStringLiteral("(空)") || t == QStringLiteral("（空）");
}

void applySendParamValueCell(QTableWidgetItem* valItem, const QString& text, bool fromSavedIni, bool asPlaceholder) {
    if (!valItem)
        return;
    valItem->setText(text);
    valItem->setData(SendParamTouchedRole, false);
    valItem->setData(SendParamPlaceholderRole, asPlaceholder);
    QFont font = valItem->font();
    font.setItalic(asPlaceholder);
    valItem->setFont(font);
    if (asPlaceholder) {
        valItem->setForeground(QBrush(QColor(0x88, 0x88, 0x88)));
        valItem->setToolTip(QStringLiteral("灰色斜体为参考默认值，尚未写入 ini；保存工站步骤时会将非空参考默认一并写入。"
                                             "若需显式空值：改过后留空保存，或填「-」。"));
    } else {
        valItem->setForeground(QBrush());
        if (fromSavedIni && text.trimmed().isEmpty())
            valItem->setToolTip(QStringLiteral("已显式保存为空值（Param_键=）"));
        else
            valItem->setToolTip(QString());
    }
}

/** 将灰色占位格转为可保存的普通值（编辑确认或保存前 materialize）。 */
void materializeSendParamValueItem(QTableWidgetItem* valItem) {
    if (!valItem)
        return;
    valItem->setData(SendParamTouchedRole, true);
    valItem->setData(SendParamPlaceholderRole, false);
    QFont font = valItem->font();
    font.setItalic(false);
    valItem->setFont(font);
    valItem->setForeground(QBrush());
    if (valItem->text().trimmed().isEmpty())
        valItem->setToolTip(QStringLiteral("已显式保存为空值（Param_键=）"));
    else
        valItem->setToolTip(QString());
}

/** 保存工站步骤前：非空灰色参考默认一并写入 ini，便于跨机 profiles 不缺参。 */
void materializeAllSendParamPlaceholdersForSave(QTableWidget* table) {
    if (!table || table->columnCount() < 2)
        return;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem* valItem = table->item(r, 1);
        if (!valItem || !valItem->data(SendParamPlaceholderRole).toBool())
            continue;
        if (valItem->text().trimmed().isEmpty())
            continue;
        materializeSendParamValueItem(valItem);
    }
}

QTableWidgetItem* makeSendParamValueItem(const QString& text, bool fromSavedIni, bool asPlaceholder) {
    auto* item = new QTableWidgetItem();
    applySendParamValueCell(item, text, fromSavedIni, asPlaceholder);
    return item;
}

void applySendParamNameCell(QTableWidgetItem* nameItem, const QString& key) {
    if (!nameItem)
        return;
    const QString k = key.trimmed();
    const QString zh = sendParamKeyZhLabel(k);
    nameItem->setData(SendParamKeyRole, k);
    if (k.isEmpty()) {
        nameItem->setText(QString());
    } else if (zh == k) {
        nameItem->setText(k);
    } else {
        nameItem->setText(QStringLiteral("%1  (%2)").arg(zh, k));
    }
    nameItem->setFlags((nameItem->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);
    nameItem->setToolTip(k.isEmpty() ? QStringLiteral("双击可填写英文参数名")
                                      : QStringLiteral("英文参数名：%1").arg(k));
}

QTableWidgetItem* makeSendParamNameItem(const QString& key) {
    auto* item = new QTableWidgetItem();
    applySendParamNameCell(item, key);
    return item;
}

void configureSendParamTable(QTableWidget* table) {
    if (!table)
        return;
    table->clear();
    table->setRowCount(0);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("参数说明 (英文键)"), QStringLiteral("参数值（灰斜体=参考默认；保存工站步骤时非空默认会写入 ini）")});
    table->setColumnWidth(0, 320);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
}

/** 三元组云端登录等指令的参数字段固定显示顺序（账号在密码上方）。 */
QStringList sendParamPreferredOrder(TestCaseSendChannel channel, const QString& cmdName,
                                    TestCaseSendAction action = TestCaseSendAction::Set) {
    if (channel == TestCaseSendChannel::Cloud && cmdName == QLatin1String("Login"))
        return {QStringLiteral("baseUrl"), QStringLiteral("userName"), QStringLiteral("password")};
    if (channel == TestCaseSendChannel::Dongle && cmdName == QLatin1String("SampleSuctionDual")) {
        return {QStringLiteral("sampleDurationMs"), QStringLiteral("sampleIntervalMs"),
                QStringLiteral("minPeakCount"), QStringLiteral("peakBaselineKpa"),
                QStringLiteral("peakDipStartKpa"), QStringLiteral("offsetKpa"),
                QStringLiteral("showCurveInTable")};
    }
    if (channel == TestCaseSendChannel::Dongle && cmdName == QLatin1String("SampleSuctionSingle")) {
        return {QStringLiteral("sampleDurationMs"), QStringLiteral("sampleIntervalMs"), QStringLiteral("channel"),
                QStringLiteral("minPeakCount"), QStringLiteral("peakBaselineKpa"),
                QStringLiteral("peakDipStartKpa"), QStringLiteral("offsetKpa"),
                QStringLiteral("showCurveInTable")};
    }
    if (channel == TestCaseSendChannel::Fixture && cmdName == QLatin1String("SetBrightness"))
        return {QStringLiteral("brightness")};
    if (channel == TestCaseSendChannel::Fixture
        && (cmdName == QLatin1String("ScreenDeadPixelCheck")
            || cmdName == QLatin1String("ScreenDisplayAnomalyCheck")
            || cmdName == QLatin1String("ScreenCameraCalibration"))) {
        // cameraSource 已拆到设备协议；参数表按 USB/GigE 设备区分（编辑时仍可能看到旧键）
        QStringList keys = {QStringLiteral("cameraIp"), QStringLiteral("cameraSerial"),
                            QStringLiteral("cameraIndex"), QStringLiteral("cameraName"),
                            QStringLiteral("warmupMs"), QStringLiteral("expectedColor"), QStringLiteral("deadDiff"),
                            QStringLiteral("deadRadiusPercent"), QStringLiteral("saveCapture"), QStringLiteral("roi"),
                            QStringLiteral("reuseCircleRoi"), QStringLiteral("cameraSource")};
        if (cmdName == QLatin1String("ScreenDisplayAnomalyCheck")
            || cmdName == QLatin1String("ScreenCameraCalibration"))
            keys.append(QStringLiteral("referencePath"));
        return keys;
    }
    if (channel == TestCaseSendChannel::Fixture && cmdName == QLatin1String("LedOnOffCheck")) {
        return {QStringLiteral("cameraIp"), QStringLiteral("cameraSerial"), QStringLiteral("cameraIndex"),
                QStringLiteral("cameraName"), QStringLiteral("warmupMs"), QStringLiteral("saveCapture"),
                QStringLiteral("ledCount"), QStringLiteral("minLuma"), QStringLiteral("mode1"),
                QStringLiteral("mode2"), QStringLiteral("mode3"), QStringLiteral("mode4"), QStringLiteral("modes"),
                QStringLiteral("mode"), QStringLiteral("sampleDurationMs"), QStringLiteral("sampleIntervalMs"),
                QStringLiteral("cameraSource")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && (cmdName == QLatin1String("RootPumpControl") || cmdName == QLatin1String("RootSuctionTest"))) {
        return {QStringLiteral("status"), QStringLiteral("switch"), QStringLiteral("mode"), QStringLiteral("level"),
                QStringLiteral("customMode"), QStringLiteral("customLevel"), QStringLiteral("customFreq"),
                QStringLiteral("controlType"), QStringLiteral("pumpFreq")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("Root2ModeSet")) {
        return {QStringLiteral("mode")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("Root2LevelSet")) {
        return {QStringLiteral("level")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("Root2PumpState")) {
        return {QStringLiteral("on"), QStringLiteral("state"), QStringLiteral("value")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("FacMode")) {
        return {QStringLiteral("on"), QStringLiteral("mode"), QStringLiteral("value")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("Sn")) {
        // 读 SN 只需类型；写才要内容/长度
        if (action == TestCaseSendAction::Get)
            return {QStringLiteral("which_sn")};
        return {QStringLiteral("sn"), QStringLiteral("which_sn"), QStringLiteral("length")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("RssiRead")) {
        return {QStringLiteral("mode")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("FacResult")) {
        return {QStringLiteral("done")};
    }
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && cmdName == QLatin1String("FacMode")) {
        return {QStringLiteral("on"), QStringLiteral("mode")};
    }
    return {};
}

bool isDroppedSendParamKey(const QString& key) {
    return key.compare(QLatin1String("setScreenColor"), Qt::CaseInsensitive) == 0;
}

void sortSendParamKeys(QStringList& keys, const QStringList& preferredOrder) {
    QStringList ordered;
    for (const QString& k : preferredOrder) {
        if (keys.contains(k))
            ordered.append(k);
    }
    QStringList rest;
    for (const QString& k : keys) {
        if (!ordered.contains(k))
            rest.append(k);
    }
    rest.sort(Qt::CaseInsensitive);
    keys = ordered + rest;
}

void setSendParamTableFromMap(QTableWidget* table, const QVariantMap& map,
                              const QStringList& preferredOrder = {}) {
    QSignalBlocker blocker(table);
    configureSendParamTable(table);
    QStringList keys = map.keys();
    keys.erase(std::remove_if(keys.begin(), keys.end(), isDroppedSendParamKey), keys.end());
    sortSendParamKeys(keys, preferredOrder);
    for (const QString& key : keys) {
        const int r = table->rowCount();
        table->insertRow(r);
        QTableWidgetItem* nameItem = makeSendParamNameItem(key);
        nameItem->setData(SendParamSavedRole, true);
        table->setItem(r, 0, nameItem);
        table->setItem(r, 1, makeSendParamValueItem(map.value(key).toString(), true, false));
    }
    if (table->rowCount() == 0) {
        table->insertRow(0);
        table->setItem(0, 0, makeSendParamNameItem(QString()));
        table->setItem(0, 1, makeSendParamValueItem(QString(), false, false));
    }
}

/** 按模板列出全部参数字段；userMap 覆盖已有值，模板缺省值仅作灰色占位提示。 */
void setSendParamTableFromMapWithTemplate(QTableWidget* table, const QVariantMap& userMap,
                                          const QVariantMap& templateMap,
                                          const QStringList& preferredOrder = {}) {
    QSignalBlocker blocker(table);
    configureSendParamTable(table);
    QStringList ordered;
    for (const QString& k : templateMap.keys()) {
        if (!k.isEmpty() && !isDroppedSendParamKey(k) && !ordered.contains(k))
            ordered.append(k);
    }
    QStringList extras;
    for (const QString& k : userMap.keys()) {
        if (!k.isEmpty() && !isDroppedSendParamKey(k) && !ordered.contains(k))
            extras.append(k);
    }
    extras.sort(Qt::CaseInsensitive);
    ordered.append(extras);
    sortSendParamKeys(ordered, preferredOrder);

    for (const QString& key : ordered) {
        const int r = table->rowCount();
        table->insertRow(r);
        QTableWidgetItem* nameItem = makeSendParamNameItem(key);
        if (userMap.contains(key)) {
            nameItem->setData(SendParamSavedRole, true);
            table->setItem(r, 0, nameItem);
            table->setItem(r, 1, makeSendParamValueItem(userMap.value(key).toString(), true, false));
            continue;
        }
        table->setItem(r, 0, nameItem);
        const QString tmplVal = templateMap.value(key).toString();
        const bool asPlaceholder = templateMap.contains(key) && !tmplVal.isEmpty();
        table->setItem(r, 1, makeSendParamValueItem(asPlaceholder ? tmplVal : QString(), false, asPlaceholder));
    }
    if (table->rowCount() == 0) {
        table->insertRow(0);
        table->setItem(0, 0, makeSendParamNameItem(QString()));
        table->setItem(0, 1, makeSendParamValueItem(QString(), false, false));
    }
}

/** 配置程控电源：设备缺省 SCPI 参数模板。 */
QVariantMap defaultVisaConfigureParamMap(ScpiDeviceRoute route) {
    QVariantMap map;
    if (route == ScpiDeviceRoute::Agilent66319d) {
        map.insert(QStringLiteral("visaAddress"), QStringLiteral("GPIB0::7::INSTR"));
        map.insert(QStringLiteral("voltage"), QStringLiteral("5.0"));
        map.insert(QStringLiteral("current"), QStringLiteral("3.0"));
        map.insert(QStringLiteral("currentRange"), QStringLiteral("3"));
        map.insert(QStringLiteral("scpiSetVoltageCmd"), QStringLiteral("VOLT %1"));
        map.insert(QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("CURR %1"));
        map.insert(QStringLiteral("scpiOutputOnCmd"), QStringLiteral("OUTP ON"));
        map.insert(QStringLiteral("scpiOutputOffCmd"), QStringLiteral("OUTP OFF"));
        map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT:DC?"));
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS:CURR:DC?"));
        map.insert(QStringLiteral("scpiSetCurrentRangeCmd"), QStringLiteral("SENS:CURR:RANG %1"));
        // 一拖多两工位共一台双通道电源：在步骤里配 sharedPair + visaAddress0/1（不走上位机设置）
        map.insert(QStringLiteral("sharedPair"), QStringLiteral("true"));
        map.insert(QStringLiteral("stationsPerDevice"), QStringLiteral("2"));
        map.insert(QStringLiteral("visaAddress0"), QStringLiteral("GPIB0::7::INSTR"));
        map.insert(QStringLiteral("visaAddress1"), QStringLiteral("GPIB0::8::INSTR"));
        map.insert(QStringLiteral("scpiChannelSelectCmd"), QStringLiteral("INST OUT%1"));
        return map;
    }
    if (route == ScpiDeviceRoute::GenericVisaPower) {
        map.insert(QStringLiteral("visaAddress"), QStringLiteral("GPIB0::5::INSTR"));
        map.insert(QStringLiteral("voltage"), QStringLiteral("5.0"));
        map.insert(QStringLiteral("current"), QStringLiteral("2.0"));
        map.insert(QStringLiteral("currentRange"), QString());
        map.insert(QStringLiteral("scpiSetVoltageCmd"), QStringLiteral("VOLT %1"));
        map.insert(QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("CURR %1"));
        map.insert(QStringLiteral("scpiOutputOnCmd"), QStringLiteral("OUTP ON"));
        map.insert(QStringLiteral("scpiOutputOffCmd"), QStringLiteral("OUTP OFF"));
        map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT?"));
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS:CURR?"));
        map.insert(QStringLiteral("scpiSetCurrentRangeCmd"), QString());
        map.insert(QStringLiteral("sharedPair"), QStringLiteral("false"));
        map.insert(QStringLiteral("stationsPerDevice"), QStringLiteral("1"));
        map.insert(QStringLiteral("visaAddress0"), QString());
        map.insert(QStringLiteral("visaAddress1"), QString());
        map.insert(QStringLiteral("scpiChannelSelectCmd"), QString());
        return map;
    }
    map.insert(QStringLiteral("visaAddress"), QStringLiteral("TCPIP::localhost::5026::SOCKET"));
    map.insert(QStringLiteral("voltage"), QStringLiteral("12.0"));
    map.insert(QStringLiteral("current"), QStringLiteral("2.5"));
    // 会凌短写，与 huiling_wfp60h_profile::defaults 一致
    map.insert(QStringLiteral("scpiSetVoltageCmd"), QStringLiteral("SOUR1:VOLT %1"));
    map.insert(QStringLiteral("scpiSetCurrentCmd"), QStringLiteral("SOUR1:CURR %1"));
    map.insert(QStringLiteral("scpiOutputOnCmd"), QStringLiteral("OUTP1 ON"));
    map.insert(QStringLiteral("scpiOutputOffCmd"), QStringLiteral("OUTP1 OFF"));
    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS1:VOLT:DC?"));
    map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS1:CURR:DC?"));
    return map;
}

QVariantMap defaultVisaReadCurrentParamMap(ScpiDeviceRoute route) {
    QVariantMap map;
    if (route == ScpiDeviceRoute::Agilent66319d) {
        map.insert(QStringLiteral("currentRange"), QStringLiteral("3"));
        map.insert(QStringLiteral("scpiSetCurrentRangeCmd"), QStringLiteral("SENS:CURR:RANG %1"));
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS:CURR:DC?"));
    } else if (route == ScpiDeviceRoute::GenericVisaPower) {
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS:CURR?"));
        map.insert(QStringLiteral("currentRange"), QString());
        map.insert(QStringLiteral("scpiSetCurrentRangeCmd"), QString());
    } else {
        map.insert(QStringLiteral("scpiReadCurrentCmd"), QStringLiteral("MEAS1:CURR:DC?"));
    }
    map.insert(QStringLiteral("sampleDurationMs"), QStringLiteral("3000"));
    map.insert(QStringLiteral("sampleIntervalMs"), QStringLiteral("200"));
    return map;
}

QVariantMap sendParamDefaultMapForCmd(TestCaseSendChannel channel, const QString& device, const QString& cmdName,
                                      TestCaseSendAction action = TestCaseSendAction::Set) {
    if (channel == TestCaseSendChannel::Scpi) {
        const ScpiDeviceRoute route = ScpiPeriphCmdCatalog::deviceFromIni(device);
        if (isVisaProgrammablePowerRoute(route)) {
            if (cmdName == QLatin1String("ConfigureProgrammablePower"))
                return defaultVisaConfigureParamMap(route);
            if (cmdName == QLatin1String("ReadProgrammableCurrent"))
                return defaultVisaReadCurrentParamMap(route);
            if (cmdName == QLatin1String("ReadProgrammableVoltage")) {
                QVariantMap map;
                if (route == ScpiDeviceRoute::Agilent66319d)
                    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT:DC?"));
                else if (route == ScpiDeviceRoute::GenericVisaPower)
                    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT?"));
                else
                    map.insert(QStringLiteral("scpiReadVoltageCmd"), QStringLiteral("MEAS1:VOLT:DC?"));
                return map;
            }
            if (cmdName == QLatin1String("SendRawLine")) {
                return QVariantMap{{QStringLiteral("line"), QString()}};
            }
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Modbus
        && ModbusDeviceCatalog::deviceRouteFromIni(device) == ModbusDeviceRoute::MultiTempLoggerRtu) {
        if (cmdName == QLatin1String("ReadChannelTemp")) {
            QVariantMap map;
            map.insert(QStringLiteral("slaveAddr"), QStringLiteral("1"));
            map.insert(QStringLiteral("sharedPair"), QStringLiteral("true"));
            map.insert(QStringLiteral("stationsPerDevice"), QStringLiteral("2"));
            map.insert(QStringLiteral("channelsPerStation"), QStringLiteral("6"));
            map.insert(QStringLiteral("tempComName0"), QStringLiteral("COM10"));
            map.insert(QStringLiteral("tempComName1"), QStringLiteral("COM11"));
            map.insert(QStringLiteral("tempBaudRate"), QStringLiteral("115200"));
            map.insert(QStringLiteral("sampleDurationMs"), QStringLiteral("20000"));
            map.insert(QStringLiteral("sampleIntervalMs"), QStringLiteral("500"));
            return map;
        }
        if (cmdName == QLatin1String("SendRaw")) {
            return QVariantMap{{QStringLiteral("txHex"), QStringLiteral("01 03 00 12 00 02 64 0E")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Modbus
        && ModbusDeviceCatalog::deviceRouteFromIni(device) == ModbusDeviceRoute::XinjiePlcRtu) {
        if (cmdName == QLatin1String("Connect")) {
            // comPort 留空占位：用户填写后须写入 ini；勿填 COM11 等硬编码以免误保存
            return QVariantMap{{QStringLiteral("comPort"), QString()},
                               {QStringLiteral("baudRate"), QStringLiteral("19200")},
                               {QStringLiteral("slaveId"), QStringLiteral("1")},
                               {QStringLiteral("parity"), QStringLiteral("even")},
                               {QStringLiteral("rtsMode"), QStringLiteral("none")}};
        }
        if (cmdName == QLatin1String("WriteCoil")) {
            return QVariantMap{{QStringLiteral("address"), QStringLiteral("M20")},
                               {QStringLiteral("value"), QStringLiteral("true")},
                               {QStringLiteral("expectReply"), QStringLiteral("false")}};
        }
        if (cmdName == QLatin1String("WriteRegister")) {
            return QVariantMap{{QStringLiteral("address"), QStringLiteral("D100")},
                               {QStringLiteral("value"), QStringLiteral("0")},
                               {QStringLiteral("expectReply"), QStringLiteral("false")}};
        }
        if (cmdName == QLatin1String("ReadCoils") || cmdName == QLatin1String("ReadDiscreteInputs")
            || cmdName == QLatin1String("ReadHoldingRegisters")) {
            return QVariantMap{{QStringLiteral("address"), QStringLiteral("M20")},
                               {QStringLiteral("quantity"), QStringLiteral("1")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial) {
        if (cmdName == QLatin1String("RootPumpControl")) {
            // 0xC0：status/mode/level 必填；其余可按需改（吸乳模式常见 mode=1）
            return QVariantMap{{QStringLiteral("status"), QStringLiteral("1")},
                               {QStringLiteral("mode"), QStringLiteral("1")},
                               {QStringLiteral("level"), QStringLiteral("15")},
                               {QStringLiteral("customMode"), QStringLiteral("1")},
                               {QStringLiteral("customLevel"), QStringLiteral("1")},
                               {QStringLiteral("customFreq"), QStringLiteral("2")},
                               {QStringLiteral("controlType"), QStringLiteral("2")},
                               {QStringLiteral("pumpFreq"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("RootSuctionTest")) {
            return QVariantMap{{QStringLiteral("switch"), QStringLiteral("1")},
                               {QStringLiteral("mode"), QStringLiteral("1")},
                               {QStringLiteral("level"), QStringLiteral("8")}};
        }
        if (cmdName == QLatin1String("Root2ModeSet")) {
            return QVariantMap{{QStringLiteral("mode"), QStringLiteral("2")}};
        }
        if (cmdName == QLatin1String("Root2LevelSet")) {
            return QVariantMap{{QStringLiteral("level"), QStringLiteral("15")}};
        }
        if (cmdName == QLatin1String("Root2PumpState")) {
            return QVariantMap{{QStringLiteral("on"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("RssiRead")) {
            // 0=BLE，1=经典 BT
            return QVariantMap{{QStringLiteral("mode"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("FacResult")) {
            // 1=产测完成，0=未完成（兼容旧 Param_int）
            return QVariantMap{{QStringLiteral("done"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("FacMode")) {
            return QVariantMap{{QStringLiteral("on"), QStringLiteral("1")},
                               {QStringLiteral("mode"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("Sleep")) {
            return QVariantMap{{QStringLiteral("switch"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("ForbidSleep")) {
            return QVariantMap{{QStringLiteral("value"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("LedTest") || cmdName == QLatin1String("LcdBacklight")) {
            return QVariantMap{{QStringLiteral("on"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("LcdColorTestMode")) {
            return QVariantMap{{QStringLiteral("enter"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("SetLcdColor")) {
            return QVariantMap{{QStringLiteral("color"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("CompensationSet")) {
            return QVariantMap{{QStringLiteral("enable"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("BtSignalMode") || cmdName == QLatin1String("BtNoSignalMode")
            || cmdName == QLatin1String("BtFreqMode")) {
            return QVariantMap{{QStringLiteral("enter"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("ChargeCurrentSet")) {
            return QVariantMap{{QStringLiteral("currentMa"), QStringLiteral("2000")}};
        }
        if (cmdName == QLatin1String("SetBattery")) {
            return QVariantMap{{QStringLiteral("percent"), QStringLiteral("50")}};
        }
        if (cmdName == QLatin1String("RootHeatLevelControl")) {
            return QVariantMap{{QStringLiteral("switch"), QStringLiteral("1")},
                               {QStringLiteral("level"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("RootVibration")) {
            return QVariantMap{{QStringLiteral("value"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("RootSystemControl")) {
            return QVariantMap{{QStringLiteral("command"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("PressSampleIntervalSet")) {
            return QVariantMap{{QStringLiteral("intervalMs"), QStringLiteral("50")}};
        }
        if (cmdName == QLatin1String("LightCalibWrite")) {
            return QVariantMap{{QStringLiteral("index"), QStringLiteral("0")},
                               {QStringLiteral("value"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("LightCalibRead")) {
            return QVariantMap{{QStringLiteral("index"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("KeySignalRead")) {
            return QVariantMap{{QStringLiteral("key"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("AgingStatusRead")) {
            return QVariantMap{{QStringLiteral("mode"), QStringLiteral("2")}};
        }
        if (cmdName == QLatin1String("WifiConnect")) {
            return QVariantMap{{QStringLiteral("name"), QStringLiteral("TestAP")},
                               {QStringLiteral("password"), QStringLiteral("12345678")}};
        }
        if (cmdName == QLatin1String("Sn")) {
            // Get：只读类型（1=整机SN）；Set：内容 + 类型（+ Qroot2 length）
            if (action == TestCaseSendAction::Get)
                return QVariantMap{{QStringLiteral("which_sn"), QStringLiteral("1")}};
            QVariantMap m{{QStringLiteral("sn"), QStringLiteral("$WHOLE_MACHINE_SN")},
                          {QStringLiteral("which_sn"), QStringLiteral("1")}};
            if (device.contains(QLatin1String("Qroot2"), Qt::CaseInsensitive)
                || device.contains(QLatin1String("root2"), Qt::CaseInsensitive))
                m.insert(QStringLiteral("length"), QStringLiteral("35"));
            return m;
        }
        if (cmdName == QLatin1String("SuctionMode")) {
            return QVariantMap{{QStringLiteral("enter"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("BurningMode")) {
            return QVariantMap{{QStringLiteral("mode"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("Root2BowlCalib")) {
            return QVariantMap{{QStringLiteral("value"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("SoftVersionRead")) {
            return QVariantMap{{QStringLiteral("field"), QStringLiteral("soft_version")}};
        }
        if (cmdName == QLatin1String("GetBattery")) {
            return QVariantMap{{QStringLiteral("field"), QStringLiteral("percent")}};
        }
        if (cmdName == QLatin1String("MacWrite")) {
            return QVariantMap{{QStringLiteral("value"), QStringLiteral("$MAC")},
                               {QStringLiteral("side"), QStringLiteral("2")}};
        }
        if (cmdName == QLatin1String("WriteKey")) {
            return QVariantMap{{QStringLiteral("value"), QStringLiteral("$TUPLE_DEVICE_SECRET")},
                               {QStringLiteral("side"), QStringLiteral("2")}};
        }
        if (cmdName == QLatin1String("MacRead") || cmdName == QLatin1String("TupleRead")
            || cmdName == QLatin1String("FactoryReset") || cmdName == QLatin1String("ShipMode")
            || cmdName == QLatin1String("DevReset") || cmdName == QLatin1String("TravelLock")) {
            // Qaiot 常用 side；FCTP/Qroot 无参也可忽略该键
            return QVariantMap{{QStringLiteral("side"), QStringLiteral("2")}};
        }
        if (cmdName == QLatin1String("PeriphState")) {
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("PressCollect")) {
            return QVariantMap{{QStringLiteral("on"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("LightReportControl")) {
            return QVariantMap{{QStringLiteral("start"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("Root2PoseSwitch")) {
            return QVariantMap{{QStringLiteral("on"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("Root2LedControl")) {
            return QVariantMap{{QStringLiteral("color"), QStringLiteral("0")},
                               {QStringLiteral("on"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("ExceptionThresholdRead")) {
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("1")}};
        }
        if (cmdName == QLatin1String("ExceptionThresholdWrite")) {
            return QVariantMap{{QStringLiteral("type"), QStringLiteral("1")},
                               {QStringLiteral("value"), QStringLiteral("20")}};
        }
        if (cmdName == QLatin1String("PumpParamWrite")) {
            return QVariantMap{{QStringLiteral("circleNum"), QStringLiteral("1")},
                               {QStringLiteral("durationTime"), QStringLiteral("1000")},
                               {QStringLiteral("intervalTime"), QStringLiteral("100")},
                               {QStringLiteral("pumpPwm"), QStringLiteral("50")}};
        }
        if (cmdName == QLatin1String("ValveParamWrite")) {
            return QVariantMap{{QStringLiteral("valveEnableTime"), QStringLiteral("100")},
                               {QStringLiteral("valveDisableTime"), QStringLiteral("100")},
                               {QStringLiteral("valvePwm"), QStringLiteral("50")}};
        }
        if (cmdName == QLatin1String("HeatTestWrite")) {
            return QVariantMap{{QStringLiteral("enable"), QStringLiteral("1")},
                               {QStringLiteral("driveStrength"), QStringLiteral("50")},
                               {QStringLiteral("durationTime"), QStringLiteral("1000")}};
        }
        if (cmdName == QLatin1String("VibrationTestWrite")) {
            return QVariantMap{{QStringLiteral("enable"), QStringLiteral("1")},
                               {QStringLiteral("driveStrength"), QStringLiteral("50")},
                               {QStringLiteral("freq"), QStringLiteral("100")},
                               {QStringLiteral("durationTime"), QStringLiteral("1000")}};
        }
        if (cmdName == QLatin1String("CycleReportWrite")) {
            return QVariantMap{{QStringLiteral("enable"), QStringLiteral("1")},
                               {QStringLiteral("type"), QStringLiteral("0")},
                               {QStringLiteral("intervalTime"), QStringLiteral("100")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Dongle) {
        if (cmdName == QLatin1String("BleScanConnectByName")) {
            return QVariantMap{{QStringLiteral("name"), QStringLiteral("M5 Ultra")},
                               {QStringLiteral("rssi"), QStringLiteral("-70")}};
        }
        if (cmdName == QLatin1String("Bomb")) {
            return QVariantMap{{QStringLiteral("deviceName"), QStringLiteral("Device")},
                               {QStringLiteral("rssi"), QStringLiteral("-50")},
                               {QStringLiteral("connectionInterval"), QStringLiteral("0")},
                               {QStringLiteral("command"), QStringLiteral("0")}};
        }
        if (cmdName == QLatin1String("SampleSuctionSingle")) {
            return QVariantMap{{QStringLiteral("sampleDurationMs"), QStringLiteral("10000")},
                               {QStringLiteral("sampleIntervalMs"), QStringLiteral("20")},
                               {QStringLiteral("channel"), QStringLiteral("1")},
                               {QStringLiteral("minPeakCount"), QStringLiteral("3")},
                               {QStringLiteral("peakBaselineKpa"), QStringLiteral("-8")},
                               {QStringLiteral("peakDipStartKpa"), QStringLiteral("-10")},
                               {QStringLiteral("offsetKpa"), QStringLiteral("0")},
                               {QStringLiteral("showCurveInTable"), QStringLiteral("true")}};
        }
        if (cmdName == QLatin1String("SampleSuctionDual")) {
            return QVariantMap{{QStringLiteral("sampleDurationMs"), QStringLiteral("10000")},
                               {QStringLiteral("sampleIntervalMs"), QStringLiteral("20")},
                               {QStringLiteral("minPeakCount"), QStringLiteral("3")},
                               {QStringLiteral("peakBaselineKpa"), QStringLiteral("-8")},
                               {QStringLiteral("peakDipStartKpa"), QStringLiteral("-10")},
                               {QStringLiteral("offsetKpa"), QStringLiteral("0")},
                               {QStringLiteral("showCurveInTable"), QStringLiteral("true")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Fixture
        && FixturePcbaCmdCatalog::fixtureProtocolFromIni(device) == TestCaseFixtureProtocol::Asd9026a) {
        if (cmdName == QLatin1String("ConfigureProgrammablePower")) {
            return QVariantMap{{QStringLiteral("voltage"), QStringLiteral("4.0")},
                               {QStringLiteral("current"), QStringLiteral("2.0")},
                               {QStringLiteral("currentRange"), QStringLiteral("4")},
                               {QStringLiteral("txHex"),
                                QStringLiteral("02 21 10 0D 00 00 3D 09 00 00 00 07 D0 04 01 00 00 91 8F")}};
        }
        if (cmdName == QLatin1String("ConfigureCurrentMeasureRange")) {
            return QVariantMap{
                {QStringLiteral("currentRange"), QStringLiteral("4")},
                {QStringLiteral("txHex"), QStringLiteral("02 21 10 0D 00 00 00 00 00 00 00 00 00 04 00 00 00 14 A9")}};
        }
        if (cmdName == QLatin1String("ProgrammablePowerOutput")) {
            return QVariantMap{{QStringLiteral("enable"), QStringLiteral("1")},
                               {QStringLiteral("txHex"), QStringLiteral("02 11 04 03 01 00 00 9B C5")}};
        }
        if (cmdName == QLatin1String("ReadProgrammableVoltage")
            || cmdName == QLatin1String("ReadProgrammableCurrent")) {
            QVariantMap map{{QStringLiteral("txHex"), QStringLiteral("02 20 10 00 0D 96")}};
            if (cmdName == QLatin1String("ReadProgrammableCurrent")) {
                map.insert(QStringLiteral("sampleDurationMs"), QStringLiteral("3000"));
                map.insert(QStringLiteral("sampleIntervalMs"), QStringLiteral("200"));
            }
            return map;
        }
        if (cmdName == QLatin1String("SendRaw")) {
            return QVariantMap{{QStringLiteral("txHex"), QStringLiteral("02 11 04 03 01 00 00 9B C5")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Modbus
        && device.compare(QLatin1String("FaFbFloatAmmeterRtu"), Qt::CaseInsensitive) == 0) {
        if (cmdName == QLatin1String("ReadVoltageCentivolt")) {
            return QVariantMap{{QStringLiteral("slaveAddr"), QStringLiteral("1")},
                               {QStringLiteral("registerAddress"), QStringLiteral("1")},
                               {QStringLiteral("sampleDurationMs"), QStringLiteral("3000")},
                               {QStringLiteral("sampleIntervalMs"), QStringLiteral("200")}};
        }
        if (cmdName == QLatin1String("ReadMeasurement")) {
            return QVariantMap{{QStringLiteral("sampleDurationMs"), QStringLiteral("3000")},
                               {QStringLiteral("sampleIntervalMs"), QStringLiteral("200")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Fixture
        && FixturePcbaCmdCatalog::fixtureProtocolFromIni(device) == TestCaseFixtureProtocol::Xwd) {
        return {};
    }
    if (channel == TestCaseSendChannel::Fixture
        && FixturePcbaCmdCatalog::isScreenCameraFixture(
               FixturePcbaCmdCatalog::fixtureProtocolFromIni(device))) {
        const bool usbCam =
            FixturePcbaCmdCatalog::fixtureProtocolFromIni(device) == TestCaseFixtureProtocol::UsbCamera;
        if (cmdName == QLatin1String("ScreenDeadPixelCheck")
            || cmdName == QLatin1String("ScreenDisplayAnomalyCheck")
            || cmdName == QLatin1String("ScreenCameraCalibration")) {
            QVariantMap map{{QStringLiteral("warmupMs"), QStringLiteral("450")},
                            {QStringLiteral("expectedColor"), QStringLiteral("不判断")},
                            {QStringLiteral("deadDiff"), QStringLiteral("35")},
                            {QStringLiteral("deadRadiusPercent"), QStringLiteral("82")},
                            {QStringLiteral("saveCapture"), QStringLiteral("1")},
                            {QStringLiteral("roi"), QString()},
                            {QStringLiteral("reuseCircleRoi"), QStringLiteral("0")}};
            if (usbCam) {
                map.insert(QStringLiteral("cameraIndex"), QStringLiteral("0"));
                map.insert(QStringLiteral("cameraName"), QString());
            } else {
                map.insert(QStringLiteral("cameraIp"), QStringLiteral("169.254.64.10"));
                map.insert(QStringLiteral("cameraSerial"), QString());
            }
            if (cmdName == QLatin1String("ScreenDisplayAnomalyCheck")
                || cmdName == QLatin1String("ScreenCameraCalibration"))
                map.insert(QStringLiteral("referencePath"), QString());
            if (cmdName == QLatin1String("ScreenCameraCalibration")) {
                map.remove(QStringLiteral("expectedColor"));
                map.remove(QStringLiteral("deadDiff"));
                map.remove(QStringLiteral("deadRadiusPercent"));
            }
            return map;
        }
        if (cmdName == QLatin1String("LedOnOffCheck")) {
            QVariantMap map{{QStringLiteral("warmupMs"), QStringLiteral("450")},
                            {QStringLiteral("saveCapture"), QStringLiteral("1")},
                            {QStringLiteral("ledCount"), QStringLiteral("4")},
                            {QStringLiteral("minLuma"), QStringLiteral("80")},
                            {QStringLiteral("mode1"), QStringLiteral("on")},
                            {QStringLiteral("mode2"), QStringLiteral("on")},
                            {QStringLiteral("mode3"), QStringLiteral("blink")},
                            {QStringLiteral("mode4"), QStringLiteral("blink")},
                            {QStringLiteral("sampleDurationMs"), QStringLiteral("1500")},
                            {QStringLiteral("sampleIntervalMs"), QStringLiteral("80")}};
            if (usbCam) {
                map.insert(QStringLiteral("cameraIndex"), QStringLiteral("0"));
                map.insert(QStringLiteral("cameraName"), QString());
            } else {
                map.insert(QStringLiteral("cameraIp"), QStringLiteral("169.254.64.10"));
                map.insert(QStringLiteral("cameraSerial"), QString());
            }
            return map;
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Fixture
        && FixturePcbaCmdCatalog::fixtureProtocolFromIni(device) == TestCaseFixtureProtocol::VesLight) {
        if (cmdName == QLatin1String("SetBrightness"))
            return QVariantMap{{QStringLiteral("brightness"), QStringLiteral("22")}};
        return {};
    }
    if (channel == TestCaseSendChannel::Software
        || (channel == TestCaseSendChannel::Fixture
            && FixturePcbaCmdCatalog::fixtureProtocolFromIni(device) == TestCaseFixtureProtocol::AudioExpert)) {
        if (cmdName == QLatin1String("RunTest")) {
            return QVariantMap{{QStringLiteral("host"), QStringLiteral("127.0.0.1")},
                               {QStringLiteral("port"), QString()},
                               {QStringLiteral("command"), QStringLiteral("$test#")},
                               {QStringLiteral("connectTimeoutMs"), QStringLiteral("3000")},
                               {QStringLiteral("readTimeoutMs"), QStringLiteral("120000")},
                               {QStringLiteral("requireOk"), QStringLiteral("1")}};
        }
        return {};
    }
    if (channel == TestCaseSendChannel::Cloud && cmdName == QLatin1String("Login")) {
        // 三元组云端登录：提供 baseUrl/userName/password 字段；显示顺序见 sendParamPreferredOrder（账号在密码上方）
        return QVariantMap{{QStringLiteral("baseUrl"), QString()},
                           {QStringLiteral("userName"), QString()},
                           {QStringLiteral("password"), QString()}};
    }
    if (channel == TestCaseSendChannel::Cloud && cmdName == QLatin1String("ApplyTupleByMac")) {
        return QVariantMap{{QStringLiteral("mac"), QStringLiteral("$MAC")},
                           {QStringLiteral("sku"), QString()},
                           {QStringLiteral("position"), QStringLiteral("F")}};
    }
    if (channel == TestCaseSendChannel::Cloud && cmdName == QLatin1String("DebugUpdateMacStatus")) {
        return QVariantMap{{QStringLiteral("mac"), QStringLiteral("$MAC")},
                           {QStringLiteral("status"), QStringLiteral("2")},
                           {QStringLiteral("sn"), QStringLiteral("$SN")}};
    }
    if (channel == TestCaseSendChannel::Cloud && cmdName == QLatin1String("ResetStatus")) {
        return QVariantMap{{QStringLiteral("mac"), QStringLiteral("$MAC")},
                           {QStringLiteral("status"), QStringLiteral("1")}};
    }
    return {};
}

/** 字符串型指令默认值（切换指令时填入参数表）。 */
QString sendParamDefaultStringForCmd(TestCaseSendChannel channel, const QString& cmdName) {
    if (channel == TestCaseSendChannel::Dongle) {
        if (cmdName == QLatin1String("BleScanConnect") || cmdName == QLatin1String("BleDirectConnect")
            || cmdName == QLatin1String("BleOtaConnect") || cmdName == QLatin1String("BleAppConnect")
            || cmdName == QLatin1String("BleMainConnect"))
            return QStringLiteral("$MAC");
    }
    return {};
}

void fillScreenInspectColorCombo(QComboBox* combo, const QString& currentText) {
    if (!combo)
        return;
    combo->clear();
    // userData 用字符串，避免 findData(-1) 在 Qt 里匹配失败、保存仍落成 -1
    combo->addItem(QStringLiteral("不判断（自动识别）"), QStringLiteral("-1"));
    combo->addItem(QStringLiteral("蓝"), QStringLiteral("0"));
    combo->addItem(QStringLiteral("绿"), QStringLiteral("1"));
    combo->addItem(QStringLiteral("红"), QStringLiteral("2"));
    combo->addItem(QStringLiteral("白"), QStringLiteral("3"));
    combo->addItem(QStringLiteral("黑"), QStringLiteral("4"));
    combo->addItem(QStringLiteral("灰"), QStringLiteral("5"));
    bool ok = false;
    const int idx = ScreenInspectAnalyzer::parseColorIndex(
        currentText.trimmed().isEmpty() ? QStringLiteral("-1") : currentText, &ok);
    const QString want = QString::number(ok ? idx : -1);
    int comboIdx = combo->findData(want, Qt::UserRole, Qt::MatchExactly);
    if (comboIdx < 0) {
        for (int i = 0; i < combo->count(); ++i) {
            if (combo->itemData(i).toString() == want) {
                comboIdx = i;
                break;
            }
        }
    }
    combo->setCurrentIndex(comboIdx >= 0 ? comboIdx : 0);
}

/** 期望纯色：中文下拉，保存为 -1/0~5（ini 稳定）；界面仍显示中文。 */
void installScreenInspectExpectedColorCombo(QTableWidget* table) {
    if (!table)
        return;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem* nameItem = table->item(r, 0);
        if (!nameItem)
            continue;
        const QString key = nameItem->data(SendParamKeyRole).toString().trimmed();
        if (key != QLatin1String("expectedColor"))
            continue;
        QTableWidgetItem* valItem = table->item(r, 1);
        const QString cur = valItem ? valItem->text().trimmed() : QString();
        auto* combo = new QComboBox(table);
        fillScreenInspectColorCombo(combo, cur);
        combo->setToolTip(QStringLiteral("选「蓝」等文字即可，无需记数字。"));
        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), table, [table, r](int) {
            QComboBox* cb = qobject_cast<QComboBox*>(table->cellWidget(r, 1));
            QTableWidgetItem* item = table->item(r, 1);
            if (!cb || !item)
                return;
            item->setText(cb->currentData().toString());
            item->setData(SendParamTouchedRole, true);
            item->setData(SendParamPlaceholderRole, false);
        });
        table->setCellWidget(r, 1, combo);
        if (valItem) {
            valItem->setText(combo->currentData().toString());
            valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
            valItem->setData(SendParamTouchedRole, true);
            valItem->setData(SendParamPlaceholderRole, false);
        }
    }
}

void fillWhichSnCombo(QComboBox* combo, const QString& currentText) {
    if (!combo)
        return;
    combo->clear();
    // 产线实际用到的 FacDevInfoType（板级SN=2 未用，不下发）
    combo->addItem(QStringLiteral("整机SN"), QStringLiteral("1"));
    combo->addItem(QStringLiteral("deviceName（deviceID）"), QStringLiteral("6"));
    combo->addItem(QStringLiteral("productKey（productID）"), QStringLiteral("7"));
    const QString want = currentText.trimmed().isEmpty() ? QStringLiteral("1") : currentText.trimmed();
    int comboIdx = combo->findData(want, Qt::UserRole, Qt::MatchExactly);
    if (comboIdx < 0) {
        for (int i = 0; i < combo->count(); ++i) {
            const QString text = combo->itemText(i);
            if (combo->itemData(i).toString() == want || text == want
                || (want == QLatin1String("deviceName") && text.startsWith(QLatin1String("deviceName")))
                || (want == QLatin1String("productKey") && text.startsWith(QLatin1String("productKey")))
                || want == QLatin1String("整机SN")) {
                comboIdx = i;
                break;
            }
        }
    }
    // 旧步骤若仍是板级SN=2，保留数字显示在首项旁不可选——落到整机以免静默改错；仅选已知三项
    combo->setCurrentIndex(comboIdx >= 0 ? comboIdx : 0);
}

/** SN 类型：中文/英文名下拉，保存仍为 1/2/6/7。 */
void installWhichSnCombo(QTableWidget* table) {
    if (!table)
        return;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem* nameItem = table->item(r, 0);
        if (!nameItem)
            continue;
        const QString key = nameItem->data(SendParamKeyRole).toString().trimmed();
        if (key != QLatin1String("which_sn"))
            continue;
        QTableWidgetItem* valItem = table->item(r, 1);
        const QString cur = valItem ? valItem->text().trimmed() : QString();
        auto* combo = new QComboBox(table);
        fillWhichSnCombo(combo, cur);
        combo->setToolTip(QStringLiteral("整机SN / deviceName / productKey；保存仍为 1/6/7。"));
        QObject::connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), table, [table, r](int) {
            QComboBox* cb = qobject_cast<QComboBox*>(table->cellWidget(r, 1));
            QTableWidgetItem* item = table->item(r, 1);
            if (!cb || !item)
                return;
            item->setText(cb->currentData().toString());
            item->setData(SendParamTouchedRole, true);
            item->setData(SendParamPlaceholderRole, false);
        });
        table->setCellWidget(r, 1, combo);
        if (valItem) {
            valItem->setText(combo->currentData().toString());
            valItem->setFlags(valItem->flags() & ~Qt::ItemIsEditable);
            valItem->setData(SendParamTouchedRole, true);
            valItem->setData(SendParamPlaceholderRole, false);
        }
    }
}

void installSendParamValueCombos(QTableWidget* table) {
    installScreenInspectExpectedColorCombo(table);
    installWhichSnCombo(table);
}

void applySendParamTableWithTemplate(QTableWidget* table, TestCaseSendChannel channel, const QString& device,
                                     const QString& cmdName, const QVariantMap& userMap,
                                     TestCaseSendAction action = TestCaseSendAction::Set) {
    if (!table)
        return;
    QVariantMap normalizedUser = userMap;
    // Sn：sn / value 等价，只保留 sn，避免参数表出现两行内容
    if (cmdName == QLatin1String("Sn")) {
        if (normalizedUser.contains(QStringLiteral("sn")) && normalizedUser.contains(QStringLiteral("value")))
            normalizedUser.remove(QStringLiteral("value"));
        else if (!normalizedUser.contains(QStringLiteral("sn")) && normalizedUser.contains(QStringLiteral("value"))) {
            normalizedUser.insert(QStringLiteral("sn"), normalizedUser.value(QStringLiteral("value")));
            normalizedUser.remove(QStringLiteral("value"));
        }
        // 读 SN：界面不展示写专用字段（即便旧 ini 里残留）
        if (action == TestCaseSendAction::Get) {
            normalizedUser.remove(QStringLiteral("sn"));
            normalizedUser.remove(QStringLiteral("value"));
            normalizedUser.remove(QStringLiteral("length"));
            normalizedUser.remove(QStringLiteral("snLength"));
        }
    }
    // FacResult：旧 Param_int / Param_value 归一到 done，避免空表或双行
    if (cmdName == QLatin1String("FacResult")) {
        if (!normalizedUser.contains(QStringLiteral("done"))) {
            if (normalizedUser.contains(QStringLiteral("int")))
                normalizedUser.insert(QStringLiteral("done"), normalizedUser.take(QStringLiteral("int")));
            else if (normalizedUser.contains(QStringLiteral("value")))
                normalizedUser.insert(QStringLiteral("done"), normalizedUser.take(QStringLiteral("value")));
        } else {
            normalizedUser.remove(QStringLiteral("int"));
            normalizedUser.remove(QStringLiteral("value"));
        }
    }
    const QStringList preferred = sendParamPreferredOrder(channel, cmdName, action);
    const QVariantMap tmpl = sendParamDefaultMapForCmd(channel, device, cmdName, action);
    if (!tmpl.isEmpty())
        setSendParamTableFromMapWithTemplate(table, normalizedUser, tmpl, preferred);
    else
        setSendParamTableFromMap(table, normalizedUser, preferred);
    installSendParamValueCombos(table);
}

void setSendParamTableFromString(QTableWidget* table, const QString& value) {
    configureSendParamTable(table);
    table->insertRow(0);
    table->setItem(0, 0, makeSendParamNameItem(QStringLiteral("string")));
    table->setItem(0, 1, makeSendParamValueItem(value, false, false));
}

/** 关掉单元格内联编辑器，让 item->text() 拿到正在输入的值。 */
void commitTableInlineEdits(QTableWidget* table) {
    if (!table)
        return;
    if (QTableWidgetItem* item = table->currentItem())
        table->closePersistentEditor(item);
    if (QWidget* fw = QApplication::focusWidget()) {
        if (fw == table || table->isAncestorOf(fw))
            fw->clearFocus();
    }
    table->clearFocus();
}

/** 保存/definition 前提交发送参数表：先落占位符，再关编辑器。 */
void commitSendParamTableEdits(QTableWidget* table) {
    if (!table)
        return;
    for (int r = 0; r < table->rowCount(); ++r) {
        QTableWidgetItem* valItem = table->item(r, 1);
        if (!valItem)
            continue;
        if (table->isPersistentEditorOpen(valItem)) {
            table->closePersistentEditor(valItem);
            // 用户打开编辑框后直接确认（文本未变）时 itemChanged 可能不触发，须在此 materialize
            if (valItem->data(SendParamPlaceholderRole).toBool() && !valItem->text().trimmed().isEmpty())
                materializeSendParamValueItem(valItem);
        }
    }
    commitTableInlineEdits(table);
}

QVariantMap readSendParamMapFromTable(const QTableWidget* table) {
    QVariantMap map;
    if (!table || table->columnCount() < 2)
        return map;
    for (int r = 0; r < table->rowCount(); ++r) {
        const QTableWidgetItem* nameItem = table->item(r, 0);
        const QTableWidgetItem* valItem = table->item(r, 1);
        QString name;
        if (nameItem) {
            name = nameItem->data(SendParamKeyRole).toString().trimmed();
            if (name.isEmpty())
                name = nameItem->text().trimmed();
        }
        if (name.isEmpty())
            continue;

        QString val = valItem ? valItem->text().trimmed() : QString();
        if (QComboBox* combo = qobject_cast<QComboBox*>(table->cellWidget(r, 1))) {
            if (name == QLatin1String("expectedColor")) {
                bool ok = false;
                int colorIdx = ScreenInspectAnalyzer::parseColorIndex(combo->currentData().toString(), &ok);
                if (!ok)
                    colorIdx = ScreenInspectAnalyzer::parseColorIndex(combo->currentText(), &ok);
                // 写入数字，避免中文 ini 编码或 findData(-1) 导致重开变回 -1
                val = QString::number(ok ? colorIdx : -1);
            } else {
                // which_sn 等：界面显示名称，ini 仍写 userData 数字串
                val = combo->currentData().toString();
                if (val.isEmpty())
                    val = combo->currentText().trimmed();
            }
            map.insert(name, val);
            continue;
        }
        const bool explicitEmptySentinel = isSendParamExplicitEmptySentinel(val);
        if (explicitEmptySentinel)
            val.clear();

        const bool fromSavedIni = nameItem && nameItem->data(SendParamSavedRole).toBool();
        const bool touched = valItem && valItem->data(SendParamTouchedRole).toBool();
        const bool placeholder = valItem && valItem->data(SendParamPlaceholderRole).toBool();

        // 未改动的灰色模板占位：保存时不写入 ini
        if (placeholder && !touched && !explicitEmptySentinel)
            continue;

        if (val.isEmpty()) {
            if (fromSavedIni || touched || explicitEmptySentinel)
                map.insert(name, QString());
            continue;
        }
        map.insert(name, val);
    }
    return map;
}

QString readSendParamStringFromTable(const QTableWidget* table) {
    if (!table || table->rowCount() <= 0)
        return {};
    const int col = table->columnCount() >= 2 ? 1 : 0;
    const QTableWidgetItem* item = table->item(0, col);
    return item ? item->text().trimmed() : QString();
}

bool isFixtureMachineIndexPlaceholder(const QVariant& param) {
    if (param.userType() == QMetaType::QString) {
        const QString s = param.toString().trimmed();
        return s.isEmpty() || s.compare(QStringLiteral("$INDEX"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${INDEX}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$SLOT"), Qt::CaseInsensitive) == 0;
    }
    return !param.isValid() || param.toInt() == 0;
}

int comboIndexByData(const QComboBox* box, const QString& data) {
    for (int i = 0; i < box->count(); ++i) {
        if (box->itemData(i).toString() == data)
            return i;
    }
    return -1;
}

QString comboData(const QComboBox* box) {
    return box->currentData().toString();
}

void fillActionCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("设置"), QStringLiteral("Set"));
    box->addItem(QStringLiteral("读取"), QStringLiteral("Get"));
}

void fillSendChannelCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("产品蓝牙通信"), QStringLiteral("Product"));
    box->addItem(QStringLiteral("产品串口通信"), QStringLiteral("ProductSerial"));
    box->addItem(QStringLiteral("Dongle通信"), QStringLiteral("Dongle"));
    box->addItem(QStringLiteral("云端交互"), QStringLiteral("Cloud"));
    box->addItem(QStringLiteral("治具通信"), QStringLiteral("Fixture"));
    box->addItem(QStringLiteral("软件通信"), QStringLiteral("Software"));
    box->addItem(QStringLiteral("Modbus通信"), QStringLiteral("Modbus"));
    box->addItem(QStringLiteral("SCPI通信"), QStringLiteral("Scpi"));
}

void fillProductProtocolCombo(QComboBox* box) {
    box->clear();
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qfctp),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qfctp));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qpb),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qpb));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qroot),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qroot));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qroot2),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qroot2));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::Qaiot),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::Qaiot));
    box->addItem(DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol::QaiotV2),
                 DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol::QaiotV2));
}

void fillFixtureProtocolCombo(QComboBox* box) {
    box->clear();
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Pcba),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Pcba));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Asd9026a),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Asd9026a));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Xwd),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Xwd));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::JieliBtBox),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::JieliBtBox));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::UsbCamera),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::UsbCamera));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::GigECamera),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::GigECamera));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::VesLight),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::VesLight));
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::Universal),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Universal));
}

void fillSoftwareProtocolCombo(QComboBox* box) {
    box->clear();
    box->addItem(FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol::AudioExpert),
                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::AudioExpert));
}

void fillProtocolComboForChannel(QComboBox* box, TestCaseSendChannel channel) {
    box->clear();
    if (channel == TestCaseSendChannel::Fixture) {
        fillFixtureProtocolCombo(box);
    } else if (channel == TestCaseSendChannel::Software) {
        fillSoftwareProtocolCombo(box);
    } else if (channel == TestCaseSendChannel::Modbus) {
        for (const QString& dev : ModbusPeriphCmdCatalog::allDeviceKeys()) {
            box->addItem(ModbusDeviceCatalog::deviceRouteUiLabel(ModbusDeviceCatalog::deviceRouteFromIni(dev)), dev);
        }
    } else if (channel == TestCaseSendChannel::Scpi) {
        for (const QString& dev : ScpiPeriphCmdCatalog::allDeviceKeys()) {
            box->addItem(ScpiPeriphCmdCatalog::deviceUiLabel(ScpiPeriphCmdCatalog::deviceFromIni(dev)), dev);
        }
    } else {
        fillProductProtocolCombo(box);
    }
}

TestCaseProductProtocol productProtocolFromComboData(const QString& data) {
    return DeviceCmdCatalog::productProtocolFromIni(data);
}

TestCaseSendAction sendActionFromComboData(const QString& data) {
    return data.compare(QStringLiteral("Get"), Qt::CaseInsensitive) == 0 ? TestCaseSendAction::Get
                                                                         : TestCaseSendAction::Set;
}

void fillDeviceCmdCombo(QComboBox* box, TestCaseSendChannel channel, TestCaseSendAction action,
                        const QString& device, const QString& keepCmdIfMissing = QString()) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    if (channel == TestCaseSendChannel::Dongle) {
        const QStringList names = DongleCmdCatalog::catalog().allCmdNames(action);
        items.reserve(names.size());
        for (const QString& name : names)
            items.append({DongleCmdCatalog::catalog().cmdUiLabel(name), name});
    } else if (channel == TestCaseSendChannel::Cloud) {
        const QStringList names = TupleCmdCatalog::catalog().allCmdNames(action);
        items.reserve(names.size());
        for (const QString& name : names)
            items.append({TupleCmdCatalog::catalog().cmdUiLabel(name), name});
    } else if (channel == TestCaseSendChannel::Fixture) {
        const TestCaseFixtureProtocol proto = FixturePcbaCmdCatalog::fixtureProtocolFromIni(device);
        if (proto == TestCaseFixtureProtocol::Asd9026a) {
            const QStringList names = Asd9026aCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({Asd9026aCmdCatalog::catalog().cmdUiLabel(name), name});
        } else if (proto == TestCaseFixtureProtocol::Xwd) {
            const QStringList names = XwdRawFixtureCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({XwdRawFixtureCmdCatalog::catalog().cmdUiLabel(name), name});
        } else if (proto == TestCaseFixtureProtocol::Universal) {
            const QStringList names = UniversalFixtureCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({UniversalFixtureCmdCatalog::catalog().cmdUiLabel(name), name});
        } else if (proto == TestCaseFixtureProtocol::JieliBtBox) {
            const QStringList names = JieliBtBoxCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({JieliBtBoxCmdCatalog::catalog().cmdUiLabel(name), name});
        } else if (FixturePcbaCmdCatalog::isScreenCameraFixture(proto)) {
            const QStringList names = UsbCameraCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({UsbCameraCmdCatalog::catalog().cmdUiLabel(name), name});
        } else if (proto == TestCaseFixtureProtocol::VesLight) {
            const QStringList names = VesLightCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({VesLightCmdCatalog::catalog().cmdUiLabel(name), name});
        } else {
            const QStringList names = FixturePcbaCmdCatalog::catalog().allCmdNames(action);
            items.reserve(names.size());
            for (const QString& name : names)
                items.append({FixturePcbaCmdCatalog::catalog().cmdUiLabel(name), name});
        }
    } else if (channel == TestCaseSendChannel::Software) {
        const QStringList names = AudioExpertCmdCatalog::catalog().allCmdNames(action);
        items.reserve(names.size());
        for (const QString& name : names)
            items.append({AudioExpertCmdCatalog::catalog().cmdUiLabel(name), name});
    } else if (channel == TestCaseSendChannel::Modbus) {
        ModbusDeviceRoute devRoute = ModbusDeviceCatalog::deviceRouteFromIni(device);
        const QStringList names = ModbusPeriphCmdCatalog::allCmdNames(devRoute, action);
        items.reserve(names.size());
        for (const QString& name : names) {
            items.append({ModbusPeriphCmdCatalog::cmdUiLabel(devRoute, name), name});
        }
    } else if (channel == TestCaseSendChannel::Scpi) {
        ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(device);
        const QStringList names = ScpiPeriphCmdCatalog::allCmdNames(devRoute, action);
        items.reserve(names.size());
        for (const QString& name : names) {
            items.append({ScpiPeriphCmdCatalog::cmdUiLabel(devRoute, name), name});
        }
    } else if (channel == TestCaseSendChannel::ProductSerial) {
        const QStringList names = ProductSerialCmdCatalog::catalog().allCmdNames(action);
        items.reserve(names.size());
        for (const QString& name : names)
            items.append({ProductSerialCmdCatalog::catalog().cmdUiLabel(name), name});
    } else {
        // 产品通道：按下拉当前产品协议过滤，避免 Qroot2 工站看到整表 FCTP/Root 指令
        const TestCaseProductProtocol productProto = DeviceCmdCatalog::productProtocolFromIni(device);
        const QStringList names = DeviceCmdCatalog::allCmdNames(action, productProto);
        items.reserve(names.size());
        for (const QString& name : names)
            items.append({DeviceCmdCatalog::catalog().cmdUiLabel(name), name});
    }
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    if ((channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        && !keepCmdIfMissing.isEmpty()) {
        bool found = false;
        for (const auto& item : items) {
            if (item.second == keepCmdIfMissing) {
                found = true;
                break;
            }
        }
        if (!found) {
            QString label;
            if (channel == TestCaseSendChannel::ProductSerial)
                label = ProductSerialCmdCatalog::catalog().cmdUiLabel(keepCmdIfMissing);
            else
                label = DeviceCmdCatalog::catalog().cmdUiLabel(keepCmdIfMissing);
            label += QStringLiteral("（未登记/非当前协议）");
            items.prepend({label, keepCmdIfMissing});
        }
    }
    for (const auto& item : items)
        box->addItem(item.first, item.second);
    // 产品通道首项留空：PromptOnly 等上报时 DeviceCmd 为空可选中，避免误存成下拉第一项
    if (channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial)
        box->insertItem(0, QStringLiteral("（不发送）"), QString());
}

TestCaseSendChannel sendChannelFromComboData(const QString& data) {
    if (data.compare(QStringLiteral("Dongle"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Dongle;
    if (data.compare(QStringLiteral("Cloud"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Cloud;
    if (data.compare(QStringLiteral("ProductSerial"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::ProductSerial;
    if (data.compare(QStringLiteral("Fixture"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Fixture;
    if (data.compare(QStringLiteral("Software"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Software;
    if (data.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Modbus;
    if (data.compare(QStringLiteral("Scpi"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Scpi;
    return TestCaseSendChannel::Product;
}

QString sendChannelComboData(TestCaseSendChannel channel) {
    if (channel == TestCaseSendChannel::Dongle)
        return QStringLiteral("Dongle");
    if (channel == TestCaseSendChannel::Cloud)
        return QStringLiteral("Cloud");
    if (channel == TestCaseSendChannel::ProductSerial)
        return QStringLiteral("ProductSerial");
    if (channel == TestCaseSendChannel::Fixture)
        return QStringLiteral("Fixture");
    if (channel == TestCaseSendChannel::Software)
        return QStringLiteral("Software");
    if (channel == TestCaseSendChannel::Modbus)
        return QStringLiteral("Modbus");
    if (channel == TestCaseSendChannel::Scpi)
        return QStringLiteral("Scpi");
    return QStringLiteral("Product");
}

enum class SendCmdParamKind { None,
                              Int,
                              UInt,
                              JsonMap,
                              String };

struct SendCmdParamUi {
    bool valid = false;
    SendCmdParamKind kind = SendCmdParamKind::None;
    QString hint;
};

SendCmdParamKind sendParamUiKindFromSchema(DeviceCmdParamKind kind) {
    switch (kind) {
    case DeviceCmdParamKind::None:
        return SendCmdParamKind::None;
    case DeviceCmdParamKind::Int:
        return SendCmdParamKind::Int;
    case DeviceCmdParamKind::UInt:
        return SendCmdParamKind::UInt;
    case DeviceCmdParamKind::String:
        return SendCmdParamKind::String;
    case DeviceCmdParamKind::JsonMap:
        return SendCmdParamKind::JsonMap;
    }
    return SendCmdParamKind::None;
}

QString sendParamProtocolContext(TestCaseSendChannel channel, const QString& protocolComboData) {
    if (channel == TestCaseSendChannel::Fixture || channel == TestCaseSendChannel::Software
        || channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi)
        return protocolComboData;
    return QString();
}

SendCmdParamUi sendCmdParamUiForName(const QString& name, TestCaseSendChannel channel, const QString& device = QString()) {
    SendCmdParamUi out;
    if (channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (cmdEnumFromName(DongleCmdCatalog::catalog(), name, dongleCmd)) {
            DeviceCmdParamSchema schema;
            if (DongleCmdCatalog::catalog().paramSchemaFor(static_cast<int>(dongleCmd), schema)) {
                out.valid = true;
                out.hint = DongleCmdCatalog::catalog().paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (cmdEnumFromName(TupleCmdCatalog::catalog(), name, tupleCmd)) {
            DeviceCmdParamSchema schema;
            if (TupleCmdCatalog::catalog().paramSchemaFor(static_cast<int>(tupleCmd), schema)) {
                out.valid = true;
                out.hint = TupleCmdCatalog::catalog().paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Software) {
        AudioExpertCmd aeCmd;
        if (cmdEnumFromName(AudioExpertCmdCatalog::catalog(), name, aeCmd)) {
            DeviceCmdParamSchema schema;
            if (AudioExpertCmdCatalog::catalog().paramSchemaFor(static_cast<int>(aeCmd), schema)) {
                out.valid = true;
                out.hint = AudioExpertCmdCatalog::catalog().paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Fixture) {
        const TestCaseFixtureProtocol proto = FixturePcbaCmdCatalog::fixtureProtocolFromIni(device);
        if (proto == TestCaseFixtureProtocol::Asd9026a) {
            Asd9026aCmd asdCmd;
            if (cmdEnumFromName(Asd9026aCmdCatalog::catalog(), name, asdCmd)) {
                DeviceCmdParamSchema schema;
                if (Asd9026aCmdCatalog::catalog().paramSchemaFor(static_cast<int>(asdCmd), schema)) {
                    out.valid = true;
                    out.hint = Asd9026aCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::Xwd) {
            XwdRawFixtureCmd xwdCmd;
            if (cmdEnumFromName(XwdRawFixtureCmdCatalog::catalog(), name, xwdCmd)) {
                DeviceCmdParamSchema schema;
                if (XwdRawFixtureCmdCatalog::catalog().paramSchemaFor(static_cast<int>(xwdCmd), schema)) {
                    out.valid = true;
                    out.hint = XwdRawFixtureCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::Universal) {
            UniversalFixtureCmd uniCmd;
            if (cmdEnumFromName(UniversalFixtureCmdCatalog::catalog(), name, uniCmd)) {
                DeviceCmdParamSchema schema;
                if (UniversalFixtureCmdCatalog::catalog().paramSchemaFor(static_cast<int>(uniCmd), schema)) {
                    out.valid = true;
                    out.hint = UniversalFixtureCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::JieliBtBox) {
            JieliBtBoxCmd jieliCmd;
            if (cmdEnumFromName(JieliBtBoxCmdCatalog::catalog(), name, jieliCmd)) {
                DeviceCmdParamSchema schema;
                if (JieliBtBoxCmdCatalog::catalog().paramSchemaFor(static_cast<int>(jieliCmd), schema)) {
                    out.valid = true;
                    out.hint = JieliBtBoxCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (FixturePcbaCmdCatalog::isScreenCameraFixture(proto)) {
            UsbCameraCmd camCmd;
            if (cmdEnumFromName(UsbCameraCmdCatalog::catalog(), name, camCmd)) {
                DeviceCmdParamSchema schema;
                if (UsbCameraCmdCatalog::catalog().paramSchemaFor(static_cast<int>(camCmd), schema)) {
                    out.valid = true;
                    out.hint = UsbCameraCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::VesLight) {
            VesLightCmd vesCmd;
            if (cmdEnumFromName(VesLightCmdCatalog::catalog(), name, vesCmd)) {
                DeviceCmdParamSchema schema;
                if (VesLightCmdCatalog::catalog().paramSchemaFor(static_cast<int>(vesCmd), schema)) {
                    out.valid = true;
                    out.hint = VesLightCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        if (proto == TestCaseFixtureProtocol::AudioExpert) {
            AudioExpertCmd aeCmd;
            if (cmdEnumFromName(AudioExpertCmdCatalog::catalog(), name, aeCmd)) {
                DeviceCmdParamSchema schema;
                if (AudioExpertCmdCatalog::catalog().paramSchemaFor(static_cast<int>(aeCmd), schema)) {
                    out.valid = true;
                    out.hint = AudioExpertCmdCatalog::catalog().paramUiHint(name);
                    out.kind = sendParamUiKindFromSchema(schema.kind);
                }
            }
            return out;
        }
        FixturePcbaCmd fixtureCmd;
        if (cmdEnumFromName(FixturePcbaCmdCatalog::catalog(), name, fixtureCmd)) {
            DeviceCmdParamSchema schema;
            if (FixturePcbaCmdCatalog::catalog().paramSchemaFor(static_cast<int>(fixtureCmd), schema)) {
                out.valid = true;
                out.hint = FixturePcbaCmdCatalog::catalog().paramUiHint(name);
                out.kind = sendParamUiKindFromSchema(schema.kind);
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Modbus) {
        ModbusDeviceRoute devRoute = ModbusDeviceCatalog::deviceRouteFromIni(device);
        const auto* row = ModbusCmdManifest::findByDeviceAndName(devRoute, name);
        if (row) {
            out.valid = true;
            out.hint = ModbusPeriphCmdCatalog::paramUiHint(devRoute, name);
            if (devRoute == ModbusDeviceRoute::InovanceH5uTcp || devRoute == ModbusDeviceRoute::GcSeriesTcp) {
                if (name == QLatin1String("ReadCoil")) {
                    out.kind = SendCmdParamKind::Int;
                } else if (name == QLatin1String("WriteCoil") ||
                           name == QLatin1String("ReadCoils") ||
                           name == QLatin1String("WaitCoilTrue") ||
                           name == QLatin1String("WaitCoilFalse") ||
                           name == QLatin1String("Connect")) {
                    out.kind = SendCmdParamKind::JsonMap;
                } else {
                    out.kind = SendCmdParamKind::None;
                }
            } else if (devRoute == ModbusDeviceRoute::MultiTempLoggerRtu) {
                // 开放报文与读温通道均走 Param_* map
                out.kind = SendCmdParamKind::JsonMap;
            } else if (devRoute == ModbusDeviceRoute::XinjiePlcRtu) {
                if (name == QLatin1String("Connect") || name == QLatin1String("WriteCoil")
                    || name == QLatin1String("ReadCoils") || name == QLatin1String("WriteRegister")
                    || name == QLatin1String("ReadHoldingRegisters")
                    || name == QLatin1String("ReadDiscreteInputs")) {
                    out.kind = SendCmdParamKind::JsonMap;
                } else {
                    out.kind = SendCmdParamKind::None;
                }
            } else {
                out.kind = SendCmdParamKind::None;
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Scpi) {
        ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(device);
        const auto* row = ScpiCmdManifest::findByDeviceAndName(devRoute, name);
        if (row) {
            out.valid = true;
            out.hint = ScpiPeriphCmdCatalog::paramUiHint(devRoute, name);
            if (isVisaProgrammablePowerRoute(devRoute)
                && (name == QLatin1String("ConfigureProgrammablePower")
                    || name == QLatin1String("ReadProgrammableVoltage")
                    || name == QLatin1String("ReadProgrammableCurrent")
                    || name == QLatin1String("InitializeProgrammablePower"))) {
                out.kind = SendCmdParamKind::JsonMap;
            } else if (name == QLatin1String("ProgrammablePowerOutput") ||
                       name == QLatin1String("ArbCycles")) {
                out.kind = SendCmdParamKind::Int;
            } else if (name == QLatin1String("ArbFile") ||
                       name == QLatin1String("SendRawLine") ||
                       name == QLatin1String("WriteLine") ||
                       name == QLatin1String("QueryLine") ||
                       name == QLatin1String("TxLevelDbm") ||
                       name == QLatin1String("FrequencyMhz")) {
                out.kind = SendCmdParamKind::String;
            } else {
                out.kind = SendCmdParamKind::None;
            }
        }
        return out;
    }
    DeviceCmd cmd;
    if (cmdEnumFromName(DeviceCmdCatalog::catalog(), name, cmd)) {
        DeviceCmdParamSchema schema;
        if (DeviceCmdCatalog::catalog().paramSchemaFor(static_cast<int>(cmd), schema)) {
            out.valid = true;
            out.hint = DeviceCmdCatalog::catalog().paramUiHint(name);
            out.kind = sendParamUiKindFromSchema(schema.kind);
        }
    }
    return out;
}

void applySendParamHintToUi(const SendCmdParamUi& uiSchema, bool hasParam, QLabel* hintLabel, QTableWidget* paramTable,
                            QSpinBox* spinBox, QWidget* addRowBtn, QWidget* removeRowBtn, QWidget* restoreBtn,
                            bool hasDefaultTemplate) {
    if (!hintLabel)
        return;
    QString hintText = uiSchema.hint;
    const bool namedMap = uiSchema.valid && uiSchema.kind == SendCmdParamKind::JsonMap;
    if (namedMap) {
        const QString saveNote =
            QStringLiteral("灰色斜体为参考默认；保存工站步骤时会将非空参考默认一并写入 ini。"
                           "改过后留空保存=显式空值（写入 Param_键=）；也可填「-」表示显式空值。"
                           "误删字段可点「恢复默认参数表」。");
        if (!hintText.isEmpty())
            hintText += QStringLiteral("\n") + saveNote;
        else
            hintText = saveNote;
    }
    hintLabel->setText(hintText);
    hintLabel->setVisible(hasParam && uiSchema.valid && !hintText.isEmpty());
    hintLabel->setToolTip(hintText.isEmpty() ? QString() : hintText);

    if (addRowBtn)
        addRowBtn->setVisible(namedMap);
    if (removeRowBtn)
        removeRowBtn->setVisible(namedMap);
    if (restoreBtn)
        restoreBtn->setVisible(namedMap && hasDefaultTemplate);

    if (paramTable) {
        if (uiSchema.valid && !uiSchema.hint.isEmpty())
            paramTable->setToolTip(uiSchema.hint);
        else if (namedMap)
            paramTable->setToolTip(QStringLiteral("灰色斜体=参考默认；保存工站步骤时非空默认会写入 ini；改过后空=显式空值。"));
        else
            paramTable->setToolTip(QString());
    }
    if (spinBox) {
        if (uiSchema.valid && uiSchema.kind == SendCmdParamKind::Int)
            spinBox->setToolTip(uiSchema.hint);
        else
            spinBox->setToolTip(QString());
    }
}

void applyHookSendParamUi(Ui::TestCaseEditDialog* ui, const QString& hookId, const QVariantMap& userMap = {}) {
    if (!ui || !hookUsesSendParamUi(hookId))
        return;
    const QVariantMap tmpl = hookSendParamDefaultMap(hookId);
    const QVariantMap current =
        userMap.isEmpty() ? readSendParamMapFromTable(ui->tableWidget_sendParam) : userMap;
    // 严格过滤：Hook 参数表仅保留当前 Hook 模板声明的参数，彻底清除其他步骤遗留的电流、电压等无关参数
    QVariantMap filteredUserMap;
    if (!tmpl.isEmpty()) {
        for (auto it = current.cbegin(); it != current.cend(); ++it) {
            if (tmpl.contains(it.key())) {
                filteredUserMap.insert(it.key(), it.value());
            }
        }
    } else {
        filteredUserMap = current;
    }
    setSendParamTableFromMapWithTemplate(ui->tableWidget_sendParam, filteredUserMap, tmpl);
    ui->stackedWidget_param->setCurrentWidget(ui->page_paramJson);
    ui->label_param->setVisible(true);
    ui->stackedWidget_param->setVisible(true);

    SendCmdParamUi uiSchema;
    uiSchema.valid = true;
    uiSchema.kind = SendCmdParamKind::JsonMap;
    if (hookId == QLatin1String("COUNTDOWN_WAIT")) {
        uiSchema.hint = QStringLiteral("seconds：倒计时秒数（必填，>0）；prompt：弹窗提示文字（可选）");
    } else if (hookId == QLatin1String("SN_PREFIX4_CHECK")) {
        uiSchema.hint = QStringLiteral(
            "期望SN前四位：必填，如 V300/M5SM；比对界面 SN 栏（BYD 须先经 MES 换 SN 并回填）");
    } else if (hookId == QLatin1String("LIGHT_SENSOR_GOLDEN_CALIB")) {
        uiSchema.hint = QStringLiteral(
            "产品协议单点：index 写入下标；golden 金样光感（仅日志）；waitMs/minSamples 采光感取平均并写回读；"
            "采齐后各点 enableFivePointJudge=1 时按已采点做范围+差值卡控（不合格立即 FAIL）。治具亮度请用前一步 VES SetBrightness");
    } else if (hookId == QLatin1String("LIGHT_SENSOR_CALIB_WRITE")) {
        uiSchema.hint = QStringLiteral(
            "仅采光感取平均并写入：index/golden/waitMs/minSamples。流程需先「开启光感上报」再 VES 设亮度；"
            "回读请用 LIGHT_SENSOR_CALIB_READ 步骤");
    } else if (hookId == QLatin1String("LIGHT_SENSOR_CALIB_READ")) {
        uiSchema.hint = QStringLiteral(
            "本点回读：index/verifyWrite；Param_rangeLo/Hi/HiInc 配本点范围；"
            "index>0 写 Param_diffMin（相对上一点，判定>）；"
            "各读取点建议 enableFivePointJudge=1：已采点越界/差值不达标立即 FAIL");
    } else if (hookId == QLatin1String("VES_CH1_SET_BRIGHTNESS")) {
        uiSchema.hint = QStringLiteral(
            "走工位治具串口，协议固定通道 1；Param_brightness：亮度 0~255（默认 22）");
    } else if (hookId == QLatin1String("AIR_PRESSURE_SAMPLE")) {
        uiSchema.hint = QStringLiteral(
            "sampleCount：采样点数；intervalMs：设备采样间隔(ms)；waitMs：收齐上报超时(ms，可留空用步骤超时)；"
            "channel：left=仅左气压，right=仅右气压，both=左右各采 N 点并逐点卡控；"
            "卡控上下限(kPa)在下方「卡控」区勾选启用后配置（勾选左气压并设最小/最大值即可，逐点共用）");
    } else if (hookId.startsWith(QLatin1String("PLC_SYNC_WRITE"))) {
        uiSchema.hint = QStringLiteral(
            "address：PLC线圈地址（如 M10/M11/M20/Y0，支持任意自定义修改）；"
            "pulse：是否脉冲动作（true/false，默认 true 即 写ON->延时->写OFF）；"
            "pulseHoldMs：脉冲保持时间（ms，默认 200）；"
            "syncWaitMs：等待所有工位扫码到齐时间（ms，默认 120000）；"
            "value：写入值（true/false，默认 true）");
    } else if (hookId.startsWith(QLatin1String("PLC_SYNC_READ"))) {
        uiSchema.hint = QStringLiteral(
            "address：PLC监控线圈地址（如 M0/M1/M5/X0，支持任意自定义修改）；"
            "syncWaitMs：等待所有工位扫码到齐时间（ms，默认 120000）；全员到齐后轮询线圈，检测到置 1 放行");
    }
    applySendParamHintToUi(uiSchema, true, ui->label_sendParamHint, ui->tableWidget_sendParam, ui->spinBox_intParam,
                           ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                           ui->pushButton_restoreParamDefaults, !tmpl.isEmpty());
}

void applySendParamToUi(const SendCmdParamUi& uiSchema, const QVariant& param, QWidget* pageNone, QWidget* pageInt,
                        QWidget* pageJson, QStackedWidget* stack, QSpinBox* spinBox, QTableWidget* paramTable) {
    if (!uiSchema.valid || uiSchema.kind == SendCmdParamKind::None) {
        stack->setCurrentWidget(pageNone);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::Int || uiSchema.kind == SendCmdParamKind::UInt) {
        stack->setCurrentWidget(pageInt);
        int intVal = param.toInt();
        if (param.canConvert<QVariantMap>()) {
            const QVariantMap map = param.toMap();
            if (map.contains(QStringLiteral("int")))
                intVal = map.value(QStringLiteral("int")).toInt();
            else if (map.contains(QStringLiteral("value")))
                intVal = map.value(QStringLiteral("value")).toInt();
            else if (map.size() == 1)
                intVal = map.constBegin().value().toInt();
        }
        spinBox->setValue(intVal);
        return;
    }
    stack->setCurrentWidget(pageJson);
    if (uiSchema.kind == SendCmdParamKind::String) {
        // overlay 可能把 Param_string 读成 {string:xxx}，直接 toString() 会是空
        QString text;
        if (param.canConvert<QVariantMap>()) {
            const QVariantMap map = param.toMap();
            if (map.contains(QStringLiteral("string")))
                text = map.value(QStringLiteral("string")).toString();
            else if (map.size() == 1)
                text = map.constBegin().value().toString();
            else
                text = param.toString();
        } else {
            text = param.toString();
        }
        setSendParamTableFromString(paramTable, text);
    } else {
        setSendParamTableFromMap(paramTable, sendParamAsJsonMap(param));
    }
}

QVariant readSendParamFromUi(const SendCmdParamUi& uiSchema, QSpinBox* spinBox, QTableWidget* paramTable) {
    switch (uiSchema.kind) {
    case SendCmdParamKind::Int:
    case SendCmdParamKind::UInt:
        return spinBox->value();
    case SendCmdParamKind::String:
        return readSendParamStringFromTable(paramTable);
    case SendCmdParamKind::JsonMap:
        return readSendParamMapFromTable(paramTable);
    default:
        return {};
    }
}

void fillGateReportTypeCombo(QComboBox* box, const QStringList& allowedTypes = {}) {
    box->clear();
    // 只列指令绑定的回包类型；禁止「空绑定 → 全量列表」导致随意选
    if (allowedTypes.isEmpty())
        return;
    for (const GateTypeDescriptor& t : GateRegistry::allTypeDescriptors()) {
        if (!allowedTypes.contains(t.reportType))
            continue;
        box->addItem(t.displayName, t.reportType);
    }
}

void fillGateFieldCombo(QComboBox* box, const QString& reportType) {
    box->clear();
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(reportType, desc))
        return;
    for (const GateFieldDescriptor& f : desc.fields)
        box->addItem(f.displayName, f.field);
}

QString gateOpToTableText(TestCaseGateOp op) {
    switch (op) {
    case TestCaseGateOp::Gt:
        return QStringLiteral("gt");
    case TestCaseGateOp::Lt:
        return QStringLiteral("lt");
    case TestCaseGateOp::Eq:
        return QStringLiteral("eq");
    case TestCaseGateOp::CompareVersions:
        return QStringLiteral("compareVersions");
    default:
        return QStringLiteral("range");
    }
}

TestCaseGateOp gateOpFromTableText(const QString& text) {
    const QString t = text.trimmed();
    if (t == QLatin1String("gt"))
        return TestCaseGateOp::Gt;
    if (t == QLatin1String("lt"))
        return TestCaseGateOp::Lt;
    if (t == QLatin1String("eq"))
        return TestCaseGateOp::Eq;
    if (t == QLatin1String("compareVersions"))
        return TestCaseGateOp::CompareVersions;
    return TestCaseGateOp::Range;
}

void fillGateOpCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("在范围内"), QStringLiteral("range"));
    box->addItem(QStringLiteral("大于"), QStringLiteral("gt"));
    box->addItem(QStringLiteral("小于"), QStringLiteral("lt"));
    box->addItem(QStringLiteral("等于"), QStringLiteral("eq"));
    box->addItem(QStringLiteral("版本比对"), QStringLiteral("compareVersions"));
}

/** 多卡控表格「方式」列的中文下拉；currentData 仍是 range/gt/lt/eq，与 ini Op 同源。 */
QComboBox* gateOpCellCombo(QTableWidget* table, int row) {
    if (!table)
        return nullptr;
    return qobject_cast<QComboBox*>(table->cellWidget(row, 2));
}

void setMultiGateCellEditable(QTableWidgetItem* item, bool editable) {
    if (!item)
        return;
    Qt::ItemFlags flags = item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    if (editable)
        flags |= Qt::ItemIsEditable;
    else
        flags &= ~Qt::ItemIsEditable;
    item->setFlags(flags);
    item->setBackground(editable ? QBrush() : QBrush(QColor(235, 235, 235)));
}

/** 范围：填最小/最大；大于/小于/等于：只填期望值。 */
void updateMultiGateRowInputMode(QTableWidget* table, int row) {
    if (!table || table->columnCount() < 6)
        return;
    QComboBox* opCombo = gateOpCellCombo(table, row);
    if (!opCombo)
        return;
    const QString opKey = opCombo->currentData().toString();
    const bool isRange = (opKey == QLatin1String("range"));
    const bool useExpected = (opKey == QLatin1String("gt") || opKey == QLatin1String("lt")
                              || opKey == QLatin1String("eq") || opKey == QLatin1String("compareVersions"));
    setMultiGateCellEditable(table->item(row, 3), isRange);
    setMultiGateCellEditable(table->item(row, 4), isRange);
    setMultiGateCellEditable(table->item(row, 5), useExpected);
    if (useExpected) {
        QTableWidgetItem* expectedItem = table->item(row, 5);
        if (expectedItem && expectedItem->text().trimmed().isEmpty()) {
            if (QTableWidgetItem* lowItem = table->item(row, 3)) {
                const QString lowText = lowItem->text().trimmed();
                if (!lowText.isEmpty())
                    expectedItem->setText(lowText);
            }
        }
    }
}

void initPeriphGateTable(QTableWidget* table) {
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(QStringLiteral("ProtocolPeriphStateData"), desc))
        return;
    table->clear();
    // clear() 不销毁 cellWidget，先清行避免上一次「方式」列下拉残留
    table->setRowCount(0);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(
        {QStringLiteral("外设项"), QStringLiteral("期望值（等于）")});
    table->setRowCount(desc.fields.size());
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    for (int i = 0; i < desc.fields.size(); ++i) {
        auto* nameItem = new QTableWidgetItem(desc.fields.at(i).displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, desc.fields.at(i).field);
        table->setItem(i, 0, nameItem);
        table->setItem(i, 1, new QTableWidgetItem(QStringLiteral("0")));
    }
}

void applyScreenInspectGatePlaceholders(QTableWidget* table, const QString& reportType,
                                       const QString& defaultField) {
    if (!table || reportType != QLatin1String("ProtocolScreenInspectData"))
        return;
    for (int i = 0; i < table->rowCount(); ++i) {
        QTableWidgetItem* nameItem = table->item(i, 1);
        if (!nameItem)
            continue;
        const QString field = nameItem->data(Qt::UserRole).toString();
        QString low = QStringLiteral("0");
        QString high = QStringLiteral("0");
        QString tip;
        if (field == QLatin1String("deadPixels")) {
            high = QStringLiteral("8");
            tip = QStringLiteral("ROI 内判定为坏点的像素个数。良品接近 0，上限可放 8。");
        } else if (field == QLatin1String("detectedColor")) {
            low = QStringLiteral("蓝");
            high = QStringLiteral("灰");
            tip = QStringLiteral("主色：下拉/填写「蓝绿红白黑灰」；范围可填最小最大（也支持数字 0~5）。");
        } else if (field == QLatin1String("colorMatch")) {
            low = QStringLiteral("是");
            high = QStringLiteral("是");
            tip = QStringLiteral("步骤写了期望纯色时：是/否。可用范围 是~是，或等于+期望值「是」。");
        } else if (field == QLatin1String("ssim")) {
            low = QStringLiteral("0.85");
            high = QStringLiteral("1");
            tip = QStringLiteral("与参考图结构相似度，1 为完全相同。灰阶/花屏对比只卡这一项。");
        } else if (field == QLatin1String("muraStd")) {
            high = QStringLiteral("22");
            tip = QStringLiteral("整屏灰度标准差。纯色画面才有意义（越小越均匀）。灰阶条纹本身很大，不要勾选。");
        }
        nameItem->setToolTip(tip);
        if (QTableWidgetItem* enableItem = table->item(i, 0)) {
            enableItem->setToolTip(QStringLiteral("不必三项全勾，只勾本步需要的判定项。"));
            if (!defaultField.isEmpty() && field == defaultField)
                enableItem->setCheckState(Qt::Checked);
        }
        if (QTableWidgetItem* lo = table->item(i, 3))
            lo->setText(low);
        if (QTableWidgetItem* hi = table->item(i, 4))
            hi->setText(high);
    }
}

/** 指令已绑定的单字段卡控：默认勾选判定项，并给出常用 Op/期望（产测完成=等于1）。 */
void applyBoundGatePlaceholders(QTableWidget* table, const QString& reportType, const QString& defaultField) {
    if (!table || table->columnCount() < 6 || reportType.isEmpty())
        return;
    const QString wantField = defaultField.trimmed();
    for (int i = 0; i < table->rowCount(); ++i) {
        QTableWidgetItem* nameItem = table->item(i, 1);
        if (!nameItem)
            continue;
        const QString field = nameItem->data(Qt::UserRole).toString();
        const bool isDefault = !wantField.isEmpty() && field == wantField;
        const bool onlyOneRow = (table->rowCount() == 1);
        if (!isDefault && !onlyOneRow)
            continue;

        if (QTableWidgetItem* enableItem = table->item(i, 0))
            enableItem->setCheckState(Qt::Checked);

        if (reportType == QLatin1String("ProtocolFactoryDoneData") && field == QLatin1String("done")) {
            if (QComboBox* opCombo = gateOpCellCombo(table, i)) {
                const int opIdx = opCombo->findData(QStringLiteral("eq"));
                if (opIdx >= 0)
                    opCombo->setCurrentIndex(opIdx);
            }
            if (QTableWidgetItem* expectedItem = table->item(i, 5))
                expectedItem->setText(QStringLiteral("1"));
            nameItem->setToolTip(QStringLiteral("指令已绑定「产测完成标志」：期望 1=已完成，0=未完成。"));
            updateMultiGateRowInputMode(table, i);
        }
    }
}

void initRangeMultiGateTable(QTableWidget* table, const QString& reportType) {
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(reportType, desc))
        return;
    table->clear();
    // clear() 不销毁 cellWidget，先清行避免上一次「方式」列下拉残留
    table->setRowCount(0);
    table->setColumnCount(6);
    table->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("判定项"), QStringLiteral("方式"),
                                      QStringLiteral("最小值"), QStringLiteral("最大值"),
                                      QStringLiteral("期望值")});
    table->setRowCount(desc.fields.size());
    table->setColumnWidth(2, 110);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    for (int i = 0; i < desc.fields.size(); ++i) {
        auto* enableItem = new QTableWidgetItem();
        enableItem->setFlags((enableItem->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        enableItem->setCheckState(Qt::Unchecked);
        table->setItem(i, 0, enableItem);
        // read/writeMultiGates 从「判定项」列 UserRole 取字段名；须与这里一致
        auto* nameItem = new QTableWidgetItem(desc.fields.at(i).displayName);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        nameItem->setData(Qt::UserRole, desc.fields.at(i).field);
        table->setItem(i, 1, nameItem);
        auto* opCombo = new QComboBox(table);
        fillGateOpCombo(opCombo);
        opCombo->setCurrentIndex(opCombo->findData(QStringLiteral("range")));
        table->setCellWidget(i, 2, opCombo);
        table->setItem(i, 3, new QTableWidgetItem(QStringLiteral("0")));
        table->setItem(i, 4, new QTableWidgetItem(QStringLiteral("0")));
        table->setItem(i, 5, new QTableWidgetItem());
        QObject::connect(opCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), table, [table, i](int) {
            updateMultiGateRowInputMode(table, i);
        });
        updateMultiGateRowInputMode(table, i);
    }
}

void initFixturePcbaGateTable(QTableWidget* table) {
    initRangeMultiGateTable(table, QStringLiteral("ProtocolFixturePcbaData"));
}

const QHash<QString, QString>& hookDisplayNameMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("HIKVISION_SCANNER_READ"), QStringLiteral("海康扫码枪读取(HIKVISION_SCANNER)")},
        {QStringLiteral("JIG_CURRENT_READ"), QStringLiteral("读取治具电流测量值")},
        {QStringLiteral("DONGLE_SUCTION_ENABLE"), QStringLiteral("开启 dongle 吸力读取")},
        {QStringLiteral("DONGLE_SUCTION_DISABLE"), QStringLiteral("关闭 dongle 吸力读取")},
        {QStringLiteral("DONGLE_SUCTION_SAMPLE"), QStringLiteral("采集双通道吸力(旧Hook，请改用Dongle指令)")},
        {QStringLiteral("DONGLE_SUCTION_SAMPLE_SINGLE"), QStringLiteral("采集单通道吸力(旧Hook，请改用Dongle指令)")},
        {QStringLiteral("SN_WRITE_TAIL"), QStringLiteral("写入 SN 码")},
        {QStringLiteral("MAC_WRITE_ROOT"), QStringLiteral("写入 MAC 地址（Qroot）")},
        {QStringLiteral("GENERATE_WHOLE_MACHINE_SN"), QStringLiteral("由PCBA主板SN生成整机SN并回填界面")},
        {QStringLiteral("BOWL_CAP_DIFF_CHECK"), QStringLiteral("空满碗电容差值卡控（取前序空/满读值）")},
        {QStringLiteral("PRINT_WHOLE_MACHINE_SN"), QStringLiteral("打印整机 SN 二维码")},
        {QStringLiteral("QR_SN_CONSISTENCY_CHECK"), QStringLiteral("二维码一致性校验（与开局SN比对）")},
        {QStringLiteral("SN_PREFIX4_CHECK"), QStringLiteral("产品型号卡控（PCBA SN前四位，不通讯产品）")},
        {QStringLiteral("COUNTDOWN_WAIT"), QStringLiteral("倒计时等待（结果表倒计时，到点自动下一步）")},
        {QStringLiteral("LIGHT_SENSOR_GOLDEN_CALIB"), QStringLiteral("光感单点校准（设亮度→采光感→写+回读，旧）")},
        {QStringLiteral("LIGHT_SENSOR_CALIB_WRITE"), QStringLiteral("光感写入校准值（采光感→写）")},
        {QStringLiteral("LIGHT_SENSOR_CALIB_READ"), QStringLiteral("光感读取校准值（回读+五点终判）")},
        {QStringLiteral("AIR_PRESSURE_SAMPLE"), QStringLiteral("气压采样（多点逐点卡控）")},
        {QStringLiteral("VES_CH1_SET_BRIGHTNESS"), QStringLiteral("VES光源CH1设亮度")},
        {QStringLiteral("MES_GET_ROOT_SKU"), QStringLiteral("MES 获取 ROOTSKU（三元组 SKU）")},
        {QStringLiteral("PLC_MODBUS_CONN"), QStringLiteral("PLC Modbus 连接")},
        {QStringLiteral("PLC_V3_SWITCH_RIGHT_WHOLE"), QStringLiteral("PLC+V3 旋钮整步右旋")},
        {QStringLiteral("PLC_V3_SWITCH_DONE_RESET_M"), QStringLiteral("PLC+V3 旋钮测试完成 M 复位")},
        {QStringLiteral("PLC_V3_KEY_MODE"), QStringLiteral("PLC+V3 模式键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_PROGRAM"), QStringLiteral("PLC+V3 程序键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_SPEED"), QStringLiteral("PLC+V3 速度键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_RIGHT"), QStringLiteral("PLC+V3 右键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_START_PAUSE"), QStringLiteral("PLC+V3 开始暂停键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_LEFT"), QStringLiteral("PLC+V3 左键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_POWER"), QStringLiteral("PLC+V3 电源键触摸整步")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2402_1M"), QStringLiteral("并联 CMW 播放 2402 BLE1M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2440_1M"), QStringLiteral("并联 CMW 播放 2440 BLE1M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2480_1M"), QStringLiteral("并联 CMW 播放 2480 BLE1M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2402_2M"), QStringLiteral("并联 CMW 播放 2402 BLE2M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2440_2M"), QStringLiteral("并联 CMW 播放 2440 BLE2M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_2480_2M"), QStringLiteral("并联 CMW 播放 2480 BLE2M")},
        {QStringLiteral("BYD_MES_GET_UDI_FROM_SN"), QStringLiteral("BYD MES 获取过渡码与UDI(newSfc)")},
        {QStringLiteral("BYD_MES_GET_TRANSITION_CODE"), QStringLiteral("BYD MES 通过整机SN获取过渡码")},
        {QStringLiteral("BYD_MES_GET_NEW_SFC"), QStringLiteral("BYD MES 通过过渡码获取 newSfc")},
        {QStringLiteral("BYD_MES_START_BY_NEW_SFC"), QStringLiteral("BYD MES 使用 newSfc 进行站前检查")},
        {QStringLiteral("PLC_SYNC_WRITE_COIL"), QStringLiteral("[一拖多同步] 统一写线圈(自定义线圈)")},
        {QStringLiteral("PLC_SYNC_READ_COIL"), QStringLiteral("[一拖多同步] 统一监控线圈(自定义线圈)")},
        {QStringLiteral("PLC_SYNC_WRITE_M10"), QStringLiteral("[一拖多同步] 统一写M10脉冲复位(默认M10/可改)")},
        {QStringLiteral("PLC_SYNC_WRITE_M11"), QStringLiteral("[一拖多同步] 统一写M11脉冲复位(默认M11/可改)")},
        {QStringLiteral("PLC_SYNC_READ_M0"), QStringLiteral("[一拖多同步] 统一监听M0按键信号(默认M0/可改)")},
        {QStringLiteral("PLC_SYNC_READ_M1"), QStringLiteral("[一拖多同步] 统一监听M1按键信号(默认M1/可改)")},
    };
    return map;
}

QString hookUiLabel(const QString& hookId) {
    const QString key = hookId.trimmed();
    const auto it = hookDisplayNameMap().constFind(key);
    if (it != hookDisplayNameMap().cend())
        return it.value();
    if (key.isEmpty())
        return QString();
    // 已注册但未维护中文名时显示 HookId，避免与「未实现」混淆
    if (TestCaseHookRegistry::contains(key))
        return QStringLiteral("%1（待补全显示名）").arg(key);
    return QStringLiteral("未登记流程：%1").arg(key);
}

void fillHookCombo(QComboBox* box) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    for (const QString& id : TestCaseHookRegistry::hookIds())
        items.append({hookUiLabel(id), id});
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    for (const auto& item : items)
        box->addItem(item.first, item.second);
}

} // namespace

TestCaseEditDialog::TestCaseEditDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TestCaseEditDialog) {
    ui->setupUi(this);

    // 禁用本页面下拉框的滚轮改值，避免小屏滚动页面时误触修改
    auto* comboWheelGuard = new ComboBoxWheelGuard(this);
    for (QComboBox* combo : findChildren<QComboBox*>()) {
        combo->installEventFilter(comboWheelGuard);
    }

    // 主体可滚动，底部保存/取消始终可见，避免矮屏点不到按钮
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* scrollContent = new QWidget(scroll);
    auto* scrollLayout = new QVBoxLayout(scrollContent);
    scrollLayout->setContentsMargins(0, 0, 0, 0);
    scrollLayout->setSpacing(ui->verticalLayout_root->spacing());
    const QList<QWidget*> sections = {ui->groupBox_meta, ui->groupBox_send, ui->groupBox_timing, ui->groupBox_gate,
                                      ui->groupBox_hook};
    for (QWidget* section : sections) {
        ui->verticalLayout_root->removeWidget(section);
        scrollLayout->addWidget(section);
    }
    scrollLayout->addStretch(1);
    scroll->setWidget(scrollContent);
    ui->verticalLayout_root->insertWidget(0, scroll);
    ui->verticalLayout_root->setStretch(0, 1);

    fillSendChannelCombo(ui->comboBox_sendChannel);
    fillProductProtocolCombo(ui->comboBox_productProtocol);
    fillActionCombo(ui->comboBox_action);
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, TestCaseSendChannel::Product, TestCaseSendAction::Set, QString());
    fillGateReportTypeCombo(ui->comboBox_gateReportType);
    fillGateOpCombo(ui->comboBox_gateOp);
    registerQFreeWorkCatalogTestCaseHooks();
    fillHookCombo(ui->comboBox_hookId);

    // 工站已绑定能力时：用「目标设备」代替通道/协议，给工人简化配置
    label_stationTarget_ = new QLabel(QStringLiteral("目标设备"), this);
    comboBox_stationTarget_ = new QComboBox(this);
    comboBox_stationTarget_->setToolTip(
        QStringLiteral("本工站在「设备能力绑定」中勾选的产品/治具/电源等；选后自动带上通道与协议"));
    ui->formLayout_send->insertRow(0, label_stationTarget_, comboBox_stationTarget_);
    label_stationTarget_->setVisible(false);
    comboBox_stationTarget_->setVisible(false);
    comboBox_stationTarget_->installEventFilter(comboWheelGuard);

    if (QPushButton* saveBtn = ui->buttonBox->button(QDialogButtonBox::Save)) {
        saveBtn->setText(QStringLiteral("保存"));
    }
    if (QPushButton* cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setText(QStringLiteral("取消"));
    }

    connect(ui->comboBox_sendChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onSendChannelChanged);
    connect(ui->comboBox_productProtocol, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onProductProtocolChanged);
    connect(ui->comboBox_action, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onSendActionChanged);
    connect(ui->comboBox_deviceCmd, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onDeviceCmdChanged);
    connect(comboBox_stationTarget_, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onStationTargetChanged);
    connect(ui->comboBox_gateReportType, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onGateReportTypeChanged);
    connect(ui->checkBox_gateEnabled, &QCheckBox::toggled, this, [this](bool) {
        // 勾选卡控时按当前指令重新绑定回包，避免残留「全量类型」下拉
        if (!loadingDefinition_)
            syncGateToSendCommand(comboData(ui->comboBox_gateReportType), comboData(ui->comboBox_gateField));
        else
            updateGateFieldsEnabled();
    });
    connect(ui->checkBox_promptEnabled, &QCheckBox::toggled, this, [this](bool) {
        updatePromptFieldsEnabled();
        if (!loadingDefinition_)
            syncGateToSendCommand(comboData(ui->comboBox_gateReportType), comboData(ui->comboBox_gateField));
    });
    connect(ui->checkBox_promptOnly, &QCheckBox::toggled, this, [this](bool) {
        updatePromptFieldsEnabled();
        if (!loadingDefinition_)
            syncGateToSendCommand(comboData(ui->comboBox_gateReportType), comboData(ui->comboBox_gateField));
    });
    connect(ui->checkBox_hookEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updateHookFieldsEnabled);
    connect(ui->comboBox_hookId, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onHookIdChanged);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (saveValidated())
            accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(ui->pushButton_addParamRow, &QPushButton::clicked, this, [this]() {
        if (!ui->tableWidget_sendParam || ui->tableWidget_sendParam->columnCount() < 2)
            return;
        bool ok = false;
        const QString key = QInputDialog::getText(this, QStringLiteral("添加参数"),
                                                  QStringLiteral("英文参数名（保存到步骤 ini）："),
                                                  QLineEdit::Normal, QString(), &ok)
                                .trimmed();
        if (!ok || key.isEmpty())
            return;
        const int r = ui->tableWidget_sendParam->rowCount();
        ui->tableWidget_sendParam->insertRow(r);
        ui->tableWidget_sendParam->setItem(r, 0, makeSendParamNameItem(key));
        ui->tableWidget_sendParam->setItem(r, 1, makeSendParamValueItem(QString(), false, false));
        ui->tableWidget_sendParam->setCurrentCell(r, 1);
        ui->tableWidget_sendParam->editItem(ui->tableWidget_sendParam->item(r, 1));
    });
    connect(ui->tableWidget_sendParam, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!item || !ui->tableWidget_sendParam || item->column() != 1)
            return;
        if (item->data(SendParamPlaceholderRole).toBool())
            materializeSendParamValueItem(item);
        else
            item->setData(SendParamTouchedRole, true);
    });
    connect(ui->pushButton_removeParamRow, &QPushButton::clicked, this, [this]() {
        if (!ui->tableWidget_sendParam)
            return;
        const auto ranges = ui->tableWidget_sendParam->selectionModel()->selectedRows();
        QList<int> rows;
        for (const QModelIndex& idx : ranges)
            rows.append(idx.row());
        std::sort(rows.begin(), rows.end(), std::greater<int>());
        for (int r : rows)
            ui->tableWidget_sendParam->removeRow(r);
        if (ui->tableWidget_sendParam->columnCount() >= 2 && ui->tableWidget_sendParam->rowCount() == 0) {
            ui->tableWidget_sendParam->insertRow(0);
            ui->tableWidget_sendParam->setItem(0, 0, makeSendParamNameItem(QString()));
            ui->tableWidget_sendParam->setItem(0, 1, new QTableWidgetItem());
        }
    });
    connect(ui->pushButton_restoreParamDefaults, &QPushButton::clicked, this, [this]() {
        if (!ui->tableWidget_sendParam || ui->tableWidget_sendParam->columnCount() < 2)
            return;
        QVariantMap tmpl;
        QStringList preferred;
        if (ui->checkBox_hookEnabled->isChecked() && hookUsesSendParamUi(comboData(ui->comboBox_hookId))) {
            tmpl = hookSendParamDefaultMap(comboData(ui->comboBox_hookId));
        } else {
            const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
            const QString device = comboData(ui->comboBox_productProtocol);
            const QString cmdName = comboData(ui->comboBox_deviceCmd);
            const TestCaseSendAction action = sendActionFromComboData(comboData(ui->comboBox_action));
            tmpl = sendParamDefaultMapForCmd(channel, device, cmdName, action);
            preferred = sendParamPreferredOrder(channel, cmdName, action);
        }
        if (tmpl.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("恢复默认参数表"),
                                     QStringLiteral("当前指令没有预置参数模板，请用「添加一行」自行填写。"));
            return;
        }
        const QVariantMap current = readSendParamMapFromTable(ui->tableWidget_sendParam);
        setSendParamTableFromMapWithTemplate(ui->tableWidget_sendParam, current, tmpl, preferred);
        installSendParamValueCombos(ui->tableWidget_sendParam);
    });
    // 双击参数名列改英文键；双击「参考图路径」的值列选图片
    connect(ui->tableWidget_sendParam, &QTableWidget::cellDoubleClicked, this, [this](int row, int column) {
        if (!ui->tableWidget_sendParam || ui->tableWidget_sendParam->columnCount() != 2)
            return;
        QTableWidgetItem* nameItem = ui->tableWidget_sendParam->item(row, 0);
        if (!nameItem)
            return;
        const QString key = nameItem->data(SendParamKeyRole).toString();
        if (column == 1 && key == QLatin1String("referencePath")) {
            QTableWidgetItem* valItem = ui->tableWidget_sendParam->item(row, 1);
            const QString start = valItem ? valItem->text().trimmed() : QString();
            const QString startDir = start.isEmpty()
                                        ? ScreenInspectAnalyzer::referenceLibraryDir()
                                        : start;
            const QString path = QFileDialog::getOpenFileName(
                this, QStringLiteral("选择标准参考图"), startDir,
                QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp);;所有文件 (*.*)"));
            if (path.isEmpty() || !valItem)
                return;
            QString err;
            const QString stored = ScreenInspectAnalyzer::importReferenceToLibrary(path, &err);
            if (stored.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("参考图"),
                                     err.isEmpty() ? QStringLiteral("导入参考图失败") : err);
                return;
            }
            materializeSendParamValueItem(valItem);
            valItem->setText(stored);
            return;
        }
        if (column != 0)
            return;
        const QString oldKey = key;
        bool ok = false;
        const QString newKey = QInputDialog::getText(this, QStringLiteral("修改参数名"),
                                                     QStringLiteral("英文参数名（保存到步骤 ini）："),
                                                     QLineEdit::Normal, oldKey, &ok)
                                   .trimmed();
        if (!ok || newKey.isEmpty())
            return;
        applySendParamNameCell(nameItem, newKey);
    });

    label_gateTableHint_ = new QLabel(
        QStringLiteral("卡控阈值（勾选判定项，填最小/最大值；气压单位 kPa，逐点共用同一范围）"), ui->groupBox_gate);
    label_gateTableHint_->setWordWrap(true);
    ui->formLayout_gate->addRow(label_gateTableHint_);
    label_gateTableHint_->setVisible(false);

    tableWidget_multiGates_ = new QTableWidget(ui->groupBox_gate);
    initPeriphGateTable(tableWidget_multiGates_);
    tableWidget_multiGates_->setMinimumHeight(120);
    ui->formLayout_gate->addRow(tableWidget_multiGates_);
    tableWidget_multiGates_->setVisible(false);

    updateGateFieldsEnabled();
    updatePromptFieldsEnabled();
    updateHookFieldsEnabled();
    updateProductProtocolRowVisible();
    onDeviceCmdChanged(ui->comboBox_deviceCmd->currentIndex());
}

TestCaseEditDialog::~TestCaseEditDialog() {
    delete ui;
}

void TestCaseEditDialog::fitDialogToScreen() {
    QScreen* scr = screen();
    if (!scr && QGuiApplication::primaryScreen())
        scr = QGuiApplication::primaryScreen();
    if (!scr)
        return;
    const QRect avail = scr->availableGeometry();
    const int maxH = qMax(420, avail.height() - 48);
    const int maxW = qMax(480, avail.width() - 48);
    if (height() > maxH || width() > maxW)
        resize(qMin(width(), maxW), qMin(height(), maxH));
    QRect geo = geometry();
    if (geo.bottom() > avail.bottom())
        geo.moveBottom(avail.bottom());
    if (geo.top() < avail.top())
        geo.moveTop(avail.top());
    if (geo.right() > avail.right())
        geo.moveRight(avail.right());
    if (geo.left() < avail.left())
        geo.moveLeft(avail.left());
    setGeometry(geo);
}

void TestCaseEditDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    fitDialogToScreen();
}

bool TestCaseEditDialog::isRangeMultiGateMode() const {
    // 6列表格（启用/判定项/方式/最小值/最大值/期望值）：除「外设状态」外的所有回传类型
    return ui->checkBox_gateEnabled->isChecked()
           && comboData(ui->comboBox_gateReportType) != QLatin1String("ProtocolPeriphStateData");
}

bool TestCaseEditDialog::isPeriphMultiGateMode() const {
    return ui->checkBox_gateEnabled->isChecked() && comboData(ui->comboBox_gateReportType) == QLatin1String("ProtocolPeriphStateData");
}

void TestCaseEditDialog::rebuildMultiGateTable() {
    if (!tableWidget_multiGates_)
        return;
    if (isRangeMultiGateMode()) {
        const QString reportType = comboData(ui->comboBox_gateReportType);
        if (reportType.isEmpty()) {
            tableWidget_multiGates_->setRowCount(0);
            return;
        }
        initRangeMultiGateTable(tableWidget_multiGates_, reportType);
        applyScreenInspectGatePlaceholders(tableWidget_multiGates_, reportType,
                                           comboData(ui->comboBox_gateField));
        applyBoundGatePlaceholders(tableWidget_multiGates_, reportType, comboData(ui->comboBox_gateField));
    } else if (isPeriphMultiGateMode())
        initPeriphGateTable(tableWidget_multiGates_);
}

void TestCaseEditDialog::writePeriphGatesToTable(const QVector<TestCaseGate>& gates) {
    if (!tableWidget_multiGates_)
        return;
    for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
        QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 0);
        if (!nameItem)
            continue;
        const QString field = nameItem->data(Qt::UserRole).toString();
        QString expected = QStringLiteral("0");
        for (const TestCaseGate& g : gates) {
            if (g.field == field) {
                expected = g.expected.trimmed();
                if (expected.isEmpty() && g.op == TestCaseGateOp::Eq)
                    expected = QString::number(static_cast<int>(g.low));
                break;
            }
        }
        if (QTableWidgetItem* valItem = tableWidget_multiGates_->item(row, 1))
            valItem->setText(expected);
        else
            tableWidget_multiGates_->setItem(row, 1, new QTableWidgetItem(expected));
    }
}

void TestCaseEditDialog::writeMultiGatesToTable(const QVector<TestCaseGate>& gates) {
    if (!tableWidget_multiGates_)
        return;
    if (isRangeMultiGateMode()) {
        for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
            QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 1);
            if (!nameItem)
                continue;
            QString field = nameItem->data(Qt::UserRole).toString();
            if (field.isEmpty()) {
                if (QTableWidgetItem* enableItem = tableWidget_multiGates_->item(row, 0))
                    field = enableItem->data(Qt::UserRole).toString();
            }
            TestCaseGate matched;
            bool found = false;
            for (const TestCaseGate& g : gates) {
                if (g.field == field) {
                    matched = g;
                    found = true;
                    break;
                }
            }
            if (QTableWidgetItem* enableItem = tableWidget_multiGates_->item(row, 0)) {
                enableItem->setCheckState(found && matched.enabled ? Qt::Checked : Qt::Unchecked);
                // 保存时回写 SettingsKey，避免 UI 往返丢掉 BLE/LowRssi 等绑定
                enableItem->setData(Qt::UserRole + 1, found ? matched.lowSettingsKey : QString());
                enableItem->setData(Qt::UserRole + 2, found ? matched.highSettingsKey : QString());
                enableItem->setData(Qt::UserRole + 3, found ? matched.expectedSettingsKey : QString());
            }
            if (!found)
                continue;
            auto setCell = [&](int col, const QString& text) {
                if (QTableWidgetItem* item = tableWidget_multiGates_->item(row, col))
                    item->setText(text);
                else
                    tableWidget_multiGates_->setItem(row, col, new QTableWidgetItem(text));
            };
            if (QComboBox* opCombo = gateOpCellCombo(tableWidget_multiGates_, row)) {
                const int opIdx = opCombo->findData(gateOpToTableText(matched.op));
                opCombo->setCurrentIndex(opIdx >= 0 ? opIdx : 0);
            }
            // 有 LowSettingsKey/HighSettingsKey 时显示 SETTINGS 解析后的实际卡控范围
            double lowShow = matched.low;
            double highShow = matched.high;
            GateRegistry::resolveRangeBounds(matched, lowShow, highShow);
            if (field == QLatin1String("detectedColor")) {
                setCell(3, ScreenInspectAnalyzer::colorName(qRound(lowShow)));
                setCell(4, ScreenInspectAnalyzer::colorName(qRound(highShow)));
            } else if (field == QLatin1String("colorMatch")) {
                setCell(3, qAbs(lowShow - 1.0) < 0.0001 ? QStringLiteral("是") : QStringLiteral("否"));
                setCell(4, qAbs(highShow - 1.0) < 0.0001 ? QStringLiteral("是") : QStringLiteral("否"));
            } else {
                setCell(3, QString::number(lowShow));
                setCell(4, QString::number(highShow));
            }
            QString expectedShow = matched.expected;
            if (expectedShow.isEmpty()
                && (matched.op == TestCaseGateOp::Gt || matched.op == TestCaseGateOp::Lt
                    || matched.op == TestCaseGateOp::Eq)) {
                expectedShow = QString::number(lowShow);
            }
            if (field == QLatin1String("detectedColor") && !expectedShow.isEmpty()) {
                bool cok = false;
                const int ci = ScreenInspectAnalyzer::parseColorIndex(expectedShow, &cok);
                if (cok)
                    expectedShow = ScreenInspectAnalyzer::colorName(ci);
            } else if (field == QLatin1String("colorMatch") && !expectedShow.isEmpty()) {
                bool okNum = false;
                const double v = expectedShow.toDouble(&okNum);
                if (okNum)
                    expectedShow = qAbs(v - 1.0) < 0.0001 ? QStringLiteral("是") : QStringLiteral("否");
            }
            setCell(5, expectedShow);
            updateMultiGateRowInputMode(tableWidget_multiGates_, row);
        }
        return;
    }
    writePeriphGatesToTable(gates);
}

QVector<TestCaseGate> TestCaseEditDialog::readPeriphGatesFromTable() const {
    QVector<TestCaseGate> gates;
    if (!tableWidget_multiGates_)
        return gates;
    const QString reportType = comboData(ui->comboBox_gateReportType);
    for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
        QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 0);
        QTableWidgetItem* valItem = tableWidget_multiGates_->item(row, 1);
        if (!nameItem || !valItem)
            continue;
        TestCaseGate g;
        g.enabled = true;
        g.reportType = reportType;
        g.field = nameItem->data(Qt::UserRole).toString();
        g.op = TestCaseGateOp::Eq;
        g.expected = valItem->text().trimmed();
        g.low = g.expected.toDouble();
        g.high = g.low;
        gates.append(g);
    }
    return gates;
}

QVector<TestCaseGate> TestCaseEditDialog::readMultiGatesFromTable() const {
    if (!tableWidget_multiGates_)
        return {};
    const bool rangeTable = tableWidget_multiGates_->columnCount() >= 6;
    if (rangeTable || isRangeMultiGateMode()) {
        QVector<TestCaseGate> gates;
        const QString reportType = comboData(ui->comboBox_gateReportType);
        for (int row = 0; row < tableWidget_multiGates_->rowCount(); ++row) {
            QTableWidgetItem* enableItem = tableWidget_multiGates_->item(row, 0);
            QTableWidgetItem* nameItem = tableWidget_multiGates_->item(row, 1);
            if (!nameItem)
                continue;
            const bool rowEnabled = enableItem && enableItem->checkState() == Qt::Checked;
            QString field = nameItem->data(Qt::UserRole).toString();
            if (field.isEmpty() && enableItem)
                field = enableItem->data(Qt::UserRole).toString();
            auto cellText = [&](int col) -> QString {
                if (QTableWidgetItem* item = tableWidget_multiGates_->item(row, col))
                    return item->text().trimmed();
                return QString();
            };
            QString opText;
            if (QComboBox* opCombo = gateOpCellCombo(tableWidget_multiGates_, row))
                opText = opCombo->currentData().toString();
            const QString lowText = cellText(3);
            const QString highText = cellText(4);
            const QString expectedText = cellText(5);
            auto parseBound = [&](const QString& text) -> double {
                if (field == QLatin1String("detectedColor")) {
                    bool cok = false;
                    const int ci = ScreenInspectAnalyzer::parseColorIndex(text, &cok);
                    if (cok && !text.isEmpty())
                        return ci;
                }
                if (field == QLatin1String("colorMatch")) {
                    const QString t = text.trimmed();
                    if (t == QStringLiteral("是") || t == QLatin1String("1"))
                        return 1.0;
                    if (t == QStringLiteral("否") || t == QLatin1String("0"))
                        return 0.0;
                }
                return text.toDouble();
            };
            // 未勾选但已填数值的行也要落盘（Enabled=false），否则重开后用户填的上下限全丢
            const bool hasData = !qFuzzyIsNull(parseBound(lowText)) || !qFuzzyIsNull(parseBound(highText))
                || !expectedText.isEmpty()
                || (field == QLatin1String("detectedColor")
                    && (!lowText.isEmpty() || !highText.isEmpty()));
            TestCaseGate g;
            g.enabled = rowEnabled;
            g.reportType = reportType;
            g.field = field;
            g.op = gateOpFromTableText(opText.isEmpty() ? QStringLiteral("range") : opText);
            g.low = parseBound(lowText);
            g.high = parseBound(highText);
            g.expected = expectedText;
            if (enableItem) {
                g.lowSettingsKey = enableItem->data(Qt::UserRole + 1).toString();
                g.highSettingsKey = enableItem->data(Qt::UserRole + 2).toString();
                g.expectedSettingsKey = enableItem->data(Qt::UserRole + 3).toString();
            }
            // 大于/小于/等于：以期望值为准；兼容旧配置只写了最小值
            if (g.op == TestCaseGateOp::Gt || g.op == TestCaseGateOp::Lt || g.op == TestCaseGateOp::Eq) {
                if (g.expected.trimmed().isEmpty())
                    g.expected = lowText;
                bool ok = false;
                double thr = g.expected.trimmed().toDouble(&ok);
                if (!ok && field == QLatin1String("detectedColor")) {
                    const int ci = ScreenInspectAnalyzer::parseColorIndex(g.expected, &ok);
                    if (ok)
                        thr = ci;
                }
                if (!ok && field == QLatin1String("colorMatch")) {
                    const QString t = g.expected.trimmed();
                    if (t == QStringLiteral("是") || t == QLatin1String("1")) {
                        thr = 1.0;
                        ok = true;
                    } else if (t == QStringLiteral("否") || t == QLatin1String("0")) {
                        thr = 0.0;
                        ok = true;
                    }
                }
                if (ok) {
                    g.low = thr;
                    g.high = thr;
                }
            }
            if (!rowEnabled && !hasData)
                continue;
            gates.append(g);
        }
        return gates;
    }
    return readPeriphGatesFromTable();
}

void TestCaseEditDialog::updateGateFieldsEnabled() {
    const bool on = ui->checkBox_gateEnabled->isChecked();
    const bool hasBoundType = ui->comboBox_gateReportType->count() > 0;
    // 仅一种回包：已绑定指令，不展示下拉；无绑定：只提示、不可选全量类型
    const bool showReportTypeCombo = on && hasBoundType && !gateReportTypeLocked_;
    const bool showReportTypeLabel = on && (!hasBoundType || !gateReportTypeLocked_);
    ui->label_gateReportType->setVisible(showReportTypeLabel);
    ui->comboBox_gateReportType->setVisible(showReportTypeCombo);
    ui->comboBox_gateReportType->setEnabled(showReportTypeCombo);
    if (label_gateTableHint_)
        label_gateTableHint_->setVisible(on && hasBoundType);
    if (tableWidget_multiGates_)
        tableWidget_multiGates_->setVisible(on && hasBoundType);
    // 单字段控件已统一为表格，全部隐藏
    ui->label_gateField->setVisible(false);
    ui->comboBox_gateField->setVisible(false);
    ui->label_gateOp->setVisible(false);
    ui->comboBox_gateOp->setVisible(false);
    ui->label_gateLow->setVisible(false);
    ui->lineEdit_gateLow->setVisible(false);
    ui->label_gateHigh->setVisible(false);
    ui->lineEdit_gateHigh->setVisible(false);
    ui->label_gateExpected->setVisible(false);
    ui->lineEdit_gateExpected->setVisible(false);
}

void TestCaseEditDialog::updatePromptFieldsEnabled() {
    const bool on = ui->checkBox_promptEnabled->isChecked();
    ui->checkBox_promptOnly->setVisible(on);
    ui->label_promptText->setVisible(on);
    ui->plainTextEdit_promptText->setVisible(on);
    if (!on)
        ui->checkBox_promptOnly->setChecked(false);
    // 纯空白提醒时隐藏测试指令区；带 Param_ 的 Hook 步骤仍展示参数表
    if (ui->checkBox_hookEnabled->isChecked())
        updateHookFieldsEnabled();
    else
        ui->groupBox_send->setVisible(!on || !ui->checkBox_promptOnly->isChecked());
}

void TestCaseEditDialog::updateHookFieldsEnabled() {
    const bool on = ui->checkBox_hookEnabled->isChecked();
    const bool promptOnly =
        ui->checkBox_promptEnabled->isChecked() && ui->checkBox_promptOnly->isChecked();
    const QString hookId = comboData(ui->comboBox_hookId);
    const bool hookParamOnly = on && hookUsesSendParamUi(hookId);

    ui->label_hookId->setVisible(on);
    ui->comboBox_hookId->setVisible(on);

    if (hookParamOnly) {
        ui->groupBox_send->setVisible(true);
        ui->groupBox_send->setTitle(QStringLiteral("Hook 参数"));
        ui->label_param->setText(QStringLiteral("步骤参数"));
        setSendCommandFieldRowsVisible(ui, label_stationTarget_, comboBox_stationTarget_, false,
                                       stationBindings_.configured);
        const QVariantMap current = readSendParamMapFromTable(ui->tableWidget_sendParam);
        const QVariantMap tmpl = hookSendParamDefaultMap(hookId);
        bool hasMatchingKey = false;
        for (const QString& k : tmpl.keys()) {
            if (current.contains(k)) {
                hasMatchingKey = true;
                break;
            }
        }
        if (hasMatchingKey)
            applyHookSendParamUi(ui, hookId, current);
        else
            applyHookSendParamUi(ui, hookId, QVariantMap());
        // Hook 步骤隐藏 Send 区后仍须绑定卡控回包，否则「卡控」表为空
        if (hookId == QLatin1String("AIR_PRESSURE_SAMPLE")) {
            syncGateToSendCommand(QStringLiteral("ProtocolAirPressureSampleData"),
                                  QStringLiteral("airPressureLeft"));
        }
        return;
    }

    ui->groupBox_send->setTitle(QStringLiteral("测试指令"));
    ui->label_param->setText(QStringLiteral("指令参数"));
    ui->groupBox_send->setVisible(!on && !promptOnly);
    if (!on && !promptOnly) {
        setSendCommandFieldRowsVisible(ui, label_stationTarget_, comboBox_stationTarget_, true,
                                       stationBindings_.configured);
        updateProductProtocolRowVisible();
        onDeviceCmdChanged(ui->comboBox_deviceCmd->currentIndex());
        return;
    }
    setSendCommandFieldRowsVisible(ui, label_stationTarget_, comboBox_stationTarget_, false,
                                   stationBindings_.configured);
    updateSendParamVisibility(false);
}

void TestCaseEditDialog::onHookIdChanged(int) {
    updateHookFieldsEnabled();
}

void TestCaseEditDialog::updateSendParamVisibility(bool hasParam) {
    ui->label_param->setVisible(hasParam);
    ui->stackedWidget_param->setVisible(hasParam);
}

void TestCaseEditDialog::syncGateToSendCommand(const QString& preferredReportType, const QString& preferredField) {
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const QString protocolOrDevice = comboData(ui->comboBox_productProtocol);
    const QString cmdName = comboData(ui->comboBox_deviceCmd);
    // PromptOnly 只等主动上报，不发指令：卡控类型勿跟 DeviceCmd 走（否则 KeySignalRead 等会冲掉 keyButtonId）
    const bool promptOnlyGate =
        ui->checkBox_promptEnabled->isChecked() && ui->checkBox_promptOnly->isChecked();
    GateSendBinding binding;
    if (!promptOnlyGate)
        binding = GateRegistry::bindingForSend(channel, protocolOrDevice, cmdName);
    if (ui->checkBox_hookEnabled->isChecked()) {
        const GateSendBinding hookBinding = GateRegistry::bindingForHook(comboData(ui->comboBox_hookId));
        if (!hookBinding.reportTypes.isEmpty())
            binding = hookBinding;
    }

    QStringList allowedTypes = binding.reportTypes;
    const QString preferredType = preferredReportType.trimmed();
    if (allowedTypes.isEmpty() && !preferredType.isEmpty())
        allowedTypes << preferredType;
    // 仅提示等按键上报：默认允许 ProtocolButtonStateData
    if (allowedTypes.isEmpty() && promptOnlyGate)
        allowedTypes << QStringLiteral("ProtocolButtonStateData");
    if (promptOnlyGate && binding.defaultField.isEmpty())
        binding.defaultField = QStringLiteral("keyButtonId");

    gateReportTypeLocked_ = (allowedTypes.size() == 1);
    if (allowedTypes.isEmpty())
        ui->label_gateReportType->setText(QStringLiteral("当前指令未绑定卡控回包（请先选带回包的 Get 指令）"));
    else
        ui->label_gateReportType->setText(QStringLiteral("本步回包"));

    {
        QSignalBlocker blocker(ui->comboBox_gateReportType);
        fillGateReportTypeCombo(ui->comboBox_gateReportType, allowedTypes);
        QString wantType = preferredType;
        if (wantType.isEmpty() || comboIndexByData(ui->comboBox_gateReportType, wantType) < 0) {
            if (!allowedTypes.isEmpty())
                wantType = allowedTypes.constFirst();
        }
        const int typeIdx = comboIndexByData(ui->comboBox_gateReportType, wantType);
        if (typeIdx >= 0)
            ui->comboBox_gateReportType->setCurrentIndex(typeIdx);
    }

    fillGateFieldCombo(ui->comboBox_gateField, comboData(ui->comboBox_gateReportType));
    QString wantField = preferredField.trimmed();
    if (wantField.isEmpty() || comboIndexByData(ui->comboBox_gateField, wantField) < 0)
        wantField = binding.defaultField;
    const int fieldIdx = comboIndexByData(ui->comboBox_gateField, wantField);
    if (fieldIdx >= 0)
        ui->comboBox_gateField->setCurrentIndex(fieldIdx);

    rebuildMultiGateTable();
    updateGateFieldsEnabled();
}

void TestCaseEditDialog::onGateReportTypeChanged(int) {
    fillGateFieldCombo(ui->comboBox_gateField, comboData(ui->comboBox_gateReportType));
    rebuildMultiGateTable();
    updateGateFieldsEnabled();
}

void TestCaseEditDialog::setStationContext(const QString& stationKey) {
    stationKey_ = stationKey.trimmed();
    if (stationKey_.isEmpty())
        stationBindings_ = TestCaseStationBindings{};
    else
        stationBindings_ = TestCaseStore::loadStationBindings(stationKey_);
    applyStationBindingsUiMode();
}

void TestCaseEditDialog::setFlowContext(const QVector<TestFlowItemEntry>& entries) {
    flowEntries_ = entries;
}

void TestCaseEditDialog::setDefinition(const TestCaseDefinition& def, const QString& storageKey) {
    loadingDefinition_ = true;
    originalCaseName_ = storageKey.trimmed().isEmpty() ? def.meta.name.trimmed() : storageKey.trimmed();
    ui->lineEdit_caseName->setText(def.meta.name);
    const QString displayName =
        def.meta.displayName.trimmed().isEmpty() ? def.meta.name : def.meta.displayName.trimmed();
    ui->lineEdit_displayName->setText(displayName);
    ui->lineEdit_mesTag->setText(def.meta.mesTag);
    ui->checkBox_promptEnabled->setChecked(def.meta.promptEnabled);
    ui->checkBox_promptOnly->setChecked(def.meta.promptOnly);
    ui->plainTextEdit_promptText->setPlainText(def.meta.promptText);

    const TestCaseSendAction action = def.send.action;
    ui->comboBox_action->blockSignals(true);
    const int actionIdx = comboIndexByData(ui->comboBox_action,
                                           action == TestCaseSendAction::Get ? QStringLiteral("Get")
                                                                             : QStringLiteral("Set"));
    if (actionIdx >= 0)
        ui->comboBox_action->setCurrentIndex(actionIdx);
    ui->comboBox_action->blockSignals(false);

    const TestCaseSendChannel channel = def.send.channel;
    ui->comboBox_sendChannel->blockSignals(true);
    ui->comboBox_deviceCmd->blockSignals(true);
    const int channelIdx = comboIndexByData(ui->comboBox_sendChannel, sendChannelComboData(channel));
    if (channelIdx >= 0)
        ui->comboBox_sendChannel->setCurrentIndex(channelIdx);
    ui->comboBox_productProtocol->blockSignals(true);
    fillProtocolComboForChannel(ui->comboBox_productProtocol, channel);
    QString protoIni;
    if (channel == TestCaseSendChannel::Fixture || channel == TestCaseSendChannel::Software)
        protoIni = FixturePcbaCmdCatalog::fixtureProtocolToIni(
            channel == TestCaseSendChannel::Software ? TestCaseFixtureProtocol::AudioExpert
                                                    : def.send.fixtureProtocol);
    else if (channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi)
        protoIni = def.send.device;
    else
        protoIni = DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol);
    const int protoIdx = comboIndexByData(ui->comboBox_productProtocol, protoIni);
    if (protoIdx >= 0)
        ui->comboBox_productProtocol->setCurrentIndex(protoIdx);
    ui->comboBox_productProtocol->blockSignals(false);
    applyStationBindingsUiMode();
    if (stationBindings_.configured) {
        const QString wantTarget = stationTargetKeyForSend(channel, protoIni);
        fillStationTargetCombo(wantTarget);
        // 回显目标后同步内部通道/协议（含绑定外旧步骤临时补进的一项）
        if (comboBox_stationTarget_ && comboBox_stationTarget_->findData(wantTarget) >= 0)
            applyStationTargetToChannelProtocol();
    }

    // 产品/治具/软件/外设：指令表按「协议或目标设备」下拉过滤，统一传 protoIni（勿用空的 send.device）
    const QString cmdListDevice =
        (channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial
         || channel == TestCaseSendChannel::Fixture || channel == TestCaseSendChannel::Software
         || channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi)
            ? protoIni
            : def.send.device;
    // 绑定模式下指令表与目标设备一致（apply 后通道/协议已对齐）
    QString cmdFilterDevice = cmdListDevice;
    TestCaseSendChannel cmdFilterChannel = channel;
    if (stationBindings_.configured && comboBox_stationTarget_) {
        TestCaseSendChannel targetCh = channel;
        QString targetDev;
        if (parseStationTargetKey(comboBox_stationTarget_->currentData().toString(), targetCh, targetDev)) {
            cmdFilterChannel = targetCh;
            cmdFilterDevice = targetDev;
        }
    }
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, cmdFilterChannel, action, cmdFilterDevice, QString());
    int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, def.send.deviceCmd);
    if (cmdIdx < 0 && ui->comboBox_deviceCmd->count() > 0)
        cmdIdx = 0;
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    ui->comboBox_sendChannel->blockSignals(false);
    ui->comboBox_deviceCmd->blockSignals(false);
    onDeviceCmdChanged(cmdIdx);

    const QString protocolCtx = sendParamProtocolContext(channel, comboData(ui->comboBox_productProtocol));
    const SendCmdParamUi uiSchema = sendCmdParamUiForName(def.send.deviceCmd, channel, protocolCtx);
    QVariant paramForUi = def.send.param;
    if (channel == TestCaseSendChannel::Fixture && uiSchema.kind == SendCmdParamKind::Int && isFixtureMachineIndexPlaceholder(def.send.param))
        paramForUi = 0;
    applySendParamToUi(uiSchema, paramForUi, ui->page_paramNone, ui->page_paramInt, ui->page_paramJson,
                       ui->stackedWidget_param, ui->spinBox_intParam, ui->tableWidget_sendParam);
    if (uiSchema.kind == SendCmdParamKind::JsonMap) {
        applySendParamTableWithTemplate(ui->tableWidget_sendParam, channel, comboData(ui->comboBox_productProtocol),
                                        def.send.deviceCmd, sendParamAsJsonMap(paramForUi), action);
    }
    const bool hasParamTemplate =
        !sendParamDefaultMapForCmd(channel, comboData(ui->comboBox_productProtocol), def.send.deviceCmd, action)
             .isEmpty();
    applySendParamHintToUi(uiSchema, uiSchema.valid && uiSchema.kind != SendCmdParamKind::None,
                           ui->label_sendParamHint, ui->tableWidget_sendParam, ui->spinBox_intParam,
                           ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                           ui->pushButton_restoreParamDefaults, hasParamTemplate);
    if (def.hook.enabled && hookUsesSendParamUi(def.hook.hookId))
        applyHookSendParamUi(ui, def.hook.hookId, sendParamAsJsonMap(paramForUi));
    // 须与 onDeviceCmdChanged 的 cmdKey 格式一致（含 |Get/|Set），否则 updateHookFieldsEnabled
    // 再进 onDeviceCmdChanged 会误判指令变更，用空表刷掉已回读的 Param（如 FacResult done=0）
    lastSendParamCmdKey_ = sendChannelComboData(channel) + QLatin1Char('|')
        + comboData(ui->comboBox_productProtocol) + QLatin1Char('|') + def.send.deviceCmd
        + QLatin1Char('|') + (action == TestCaseSendAction::Get ? QLatin1String("Get") : QLatin1String("Set"));

    ui->spinBox_delayBefore->setValue(def.timing.delayBeforeMs);
    ui->spinBox_delayAfter->setValue(def.timing.delayAfterMs);
    {
        int timeoutMs = def.timing.commandTimeoutMs;
        if (timeoutMs <= 0)
            timeoutMs = def.gate.enabled ? 8000 : 3000;
        ui->spinBox_commandTimeout->setValue(timeoutMs);
    }
    ui->checkBox_waitReply->setChecked(def.timing.waitReply);

    const QVector<TestCaseGate> gatesForUi = def.gates;
    const QString gateReportTypeForUi = def.gate.reportType;

    // loadingDefinition_ 需保持到最后：下面的 update* 会经 onDeviceCmdChanged 重建卡控表格
    ui->checkBox_gateEnabled->setChecked(def.gate.enabled);
    syncGateToSendCommand(gateReportTypeForUi, def.gate.field);

    const QString opKey = def.gate.op == TestCaseGateOp::Gt ? QStringLiteral("gt")
        : def.gate.op == TestCaseGateOp::Lt                 ? QStringLiteral("lt")
        : def.gate.op == TestCaseGateOp::Eq                 ? QStringLiteral("eq")
        : def.gate.op == TestCaseGateOp::CompareVersions    ? QStringLiteral("compareVersions")
                                                            : QStringLiteral("range");
    const int opIdx = comboIndexByData(ui->comboBox_gateOp, opKey);
    if (opIdx >= 0)
        ui->comboBox_gateOp->setCurrentIndex(opIdx);

    ui->lineEdit_gateLow->setText(QString::number(def.gate.low));
    ui->lineEdit_gateHigh->setText(QString::number(def.gate.high));
    ui->lineEdit_gateExpected->setText(def.gate.expected);
    {
        // 加载时先稳定 Hook 选择，避免中途信号用旧 Hook 模板重建参数表并覆盖已保存值。
        const QSignalBlocker hookEnabledBlocker(ui->checkBox_hookEnabled);
        const QSignalBlocker hookIdBlocker(ui->comboBox_hookId);
        ui->checkBox_hookEnabled->setChecked(def.hook.enabled);
        const int hookIdx = comboIndexByData(ui->comboBox_hookId, def.hook.hookId);
        if (hookIdx >= 0)
            ui->comboBox_hookId->setCurrentIndex(hookIdx);
    }

    updateGateFieldsEnabled();
    updatePromptFieldsEnabled();
    updateHookFieldsEnabled();

    // 表格回填必须放在 update* 之后，否则会被 rebuildMultiGateTable 清成 0/未勾选
    if (ui->checkBox_gateEnabled->isChecked() && !gatesForUi.isEmpty())
        writeMultiGatesToTable(gatesForUi);
    loadingDefinition_ = false;
}

TestCaseDefinition TestCaseEditDialog::definition() const {
    TestCaseDefinition def;
    def.meta.name = ui->lineEdit_caseName->text().trimmed();
    def.meta.displayName = ui->lineEdit_displayName->text().trimmed();
    if (def.meta.displayName.isEmpty())
        def.meta.displayName = def.meta.name;
    def.meta.mesTag = ui->lineEdit_mesTag->text().trimmed();
    def.meta.promptEnabled = ui->checkBox_promptEnabled->isChecked();
    def.meta.promptOnly = def.meta.promptEnabled && ui->checkBox_promptOnly->isChecked();
    def.meta.promptText = ui->plainTextEdit_promptText->toPlainText();

    def.send.action = comboData(ui->comboBox_action) == QLatin1String("Get") ? TestCaseSendAction::Get
                                                                             : TestCaseSendAction::Set;
    def.send.channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    if (def.send.channel == TestCaseSendChannel::Fixture) {
        def.send.fixtureProtocol =
            FixturePcbaCmdCatalog::fixtureProtocolFromIni(comboData(ui->comboBox_productProtocol));
    } else if (def.send.channel == TestCaseSendChannel::Software) {
        def.send.fixtureProtocol = TestCaseFixtureProtocol::AudioExpert;
        def.send.device = QStringLiteral("AUDIO_EXPERT");
    } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        def.send.device = comboData(ui->comboBox_productProtocol);
    } else {
        def.send.productProtocol = productProtocolFromComboData(comboData(ui->comboBox_productProtocol));
    }
    def.send.deviceCmd = comboData(ui->comboBox_deviceCmd);
    // 仅提示等上报：不写死指令名，避免空选中被保存成下拉第一项（曾冲掉加减/模式按键卡控）
    if (def.meta.promptOnly)
        def.send.deviceCmd.clear();
    def.hook.enabled = ui->checkBox_hookEnabled->isChecked();
    def.hook.hookId = comboData(ui->comboBox_hookId);
    if (def.hook.enabled && hookUsesSendParamUi(def.hook.hookId)) {
        def.send.param = readSendParamMapFromTable(ui->tableWidget_sendParam);
    } else {
        const QString protocolCtx =
            sendParamProtocolContext(def.send.channel, comboData(ui->comboBox_productProtocol));
        const SendCmdParamUi uiSchema =
            sendCmdParamUiForName(def.send.deviceCmd, def.send.channel, protocolCtx);
        def.send.param = readSendParamFromUi(uiSchema, ui->spinBox_intParam, ui->tableWidget_sendParam);
        if (def.send.channel == TestCaseSendChannel::Fixture && uiSchema.kind == SendCmdParamKind::Int
            && ui->spinBox_intParam->value() == 0)
            def.send.param = QStringLiteral("$INDEX");
    }

    def.timing.delayBeforeMs = ui->spinBox_delayBefore->value();
    def.timing.delayAfterMs = ui->spinBox_delayAfter->value();
    def.timing.commandTimeoutMs = ui->spinBox_commandTimeout->value();
    def.timing.waitReply = ui->checkBox_waitReply->isChecked();
    def.gate.enabled = ui->checkBox_gateEnabled->isChecked();
    def.gate.reportType = comboData(ui->comboBox_gateReportType);
    def.gates.clear();
    if (def.gate.enabled) {
        def.gates = readMultiGatesFromTable();
        // 工站 profile 保存以表格数值为准，避免 LowSettingsKey 仍读上位机设置导致「改了不生效」
        if (!stationKey_.isEmpty()) {
            for (TestCaseGate& g : def.gates) {
                g.lowSettingsKey.clear();
                g.highSettingsKey.clear();
                g.expectedSettingsKey.clear();
            }
        }
        if (!def.gates.isEmpty()) {
            def.gate = def.gates.first();
            def.gate.enabled = true;
            def.gate.reportType = comboData(ui->comboBox_gateReportType);
            if (def.gates.size() > 1
                || def.gate.reportType == QLatin1String("ProtocolDongleSuctionPeakData")
                || def.gate.reportType == QLatin1String("ProtocolFixturePcbaData")
                || def.gate.reportType == QLatin1String("ProtocolJieliBtBoxData")
                || def.gate.reportType == QLatin1String("ProtocolScreenInspectData"))
                def.gate.field = QStringLiteral("multi");
        }
    }
    return def;
}

void TestCaseEditDialog::updateProductProtocolRowVisible() {
    if (stationBindings_.configured) {
        ui->label_productProtocol->setVisible(false);
        ui->comboBox_productProtocol->setVisible(false);
        return;
    }
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const bool showProtocol =
        channel == TestCaseSendChannel::Product || channel == TestCaseSendChannel::ProductSerial
        || channel == TestCaseSendChannel::Fixture || channel == TestCaseSendChannel::Software
        || channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi;
    ui->label_productProtocol->setVisible(showProtocol);
    ui->comboBox_productProtocol->setVisible(showProtocol);
    if (showProtocol) {
        if (channel == TestCaseSendChannel::Fixture)
            ui->label_productProtocol->setText(QStringLiteral("治具协议"));
        else if (channel == TestCaseSendChannel::Software)
            ui->label_productProtocol->setText(QStringLiteral("软件协议"));
        else if (channel == TestCaseSendChannel::Modbus || channel == TestCaseSendChannel::Scpi)
            ui->label_productProtocol->setText(QStringLiteral("目标外设"));
        else
            ui->label_productProtocol->setText(QStringLiteral("产品协议"));
    }
}

void TestCaseEditDialog::applyStationBindingsUiMode() {
    const bool use = stationBindings_.configured;
    if (label_stationTarget_)
        label_stationTarget_->setVisible(use);
    if (comboBox_stationTarget_)
        comboBox_stationTarget_->setVisible(use);
    ui->label_sendChannel->setVisible(!use);
    ui->comboBox_sendChannel->setVisible(!use);
    updateProductProtocolRowVisible();
}

void TestCaseEditDialog::fillStationTargetCombo(const QString& keepTargetKey) {
    if (!comboBox_stationTarget_)
        return;
    const QSignalBlocker blocker(comboBox_stationTarget_);
    comboBox_stationTarget_->clear();
    if (!stationBindings_.configured)
        return;

    // 类别用【】打头；后缀短名。旧步骤目标若不在绑定内，按同款文案临时补一项以便回显/保存
    auto addTarget = [this](const QString& label, const QString& targetKey) {
        comboBox_stationTarget_->addItem(label, targetKey);
    };

    if (stationBindings_.enableProduct) {
        for (const QString& protoIni : stationBindings_.productProtocols)
            addTarget(stationTargetUiLabel(QStringLiteral("product:") + protoIni),
                      QStringLiteral("product:") + protoIni);
    }
    if (stationBindings_.enableProductSerial) {
        for (const QString& protoIni : stationBindings_.productProtocols)
            addTarget(stationTargetUiLabel(QStringLiteral("product_serial:") + protoIni),
                      QStringLiteral("product_serial:") + protoIni);
    }
    if (stationBindings_.enableDongle)
        addTarget(stationTargetUiLabel(QStringLiteral("dongle")), QStringLiteral("dongle"));
    if (stationBindings_.enableCloud)
        addTarget(stationTargetUiLabel(QStringLiteral("cloud")), QStringLiteral("cloud"));
    for (const QString& fix : stationBindings_.fixtures) {
        const QString shortName =
            FixturePcbaCmdCatalog::fixtureProtocolToIni(FixturePcbaCmdCatalog::fixtureProtocolFromIni(fix));
        addTarget(stationTargetUiLabel(QStringLiteral("fixture:") + shortName),
                  QStringLiteral("fixture:") + shortName);
    }
    for (const QString& mod : stationBindings_.modbusDevices)
        addTarget(stationTargetUiLabel(QStringLiteral("modbus:") + mod), QStringLiteral("modbus:") + mod);
    for (const QString& scpi : stationBindings_.scpiDevices)
        addTarget(stationTargetUiLabel(QStringLiteral("scpi:") + scpi), QStringLiteral("scpi:") + scpi);

    const QString keep = keepTargetKey.trimmed();
    if (!keep.isEmpty() && comboBox_stationTarget_->findData(keep) < 0)
        addTarget(stationTargetUiLabel(keep), keep);

    QString want = keep;
    if (want.isEmpty() && !stationBindings_.defaultTarget.isEmpty())
        want = stationBindings_.defaultTarget.trimmed();
    int idx = want.isEmpty() ? -1 : comboBox_stationTarget_->findData(want);
    if (idx < 0 && keep.isEmpty() && comboBox_stationTarget_->count() > 0)
        idx = 0;
    comboBox_stationTarget_->setCurrentIndex(idx >= 0 ? idx : -1);
}

void TestCaseEditDialog::applyStationTargetToChannelProtocol() {
    if (!comboBox_stationTarget_)
        return;
    TestCaseSendChannel channel = TestCaseSendChannel::Product;
    QString device;
    if (!parseStationTargetKey(comboBox_stationTarget_->currentData().toString(), channel, device))
        return;
    ui->comboBox_sendChannel->blockSignals(true);
    ui->comboBox_productProtocol->blockSignals(true);
    const int channelIdx = comboIndexByData(ui->comboBox_sendChannel, sendChannelComboData(channel));
    if (channelIdx >= 0)
        ui->comboBox_sendChannel->setCurrentIndex(channelIdx);
    fillProtocolComboForChannel(ui->comboBox_productProtocol, channel);
    if (!device.isEmpty()) {
        const int protoIdx = comboIndexByData(ui->comboBox_productProtocol, device);
        if (protoIdx >= 0)
            ui->comboBox_productProtocol->setCurrentIndex(protoIdx);
    }
    ui->comboBox_sendChannel->blockSignals(false);
    ui->comboBox_productProtocol->blockSignals(false);
}

void TestCaseEditDialog::onStationTargetChanged(int) {
    if (!comboBox_stationTarget_ || loadingDefinition_)
        return;
    applyStationTargetToChannelProtocol();
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::onSendChannelChanged(int) {
    if (stationBindings_.configured)
        return;
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    fillProtocolComboForChannel(ui->comboBox_productProtocol, channel);
    updateProductProtocolRowVisible();
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::onProductProtocolChanged(int) {
    if (stationBindings_.configured)
        return;
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::onSendActionChanged(int) {
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::refreshDeviceCmdCombo() {
    const QString previousCmd = comboData(ui->comboBox_deviceCmd);
    TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const TestCaseSendAction action = sendActionFromComboData(comboData(ui->comboBox_action));
    QString device = comboData(ui->comboBox_productProtocol);
    // 绑定模式下以「目标设备」为准，避免隐藏的协议下拉未同步导致指令串设备
    if (stationBindings_.configured && comboBox_stationTarget_) {
        TestCaseSendChannel targetCh = channel;
        QString targetDev;
        if (parseStationTargetKey(comboBox_stationTarget_->currentData().toString(), targetCh, targetDev)) {
            channel = targetCh;
            device = targetDev;
        }
    }
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, channel, action, device, QString());
    int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, previousCmd);
    // 上一指令不属于当前设备时落到首项（产品通道为「不发送」）
    if (cmdIdx < 0 && ui->comboBox_deviceCmd->count() > 0)
        cmdIdx = 0;
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    onDeviceCmdChanged(cmdIdx);
}

void TestCaseEditDialog::onDeviceCmdChanged(int) {
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const QString cmdName = comboData(ui->comboBox_deviceCmd);
    const QString device = comboData(ui->comboBox_productProtocol);
    const TestCaseSendAction action = sendActionFromComboData(comboData(ui->comboBox_action));
    if (!loadingDefinition_)
        syncGateToSendCommand(QString());
    const SendCmdParamUi uiSchema = sendCmdParamUiForName(cmdName, channel, device);
    const bool hasParam = uiSchema.valid && uiSchema.kind != SendCmdParamKind::None;
    updateSendParamVisibility(hasParam);
    if (!uiSchema.valid) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::None) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::Int || uiSchema.kind == SendCmdParamKind::UInt) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramInt);
        if (channel == TestCaseSendChannel::Fixture) {
            ui->spinBox_intParam->setRange(0, 15);
            ui->spinBox_intParam->setSpecialValueText(QStringLiteral("当前工位"));
        } else {
            ui->spinBox_intParam->setSpecialValueText(QString());
        }
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    if (uiSchema.kind != SendCmdParamKind::JsonMap && uiSchema.kind != SendCmdParamKind::String) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam,
                               ui->spinBox_intParam, ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                               ui->pushButton_restoreParamDefaults, false);
        return;
    }
    ui->stackedWidget_param->setCurrentWidget(ui->page_paramJson);
    const QString cmdKey =
        sendChannelComboData(channel) + QLatin1Char('|') + device + QLatin1Char('|') + cmdName
        + QLatin1Char('|') + (action == TestCaseSendAction::Get ? QLatin1String("Get") : QLatin1String("Set"));
    const bool cmdChanged = (cmdKey != lastSendParamCmdKey_);
    lastSendParamCmdKey_ = cmdKey;
    const int expectCols = 2;
    // 指令切换时重置表格；同指令重复进入（如 setDefinition 后再调）则保留已填内容。
    // 加载定义中途勿用空 userMap 刷表，否则会盖掉随后回填的 ini 值（灰度一直停在模板默认）。
    if (!loadingDefinition_ && (cmdChanged || ui->tableWidget_sendParam->columnCount() != expectCols)) {
        if (uiSchema.kind == SendCmdParamKind::String)
            setSendParamTableFromString(ui->tableWidget_sendParam,
                                        sendParamDefaultStringForCmd(channel, cmdName));
        else
            applySendParamTableWithTemplate(ui->tableWidget_sendParam, channel, device, cmdName, {}, action);
    }
    const bool hasParamTemplate = !sendParamDefaultMapForCmd(channel, device, cmdName, action).isEmpty();
    applySendParamHintToUi(uiSchema, hasParam, ui->label_sendParamHint, ui->tableWidget_sendParam, ui->spinBox_intParam,
                           ui->pushButton_addParamRow, ui->pushButton_removeParamRow,
                           ui->pushButton_restoreParamDefaults, hasParamTemplate);
}

bool TestCaseEditDialog::saveValidated() {
    commitSendParamTableEdits(ui->tableWidget_sendParam);
    commitTableInlineEdits(tableWidget_multiGates_);
    materializeAllSendParamPlaceholdersForSave(ui->tableWidget_sendParam);
    const TestCaseDefinition def = definition();
    QStringList errors;
    if (!TestCaseValidator::validateCase(def, errors)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), errors.join(QStringLiteral("\r\n")));
        return false;
    }
    if (def.gate.enabled && def.gate.reportType == QLatin1String("ProtocolDongleSuctionPeakData")
        && def.gates.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("吸力卡控已启用，请至少在判定表格中勾选一项并填写上下限。"));
        return false;
    }
    if (stationKey_.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
                             QStringLiteral("请先在「测试流程编排」顶部选择工站，再保存步骤参数到该工站 profiles/.../steps/ 目录。"));
        return false;
    }
    QVector<TestFlowItemEntry> flowEntries = flowEntries_;
    if (flowEntries.isEmpty())
        flowEntries = TestCaseStore::loadStationFlowItems(stationKey_);
    QStringList flowErrors;
    if (!TestCaseValidator::validateFlowMesTags(stationKey_, flowEntries, flowErrors, originalCaseName_, &def)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), flowErrors.join(QStringLiteral("\r\n")));
        return false;
    }
    if (!TestCaseStore::saveCaseForStation(stationKey_, def)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入配置文件"));
        return false;
    }
    // 工站内改名：只清理本工站 profile 旧步骤文件，禁止删除总步骤库模板
    if (!originalCaseName_.isEmpty() && originalCaseName_ != def.meta.name) {
        const QString oldOverride = TestCasePaths::profileStepOverridePath(stationKey_, originalCaseName_);
        if (QFile::exists(oldOverride))
            QFile::remove(oldOverride);
    }
    return true;
}
