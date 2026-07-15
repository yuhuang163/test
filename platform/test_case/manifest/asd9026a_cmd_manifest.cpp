#include "asd9026a_cmd_manifest.h"

namespace {

using Asd9026aCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

const Row kRows[] = {
    {Asd9026aCmd::ConfigureProgrammablePower, "ConfigureProgrammablePower", u8"配置模拟电池(恒压)",
     DeviceCmdParamKind::JsonMap, u8"channel=1|2，voltage(V)，current(A)", kSet},
    {Asd9026aCmd::ProgrammablePowerOutput, "ProgrammablePowerOutput", u8"输出开关", DeviceCmdParamKind::JsonMap,
     u8"channel=1|2，enable=0|1", kSet},
    {Asd9026aCmd::ReadProgrammableVoltage, "ReadProgrammableVoltage", u8"读取输出电压", DeviceCmdParamKind::JsonMap,
     u8"channel=1|2", kGet},
    {Asd9026aCmd::ReadProgrammableCurrent, "ReadProgrammableCurrent", u8"读取输出电流", DeviceCmdParamKind::JsonMap,
     u8"channel=1|2", kGet},
};

} // namespace

namespace Asd9026aCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(Asd9026aCmd cmd) {
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

} // namespace Asd9026aCmdManifest
