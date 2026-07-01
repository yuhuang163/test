#include "scpi_cmd_manifest.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

using ScpiCmdManifest::Row;

constexpr uint8_t kSet = TestCaseCmdManifest::kSendActionSet;
constexpr uint8_t kGet = TestCaseCmdManifest::kSendActionGet;

const Row kRows[] = {
    // HuilingWfp60h
    {ScpiDeviceRoute::HuilingWfp60h, "ConfigureMeasure", u8"配置测量参数", nullptr, kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "ReadMeasureCurrent", u8"读取电流测量值", nullptr, kGet},
    {ScpiDeviceRoute::HuilingWfp60h, "ReadMeasureConfiguration", u8"读取测量配置", nullptr, kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "InitializeDevice", u8"初始化设备", nullptr, kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "ConfigureProgrammablePower", u8"配置源通道属性",
     u8"visaAddress(VISA地址，必填，后续开关步骤可复用); voltage(V); current(A); scpiSetVoltageCmd 等 SCPI 模板可选", kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "ProgrammablePowerOutput", u8"源通道输出开关",
     u8"int=1开/0关（须先执行配置步骤；无需重复填 visaAddress）", kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "ReadProgrammableVoltage", u8"读取源电压测量值",
     u8"复用配置步骤的 VISA 连接；或填写 visaAddress", kGet},
    {ScpiDeviceRoute::HuilingWfp60h, "ReadProgrammableCurrent", u8"读取源电流测量值",
     u8"复用配置步骤的 VISA 连接；或填写 visaAddress", kGet},
    {ScpiDeviceRoute::HuilingWfp60h, "InitializeProgrammablePower", u8"初始化源通道",
     u8"复用配置步骤；或填写 visaAddress、voltage/current", kGet},
    {ScpiDeviceRoute::HuilingWfp60h, "SendRawLine", u8"发送原始命令", u8"原始文本命令", kSet},

    // RsCmw100
    {ScpiDeviceRoute::RsCmw100, "ClearStatus", u8"清除状态", nullptr, kSet},
    {ScpiDeviceRoute::RsCmw100, "GenOff", u8"关闭射频源", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "GenOn", u8"开启射频源", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "ListOff", u8"关闭列表模式", nullptr, kSet},
    {ScpiDeviceRoute::RsCmw100, "BbModeArb", u8"设置基带ARB模式", nullptr, kSet},
    {ScpiDeviceRoute::RsCmw100, "ArbFile", u8"载入ARB波形文件", u8"波形名称（例如: BLE1M）", kSet},
    {ScpiDeviceRoute::RsCmw100, "ArbRepetition", u8"设置ARB重复模式", nullptr, kSet},
    {ScpiDeviceRoute::RsCmw100, "ArbCycles", u8"设置ARB周期数", u8"周期数", kSet},
    {ScpiDeviceRoute::RsCmw100, "TxLevelDbm", u8"设置发射功率(dBm)", u8"dBm值", kSet},
    {ScpiDeviceRoute::RsCmw100, "FrequencyMhz", u8"设置发射频率(MHz)", u8"MHz值", kSet},
    {ScpiDeviceRoute::RsCmw100, "ManualArbTrigger", u8"手动ARB触发", nullptr, kSet},
    {ScpiDeviceRoute::RsCmw100, "WriteLine", u8"写入SCPI指令", u8"SCPI命令行", kSet},
    {ScpiDeviceRoute::RsCmw100, "Identity", u8"查询设备身份(*IDN?)", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "ArbFilePath", u8"查询波形文件路径", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "ArbScount", u8"查询ARB循环计数", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "GenState", u8"查询射频源状态", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "SystemError", u8"查询系统错误", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "QueryLine", u8"SCPI查询指令", u8"SCPI命令行", kGet},
};

} // namespace

namespace ScpiCmdManifest {

const Row* rows() {
    return kRows;
}

int rowCount() {
    return static_cast<int>(sizeof(kRows) / sizeof(kRows[0]));
}

const Row* findByDeviceAndName(ScpiDeviceRoute device, const QString& enumName) {
    for (int i = 0; i < rowCount(); ++i) {
        if (kRows[i].device == device && QString::fromLatin1(kRows[i].enumName).compare(enumName, Qt::CaseInsensitive) == 0) {
            return &kRows[i];
        }
    }
    return nullptr;
}

} // namespace ScpiCmdManifest
