#include "cmw_gprf_facade.h"

#include "Abini.h"
#include "qscpimanager.h"

#include <QElapsedTimer>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

CmwGprfFacade::Config CmwGprfFacade::Config::fromSettings() {
    Config cfg;
    cfg.enableFixedInit = SETTINGS.value(QStringLiteral("BlePer/CmwEnableFixedInit"), true).toBool();
    cfg.arbCycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCycles"), SETTINGS.value(QStringLiteral("BlePer/TxCount"), 1000).toInt())
            .toInt();
    cfg.arbRepetition = SETTINGS.value(QStringLiteral("BlePer/CmwArbRepetition"), QStringLiteral("SINGle")).toString();
    cfg.txPowerDbm = SETTINGS.value(QStringLiteral("BlePer/CmwTxPowerDbm"), -50.0).toDouble();
    cfg.waveformFile = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    cfg.queryCurrentArbFile = SETTINGS.value(QStringLiteral("BlePer/CmwQueryCurrentArbFile"), true).toBool();
    cfg.checkErrorAfterInit = SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterInit"), false).toBool();
    cfg.arbCompleteCycles =
        SETTINGS.value(QStringLiteral("BlePer/CmwArbCompleteCycles"), qMax(0, cfg.arbCycles - 1)).toInt();
    cfg.arbPollIntervalMs = qMax(50, SETTINGS.value(QStringLiteral("BlePer/CmwArbPollIntervalMs"), 100).toInt());
    cfg.arbTimeoutMs = qMax(500, SETTINGS.value(QStringLiteral("BlePer/CmwArbTimeoutMs"), 10000).toInt());
    cfg.verboseArbPollLog = SETTINGS.value(QStringLiteral("BlePer/CmwVerboseArbPollLog"), false).toBool();
    cfg.stopAfterScenario = SETTINGS.value(QStringLiteral("BlePer/CmwStopAfterScenario"), true).toBool();
    cfg.burstStatOffFirst = SETTINGS.value(QStringLiteral("BlePer/CmwBurstStatOffFirst"), true).toBool();
    cfg.useGuiRfConfig = SETTINGS.value(QStringLiteral("BlePer/CmwUseGuiRfConfig"), true).toBool();
    cfg.triggerWaitMs = SETTINGS.value(QStringLiteral("BlePer/CmwTriggerWaitMs"), 1000).toInt();
    cfg.waitArbScountOnly = SETTINGS.value(QStringLiteral("BlePer/CmwWaitArbScount"), false).toBool();
    cfg.burstPollArbScount = SETTINGS.value(QStringLiteral("BlePer/CmwBurstPollArbScount"), true).toBool();
    cfg.checkErrorAfterScenario = SETTINGS.value(QStringLiteral("BlePer/CmwCheckErrorAfterScenario"), false).toBool();
    return cfg;
}

CmwGprfFacade::BrushProfile CmwGprfFacade::BrushProfile::fromProfile(int profile) {
    struct Row {
        int profile;
        int freqMhz;
        const char* bandLabel;
    };
    static constexpr Row kRows[] = {
        {0, 2402, "2402_BLE1M"},
        {1, 2440, "2440_BLE1M"},
        {2, 2480, "2480_BLE1M"},
        {3, 2402, "2402_BLE2M"},
        {4, 2440, "2440_BLE2M"},
        {5, 2480, "2480_BLE2M"},
    };
    BrushProfile out;
    for (const Row& row : kRows) {
        if (row.profile == profile) {
            out.profile = row.profile;
            out.freqMhz = row.freqMhz;
            out.bandLabel = QString::fromUtf8(row.bandLabel);
            return out;
        }
    }
    out.profile = kRows[0].profile;
    out.freqMhz = kRows[0].freqMhz;
    out.bandLabel = QString::fromUtf8(kRows[0].bandLabel);
    return out;
}

bool CmwGprfFacade::BrushProfile::isValid(int profile) {
    return profile >= 0 && profile <= 5;
}

void CmwGprfFacade::resetSession() {
    burstDoneSinceStartRx_ = false;
    sessionReady_ = false;
    gprfPrimed_ = false;
}

QString CmwGprfFacade::cmwVisaAddress() const {
    return SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed();
}

void CmwGprfFacade::bindSession(QScpiManager* scpi, const LogFn& log, const WaitFn& wait) {
    log_ = log;
    wait_ = wait;
    if (scpi) {
        scpi_ = scpi;
        scpi_->loadCmwVisaFromSettings();
    }
    if (!sessionReady_ && scpi_) {
        sessionReady_ = true;
    }
}

bool CmwGprfFacade::cmwSet(CmwScpiCmd cmd, const QVariant& data) {
    return scpi_ && scpi_->exec(cmd, data);
}

bool CmwGprfFacade::cmwGet(CmwScpiCmd cmd, const QVariant& param, QString* response) {
    const bool ok = scpi_ && scpi_->exec(cmd, param);
    if (response && scpi_) {
        *response = scpi_->lastQueryResponse();
    }
    return ok;
}

bool CmwGprfFacade::ensureVisaConnected(QScpiManager* scpi, const LogFn& log, QString* detail) {
    bindSession(scpi, log, wait_);
    QString idn;
    if (cmwGet(CmwScpiCmd::Identity, {}, &idn) && !idn.trimmed().isEmpty()) {
        if (log_) {
            log_(QStringLiteral("并联CMW: %1").arg(idn.trimmed()));
        }
        return true;
    }
    if (detail) {
        *detail = QStringLiteral("CMW VISA连接失败（%1）").arg(cmwVisaAddress());
    }
    return false;
}

bool CmwGprfFacade::waitArbComplete(const QString& scenarioLabel, const Config& cfg, QString* errorMessage,
                                    int* outElapsedMs) {
    QElapsedTimer timer;
    timer.start();
    QString lastResponse;
    int lastCycles = 0;
    int prevCycles = -1;
    while (timer.elapsed() < cfg.arbTimeoutMs) {
        QString response;
        if (!cmwGet(CmwScpiCmd::ArbScount, {}, &response)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100发包进度查询失败").arg(scenarioLabel);
            }
            if (outElapsedMs) {
                *outElapsedMs = static_cast<int>(timer.elapsed());
            }
            return false;
        }
        lastResponse = response;
        const QString clean = response.trimmed().remove(QLatin1Char('"'));
        const QStringList parts = clean.split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            bool timeOk = false;
            bool cyclesOk = false;
            bool samplesOk = false;
            const double countTime = parts.at(0).trimmed().toDouble(&timeOk);
            const int cycles = parts.at(1).trimmed().toInt(&cyclesOk);
            const int samplesCurrent = parts.at(2).trimmed().toInt(&samplesOk);
            if (timeOk && cyclesOk && samplesOk) {
                lastCycles = cycles;
                if (cfg.verboseArbPollLog || cycles != prevCycles) {
                    prevCycles = cycles;
                    if (log_) {
                        log_(QStringLiteral("CMW100发包进度 %1: time=%2s, cycles=%3, samples=%4")
                                 .arg(scenarioLabel)
                                 .arg(countTime, 0, 'f', 3)
                                 .arg(cycles)
                                 .arg(samplesCurrent));
                    }
                }
                if (cfg.arbCompleteCycles <= 0 || cycles >= cfg.arbCompleteCycles) {
                    if (outElapsedMs) {
                        *outElapsedMs = static_cast<int>(timer.elapsed());
                    }
                    if (!cfg.verboseArbPollLog && log_) {
                        log_(QStringLiteral("CMW100发包进度 %1：ARB 完成 time=%2s cycles=%3 samples=%4 耗时%5ms")
                                 .arg(scenarioLabel)
                                 .arg(countTime, 0, 'f', 3)
                                 .arg(cycles)
                                 .arg(samplesCurrent)
                                 .arg(timer.elapsed()));
                    }
                    return true;
                }
            } else if (log_) {
                log_(QStringLiteral("CMW100发包进度 %1: 无法解析SCOunt返回 %2").arg(scenarioLabel, response));
            }
        } else if (log_) {
            log_(QStringLiteral("CMW100发包进度 %1: 无法解析SCOunt返回 %2").arg(scenarioLabel, response));
        }
        if (wait_) {
            wait_(cfg.arbPollIntervalMs);
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

CmwGprfRunResult CmwGprfFacade::runBurstAtProfile(const CmwGprfRunParams& params, const QString& burstLabel,
                                                  bool logWaveformHint) {
    CmwGprfRunResult result;
    const BrushProfile brush = BrushProfile::fromProfile(params.brushProfile);
    const Config cfg = Config::fromSettings();

    bindSession(params.scpi, params.log, params.wait);

    // GPRF 首次初始化（仅 enableFixedInit 且本会话未 primed）
    if (cfg.enableFixedInit && !gprfPrimed_) {
        cmwSet(CmwScpiCmd::ClearStatus);
        cmwSet(CmwScpiCmd::GenOff);
        cmwSet(CmwScpiCmd::ListOff);
        cmwSet(CmwScpiCmd::BbModeArb);

        if (!cfg.waveformFile.isEmpty()) {
            if (log_) {
                log_(QStringLiteral("CMW GPRF 加载 ARB 波形文件：%1").arg(cfg.waveformFile));
            }
            cmwSet(CmwScpiCmd::ArbFile, cfg.waveformFile);
            QString arbReadBack;
            if (cmwGet(CmwScpiCmd::ArbFilePath, {}, &arbReadBack) && log_) {
                log_(QStringLiteral("CMW GPRF 仪侧当前波形路径：%1").arg(arbReadBack.trimmed()));
            }
        } else {
            if (log_) {
                log_(QStringLiteral("CMW GPRF：BlePer/CmwWaveformFile 未配置（首次初始化仍继续，请确认仪上 ARB）"));
            }
            if (cfg.queryCurrentArbFile) {
                QString arbReadBack;
                if (cmwGet(CmwScpiCmd::ArbFilePath, {}, &arbReadBack) && log_) {
                    log_(QStringLiteral("CMW GPRF 仪侧波形（未配置本机路径）：%1").arg(arbReadBack.trimmed()));
                }
            }
        }

        cmwSet(CmwScpiCmd::ArbRepetition, cfg.arbRepetition);
        cmwSet(CmwScpiCmd::ArbCycles, qMax(1, cfg.arbCycles));
        cmwSet(CmwScpiCmd::TxLevelDbm, cfg.txPowerDbm);

        if (cfg.checkErrorAfterInit) {
            QString err;
            if (cmwGet(CmwScpiCmd::SystemError, {}, &err) && !err.startsWith(QLatin1Char('0'))) {
                result.ok = false;
                result.detail = QStringLiteral("CMW100 GPRF初始化错误: %1").arg(err);
                if (log_) {
                    log_(QStringLiteral("并联CMW %1 GPRF初始化失败：%2").arg(brush.bandLabel).arg(result.detail));
                }
                return result;
            }
        }
        gprfPrimed_ = true;
    } else if (!cfg.enableFixedInit) {
        gprfPrimed_ = true;
    }

    if (logWaveformHint && log_) {
        if (cfg.waveformFile.isEmpty()) {
            log_(QStringLiteral("并联CMW %1：CMW ARB 波形未配置").arg(brush.bandLabel));
        } else {
            log_(QStringLiteral("并联CMW %1：CMW ARB 波形文件 %2").arg(brush.bandLabel).arg(cfg.waveformFile));
        }
    }

    // 单频点一次 GPRF burst
    const int freqMhz = brush.freqMhz;
    const int postTrigHoldMsOverride = params.alignedPostTrigHoldMs;
    QString burstErr;

    if (cfg.waveformFile.isEmpty()) {
        if (log_) {
            log_(QStringLiteral("[%1] CMW 单次 GPRF：BlePer/CmwWaveformFile 未配置").arg(burstLabel));
        }
    } else if (log_) {
        log_(QStringLiteral("[%1] CMW 单次 GPRF ARB：%2").arg(burstLabel).arg(cfg.waveformFile));
    }

    const auto stopGen = [&]() {
        if (!cfg.stopAfterScenario) {
            return;
        }
        cmwSet(CmwScpiCmd::GenOff);
        QString state;
        if (cmwGet(CmwScpiCmd::GenState, {}, &state) && log_) {
            log_(QStringLiteral("CMW100 GPRF状态: %1").arg(state));
        }
    };

    if (cfg.burstStatOffFirst) {
        cmwSet(CmwScpiCmd::GenOff);
    }
    cmwSet(CmwScpiCmd::ClearStatus);
    cmwSet(CmwScpiCmd::FrequencyMhz, freqMhz);
    // 与 cmw100rx「一」对齐：TRIG 前紧挨着下发 REPetition/CYCLes（每频重装，避免仅初始化写一次后失效）
    cmwSet(CmwScpiCmd::ArbRepetition, cfg.arbRepetition);
    cmwSet(CmwScpiCmd::ArbCycles, qMax(1, cfg.arbCycles));
    if (cfg.queryCurrentArbFile) {
        QString arbCur;
        if (cmwGet(CmwScpiCmd::ArbFilePath, {}, &arbCur) && log_) {
            log_(QStringLiteral("[%1] 仪侧当前波形：%2").arg(burstLabel).arg(arbCur.trimmed()));
        }
    }
    // 顺序与参考一致：Manual EXECute → STATe ON（见 docs/开发参考资料/cmw100rx.txt）
    if (!cmwSet(CmwScpiCmd::ManualArbTrigger)) {
        result.ok = false;
        result.detail = QStringLiteral("%1 CMW100触发发包失败").arg(burstLabel);
        if (log_) {
            log_(QStringLiteral("并联CMW %1 失败：%2").arg(brush.bandLabel).arg(result.detail));
        }
        stopGen();
        return result;
    }
    if (!cmwSet(CmwScpiCmd::GenOn)) {
        result.ok = false;
        result.detail = QStringLiteral("%1 CMW100打开发射失败").arg(burstLabel);
        if (log_) {
            log_(QStringLiteral("并联CMW %1 失败：%2").arg(brush.bandLabel).arg(result.detail));
        }
        stopGen();
        return result;
    }

    const int holdMs = postTrigHoldMsOverride >= 0 ? postTrigHoldMsOverride : cfg.triggerWaitMs;
    if (cfg.waitArbScountOnly) {
        if (postTrigHoldMsOverride >= 0 && log_) {
            log_(QStringLiteral("%1：仅以 ARB SCOunt? 轮询待发完（积包 %2 ms 不阻塞 TRIG 后）")
                     .arg(burstLabel)
                     .arg(postTrigHoldMsOverride));
        }
        if (!waitArbComplete(burstLabel, cfg, &burstErr, nullptr)) {
            result.ok = false;
            result.detail = burstErr;
            if (log_) {
                log_(QStringLiteral("并联CMW %1 失败：%2").arg(brush.bandLabel).arg(burstErr));
            }
            stopGen();
            return result;
        }
    } else if (cfg.burstPollArbScount) {
        int arbElapsedMs = 0;
        if (!waitArbComplete(burstLabel, cfg, &burstErr, &arbElapsedMs)) {
            result.ok = false;
            result.detail = burstErr;
            if (log_) {
                log_(QStringLiteral("并联CMW %1 失败：%2").arg(brush.bandLabel).arg(burstErr));
            }
            stopGen();
            return result;
        }
        if (arbElapsedMs < holdMs) {
            const int padMs = holdMs - arbElapsedMs;
            if (postTrigHoldMsOverride >= 0 && log_) {
                log_(QStringLiteral("%1：SCOunt 完成后再补足积包 %2ms（总≥%3ms）")
                         .arg(burstLabel)
                         .arg(padMs)
                         .arg(holdMs));
            }
            if (wait_) {
                wait_(padMs);
            }
        } else if (postTrigHoldMsOverride >= 0 && log_) {
            log_(QStringLiteral("%1：ARB 轮询已耗时 %2 ms，不小于积包 %3 ms，跳过补足")
                     .arg(burstLabel)
                     .arg(arbElapsedMs)
                     .arg(holdMs));
        }
    } else {
        if (postTrigHoldMsOverride >= 0 && log_) {
            log_(QStringLiteral("%1：STAT ON 后仅定时阻塞 %2ms（不轮询 SCOunt）").arg(burstLabel).arg(holdMs));
        }
        if (wait_) {
            wait_(holdMs);
        }
    }

    if (cfg.checkErrorAfterScenario) {
        QString err;
        if (cmwGet(CmwScpiCmd::SystemError, {}, &err) && !err.startsWith(QLatin1Char('0'))) {
            result.ok = false;
            result.detail = QStringLiteral("%1 CMW100错误: %2").arg(burstLabel, err);
            if (log_) {
                log_(QStringLiteral("并联CMW %1 失败：%2").arg(brush.bandLabel).arg(result.detail));
            }
            stopGen();
            return result;
        }
    }
    stopGen();

    result.detail = QStringLiteral("OK %1 %2MHz").arg(brush.bandLabel).arg(brush.freqMhz);
    if (log_) {
        log_(result.detail);
    }
    return result;
}

CmwGprfRunResult CmwGprfFacade::run(CmwGprfCommand command, const CmwGprfRunParams& params) {
    CmwGprfRunResult result;

    switch (command) {
    case CmwGprfCommand::ResetSession:
        resetSession();
        return result;

    case CmwGprfCommand::BrushBurstOnStopPer: {
        bindSession(params.scpi, params.log, params.wait);
        if (params.outRanBurst) {
            *params.outRanBurst = false;
        }
        if (params.outAlignedWaitDoneByCmw) {
            *params.outAlignedWaitDoneByCmw = false;
        }
        if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwOnStopPer"), true).toBool()) {
            result.skipped = true;
            return result;
        }
        if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool()) {
            result.skipped = true;
            return result;
        }
        if (cmwVisaAddress().isEmpty()) {
            if (log_) {
                log_(QStringLiteral("%1：已启用并联射频但未配置 BlePer/CmwVisaAddress，跳过 CMW")
                         .arg(params.scenarioLabel));
            }
            result.skipped = true;
            return result;
        }
        if (!BrushProfile::isValid(params.brushProfile)) {
            if (log_) {
                log_(QStringLiteral("%1：无有效 brush profile，跳过 CMW").arg(params.scenarioLabel));
            }
            result.skipped = true;
            return result;
        }
        if (burstDoneSinceStartRx_) {
            if (log_) {
                log_(QStringLiteral("%1：本收包周期内已发过并联 CMW，PER 内跳过重复 GPRF")
                         .arg(params.scenarioLabel));
            }
            if (params.outRanBurst) {
                *params.outRanBurst = true;
            }
            result.skipped = true;
            return result;
        }
        if (!ensureVisaConnected(params.scpi, params.log, &result.detail)) {
            result.ok = false;
            return result;
        }
        const BrushProfile brush = BrushProfile::fromProfile(params.brushProfile);
        const QString burstLabel =
            QStringLiteral("%1 Profile%2@%3MHz").arg(params.scenarioLabel).arg(brush.profile).arg(brush.freqMhz);
        result = runBurstAtProfile(params, burstLabel, false);
        if (!result.ok) {
            return result;
        }
        if (params.outRanBurst) {
            *params.outRanBurst = true;
        }
        if (params.outAlignedWaitDoneByCmw && params.alignedPostTrigHoldMs >= 0 &&
            !Config::fromSettings().waitArbScountOnly) {
            *params.outAlignedWaitDoneByCmw = true;
        }
        return result;
    }

    case CmwGprfCommand::ParallelBurstForProfile: {
        bindSession(params.scpi, params.log, params.wait);
        const BrushProfile brush = BrushProfile::fromProfile(params.brushProfile);
        if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool()) {
            const QString msg = QStringLiteral("跳过：未勾选并联 CMW100（BleBrushCmwConcurrent）");
            if (log_) {
                log_(QStringLiteral("并联CMW %1：%2").arg(brush.bandLabel).arg(msg));
            }
            result.detail = msg;
            result.skipped = true;
            return result;
        }
        if (!BrushProfile::isValid(params.brushProfile)) {
            const QString msg = QStringLiteral("频点无效：%1").arg(params.brushProfile);
            result.detail = msg;
            result.ok = false;
            if (log_) {
                log_(QStringLiteral("并联CMW失败：%1").arg(msg));
            }
            return result;
        }
        if (cmwVisaAddress().isEmpty()) {
            const QString msg = QStringLiteral("跳过：未配置 BlePer/CmwVisaAddress");
            if (log_) {
                log_(QStringLiteral("并联CMW %1：%2").arg(brush.bandLabel).arg(msg));
            }
            result.detail = msg;
            result.skipped = true;
            return result;
        }
        if (!ensureVisaConnected(params.scpi, params.log, &result.detail)) {
            result.ok = false;
            if (log_) {
                log_(QStringLiteral("并联CMW %1：%2").arg(brush.bandLabel).arg(result.detail));
            }
            return result;
        }
        const QString burstLabel = QStringLiteral("并联CMW播放%1@%2MHz").arg(brush.bandLabel).arg(brush.freqMhz);
        result = runBurstAtProfile(params, burstLabel, true);
        if (!result.ok) {
            return result;
        }
        result.markBurstDoneSinceStartRx = true;
        burstDoneSinceStartRx_ = true;
        return result;
    }
    }

    result.ok = false;
    result.detail = QStringLiteral("未知 CMW 命令");
    return result;
}
