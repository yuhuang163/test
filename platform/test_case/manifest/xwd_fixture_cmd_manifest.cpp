#include "xwd_fixture_cmd_manifest.h"

namespace {

using XwdRawFixtureCmdManifest::Row;

constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;

const Row kRows[] = {
    {XwdRawFixtureCmd::SendRaw, "SendRaw", u8"原文/十六进制收发", DeviceCmdParamKind::String,
     u8"设置：只下发；读取：下发后等回包。\r\n纯十六进制例：11 11 22 → 发三字节；含字母例：readonce → 原文 UTF-8",
     kBoth},
};

} // namespace

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
