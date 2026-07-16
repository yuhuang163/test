#include "agilent_66319d_profile.h"

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Agilent66319dScpiProfileUtil {

Agilent66319dScpiProfile defaults() {
    Agilent66319dScpiProfile profile;
    // 66319D 主输出：VOLT/CURR/OUTP；读数 MEAS:VOLT:DC? / MEAS:CURR:DC?（见 66319D 用户手册 §7）
    profile.scpiPowerVoltageV = 5.0;
    profile.scpiPowerCurrentA = 3.0;
    profile.scpiSetVoltageCmd = QStringLiteral("VOLT %1");
    profile.scpiSetCurrentCmd = QStringLiteral("CURR %1");
    profile.scpiOutputOnCmd = QStringLiteral("OUTP ON");
    profile.scpiOutputOffCmd = QStringLiteral("OUTP OFF");
    profile.scpiReadVoltageCmd = QStringLiteral("MEAS:VOLT:DC?");
    profile.scpiReadCurrentCmd = QStringLiteral("MEAS:CURR:DC?");
    profile.scpiCurrentType = QStringLiteral("CURR");
    profile.scpiCurrentMode = QStringLiteral("DC");
    profile.scpiRange = QStringLiteral("3");
    return profile;
}

Agilent66319dScpiProfile fromVisaPowerSettings() {
    Agilent66319dScpiProfile profile = defaults();
    profile.scpiPowerVoltageV = SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), profile.scpiPowerVoltageV).toDouble();
    profile.scpiPowerCurrentA =
        SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), profile.scpiPowerCurrentA).toDouble();
    profile.scpiSetVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), QStringLiteral("VOLT %1")).toString();
    profile.scpiSetCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), QStringLiteral("CURR %1")).toString();
    profile.scpiOutputOnCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOnCmd"), QStringLiteral("OUTP ON")).toString();
    profile.scpiOutputOffCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiOutputOffCmd"), QStringLiteral("OUTP OFF")).toString();
    profile.scpiReadVoltageCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), QStringLiteral("MEAS:VOLT:DC?")).toString();
    profile.scpiReadCurrentCmd =
        SETTINGS.value(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), QStringLiteral("MEAS:CURR:DC?")).toString();
    return profile;
}

Agilent66319dScpiProfile fromParamMap(const QVariantMap& map) {
    Agilent66319dScpiProfile profile = defaults();
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

} // namespace Agilent66319dScpiProfileUtil
