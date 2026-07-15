#include "xwd_ble_fixture_cmd_manifest.h"

namespace {

using XwdBleFixtureCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;

const Row kRows[] = {
    {XwdBleFixtureCmd::SendRaw, "SendRaw", u8"原始字节发送", DeviceCmdParamKind::String,
     u8"十六进制原始字节，例：11 11 22 或 111122\n非 hex 内容按 UTF-8 原文下发", kSet},
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
