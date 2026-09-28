#include "xwd_fixture_cmd_manifest.h"

using XwdRawFixtureCmdManifest::Row;

static constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;
static constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;
static constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;

static const Row kRows[] = {
    {XwdRawFixtureCmd::SendRaw, "SendRaw", u8"原文/十六进制收发", DeviceCmdParamKind::String,
     u8"设置：只下发；读取：下发后等回包。\r\n纯十六进制例：11 11 22 → 发三字节；含字母例：readonce 原文 UTF-8；"
     u8"需换行时写 readonce\\r\\n（配置有 \\r\\n 才带换行）\r\n"
     u8"开 Gate 读电流时连续采样：Param_sampleDurationMs / Param_sampleIntervalMs，期间任一值合格即通过\r\n"
     u8"文本应答（如 DOWN?/UP/SET_UP/SET_ALL_UP）：Gate Field=valueText",
     kBoth, "ProtocolMeasureData", "value"},
    {XwdRawFixtureCmd::QueryDown, "QueryDown", u8"检测下到位(DOWN?)", DeviceCmdParamKind::None,
     u8"发 DOWN?\\r\\n；一拖多全部下压后回 ALL_DOWN_OK（另有 NO / n#DOWN_OK）。超时内轮询直至 ALL_DOWN_OK。\r\n"
     u8"串口：主页「连接PLC治具串口」（PlcFixtureComPort，115200 8N1）。\r\n"
     u8"一拖多：各工位到齐后由 Leader 统一下发一次。\r\n"
     u8"卡控 Field=valueText Expected=ALL_DOWN_OK；Param_syncWaitMs 可调等待全员超时",
     kGet, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::Up, "Up", u8"测试完成上升(UP)", DeviceCmdParamKind::None,
     u8"发 UP\\r\\n（原单台/旧固件协议），期望回 OK。\r\n"
     u8"串口：主页「连接PLC治具串口」115200 8N1；一拖多时统一下发一次。\r\n"
     u8"卡控 Field=valueText Expected=OK；亦可选 SetUp(SET_UP) / MoveUp(SET_ALL_UP)",
     kBoth, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::SetUp, "SetUp", u8"测试完成上升(SET_UP)", DeviceCmdParamKind::None,
     u8"发 SET_UP\\r\\n，期望回 OK。\r\n"
     u8"串口：主页「连接PLC治具串口」115200 8N1；一拖多时统一下发一次。\r\n"
     u8"卡控 Field=valueText Expected=OK；一拖多全抬请用 MoveUp(SET_ALL_UP)",
     kBoth, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::MoveUp, "MoveUp", u8"测试完成上升(SET_ALL_UP)", DeviceCmdParamKind::None,
     u8"发 SET_ALL_UP\\r\\n（不分大小写）。\r\n"
     u8"串口：主页「连接PLC治具串口」115200 8N1。一拖多时统一下发一次。\r\n"
     u8"成功区与失败区抬起步骤名可不同，按 SET_ALL_UP 指令同会话同步。\r\n"
     u8"卡控 Field=valueText Expected=OK（有回包时）；单台亦可选 Up(UP) / SetUp(SET_UP)",
     kBoth, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::Relay1On, "Relay1On", u8"继电器1接通(RELAY_1_ON)", DeviceCmdParamKind::None,
     u8"按工位号立即发 RELAY_N_ON\\r\\n（工位1→继电器1，工位2→继电器2），不等待其它工站。\r\n"
     u8"走「连接PLC治具串口」共享口。一拖多流程只需配本条，2号工站自动切继电器2。", kSet},
    {XwdRawFixtureCmd::Relay1Off, "Relay1Off", u8"继电器1断开(RELAY_1_OFF)", DeviceCmdParamKind::None,
     u8"按工位号立即发 RELAY_N_OFF\\r\\n（工位1→继电器1，工位2→继电器2），不等待其它工站。\r\n"
     u8"走「连接PLC治具串口」共享口。一拖多流程只需配本条。", kSet},
    {XwdRawFixtureCmd::Relay2On, "Relay2On", u8"继电器2接通(RELAY_2_ON)", DeviceCmdParamKind::None,
     u8"与 Relay1On 相同：仍按工位号映射 RELAY_N_ON（便于单工位调试显式选继电器2）。不等待其它工站。", kSet},
    {XwdRawFixtureCmd::Relay2Off, "Relay2Off", u8"继电器2断开(RELAY_2_OFF)", DeviceCmdParamKind::None,
     u8"与 Relay1Off 相同：仍按工位号映射 RELAY_N_OFF。不等待其它工站。", kSet},
    {XwdRawFixtureCmd::LoadOn, "LoadOn", u8"负载接通(LOAD_ON)", DeviceCmdParamKind::None,
     u8"发 LOAD_ON\\r\\n（控制板文本指令）。走 PLC 治具共享串口 115200 8N1；一拖多统一下发一次。", kSet},
    {XwdRawFixtureCmd::LoadOff, "LoadOff", u8"负载断开(LOAD_OFF)", DeviceCmdParamKind::None,
     u8"发 LOAD_OFF\\r\\n。走 PLC 治具共享串口；一拖多统一下发一次。", kSet},
    {XwdRawFixtureCmd::AddLoad1, "AddLoad1", u8"加负载1", DeviceCmdParamKind::None,
     u8"工位治具口发 Modbus：01 10 00 2B 00 02 04 00 08 03 E8 31 78\r\n"
     u8"波特率见 XwdFixture/BaudRate / 工位治具串口。",
     kSet},
    {XwdRawFixtureCmd::AddLoad2, "AddLoad2", u8"加负载2", DeviceCmdParamKind::None,
     u8"工位治具口发 Modbus：01 10 00 2F 00 02 04 00 01 03 E8 E0 89",
     kSet},
    {XwdRawFixtureCmd::CloseLoad1, "CloseLoad1", u8"关闭负载1", DeviceCmdParamKind::None,
     u8"工位治具口发 Modbus：01 10 00 2F 00 02 04 00 01 00 00 E0 37",
     kSet},
    {XwdRawFixtureCmd::CloseLoad2, "CloseLoad2", u8"关闭负载2", DeviceCmdParamKind::None,
     u8"工位治具口发 Modbus：01 10 00 2B 00 02 04 00 08 00 00 31 C6",
     kSet},
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
