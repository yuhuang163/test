#include "modbus_cmd_manifest.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

using ModbusCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;
constexpr uint8_t kBoth = TestCaseCmdManifest::kSendActionBoth;

const Row kRows[] = {
    {ModbusDeviceRoute::InovanceH5uTcp, "Connect", u8"连接 PLC", nullptr, kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "Disconnect", u8"断开 PLC", nullptr, kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "IsConnected", u8"是否已连接", nullptr, kGet},
    {ModbusDeviceRoute::InovanceH5uTcp, "ReadCoil", u8"读线圈", u8"M 地址（int）\r\n示例：210", kGet},
    {ModbusDeviceRoute::InovanceH5uTcp, "WriteCoil", u8"写线圈", u8"Param：m、value；1拖2可用 mLeft/mRight", kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "ReadCoils", u8"读多线圈", u8"PlcReadCoilsRequest：m、quantity", kGet},
    {ModbusDeviceRoute::InovanceH5uTcp, "WaitCoilTrue", u8"等待线圈置 1", u8"PlcWaitCoilRequest：m、timeoutMs", kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "WaitCoilFalse", u8"等待线圈置 0", u8"PlcWaitCoilRequest：m、timeoutMs", kSet},
    {ModbusDeviceRoute::InovanceH5uTcp, "SendStepDone", u8"步骤完成脉冲", nullptr, kSet},
    {ModbusDeviceRoute::GcSeriesTcp, "Connect", u8"连接 GC PLC", u8"Param：host、port、unitId（可选，默认读 GC_PLC/*）", kSet},
    {ModbusDeviceRoute::GcSeriesTcp, "Disconnect", u8"断开 GC PLC", nullptr, kSet},
    {ModbusDeviceRoute::GcSeriesTcp, "IsConnected", u8"GC PLC 是否已连接", nullptr, kGet},
    {ModbusDeviceRoute::GcSeriesTcp, "WriteCoil", u8"写 GC 线圈(M)",
     u8"Param：m/value 或 mLeft/mRight；协议地址=M+4096（GC_PLC/MCoilAddressOffset）", kSet},
    {ModbusDeviceRoute::HqAmmeterRtu, "ReadMeasurement", u8"读电流", nullptr, kGet, "ProtocolMeasureData", "value"},
    {ModbusDeviceRoute::HqAmmeterRtu, "SetBaud115200", u8"初始化波特率 115200", nullptr, kSet},
    {ModbusDeviceRoute::LxAmmeterRtu, "ReadMeasurement", u8"读电流", u8"机台号见 Current/LuxshareMachineId", kGet,
     "ProtocolMeasureData", "value"},
    {ModbusDeviceRoute::MultiTempLoggerRtu, "ReadChannelTemp", u8"读通道温度",
     u8"Param：channel=1~64，slaveAddr=1~247；或 channels=1,2,3 / 1-6\r\n"
     u8"法兰加热多点：channelsPerStation=6；stationsPerDevice=2 → 工位奇偶映射 CH1-6 / CH7-12\r\n"
     u8"一拖多共享：sharedPair=true；tempComName0/1…；tempBaudRate；sampleDurationMs=20000\r\n"
     u8"Get+Gate 且通道数>1：窗口内某一轮全部通道落入 Gate 温度范围则通过",
     kGet, "ProtocolMeasureData", "value"},
    {ModbusDeviceRoute::MultiTempLoggerRtu, "SendRaw", u8"原文/十六进制收发",
     u8"开放报文：Param_txHex 填完整 RTU 帧（含CRC，低字节在前）\r\n"
     u8"例读通道1温度：01 03 00 12 00 02 64 0E\r\n"
     u8"例读寄存器0起4个：01 03 00 00 00 04 44 09\r\n"
     u8"设置只下发；读取下发后等回包（FC03 且≥4字节数据时解析为 ℃）",
     kBoth},
    {ModbusDeviceRoute::XinjiePlcRtu, "Connect", u8"连接信捷 PLC RTU",
     u8"Param：comPort（可空，默认工位万用表串口或 XINJE_PLC/ComPort）、baudRate（默认19200）、slaveId、parity（默认even）", kSet},
    {ModbusDeviceRoute::XinjiePlcRtu, "Disconnect", u8"断开信捷 PLC RTU", nullptr, kSet},
    {ModbusDeviceRoute::XinjiePlcRtu, "IsConnected", u8"信捷 PLC 是否已连接", nullptr, kGet},
    {ModbusDeviceRoute::XinjiePlcRtu, "WriteCoil", u8"写线圈(M/Y/S/T/C)",
     u8"Param：address（如 M100、Y0）、value=true/false、expectReply（默认false只发不收）", kSet},
    {ModbusDeviceRoute::XinjiePlcRtu, "ReadCoils", u8"读线圈", u8"Param：address、quantity（默认1）", kGet},
    {ModbusDeviceRoute::XinjiePlcRtu, "ReadDiscreteInputs", u8"读离散输入(X)", u8"Param：address（如 X0）、quantity", kGet},
    {ModbusDeviceRoute::XinjiePlcRtu, "WriteRegister", u8"写保持寄存器(D/T/C)",
     u8"Param：address（如 D100）、value、expectReply（默认false只发不收）", kSet},
    {ModbusDeviceRoute::XinjiePlcRtu, "ReadHoldingRegisters", u8"读保持寄存器",
     u8"Param：address（如 D100）、quantity（默认1）", kGet},
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
