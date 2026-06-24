#include "plc_v3_fixture.h"

#include "qmodbusmanager.h"
#include "plc_v3_touch.h"

#include "Abini.h"

#include <QThread>
#include <QVector>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

void noopLog(const QString&) {
}

} // namespace

PlcV3RunResult PlcV3Fixture::run(PlcV3Command command, const PlcV3RunParams& params) {
    PlcV3RunResult result;
    if (!params.modbus) {
        result.summary = QStringLiteral("PlcV3RunParams.modbus 未设置");
        return result;
    }

    QModbusManager* modbus = params.modbus;
    modbus->setStationIndex(params.stationIndex);
    if (params.log) {
        modbus->setLogFn(params.log);
    }
    if (params.isTestContinue) {
        modbus->setIsContinueFn(params.isTestContinue);
    }
    const auto log = params.log ? params.log : noopLog;

    switch (command) {
    case PlcV3Command::ModbusConnectTest:
        return modbus->withSession([&](PlcModbusSession& session) {
            PlcV3RunResult connectResult;
            session.logSessionSummary(QStringLiteral("连接测试"));
            const PlcStationConfig& cfg = session.config();
            const int connMs = SETTINGS.value(QStringLiteral("PLC/ConnectTimeoutMs"), 3000).toInt();
            const int reqMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
            log(QStringLiteral("PLC_Modbus连接开始(工位%1): IP=%2 Port=%3 UnitId=%4 ConnectTimeoutMs=%5 RequestTimeoutMs=%6")
                    .arg(cfg.stationIndex)
                    .arg(cfg.ipAddress)
                    .arg(cfg.port)
                    .arg(cfg.unitId)
                    .arg(connMs)
                    .arg(reqMs));
            QString err;
            if (!session.connectPlc(&err)) {
                connectResult.summary = QStringLiteral("%1；配置: IP=%2 Port=%3 UnitId=%4 ConnectTimeoutMs=%5 "
                                                       "RequestTimeoutMs=%6 M偏移=%7 验证读=%8；请检查PLC IP/端口、网线、"
                                                       "PLC Modbus TCP服务和防火墙")
                                            .arg(err)
                                            .arg(cfg.ipAddress)
                                            .arg(cfg.port)
                                            .arg(cfg.unitId)
                                            .arg(connMs)
                                            .arg(reqMs)
                                            .arg(cfg.mCoilAddressOffset)
                                            .arg(SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()
                                                     ? QStringLiteral("开启")
                                                     : QStringLiteral("关闭"));
                log(QStringLiteral("PLC_Modbus连接失败(工位%1): %2").arg(cfg.stationIndex).arg(connectResult.summary));
                return connectResult;
            }
            log(QStringLiteral("PLC_Modbus TCP已连接(工位%1): IP=%2 Port=%3 UnitId=%4")
                    .arg(cfg.stationIndex)
                    .arg(cfg.ipAddress)
                    .arg(cfg.port)
                    .arg(cfg.unitId));
            bool ok = true;
            if (SETTINGS.value(QStringLiteral("PLC/ConnectVerifyRead"), true).toBool()) {
                const int verifyM = cfg.connectVerifyM;
                const int offset = cfg.mCoilAddressOffset;
                log(QStringLiteral("PLC_Modbus连接验证读: M%1(addr=%2) Quantity=1 TimeoutMs=%3")
                        .arg(verifyM)
                        .arg(verifyM + offset)
                        .arg(reqMs));
                QVector<bool> bits;
                if (!modbus->h5uTcp()->readMCoils(verifyM, 1, offset, cfg.unitId, reqMs, &bits, &err)) {
                    ok = false;
                    connectResult.summary = QStringLiteral("连接验证读失败: M%1(addr=%2) %3")
                                                .arg(verifyM)
                                                .arg(verifyM + offset)
                                                .arg(err);
                    log(QStringLiteral("PLC 连接后读线圈失败(可关 PLC/ConnectVerifyRead): M%1(addr=%2) %3")
                            .arg(verifyM)
                            .arg(verifyM + offset)
                            .arg(err));
                } else {
                    connectResult.summary = QStringLiteral("已连 %1:%2").arg(cfg.ipAddress).arg(cfg.port);
                    log(QStringLiteral("PLC_Modbus连接验证读通过: M%1(addr=%2)=%3")
                            .arg(verifyM)
                            .arg(verifyM + offset)
                            .arg(bits.value(0) ? 1 : 0));
                }
            } else {
                connectResult.summary = QStringLiteral("已连 %1:%2 UnitId=%3，验证读关闭")
                                            .arg(cfg.ipAddress)
                                            .arg(cfg.port)
                                            .arg(cfg.unitId);
                log(QStringLiteral("PLC_Modbus连接验证读关闭"));
            }
            session.disconnect();
            connectResult.ok = ok;
            log(ok ? QStringLiteral("PLC_Modbus连接通过") : QStringLiteral("PLC_Modbus连接未通过"));
            return connectResult;
        });
    case PlcV3Command::SwitchDoneReset:
        return modbus->withSession([&](PlcModbusSession& session) {
            PlcV3RunResult resetResult;
            const PlcStationConfig& cfg = session.config();
            const int resetM = cfg.switchTestDoneResetM;
            const int gapMs = SETTINGS.value(QStringLiteral("PLC/CommandGapMs"), 80).toInt();
            const int pulseMs = SETTINGS.value(QStringLiteral("PLC/SwitchTestDoneResetPulseMs"), 0).toInt();
            const int offset = cfg.mCoilAddressOffset;
            log(QStringLiteral("PLC旋钮测试完成复位: 工位=%1 M%2(addr=%3) PulseMs=%4")
                    .arg(cfg.stationIndex)
                    .arg(resetM)
                    .arg(resetM + offset)
                    .arg(pulseMs));
            QString err;
            if (!session.connectPlc(&err)) {
                resetResult.summary = QStringLiteral("复位线圈连接失败: %1").arg(err);
                log(QStringLiteral("PLC旋钮测试完成复位失败: %1").arg(err));
                return resetResult;
            }
            log(QStringLiteral("PLC旋钮测试完成复位: 写 M%1(addr=%2)=1").arg(resetM).arg(resetM + offset));
            if (!session.writeCoil(resetM, true, &err)) {
                session.disconnect();
                resetResult.summary = QStringLiteral("写复位线圈失败: %1").arg(err);
                log(QStringLiteral("PLC旋钮测试完成复位失败: %1").arg(err));
                return resetResult;
            }
            if (gapMs > 0) {
                QThread::msleep(static_cast<unsigned long>(gapMs));
            }
            if (pulseMs > 0) {
                QThread::msleep(static_cast<unsigned long>(pulseMs));
                log(QStringLiteral("PLC旋钮测试完成复位: 脉冲置0 M%1(addr=%2)").arg(resetM).arg(resetM + offset));
                if (!session.writeCoil(resetM, false, &err)) {
                    session.disconnect();
                    resetResult.summary = QStringLiteral("复位线圈脉冲回0失败: %1").arg(err);
                    log(QStringLiteral("PLC旋钮测试完成复位失败: %1").arg(err));
                    return resetResult;
                }
                if (gapMs > 0) {
                    QThread::msleep(static_cast<unsigned long>(gapMs));
                }
            }
            session.disconnect();
            resetResult.ok = true;
            resetResult.summary = pulseMs > 0 ? QStringLiteral("M%1 复位脉冲%2ms").arg(resetM).arg(pulseMs)
                                              : QStringLiteral("M%1 复位常1").arg(resetM);
            log(QStringLiteral("PLC旋钮测试完成复位通过"));
            return resetResult;
        });
    case PlcV3Command::TouchKey:
        return modbus->withSession([&](PlcModbusSession& session) {
            PlcV3RunResult touchResult;
            session.logSessionSummary(QStringLiteral("V3键%1").arg(params.keyIndex0To6));
            PlcV3TouchKeyOptions options;
            options.pollCapDuringPress = params.pollCapDuringPress;
            const PlcV3TouchResult touch = runPlcV3TouchKey(session, params.keyIndex0To6, options);
            touchResult.ok = touch.ok;
            touchResult.summary = touch.summary;
            if (!touch.ok) {
                log(touch.summary);
            }
            return touchResult;
        });
    case PlcV3Command::TouchSwitch:
        return modbus->withSession([&](PlcModbusSession& session) {
            PlcV3RunResult touchResult;
            session.logSessionSummary(QStringLiteral("V3旋钮"));
            const PlcV3TouchResult touch = runPlcV3TouchSwitch(session);
            touchResult.ok = touch.ok;
            touchResult.summary = touch.summary;
            if (!touch.ok) {
                log(touch.summary);
            }
            return touchResult;
        });
    case PlcV3Command::Disconnect:
        modbus->disconnectPlc();
        result.ok = true;
        return result;
    }
    result.summary = QStringLiteral("未知 PLC 命令");
    return result;
}
