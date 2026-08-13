#include "huiling_wfp60h_profile.h"

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::defaults() {
    HuilingWfp60hScpiProfile profile;
    // 会凌短写（SCPI 缩写，与手册最短合法形式一致；勿用全拼长词）
    profile.scpiPowerVoltageV = 12.0;
    profile.scpiPowerCurrentA = 2.5;
    profile.scpiSetVoltageCmd = QStringLiteral("SOUR1:VOLT %1");
    profile.scpiSetCurrentCmd = QStringLiteral("SOUR1:CURR %1");
    profile.scpiOutputOnCmd = QStringLiteral("OUTP1 ON");
    profile.scpiOutputOffCmd = QStringLiteral("OUTP1 OFF");
    profile.scpiReadVoltageCmd = QStringLiteral("MEAS1:VOLT:DC?");
    profile.scpiReadCurrentCmd = QStringLiteral("MEAS1:CURR:DC?");
    profile.scpiCurrentType = QStringLiteral("CURR");
    profile.scpiCurrentMode = QStringLiteral("DC");
    profile.scpiRange = QStringLiteral("500e-3");
    return profile;
}

HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::fromSettings() {
    return defaults();
}

HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::fromVisaPowerSettings() {
    HuilingWfp60hScpiProfile profile = defaults();
    profile.scpiPowerVoltageV = SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), 12.0).toDouble();
    profile.scpiPowerCurrentA = SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), 2.5).toDouble();
    profile.scpiSetVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), profile.scpiSetVoltageCmd).toString();
    profile.scpiSetCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), profile.scpiSetCurrentCmd).toString();
    profile.scpiOutputOnCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOnCmd"), profile.scpiOutputOnCmd).toString();
    profile.scpiOutputOffCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOffCmd"), profile.scpiOutputOffCmd).toString();
    profile.scpiReadVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), profile.scpiReadVoltageCmd).toString();
    profile.scpiReadCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), profile.scpiReadCurrentCmd).toString();
    return profile;
}

HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::fromParamMap(const QVariantMap& map) {
    HuilingWfp60hScpiProfile profile = defaults();
    if (map.contains(QStringLiteral("voltage")))
        profile.scpiPowerVoltageV = map.value(QStringLiteral("voltage")).toDouble();
    if (map.contains(QStringLiteral("current")))
        profile.scpiPowerCurrentA = map.value(QStringLiteral("current")).toDouble();
    if (map.contains(QStringLiteral("currentRange")))
        profile.scpiRange = map.value(QStringLiteral("currentRange")).toString().trimmed();
    auto applyCmd = [&](const char* key, QString HuilingWfp60hScpiProfile::* member) {
        const QString text = map.value(QString::fromLatin1(key)).toString().trimmed();
        if (!text.isEmpty())
            profile.*member = text;
    };
    applyCmd("scpiSetVoltageCmd", &HuilingWfp60hScpiProfile::scpiSetVoltageCmd);
    applyCmd("scpiSetCurrentCmd", &HuilingWfp60hScpiProfile::scpiSetCurrentCmd);
    applyCmd("scpiOutputOnCmd", &HuilingWfp60hScpiProfile::scpiOutputOnCmd);
    applyCmd("scpiOutputOffCmd", &HuilingWfp60hScpiProfile::scpiOutputOffCmd);
    applyCmd("scpiReadVoltageCmd", &HuilingWfp60hScpiProfile::scpiReadVoltageCmd);
    applyCmd("scpiReadCurrentCmd", &HuilingWfp60hScpiProfile::scpiReadCurrentCmd);
    applyCmd("scpiSetCurrentRangeCmd", &HuilingWfp60hScpiProfile::scpiSetCurrentRangeCmd);
    applyCmd("scpiChannelSelectCmd", &HuilingWfp60hScpiProfile::scpiChannelSelectCmd);
    if (map.contains(QStringLiteral("powerChannel")))
        profile.powerChannel = qBound(0, map.value(QStringLiteral("powerChannel")).toInt(), 8);
    return profile;
}

QString HuilingWfp60hScpiProfile::buildConfigureMeasureLine() const {
    return QStringLiteral("CONF:") + scpiCurrentType + QStringLiteral(":") + scpiCurrentMode + QStringLiteral(" ") +
        scpiRange;
}

QString HuilingWfp60hScpiProfile::buildReadMeasureCurrentLine() const {
    return QStringLiteral("MEAS:CURR:DC? ") + scpiRange;
}

QString HuilingWfp60hScpiProfile::buildReadMeasureConfigurationLine() const {
    return QStringLiteral("CONF:FUNC?");
}

QString HuilingWfp60hScpiProfile::buildSetCurrentRangeLine(const QString& rangeValue) const {
    const QString range = rangeValue.trimmed();
    if (scpiSetCurrentRangeCmd.isEmpty() || range.isEmpty())
        return {};
    return scpiSetCurrentRangeCmd.arg(range);
}
