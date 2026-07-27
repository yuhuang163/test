#ifndef HUILING_WFP60H_PROFILE_H
#define HUILING_WFP60H_PROFILE_H

#include <QString>
#include <QVariantMap>

/** 会凌电子 HUILING ELECTRONIC（因曼吉）WFP60H 双通道电池模拟器 SCPI 命令表。 */
struct HuilingWfp60hScpiProfile {
    QString scpiCurrentType;
    QString scpiCurrentMode;
    QString scpiRange;
    double scpiPowerVoltageV = 0.0;
    double scpiPowerCurrentA = 0.0;
    QString scpiSetVoltageCmd;
    QString scpiSetCurrentCmd;
    QString scpiOutputOnCmd;
    QString scpiOutputOffCmd;
    QString scpiReadVoltageCmd;
    QString scpiReadCurrentCmd;
    /** 66319D 电流量程：如 SENS:CURR:RANG %1；WFP60H 留空。 */
    QString scpiSetCurrentRangeCmd;
    /** 双通道电源：先选通再 VOLT/OUTP，如 INST OUT%1；会凌 SOURceN 类可留空。 */
    QString scpiChannelSelectCmd;
    /** 1/2；0 表示不发通道选择。 */
    int powerChannel = 0;

    static HuilingWfp60hScpiProfile defaults();
    /** 暂与 defaults 相同（常量表）；后续若需再接入 SETTINGS。 */
    static HuilingWfp60hScpiProfile fromSettings();
    /** VISA 程控电源：从 [VisaPower] 加载命令模板。 */
    static HuilingWfp60hScpiProfile fromVisaPowerSettings();
    /** 从测试步骤 Send/Param map 加载 profile（voltage/current/scpi* 等）。 */
    static HuilingWfp60hScpiProfile fromParamMap(const QVariantMap& map);

    QString buildConfigureMeasureLine() const;
    QString buildReadMeasureCurrentLine() const;
    QString buildReadMeasureConfigurationLine() const;
    QString buildSetCurrentRangeLine(const QString& rangeValue) const;
};

#endif // HUILING_WFP60H_PROFILE_H
