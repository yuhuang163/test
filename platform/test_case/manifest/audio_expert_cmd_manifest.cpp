#include "audio_expert_cmd_manifest.h"

#include <QString>

namespace {

using AudioExpertCmdManifest::Row;

constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintRunTest[] =
    u8"通道请选「软件通信」，软件协议选「AudioExpert(TCP)」\r\n"
    u8"通过 IP 连接 AudioExpert Remote：发指令控制测试，等到 $test_over 取 data\r\n"
    u8"Param_host 主机（默认 127.0.0.1）；Param_port 必填（Remote 通讯端口）\r\n"
    u8"Param_command 发送指令（默认 $test#）；帧=4 字节大端长度 + UTF-8\r\n"
    u8"Param_connectTimeoutMs 连接超时；Param_readTimeoutMs 等待 $test_over（音频宜 ≥120000）\r\n"
    u8"Param_requireOk=1 时要求 Result/Status 为 OK；步骤超时建议同步加大\r\n"
    u8"回包 data 解析后结果表：测试项名=Data Info，实测=数值+单位\r\n"
    u8"卡控：pass、value~value4（对应第1~4项）；原文见 dataText";

const Row kRows[] = {
    {AudioExpertCmd::RunTest, "RunTest", u8"运行测试（发指令等$test_over）", DeviceCmdParamKind::JsonMap,
     kHintRunTest, kGet, "ProtocolAudioExpertData", "value"},
};

} // namespace

namespace AudioExpertCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(AudioExpertCmd cmd) {
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

} // namespace AudioExpertCmdManifest
