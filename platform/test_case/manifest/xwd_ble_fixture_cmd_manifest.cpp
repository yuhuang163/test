#include "xwd_ble_fixture_cmd_manifest.h"

namespace {

using XwdBleFixtureCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;

const Row kRows[] = {
    {XwdBleFixtureCmd::SendRaw, "SendRaw", u8"原文发送", DeviceCmdParamKind::String,
     u8"按 UTF-8 原文下发至治具串口（含 \\r\\n 需自行写入）", kSet},
};

} // namespace

namespace XwdBleFixtureCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(XwdBleFixtureCmd cmd) {
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

} // namespace XwdBleFixtureCmdManifest
