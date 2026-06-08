#include "plc_v3_touch.h"

#include "Abini.h"

#include <QThread>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

PlcV3TouchResult runPlcV3TouchKey(PlcModbusSession& session, int keyIndex0To6, const PlcV3TouchKeyOptions& options) {
    PlcV3TouchResult result;
    const PlcStationConfig& cfg = session.config();
    const int offset = cfg.mCoilAddressOffset;

    if (keyIndex0To6 < 0 || keyIndex0To6 > 6) {
        result.summary = QStringLiteral("V3 Touch keyIndex 非法: %1").arg(keyIndex0To6);
        return result;
    }

    QString err;
    session.logLine(QStringLiteral("PLC按键整步连接: 键Index=%1 工位=%2 IP=%3 Port=%4 UnitId=%5")
                        .arg(keyIndex0To6)
                        .arg(cfg.stationIndex)
                        .arg(cfg.ipAddress)
                        .arg(cfg.port)
                        .arg(cfg.unitId));
    if (!session.connectPlc(&err)) {
        result.summary = QStringLiteral("PLC 连接失败: %1").arg(err);
        return result;
    }

    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const bool useHandshake = SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool();
    const bool waitKeyDone = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool();
    const bool releaseAfter = SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool();
    const bool ensureKeyIdle = SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool();
    const bool waitKeyReset = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool();
    const int posSettle = SETTINGS.value(QStringLiteral("PLC/PositionSettleMs"), 500).toInt();
    const int actionSettle = SETTINGS.value(QStringLiteral("PLC/KeyActionSettleMs"),
                                            SETTINGS.value(QStringLiteral("KeyTest/ActionSettleMs"), 0).toInt())
                                 .toInt();
    const int releaseSettle = SETTINGS.value(QStringLiteral("PLC/KeyReleaseSettleMs"),
                                             SETTINGS.value(QStringLiteral("KeyTest/ReleaseSettleMs"), 120).toInt())
                                  .toInt();
    const int posTimeout = SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                               .toInt();
    const int posPoll = SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                       SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 1000).toInt())
                            .toInt();
    const int keyDoneTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt();
    const int keyDonePoll = SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt();
    const int keyResetTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneResetTimeoutMs"), 1500).toInt();

    const int keyM = cfg.mBase + keyIndex0To6;
    const int posReadyM = cfg.positionReadyBase + keyIndex0To6;
    const int stepDoneM = cfg.stepDoneBase;
    const int failResetM = cfg.switchTestDoneResetM;
    const int keyDoneM = cfg.keyDoneM;

    const auto fail = [&](const QString& msg) {
        if (session.isConnected()) {
            QString plcErr;
            session.logLine(QStringLiteral("按键测试失败补发：StepDone M%1(addr=%2)=1").arg(stepDoneM).arg(stepDoneM + offset));
            if (!session.writeCoil(stepDoneM, true, &plcErr)) {
                session.logLine(QStringLiteral("按键失败补发 StepDone 失败: %1").arg(plcErr));
            } else if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
            session.logLine(QStringLiteral("按键测试失败补发：复位 M%1(addr=%2)=1").arg(failResetM).arg(failResetM + offset));
            if (!session.writeCoil(failResetM, true, &plcErr)) {
                session.logLine(QStringLiteral("按键失败补发复位M失败: %1").arg(plcErr));
            }
        }
        session.disconnect();
        result.summary = msg;
    };

    session.logLine(QStringLiteral("PLC按键整步开始: 键Index=%1 KeyM=M%2(addr=%3) PosReady=M%4(addr=%5) "
                                   "StepDone=M%6(addr=%7) KeyDone=M%8(addr=%9) 握手=%10 等KeyDone=%11 完成后释放=%12")
                        .arg(keyIndex0To6)
                        .arg(keyM)
                        .arg(keyM + offset)
                        .arg(posReadyM)
                        .arg(posReadyM + offset)
                        .arg(stepDoneM)
                        .arg(stepDoneM + offset)
                        .arg(keyDoneM)
                        .arg(keyDoneM + offset)
                        .arg(plcBoolText(useHandshake))
                        .arg(plcBoolText(waitKeyDone))
                        .arg(plcBoolText(releaseAfter)));

    if (ensureKeyIdle) {
        bool kd = false;
        session.logLine(QStringLiteral("PLC预检 KeyDone 空闲: 读 M%1(addr=%2)").arg(keyDoneM).arg(keyDoneM + offset));
        if (!session.readCoil(keyDoneM, &kd, &err)) {
            fail(QStringLiteral("读 KeyDone 失败: %1").arg(err));
            return result;
        }
        if (kd && !session.waitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return result;
        }
    }

    if (useHandshake) {
        bool sd = false;
        session.logLine(QStringLiteral("PLC预检 StepDone: 读 M%1(addr=%2)").arg(stepDoneM).arg(stepDoneM + offset));
        if (!session.readCoil(stepDoneM, &sd, &err)) {
            fail(QStringLiteral("读 StepDone 失败: %1").arg(err));
            return result;
        }
        if (sd) {
            session.logLine(QStringLiteral("PLC复位残留 StepDone: M%1(addr=%2)=0").arg(stepDoneM).arg(stepDoneM + offset));
            if (!session.writeCoil(stepDoneM, false, &err)) {
                fail(QStringLiteral("复位 StepDone 失败: %1").arg(err));
                return result;
            }
            if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
        }
    }

    session.logLine(QStringLiteral("PLC按键下压: M%1(addr=%2)=1").arg(keyM).arg(keyM + offset));
    if (!session.writeCoil(keyM, true, &err)) {
        fail(QStringLiteral("下压 M%1 失败: %2").arg(keyM).arg(err));
        return result;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }

    if (useHandshake) {
        session.logLine(QStringLiteral("PLC等待位置到位哈哈: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                            .arg(posReadyM)
                            .arg(posReadyM + offset)
                            .arg(posTimeout)
                            .arg(posPoll));
        if (!session.waitCoilTrue(posReadyM, posTimeout, posPoll, &err)) {
            fail(QStringLiteral("等待位置到位 M%1: %2").arg(posReadyM).arg(err));
            return result;
        }
    }

    if (posSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(posSettle));
    }
    if (actionSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(actionSettle));
    }

    QString capSummary;
    if (options.pollCapDuringPress) {
        QString capErr;
        if (!options.pollCapDuringPress(&capErr, &capSummary)) {
            fail(capErr.isEmpty() ? QStringLiteral("按下期间读取按键电容失败") : capErr);
            return result;
        }
    }

    if (useHandshake) {
        if (!session.sendStepDone(&err)) {
            fail(QStringLiteral("发送 StepDone 失败: %1").arg(err));
            return result;
        }
    }

    bool sawKeyDone = false;
    if (waitKeyDone) {
        session.logLine(QStringLiteral("PLC等待 KeyDone: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                            .arg(keyDoneM)
                            .arg(keyDoneM + offset)
                            .arg(keyDoneTimeout)
                            .arg(keyDonePoll));
        if (!session.waitCoilTrue(keyDoneM, keyDoneTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone M%1: %2").arg(keyDoneM).arg(err));
            return result;
        }
        sawKeyDone = true;
    }

    if (releaseAfter) {
        session.logLine(QStringLiteral("PLC按键抬起: M%1(addr=%2)=0").arg(keyM).arg(keyM + offset));
        if (!session.writeCoil(keyM, false, &err)) {
            fail(QStringLiteral("抬起 M%1 失败: %2").arg(keyM).arg(err));
            return result;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }

    if (releaseSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(releaseSettle));
    }

    if (sawKeyDone && waitKeyReset) {
        session.logLine(QStringLiteral("PLC等待 KeyDone 复位: M%1(addr=%2)=0 TimeoutMs=%3 PollMs=%4")
                            .arg(keyDoneM)
                            .arg(keyDoneM + offset)
                            .arg(keyResetTimeout)
                            .arg(keyDonePoll));
        if (!session.waitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return result;
        }
    }

    session.disconnect();
    result.ok = true;
    if (!capSummary.isEmpty()) {
        result.summary = QStringLiteral("键%1 M%2 P%3 S%4 K%5 电容:%6")
                             .arg(keyIndex0To6)
                             .arg(keyM)
                             .arg(posReadyM)
                             .arg(stepDoneM)
                             .arg(keyDoneM)
                             .arg(capSummary);
    } else {
        result.summary = QStringLiteral("键%1 M%2 P%3 S%4 K%5")
                             .arg(keyIndex0To6)
                             .arg(keyM)
                             .arg(posReadyM)
                             .arg(stepDoneM)
                             .arg(keyDoneM);
    }
    return result;
}

PlcV3TouchResult runPlcV3TouchSwitch(PlcModbusSession& session) {
    PlcV3TouchResult result;
    const PlcStationConfig& cfg = session.config();
    const int offset = cfg.mCoilAddressOffset;

    QString err;
    session.logLine(QStringLiteral("PLC旋钮整步连接: 工位=%1 IP=%2 Port=%3 UnitId=%4")
                        .arg(cfg.stationIndex)
                        .arg(cfg.ipAddress)
                        .arg(cfg.port)
                        .arg(cfg.unitId));
    if (!session.connectPlc(&err)) {
        result.summary = QStringLiteral("PLC 连接失败: %1").arg(err);
        return result;
    }

    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const bool useHandshake = SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool();
    const bool waitKeyDone = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool();
    const bool releaseAfter = SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool();
    const bool ensureKeyIdle = SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool();
    const bool waitKeyReset = SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool();
    const int posSettle = SETTINGS.value(QStringLiteral("PLC/PositionSettleMs"), 500).toInt();
    const int actionSettle = SETTINGS.value(QStringLiteral("PLC/KeyActionSettleMs"),
                                            SETTINGS.value(QStringLiteral("KeyTest/ActionSettleMs"), 0).toInt())
                                 .toInt();
    const int releaseSettle = SETTINGS.value(QStringLiteral("PLC/KeyReleaseSettleMs"),
                                             SETTINGS.value(QStringLiteral("KeyTest/ReleaseSettleMs"), 120).toInt())
                                  .toInt();
    const int posTimeout = SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                          SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                               .toInt();
    const int posPoll = SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                       SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt())
                            .toInt();
    const int keyDoneTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt();
    const int keyDonePoll = SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt();
    const int keyResetTimeout = SETTINGS.value(QStringLiteral("PLC/KeyDoneResetTimeoutMs"), 1500).toInt();

    const int forwardM = cfg.switchForwardM;
    const int pressM = cfg.switchPressM;
    const int posReadyM = cfg.positionReadyBase + 7;
    const int stepDoneM = cfg.stepDoneBase;
    const int keyDoneM = cfg.keyDoneM;

    const auto fail = [&](const QString& msg) {
        session.disconnect();
        result.summary = msg;
    };

    session.logLine(QStringLiteral("PLC旋钮整步开始: Forward=M%1(addr=%2) Press=M%3(addr=%4) PosReady=M%5(addr=%6) "
                                   "StepDone=M%7(addr=%8) KeyDone=M%9(addr=%10) 握手=%11 等KeyDone=%12 完成后释放=%13")
                        .arg(forwardM)
                        .arg(forwardM + offset)
                        .arg(pressM)
                        .arg(pressM + offset)
                        .arg(posReadyM)
                        .arg(posReadyM + offset)
                        .arg(stepDoneM)
                        .arg(stepDoneM + offset)
                        .arg(keyDoneM)
                        .arg(keyDoneM + offset)
                        .arg(plcBoolText(useHandshake))
                        .arg(plcBoolText(waitKeyDone))
                        .arg(plcBoolText(releaseAfter)));

    if (ensureKeyIdle) {
        bool kd = false;
        session.logLine(QStringLiteral("PLC预检 KeyDone 空闲: 读 M%1(addr=%2)").arg(keyDoneM).arg(keyDoneM + offset));
        if (!session.readCoil(keyDoneM, &kd, &err)) {
            fail(QStringLiteral("读 KeyDone 失败: %1").arg(err));
            return result;
        }
        if (kd && !session.waitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return result;
        }
    }

    if (useHandshake) {
        bool sd = false;
        session.logLine(QStringLiteral("PLC预检 StepDone: 读 M%1(addr=%2)").arg(stepDoneM).arg(stepDoneM + offset));
        if (!session.readCoil(stepDoneM, &sd, &err)) {
            fail(QStringLiteral("读 StepDone 失败: %1").arg(err));
            return result;
        }
        if (sd) {
            session.logLine(QStringLiteral("PLC复位残留 StepDone: M%1(addr=%2)=0").arg(stepDoneM).arg(stepDoneM + offset));
            if (!session.writeCoil(stepDoneM, false, &err)) {
                fail(QStringLiteral("复位 StepDone 失败: %1").arg(err));
                return result;
            }
            if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
        }
    }

    session.logLine(QStringLiteral("PLC旋钮前推: M%1(addr=%2)=1").arg(forwardM).arg(forwardM + offset));
    if (!session.writeCoil(forwardM, true, &err)) {
        fail(QStringLiteral("旋钮前推 M%1 失败: %2").arg(forwardM).arg(err));
        return result;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    session.logLine(QStringLiteral("PLC旋钮按压: M%1(addr=%2)=1").arg(pressM).arg(pressM + offset));
    if (!session.writeCoil(pressM, true, &err)) {
        fail(QStringLiteral("旋钮按压 M%1 失败: %2").arg(pressM).arg(err));
        return result;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }

    if (useHandshake) {
        session.logLine(QStringLiteral("PLC等待位置到位: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                            .arg(posReadyM)
                            .arg(posReadyM + offset)
                            .arg(posTimeout)
                            .arg(posPoll));
        if (!session.waitCoilTrue(posReadyM, posTimeout, posPoll, &err)) {
            fail(QStringLiteral("等待位置到位 M%1: %2").arg(posReadyM).arg(err));
            return result;
        }
    }

    if (posSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(posSettle));
    }
    if (actionSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(actionSettle));
    }

    if (useHandshake) {
        if (!session.sendStepDone(&err)) {
            fail(QStringLiteral("发送 StepDone 失败: %1").arg(err));
            return result;
        }
    }

    bool sawKeyDone = false;
    if (waitKeyDone) {
        session.logLine(QStringLiteral("PLC等待 KeyDone: M%1(addr=%2)=1 TimeoutMs=%3 PollMs=%4")
                            .arg(keyDoneM)
                            .arg(keyDoneM + offset)
                            .arg(keyDoneTimeout)
                            .arg(keyDonePoll));
        if (!session.waitCoilTrue(keyDoneM, keyDoneTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone M%1: %2").arg(keyDoneM).arg(err));
            return result;
        }
        sawKeyDone = true;
    }

    if (releaseAfter) {
        session.logLine(QStringLiteral("PLC旋钮释放按压: M%1(addr=%2)=0").arg(pressM).arg(pressM + offset));
        if (!session.writeCoil(pressM, false, &err)) {
            fail(QStringLiteral("旋钮释放按压 M%1 失败: %2").arg(pressM).arg(err));
            return result;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
        session.logLine(QStringLiteral("PLC旋钮收回前推: M%1(addr=%2)=0").arg(forwardM).arg(forwardM + offset));
        if (!session.writeCoil(forwardM, false, &err)) {
            fail(QStringLiteral("旋钮收回前推 M%1 失败: %2").arg(forwardM).arg(err));
            return result;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }

    if (releaseSettle > 0) {
        QThread::msleep(static_cast<unsigned long>(releaseSettle));
    }

    if (sawKeyDone && waitKeyReset) {
        session.logLine(QStringLiteral("PLC等待 KeyDone 复位: M%1(addr=%2)=0 TimeoutMs=%3 PollMs=%4")
                            .arg(keyDoneM)
                            .arg(keyDoneM + offset)
                            .arg(keyResetTimeout)
                            .arg(keyDonePoll));
        if (!session.waitCoilFalse(keyDoneM, keyResetTimeout, keyDonePoll, &err)) {
            fail(QStringLiteral("等待 KeyDone 复位: %1").arg(err));
            return result;
        }
    }

    session.disconnect();
    result.ok = true;
    return result;
}
