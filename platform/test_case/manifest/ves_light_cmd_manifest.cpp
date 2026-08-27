#include "ves_light_cmd_manifest.h"

#include <QString>

namespace {

using VesLightCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;

constexpr const char kHintSetBrightness[] =
    u8"通道请选「治具通信」，治具协议选「VES光源」。单通道指令固定通道 1\r\n"
    u8"Param_brightness 亮度 0~255（步骤 ini 可调，默认 22）\r\n"
    u8"走工位「连接治具串口」，波特率 9600";

const Row kRows[] = {
    {VesLightCmd::SetBrightness, "SetBrightness", u8"设置CH1亮度", DeviceCmdParamKind::JsonMap, kHintSetBrightness, kSet,
     nullptr, nullptr},
};

} // namespace

namespace VesLightCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(VesLightCmd cmd) {
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

} // namespace VesLightCmdManifest
