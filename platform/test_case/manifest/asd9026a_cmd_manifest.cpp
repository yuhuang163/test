#include "asd9026a_cmd_manifest.h"

namespace {

using Asd9026aCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;
constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;

const Row kRows[] = {
    {Asd9026aCmd::ConfigureProgrammablePower, "ConfigureProgrammablePower", u8"配置模拟电池(恒压)",
     DeviceCmdParamKind::JsonMap,
     u8"语义：voltage/current/currentRange；通道按工位 index 自动取 1/2\r\n"
     u8"txHex 首字节和 CRC 发送前自动按工位改写；例 02 21 10 0D 00 00 3D 09 00 00 00 07 D0 04 01 00 00 91 8F",
     kSet},
    {Asd9026aCmd::ConfigureCurrentMeasureRange, "ConfigureCurrentMeasureRange", u8"切换电流测量量程",
     DeviceCmdParamKind::JsonMap,
     u8"语义：currentRange；通道按工位 index 自动取 1/2；txHex 地址和 CRC 自动改写\r\n"
     u8"例自动档 txHex=02 21 10 0D 00 00 00 00 00 00 00 00 00 04 00 00 00 14 A9",
     kSet},
    {Asd9026aCmd::ProgrammablePowerOutput, "ProgrammablePowerOutput", u8"输出开关", DeviceCmdParamKind::JsonMap,
     u8"语义：enable=0|1；通道按工位 index 自动取 1/2；txHex 地址和 CRC 自动改写\r\n"
     u8"模板开：02 11 04 03 01 00 00 9B C5；关：02 11 04 03 00 00 00 CA 05",
     kSet},
    {Asd9026aCmd::ReadProgrammableVoltage, "ReadProgrammableVoltage", u8"读取输出电压", DeviceCmdParamKind::JsonMap,
     u8"通道按工位 index 自动取 1/2；可选 txHex 覆盖读状态帧（地址和 CRC 自动改写）", kGet, "ProtocolMeasureData",
     "value"},
    {Asd9026aCmd::ReadProgrammableCurrent, "ReadProgrammableCurrent", u8"读取输出电流", DeviceCmdParamKind::JsonMap,
     u8"通道按工位 index 自动取 1/2；可选 txHex 覆盖读状态帧；Gate 开时连续采样", kGet, "ProtocolMeasureData", "value"},
    {Asd9026aCmd::SendRaw, "SendRaw", u8"原文/十六进制收发", DeviceCmdParamKind::String,
     u8"与治具同类：纯十六进制整帧(含CRC)例 02 11 04 03 01 00 00 9B C5；设置只下发，读取下发后等回包",
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
