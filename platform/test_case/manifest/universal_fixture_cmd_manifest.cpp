#include "universal_fixture_cmd_manifest.h"

namespace {

using UniversalFixtureCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;
constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;

const Row kRows[] = {
    {UniversalFixtureCmd::CylinderDown, "CylinderDown", u8"气缸下压", DeviceCmdParamKind::JsonMap,
     u8"通用治具 V1.2：发 55 01 ss 05 00 01，等到位应答 0x81（STATUS=0）。\r\n"
     u8"SLOT 默认用工位号(1~15)；可 Param_slot 覆盖；广播用 BroadcastCylinderDown。\r\n"
     u8"串口：主页「连接治具串口」115200；超时建议 ≥2000ms",
     kSet},
    {UniversalFixtureCmd::CylinderUp, "CylinderUp", u8"气缸复位(抬起)", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 05 00 00，等到位应答 0x81。Param_slot 可选；广播用 BroadcastCylinderUp",
     kSet},
    {UniversalFixtureCmd::BroadcastCylinderDown, "BroadcastCylinderDown", u8"广播气缸下压", DeviceCmdParamKind::None,
     u8"SLOT=00 广播下压；各工位分别回 0x81。超时建议 ≥2000ms", kSet},
    {UniversalFixtureCmd::BroadcastCylinderUp, "BroadcastCylinderUp", u8"广播气缸抬起", DeviceCmdParamKind::None,
     u8"SLOT=00 广播抬起；收齐各工位 0x81 成功帧后才算完成", kSet},
    {UniversalFixtureCmd::Relay1On, "Relay1On", u8"继电器1吸合", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 01 00 01，等 0x81。Param_slot 可选", kSet},
    {UniversalFixtureCmd::Relay1Off, "Relay1Off", u8"继电器1断开", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 01 00 00，等 0x81", kSet},
    {UniversalFixtureCmd::Relay2On, "Relay2On", u8"继电器2吸合", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 02 00 01，等 0x81", kSet},
    {UniversalFixtureCmd::Relay2Off, "Relay2Off", u8"继电器2断开", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 02 00 00，等 0x81", kSet},
    {UniversalFixtureCmd::Relay3On, "Relay3On", u8"继电器3吸合", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 03 00 01，等 0x81", kSet},
    {UniversalFixtureCmd::Relay3Off, "Relay3Off", u8"继电器3断开", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 03 00 00，等 0x81", kSet},
    {UniversalFixtureCmd::Relay4On, "Relay4On", u8"继电器4吸合", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 04 00 01，等 0x81", kSet},
    {UniversalFixtureCmd::Relay4Off, "Relay4Off", u8"继电器4断开", DeviceCmdParamKind::JsonMap,
     u8"发 55 01 ss 04 00 00，等 0x81", kSet},
    {UniversalFixtureCmd::ReadCurrent, "ReadCurrent", u8"读取电流", DeviceCmdParamKind::JsonMap,
     u8"发 55 02 ss ch 00 01，等 0x82。\r\n"
     u8"Param_ch：1=CH1（默认）2=CH2 0=全部通道（多帧）。Param_slot 可选。\r\n"
     u8"卡控 ReportType=ProtocolMeasureData Field=value（mA）",
     kGet, "ProtocolMeasureData", "value"},
    {UniversalFixtureCmd::SendRaw, "SendRaw", u8"原文/十六进制收发", DeviceCmdParamKind::String,
     u8"设置：只下发；读取：下发后等回包。纯十六进制例：55 01 01 05 00 01",
     kBoth, "ProtocolMeasureData", "valueText"},
};

} // namespace

namespace UniversalFixtureCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(UniversalFixtureCmd cmd) {
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

} // namespace UniversalFixtureCmdManifest
