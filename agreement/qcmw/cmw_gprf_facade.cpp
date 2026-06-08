#include "cmw_gprf_facade.h"

#include "Abini.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

void noopLog(const QString&)
{
}

void noopWait(int)
{
}

bool cmwVisaTraceEnabled()
{
    return SETTINGS.value(QStringLiteral("BlePer/CmwVisaTrace"), true).toBool();
}

void ensureRxCmwProfile(Qvisa* visa)
{
    if (visa) {
        visa->set(VisaCmd::DeviceProfile, static_cast<int>(VisaDeviceProfile::RxCmwInstrument));
    }
}

}  // namespace

void CmwGprfFacade::resetSession()
{
    burstDoneSinceStartRx_ = false;
    protocolReady_ = false;
    gprf_.resetPrimed();
}

void CmwGprfFacade::ensureProtocol(Qvisa* visa, const std::function<void(const QString&)>& log,
                                   const std::function<void(int)>& wait)
{
    if (protocolReady_) {
        return;
    }
    const auto logFn = log ? log : noopLog;
    const auto waitFn = wait ? wait : noopWait;
    gprf_ = CmwGprfProtocol(
        [visa, logFn](const QString& cmd) {
            ensureRxCmwProfile(visa);
            const bool trace = cmwVisaTraceEnabled();
            if (trace) {
                logFn(QStringLiteral("[CMW-VISA] >> %1").arg(cmd));
            }
            const bool ok = visa && visa->set(VisaCmd::WriteLine, cmd);
            if (trace) {
                logFn(QStringLiteral("[CMW-VISA] << (write ok=%1)").arg(ok ? QStringLiteral("yes") : QStringLiteral("no")));
            }
            return ok;
        },
        [visa, logFn](const QString& cmd, QString* response) {
            ensureRxCmwProfile(visa);
            const bool trace = cmwVisaTraceEnabled();
            if (trace) {
                logFn(QStringLiteral("[CMW-VISA] >> %1").arg(cmd));
            }
            QString stack;
            QString& ref = response ? *response : stack;
            const bool ok = visa && visa->get(VisaCmd::QueryLine, cmd);
            if (ok) {
                ref = visa->lastQueryResponse();
            }
            if (trace) {
                logFn(QStringLiteral("[CMW-VISA] << %1").arg(ref));
            }
            return ok;
        },
        logFn, waitFn);
    protocolReady_ = true;
}

CmwGprfRunResult CmwGprfFacade::run(CmwGprfCommand command, const CmwGprfRunParams& params)
{
    CmwGprfRunResult result;
    const auto log = params.log ? params.log : noopLog;

    switch (command) {
    case CmwGprfCommand::ResetSession:
        resetSession();
        return result;
    case CmwGprfCommand::BrushBurstOnStopPer: {
        ensureProtocol(params.visa, log, params.wait);
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
        ensureRxCmwProfile(params.visa);
        if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed().isEmpty()) {
            log(QStringLiteral("%1：已启用并联射频但未配置 BlePer/CmwVisaAddress，跳过 CMW").arg(params.scenarioLabel));
            result.skipped = true;
            return result;
        }
        if (params.brushProfile < 0 || params.brushProfile > 5) {
            log(QStringLiteral("%1：无有效 brush profile（请先完成对应「开始接收」），跳过 CMW").arg(params.scenarioLabel));
            result.skipped = true;
            return result;
        }
        if (burstDoneSinceStartRx_) {
            log(QStringLiteral(
                     "%1：本收包周期内「并联CMW播放*MHz」已发过射频，PER 内跳过重复 GPRF（仅发停止接收；无并联播放步时可开 "
                     "FreeInstrument/BleBrushCmwOnStopPer）")
                    .arg(params.scenarioLabel));
            if (params.outRanBurst) {
                *params.outRanBurst = true;
            }
            result.skipped = true;
            return result;
        }
        QString idn;
        if (!params.visa || !params.visa->get(VisaCmd::QueryLine, QStringLiteral("*IDN?"))) {
            result.ok = false;
            result.detail = QStringLiteral("CMW VISA连接失败（%1）")
                                .arg(SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed());
            return result;
        }
        idn = params.visa->lastQueryResponse();
        if (!idn.trimmed().isEmpty()) {
            log(QStringLiteral("并联CMW: %1").arg(idn.trimmed()));
        }
        QString primeErr;
        if (!gprf_.primeGprf(&primeErr)) {
            result.ok = false;
            result.detail = primeErr;
            return result;
        }
        const int mhz = CmwGprfProtocol::brushProfileToMHz(params.brushProfile);
        const QString label =
            QStringLiteral("%1 Profile%2@%3MHz").arg(params.scenarioLabel).arg(params.brushProfile).arg(mhz);
        QString burstErr;
        const bool burstOk = gprf_.runSingleBurstAtMhz(mhz, label, &burstErr, params.alignedPostTrigHoldMs);
        if (!burstOk) {
            result.ok = false;
            result.detail = burstErr;
            return result;
        }
        if (params.outRanBurst) {
            *params.outRanBurst = true;
        }
        if (params.outAlignedWaitDoneByCmw && params.alignedPostTrigHoldMs >= 0
            && !SETTINGS.value(QStringLiteral("BlePer/CmwWaitArbScount"), false).toBool()) {
            *params.outAlignedWaitDoneByCmw = true;
        }
        return result;
    }
    case CmwGprfCommand::ParallelBurstForProfile: {
        ensureProtocol(params.visa, log, params.wait);
        auto setDetail = [&](const QString& s) {
            result.detail = s;
        };
        if (!SETTINGS.value(QStringLiteral("FreeInstrument/BleBrushCmwConcurrent"), false).toBool()) {
            const QString msg = QStringLiteral("跳过：未勾选并联 CMW100（BleBrushCmwConcurrent）");
            log(QStringLiteral("并联CMW %1：%2").arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)).arg(msg));
            setDetail(msg);
            result.skipped = true;
            return result;
        }
        if (params.brushProfile < 0 || params.brushProfile > 5) {
            const QString msg = QStringLiteral("频点无效：%1").arg(params.brushProfile);
            setDetail(msg);
            log(QStringLiteral("并联CMW失败：%1").arg(msg));
            result.ok = false;
            return result;
        }
        ensureRxCmwProfile(params.visa);
        if (SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed().isEmpty()) {
            const QString msg = QStringLiteral("跳过：未配置 BlePer/CmwVisaAddress");
            log(QStringLiteral("并联CMW %1：%2").arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)).arg(msg));
            setDetail(msg);
            result.skipped = true;
            return result;
        }
        if (!params.visa || !params.visa->get(VisaCmd::QueryLine, QStringLiteral("*IDN?"))) {
            const QString err = QStringLiteral("CMW VISA连接失败（%1）")
                                    .arg(SETTINGS.value(QStringLiteral("BlePer/CmwVisaAddress")).toString().trimmed());
            setDetail(err);
            log(QStringLiteral("并联CMW %1：%2").arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)).arg(err));
            result.ok = false;
            return result;
        }
        const QString wfPath = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
        if (wfPath.isEmpty()) {
            log(QStringLiteral("并联CMW %1：CMW ARB 波形未配置（BlePer/CmwWaveformFile 为空）")
                    .arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)));
        } else {
            log(QStringLiteral("并联CMW %1：CMW ARB 波形文件 %2")
                    .arg(CmwGprfProtocol::brushBandLabel(params.brushProfile))
                    .arg(wfPath));
        }
        QString primeErr;
        if (!gprf_.primeGprf(&primeErr)) {
            setDetail(primeErr);
            log(QStringLiteral("并联CMW %1 GPRF初始化失败：%2")
                    .arg(CmwGprfProtocol::brushBandLabel(params.brushProfile))
                    .arg(primeErr));
            result.ok = false;
            return result;
        }
        const int mhz = CmwGprfProtocol::brushProfileToMHz(params.brushProfile);
        const QString label =
            QStringLiteral("并联CMW播放%1@%2MHz").arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)).arg(mhz);
        QString burstErr;
        if (!gprf_.runSingleBurstAtMhz(mhz, label, &burstErr)) {
            setDetail(burstErr);
            log(QStringLiteral("并联CMW %1 失败：%2").arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)).arg(burstErr));
            result.ok = false;
            return result;
        }
        const QString okLine = QStringLiteral("OK %1 %2MHz").arg(CmwGprfProtocol::brushBandLabel(params.brushProfile)).arg(mhz);
        setDetail(okLine);
        log(okLine);
        result.markBurstDoneSinceStartRx = true;
        burstDoneSinceStartRx_ = true;
        return result;
    }
    }
    result.ok = false;
    result.detail = QStringLiteral("未知 CMW 命令");
    return result;
}
