#ifndef HUILING_WFP60H_PROFILE_H
#define HUILING_WFP60H_PROFILE_H

#include <QString>

/** 会凌电子 HUILING ELECTRONIC（因曼吉）WFP60H 双通道电池模拟器 SCPI 命令表。 */
struct HuilingWfp60hScpiProfile {
    QString scpiCurrentType = QStringLiteral("CURRent");
    QString scpiCurrentMode = QStringLiteral("DC");
    QString scpiRange = QStringLiteral("500e-3");
    double scpiPowerVoltageV = 16.8;
    double scpiPowerCurrentA = 1.0;
    QString scpiSetVoltageCmd = QStringLiteral("VOLT %1");
    QString scpiSetCurrentCmd = QStringLiteral("CURR %1");
    QString scpiOutputOnCmd = QStringLiteral("OUTP ON");
    QString scpiOutputOffCmd = QStringLiteral("OUTP OFF");
    QString scpiReadVoltageCmd = QStringLiteral("MEASure1:VOLTage:DC?");
    QString scpiReadCurrentCmd = QStringLiteral("MEASure1:CURRent:DC?");

    static HuilingWfp60hScpiProfile defaults();
    /** 读 [VisaPower] 与 Current/Scpi*（程控电源模式回退链与 qvisa 一致）。 */
    static HuilingWfp60hScpiProfile fromSettings();
};

#endif // HUILING_WFP60H_PROFILE_H
