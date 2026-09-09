#include "plc_station_sync_barrier.h"
#include "work_station/freework/qfreework.h"
#include "work_station/freework/qfreeworkbox.h"
#include "platform/test_case/types/test_case_types.h"
#include "agreement/modbus_protocol/device/xinjie_plc_rtu/xinjie_plc_rtu_device.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>

PlcStationSyncBarrier& PlcStationSyncBarrier::instance() {
    static PlcStationSyncBarrier barrier;
    return barrier;
}

QList<QFreeWork*> PlcStationSyncBarrier::collectActiveTestingStations(QFreeWork* ctx) {
    QList<QFreeWork*> list;
    if (!ctx)
        return list;

    auto* box = qobject_cast<QFreeWorkBox*>(ctx->window());
    if (!box) {
        QWidget* p = ctx->parentWidget();
        while (p) {
            box = qobject_cast<QFreeWorkBox*>(p);
            if (box)
                break;
            p = p->parentWidget();
        }
    }

    if (box) {
        for (test_base* tb : box->testList) {
            auto* fw = qobject_cast<QFreeWork*>(tb);
            if (fw && fw->isTestContinue) {
                list.append(fw);
            }
        }
    }

    if (list.isEmpty() && ctx->isTestContinue) {
        list.append(ctx);
    }
    return list;
}

QString PlcStationSyncBarrier::resolveTargetCoilAddress(const TestCaseDefinition& def, const QString& defaultAddr) {
    const QVariant param = def.send.param;
    if (param.userType() == QMetaType::QString || param.type() == QVariant::String) {
        const QString s = param.toString().trimmed();
        if (!s.isEmpty())
            return s;
    }
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        if (map.contains(QStringLiteral("address")))
            return map.value(QStringLiteral("address")).toString().trimmed();
        if (map.contains(QStringLiteral("addr")))
            return map.value(QStringLiteral("addr")).toString().trimmed();
        if (map.contains(QStringLiteral("m")))
            return map.value(QStringLiteral("m")).toString().trimmed();
    }
    return defaultAddr.trimmed().isEmpty() ? QStringLiteral("M0") : defaultAddr.trimmed();
}

void PlcStationSyncBarrier::releaseSessionRef(const QString& sessionKey, SyncSession* session) {
    QMutexLocker locker(&mutex_);
    if (!session)
        return;
    --session->remainingExitCount;
    if (session->remainingExitCount <= 0) {
        activeSessions_.remove(sessionKey);
        delete session;
    }
}

void PlcStationSyncBarrier::executeSyncWriteCoil(QFreeWork* ctx, const TestCaseDefinition& def, const QString& defaultAddr) {
    if (!ctx)
        return;

    const QString addr = resolveTargetCoilAddress(def, defaultAddr);
    bool pulse = true;
    int pulseHoldMs = 200;
    bool writeVal = true;

    if (def.send.param.canConvert<QVariantMap>()) {
        const QVariantMap map = def.send.param.toMap();
        if (map.contains(QStringLiteral("pulse")))
            pulse = map.value(QStringLiteral("pulse")).toBool();
        if (map.contains(QStringLiteral("pulseHoldMs")))
            pulseHoldMs = qMax(50, map.value(QStringLiteral("pulseHoldMs")).toInt());
        if (map.contains(QStringLiteral("value")))
            writeVal = map.value(QStringLiteral("value")).toBool();
    }

    const QList<QFreeWork*> activeStations = collectActiveTestingStations(ctx);
    const int totalActive = activeStations.size();

    // 单工位模式：直接执行，不傻等
    if (totalActive <= 1) {
        ctx->showlog(QStringLiteral("[PLC单工位写线圈] 目标: %1 (值=%2, 脉冲=%3, 保持%4ms)")
                         .arg(addr).arg(writeVal ? 1 : 0).arg(pulse ? "是" : "否").arg(pulseHoldMs));

        ctx->modbusManager.setDeviceRoute(ModbusDeviceRoute::XinjiePlcRtu);
        QVariantMap writeParam;
        writeParam.insert(QStringLiteral("address"), addr);
        writeParam.insert(QStringLiteral("value"), writeVal);
        QString errStr;
        bool ok = ctx->modbusManager.exec(XinjePlcCmd::WriteCoil, writeParam, nullptr, &errStr);
        if (ok && pulse) {
            QThread::msleep(static_cast<unsigned long>(pulseHoldMs));
            writeParam.insert(QStringLiteral("value"), !writeVal);
            ok = ctx->modbusManager.exec(XinjePlcCmd::WriteCoil, writeParam, nullptr, &errStr);
        }

        if (ok) {
            ctx->markActiveTestCaseStepDone(true, QStringLiteral("写入成功: %1").arg(addr), QStringLiteral("通过"));
        } else {
            ctx->showlog(QStringLiteral("PLC 写线圈 [%1] 失败: %2").arg(addr, errStr));
            ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
        }
        return;
    }

    // 一拖多模式：进入同步屏障
    const QString stepName = def.meta.name.trimmed().isEmpty() ? def.hook.hookId : def.meta.name.trimmed();
    const QString sessionKey = QStringLiteral("WRITE_%1").arg(stepName);

    SyncSession* session = nullptr;
    bool isLeader = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            session = new SyncSession();
            session->key = sessionKey;
            session->requiredCount = totalActive;
            session->remainingExitCount = totalActive;
            activeSessions_.insert(sessionKey, session);
        }
        if (!session->arrived.contains(ctx)) {
            session->arrived.append(ctx);
        }
        arrivedIndex = session->arrived.size();
        requiredCount = session->requiredCount;
        isLeader = (arrivedIndex >= requiredCount);
    }

    ctx->showlog(QStringLiteral("[一拖多同步写线圈] 工位%1已就绪，等待其他工站到齐... (当前 %2/%3)")
                     .arg(ctx->getIndex()).arg(arrivedIndex).arg(requiredCount));

    const int timeoutMs = qMax(20000, def.timing.commandTimeoutMs > 0 ? def.timing.commandTimeoutMs : 20000);

    if (isLeader) {
        // 所有在测工站到齐，由 Leader 执行一次统一写入
        ctx->showlog(QStringLiteral("[一拖多同步写线圈] 所有在测工站已到齐(%1/%2)！由工位%3统一向治具写线圈: %4 (脉冲=%5)")
                         .arg(requiredCount).arg(requiredCount).arg(ctx->getIndex()).arg(addr).arg(pulse ? "是" : "否"));

        ctx->modbusManager.setDeviceRoute(ModbusDeviceRoute::XinjiePlcRtu);
        QVariantMap writeParam;
        writeParam.insert(QStringLiteral("address"), addr);
        writeParam.insert(QStringLiteral("value"), writeVal);
        QString errStr;
        bool ok = ctx->modbusManager.exec(XinjePlcCmd::WriteCoil, writeParam, nullptr, &errStr);
        if (ok && pulse) {
            QElapsedTimer pulseTimer;
            pulseTimer.start();
            while (pulseTimer.elapsed() < pulseHoldMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                QThread::msleep(20);
            }
            writeParam.insert(QStringLiteral("value"), !writeVal);
            ok = ctx->modbusManager.exec(XinjePlcCmd::WriteCoil, writeParam, nullptr, &errStr);
        }

        {
            QMutexLocker locker(&mutex_);
            session->success = ok;
            session->resultData = ok ? QStringLiteral("统一写入完成: %1").arg(addr) : errStr;
            session->errorString = errStr;
            session->finished = true;
        }

        ctx->showlog(QStringLiteral("[一拖多同步写线圈] 执行完毕(%1)，广播放行所有工站").arg(ok ? "成功" : "失败"));
        ctx->markActiveTestCaseStepDone(ok, session->resultData, ok ? QStringLiteral("通过") : QStringLiteral("失败"));
        releaseSessionRef(sessionKey, session);
        return;
    }

    // 先到达的工站挂起等待
    QElapsedTimer waitTimer;
    waitTimer.start();

    while (waitTimer.elapsed() < timeoutMs && !ctx->isActiveTestCaseStepDone()) {
        if (!ctx->isTestContinue) {
            ctx->markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            releaseSessionRef(sessionKey, session);
            return;
        }

        bool done = false;
        bool succ = false;
        QString resData;
        {
            QMutexLocker locker(&mutex_);
            done = session->finished;
            succ = session->success;
            resData = session->resultData;
        }

        if (done) {
            ctx->showlog(QStringLiteral("[一拖多同步写线圈] 工位%1收到同步完成通知，齐步放行进入下一步").arg(ctx->getIndex()));
            ctx->markActiveTestCaseStepDone(succ, resData, succ ? QStringLiteral("通过") : QStringLiteral("失败"));
            releaseSessionRef(sessionKey, session);
            return;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(50);
    }

    if (!ctx->isActiveTestCaseStepDone()) {
        ctx->showlog(QStringLiteral("[一拖多同步写线圈] 工位%1等待同步超时(%2ms)").arg(ctx->getIndex()).arg(timeoutMs));
        ctx->markActiveTestCaseStepDone(false, QStringLiteral("等待全员同步超时"), QStringLiteral("失败"));
        releaseSessionRef(sessionKey, session);
    }
}

void PlcStationSyncBarrier::executeSyncReadCoil(QFreeWork* ctx, const TestCaseDefinition& def, const QString& defaultAddr) {
    if (!ctx)
        return;

    const QString addr = resolveTargetCoilAddress(def, defaultAddr);
    const QList<QFreeWork*> activeStations = collectActiveTestingStations(ctx);
    const int totalActive = activeStations.size();
    const int timeoutMs = qMax(15000, def.timing.commandTimeoutMs > 0 ? def.timing.commandTimeoutMs : 15000);
    const int pollIntervalMs = 120;

    // 单工位模式：直接监控
    if (totalActive <= 1) {
        ctx->showlog(QStringLiteral("[PLC单工位监控] 监控地址: %1，超时 %2ms").arg(addr).arg(timeoutMs));
        ctx->modbusManager.setDeviceRoute(ModbusDeviceRoute::XinjiePlcRtu);
        QElapsedTimer timer;
        timer.start();
        int sampleIdx = 0;
        while (timer.elapsed() < timeoutMs && !ctx->isActiveTestCaseStepDone()) {
            if (!ctx->isTestContinue) {
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
                return;
            }
            QVariant resultVal;
            QString errStr;
            QVariantMap param;
            param.insert(QStringLiteral("address"), addr);
            param.insert(QStringLiteral("quantity"), 1);
            bool ok = ctx->modbusManager.exec(XinjePlcCmd::ReadCoils, param, &resultVal, &errStr);
            ++sampleIdx;
            if (ok && resultVal.toBool()) {
                ctx->showlog(QStringLiteral("[PLC单工位监控] 采样#%1 检测到信号触发 (%2 = 1)").arg(sampleIdx).arg(addr));
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("信号触发(1)"), QStringLiteral("通过"));
                return;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(static_cast<unsigned long>(pollIntervalMs));
        }
        if (!ctx->isActiveTestCaseStepDone()) {
            ctx->showlog(QStringLiteral("[PLC单工位监控] 超时未检测到信号触发"));
            ctx->markActiveTestCaseStepDone(false, QStringLiteral("监控超时未触发"), QStringLiteral("失败"));
        }
        return;
    }

    // 一拖多模式：进入同步屏障
    const QString stepName = def.meta.name.trimmed().isEmpty() ? def.hook.hookId : def.meta.name.trimmed();
    const QString sessionKey = QStringLiteral("READ_%1").arg(stepName);

    SyncSession* session = nullptr;
    bool isLeader = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            session = new SyncSession();
            session->key = sessionKey;
            session->requiredCount = totalActive;
            session->remainingExitCount = totalActive;
            activeSessions_.insert(sessionKey, session);
        }
        if (!session->arrived.contains(ctx)) {
            session->arrived.append(ctx);
        }
        arrivedIndex = session->arrived.size();
        requiredCount = session->requiredCount;
        isLeader = (arrivedIndex >= requiredCount);
    }

    ctx->showlog(QStringLiteral("[一拖多同步监控] 工位%1已就绪，等待其他工站到齐... (当前 %2/%3)")
                     .arg(ctx->getIndex()).arg(arrivedIndex).arg(requiredCount));

    if (isLeader) {
        // 所有工站到齐，由 Leader 开始统一轮询监控该线圈
        ctx->showlog(QStringLiteral("[一拖多同步监控] 所有在测工站已到齐(%1/%2)！由工位%3启动集中轮询治具信号 [%4]，超时 %5ms...")
                         .arg(requiredCount).arg(requiredCount).arg(ctx->getIndex()).arg(addr).arg(timeoutMs));

        ctx->modbusManager.setDeviceRoute(ModbusDeviceRoute::XinjiePlcRtu);
        QElapsedTimer pollTimer;
        pollTimer.start();
        bool triggered = false;
        int sampleIdx = 0;

        while (pollTimer.elapsed() < timeoutMs) {
            if (!ctx->isTestContinue) {
                break;
            }

            QVariant resultVal;
            QString errStr;
            QVariantMap param;
            param.insert(QStringLiteral("address"), addr);
            param.insert(QStringLiteral("quantity"), 1);
            bool ok = ctx->modbusManager.exec(XinjePlcCmd::ReadCoils, param, &resultVal, &errStr);
            ++sampleIdx;

            if (ok && resultVal.toBool()) {
                triggered = true;
                ctx->showlog(QStringLiteral("[一拖多同步监控] 采样#%1 检测到治具信号触发 (%2 = 1)！广播放行全工站")
                                 .arg(sampleIdx).arg(addr));
                break;
            }

            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(static_cast<unsigned long>(pollIntervalMs));
        }

        {
            QMutexLocker locker(&mutex_);
            session->success = triggered;
            session->resultData = triggered ? QStringLiteral("治具信号触发: %1=1").arg(addr) : QStringLiteral("监控超时未触发");
            session->errorString = triggered ? QString() : QStringLiteral("监控超时");
            session->finished = true;
        }

        ctx->markActiveTestCaseStepDone(triggered, session->resultData, triggered ? QStringLiteral("通过") : QStringLiteral("失败"));
        releaseSessionRef(sessionKey, session);
        return;
    }

    // 先到达的工站挂起等待
    QElapsedTimer waitTimer;
    waitTimer.start();
    const int totalWaitTimeout = timeoutMs + 10000;

    while (waitTimer.elapsed() < totalWaitTimeout && !ctx->isActiveTestCaseStepDone()) {
        if (!ctx->isTestContinue) {
            ctx->markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
            releaseSessionRef(sessionKey, session);
            return;
        }

        bool done = false;
        bool succ = false;
        QString resData;
        {
            QMutexLocker locker(&mutex_);
            done = session->finished;
            succ = session->success;
            resData = session->resultData;
        }

        if (done) {
            ctx->showlog(QStringLiteral("[一拖多同步监控] 工位%1收到信号就绪广播，齐步放行进入下一步").arg(ctx->getIndex()));
            ctx->markActiveTestCaseStepDone(succ, resData, succ ? QStringLiteral("通过") : QStringLiteral("失败"));
            releaseSessionRef(sessionKey, session);
            return;
        }

        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(50);
    }

    if (!ctx->isActiveTestCaseStepDone()) {
        ctx->showlog(QStringLiteral("[一拖多同步监控] 工位%1等待信号监控超时(%2ms)").arg(ctx->getIndex()).arg(totalWaitTimeout));
        ctx->markActiveTestCaseStepDone(false, QStringLiteral("同步监控超时未触发"), QStringLiteral("失败"));
        releaseSessionRef(sessionKey, session);
    }
}
