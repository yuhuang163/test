#include "huiling_wfp60h_profile.h"

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::defaults() {
    HuilingWfp60hScpiProfile profile;
    // 与 上位机设置.ini [VisaPower] 缺省一致，暂写死不从 SETTINGS 读取
    profile.scpiPowerVoltageV = 12.0;
    profile.scpiPowerCurrentA = 2.5;
    profile.scpiSetVoltageCmd = QStringLiteral("SOURce1:VOLTage:LEVel:IMMediate:AMPLitude %1");
    profile.scpiSetCurrentCmd = QStringLiteral("SOURce1:CURRent:LIMit:VALue %1");
    profile.scpiOutputOnCmd = QStringLiteral("OUTPut1:STATe ON");
    profile.scpiOutputOffCmd = QStringLiteral("OUTPut1:STATe OFF");
    profile.scpiReadVoltageCmd = QStringLiteral("MEASure1:VOLTage:DC?");
    profile.scpiReadCurrentCmd = QStringLiteral("MEASure1:CURRent:DC?");
    profile.scpiCurrentType = QStringLiteral("CURRent");
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
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), QStringLiteral("VOLT %1")).toString();
    profile.scpiSetCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), QStringLiteral("CURR %1")).toString();
    profile.scpiOutputOnCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOnCmd"), QStringLiteral("OUTP ON")).toString();
    profile.scpiOutputOffCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOffCmd"), QStringLiteral("OUTP OFF")).toString();
    profile.scpiReadVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), QStringLiteral("MEASure:VOLTage:DC?")).toString();
    profile.scpiReadCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), QStringLiteral("MEASure:CURRent:DC? 500e-3"))
            .toString();
    return profile;
}

HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::fromParamMap(const QVariantMap& map) {
    HuilingWfp60hScpiProfile profile = defaults();
    if (map.contains(QStringLiteral("voltage")))
        profile.scpiPowerVoltageV = map.value(QStringLiteral("voltage")).toDouble();
    if (map.contains(QStringLiteral("current")))
        profile.scpiPowerCurrentA = map.value(QStringLiteral("current")).toDouble();
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
    return profile;
}

QString HuilingWfp60hScpiProfile::buildConfigureMeasureLine() const {
    return QStringLiteral("CONF:") + scpiCurrentType + QStringLiteral(":") + scpiCurrentMode + QStringLiteral(" ") +
        scpiRange;
}

QString HuilingWfp60hScpiProfile::buildReadMeasureCurrentLine() const {
    return QStringLiteral("MEASure:CURRent:DC? ") + scpiRange;
}

QString HuilingWfp60hScpiProfile::buildReadMeasureConfigurationLine() const {
    return QStringLiteral("CONFigure:FUNCtion?");
}
