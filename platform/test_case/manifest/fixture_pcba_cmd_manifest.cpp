#include "fixture_pcba_cmd_manifest.h"

namespace {

using FixturePcbaCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

// 新增治具指令：在此表加一行；执行见 qfreework_test_case / fixture 协议。
const Row kRows[] = {
    {FixturePcbaCmd::StartTest, "StartTest", u8"开始测试（AA）", DeviceCmdParamKind::Int,
     u8"开始测试 AA+机位：0 或 $INDEX=当前工位号(getIndex)，1~15=指定机位", kSet},
    {FixturePcbaCmd::StartSleep, "StartSleep", u8"开始休眠（CC）", DeviceCmdParamKind::Int,
     u8"开始休眠 CC+机位：0 或 $INDEX=当前工位号，1~15=指定机位", kSet},
    {FixturePcbaCmd::StartWhiteMode, "StartWhiteMode", u8"亮白模式（EE）", DeviceCmdParamKind::Int,
     u8"亮白模式 EE+机位：0 或 $INDEX=当前工位号，1~15=指定机位", kSet},
    {FixturePcbaCmd::WaitFixturePacket, "WaitFixturePacket", u8"等待治具数据长包", DeviceCmdParamKind::None,
     u8"等待 0x55 治具长包（电流/按键等），回传类型选 ProtocolFixturePcbaData", kGet},
    {FixturePcbaCmd::WaitStartTestAck, "WaitStartTestAck", u8"等待开始测试应答", DeviceCmdParamKind::None,
     u8"等待治具短包「开始测试」应答（55 AA … AA AA）", kGet},
    {FixturePcbaCmd::WaitSleepRequest, "WaitSleepRequest", u8"等待休眠请求", DeviceCmdParamKind::None,
     u8"等待治具短包「请求休眠」（55 xx 05 CC AA）", kGet},
    {FixturePcbaCmd::WaitWorkCurrentDoneAck, "WaitWorkCurrentDoneAck", u8"等待工作电流测量完成",
     DeviceCmdParamKind::None, u8"等待治具短包 55 01 05 CC AA（工作电流测量完成）", kGet},
};

} // namespace

namespace FixturePcbaCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(FixturePcbaCmd cmd) {
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

} // namespace FixturePcbaCmdManifest
