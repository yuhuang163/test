#include "jieli_bt_box_cmd_manifest.h"

namespace {

using JieliBtBoxCmdManifest::Row;

constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

const Row kRows[] = {
    {JieliBtBoxCmd::WaitRfInfo, "WaitRfInfo", u8"等待频偏与RSSI上报", DeviceCmdParamKind::None,
     u8"走工位「产品串口(仪器)」：上电后等待杰理盒子 TLV 上报，解析 T=7 频偏、T=8 RSSI（小端 int32）", kGet},
};

} // namespace

namespace JieliBtBoxCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(JieliBtBoxCmd cmd) {
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

} // namespace JieliBtBoxCmdManifest
