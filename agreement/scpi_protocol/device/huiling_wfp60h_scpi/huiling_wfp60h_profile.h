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
    /** 可选选通命令（含 %1）；Agilent 66319D 辅路用 VOLT2/CURR2，通常留空。 */
    QString scpiChannelSelectCmd;
    /** 1/2；0 表示不发通道选择。 */
    int powerChannel = 0;
    /**
     * 二托二共用一台 Agilent 时：配置步写 VOLT/CURR 与 VOLT2/CURR2（同一套电压电流）。
     * 会凌 SOURceN 改写路径保持 1（仍只配本工位通道）。
     */
    int stationsPerDevice = 1;

    static HuilingWfp60hScpiProfile defaults();
    /** 暂与 defaults 相同（常量表）；后续若需再接入 SETTINGS。 */
    static HuilingWfp60hScpiProfile fromSettings();
    /** VISA 程控电源：从 [VisaPower] 加载命令模板。 */
    static HuilingWfp60hScpiProfile fromVisaPowerSettings();
    /** 从测试步骤 Send/Param map 加载 profile（voltage/current/scpi* 等）。 */
    static HuilingWfp60hScpiProfile fromParamMap(const QVariantMap& map);
    /** 通用 VISA 电源：默认短写 VOLT/CURR/OUTP/MEAS:*?，量程与通道选择留空。 */
    static HuilingWfp60hScpiProfile genericVisaPowerDefaults();
    static HuilingWfp60hScpiProfile fromGenericVisaPowerParamMap(const QVariantMap& map);
    void applyParamMap(const QVariantMap& map);

    QString buildConfigureMeasureLine() const;
    QString buildReadMeasureCurrentLine() const;
    QString buildReadMeasureConfigurationLine() const;
    QString buildSetCurrentRangeLine(const QString& rangeValue) const;
};

#endif // HUILING_WFP60H_PROFILE_H
