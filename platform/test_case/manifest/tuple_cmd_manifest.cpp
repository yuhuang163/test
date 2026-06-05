#include "tuple_cmd_manifest.h"

namespace {

using TupleCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

constexpr const char kHintLogin[] =
    u8"三元组云端登录（环境）：每环境单独 ini，如 三元组云端登录（prod）\n"
    u8"Param/baseUrl、Param/userName、Param/password；须在获取三元组之前执行";
constexpr const char kHintApply[] =
    u8"按 MAC 拉三元组：Param/mac 留空或 $MAC；Param/sku、Param/position 在用例 ini 配置\n"
    u8"示例：mac=$MAC，sku=PH9，position=L";
constexpr const char kHintDebugMac[] = u8"调试：mac、status\n示例：mac=AA1122334455\nstatus=2";

// 新增三元组指令：在此表加一行；实现见 qtupleservice.cpp set/get。
const Row kRows[] = {
    {TupleCmd::Login, "Login", u8"三元组云端登录", DeviceCmdParamKind::JsonMap, kHintLogin, kSet},
    {TupleCmd::ApplyTupleByMac, "ApplyTupleByMac", u8"云端三元组", DeviceCmdParamKind::JsonMap, kHintApply, kGet},
    {TupleCmd::DebugUpdateMacStatus, "DebugUpdateMacStatus", u8"调试 MAC 状态", DeviceCmdParamKind::JsonMap,
     kHintDebugMac, kSet},
    {TupleCmd::ReportWriteRecord, "ReportWriteRecord", u8"三元组写入记录", DeviceCmdParamKind::None,
     u8"无需参数；使用内存中已获取的三元组与测试结果", kSet},
};

}  // namespace

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

}  // namespace TupleCmdManifest
