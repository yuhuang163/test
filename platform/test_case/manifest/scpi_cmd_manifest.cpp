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
    {ScpiDeviceRoute::HuilingWfp60h, "ConfigureProgrammablePower", u8"配置程控电源",
     u8"单电源：visaAddress；voltage(V)；current(A)\n"
     u8"一拖多共享：sharedPair=true；stationsPerDevice=2|3；visaAddress0/1/2…；可选 scpiChannelSelectCmd=INST OUT%1\n"
     u8"可选 SCPI 模板(含 %1)：scpiSetVoltageCmd / scpiSetCurrentCmd / scpiOutputOnCmd / Off / Read*\n"
     u8"会凌默认短写：SOUR1:VOLT %1 / SOUR1:CURR %1 / OUTP1 ON|OFF / MEAS1:VOLT:DC?；通道号写在 SOURn/OUTPn",
     kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "ProgrammablePowerOutput", u8"电源输出开关",
     u8"int=1开/0关（须先执行配置步骤；输出 ON/OFF 命令复用配置步的 scpiOutputOnCmd/scpiOutputOffCmd）", kSet},
    {ScpiDeviceRoute::HuilingWfp60h, "ReadProgrammableCurrent", u8"读取电流",
     u8"复用配置步骤；可选 scpiReadCurrentCmd / visaAddress\n"
     u8"开 Gate 时连续采样：Param_sampleDurationMs、Param_sampleIntervalMs\n"
     u8"Param_sampleJudgeMode=any（默认）|average（采完后用平均值卡控）",
     kGet, "ProtocolMeasureData", "value"},
    {ScpiDeviceRoute::HuilingWfp60h, "ReadProgrammableVoltage", u8"读取电压",
     u8"复用配置步骤；可选 scpiReadVoltageCmd / visaAddress", kGet, "ProtocolMeasureData", "value"},
    {ScpiDeviceRoute::HuilingWfp60h, "SendRawLine", u8"发送原始SCPI", u8"原始文本命令", kSet},

    // Agilent66319d（程控电源，指令集与会凌电源相同，SCPI 模板不同）
    {ScpiDeviceRoute::Agilent66319d, "ConfigureProgrammablePower", u8"配置程控电源",
     u8"单电源：visaAddress；voltage(V)；current(A)；currentRange(3/1/0.02/AUTO)\n"
     u8"一拖多共享：sharedPair=true；stationsPerDevice=2|3；visaAddress0/1/2…；scpiChannelSelectCmd=INST OUT%1\n"
     u8"可选模板：scpiSetVoltageCmd=VOLT %1；scpiSetCurrentCmd=CURR %1；scpiOutputOn/Off；MEAS:*；SENS:CURR:RANG %1\n"
     u8"界面参数表左侧「中文说明」对照英文参数名",
     kSet},
    {ScpiDeviceRoute::Agilent66319d, "ProgrammablePowerOutput", u8"电源输出开关",
     u8"int=1开/0关（须先执行配置步骤；输出 ON/OFF 命令复用配置步的 scpiOutputOnCmd/scpiOutputOffCmd）", kSet},
    {ScpiDeviceRoute::Agilent66319d, "ReadProgrammableCurrent", u8"读取电流",
     u8"复用配置步骤；读前按 currentRange 发 scpiSetCurrentRangeCmd\n"
     u8"Param_currentRange=3(工作)/0.02(休眠)；可选 scpiReadCurrentCmd / scpiSetCurrentRangeCmd\n"
     u8"开 Gate 时连续采样：Param_sampleDurationMs、Param_sampleIntervalMs\n"
     u8"Param_sampleJudgeMode=any（默认，任一值合格即过）|average（采完后用平均值卡控）",
     kGet, "ProtocolMeasureData", "value"},
    {ScpiDeviceRoute::Agilent66319d, "ReadProgrammableVoltage", u8"读取电压",
     u8"复用配置步骤；可选 scpiReadVoltageCmd / visaAddress", kGet, "ProtocolMeasureData", "value"},
    {ScpiDeviceRoute::Agilent66319d, "SendRawLine", u8"发送原始SCPI", u8"原始文本命令", kSet},

    // GenericVisaPower（SCPI 模板全由步骤 Param 决定；空模板跳过）
    {ScpiDeviceRoute::GenericVisaPower, "ConfigureProgrammablePower", u8"配置程控电源",
     u8"单电源：visaAddress；voltage(V)；current(A)\n"
     u8"一拖多：sharedPair=true；stationsPerDevice；visaAddress0/1\n"
     u8"SCPI 模板空则跳过：scpiSetVoltageCmd / scpiSetCurrentCmd / scpiOutputOnCmd / Off / Read*\n"
     u8"量程仅当 currentRange 与 scpiSetCurrentRangeCmd 都填才发；通道仅当 scpiChannelSelectCmd 有值才发（如 INST OUT%1）",
     kSet},
    {ScpiDeviceRoute::GenericVisaPower, "ProgrammablePowerOutput", u8"电源输出开关",
     u8"int=1开/0关（须先执行配置步骤；输出 ON/OFF 命令复用配置步的 scpiOutputOnCmd/scpiOutputOffCmd）", kSet},
    {ScpiDeviceRoute::GenericVisaPower, "ReadProgrammableCurrent", u8"读取电流",
     u8"复用配置步骤；可选 scpiReadCurrentCmd / visaAddress\n"
     u8"模板空则不切量程；currentRange 与 scpiSetCurrentRangeCmd 都填才发量程\n"
     u8"开 Gate 时连续采样：Param_sampleDurationMs、Param_sampleIntervalMs\n"
     u8"Param_sampleJudgeMode=any（默认）|average（采完后用平均值卡控）",
     kGet, "ProtocolMeasureData", "value"},
    {ScpiDeviceRoute::GenericVisaPower, "ReadProgrammableVoltage", u8"读取电压",
     u8"复用配置步骤；可选 scpiReadVoltageCmd / visaAddress", kGet, "ProtocolMeasureData", "value"},
    {ScpiDeviceRoute::GenericVisaPower, "SendRawLine", u8"发送原始SCPI", u8"原始文本命令", kSet},

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
    {ScpiDeviceRoute::RsCmw100, "WriteLine", u8"发送原始SCPI", u8"SCPI命令行", kSet},
    {ScpiDeviceRoute::RsCmw100, "Identity", u8"查询设备身份", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "ArbFilePath", u8"查询波形文件路径", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "ArbScount", u8"查询ARB循环计数", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "GenState", u8"查询射频源状态", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "SystemError", u8"查询系统错误", nullptr, kGet},
    {ScpiDeviceRoute::RsCmw100, "QueryLine", u8"查询原始SCPI", u8"SCPI命令行", kGet},
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