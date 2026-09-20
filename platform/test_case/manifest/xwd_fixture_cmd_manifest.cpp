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
     u8"串口：115200 8N1；卡控 Field=valueText Expected=OK",
     kGet, "ProtocolMeasureData", "valueText"},
    {XwdRawFixtureCmd::MoveUp, "MoveUp", u8"测试完成上升(UP)", DeviceCmdParamKind::None,
     u8"发 UP\\r\\n；成功回 OK\\r\\n。\r\n串口：115200 8N1；卡控 Field=valueText Expected=OK",
     kGet, "ProtocolMeasureData", "valueText"},
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
