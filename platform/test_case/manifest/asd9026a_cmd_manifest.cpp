#include "asd9026a_cmd_manifest.h"

namespace {

using Asd9026aCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;
constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;

const Row kRows[] = {
    {Asd9026aCmd::ConfigureProgrammablePower, "ConfigureProgrammablePower", u8"配置模拟电池(恒压)",
     DeviceCmdParamKind::JsonMap,
     u8"语义：channel/voltage/current/currentRange；或填 txHex 覆盖整帧(含CRC，空格分隔)\r\n"
     u8"例 txHex=01 21 0D 0D 00 40 4B 4C 00 D0 07 00 00 01 01 00 00 A0 F1（5V/2A大档）",
     kSet},
    {Asd9026aCmd::ConfigureCurrentMeasureRange, "ConfigureCurrentMeasureRange", u8"切换电流测量量程",
     DeviceCmdParamKind::JsonMap,
     u8"语义：channel/currentRange；或填 txHex 覆盖整帧(含CRC)\r\n"
     u8"例小档 txHex=01 21 0D 0D 00 00 00 00 00 00 00 00 00 03 00 00 00 38 24",
     kSet},
    {Asd9026aCmd::ProgrammablePowerOutput, "ProgrammablePowerOutput", u8"输出开关", DeviceCmdParamKind::JsonMap,
     u8"语义：channel/enable=0|1；或填 txHex 覆盖整帧(含CRC)\r\n"
     u8"开：01 11 04 03 01 00 00 A8 C5；关：01 11 04 03 00 00 00 F9 05",
     kSet},
    {Asd9026aCmd::ReadProgrammableVoltage, "ReadProgrammableVoltage", u8"读取输出电压", DeviceCmdParamKind::JsonMap,
     u8"channel=1|2；可选 txHex 覆盖读状态帧", kGet},
    {Asd9026aCmd::ReadProgrammableCurrent, "ReadProgrammableCurrent", u8"读取输出电流", DeviceCmdParamKind::JsonMap,
     u8"channel=1|2；可选 txHex 覆盖读状态帧；Gate 开时连续采样", kGet},
    {Asd9026aCmd::SendRaw, "SendRaw", u8"原文/十六进制收发", DeviceCmdParamKind::String,
     u8"与治具同类：纯十六进制整帧(含CRC)例 01 11 04 03 01 00 00 A8 C5；设置只下发，读取下发后等回包",
     kBoth},
};

} // namespace

namespace Asd9026aCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(Asd9026aCmd cmd) {
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
    return nullptr;
}

} // namespace Asd9026aCmdManifest
