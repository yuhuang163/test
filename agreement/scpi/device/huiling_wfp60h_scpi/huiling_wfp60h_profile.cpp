#include "huiling_wfp60h_profile.h"

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::defaults() {
    HuilingWfp60hScpiProfile profile;
    return profile;
}

HuilingWfp60hScpiProfile HuilingWfp60hScpiProfile::fromSettings() {
    HuilingWfp60hScpiProfile profile = defaults();
    const bool useProgPower = SETTINGS.value(QStringLiteral("Current/UseProgrammablePower"), false).toBool();

    auto readKey = [&](const QString& visaKey, const QString& currentKey, const QString& fallback) {
        if (useProgPower) {
            const QString fromCurrent = SETTINGS.value(currentKey).toString();
            if (!fromCurrent.isEmpty()) {
                return fromCurrent;
            }
            return SETTINGS.value(visaKey, fallback).toString();
        }
        return SETTINGS.value(currentKey, fallback).toString();
    };

    profile.scpiPowerVoltageV =
        SETTINGS.value(QStringLiteral("VisaPower/PowerVoltageV"), profile.scpiPowerVoltageV).toDouble();
    profile.scpiPowerCurrentA =
        SETTINGS.value(QStringLiteral("VisaPower/PowerCurrentLimitA"), profile.scpiPowerCurrentA).toDouble();
    profile.scpiSetVoltageCmd =
        readKey(QStringLiteral("VisaPower/ScpiSetVoltageCmd"), QStringLiteral("Current/ScpiSetVoltageCmd"),
                profile.scpiSetVoltageCmd);
    profile.scpiSetCurrentCmd =
        readKey(QStringLiteral("VisaPower/ScpiSetCurrentCmd"), QStringLiteral("Current/ScpiSetCurrentCmd"),
                profile.scpiSetCurrentCmd);
    profile.scpiOutputOnCmd = readKey(QStringLiteral("VisaPower/ScpiOutputOnCmd"), QStringLiteral("Current/ScpiOutputOnCmd"),
                                      profile.scpiOutputOnCmd);
    profile.scpiOutputOffCmd =
        readKey(QStringLiteral("VisaPower/ScpiOutputOffCmd"), QStringLiteral("Current/ScpiOutputOffCmd"),
                profile.scpiOutputOffCmd);
    profile.scpiReadVoltageCmd =
        readKey(QStringLiteral("VisaPower/ScpiReadVoltageCmd"), QStringLiteral("Current/ScpiReadVoltageCmd"),
                profile.scpiReadVoltageCmd);
    profile.scpiReadCurrentCmd =
        readKey(QStringLiteral("VisaPower/ScpiReadCurrentCmd"), QStringLiteral("Current/ScpiReadCurrentCmd"),
                profile.scpiReadCurrentCmd);

    profile.scpiCurrentType = SETTINGS.value(QStringLiteral("Current/ScpiCurrentType"), profile.scpiCurrentType).toString();
    profile.scpiCurrentMode = SETTINGS.value(QStringLiteral("Current/ScpiCurrentMode"), profile.scpiCurrentMode).toString();
    profile.scpiRange = SETTINGS.value(QStringLiteral("Current/ScpiRange"), profile.scpiRange).toString();
    return profile;
}
