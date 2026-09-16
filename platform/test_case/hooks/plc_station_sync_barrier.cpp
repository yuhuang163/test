#include "plc_station_sync_barrier.h"
#include "work_station/freework/qfreework.h"
#include "work_station/freework/qfreeworkbox.h"
#include "platform/test_case/types/test_case_types.h"
#include "agreement/modbus_protocol/codec/qmodbus_pdu.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QThread>
#include <QTimer>

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

void PlcStationSyncBarrier::completeSession(const QString& sessionKey, SyncSession* session, bool success,
                                            const QString& resultData, const QString& logText) {
    QList<QFreeWork*> arrived;
    QString stepName;
    {
        QMutexLocker locker(&mutex_);
        if (!session || activeSessions_.value(sessionKey, nullptr) != session || session->finished)
            return;
        session->finished = true;
        session->success = success;
        session->resultData = resultData;
        arrived = session->arrived;
        stepName = session->stepName;
        activeSessions_.remove(sessionKey);
    }
    for (QFreeWork* station : arrived) {
        if (!station || !station->isActiveTestCaseStep(stepName) || station->isActiveTestCaseStepDone())
            continue;
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
    const QString stepName = def.meta.name.trimmed().isEmpty() ? def.hook.hookId : def.meta.name.trimmed();
    const QString sessionKey = QStringLiteral("WRITE_%1").arg(stepName);

    SyncSession* session = nullptr;
    bool isLeader = false;
    bool createdSession = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            session = new SyncSession();
            session->key = sessionKey;
            session->stepName = stepName;
            session->requiredCount = totalActive;
            activeSessions_.insert(sessionKey, session);
            createdSession = true;
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
    if (createdSession)
        scheduleSessionTimeout(sessionKey, session, syncWaitMs);

    if (isLeader) {
        // 所有在测工站到齐，由 Leader 执行一次统一写入
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
        ctx->showlog(QStringLiteral("[一拖多同步写线圈] 所有工站已到齐(%1/%2)！由工位%3使用独立PLC治具串口写线圈: %4 (脉冲=%5)")
                         .arg(requiredCount)
                         .arg(requiredCount)
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
    const QString stepName = def.meta.name.trimmed().isEmpty() ? def.hook.hookId : def.meta.name.trimmed();
    const QString sessionKey = QStringLiteral("READ_%1").arg(stepName);

    SyncSession* session = nullptr;
    bool isLeader = false;
    bool createdSession = false;
    int arrivedIndex = 0;
    int requiredCount = totalActive;

    {
        QMutexLocker locker(&mutex_);
        session = activeSessions_.value(sessionKey, nullptr);
        if (!session) {
            session = new SyncSession();
            session->key = sessionKey;
            session->stepName = stepName;
            session->requiredCount = totalActive;
            activeSessions_.insert(sessionKey, session);
            createdSession = true;
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
    if (createdSession)
        scheduleSessionTimeout(sessionKey, session, syncWaitMs);

    if (isLeader) {
        // 所有工站到齐，由 Leader 开始统一轮询监控该线圈
        QString errStr;
        Fixture_uart* uart = sharedFixtureUartForPlc(ctx, &errStr);
        ctx->showlog(QStringLiteral("[一拖多同步监控] 所有工站已到齐(%1/%2)！由工位%3使用独立PLC治具串口轮询 [%4]，超时 %5ms")
                         .arg(requiredCount)
                         .arg(requiredCount)
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
