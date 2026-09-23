#include "xwd_fixture_cmd_manifest.h"

using XwdRawFixtureCmdManifest::Row;

static constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;
static constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

static const Row kRows[] = {
    {XwdRawFixtureCmd::SendRaw, "SendRaw", u8"原文/十六进制收发", DeviceCmdParamKind::String,
     u8"设置：只下发；读取：下发后等回包。\r\n纯十六进制例：11 11 22 → 发三字节；含字母例：readonce 原文 UTF-8；"
     u8"需换行时写 readonce\\r\\n（配置有 \\r\\n 才带换行）\r\n"
     u8"开 Gate 读电流时连续采样：Param_sampleDurationMs / Param_sampleIntervalMs，期间任一值合格即通过\r\n"
     u8"文本应答（如 DOWN?/UP）：Gate Field=valueText Expected=OK，读取会等行回包",
     kBoth, "ProtocolMeasureData", "value"},
    {XwdRawFixtureCmd::QueryDown, "QueryDown", u8"检测下到位(DOWN?)", DeviceCmdParamKind::None,
     u8"发 DOWN?\\r\\n；到位回 OK\\r\\n，否则 NO\\r\\n。超时内轮询直至 OK。\r\n"
     u8"串口：主页「连接PLC治具串口」（PlcFixtureComPort / PlcFixtureBaudRate）。\r\n"
     u8"一拖多：各工位到齐后由 Leader 统一下发一次，其余工位同步放行。\r\n"
     u8"卡控 Field=valueText Expected=OK；Param_syncWaitMs 可调等待全员超时",
     kGet, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::MoveUp, "MoveUp", u8"测试完成上升(UP)", DeviceCmdParamKind::None,
     u8"发 UP\\r\\n；成功回 OK\\r\\n。\r\n"
     u8"串口：主页「连接PLC治具串口」。一拖多时同样统一下发一次。\r\n"
     u8"卡控 Field=valueText Expected=OK",
     kGet, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::ReadFloatCurrentA, "ReadFloatCurrentA", u8"读取电流(Air1/float·A)",
     DeviceCmdParamKind::None,
     u8"发 88 AE 00 11；回 FA FB + 大端 IEEE754 电流(A)，上报 mA。\r\n"
     u8"开 Gate 时连续采样：Param_sampleDurationMs / Param_sampleIntervalMs，期间任一值合格即通过。\r\n"
     u8"治具串口波特率用 XwdFixture/BaudRate（与 XWD 共用工位治具口）",
     kGet, "ProtocolMeasureData", "value"},
    {XwdRawFixtureCmd::ReadModbusVoltageCentivolt, "ReadModbusVoltageCentivolt",
     u8"读取电压(Air1/Modbus·0.01V)", DeviceCmdParamKind::JsonMap,
     u8"Modbus RTU FC03 读 1 寄存器：raw/100=V。\r\n"
     u8"Param_registerAddress：保持寄存器地址（抓包常用 1 与 4）。\r\n"
     u8"Param_slaveAddr：从站地址，默认 1。\r\n"
     u8"例：01 03 00 01 00 01 → 回 01 03 02 01 EE → 4.94V。\r\n"
     u8"开 Gate 时连续采样；卡控单位 V（Field=value）",
     kGet, "ProtocolMeasureData", "value"},
};

namespace XwdRawFixtureCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(XwdRawFixtureCmd cmd) {
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

} // namespace XwdRawFixtureCmdManifest
