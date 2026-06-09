#include "plc_station_config.h"

#include "Abini.h"

#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QString plcBoolText(bool on) {
    return on ? QStringLiteral("开启") : QStringLiteral("关闭");
}

PlcStationConfig PlcStationConfig::fromSettings(int stationIndex) {
    PlcStationConfig cfg;
    const int st = qMax(1, stationIndex);
    cfg.stationIndex = st;

    cfg.ipAddress = SETTINGS.value(QStringLiteral("PLC/IpAddress_Station%1").arg(st),
                                   SETTINGS.value(QStringLiteral("PLC/IpAddress"), QStringLiteral("192.168.1.88")))
                        .toString();
    cfg.port = SETTINGS.value(QStringLiteral("PLC/Port_Station%1").arg(st), SETTINGS.value(QStringLiteral("PLC/Port"), 502))
                   .toInt();
    cfg.unitId = quint8(SETTINGS.value(QStringLiteral("PLC/UnitId_Station%1").arg(st),
                                       SETTINGS.value(QStringLiteral("PLC/UnitId"), 1))
                            .toUInt());
    cfg.mCoilAddressOffset =
        SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset_Station%1").arg(st),
                       SETTINGS.value(QStringLiteral("PLC/MCoilAddressOffset"), 0))
            .toInt();

    const int perMBase = SETTINGS.value(QStringLiteral("PLC/MBase_Station%1").arg(st), -1).toInt();
    if (perMBase >= 0) {
        cfg.mBase = perMBase;
    } else {
        const int base1 = SETTINGS.value(QStringLiteral("PLC/MBase"), 200).toInt();
        const int step = SETTINGS.value(QStringLiteral("PLC/MBaseStationStep"), 20).toInt();
        cfg.mBase = st <= 1 ? base1 : base1 + step * (st - 1);
    }

    const int perForward = SETTINGS.value(QStringLiteral("PLC/SwitchForwardM_Station%1").arg(st), -1).toInt();
    if (perForward >= 0) {
        cfg.switchForwardM = perForward;
    } else {
        const int globalForward = SETTINGS.value(QStringLiteral("PLC/SwitchForwardM"), -1).toInt();
        cfg.switchForwardM = globalForward >= 0
            ? globalForward
            : cfg.mBase + SETTINGS.value(QStringLiteral("PLC/SwitchForwardOffset"), 12).toInt();
    }

    const int perPress = SETTINGS.value(QStringLiteral("PLC/SwitchPressM_Station%1").arg(st), -1).toInt();
    if (perPress >= 0) {
        cfg.switchPressM = perPress;
    } else {
        const int globalPress = SETTINGS.value(QStringLiteral("PLC/SwitchPressM"), -1).toInt();
        cfg.switchPressM =
            globalPress >= 0 ? globalPress : cfg.mBase + SETTINGS.value(QStringLiteral("PLC/SwitchPressOffset"), 7).toInt();
    }

    const int perVerify = SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM_Station%1").arg(st), -1).toInt();
    cfg.connectVerifyM =
        perVerify >= 0 ? perVerify : SETTINGS.value(QStringLiteral("PLC/ConnectVerifyM"), cfg.mBase).toInt();

    const int perPos = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase_Station%1").arg(st), -1).toInt();
    if (perPos >= 0) {
        cfg.positionReadyBase = perPos;
    } else {
        const int globalPos = SETTINGS.value(QStringLiteral("PLC/PositionReadyBase"), -1).toInt();
        if (globalPos >= 0) {
            cfg.positionReadyBase = globalPos;
        } else {
            const int step = SETTINGS.value(QStringLiteral("PLC/PositionReadyBaseStationStep"), 20).toInt();
            cfg.positionReadyBase = 250 + step * (st - 1);
        }
    }

    const int perStepDone = SETTINGS.value(QStringLiteral("PLC/StepDoneBase_Station%1").arg(st), -1).toInt();
    if (perStepDone >= 0) {
        cfg.stepDoneBase = perStepDone;
    } else {
        const int globalStepDone = SETTINGS.value(QStringLiteral("PLC/StepDoneBase"), -1).toInt();
        if (globalStepDone >= 0) {
            cfg.stepDoneBase = globalStepDone;
        } else {
            const int step = SETTINGS.value(QStringLiteral("PLC/StepDoneBaseStationStep"), 20).toInt();
            cfg.stepDoneBase = 260 + step * (st - 1);
        }
    }

    const int perKeyDone = SETTINGS.value(QStringLiteral("PLC/KeyDoneM_Station%1").arg(st), -1).toInt();
    if (perKeyDone >= 0) {
        cfg.keyDoneM = perKeyDone;
    } else {
        const int globalKeyDone = SETTINGS.value(QStringLiteral("PLC/KeyDoneM"), -1).toInt();
        cfg.keyDoneM = globalKeyDone >= 0
            ? globalKeyDone
            : cfg.mBase + SETTINGS.value(QStringLiteral("PLC/KeyDoneOffsetFromMBase"), 10).toInt();
    }

    const int perReset = SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM_Station%1").arg(st), -1).toInt();
    cfg.switchTestDoneResetM =
        perReset >= 0 ? perReset : SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetM"), 211).toInt();

    return cfg;
}
