#include "plc_modbus_session.h"

#include "Abini.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QVector>
#include <QtGlobal>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

PlcModbusSession::PlcModbusSession(InovanceH5uModbusTcp* tcp, const PlcStationConfig& cfg, LogFn log, IsContinueFn isContinue)
    : tcp_(tcp), cfg_(cfg), log_(std::move(log)), isContinue_(std::move(isContinue))
{
}

int PlcModbusSession::requestTimeoutMs() const
{
    return SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
}

void PlcModbusSession::logLine(const QString& line) const
{
    if (log_) {
        log_(line);
    }
}

bool PlcModbusSession::connectPlc(QString* errorMessage)
{
    const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
    return tcp_ && tcp_->connectPlc(cfg_.ipAddress, quint16(cfg_.port), cfg_.unitId, connMs, errorMessage);
}

void PlcModbusSession::disconnect()
{
    if (tcp_) {
        tcp_->disconnect();
    }
}

bool PlcModbusSession::isConnected() const
{
    return tcp_ && tcp_->isConnected();
}

bool PlcModbusSession::readCoil(int absoluteM, bool* value, QString* errorMessage)
{
    QVector<bool> bits;
    if (!tcp_ || !tcp_->readMCoils(absoluteM, 1, cfg_.mCoilAddressOffset, cfg_.unitId, requestTimeoutMs(), &bits,
                                   errorMessage)) {
        return false;
    }
    *value = bits.value(0);
    return true;
}

bool PlcModbusSession::writeCoil(int absoluteM, bool value, QString* errorMessage)
{
    return tcp_ && tcp_->writeMCoil(absoluteM, value, cfg_.mCoilAddressOffset, cfg_.unitId, requestTimeoutMs(), errorMessage);
}

bool PlcModbusSession::waitCoilTrue(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage)
{
    QElapsedTimer t;
    t.start();
    const int step = qMax(10, pollMs);
    while (t.elapsed() < timeoutMs) {
        bool v = false;
        if (!readCoil(absoluteM, &v, errorMessage)) {
            return false;
        }
        if (v) {
            return true;
        }
        QThread::msleep(static_cast<unsigned long>(step));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待 M%1=1 超时 %2ms").arg(absoluteM).arg(timeoutMs);
    }
    return false;
}

bool PlcModbusSession::waitCoilFalse(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage)
{
    QElapsedTimer t;
    t.start();
    const int step = qMax(10, pollMs);
    while (t.elapsed() < timeoutMs) {
        if (isContinue_ && !isContinue_()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("测试已停止");
            }
            return false;
        }
        bool v = false;
        if (!readCoil(absoluteM, &v, errorMessage)) {
            return false;
        }
        if (!v) {
            return true;
        }
        QThread::msleep(static_cast<unsigned long>(step));
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("等待 M%1=0 超时 %2ms").arg(absoluteM).arg(timeoutMs);
    }
    return false;
}

bool PlcModbusSession::sendStepDone(QString* errorMessage)
{
    const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
    const int pulseMs = SETTINGS.value(QStringLiteral("PLC/StepDonePulseMs"), 0).toInt();
    const int m = cfg_.stepDoneBase;
    const int offset = cfg_.mCoilAddressOffset;
    logLine(QStringLiteral("PLC发送 StepDone: M%1(addr=%2)=1 GapMs=%3 PulseMs=%4")
            .arg(m)
            .arg(m + offset)
            .arg(gapMs)
            .arg(pulseMs));
    if (!writeCoil(m, true, errorMessage)) {
        return false;
    }
    if (gapMs > 0) {
        QThread::msleep(static_cast<unsigned long>(gapMs));
    }
    if (pulseMs > 0) {
        QThread::msleep(static_cast<unsigned long>(pulseMs));
        logLine(QStringLiteral("PLC复位 StepDone 脉冲: M%1(addr=%2)=0").arg(m).arg(m + offset));
        if (!writeCoil(m, false, errorMessage)) {
            return false;
        }
        if (gapMs > 0) {
            QThread::msleep(static_cast<unsigned long>(gapMs));
        }
    }
    return true;
}

void PlcModbusSession::logSessionSummary(const QString& stepTag) const
{
    logLine(QStringLiteral("[PLC调试 %1] IP=%2 Port=%3 UnitId=%4 M偏移=%5 MBase=%6 PosReady基=%7 StepDone基=%8 "
                       "KeyDoneM=%9 ConnectVerifyM=%10 ConnectTimeoutMs=%11 RequestTimeoutMs=%12 "
                       "Trace=%13 验证读=%14 握手=%15 等KeyDone=%16 完成后释放=%17 KeyDone预复位=%18 KeyDone复位等待=%19 "
                       "CommandGapMs=%20 PosTimeoutMs=%21 PosPollMs=%22 KeyDoneTimeoutMs=%23 KeyDonePollMs=%24")
            .arg(stepTag)
            .arg(cfg_.ipAddress)
            .arg(cfg_.port)
            .arg(cfg_.unitId)
            .arg(cfg_.mCoilAddressOffset)
            .arg(cfg_.mBase)
            .arg(cfg_.positionReadyBase)
            .arg(cfg_.stepDoneBase)
            .arg(cfg_.keyDoneM)
            .arg(cfg_.connectVerifyM)
            .arg(SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt())
            .arg(SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt())
            .arg(plcBoolText(true))
            .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()))
            .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/UseStepHandshake"), true).toBool()))
            .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneAfterStepDone"), true).toBool()))
            .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/ReleasePositionAfterKeyDone"), true).toBool()))
            .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/EnsureKeyDoneIdleBeforeStep"), false).toBool()))
            .arg(plcBoolText(SETTINGS.value(QStringLiteral("PLC/WaitKeyDoneResetAfterStep"), false).toBool()))
            .arg(SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt())
            .arg(SETTINGS.value(QStringLiteral("PLC/PositionReadyTimeoutMs"),
                                SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
                     .toInt())
            .arg(SETTINGS.value(QStringLiteral("PLC/PositionReadyPollMs"),
                                SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt())
                     .toInt())
            .arg(SETTINGS.value(QStringLiteral("PLC/KeyDoneTimeoutMs"), 8000).toInt())
            .arg(SETTINGS.value(QStringLiteral("PLC/KeyDonePollMs"), 50).toInt()));
}
