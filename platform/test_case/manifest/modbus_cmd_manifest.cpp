#include "modbus_cmd_manifest.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

using ModbusCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

const Row kRows[] = {
    {ModbusDeviceRoute::InovanceH5uTcp, "Connect", u8"连接 PLC", nullptr, kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "Disconnect", u8"断开 PLC", nullptr, kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "IsConnected", u8"是否已连接", nullptr, kGet},
    {ModbusDeviceRoute::InovanceH5uTcp, "ReadCoil", u8"读线圈", u8"M 地址（int）\n示例：210", kGet},
    {ModbusDeviceRoute::InovanceH5uTcp, "WriteCoil", u8"写线圈", u8"PlcCoilRequest JSON：m、value", kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "ReadCoils", u8"读多线圈", u8"PlcReadCoilsRequest：m、quantity", kGet},
    {ModbusDeviceRoute::InovanceH5uTcp, "WaitCoilTrue", u8"等待线圈置 1", u8"PlcWaitCoilRequest：m、timeoutMs", kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "WaitCoilFalse", u8"等待线圈置 0", u8"PlcWaitCoilRequest：m、timeoutMs", kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "SendStepDone", u8"步骤完成脉冲", nullptr, kSet},
    {ModbusDeviceRoute::HqAmmeterRtu, "ReadMeasurement", u8"读电流", nullptr, kGet},
    {ModbusDeviceRoute::HqAmmeterRtu, "SetBaud115200", u8"初始化波特率 115200", nullptr, kSet},
    {ModbusDeviceRoute::LxAmmeterRtu, "ReadMeasurement", u8"读电流", u8"机台号见 Current/LuxshareMachineId", kGet},
};

} // namespace

namespace ModbusCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByDeviceAndName(ModbusDeviceRoute device, const QString& enumName) {
    for (int i = 0; i < rowCount(); ++i) {
        if (kRows[i].device == device && QString::fromLatin1(kRows[i].enumName).compare(enumName, Qt::CaseInsensitive) == 0) {
            return &kRows[i];
        }
    }
    return nullptr;
}

} // namespace ModbusCmdManifest
