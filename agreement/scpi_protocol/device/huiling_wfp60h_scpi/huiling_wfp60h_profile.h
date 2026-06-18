#ifndef HUILING_WFP60H_PROFILE_H
#define HUILING_WFP60H_PROFILE_H

#include <QString>

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

    static HuilingWfp60hScpiProfile defaults();
    /** 暂与 defaults 相同（常量表）；后续若需再接入 SETTINGS。 */
    static HuilingWfp60hScpiProfile fromSettings();
    /** VISA 程控电源：从 [VisaPower] 加载命令模板。 */
    static HuilingWfp60hScpiProfile fromVisaPowerSettings();

    QString buildConfigureMeasureLine() const;
    QString buildReadMeasureCurrentLine() const;
    QString buildReadMeasureConfigurationLine() const;
};

#endif // HUILING_WFP60H_PROFILE_H
