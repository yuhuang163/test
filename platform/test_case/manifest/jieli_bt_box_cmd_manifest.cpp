#include "jieli_bt_box_cmd_manifest.h"

using JieliBtBoxCmdManifest::Row;

static constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

static const Row kRows[] = {
    {JieliBtBoxCmd::WaitRfInfo, "WaitRfInfo", u8"等待频偏与RSSI上报", DeviceCmdParamKind::JsonMap,
     u8"走顶部菜单「连接治具串口」：上电后等待杰理盒子 TLV 上报，解析 T=7 频偏、T=8 RSSI（小端 int32）\r\n"
     u8"默认单路：不区分左右耳，任一侧上报即通过；本窗有 MAC 时仍按 T=1 匹配\r\n"
     u8"1拖2：Param_oneToTwo=true 且两窗同时测、无 MAC 时，按 MASK BIT0 左耳→小号工位、右耳→大号工位\r\n"
     u8"Param_baudRate 治具串口波特率（默认 460800）",
     kGet, "ProtocolJieliBtBoxData", "rssi"},
};

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
