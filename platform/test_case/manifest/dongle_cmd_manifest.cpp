#include "dongle_cmd_manifest.h"

#include <QHash>

namespace {

using DongleCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintBleScan[] =
    u8"蓝牙 MAC：留空或 $MAC = 当前工位 MAC\n示例：Param_string=$MAC";
constexpr const char kHintBomb[] =
    u8"广播注入：deviceName,rssi,connectionInterval,command\n按 Dongle 协议填 JSON 或 name=value";

// 新增 Dongle 指令：在此表加一行；实现见 qat.cpp set/get。
const Row kRows[] = {
    {DongleCmd::BleScanConnect, "BleScanConnect", u8"扫描连接蓝牙", DeviceCmdParamKind::String, kHintBleScan, kSet},
    {DongleCmd::BleScanConnectByName, "BleScanConnectByName", u8"按广播名自动连接", DeviceCmdParamKind::JsonMap, u8"填写包含 name 和 rssi 的 JSON\n示例: {\"name\":\"Pump-E\", \"rssi\":-50}", kSet},
    {DongleCmd::BleDisconnect, "BleDisconnect", u8"断开蓝牙连接", DeviceCmdParamKind::None,
     u8"下发 AT+MAC=00:00:00:00:00:00，主动断开 dongle 与产品 BLE", kSet},
    {DongleCmd::BleDirectConnect, "BleDirectConnect", u8"直连蓝牙", DeviceCmdParamKind::String, kHintBleScan, kSet},
    {DongleCmd::BleOtaConnect, "BleOtaConnect", u8"OTA 蓝牙连接", DeviceCmdParamKind::String, kHintBleScan, kSet},
    {DongleCmd::BleAppConnect, "BleAppConnect", u8"App 蓝牙连接", DeviceCmdParamKind::String, kHintBleScan, kSet},
    {DongleCmd::BleMainConnect, "BleMainConnect", u8"主通道蓝牙连接", DeviceCmdParamKind::String, kHintBleScan, kSet},
    {DongleCmd::OtaDataPassthrough, "OtaDataPassthrough", u8"OTA 数据透传", DeviceCmdParamKind::Int, u8"0=关 1=开", kSet},
    {DongleCmd::OtaPktSize, "OtaPktSize", u8"OTA 切包大小", DeviceCmdParamKind::Int, u8"AT+OTAPKTSIZE，字节如 200", kSet},
    {DongleCmd::BleMtu, "BleMtu", u8"BLE MTU", DeviceCmdParamKind::Int, u8"AT+BLEMTU，字节如 247", kSet},
    {DongleCmd::MainDataPassthrough, "MainDataPassthrough", u8"主通道数据透传", DeviceCmdParamKind::Int, u8"0=关 1=开", kSet},
    {DongleCmd::BleLog, "BleLog", u8"BLE 日志开关", DeviceCmdParamKind::Int, u8"0=关 1=开", kSet},
    {DongleCmd::BleDeviceLog, "BleDeviceLog", u8"BLE 设备日志开关", DeviceCmdParamKind::Int, u8"0=关 1=开", kSet},
    {DongleCmd::GetSuction, "GetSuction", u8"吸力读取开关", DeviceCmdParamKind::Int, u8"0=关 1=开（AT+SUCTION）", kSet},
    {DongleCmd::SampleSuctionDual, "SampleSuctionDual", u8"采集双通道吸力", DeviceCmdParamKind::JsonMap,
     u8"可选：sampleDurationMs / sampleIntervalMs\nCH1/CH2 峰值与峰差请在 Gate（Dongle吸力峰值）中配置（同 BYD 双通道：两口最低值 + |差|）",
     kGet},
    {DongleCmd::SampleSuctionSingle, "SampleSuctionSingle", u8"采集单通道吸力", DeviceCmdParamKind::JsonMap,
     u8"可选：channel=1|2|3、sampleDurationMs/IntervalMs；峰识别可选 peakBaselineKpa/peakDipStartKpa\n"
     u8"Gate：peakKpa=各周期峰值均须在范围内；peakDiffKpa=最大峰值-最小峰值（不是窗口绝对最高-最低）",
     kGet},
    {DongleCmd::AdcSwitch, "AdcSwitch", u8"ADC 上报开关", DeviceCmdParamKind::Int, u8"0=关 1=开（AT+ADC）", kSet},
    {DongleCmd::Bomb, "Bomb", u8"广播注入", DeviceCmdParamKind::JsonMap, kHintBomb, kSet},
    {DongleCmd::GetGmac, "GetGmac", u8"GMAC", DeviceCmdParamKind::None, u8"无需参数", kGet},
};

// 旧 ini 兼容：DongleBleScanConnect 等
const QHash<QString, DongleCmd> kLegacyNameMap = {
    {QStringLiteral("DongleBleScanConnect"), DongleCmd::BleScanConnect},
    {QStringLiteral("DongleBleDirectConnect"), DongleCmd::BleDirectConnect},
    {QStringLiteral("DongleBleOtaConnect"), DongleCmd::BleOtaConnect},
    {QStringLiteral("DongleBleAppConnect"), DongleCmd::BleAppConnect},
    {QStringLiteral("DongleBleMainConnect"), DongleCmd::BleMainConnect},
    {QStringLiteral("DongleOtaDataPassthrough"), DongleCmd::OtaDataPassthrough},
    {QStringLiteral("DongleOtaPktSize"), DongleCmd::OtaPktSize},
    {QStringLiteral("DongleBleMtu"), DongleCmd::BleMtu},
    {QStringLiteral("DongleMainDataPassthrough"), DongleCmd::MainDataPassthrough},
    {QStringLiteral("DongleBleLog"), DongleCmd::BleLog},
    {QStringLiteral("DongleBleDeviceLog"), DongleCmd::BleDeviceLog},
    {QStringLiteral("DongleGetSuction"), DongleCmd::GetSuction},
    {QStringLiteral("DongleAdcSwitch"), DongleCmd::AdcSwitch},
    {QStringLiteral("DongleBomb"), DongleCmd::Bomb},
    {QStringLiteral("DongleGetGmac"), DongleCmd::GetGmac},
};

} // namespace

namespace DongleCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(DongleCmd cmd) {
    for (const Row& row : kRows) {
        if (row.cmd == cmd)
            return &row;
    }
    return nullptr;
}

const Row* findByEnumName(const QString& enumName) {
    const QString trimmed = enumName.trimmed();
    for (const Row& row : kRows) {
        if (QString::fromLatin1(row.enumName) == trimmed)
            return &row;
    }
    const auto legacy = kLegacyNameMap.constFind(trimmed);
    if (legacy != kLegacyNameMap.cend())
        return findByCmd(legacy.value());
    return nullptr;
}

} // namespace DongleCmdManifest
