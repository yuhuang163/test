#include "plc_station_sync_barrier.h"
#include "work_station/freework/qfreework.h"
#include "work_station/freework/qfreeworkbox.h"
#include "platform/test_case/types/test_case_types.h"
#include "platform/test_case/runner/test_case_runner.h"
#include "agreement/modbus_protocol/codec/qmodbus_pdu.h"
#include "agreement/fixture_protocol/xwd_fixture/codec/xwd_raw_uart_codec.h"
#include "platform/settings/widgets/fixture_uart.h"
#include "qfixturemanager.h"
#include "qprotocol_types.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QThread>
#include <QTimer>

static constexpr qint64 kRecentSyncCompletionTtlMs = 60000;

static QFreeWorkBox* freeWorkBoxForPlc(QFreeWork* ctx) {
    if (!ctx)
        return nullptr;
    auto* box = qobject_cast<QFreeWorkBox*>(ctx->window());
    QWidget* parent = ctx->parentWidget();
    while (!box && parent) {
        box = qobject_cast<QFreeWorkBox*>(parent);
        parent = parent->parentWidget();
    }
    return box;
}

static QByteArray buildPlcRtuFrame(quint8 slaveId, const QByteArray& pdu) {
    QByteArray frame;
    frame.append(char(slaveId));
    frame.append(pdu);
    const quint16 crcBe = QModbusPdu::crc16ModbusRtuBigEndian(frame);
    frame.append(char(quint8(crcBe >> 8)));
    frame.append(char(quint8(crcBe & 0xFF)));
    return frame;
}

static bool m8FixturePlcBoolParam(const QVariant& value, bool fallback) {
    if (!value.isValid())
        return fallback;
    if (value.type() == QVariant::Bool)
        return value.toBool();
    const QString text = value.toString().trimmed().toLower();
    if (text == QLatin1String("false") || text == QLatin1String("0") || text == QLatin1String("no")
        || text == QLatin1String("off"))
        return false;
    if (text == QLatin1String("true") || text == QLatin1String("1") || text == QLatin1String("yes")
        || text == QLatin1String("on"))
        return true;
    return fallback;
}

static bool parseM8FixturePlcCoilAddress(const QString& address, quint16* modbusAddress,
                                         QString* errorMessage) {
    QString raw = address.trimmed().toUpper();
    if (!raw.startsWith(QLatin1Char('M')))
        raw.prepend(QLatin1Char('M'));
    bool ok = false;
    const int index = raw.mid(1).toInt(&ok);
    if (!ok || index < 0 || index > 0xF7FF) {
        if (errorMessage)
            *errorMessage = QStringLiteral("M8治具PLC线圈地址非法: %1").arg(address);
        return false;
    }
    if (modbusAddress)
        *modbusAddress = static_cast<quint16>(0x0800 + index);
    return true;
}

static Fixture_uart* sharedFixtureUartForPlc(QFreeWork* ctx, QString* errorMessage) {
    QFreeWorkBox* box = freeWorkBoxForPlc(ctx);
    if (!box) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未找到当前工站所属 QFreeWorkBox");
        return nullptr;
    }
    QString detail;
    Fixture_uart* uart = box->ensurePlcFixtureUartConnected(&detail);
    if (!uart && errorMessage)
        *errorMessage = detail;
    return uart;
}

static bool writePlcCoil(Fixture_uart* uart, const QString& address, bool value, quint8 slaveId,
                         QString* errorMessage) {
    quint16 modbusAddress = 0;
    if (!parseM8FixturePlcCoilAddress(address, &modbusAddress, errorMessage))
        return false;
    return uart->sendRawFrame(
        buildPlcRtuFrame(slaveId, QModbusPdu::buildWriteSingleCoilRequestPdu(modbusAddress, value)),
        errorMessage);
}

static bool readPlcCoil(Fixture_uart* uart, const QString& address, quint8 slaveId, bool* valueOut,
                        QString* errorMessage) {
    quint16 modbusAddress = 0;
    if (!parseM8FixturePlcCoilAddress(address, &modbusAddress, errorMessage))
        return false;
    const QByteArray request =
        buildPlcRtuFrame(slaveId, QModbusPdu::buildReadCoilsRequestPdu(modbusAddress, 1));
    QByteArray raw;
    if (!uart->exchangeRawFrame(request, &raw, 1000, errorMessage))
        return false;
    QByteArray frame;
    if (!QModbusPdu::extractValidRtuFrame(raw, &frame) || frame.size() < 6
        || static_cast<quint8>(frame.at(0)) != slaveId || static_cast<quint8>(frame.at(1)) != 0x01) {
        if (errorMessage)
            *errorMessage = QStringLiteral("PLC 读线圈应答无效: %1")
                                .arg(QString::fromLatin1(raw.toHex(' ').toUpper()));
        return false;
    }
    QVector<bool> bits;
    const QByteArray pdu = frame.mid(1, frame.size() - 3);
    if (!QModbusPdu::parseReadCoilsPdu(pdu, 1, &bits, errorMessage) || bits.isEmpty())
        return false;
    if (valueOut)
        *valueOut = bits.first();
    return true;
}

PlcStationSyncBarrier& PlcStationSyncBarrier::instance() {
    static PlcStationSyncBarrier barrier;
    return barrier;
}

QList<QFreeWork*> PlcStationSyncBarrier::collectConfiguredStations(QFreeWork* ctx) {
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
            if (fw) {
                list.append(fw);
            }
        }
    }

    if (list.isEmpty()) {
        list.append(ctx);
    }
    return list;
}

QString PlcStationSyncBarrier::resolveTargetCoilAddress(const TestCaseDefinition& def, const QString& defaultAddr) {
    QString raw;
    const QVariant param = def.send.param;
    if (param.userType() == QMetaType::QString || param.type() == QVariant::String) {
        raw = param.toString().trimmed();
    } else if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        if (map.contains(QStringLiteral("address")) && !map.value(QStringLiteral("address")).toString().trimmed().isEmpty())
            raw = map.value(QStringLiteral("address")).toString().trimmed();
        else if (map.contains(QStringLiteral("addr")) && !map.value(QStringLiteral("addr")).toString().trimmed().isEmpty())
            raw = map.value(QStringLiteral("addr")).toString().trimmed();
        else if (map.contains(QStringLiteral("m")) && !map.value(QStringLiteral("m")).toString().trimmed().isEmpty())
            raw = map.value(QStringLiteral("m")).toString().trimmed();
    }
    if (raw.isEmpty())
        raw = defaultAddr.trimmed();
    if (raw.isEmpty())
        raw = QStringLiteral("M0");
    bool isPureNum = false;
    raw.toInt(&isPureNum);
    if (isPureNum)
        raw = QStringLiteral("M") + raw;
    return raw.toUpper();
}

void PlcStationSyncBarrier::scheduleSessionTimeout(const QString& sessionKey, SyncSession* session, int timeoutMs) {
    if (!session || session->arrived.isEmpty())
        return;
    QFreeWork* timerContext = session->arrived.first();
    QTimer::singleShot(timeoutMs, timerContext, [this, sessionKey, session]() {
        {
            QMutexLocker locker(&mutex_);
            if (activeSessions_.value(sessionKey, nullptr) != session || session->finished
                || session->arrived.size() >= session->requiredCount)
                return;
        }
        completeSession(sessionKey, session, false, QStringLiteral("等待全员同步超时"),
                        QStringLiteral("等待其他工站到齐超时"));
    });
}

void PlcStationSyncBarrier::rememberRecentCompletion(const QString& sessionKey, bool success,
                                                     const QString& resultData) {
    RecentSyncCompletion recent;
    recent.success = success;
    recent.resultData = resultData;
    recent.completedAtMs = QDateTime::currentMSecsSinceEpoch();
    recentCompletions_.insert(sessionKey, recent);
}

void PlcStationSyncBarrier::dropOrphanSessionsLocked() {
    for (auto it = activeSessions_.begin(); it != activeSessions_.end();) {
        SyncSession* session = it.value();
        if (!session) {
            it = activeSessions_.erase(it);
            continue;
        }
        for (int i = session->arrived.size() - 1; i >= 0; --i) {
            QFreeWork* s = session->arrived.at(i);
            if (!s || !s->isTestContinue || s->plcSyncWaitKey() != session->key)
                session->arrived.removeAt(i);
        }
        if (session->arrived.isEmpty() || session->finished) {
            delete session;
            it = activeSessions_.erase(it);
        } else {
            ++it;
        }
    }
}

void PlcStationSyncBarrier::onStationTestStarting(QFreeWork* ctx) {
    if (ctx)
        ctx->clearPlcSyncWaitKey();
    QMutexLocker locker(&mutex_);
    // 必须清近期完成：否则 60s 内下一轮会误 adopt 上一轮 M0/M11 结果而跳过真同步
    recentCompletions_.clear();
    dropOrphanSessionsLocked();
}

void PlcStationSyncBarrier::pruneAndRegisterLocked(SyncSession* session, QFreeWork* ctx, const QString& sessionKey) {
    if (!session || !ctx)
        return;
    for (int i = session->arrived.size() - 1; i >= 0; --i) {
        QFreeWork* s = session->arrived.at(i);
        if (!s || !s->isTestContinue) {
            session->arrived.removeAt(i);
            continue;
        }
        // 必须仍挂在本同步会话上；关电源等其它步骤的 done 工位不算数
        if (s->plcSyncWaitKey() != sessionKey) {
            session->arrived.removeAt(i);
            continue;
        }
        if (s != ctx && s->isActiveTestCaseStepDone()) {
            session->arrived.removeAt(i);
            continue;
        }
    }
    if (!session->arrived.contains(ctx))
        session->arrived.append(ctx);
}

bool PlcStationSyncBarrier::tryAdoptRecentCompletion(QFreeWork* ctx, const QString& sessionKey,
                                                     const QString& adoptLog) {
    if (!ctx)
        return false;
    RecentSyncCompletion recent;
    {
        QMutexLocker locker(&mutex_);
        auto it = recentCompletions_.find(sessionKey);
        if (it == recentCompletions_.end())
            return false;
        if (QDateTime::currentMSecsSinceEpoch() - it->completedAtMs > kRecentSyncCompletionTtlMs) {
            recentCompletions_.erase(it);
            return false;
        }
        recent = it.value();
    }
    ctx->clearPlcSyncWaitKey();
    ctx->showlog(adoptLog);
    ctx->markActiveTestCaseStepDone(recent.success, recent.resultData,
                                    recent.success ? QStringLiteral("通过") : QStringLiteral("失败"));
    return true;
}

void PlcStationSyncBarrier::completeSession(const QString& sessionKey, SyncSession* session, bool success,
                                            const QString& resultData, const QString& logText) {
    QList<QFreeWork*> arrived;
    {
        QMutexLocker locker(&mutex_);
        if (!session || activeSessions_.value(sessionKey, nullptr) != session || session->finished)
            return;
        session->finished = true;
        session->success = success;
        session->resultData = resultData;
        arrived = session->arrived;
        activeSessions_.remove(sessionKey);
        rememberRecentCompletion(sessionKey, success, resultData);
    }
    // arrived 内工位一律尝试结案；仅当明确在等「另一个」会话时才跳过
    for (QFreeWork* station : arrived) {
        if (!station || station->isActiveTestCaseStepDone())
            continue;
        const QString waitKey = station->plcSyncWaitKey();
        if (!waitKey.isEmpty() && waitKey != sessionKey)
            continue;
        station->clearPlcSyncWaitKey();
        station->showlog(logText);
        station->markActiveTestCaseStepDone(success, resultData,
                                            success ? QStringLiteral("通过") : QStringLiteral("失败"));
    }
    delete session;
}

void PlcStationSyncBarrier::executeSyncWriteCoil(QFreeWork* ctx, const TestCaseDefinition& def, const QString& defaultAddr) {
    if (!ctx)
        return;

    const QString addr = resolveTargetCoilAddress(def, defaultAddr);
    bool pulse = true;
    int pulseHoldMs = 200;
    bool writeVal = true;
    quint8 slaveId = 1;
    int syncWaitMs = 120000;

    if (def.send.param.canConvert<QVariantMap>()) {
        const QVariantMap map = def.send.param.toMap();
        if (map.contains(QStringLiteral("pulse")))
            pulse = m8FixturePlcBoolParam(map.value(QStringLiteral("pulse")), true);
        if (map.contains(QStringLiteral("pulseHoldMs")))
            pulseHoldMs = qMax(50, map.value(QStringLiteral("pulseHoldMs")).toInt());
        if (map.contains(QStringLiteral("value")))
            writeVal = m8FixturePlcBoolParam(map.value(QStringLiteral("value")), true);
        if (map.contains(QStringLiteral("slaveId")))
            slaveId = static_cast<quint8>(map.value(QStringLiteral("slaveId")).toUInt());
        if (map.contains(QStringLiteral("syncWaitMs")))
            syncWaitMs = qMax(1000, map.value(QStringLiteral("syncWaitMs")).toInt());
    }

    // 同步人数按主界面配置的工位总数固定，避免扫码启动有先后时首个工位误判为单工位。
    const QList<QFreeWork*> configuredStations = collectConfiguredStations(ctx);
    const int totalActive = configuredStations.size();

    // 单工位模式：直接执行，不傻等
    if (totalActive <= 1) {
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
        ctx->showlog(QStringLiteral("[PLC单工位写线圈] 使用独立PLC治具串口，目标: %1 (值=%2, 脉冲=%3, 保持%4ms)")
                         .arg(addr)
                         .arg(writeVal ? 1 : 0)
                         .arg(pulse ? "是" : "否")
                         .arg(pulseHoldMs));
        bool ok = uart && writePlcCoil(uart, addr, writeVal, slaveId, &errStr);
        if (ok && pulse) {
            QThread::msleep(static_cast<unsigned long>(pulseHoldMs));
            ok = writePlcCoil(uart, addr, !writeVal, slaveId, &errStr);
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
    // 会话键按线圈地址（如 M11），不按步骤中文名——成功区「测试完成」与失败区「测试完成 副本」可同会话同步
    const QString stepName = def.meta.name.trimmed().isEmpty() ? def.hook.hookId : def.meta.name.trimmed();
    const QString sessionKey = QStringLiteral("WRITE_%1").arg(addr.trimmed().toUpper());

    // 对端已先到齐并结案：本工位迟到则直接沿用结果，禁止再开「1/2」新会话傻等
    if (tryAdoptRecentCompletion(
            ctx, sessionKey,
            QStringLiteral("[一拖多同步写线圈] 工位%1迟到：%2 已由其它工站完成同步，直接放行")
                .arg(ctx->getIndex())
                .arg(addr))) {
        return;
    }

    // 先挂 key，再登记：清洗时才能识别「谁真正在等本会话」
    ctx->setPlcSyncWaitKey(sessionKey);

    SyncSession* session = nullptr;
    bool isLeader = false;
    bool createdSession = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;
    QString arrivedIds;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            // 创建前再查一次近期完成（与上面无锁窗口竞态）
            const auto it = recentCompletions_.find(sessionKey);
            if (it != recentCompletions_.end()
                && QDateTime::currentMSecsSinceEpoch() - it->completedAtMs <= kRecentSyncCompletionTtlMs) {
                const RecentSyncCompletion recent = it.value();
                locker.unlock();
                ctx->clearPlcSyncWaitKey();
                ctx->showlog(QStringLiteral("[一拖多同步写线圈] 工位%1迟到：%2 已由其它工站完成同步，直接放行")
                                 .arg(ctx->getIndex())
                                 .arg(addr));
                ctx->markActiveTestCaseStepDone(recent.success, recent.resultData,
                                                recent.success ? QStringLiteral("通过") : QStringLiteral("失败"));
                return;
            }
            session = new SyncSession();
            session->key = sessionKey;
            session->stepName = stepName;
            session->requiredCount = totalActive;
            activeSessions_.insert(sessionKey, session);
            createdSession = true;
        }
        pruneAndRegisterLocked(session, ctx, sessionKey);
        arrivedIndex = session->arrived.size();
        requiredCount = session->requiredCount;
        isLeader = (arrivedIndex >= requiredCount);
        for (QFreeWork* s : session->arrived) {
            if (!arrivedIds.isEmpty())
                arrivedIds += QLatin1Char(',');
            arrivedIds += s ? QString::number(s->getIndex()) : QStringLiteral("?");
        }
    }

    ctx->showlog(QStringLiteral("[一拖多同步写线圈] 工位%1已就绪（步骤=%2，目标=%3），等待其他工站到齐... (当前 %4/%5，已到=%6)")
                     .arg(ctx->getIndex())
                     .arg(stepName)
                     .arg(addr)
                     .arg(arrivedIndex)
                     .arg(requiredCount)
                     .arg(arrivedIds));
    if (createdSession)
        scheduleSessionTimeout(sessionKey, session, syncWaitMs);

    if (isLeader) {
        // 所有在测工站到齐，由 Leader 执行一次统一写入
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
        ctx->showlog(QStringLiteral("[一拖多同步写线圈] 所有工站已到齐(%1/%2，工位%3)！由工位%4使用独立PLC治具串口写线圈: %5 (脉冲=%6)")
                         .arg(requiredCount)
                         .arg(requiredCount)
                         .arg(arrivedIds)
                         .arg(ctx->getIndex())
                         .arg(addr)
                         .arg(pulse ? "是" : "否"));
        bool ok = uart && writePlcCoil(uart, addr, writeVal, slaveId, &errStr);
        if (ok && pulse) {
            QElapsedTimer pulseTimer;
            pulseTimer.start();
            while (pulseTimer.elapsed() < pulseHoldMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                QThread::msleep(20);
            }
            ok = writePlcCoil(uart, addr, !writeVal, slaveId, &errStr);
        }

        const QString resultData = ok ? QStringLiteral("统一写入完成: %1").arg(addr) : errStr;
        completeSession(sessionKey, session, ok, resultData,
                        QStringLiteral("[一拖多同步写线圈] 执行完毕(%1)，同步结束")
                            .arg(ok ? QStringLiteral("成功") : QStringLiteral("失败")));
        return;
    }
    // 非 Leader 仅登记后返回，主任务循环才能继续调度后续工位进入同一屏障。
}

void PlcStationSyncBarrier::executeSyncReadCoil(QFreeWork* ctx, const TestCaseDefinition& def, const QString& defaultAddr) {
    if (!ctx)
        return;

    const QString addr = resolveTargetCoilAddress(def, defaultAddr);
    // 同步人数按主界面配置的工位总数固定，避免先扫码的工位提前进入单工位轮询。
    const QList<QFreeWork*> configuredStations = collectConfiguredStations(ctx);
    const int totalActive = configuredStations.size();
    const int timeoutMs = qMax(15000, def.timing.commandTimeoutMs > 0 ? def.timing.commandTimeoutMs : 15000);
    const int pollIntervalMs = 120;
    quint8 slaveId = 1;
    int syncWaitMs = 120000;
    if (def.send.param.canConvert<QVariantMap>()) {
        const QVariantMap map = def.send.param.toMap();
        if (map.contains(QStringLiteral("slaveId")))
            slaveId = static_cast<quint8>(map.value(QStringLiteral("slaveId")).toUInt());
        if (map.contains(QStringLiteral("syncWaitMs")))
            syncWaitMs = qMax(1000, map.value(QStringLiteral("syncWaitMs")).toInt());
    }

    // 单工位模式：直接监控
    if (totalActive <= 1) {
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
        if (!uart) {
            ctx->showlog(QStringLiteral("[PLC单工位监控] 治具串口不可用: %1").arg(errStr));
            ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            return;
        }
        ctx->showlog(QStringLiteral("[PLC单工位监控] 使用独立PLC治具串口，监控地址: %1，超时 %2ms")
                         .arg(addr)
                         .arg(timeoutMs));

        QElapsedTimer timer;
        timer.start();
        int sampleIdx = 0;
        while (timer.elapsed() < timeoutMs && !ctx->isActiveTestCaseStepDone()) {
            if (!ctx->isTestContinue) {
                ctx->markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
                return;
            }
            bool value = false;
            const bool ok = readPlcCoil(uart, addr, slaveId, &value, &errStr);
            ++sampleIdx;
            if (ok && value) {
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
    // 会话键按线圈地址，不按步骤中文名（成功区/失败区同地址可同会话）
    const QString stepName = def.meta.name.trimmed().isEmpty() ? def.hook.hookId : def.meta.name.trimmed();
    const QString sessionKey = QStringLiteral("READ_%1").arg(addr.trimmed().toUpper());

    if (tryAdoptRecentCompletion(
            ctx, sessionKey,
            QStringLiteral("[一拖多同步监控] 工位%1迟到：%2 已由其它工站完成同步，直接放行")
                .arg(ctx->getIndex())
                .arg(addr))) {
        return;
    }

    ctx->setPlcSyncWaitKey(sessionKey);

    SyncSession* session = nullptr;
    bool isLeader = false;
    bool createdSession = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;
    QString arrivedIds;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            // 创建前再查一次近期完成（与上面无锁窗口竞态）
            const auto it = recentCompletions_.find(sessionKey);
            if (it != recentCompletions_.end()
                && QDateTime::currentMSecsSinceEpoch() - it->completedAtMs <= kRecentSyncCompletionTtlMs) {
                const RecentSyncCompletion recent = it.value();
                locker.unlock();
                ctx->clearPlcSyncWaitKey();
                ctx->showlog(QStringLiteral("[一拖多同步监控] 工位%1迟到：%2 已由其它工站完成同步，直接放行")
                                 .arg(ctx->getIndex())
                                 .arg(addr));
                ctx->markActiveTestCaseStepDone(recent.success, recent.resultData,
                                                recent.success ? QStringLiteral("通过") : QStringLiteral("失败"));
                return;
            }
            session = new SyncSession();
            session->key = sessionKey;
            session->stepName = stepName;
            session->requiredCount = totalActive;
            activeSessions_.insert(sessionKey, session);
            createdSession = true;
        }
        pruneAndRegisterLocked(session, ctx, sessionKey);
        arrivedIndex = session->arrived.size();
        requiredCount = session->requiredCount;
        isLeader = (arrivedIndex >= requiredCount);
        for (QFreeWork* s : session->arrived) {
            if (!arrivedIds.isEmpty())
                arrivedIds += QLatin1Char(',');
            arrivedIds += s ? QString::number(s->getIndex()) : QStringLiteral("?");
        }
    }

    ctx->showlog(QStringLiteral("[一拖多同步监控] 工位%1已就绪（步骤=%2，目标=%3），等待其他工站到齐... (当前 %4/%5，已到=%6)")
                     .arg(ctx->getIndex())
                     .arg(stepName)
                     .arg(addr)
                     .arg(arrivedIndex)
                     .arg(requiredCount)
                     .arg(arrivedIds));
    if (createdSession)
        scheduleSessionTimeout(sessionKey, session, syncWaitMs);

    if (isLeader) {
        // 所有工站到齐，由 Leader 开始统一轮询监控该线圈
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
        ctx->showlog(QStringLiteral("[一拖多同步监控] 所有工站已到齐(%1/%2，工位%3)！由工位%4使用独立PLC治具串口轮询 [%5]，超时 %6ms")
                         .arg(requiredCount)
                         .arg(requiredCount)
                         .arg(arrivedIds)
                         .arg(ctx->getIndex())
                         .arg(addr)
                         .arg(timeoutMs));

        QElapsedTimer pollTimer;
        pollTimer.start();
        bool triggered = false;
        int sampleIdx = 0;

        while (uart && pollTimer.elapsed() < timeoutMs) {
            if (!ctx->isTestContinue) {
                break;
            }

            bool value = false;
            const bool ok = readPlcCoil(uart, addr, slaveId, &value, &errStr);
            ++sampleIdx;

            if (ok && value) {
                triggered = true;
                ctx->showlog(QStringLiteral("[一拖多同步监控] 采样#%1 检测到治具信号触发 (%2 = 1)！广播放行全工站")
                                 .arg(sampleIdx).arg(addr));
                break;
            }

            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QThread::msleep(static_cast<unsigned long>(pollIntervalMs));
        }

        const QString resultData = triggered
            ? QStringLiteral("治具信号触发: %1=1").arg(addr)
            : (uart ? QStringLiteral("监控超时未触发") : errStr);
        completeSession(sessionKey, session, triggered, resultData,
                        triggered
                            ? QStringLiteral("[一拖多同步监控] 检测到 %1=1，所有工位同步放行").arg(addr)
                            : QStringLiteral("[一拖多同步监控] %1").arg(resultData));
        return;
    }
    // 非 Leader 仅登记后返回，最后到达的工位负责统一监听并回填全部工位结果。
}

static QString firstLineFromReply(const QByteArray& reply) {
    QByteArray trimmed = reply;
    const int cr = trimmed.indexOf('\r');
    const int lf = trimmed.indexOf('\n');
    int cut = -1;
    if (cr >= 0 && lf >= 0)
        cut = qMin(cr, lf);
    else if (cr >= 0)
        cut = cr;
    else if (lf >= 0)
        cut = lf;
    if (cut >= 0)
        trimmed = trimmed.left(cut);
    return QString::fromUtf8(trimmed).trimmed();
}

/** 精确匹配，或期望含 ALL_DOWN_OK 时允许回包包含 ALL_DOWN_OK。 */
static bool xwdTextLineMatchesExpect(const QString& line, const QString& expectToken) {
    if (line.compare(expectToken, Qt::CaseInsensitive) == 0)
        return true;
    if (expectToken.contains(QStringLiteral("ALL_DOWN_OK"), Qt::CaseInsensitive)
        && line.contains(QStringLiteral("ALL_DOWN_OK"), Qt::CaseInsensitive))
        return true;
    return false;
}

/** 经 PLC 治具共享串口发 XWD 文本指令；waitReply=false 时只下发，否则取一行应答（可轮询）。 */
static bool runXwdDownUpOnSharedPlcPort(QFreeWork* ctx, Fixture_uart* uart, const QByteArray& request,
                                        bool pollUntilMatch, const QString& expectToken, int timeoutMs,
                                        QString* lastLineOut, QString* errorMessage, bool waitReply = true) {
    if (!ctx || !uart || !uart->fixtureManager() || !uart->fixtureManager()->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("PLC治具串口未打开");
        return false;
    }
    QFreeWorkBox* box = freeWorkBoxForPlc(ctx);
    QMutex* mutex = box ? box->sharedPlcFixtureMutex() : nullptr;
    QMutexLocker locker(mutex);

    if (!waitReply) {
        QString err;
        if (!uart->fixtureManager()->sendRawFrame(request, &err)) {
            if (errorMessage)
                *errorMessage = err;
            return false;
        }
        if (lastLineOut)
            *lastLineOut = QString();
        return true;
    }

    QElapsedTimer timer;
    timer.start();
    QString lastLine;
    while (true) {
        if (!ctx->isTestContinue) {
            if (errorMessage)
                *errorMessage = QStringLiteral("测试中止");
            return false;
        }
        const int remain = timeoutMs - static_cast<int>(timer.elapsed());
        if (remain <= 0)
            break;
        const int onceMs = pollUntilMatch ? qMin(800, remain) : remain;
        QByteArray response;
        QString err;
        if (!uart->exchangeRawFrame(request, &response, onceMs, &err)) {
            if (!pollUntilMatch) {
                if (errorMessage)
                    *errorMessage = err;
                return false;
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            QThread::msleep(50);
            continue;
        }
        lastLine = firstLineFromReply(response);
        if (lastLineOut)
            *lastLineOut = lastLine;
        if (lastLine.isEmpty()) {
            if (!pollUntilMatch)
                break;
            continue;
        }
        ctx->showlog(QStringLiteral("XWD治具(PLC共享口)文本回包：%1").arg(lastLine));
        if (xwdTextLineMatchesExpect(lastLine, expectToken))
            return true;
        if (!pollUntilMatch) {
            if (errorMessage)
                *errorMessage = QStringLiteral("应答不匹配：%1").arg(lastLine);
            return false;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(50);
    }
    if (lastLineOut)
        *lastLineOut = lastLine;
    if (errorMessage)
        *errorMessage = lastLine.isEmpty() ? QStringLiteral("接收超时") : QStringLiteral("未等到%1").arg(expectToken);
    return false;
}

void PlcStationSyncBarrier::executeSyncXwdTextCmd(QFreeWork* ctx, const TestCaseDefinition& def,
                                                  const QString& rawTextEscaped, bool pollUntilMatch,
                                                  const QString& defaultExpect, bool waitReply) {
    if (!ctx)
        return;

    bool parsedAsHex = false;
    const QByteArray request = XwdRawUartCodec::encodeRawText(rawTextEscaped, &parsedAsHex);
    Q_UNUSED(parsedAsHex);
    if (request.isEmpty()) {
        ctx->showlog(QStringLiteral("XWD 文本指令编码失败"));
        ctx->markActiveTestCaseStepDone(false, QStringLiteral("参数为空"), QStringLiteral("失败"));
        return;
    }

    QString cmdLabel = rawTextEscaped;
    const int slashPos = cmdLabel.indexOf(QLatin1Char('\\'));
    if (slashPos >= 0)
        cmdLabel = cmdLabel.left(slashPos);
    cmdLabel = cmdLabel.trimmed();
    if (cmdLabel.isEmpty())
        cmdLabel = QStringLiteral("XWD");

    const QString expected =
        def.gate.enabled ? def.gate.expected.trimmed()
                         : (defaultExpect.isEmpty() ? QStringLiteral("OK") : defaultExpect);
    const QString expectToken = expected.isEmpty()
                                    ? (defaultExpect.isEmpty() ? QStringLiteral("OK") : defaultExpect)
                                    : expected;
    int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    if (timeoutMs < 2000)
        timeoutMs = 2000;
    int syncWaitMs = 120000;
    if (def.send.param.canConvert<QVariantMap>()) {
        const QVariantMap map = def.send.param.toMap();
        if (map.contains(QStringLiteral("syncWaitMs")))
            syncWaitMs = qMax(1000, map.value(QStringLiteral("syncWaitMs")).toInt());
    }

    const QList<QFreeWork*> configuredStations = collectConfiguredStations(ctx);
    // 与写线圈一致：按主界面一拖多工位总数，避免先扫码开测的工位在「检测下到位」误判为单工位往前跑
    const int totalActive = qMax(1, configuredStations.size());

    auto runOnce = [&](QFreeWork* logCtx) -> bool {
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(logCtx, &errStr);
        if (!uart) {
            logCtx->showlog(QStringLiteral("[XWD %1] PLC治具串口不可用：%2（请先用主页「连接PLC治具串口」选好端口）")
                                .arg(cmdLabel, errStr));
            logCtx->markActiveTestCaseStepDone(false, errStr.isEmpty() ? QStringLiteral("PLC治具串口未连接") : errStr,
                                               QStringLiteral("失败"));
            return false;
        }
        QString plcComShown = uart->currentFixtureComName();
        if (plcComShown.isEmpty())
            plcComShown = QStringLiteral("(已打开)");
        logCtx->showlog(QStringLiteral("[XWD %1] 统一走 PLC治具共享串口 %2，超时 %3ms，期望 %4")
                            .arg(cmdLabel, plcComShown)
                            .arg(timeoutMs)
                            .arg(expectToken));
        QString lastLine;
        const bool ok = runXwdDownUpOnSharedPlcPort(logCtx, uart, request, pollUntilMatch, expectToken, timeoutMs,
                                                    &lastLine, &errStr, waitReply);
        if (ok) {
            ProtocolMeasureData measureData;
            measureData.deviceName = QStringLiteral("XWD");
            measureData.type = QStringLiteral("Reply");
            measureData.valueText = lastLine.isEmpty() ? rawTextEscaped : lastLine;
            measureData.isOk = true;
            logCtx->onUsbInstrumentReport(
                ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(measureData)));
            if (!def.gate.enabled)
                logCtx->markActiveTestCaseStepDone(true, measureData.valueText, QStringLiteral("通过"));
            return true;
        }
        logCtx->showlog(QStringLiteral("[XWD %1] 失败：%2").arg(cmdLabel, errStr));
        logCtx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
        return false;
    };

    // 单工位：直接走共享 PLC 口
    if (totalActive <= 1) {
        runOnce(ctx);
        return;
    }

    const QString stepName = def.meta.name.trimmed().isEmpty() ? cmdLabel : def.meta.name.trimmed();
    // 会话键只按指令（SET_ALL_UP），不按步骤中文名——失败区抬起与成功区抬起可同会话同步
    const QString sessionKey = QStringLiteral("XWD_%1").arg(cmdLabel);

    if (tryAdoptRecentCompletion(
            ctx, sessionKey,
            QStringLiteral("[一拖多XWD %1] 工位%2迟到：该指令已由其它工站完成同步，直接放行")
                .arg(cmdLabel)
                .arg(ctx->getIndex()))) {
        return;
    }

    ctx->setPlcSyncWaitKey(sessionKey);

    SyncSession* session = nullptr;
    bool isLeader = false;
    bool createdSession = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;
    QString arrivedIds;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            const auto it = recentCompletions_.find(sessionKey);
            if (it != recentCompletions_.end()
                && QDateTime::currentMSecsSinceEpoch() - it->completedAtMs <= kRecentSyncCompletionTtlMs) {
                const RecentSyncCompletion recent = it.value();
                locker.unlock();
                ctx->clearPlcSyncWaitKey();
                ctx->showlog(QStringLiteral("[一拖多XWD %1] 工位%2迟到：该指令已由其它工站完成同步，直接放行")
                                 .arg(cmdLabel)
                                 .arg(ctx->getIndex()));
                ctx->markActiveTestCaseStepDone(recent.success, recent.resultData,
                                                recent.success ? QStringLiteral("通过") : QStringLiteral("失败"));
                return;
            }
            session = new SyncSession();
            session->key = sessionKey;
            session->stepName = stepName;
            session->requiredCount = totalActive;
            activeSessions_.insert(sessionKey, session);
            createdSession = true;
        }
        pruneAndRegisterLocked(session, ctx, sessionKey);
        arrivedIndex = session->arrived.size();
        requiredCount = session->requiredCount;
        isLeader = (arrivedIndex >= requiredCount);
        for (QFreeWork* s : session->arrived) {
            if (!arrivedIds.isEmpty())
                arrivedIds += QLatin1Char(',');
            arrivedIds += s ? QString::number(s->getIndex()) : QStringLiteral("?");
        }
    }

    ctx->showlog(QStringLiteral("[一拖多XWD %1] 工位%2步骤「%3」已就绪，等待其它工站到齐（含失败区同指令）… (%4/%5，已到=%6)")
                     .arg(cmdLabel)
                     .arg(ctx->getIndex())
                     .arg(stepName)
                     .arg(arrivedIndex)
                     .arg(requiredCount)
                     .arg(arrivedIds));
    if (createdSession)
        scheduleSessionTimeout(sessionKey, session, syncWaitMs);

    if (!isLeader)
        return;

    // Leader：统一经 PLC 治具串口下发一次
    QString errStr;
    Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
    ctx->showlog(QStringLiteral("[一拖多XWD %1] 所有工站已到齐(%2/%2)！由工位%3统一下发一次")
                     .arg(cmdLabel)
                     .arg(requiredCount)
                     .arg(ctx->getIndex()));
    if (!uart) {
        completeSession(sessionKey, session, false,
                        errStr.isEmpty() ? QStringLiteral("PLC治具串口未连接") : errStr,
                        QStringLiteral("[一拖多XWD %1] PLC治具串口不可用：%2").arg(cmdLabel, errStr));
        return;
    }

    QString lastLine;
    const bool ok = runXwdDownUpOnSharedPlcPort(ctx, uart, request, pollUntilMatch, expectToken, timeoutMs, &lastLine,
                                                &errStr, waitReply);
    const QString resultData =
        ok ? (lastLine.isEmpty() ? rawTextEscaped : lastLine)
           : (errStr.isEmpty() ? QStringLiteral("失败") : errStr);
    completeSession(sessionKey, session, ok, resultData,
                    ok ? QStringLiteral("[一拖多XWD %1] 统一下发成功(%2)，同步放行").arg(cmdLabel, resultData)
                       : QStringLiteral("[一拖多XWD %1] 统一下发失败：%2").arg(cmdLabel, resultData));
}

void PlcStationSyncBarrier::executeImmediateXwdRelayByStation(QFreeWork* ctx, const TestCaseDefinition& def,
                                                              bool turnOn) {
    if (!ctx)
        return;

    // 工位号 1-based → RELAY_N；与流程里写 Relay1/Relay2 无关，避免一拖多都发同一路
    const int stationIndex = qMax(1, ctx->getIndex());
    const QString rawTextEscaped =
        turnOn ? QStringLiteral("RELAY_%1_ON\\r\\n").arg(stationIndex)
               : QStringLiteral("RELAY_%1_OFF\\r\\n").arg(stationIndex);
    const QString cmdLabel =
        turnOn ? QStringLiteral("RELAY_%1_ON").arg(stationIndex)
               : QStringLiteral("RELAY_%1_OFF").arg(stationIndex);

    bool parsedAsHex = false;
    const QByteArray request = XwdRawUartCodec::encodeRawText(rawTextEscaped, &parsedAsHex);
    Q_UNUSED(parsedAsHex);
    if (request.isEmpty()) {
        ctx->showlog(QStringLiteral("XWD 继电器指令编码失败"));
        ctx->markActiveTestCaseStepDone(false, QStringLiteral("参数为空"), QStringLiteral("失败"));
        return;
    }

    QString errStr;
    Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
    if (!uart) {
        ctx->showlog(QStringLiteral("[XWD %1] PLC治具串口不可用：%2（请先用主页「连接PLC治具串口」选好端口）")
                         .arg(cmdLabel, errStr));
        ctx->markActiveTestCaseStepDone(false, errStr.isEmpty() ? QStringLiteral("PLC治具串口未连接") : errStr,
                                        QStringLiteral("失败"));
        return;
    }

    QString plcComShown = uart->currentFixtureComName();
    if (plcComShown.isEmpty())
        plcComShown = QStringLiteral("(已打开)");
    ctx->showlog(QStringLiteral("[XWD %1] 工位%2立即下发（不等待其它工站），串口 %3")
                     .arg(cmdLabel)
                     .arg(stationIndex)
                     .arg(plcComShown));

    // 继电器只发不等回包；与 SET_ALL_UP 共用同一互斥，避免并发抢口
    QString lastLine;
    const bool ok = runXwdDownUpOnSharedPlcPort(ctx, uart, request, false, QStringLiteral("OK"), 2000, &lastLine,
                                                &errStr, false);
    if (ok) {
        ProtocolMeasureData measureData;
        measureData.deviceName = QStringLiteral("XWD");
        measureData.type = QStringLiteral("Reply");
        measureData.valueText = cmdLabel;
        measureData.isOk = true;
        ctx->onUsbInstrumentReport(
            ProtocolReport(QStringLiteral("ProtocolMeasureData"), QVariant::fromValue(measureData)));
        if (!def.gate.enabled)
            ctx->markActiveTestCaseStepDone(true, measureData.valueText, QStringLiteral("通过"));
        return;
    }
    ctx->showlog(QStringLiteral("[XWD %1] 失败：%2").arg(cmdLabel, errStr));
    ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
}

void PlcStationSyncBarrier::executeSyncXwdDownUp(QFreeWork* ctx, const TestCaseDefinition& def, bool isQueryDown) {
    if (isQueryDown) {
        executeSyncXwdTextCmd(ctx, def, QStringLiteral("DOWN?\\r\\n"), true, QStringLiteral("ALL_DOWN_OK"), true);
        return;
    }
    // 兼容旧入口：默认仍走 SET_ALL_UP；单台 UP 请用 executeSyncXwdTextCmd("UP\\r\\n", ...)
    executeSyncXwdTextCmd(ctx, def, QStringLiteral("SET_ALL_UP\\r\\n"), false, QStringLiteral("OK"), true);
}
