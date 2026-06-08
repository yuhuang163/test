#include "cmw_gprf_facade.h"

#include "Abini.h"

#include <QElapsedTimer>
#include <QtGlobal>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

void noopLog(const QString&) {
}

void noopWait(int) {
}

struct BrushProfileRow {
    int profile;
    int freqMhz;
    const char* bandLabel;
};

constexpr BrushProfileRow kBrushProfiles[] = {
    {0, 2402, "2402_BLE1M"},
    {1, 2440, "2440_BLE1M"},
    {2, 2480, "2480_BLE1M"},
    {3, 2402, "2402_BLE2M"},
    {4, 2440, "2440_BLE2M"},
    {5, 2480, "2480_BLE2M"},
};

const BrushProfileRow& brushProfileRow(int profile) {
    for (const BrushProfileRow& row : kBrushProfiles) {
        if (row.profile == profile) {
            return row;
        }
    }
    return kBrushProfiles[0];
}

} // namespace

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

int CmwGprfFacade::Config::holdMsAfterTrigger(int postTrigHoldMsOverride) const {
    return postTrigHoldMsOverride >= 0 ? postTrigHoldMsOverride : triggerWaitMs;
}

CmwGprfFacade::BrushProfile CmwGprfFacade::BrushProfile::fromProfile(int profile) {
    const BrushProfileRow& row = brushProfileRow(profile);
    BrushProfile out;
    out.profile = row.profile;
    out.freqMhz = row.freqMhz;
    out.bandLabel = QString::fromUtf8(row.bandLabel);
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

void CmwGprfFacade::bindSession(Qvisa* visa, const LogFn& log, const WaitFn& wait) {
    log_ = log ? log : noopLog;
    wait_ = wait ? wait : noopWait;
    if (!sessionReady_) {
        cmw_.bindVisa(visa);
        sessionReady_ = true;
    }
}

bool CmwGprfFacade::cmwSet(CmwGprfCmd cmd, const QVariant& data) {
    return cmw_.set(cmd, data);
}

bool CmwGprfFacade::cmwGet(CmwGprfCmd cmd, const QVariant& param, QString* response) {
    const bool ok = cmw_.get(cmd, param);
    if (response) {
        *response = cmw_.lastQueryResponse();
    }
    return ok;
}

bool CmwGprfFacade::parseArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent) {
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

bool CmwGprfFacade::ensureVisaConnected(Qvisa* visa, const LogFn& log, QString* detail) {
    bindSession(visa, log, wait_);
    QString idn;
    if (cmwGet(CmwGprfCmd::Identity, {}, &idn) && !idn.trimmed().isEmpty()) {
        log_(QStringLiteral("并联CMW: %1").arg(idn.trimmed()));
        return true;
    }
    if (detail) {
        *detail = QStringLiteral("CMW VISA连接失败（%1）").arg(cmwVisaAddress());
    }
    return false;
}

bool CmwGprfFacade::primeGprf(QString* errorMessage) {
    const Config cfg = Config::fromSettings();
    if (!cfg.enableFixedInit) {
        gprfPrimed_ = true;
        return true;
    }
    if (gprfPrimed_) {
        return true;
    }

    cmwSet(CmwGprfCmd::ClearStatus);
    cmwSet(CmwGprfCmd::GenOff);
    cmwSet(CmwGprfCmd::ListOff);
    cmwSet(CmwGprfCmd::BbModeArb);

    if (!cfg.waveformFile.isEmpty()) {
        log_(QStringLiteral("CMW GPRF 加载 ARB 波形文件：%1").arg(cfg.waveformFile));
        cmwSet(CmwGprfCmd::ArbFile, cfg.waveformFile);
        QString arbReadBack;
        if (cmwGet(CmwGprfCmd::ArbFilePath, {}, &arbReadBack)) {
            log_(QStringLiteral("CMW GPRF 仪侧当前波形路径：%1").arg(arbReadBack.trimmed()));
        }
    } else {
        log_(QStringLiteral("CMW GPRF：BlePer/CmwWaveformFile 未配置（首次初始化仍继续，请确认仪上 ARB）"));
        if (cfg.queryCurrentArbFile) {
            QString arbReadBack;
            if (cmwGet(CmwGprfCmd::ArbFilePath, {}, &arbReadBack)) {
                log_(QStringLiteral("CMW GPRF 仪侧波形（未配置本机路径）：%1").arg(arbReadBack.trimmed()));
            }
        }
    }

    cmwSet(CmwGprfCmd::ArbRepetition, cfg.arbRepetition);
    cmwSet(CmwGprfCmd::ArbCycles, qMax(1, cfg.arbCycles));
    cmwSet(CmwGprfCmd::TxLevelDbm, cfg.txPowerDbm);

    if (cfg.checkErrorAfterInit) {
        QString err;
        if (cmwGet(CmwGprfCmd::SystemError, {}, &err) && !err.startsWith(QLatin1Char('0'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("CMW100 GPRF初始化错误: %1").arg(err);
            }
            return false;
        }
    }
    gprfPrimed_ = true;
    return true;
}

bool CmwGprfFacade::waitArbComplete(const QString& scenarioLabel, const Config& cfg, const LogFn& log,
                                     const WaitFn& wait, QString* errorMessage, int* outElapsedMs) {
    Q_UNUSED(log);
    Q_UNUSED(wait);

    QElapsedTimer timer;
    timer.start();
    QString lastResponse;
    int lastCycles = 0;
    int prevCycles = -1;
    while (timer.elapsed() < cfg.arbTimeoutMs) {
        QString response;
        if (!cmwGet(CmwGprfCmd::ArbScount, {}, &response)) {
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
            if (cfg.verboseArbPollLog || cycles != prevCycles) {
                prevCycles = cycles;
                log_(QStringLiteral("CMW100发包进度 %1: time=%2s, cycles=%3, samples=%4")
                         .arg(scenarioLabel)
                         .arg(countTime, 0, 'f', 3)
                         .arg(cycles)
                         .arg(samplesCurrent));
            }
            if (cfg.arbCompleteCycles <= 0 || cycles >= cfg.arbCompleteCycles) {
                if (outElapsedMs) {
                    *outElapsedMs = static_cast<int>(timer.elapsed());
                }
                if (!cfg.verboseArbPollLog) {
                    log_(QStringLiteral("CMW100发包进度 %1：ARB 完成 time=%2s cycles=%3 samples=%4 耗时%5ms")
                             .arg(scenarioLabel)
                             .arg(countTime, 0, 'f', 3)
                             .arg(cycles)
                             .arg(samplesCurrent)
                             .arg(timer.elapsed()));
                }
                return true;
            }
        } else {
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

bool CmwGprfFacade::runSingleBurstAtMhz(int freqMhz, const QString& scenarioLabel, const Config& cfg,
                                        const LogFn& log, const WaitFn& wait, QString* errorMessage,
                                        int postTrigHoldMsOverride) {
    bindSession(nullptr, log, wait);

    if (cfg.waveformFile.isEmpty()) {
        log_(QStringLiteral("[%1] CMW 单次 GPRF：BlePer/CmwWaveformFile 未配置").arg(scenarioLabel));
    } else {
        log_(QStringLiteral("[%1] CMW 单次 GPRF ARB：%2").arg(scenarioLabel).arg(cfg.waveformFile));
    }

    const auto stopGen = [&]() {
        if (!cfg.stopAfterScenario) {
            return;
        }
        cmwSet(CmwGprfCmd::GenOff);
        QString state;
        if (cmwGet(CmwGprfCmd::GenState, {}, &state)) {
            log_(QStringLiteral("CMW100 GPRF状态: %1").arg(state));
        }
    };

    if (cfg.burstStatOffFirst) {
        cmwSet(CmwGprfCmd::GenOff);
    }
    cmwSet(CmwGprfCmd::ClearStatus);
    cmwSet(CmwGprfCmd::FrequencyMhz, freqMhz);
    if (!cfg.useGuiRfConfig) {
        cmwSet(CmwGprfCmd::ArbRepetition, cfg.arbRepetition);
        cmwSet(CmwGprfCmd::ArbCycles, qMax(1, cfg.arbCycles));
    }
    if (cfg.queryCurrentArbFile) {
        QString arbCur;
        if (cmwGet(CmwGprfCmd::ArbFilePath, {}, &arbCur)) {
            log_(QStringLiteral("[%1] 仪侧当前波形：%2").arg(scenarioLabel).arg(arbCur.trimmed()));
        }
    }
    if (!cmwSet(CmwGprfCmd::ManualArbTrigger)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("%1 CMW100触发发包失败").arg(scenarioLabel);
        }
        stopGen();
        return false;
    }
    cmwSet(CmwGprfCmd::GenOn);

    const int holdMs = cfg.holdMsAfterTrigger(postTrigHoldMsOverride);
    if (cfg.waitArbScountOnly) {
        if (postTrigHoldMsOverride >= 0) {
            log_(QStringLiteral("%1：仅以 ARB SCOunt? 轮询待发完（积包 %2 ms 不阻塞 TRIG 后）")
                     .arg(scenarioLabel)
                     .arg(postTrigHoldMsOverride));
        }
        if (!waitArbComplete(scenarioLabel, cfg, log, wait, errorMessage, nullptr)) {
            stopGen();
            return false;
        }
    } else if (cfg.burstPollArbScount) {
        int arbElapsedMs = 0;
        if (!waitArbComplete(scenarioLabel, cfg, log, wait, errorMessage, &arbElapsedMs)) {
            stopGen();
            return false;
        }
        if (arbElapsedMs < holdMs) {
            const int padMs = holdMs - arbElapsedMs;
            if (postTrigHoldMsOverride >= 0) {
                log_(QStringLiteral("%1：SCOunt 完成后再补足积包 %2ms（总≥%3ms）")
                         .arg(scenarioLabel)
                         .arg(padMs)
                         .arg(holdMs));
            }
            if (wait_) {
                wait_(padMs);
            }
        } else if (postTrigHoldMsOverride >= 0) {
            log_(QStringLiteral("%1：ARB 轮询已耗时 %2 ms，不小于积包 %3 ms，跳过补足")
                     .arg(scenarioLabel)
                     .arg(arbElapsedMs)
                     .arg(holdMs));
        }
    } else {
        if (postTrigHoldMsOverride >= 0) {
            log_(QStringLiteral("%1：STAT ON 后仅定时阻塞 %2ms（不轮询 SCOunt）").arg(scenarioLabel).arg(holdMs));
        }
        if (wait_) {
            wait_(holdMs);
        }
    }

    if (cfg.checkErrorAfterScenario) {
        QString err;
        if (cmwGet(CmwGprfCmd::SystemError, {}, &err) && !err.startsWith(QLatin1Char('0'))) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("%1 CMW100错误: %2").arg(scenarioLabel, err);
            }
            stopGen();
            return false;
        }
    }
    stopGen();
    return true;
}

CmwGprfRunResult CmwGprfFacade::runBurstAtProfile(const CmwGprfRunParams& params, const LogFn& log,
                                                   const WaitFn& wait, const QString& burstLabel,
                                                   bool logWaveformHint) {
    CmwGprfRunResult result;
    const BrushProfile brush = BrushProfile::fromProfile(params.brushProfile);
    const Config cfg = Config::fromSettings();

    bindSession(params.visa, log, wait);
    QString primeErr;
    if (!primeGprf(&primeErr)) {
        result.ok = false;
        result.detail = primeErr;
        log_(QStringLiteral("并联CMW %1 GPRF初始化失败：%2").arg(brush.bandLabel).arg(primeErr));
        return result;
    }
    if (logWaveformHint) {
        if (cfg.waveformFile.isEmpty()) {
            log_(QStringLiteral("并联CMW %1：CMW ARB 波形未配置").arg(brush.bandLabel));
        } else {
            log_(QStringLiteral("并联CMW %1：CMW ARB 波形文件 %2").arg(brush.bandLabel).arg(cfg.waveformFile));
        }
    }

    QString burstErr;
    if (!runSingleBurstAtMhz(brush.freqMhz, burstLabel, cfg, log, wait, &burstErr, params.alignedPostTrigHoldMs)) {
        result.ok = false;
        result.detail = burstErr;
        log_(QStringLiteral("并联CMW %1 失败：%2").arg(brush.bandLabel).arg(burstErr));
        return result;
    }
    result.detail = QStringLiteral("OK %1 %2MHz").arg(brush.bandLabel).arg(brush.freqMhz);
    log_(result.detail);
    return result;
}

CmwGprfRunResult CmwGprfFacade::run(CmwGprfCommand command, const CmwGprfRunParams& params) {
    CmwGprfRunResult result;
    const auto log = params.log ? params.log : noopLog;
    const auto wait = params.wait ? params.wait : noopWait;

    switch (command) {
    case CmwGprfCommand::ResetSession:
        resetSession();
        return result;

    case CmwGprfCommand::BrushBurstOnStopPer: {
        bindSession(params.visa, log, wait);
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
            log_(QStringLiteral("%1：已启用并联射频但未配置 BlePer/CmwVisaAddress，跳过 CMW").arg(params.scenarioLabel));
            result.skipped = true;
            return result;
        }
        if (!BrushProfile::isValid(params.brushProfile)) {
            log_(QStringLiteral("%1：无有效 brush profile，跳过 CMW").arg(params.scenarioLabel));
            result.skipped = true;
            return result;
        }
        if (burstDoneSinceStartRx_) {
            log_(QStringLiteral("%1：本收包周期内已发过并联 CMW，PER 内跳过重复 GPRF").arg(params.scenarioLabel));
            if (params.outRanBurst) {
                *params.outRanBurst = true;
            }
            result.skipped = true;
            return result;
        }
        if (!ensureVisaConnected(params.visa, log, &result.detail)) {
            result.ok = false;
            return result;
        }
        const BrushProfile brush = BrushProfile::fromProfile(params.brushProfile);
        const QString burstLabel =
            QStringLiteral("%1 Profile%2@%3MHz").arg(params.scenarioLabel).arg(brush.profile).arg(brush.freqMhz);
        result = runBurstAtProfile(params, log, wait, burstLabel, false);
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
        bindSession(params.visa, log, wait);
        const BrushProfile brush = BrushProfile::fromProfile(params.brushProfile);
        if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool()) {
            const QString msg = QStringLiteral("跳过：未勾选并联 CMW100（BleBrushCmwConcurrent）");
            log_(QStringLiteral("并联CMW %1：%2").arg(brush.bandLabel).arg(msg));
            result.detail = msg;
            result.skipped = true;
            return result;
        }
        if (!BrushProfile::isValid(params.brushProfile)) {
            const QString msg = QStringLiteral("频点无效：%1").arg(params.brushProfile);
            result.detail = msg;
            result.ok = false;
            log_(QStringLiteral("并联CMW失败：%1").arg(msg));
            return result;
        }
        if (cmwVisaAddress().isEmpty()) {
            const QString msg = QStringLiteral("跳过：未配置 BlePer/CmwVisaAddress");
            log_(QStringLiteral("并联CMW %1：%2").arg(brush.bandLabel).arg(msg));
            result.detail = msg;
            result.skipped = true;
            return result;
        }
        if (!ensureVisaConnected(params.visa, log, &result.detail)) {
            result.ok = false;
            log_(QStringLiteral("并联CMW %1：%2").arg(brush.bandLabel).arg(result.detail));
            return result;
        }
        const QString burstLabel = QStringLiteral("并联CMW播放%1@%2MHz").arg(brush.bandLabel).arg(brush.freqMhz);
        result = runBurstAtProfile(params, log, wait, burstLabel, true);
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
