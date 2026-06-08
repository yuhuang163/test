#include "cmw_gprf.h"

#include "Abini.h"

#include <QElapsedTimer>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

CmwGprfProtocol::CmwGprfProtocol()
    : write_([](const QString&) { return false; }), query_([](const QString&, QString*) { return false; }) {
}

CmwGprfProtocol::CmwGprfProtocol(WriteFn write, QueryFn query, LogFn log, WaitFn wait)
    : write_(std::move(write)), query_(std::move(query)), log_(std::move(log)), wait_(std::move(wait)) {
}

void CmwGprfProtocol::logLine(const QString& line) const {
    if (log_) {
        log_(line);
    }
}

QString CmwGprfProtocol::arbFileWriteCommand(const QString& rawPath) {
    const QString p = rawPath.trimmed();
    if (p.startsWith(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE"), Qt::CaseInsensitive)) {
        return p;
    }
    if (p.size() >= 2 && p.front() == QLatin1Char('\'') && p.back() == QLatin1Char('\'')) {
        return QStringLiteral("SOURce:GPRF:GEN:ARB:FILE %1").arg(p);
    }
    QString inner = p;
    if (inner.size() >= 2 && inner.front() == QLatin1Char('"') && inner.back() == QLatin1Char('"')) {
        inner = inner.mid(1, inner.size() - 2).trimmed();
    }
    return QStringLiteral("SOURce:GPRF:GEN:ARB:FILE '%1'").arg(inner);
}

int CmwGprfProtocol::brushProfileToMHz(int profile) {
    switch (profile) {
    case 1:
    case 4:
        return 2440;
    case 2:
    case 5:
        return 2480;
    case 0:
    case 3:
    default:
        return 2402;
    }
}

QString CmwGprfProtocol::brushBandLabel(int profile) {
    switch (profile) {
    case 1:
        return QStringLiteral("2440_BLE1M");
    case 2:
        return QStringLiteral("2480_BLE1M");
    case 3:
        return QStringLiteral("2402_BLE2M");
    case 4:
        return QStringLiteral("2440_BLE2M");
    case 5:
        return QStringLiteral("2480_BLE2M");
    case 0:
    default:
        return QStringLiteral("2402_BLE1M");
    }
}

bool CmwGprfProtocol::parseArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent) {
    const QString clean = response.trimmed().remove(QLatin1Char('"'));
    const QStringList parts = clean.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (parts.size() < 3) {
        return false;
    }
    bool timeOk = false;
    bool cyclesOk = false;
    bool samplesOk = false;
    const double parsedTime = parts.at(0).trimmed().toDouble(&timeOk);
    const int parsedCycles = parts.at(1).trimmed().toInt(&cyclesOk);
    const int parsedSamples = parts.at(2).trimmed().toInt(&samplesOk);
    if (!timeOk || !cyclesOk || !samplesOk) {
        return false;
    }
    if (countTime) {
        *countTime = parsedTime;
    }
    if (cycles) {
        *cycles = parsedCycles;
    }
    if (samplesCurrent) {
        *samplesCurrent = parsedSamples;
    }
    return true;
}

bool CmwGprfProtocol::primeGprf(QString* errorMessage) {
    if (!SETTINGS.value(QStringLiteral("BlePer/CmwEnableFixedInit"), true).toBool()) {
        gprfPrimed_ = true;
        return true;
    }
    if (gprfPrimed_) {
        return true;
    }
    write_(QStringLiteral("*CLS"));
    const int cycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                       SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
            .toInt();
    const QString repetition =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
    const double level = SETTINGS.value(QStringLiteral("BlePer/CmwTxPowerDbm"), -50.0).toDouble();
    QString opc;
    query_(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opc);
    write_(QStringLiteral("SOURce:GPRF:GEN:LIST OFF"));
    write_(QStringLiteral("SOURce:GPRF:GEN:BBMode ARB"));
    const QString waveform = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (!waveform.isEmpty()) {
        logLine(QStringLiteral("CMW GPRF 加载 ARB 波形文件：%1").arg(waveform));
        write_(arbFileWriteCommand(waveform));
        QString arbReadBack;
        if (query_(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &arbReadBack)) {
            logLine(QStringLiteral("CMW GPRF SOURce:GPRF:GEN:ARB:FILE? 仪侧当前波形路径：%1").arg(arbReadBack.trimmed()));
        }
    } else {
        logLine(QStringLiteral("CMW GPRF：BlePer/CmwWaveformFile 未配置（首次初始化仍继续，请确认仪上 ARB）"));
        if (SETTINGS.value(QStringLiteral("BlePer/CmwQueryCurrentArbFile"), true).toBool()) {
            QString arbReadBack;
            if (query_(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &arbReadBack)) {
                logLine(QStringLiteral("CMW GPRF SOURce:GPRF:GEN:ARB:FILE?（未配置本机 BlePer/CmwWaveformFile）仪侧：%1")
                            .arg(arbReadBack.trimmed()));
            }
        }
    }
    write_(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(repetition));
    write_(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(qMax(1, cycles)));
    write_(QStringLiteral("SOURce:GPRF:GEN:RFSettings:LEVel %1").arg(level, 0, 'f', 1));
    QString err;
    if (SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterInit"), false).toBool() &&
        query_(QStringLiteral("SYSTem:ERRor?"), &err) && !err.startsWith(QLatin1Char('0'))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("CMW100 GPRF初始化错误: %1").arg(err);
        }
        return false;
    }
    gprfPrimed_ = true;
    return true;
}

bool CmwGprfProtocol::waitArbComplete(const QString& scenarioLabel, QString* errorMessage, int* outElapsedMs) {
    const int cyclesSetting = SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                                             SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
                                  .toInt();
    const int targetCycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCompleteCycles"), qMax(0, cyclesSetting - 1)).toInt();
    const int pollIntervalMs =
        qMax(50, SETTINGS.value(QStringLiteral("BlePer/CmwArbPollIntervalMs"), 100).toInt());
    const int timeoutMs = qMax(500, SETTINGS.value(QStringLiteral("BlePer/CmwArbTimeoutMs"), 10000).toInt());
    const bool verbosePoll = SETTINGS.value(QStringLiteral("BlePer/CmwVerboseArbPollLog"), false).toBool();
    QElapsedTimer timer;
    timer.start();
    QString lastResponse;
    int lastCycles = 0;
    int prevCycles = -1;
    while (timer.elapsed() < timeoutMs) {
        QString response;
        if (!query_(QStringLiteral("SOURce:GPRF:GEN:ARB:SCOunt?"), &response)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100发包进度查询失败").arg(scenarioLabel);
            }
            if (outElapsedMs) {
                *outElapsedMs = static_cast<int>(timer.elapsed());
            }
            return false;
        }
        lastResponse = response;
        double countTime = 0.0;
        int cycles = 0;
        int samplesCurrent = 0;
        if (parseArbScount(response, &countTime, &cycles, &samplesCurrent)) {
            lastCycles = cycles;
            if (verbosePoll || cycles != prevCycles) {
                prevCycles = cycles;
                logLine(QStringLiteral("CMW100发包进度 %1: time=%2s, cycles=%3, samples=%4")
                            .arg(scenarioLabel)
                            .arg(countTime, 0, 'f', 3)
                            .arg(cycles)
                            .arg(samplesCurrent));
            }
            if (targetCycles <= 0 || cycles >= targetCycles) {
                if (outElapsedMs) {
                    *outElapsedMs = static_cast<int>(timer.elapsed());
                }
                if (!verbosePoll) {
                    logLine(QStringLiteral("CMW100发包进度 %1：ARB 完成 time=%2s cycles=%3 samples=%4 耗时%5ms")
                                .arg(scenarioLabel)
                                .arg(countTime, 0, 'f', 3)
                                .arg(cycles)
                                .arg(samplesCurrent)
                                .arg(timer.elapsed()));
                }
                return true;
            }
        } else {
            logLine(QStringLiteral("CMW100发包进度 %1: 无法解析SCOunt返回 %2").arg(scenarioLabel, response));
        }
        if (wait_) {
            wait_(pollIntervalMs);
        }
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("%1 CMW100 ARB发包超时，最后返回:%2，cycles=%3")
                            .arg(scenarioLabel, lastResponse)
                            .arg(lastCycles);
    }
    if (outElapsedMs) {
        *outElapsedMs = static_cast<int>(timer.elapsed());
    }
    return false;
}

bool CmwGprfProtocol::runSingleBurstAtMhz(int freqMhz, const QString& scenarioLabel, QString* errorMessage,
                                          int postTrigHoldMsOverride) {
    const QString wfPathLog = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (wfPathLog.isEmpty()) {
        logLine(QStringLiteral("[%1] CMW 单次 GPRF：BlePer/CmwWaveformFile 未配置").arg(scenarioLabel));
    } else {
        logLine(QStringLiteral("[%1] CMW 单次 GPRF ARB：BlePer/CmwWaveformFile=%2").arg(scenarioLabel).arg(wfPathLog));
    }

    const int cycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"),
                       SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
            .toInt();
    const auto stopCmwGen = [&]() {
        if (!SETTINGS.value(QStringLiteral("BlePer/CmwStopAfterScenario"), true).toBool()) {
            return;
        }
        QString opc;
        query_(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opc);
        QString state;
        if (query_(QStringLiteral("SOURce:GPRF:GEN:STATe?"), &state)) {
            logLine(QStringLiteral("CMW100 GPRF状态: %1").arg(state));
        }
    };
    if (SETTINGS.value(QStringLiteral("BlePer/CmwBurstStatOffFirst"), true).toBool()) {
        QString opcOff;
        query_(QStringLiteral("SOURce:GPRF:GEN:STATe OFF;*OPC?"), &opcOff);
    }
    write_(QStringLiteral("*CLS"));
    write_(QStringLiteral("SOURce:GPRF:GEN:RFSettings:FREQuency %1MHz").arg(freqMhz));
    if (!SETTINGS.value(QStringLiteral("BlePer/CmwUseGuiRfConfig"), true).toBool()) {
        const QString repetition =
            SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
        write_(QStringLiteral("SOURce:GPRF:GEN:ARB:REPetition %1").arg(repetition));
        write_(QStringLiteral("SOURce:GPRF:GEN:ARB:CYCLes %1").arg(qMax(1, cycles)));
    }
    if (SETTINGS.value(QStringLiteral("BlePer/CmwQueryCurrentArbFile"), true).toBool()) {
        QString arbCur;
        if (query_(QStringLiteral("SOURce:GPRF:GEN:ARB:FILE?"), &arbCur)) {
            logLine(QStringLiteral("[%1] SOURce:GPRF:GEN:ARB:FILE? 仪侧当前波形：%2")
                        .arg(scenarioLabel)
                        .arg(arbCur.trimmed()));
        }
    }
    if (!write_(QStringLiteral("TRIGger:GPRF:GEN:ARB:MANual:EXECute"))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 CMW100触发发包失败").arg(scenarioLabel);
        }
        stopCmwGen();
        return false;
    }
    QString opc;
    query_(QStringLiteral("SOURce:GPRF:GEN:STATe ON;*OPC?"), &opc);

    const int holdMs =
        postTrigHoldMsOverride >= 0 ? postTrigHoldMsOverride : SETTINGS.value(QStringLiteral("BlePer/CmwTriggerWaitMs"), 1000).toInt();
    const bool legacyScountOnly = SETTINGS.value(QStringLiteral("BlePer/CmwWaitArbScount"), false).toBool();
    const bool burstPollPad = SETTINGS.value(QStringLiteral("BlePer/CmwBurstPollArbScount"), true).toBool();

    if (legacyScountOnly) {
        if (postTrigHoldMsOverride >= 0) {
            logLine(QStringLiteral("%1：BlePer/CmwWaitArbScount=真，仅以 ARB SCOunt? 轮询待发完；积包 %2 ms 不用于 TRIG 后阻塞；PER 步仍可能在该窗口后发停止（已与 CMW 顺序解耦）")
                        .arg(scenarioLabel)
                        .arg(postTrigHoldMsOverride));
        }
        if (!waitArbComplete(scenarioLabel, errorMessage, nullptr)) {
            stopCmwGen();
            return false;
        }
    } else if (burstPollPad) {
        int arbElapsedMs = 0;
        if (!waitArbComplete(scenarioLabel, errorMessage, &arbElapsedMs)) {
            stopCmwGen();
            return false;
        }
        if (arbElapsedMs < holdMs) {
            const int padMs = holdMs - arbElapsedMs;
            if (postTrigHoldMsOverride >= 0) {
                logLine(QStringLiteral(
                            "%1：ARB:SCOunt? 已达设定周期后再补足积包 %2ms（总≥配置的积包毫秒 %3）")
                            .arg(scenarioLabel)
                            .arg(padMs)
                            .arg(holdMs));
            }
            if (wait_) {
                wait_(padMs);
            }
        } else if (postTrigHoldMsOverride >= 0) {
            logLine(QStringLiteral("%1：ARB 轮询段已耗时 %2 ms，不小于积包毫秒 %3，跳过补足等待")
                        .arg(scenarioLabel)
                        .arg(arbElapsedMs)
                        .arg(holdMs));
        }
    } else {
        if (postTrigHoldMsOverride >= 0) {
            logLine(QStringLiteral("%1：BlePer/CmwBurstPollArbScount=false，STAT ON 后仅定时阻塞 %2ms（不轮询 SCOunt）")
                        .arg(scenarioLabel)
                        .arg(holdMs));
        }
        if (wait_) {
            wait_(holdMs);
        }
    }
    if (SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterScenario"), false).toBool()) {
        QString err;
        if (query_(QStringLiteral("SYSTem:ERRor?"), &err) && !err.startsWith(QLatin1Char('0'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100错误: %2").arg(scenarioLabel, err);
            }
            stopCmwGen();
            return false;
        }
    }
    stopCmwGen();
    return true;
}
