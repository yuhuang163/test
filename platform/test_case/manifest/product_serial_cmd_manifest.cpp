#include "product_serial_cmd_manifest.h"

namespace {

using ProductSerialCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;

// 新增产品串口指令：在此表加一行；执行见工站串口/CMW 相关逻辑。
const Row kRows[] = {
    {ProductSerialCmd::InstrumentReset, "InstrumentReset", u8"仪器复位应答",
     u8"经产品串口发复位帧，等待仪器应答 040E0405030C00", kSet},
    {ProductSerialCmd::StartRx2402Ble1M, "StartRx2402Ble1M", u8"开始接收 2402 BLE1M",
     u8"开始接收 2402MHz BLE 1M，等待应答 040E0405332000", kSet},
    {ProductSerialCmd::StartRx2440Ble1M, "StartRx2440Ble1M", u8"开始接收 2440 BLE1M",
     u8"开始接收 2440MHz BLE 1M，等待应答 040E0405332000", kSet},
    {ProductSerialCmd::StartRx2480Ble1M, "StartRx2480Ble1M", u8"开始接收 2480 BLE1M",
     u8"开始接收 2480MHz BLE 1M，等待应答 040E0405332000", kSet},
    {ProductSerialCmd::StartRx2402Ble2M, "StartRx2402Ble2M", u8"开始接收 2402 BLE2M",
     u8"开始接收 2402MHz BLE 2M，等待应答 040E0405332000", kSet},
    {ProductSerialCmd::StartRx2440Ble2M, "StartRx2440Ble2M", u8"开始接收 2440 BLE2M",
     u8"开始接收 2440MHz BLE 2M，等待应答 040E0405332000", kSet},
    {ProductSerialCmd::StartRx2480Ble2M, "StartRx2480Ble2M", u8"开始接收 2480 BLE2M",
     u8"开始接收 2480MHz BLE 2M，等待应答 040E0405332000", kSet},
    {ProductSerialCmd::StopRxAndPer, "StopRxAndPer", u8"停止接收与 PER",
     u8"停止接收并统计 PER；可与前序「开始接收」及并联 CMW 配置配合", kSet},
};

}  // namespace

namespace ProductSerialCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByCmd(ProductSerialCmd cmd) {
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

}  // namespace ProductSerialCmdManifest
