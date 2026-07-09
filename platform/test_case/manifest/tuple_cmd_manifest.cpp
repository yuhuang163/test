#include "tuple_cmd_manifest.h"

namespace {

using TupleCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintLogin[] =
    u8"三元组云端登录（环境）：每环境单独 ini，如 三元组云端登录（prod）\n"
    u8"Param_baseUrl、Param_userName、Param_password；须在获取三元组之前执行";
constexpr const char kHintApply[] =
    u8"按 MAC 拉三元组：Param_mac 留空或 $MAC；Param_sku、Param_position 在用例 ini 配置\n"
    u8"示例：mac=$MAC，sku=PH9，position=L";
constexpr const char kHintDebugMac[] =
    u8"上报烧录状态（/api/mac-addresses）：Param_mac=$MAC；Param_status=1 烧录工站、2 蓝牙工站\n"
    u8"可选 Param_sn=$SN；示例：Param_mac=$MAC Param_status=2";

// 新增三元组指令：在此表加一行；实现见 qtupleservice.cpp set/get。
const Row kRows[] = {
    {TupleCmd::Login, "Login", u8"三元组云端登录", DeviceCmdParamKind::JsonMap, kHintLogin, kSet},
    {TupleCmd::ApplyTupleByMac, "ApplyTupleByMac", u8"云端三元组", DeviceCmdParamKind::JsonMap, kHintApply, kGet},
    {TupleCmd::DebugUpdateMacStatus, "DebugUpdateMacStatus", u8"调试 MAC 状态", DeviceCmdParamKind::JsonMap,
     kHintDebugMac, kSet},
    {TupleCmd::ReportWriteRecord, "ReportWriteRecord", u8"检验数据上报", DeviceCmdParamKind::None,
     u8"无参数：有三元组时上报读三元组/RSSI/版本；M8 烧录等无三元组时按 SN+MAC 上报写入MAC（/api/inspection/report）", kSet},
};

} // namespace

namespace TupleCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(TupleCmd cmd) {
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

} // namespace TupleCmdManifest
