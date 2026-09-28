#include "qfreework.h"
#include "hikvision_scanner_tcp.h"

#include "common_utils.h"

#include <algorithm>

#include <QElapsedTimer>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>
#include <QVector>

#include "qlog.h"
#include "qproduct.h"
#include "qprotocol_types.h"
#include "test_case.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

struct SuctionCycleExtract {
    QVector<double> peaks;
    QVector<int> peakIndexes; // 各周期最低点在 samples 中的下标，供曲线标注
    QVector<double> baselineReturns; // 每个完整周期峰后、下一次入峰前的回基线最高点
};

static SuctionCycleExtract extractSuctionCycles(const QVector<double>& samples, double baseline, double dipStart) {
    SuctionCycleExtract out;
    // 只认「加强段里的最低点」为峰顶：变负=加强追最低点；开始回升=过峰顶立刻记；结束仍在加强则不算
    bool strengthening = false;
    bool minInit = false;
    double minKpa = 0.0;
    int minIndex = 0;
    bool trackingReturn = false;
    bool returnInit = false;
    double returnMaxKpa = 0.0;
    auto finishReturnPlateau = [&]() {
        if (trackingReturn && returnInit)
            out.baselineReturns.append(returnMaxKpa);
        trackingReturn = false;
        returnInit = false;
    };
    for (int i = 0; i < samples.size(); ++i) {
        const double kpa = samples.at(i);
        if (kpa >= baseline) {
            if (trackingReturn) {
                if (!returnInit || kpa > returnMaxKpa) {
                    returnMaxKpa = kpa;
                    returnInit = true;
                }
            }
            strengthening = false;
            minInit = false;
            continue;
        }

        const bool goingDeeper = (i > 0 && kpa < samples.at(i - 1));
        const bool goingShallower = (i > 0 && kpa > samples.at(i - 1));

        if (!strengthening) {
            // 已在衰弱的半截不入；只有继续变负且低于入峰线才开加强
            if (!goingDeeper || kpa >= dipStart)
                continue;
            finishReturnPlateau();
            strengthening = true;
            minKpa = kpa;
            minIndex = i;
            minInit = true;
            continue;
        }

        if (kpa < minKpa) {
            minKpa = kpa;
            minIndex = i;
            minInit = true;
            continue;
        }
        if (goingShallower && minInit && minKpa <= dipStart) {
            // 加强后开始回升 → 最低点即完整峰顶
            out.peaks.append(minKpa);
            out.peakIndexes.append(minIndex);
            trackingReturn = true;
            returnMaxKpa = baseline;
            returnInit = false;
            strengthening = false;
            minInit = false;
        }
    }
    // 采样结束仍在加强：峰顶未出现，不算
    finishReturnPlateau();
    return out;
}

static double pickSuctionValueForRange(const QVector<double>& values, bool hasRange, double lo, double hi) {
    if (values.isEmpty())
        return 0.0;
    if (hasRange) {
        for (double v : values) {
            if (v < lo || v > hi)
                return v;
        }
    }
    return *std::min_element(values.cbegin(), values.cend());
}

static bool suctionBaselineReturnGateRange(const TestCaseDefinition& def, double* lo, double* hi) {
    for (const TestCaseGate& g : TestCaseStore::activeGatesForEvaluation(def)) {
        if (!g.enabled || g.op != TestCaseGateOp::Range)
            continue;
        if (g.field != QLatin1String("baselineReturnKpa"))
            continue;
        double a = g.low;
        double b = g.high;
        GateRegistry::resolveRangeBounds(g, a, b);
        if (lo)
            *lo = a;
        if (hi)
            *hi = b;
        return true;
    }
    return false;
}

static QString formatSuctionReturnStats(const QVector<double>& returns) {
    if (returns.isEmpty())
        return QStringLiteral("回基线次数=0");
    const double lo = *std::min_element(returns.cbegin(), returns.cend());
    const double hi = *std::max_element(returns.cbegin(), returns.cend());
    return QStringLiteral("回基线 %1 次，最低=%2 最高=%3")
        .arg(returns.size())
        .arg(lo, 0, 'f', 3)
        .arg(hi, 0, 'f', 3);
}

void QFreeWork::startPlcKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                                      const QString& enableKey, int keyIndex0To6, bool useCapacitanceRead) {
    if (!SETTINGS.value(enableKey).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = "按键配置未启用";
        stepRuntime_.ask = "请检查配置";
        TestResult = failValue;
        showlog(testName + "失败：按键配置未启用");
        return;
    }

    currentKeyTestName_ = testName;
    currentKeyExpectedKey_ = expectedKey;
    plcKeyBlePlcOkSummary_.clear();
    plcSwitchBlePhase_ = 0;
    plcKeyCapPollMode_ = false;
    currentKeyCapRequestKk_ = -1;
    currentKeyConfiguredId_ = 0;
    resetPlcKeyCapSyncReadState();

    const bool capMode = useCapacitanceRead && keyIndex0To6 >= 0 && keyIndex0To6 <= 5;
    freeWorkKeyWaiting_ = !capMode;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = capMode ? QStringLiteral("PLC整步与读取按键电容") : QStringLiteral("PLC整步与等待按键上报");
    stepRuntime_.ask = SETTINGS.value(expectedKey).toString();

    closeKeyWaitPrompt();
    if (!promptText.isEmpty()) {
        showlog(testName + QStringLiteral("：") + promptText);
    }
    if (capMode) {
        const QString keyIdText = SETTINGS.value(expectedKey).toString().trimmed();
        bool keyIdOk = false;
        const int configuredKeyId = keyIdText.toInt(&keyIdOk);
        if (!keyIdOk || configuredKeyId <= 0) {
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("按键ID配置无效:") + keyIdText;
            TestResult = failValue;
            showlog(testName + QStringLiteral("失败：按键ID配置无效 ") + keyIdText);
            return;
        }
        currentKeyConfiguredId_ = configuredKeyId;
        currentKeyCapRequestKk_ = configuredKeyId - 1;
        lowKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/Low"), 1).toUInt();
        highKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/High"), 65535).toUInt();
        const QString capAsk = QStringLiteral("[%1,%2]").arg(lowKeyCap_).arg(highKeyCap_);
        stepRuntime_.ask = QStringLiteral("KK=%1;ID=%2;电容%3")
                               .arg(currentKeyCapRequestKk_)
                               .arg(configuredKeyId)
                               .arg(capAsk);
        plcKeyCapPollMode_ = true;
        showlog(testName + QStringLiteral("：治具下压期间读取电容 KK=%1（配置ID=%2 减1），卡控%3，读%4次").arg(currentKeyCapRequestKk_).arg(configuredKeyId).arg(capAsk).arg(SETTINGS.value(QStringLiteral("KeyCap/ReadCount"), 3).toInt()));
    } else {
        showlog(testName + QStringLiteral("：已等待协议按键，将执行PLC整步"));
    }

    runPlcV3TouchKeyFull(keyIndex0To6, capMode);

    plcKeyCapPollMode_ = false;
    resetPlcKeyCapSyncReadState();

    if (!capMode) {
        if (!stepRuntime_.pass) {
            ++plcKeyBleWaitSeq_;
            freeWorkKeyWaiting_ = false;
            plcSwitchBlePhase_ = 0;
            closeKeyWaitPrompt();
            plcKeyBlePlcOkSummary_.clear();
            return;
        }
        if (stepRuntime_.done) {
            ++plcKeyBleWaitSeq_;
            freeWorkKeyWaiting_ = false;
            plcSwitchBlePhase_ = 0;
            return;
        }
        waitPlcBleKeyReportBlocking();
    }
}
void QFreeWork::runDongleSuctionSampleStep() {
    applySuctionGateFromStepParam(activeTestCase_.send.param);

    if (dongleSuctionSampleActive_) {
        showlog(QStringLiteral("采集双通道吸力：上一窗采样尚未结束，忽略重复启动"));
        return;
    }

    if (!dongleSerialPort || !dongleSerialPort->isOpen()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("Dongle串口未连接");
        TestResult = failValue;
        showlog(QStringLiteral("采集双通道吸力失败：Dongle 串口未连接"));
        markActiveTestCaseStepDone(false, stepRuntime_.testData, QStringLiteral("失败"));
        return;
    }

    int durationMs = suctionSampleDurationMs_;
    if (durationMs <= 0 && activeTestCase_.timing.commandTimeoutMs > 0)
        durationMs = activeTestCase_.timing.commandTimeoutMs;
    durationMs = qMax(1000, durationMs);
    const int intervalMs = qMax(20, suctionSampleIntervalMs_);
    const bool restoreOff = !dongleSuctionReadEnabled_;

    // 与 BYD suction 工站一致：两口采样最低值为峰值，卡控范围 + |峰差|
    showlog(QStringLiteral("双通道吸力(Dongle)：采样 %1ms，间隔 %2ms，最少完整周期峰 %3（基线≥%4，入峰<%5）%6")
                .arg(durationMs)
                .arg(intervalMs)
                .arg(qMax(1, suctionMinPeakCount_))
                .arg(suctionPeakBaselineKpa_, 0, 'f', 1)
                .arg(suctionPeakDipStartKpa_, 0, 'f', 1)
                .arg(qFuzzyIsNull(suctionOffsetKpa_)
                         ? QString()
                         : QStringLiteral("，修正 %1 kPa（显示/判定=原始+修正）").arg(suctionOffsetKpa_, 0, 'f', 3)));

    dongleSuctionCh1Samples_.clear();
    dongleSuctionCh2Samples_.clear();
    dongleSuctionCh3Samples_.clear();
    dongleSuctionSampleTimeSec_.clear();
    const int reserveN = durationMs / 20 + 64;
    dongleSuctionCh1Samples_.reserve(reserveN);
    dongleSuctionCh2Samples_.reserve(reserveN);
    dongleSuctionCh3Samples_.reserve(reserveN);
    dongleSuctionSampleTimeSec_.reserve(reserveN);
    dongleSuctionLastCh1Kpa_ = 0.0;
    dongleSuctionLastCh2Kpa_ = 0.0;
    dongleSuctionLastCh3Kpa_ = 0.0;
    setDongleSuctionReadEnabled(true);
    // setDongleSuctionReadEnabled 可能 reset 曲线；reserve 放其后
    suctionChartTimeSec_.reserve(reserveN);
    suctionChartLeftKpa_.reserve(reserveN);
    suctionChartRightKpa_.reserve(reserveN);

    // 计时器先于 dongleSuctionSampleActive_ 起，保证首个采样点就有时间
    // 一拖多：不可在 startTask 内 waitWorkIdle 堵 10s（会嵌套泵邻工位并把两边采样串行化）
    dongleSuctionSampleIsDual_ = true;
    dongleSuctionSampleTargetMs_ = durationMs;
    dongleSuctionSampleRestoreOff_ = restoreOff;
    dongleSuctionSampleTimer_.start();
    dongleSuctionSampleActive_ = true;
    Qlog::setBufferedLogFlushDeferred(true);
    if (!dongleSuctionSamplePollTimer_) {
        dongleSuctionSamplePollTimer_ = new QTimer(this);
        QObject::connect(dongleSuctionSamplePollTimer_, &QTimer::timeout, this, [this]() { onDongleSuctionSamplePoll(); });
    }
    dongleSuctionSamplePollTimer_->start(intervalMs);
}

void QFreeWork::abortDongleSuctionSampleWindow(const QString& reasonLog) {
    if (dongleSuctionSamplePollTimer_)
        dongleSuctionSamplePollTimer_->stop();
    dongleSuctionSampleActive_ = false;
    if (dongleSuctionSampleRestoreOff_)
        setDongleSuctionReadEnabled(false);
    Qlog::setBufferedLogFlushDeferred(false);
    finalizeSuctionChartPlot();
    stepRuntime_.done = true;
    stepRuntime_.pass = false;
    stepRuntime_.testData = QStringLiteral("测试中止");
    TestResult = failValue;
    showlog(reasonLog);
    markActiveTestCaseStepDone(false, stepRuntime_.testData, QStringLiteral("失败"));
}

void QFreeWork::onDongleSuctionSamplePoll() {
    if (!dongleSuctionSampleActive_)
        return;
    if (!isTestContinue) {
        abortDongleSuctionSampleWindow(dongleSuctionSampleIsDual_ ? QStringLiteral("采集双通道吸力已中止")
                                                                  : QStringLiteral("采集单通道吸力已中止"));
        return;
    }
    if (dongleSuctionSampleTimer_.elapsed() < dongleSuctionSampleTargetMs_)
        return;
    if (dongleSuctionSamplePollTimer_)
        dongleSuctionSamplePollTimer_->stop();
    dongleSuctionSampleActive_ = false;
    if (dongleSuctionSampleRestoreOff_)
        setDongleSuctionReadEnabled(false);
    Qlog::setBufferedLogFlushDeferred(false);
    finalizeSuctionChartPlot();
    Qlog::setSuctionSamples(getIndex(), dongleSuctionSampleTimeSec_, dongleSuctionCh1Samples_,
                            dongleSuctionCh2Samples_, dongleSuctionCh3Samples_);
    if (dongleSuctionSampleIsDual_)
        finishDongleSuctionSampleDualEval();
    else
        finishDongleSuctionSampleSingleEval();
}

void QFreeWork::finishDongleSuctionSampleDualEval() {
    if (dongleSuctionCh1Samples_.isEmpty() || dongleSuctionCh2Samples_.isEmpty()) {
        showlog(QStringLiteral("采集双通道吸力失败：采样窗口内未收到 CH1/CH2 AT+SUCTION_DATA"));
        markActiveTestCaseStepDone(false, QStringLiteral("无吸力采样点"), QStringLiteral("失败"));
        return;
    }

    // 峰顶=加强段最低点；过峰顶回升后记下；结束仍在加强不算
    const double baseline = suctionPeakBaselineKpa_ + suctionOffsetKpa_;
    const double dipStart = suctionPeakDipStartKpa_ + suctionOffsetKpa_;
    SuctionCycleExtract ch1Cycles = extractSuctionCycles(dongleSuctionCh1Samples_, baseline, dipStart);
    SuctionCycleExtract ch2Cycles = extractSuctionCycles(dongleSuctionCh2Samples_, baseline, dipStart);
    const QVector<double>& ch1Peaks = ch1Cycles.peaks;
    const QVector<double>& ch2Peaks = ch2Cycles.peaks;
    if (!ch1Peaks.isEmpty() || !ch2Peaks.isEmpty()) {
        QStringList stats;
        if (!ch1Peaks.isEmpty()) {
            stats << QStringLiteral("CH1最低峰值 %1 kPa  最高峰值 %2 kPa")
                         .arg(*std::min_element(ch1Peaks.cbegin(), ch1Peaks.cend()), 0, 'f', 3)
                         .arg(*std::max_element(ch1Peaks.cbegin(), ch1Peaks.cend()), 0, 'f', 3);
        }
        if (!ch2Peaks.isEmpty()) {
            stats << QStringLiteral("CH2最低峰值 %1 kPa  最高峰值 %2 kPa")
                         .arg(*std::min_element(ch2Peaks.cbegin(), ch2Peaks.cend()), 0, 'f', 3)
                         .arg(*std::max_element(ch2Peaks.cbegin(), ch2Peaks.cend()), 0, 'f', 3);
        }
        applySuctionChartTestPeakOverlay(ch1Cycles.peakIndexes, ch1Peaks, ch2Cycles.peakIndexes, ch2Peaks,
                                         stats.join(QStringLiteral("\n")));
    }
    const int minPeaks = qMax(1, suctionMinPeakCount_);
    if (ch1Peaks.size() < minPeaks || ch2Peaks.size() < minPeaks) {
        TestResult = failValue;
        const QString detail = QStringLiteral("完整周期峰不足：CH1=%1 CH2=%2，要求≥%3（基线≥%4，入峰<%5）")
                                   .arg(ch1Peaks.size())
                                   .arg(ch2Peaks.size())
                                   .arg(minPeaks)
                                   .arg(baseline, 0, 'f', 1)
                                   .arg(dipStart, 0, 'f', 1);
        showlog(QStringLiteral("采集双通道吸力失败：%1").arg(detail));
        markActiveTestCaseStepDone(false, detail, QStringLiteral("峰数≥%1").arg(minPeaks));
        return;
    }

    ProtocolDongleSuctionPeakData peak;
    peak.ch1PeakKpa = *std::min_element(ch1Peaks.cbegin(), ch1Peaks.cend());
    peak.ch2PeakKpa = *std::min_element(ch2Peaks.cbegin(), ch2Peaks.cend());
    const double ch1Weak = *std::max_element(ch1Peaks.cbegin(), ch1Peaks.cend());
    const double ch2Weak = *std::max_element(ch2Peaks.cbegin(), ch2Peaks.cend());
    peak.sideDiffKpa = qAbs(peak.ch1PeakKpa - peak.ch2PeakKpa);
    peak.peakKpa = peak.ch1PeakKpa;
    peak.highKpa = ch1Weak;
    peak.peakDiffKpa = peak.sideDiffKpa;
    peak.peakCount = qMin(ch1Peaks.size(), ch2Peaks.size());
    QVector<double> bothReturns = ch1Cycles.baselineReturns;
    bothReturns += ch2Cycles.baselineReturns;
    double retLo = -1.0;
    double retHi = 0.0;
    const bool hasRetGate = suctionBaselineReturnGateRange(activeTestCase_, &retLo, &retHi);
    peak.baselineReturnKpa = pickSuctionValueForRange(bothReturns, hasRetGate, retLo, retHi);

    // 各完整周期峰均须落在对应通道允许范围（优先 Gate 分项，否则 Param 目标±容差）
    double ch1Lo = suctionPeakTargetKpa_ - suctionPeakToleranceKpa_;
    double ch1Hi = suctionPeakTargetKpa_ + suctionPeakToleranceKpa_;
    double ch2Lo = ch1Lo;
    double ch2Hi = ch1Hi;
    for (const TestCaseGate& g : TestCaseStore::activeGatesForEvaluation(activeTestCase_)) {
        if (g.op != TestCaseGateOp::Range)
            continue;
        double lo = g.low;
        double hi = g.high;
        GateRegistry::resolveRangeBounds(g, lo, hi);
        if (g.field == QLatin1String("ch1PeakKpa") || g.field == QLatin1String("leftPeakKpa")) {
            ch1Lo = lo;
            ch1Hi = hi;
        } else if (g.field == QLatin1String("ch2PeakKpa") || g.field == QLatin1String("rightPeakKpa")) {
            ch2Lo = lo;
            ch2Hi = hi;
        }
    }
    // 判定只看各通道最强峰（周期峰里的最低点）；单个周期偶发偏差不判失败，
    // 「一直吸住不放气」已由上面的完整周期峰数卡住。
    showlog(QStringLiteral("采样完成：点 %1，CH1完整峰 %2 个 最强=%3（最弱=%4），CH2完整峰 %5 个 最强=%6（最弱=%7），通道峰差=%8；CH1%9；CH2%10")
                .arg(dongleSuctionCh1Samples_.size())
                .arg(ch1Peaks.size())
                .arg(peak.ch1PeakKpa, 0, 'f', 3)
                .arg(ch1Weak, 0, 'f', 3)
                .arg(ch2Peaks.size())
                .arg(peak.ch2PeakKpa, 0, 'f', 3)
                .arg(ch2Weak, 0, 'f', 3)
                .arg(peak.sideDiffKpa, 0, 'f', 3)
                .arg(formatSuctionReturnStats(ch1Cycles.baselineReturns))
                .arg(formatSuctionReturnStats(ch2Cycles.baselineReturns)));

    const QVariant payload = QVariant::fromValue(peak);
    if (activeTestCase_.gate.enabled
        && evaluateActiveTestCaseGate(QStringLiteral("ProtocolDongleSuctionPeakData"), payload))
        return;

    // 未开 Gate：用步骤 Param 的目标±容差，同样只卡最强峰
    const bool ch1Pass = peak.ch1PeakKpa >= ch1Lo && peak.ch1PeakKpa <= ch1Hi;
    const bool ch2Pass = peak.ch2PeakKpa >= ch2Lo && peak.ch2PeakKpa <= ch2Hi;
    const bool diffPass = peak.sideDiffKpa <= suctionPeakDiffMaxKpa_;
    const bool pass = ch1Pass && ch2Pass && diffPass;
    const QString testData =
        QStringLiteral("CH1=%1,CH2=%2").arg(peak.ch1PeakKpa, 0, 'f', 3).arg(peak.ch2PeakKpa, 0, 'f', 3);
    if (!pass) {
        TestResult = failValue;
        showlog(QStringLiteral("吸力卡控失败：最强峰 CH1=%1 允许[%2,%3]，CH2=%4 允许[%5,%6]，通道峰差=%7 允许≤%8")
                    .arg(peak.ch1PeakKpa, 0, 'f', 3)
                    .arg(ch1Lo, 0, 'f', 2)
                    .arg(ch1Hi, 0, 'f', 2)
                    .arg(peak.ch2PeakKpa, 0, 'f', 3)
                    .arg(ch2Lo, 0, 'f', 2)
                    .arg(ch2Hi, 0, 'f', 2)
                    .arg(peak.sideDiffKpa, 0, 'f', 3)
                    .arg(suctionPeakDiffMaxKpa_, 0, 'f', 2));
    } else {
        showlog(QStringLiteral("吸力卡控通过：%1").arg(testData));
    }
    markActiveTestCaseStepDone(pass, testData,
                               QStringLiteral("CH1[%1,%2] CH2[%3,%4]")
                                   .arg(ch1Lo, 0, 'f', 2)
                                   .arg(ch1Hi, 0, 'f', 2)
                                   .arg(ch2Lo, 0, 'f', 2)
                                   .arg(ch2Hi, 0, 'f', 2));
}

void QFreeWork::runDongleSuctionSampleSingleStep() {
    applySuctionGateFromStepParam(activeTestCase_.send.param);

    if (dongleSuctionSampleActive_) {
        showlog(QStringLiteral("采集单通道吸力：上一窗采样尚未结束，忽略重复启动"));
        return;
    }

    if (!dongleSerialPort || !dongleSerialPort->isOpen()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("Dongle串口未连接");
        TestResult = failValue;
        showlog(QStringLiteral("采集单通道吸力失败：Dongle 串口未连接"));
        markActiveTestCaseStepDone(false, stepRuntime_.testData, QStringLiteral("失败"));
        return;
    }

    int durationMs = suctionSampleDurationMs_;
    if (durationMs <= 0 && activeTestCase_.timing.commandTimeoutMs > 0)
        durationMs = activeTestCase_.timing.commandTimeoutMs;
    durationMs = qMax(1000, durationMs);
    const int intervalMs = qMax(20, suctionSampleIntervalMs_);
    const bool restoreOff = !dongleSuctionReadEnabled_;
    const int chIndex = qBound(0, suctionSingleChannelIndex_, 2);
    const QString channelName = QStringLiteral("CH%1").arg(chIndex + 1);

    showlog(QStringLiteral("单通道吸力(Dongle/%1)：采样 %2ms，间隔 %3ms，最少完整周期峰 %4（基线≥%5，入峰<%6）%7")
                .arg(channelName)
                .arg(durationMs)
                .arg(intervalMs)
                .arg(qMax(1, suctionMinPeakCount_))
                .arg(suctionPeakBaselineKpa_, 0, 'f', 1)
                .arg(suctionPeakDipStartKpa_, 0, 'f', 1)
                .arg(qFuzzyIsNull(suctionOffsetKpa_)
                         ? QString()
                         : QStringLiteral("，修正 %1 kPa（显示/判定=原始+修正）").arg(suctionOffsetKpa_, 0, 'f', 3)));

    dongleSuctionCh1Samples_.clear();
    dongleSuctionCh2Samples_.clear();
    dongleSuctionCh3Samples_.clear();
    dongleSuctionSampleTimeSec_.clear();
    const int reserveN = durationMs / 20 + 64;
    dongleSuctionCh1Samples_.reserve(reserveN);
    dongleSuctionCh2Samples_.reserve(reserveN);
    dongleSuctionCh3Samples_.reserve(reserveN);
    dongleSuctionSampleTimeSec_.reserve(reserveN);
    dongleSuctionLastCh1Kpa_ = 0.0;
    dongleSuctionLastCh2Kpa_ = 0.0;
    dongleSuctionLastCh3Kpa_ = 0.0;
    setDongleSuctionReadEnabled(true);
    suctionChartTimeSec_.reserve(reserveN);
    suctionChartLeftKpa_.reserve(reserveN);
    suctionChartRightKpa_.reserve(reserveN);

    // 计时器先于 dongleSuctionSampleActive_ 起，保证首个采样点就有时间
    // 一拖多：异步窗口，beginStep 立刻返回，TotallyTask 可继续推进邻工位
    dongleSuctionSampleIsDual_ = false;
    dongleSuctionSampleTargetMs_ = durationMs;
    dongleSuctionSampleRestoreOff_ = restoreOff;
    dongleSuctionSampleTimer_.start();
    dongleSuctionSampleActive_ = true;
    Qlog::setBufferedLogFlushDeferred(true);
    if (!dongleSuctionSamplePollTimer_) {
        dongleSuctionSamplePollTimer_ = new QTimer(this);
        QObject::connect(dongleSuctionSamplePollTimer_, &QTimer::timeout, this, [this]() { onDongleSuctionSamplePoll(); });
    }
    dongleSuctionSamplePollTimer_->start(intervalMs);
}

void QFreeWork::finishDongleSuctionSampleSingleEval() {
    const int chIndex = qBound(0, suctionSingleChannelIndex_, 2);
    const QString channelName = QStringLiteral("CH%1").arg(chIndex + 1);
    const QVector<double>* samplesPtr = &dongleSuctionCh1Samples_;
    if (chIndex == 1)
        samplesPtr = &dongleSuctionCh2Samples_;
    else if (chIndex == 2)
        samplesPtr = &dongleSuctionCh3Samples_;
    const QVector<double>& samples = *samplesPtr;
    if (samples.isEmpty()) {
        showlog(QStringLiteral("采集单通道吸力失败：采样窗口内未收到 %1 AT+SUCTION_DATA").arg(channelName));
        markActiveTestCaseStepDone(false, QStringLiteral("无吸力采样点"), QStringLiteral("失败"));
        return;
    }

    // 峰顶=加强段最低点；峰值差=各完整峰中最大-最小
    const double baseline = suctionPeakBaselineKpa_ + suctionOffsetKpa_;
    const double dipStart = suctionPeakDipStartKpa_ + suctionOffsetKpa_;
    SuctionCycleExtract cycles = extractSuctionCycles(samples, baseline, dipStart);
    const QVector<double>& cyclePeaks = cycles.peaks;
    const int minPeaks = qMax(1, suctionMinPeakCount_);

    if (!cyclePeaks.isEmpty()) {
        const double strongest = *std::min_element(cyclePeaks.cbegin(), cyclePeaks.cend());
        const double weakest = *std::max_element(cyclePeaks.cbegin(), cyclePeaks.cend());
        const QString stats = QStringLiteral("最低峰值 %1 kPa\n最高峰值 %2 kPa\n峰值差 %3 kPa")
                                  .arg(strongest, 0, 'f', 3)
                                  .arg(weakest, 0, 'f', 3)
                                  .arg(weakest - strongest, 0, 'f', 3);
        QVector<int> ch1Idx, ch2Idx;
        QVector<double> ch1K, ch2K;
        if (chIndex == 1) {
            ch2Idx = cycles.peakIndexes;
            ch2K = cyclePeaks;
        } else {
            ch1Idx = cycles.peakIndexes;
            ch1K = cyclePeaks;
        }
        applySuctionChartTestPeakOverlay(ch1Idx, ch1K, ch2Idx, ch2K, stats);
    }

    if (cyclePeaks.isEmpty()) {
        showlog(QStringLiteral("采集单通道吸力失败：%1 未识别到有效吸气峰值（基线≥%2，入峰<%3）")
                    .arg(channelName)
                    .arg(baseline, 0, 'f', 1)
                    .arg(dipStart, 0, 'f', 1));
        markActiveTestCaseStepDone(false, QStringLiteral("无有效峰值"), QStringLiteral("失败"));
        return;
    }
    if (cyclePeaks.size() < minPeaks) {
        TestResult = failValue;
        const QString detail = QStringLiteral("完整周期峰不足：%1=%2，要求≥%3")
                                   .arg(channelName)
                                   .arg(cyclePeaks.size())
                                   .arg(minPeaks);
        showlog(QStringLiteral("采集单通道吸力失败：%1").arg(detail));
        markActiveTestCaseStepDone(false, detail, QStringLiteral("峰数≥%1").arg(minPeaks));
        return;
    }

    ProtocolDongleSuctionPeakData peak;
    peak.peakKpa = *std::min_element(cyclePeaks.cbegin(), cyclePeaks.cend()); // 最强峰
    peak.highKpa = *std::max_element(cyclePeaks.cbegin(), cyclePeaks.cend()); // 最弱峰
    peak.peakDiffKpa = peak.highKpa - peak.peakKpa;
    peak.ch1PeakKpa = (chIndex == 0) ? peak.peakKpa : 0.0;
    peak.ch2PeakKpa = (chIndex == 1) ? peak.peakKpa : 0.0;
    peak.sideDiffKpa = peak.peakDiffKpa;
    peak.peakCount = cyclePeaks.size();
    double retLo = -1.0;
    double retHi = 0.0;
    const bool hasRetGate = suctionBaselineReturnGateRange(activeTestCase_, &retLo, &retHi);
    peak.baselineReturnKpa = pickSuctionValueForRange(cycles.baselineReturns, hasRetGate, retLo, retHi);

    // 峰值允许范围：优先 Gate 中 peakKpa 分项，否则用步骤 Param 目标±容差
    double peakLo = suctionPeakTargetKpa_ - suctionPeakToleranceKpa_;
    double peakHi = suctionPeakTargetKpa_ + suctionPeakToleranceKpa_;
    for (const TestCaseGate& g : TestCaseStore::activeGatesForEvaluation(activeTestCase_)) {
        if (g.field == QLatin1String("peakKpa") && g.op == TestCaseGateOp::Range) {
            peakLo = g.low;
            peakHi = g.high;
            break;
        }
    }
    // 判定只看最强峰（周期峰里的最低点）；单个周期偶发偏差不判失败
    showlog(QStringLiteral("采样完成：点 %1，完整峰 %2 个（要求≥%3），最强=%4 最弱=%5 峰值差=%6，%7")
                .arg(samples.size())
                .arg(cyclePeaks.size())
                .arg(minPeaks)
                .arg(peak.peakKpa, 0, 'f', 3)
                .arg(peak.highKpa, 0, 'f', 3)
                .arg(peak.peakDiffKpa, 0, 'f', 3)
                .arg(formatSuctionReturnStats(cycles.baselineReturns)));

    const QVariant payload = QVariant::fromValue(peak);
    if (activeTestCase_.gate.enabled
        && evaluateActiveTestCaseGate(QStringLiteral("ProtocolDongleSuctionPeakData"), payload))
        return;

    // 未开 Gate：只卡最强峰是否在范围内
    const bool peakPass = peak.peakKpa >= peakLo && peak.peakKpa <= peakHi;
    const bool diffPass = peak.peakDiffKpa <= suctionPeakDiffMaxKpa_;
    const bool pass = peakPass && diffPass;
    const QString testData = QStringLiteral("%1=%2").arg(channelName).arg(peak.peakKpa, 0, 'f', 3);
    if (!pass) {
        TestResult = failValue;
        showlog(QStringLiteral("吸力卡控失败：最强峰=%1 允许[%2,%3]，峰值差=%4 允许≤%5")
                    .arg(peak.peakKpa, 0, 'f', 3)
                    .arg(peakLo, 0, 'f', 2)
                    .arg(peakHi, 0, 'f', 2)
                    .arg(peak.peakDiffKpa, 0, 'f', 3)
                    .arg(suctionPeakDiffMaxKpa_, 0, 'f', 2));
    } else {
        showlog(QStringLiteral("吸力卡控通过：%1").arg(testData));
    }
    markActiveTestCaseStepDone(pass, testData,
                               QStringLiteral("[%1,%2]").arg(peakLo, 0, 'f', 2).arg(peakHi, 0, 'f', 2));
}
void QFreeWork::resetLightSensorFivePointState() {
    for (int i = 0; i < 5; ++i) {
        lightCalibFivePointValues_[i] = 0;
        lightCalibWrittenValues_[i] = 0;
    }
    // 缺省与产品约定一致；各读取点 ini 再覆盖本点 Param_range*/diffMin
    const int kLo[5] = {0, 70, 170, 500, 1000};
    const int kHi[5] = {70, 200, 500, 1200, 1700};
    const int kHiInc[5] = {0, 0, 0, 0, 1};
    const int kDiff[4] = {20, 20, 50, 100};
    for (int i = 0; i < 5; ++i) {
        lightCalibRangeLo_[i] = kLo[i];
        lightCalibRangeHi_[i] = kHi[i];
        lightCalibRangeHiInc_[i] = kHiInc[i];
    }
    for (int i = 0; i < 4; ++i)
        lightCalibDiffMin_[i] = kDiff[i];
    lightCalibFivePointMask_ = 0;
}

bool QFreeWork::evaluateLightSensorFivePointRule(QString* detailOut) {
    // 仅判定已采到的点：范围；相邻两点都齐时再判差值（支持中途提前 FAIL）
    QStringList vals;
    for (int i = 0; i < 5; ++i) {
        if (lightCalibFivePointMask_ & static_cast<quint8>(1u << i))
            vals << QString::number(lightCalibFivePointValues_[i]);
    }
    const QString valuesText = vals.join(QStringLiteral(","));

    QStringList failParts;
    for (int i = 0; i < 5; ++i) {
        if (!(lightCalibFivePointMask_ & static_cast<quint8>(1u << i)))
            continue;
        const int v = lightCalibFivePointValues_[i];
        const int lo = lightCalibRangeLo_[i];
        const int hi = lightCalibRangeHi_[i];
        const bool hiInclusive = lightCalibRangeHiInc_[i] != 0;
        const bool inRange = hiInclusive ? (v >= lo && v <= hi) : (v >= lo && v < hi);
        if (!inRange) {
            failParts << (hiInclusive ? QStringLiteral("回读[%1]=%2∉[%3,%4]").arg(i).arg(v).arg(lo).arg(hi)
                                      : QStringLiteral("回读[%1]=%2∉[%3,%4)").arg(i).arg(v).arg(lo).arg(hi));
        }
    }
    for (int i = 0; i < 4; ++i) {
        const quint8 bitLo = static_cast<quint8>(1u << i);
        const quint8 bitHi = static_cast<quint8>(1u << (i + 1));
        if ((lightCalibFivePointMask_ & bitLo) == 0 || (lightCalibFivePointMask_ & bitHi) == 0)
            continue;
        const int diff = lightCalibFivePointValues_[i + 1] - lightCalibFivePointValues_[i];
        if (!(diff > lightCalibDiffMin_[i])) {
            failParts << QStringLiteral("回读[%1]-回读[%2]=%3≤%4")
                             .arg(i + 1)
                             .arg(i)
                             .arg(diff)
                             .arg(lightCalibDiffMin_[i]);
        }
    }

    const bool pass = failParts.isEmpty();
    if (detailOut) {
        if (pass)
            *detailOut = QStringLiteral("回读[%1] 范围+差值通过").arg(valuesText);
        else
            *detailOut = QStringLiteral("回读[%1] 卡控失败：%2").arg(valuesText, failParts.join(QStringLiteral("; ")));
    }
    return pass;
}
bool QFreeWork::runLightSensorCalibStepCore(LightCalibStepKind kind, QString* failReason, QString* lineOut) {
    QVariantMap map;
    if (activeTestCase_.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();

    const auto mapInt = [&map](const QString& key, int defVal) {
        return map.contains(key) ? map.value(key).toInt() : defVal;
    };

    const int golden = mapInt(QStringLiteral("golden"), -1);
    const int index = qBound(0, mapInt(QStringLiteral("index"), 0), 19);
    const int waitMs = qMax(500, mapInt(QStringLiteral("waitMs"), 5000));
    const int sampleCount = qBound(1, mapInt(QStringLiteral("minSamples"), 10), 50);
    const bool verifyWrite = mapInt(QStringLiteral("verifyWrite"), 1) != 0;
    const bool enableFivePointJudge = mapInt(QStringLiteral("enableFivePointJudge"), 1) != 0;
    const int cmdTimeoutMs =
        qMax(500, activeTestCase_.timing.commandTimeoutMs > 0 ? activeTestCase_.timing.commandTimeoutMs : 3000);

    const bool doSampleWrite = kind == LightCalibStepKind::WriteOnly || kind == LightCalibStepKind::WriteAndRead;
    const bool doRead = kind == LightCalibStepKind::ReadOnly || kind == LightCalibStepKind::WriteAndRead;

    if (doSampleWrite) {
        showlog(QStringLiteral("产品光感写入 index=%1 采%2点取平均%3")
                    .arg(index)
                    .arg(sampleCount)
                    .arg(golden >= 0 ? QStringLiteral(" 金样=%1").arg(golden) : QString()));
        if (index == 0)
            resetLightSensorFivePointState();
    } else {
        showlog(QStringLiteral("产品光感回读 index=%1").arg(index));
    }

    const auto waitProductCmd = [&]() -> bool {
        QElapsedTimer t;
        t.start();
        while (commandRetryTimer && isTestContinue && t.elapsed() < cmdTimeoutMs + 800)
            waitWork(20);
        lastCommandRetryCount = 0;
        if (stepRuntime_.done && !stepRuntime_.pass)
            return false;
        return commandRetryTimer == nullptr && !sendRetryOver && isTestContinue;
    };
    const auto sendProductSet = [&](DeviceCmd cmd, const QVariant& param) -> bool {
        setCommandWaitSource(CommandWaitSource::ProductProtocol);
        sendCommandWithRetry([this, cmd, param]() { protocolManager.set(cmd, param); }, cmdTimeoutMs);
        return waitProductCmd();
    };
    const auto sendProductGet = [&](DeviceCmd cmd, const QVariant& param) -> bool {
        setCommandWaitSource(CommandWaitSource::ProductProtocol);
        sendCommandWithRetry([this, cmd, param]() { protocolManager.get(cmd, param); }, cmdTimeoutMs);
        return waitProductCmd();
    };

    bool allOk = true;
    QString reason;
    int dutAvg = 0;
    int readValue = 0;

    if (!isTestContinue) {
        allOk = false;
        reason = QStringLiteral("测试中止");
    } else if (doSampleWrite) {
        QVariantMap offMap;
        offMap.insert(QStringLiteral("start"), 0);
        // 写入+回读合一（旧 Hook）仍在本步内开关上报；单独写入步骤由流程里「开启/关闭光感上报」负责
        const bool controlReport = (kind == LightCalibStepKind::WriteAndRead)
            || mapInt(QStringLiteral("controlReport"), 0) != 0;
        if (controlReport) {
            if (!sendProductSet(DeviceCmd::LightReportControl, offMap)) {
                allOk = false;
                reason = QStringLiteral("关闭光感上报失败");
            } else {
                QVariantMap onMap;
                onMap.insert(QStringLiteral("start"), 1);
                if (!sendProductSet(DeviceCmd::LightReportControl, onMap)) {
                    allOk = false;
                    reason = QStringLiteral("开启光感上报失败");
                }
            }
        }

        if (allOk) {
            lightSensorSamples_.clear();
            lightSensorCollecting_ = true;
            QElapsedTimer wt;
            wt.start();
            while (isTestContinue && wt.elapsed() < waitMs && lightSensorSamples_.size() < sampleCount)
                waitWork(20);
            lightSensorCollecting_ = false;
            if (lightSensorSamples_.size() < sampleCount) {
                allOk = false;
                reason = QStringLiteral("index=%1 光感上报不足 %2 点，实际 %3（请确认流程已「开启光感上报」）")
                             .arg(index)
                             .arg(sampleCount)
                             .arg(lightSensorSamples_.size());
            } else {
                qint64 sum = 0;
                QStringList allTxt;
                for (int i = 0; i < sampleCount; ++i) {
                    sum += lightSensorSamples_.at(i);
                    allTxt << QString::number(lightSensorSamples_.at(i));
                }
                dutAvg = static_cast<int>(qRound(static_cast<double>(sum) / sampleCount));
                showlog(QStringLiteral("index=%1 samples=[%2] avg=%3")
                            .arg(index)
                            .arg(allTxt.join(QStringLiteral(", ")))
                            .arg(dutAvg));
                QVariantMap wmap;
                wmap.insert(QStringLiteral("index"), index);
                wmap.insert(QStringLiteral("value"), dutAvg);
                showlog(QStringLiteral("写入 LightCalibWrite index=%1 value=%2").arg(index).arg(dutAvg));
                if (!sendProductSet(DeviceCmd::LightCalibWrite, wmap)) {
                    allOk = false;
                    reason = QStringLiteral("写入校准失败 index=%1").arg(index);
                } else if (index >= 0 && index < 5) {
                    lightCalibWrittenValues_[index] = dutAvg;
                }
            }
            if (controlReport)
                sendProductSet(DeviceCmd::LightReportControl, offMap);
        }
        lightSensorCollecting_ = false;
    }

    if (allOk && doRead) {
        lightCalibReadValid_ = false;
        QVariantMap rmap;
        rmap.insert(QStringLiteral("index"), index);
        if (!sendProductGet(DeviceCmd::LightCalibRead, rmap) || !lightCalibReadValid_) {
            allOk = false;
            reason = QStringLiteral("回读校准失败 index=%1").arg(index);
        } else {
            readValue = lightCalibReadValue_;
            showlog(QStringLiteral("回读 index=%1 value=%2").arg(index).arg(readValue));
            if (doSampleWrite && readValue != dutAvg) {
                allOk = false;
                reason = QStringLiteral("回读不一致 index=%1 write=%2 read=%3")
                             .arg(index)
                             .arg(dutAvg)
                             .arg(readValue);
            } else if (!doSampleWrite && verifyWrite && index >= 0 && index < 5
                       && readValue != lightCalibWrittenValues_[index]) {
                allOk = false;
                reason = QStringLiteral("回读与写入不一致 index=%1 write=%2 read=%3")
                             .arg(index)
                             .arg(lightCalibWrittenValues_[index])
                             .arg(readValue);
            }
        }
    }

    if (!allOk) {
        if (failReason)
            *failReason = reason;
        return false;
    }

    QString line = doRead ? QStringLiteral("index=%1 read=%2").arg(index).arg(readValue)
                          : QStringLiteral("index=%1 write=%2").arg(index).arg(dutAvg);
    if (doRead && index >= 0 && index < 5) {
        lightCalibFivePointValues_[index] = readValue;
        lightCalibFivePointMask_ |= static_cast<quint8>(1u << index);
        // 本点卡控写在本步 ini：Param_rangeLo/Hi/HiInc；index>0 时 Param_diffMin 相对上一点
        const int kLoDef[5] = {0, 70, 170, 500, 1000};
        const int kHiDef[5] = {70, 200, 500, 1200, 1700};
        const int kHiIncDef[5] = {0, 0, 0, 0, 1};
        const int kDiffDef[4] = {20, 20, 50, 100};
        lightCalibRangeLo_[index] = mapInt(QStringLiteral("rangeLo"), kLoDef[index]);
        lightCalibRangeHi_[index] = mapInt(QStringLiteral("rangeHi"), kHiDef[index]);
        lightCalibRangeHiInc_[index] = mapInt(QStringLiteral("rangeHiInc"), kHiIncDef[index]);
        if (index > 0)
            lightCalibDiffMin_[index - 1] = mapInt(QStringLiteral("diffMin"), kDiffDef[index - 1]);
        showlog(QStringLiteral("index=%1 卡控缓存 range=[%2,%3%4]%5")
                    .arg(index)
                    .arg(lightCalibRangeLo_[index])
                    .arg(lightCalibRangeHi_[index])
                    .arg(lightCalibRangeHiInc_[index] ? QStringLiteral("]") : QStringLiteral(")"))
                    .arg(index > 0 ? QStringLiteral(" diffMin>%1").arg(lightCalibDiffMin_[index - 1]) : QString()));
    }
    // enableFivePointJudge=1：对本步及已采点做范围/差值卡控，不合格立刻 FAIL（不必等五点齐）
    if (doRead && enableFivePointJudge && index >= 0 && index < 5
        && (lightCalibFivePointMask_ & static_cast<quint8>(1u << index))) {
        QString judgeDetail;
        const bool judgePass = evaluateLightSensorFivePointRule(&judgeDetail);
        if (!judgePass) {
            showlog(judgeDetail);
            if (failReason)
                *failReason = judgeDetail;
            if (lineOut)
                *lineOut = judgeDetail;
            return false;
        }
        if (lightCalibFivePointMask_ == 0x1F) {
            showlog(judgeDetail);
            line = judgeDetail;
        }
    }

    if (lineOut)
        *lineOut = line;
    return true;
}
void QFreeWork::finishLightSensorCalibStep(LightCalibStepKind kind, bool ok, const QString& detail) {
    if (!ok) {
        TestResult = failValue;
        markActiveTestCaseStepDone(false, detail, QStringLiteral("失败"));
        const QString action = kind == LightCalibStepKind::ReadOnly ? QStringLiteral("回读") : QStringLiteral("校准");
        showlog(QStringLiteral("光感%1失败：%2").arg(action, detail));
        return;
    }
    showlog(detail);
    markActiveTestCaseStepDone(true, detail, QStringLiteral("通过"));
}

void QFreeWork::runLightSensorCalibWriteStep() {
    QString failReason;
    QString line;
    const bool ok = runLightSensorCalibStepCore(LightCalibStepKind::WriteOnly, &failReason, &line);
    finishLightSensorCalibStep(LightCalibStepKind::WriteOnly, ok, ok ? line : failReason);
}

void QFreeWork::runLightSensorCalibReadStep() {
    QString failReason;
    QString line;
    const bool ok = runLightSensorCalibStepCore(LightCalibStepKind::ReadOnly, &failReason, &line);
    if (!ok && failReason.contains(QStringLiteral("卡控失败"))) {
        TestResult = failValue;
        markActiveTestCaseStepDone(false, failReason, QStringLiteral("失败"));
        showlog(QStringLiteral("光感卡控失败 → 产品不合格"));
        return;
    }
    finishLightSensorCalibStep(LightCalibStepKind::ReadOnly, ok, ok ? line : failReason);
}

void QFreeWork::runLightSensorGoldenCalibStep() {
    QString failReason;
    QString line;
    const bool ok = runLightSensorCalibStepCore(LightCalibStepKind::WriteAndRead, &failReason, &line);
    if (!ok && failReason.contains(QStringLiteral("卡控失败"))) {
        TestResult = failValue;
        markActiveTestCaseStepDone(false, failReason, QStringLiteral("失败"));
        showlog(QStringLiteral("光感卡控失败 → 产品不合格"));
        return;
    }
    finishLightSensorCalibStep(LightCalibStepKind::WriteAndRead, ok, ok ? line : failReason);
}
void QFreeWork::runVesCh1SetBrightnessStep() {
    QVariantMap map;
    if (activeTestCase_.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();
    int brightness = 22;
    if (map.contains(QStringLiteral("brightness")))
        brightness = map.value(QStringLiteral("brightness")).toInt();
    else if (map.contains(QStringLiteral("current")))
        brightness = map.value(QStringLiteral("current")).toInt();
    brightness = qBound(0, brightness, 255);
    QString failReason;
    if (!sendVesCh1BrightnessOnFixture(brightness, &failReason)) {
        markActiveTestCaseStepDone(false, failReason, QStringLiteral("失败"));
        showlog(QStringLiteral("VES 设亮度失败：%1").arg(failReason));
        return;
    }
    markActiveTestCaseStepDone(true, QString::number(brightness), QStringLiteral("通过"));
}

void QFreeWork::runAirPressureSampleStep() {
    QVariantMap map;
    if (activeTestCase_.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();

    const auto mapInt = [&map](const QString& key, int defVal) {
        return map.contains(key) ? map.value(key).toInt() : defVal;
    };

    const int sampleCount = qBound(1, mapInt(QStringLiteral("sampleCount"), 10), 50);
    const int intervalMs = qMax(20, mapInt(QStringLiteral("intervalMs"), 100));
    int waitMs = mapInt(QStringLiteral("waitMs"), 0);
    if (waitMs <= 0 && activeTestCase_.timing.commandTimeoutMs > 0)
        waitMs = activeTestCase_.timing.commandTimeoutMs;
    waitMs = qMax(1000, waitMs);
    const int cmdTimeoutMs =
        qMax(500, activeTestCase_.timing.commandTimeoutMs > 0 ? activeTestCase_.timing.commandTimeoutMs : 3000);

    airPressureTargetCount_ = sampleCount;
    const QString channelRaw = map.value(QStringLiteral("channel"), QStringLiteral("left")).toString().trimmed();
    const QString channelLower = channelRaw.toLower();
    airPressureSampleBothChannels_ =
        channelLower == QStringLiteral("both") || channelLower == QStringLiteral("all")
        || channelLower == QStringLiteral("left,right") || channelLower == QStringLiteral("lr");
    if (airPressureSampleBothChannels_) {
        airPressureSampleChannel_ = QStringLiteral("both");
    } else {
        airPressureSampleChannel_ = channelRaw.isEmpty() ? QStringLiteral("left") : channelRaw;
    }

    const auto airPressureSamplesReady = [&]() {
        if (airPressureSampleBothChannels_)
            return airPressureSamples_.size() >= sampleCount && airPressureRightSamples_.size() >= sampleCount;
        return airPressureSamples_.size() >= sampleCount;
    };

    double gateLo = SETTINGS.value(QStringLiteral("AirPressure/LowKpa"), -50.0).toDouble();
    double gateHi = SETTINGS.value(QStringLiteral("AirPressure/HighKpa"), 50.0).toDouble();
    for (const TestCaseGate& g : TestCaseStore::activeGatesForEvaluation(activeTestCase_)) {
        if (!g.enabled || g.op != TestCaseGateOp::Range)
            continue;
        gateLo = g.low;
        gateHi = g.high;
        GateRegistry::resolveRangeBounds(g, gateLo, gateHi);
        break;
    }

    const auto waitProductCmd = [&]() -> bool {
        QElapsedTimer t;
        t.start();
        while (commandRetryTimer && isTestContinue && t.elapsed() < cmdTimeoutMs + 800)
            waitWork(20);
        lastCommandRetryCount = 0;
        if (stepRuntime_.done && !stepRuntime_.pass)
            return false;
        return commandRetryTimer == nullptr && !sendRetryOver && isTestContinue;
    };
    const auto sendProductSet = [&](DeviceCmd cmd, const QVariant& param) -> bool {
        setCommandWaitSource(CommandWaitSource::ProductProtocol);
        sendCommandWithRetry([this, cmd, param]() { protocolManager.set(cmd, param); }, cmdTimeoutMs);
        return waitProductCmd();
    };

    showlog(QStringLiteral("气压采样：采 %1 点，间隔 %2ms，通道 %3，逐点卡控 [%4,%5] kPa（不取平均）")
                .arg(sampleCount)
                .arg(intervalMs)
                .arg(airPressureSampleChannel_)
                .arg(gateLo, 0, 'f', 3)
                .arg(gateHi, 0, 'f', 3));

    QVariantMap intervalMap;
    intervalMap.insert(QStringLiteral("intervalMs"), intervalMs);
    if (!sendProductSet(DeviceCmd::PressSampleIntervalSet, intervalMap)) {
        markActiveTestCaseStepDone(false, QStringLiteral("设置采样间隔失败"), QStringLiteral("失败"));
        return;
    }

    airPressureSamples_.clear();
    airPressureRightSamples_.clear();
    QVariantMap onMap;
    onMap.insert(QStringLiteral("on"), 1);
    if (!sendProductSet(DeviceCmd::PressCollect, onMap)) {
        markActiveTestCaseStepDone(false, QStringLiteral("开启气压采样失败"), QStringLiteral("失败"));
        return;
    }

    airPressureCollecting_ = true;
    QElapsedTimer wt;
    wt.start();
    while (isTestContinue && wt.elapsed() < waitMs && !airPressureSamplesReady())
        waitWork(20);
    airPressureCollecting_ = false;

    QVariantMap offMap;
    offMap.insert(QStringLiteral("on"), 0);
    sendProductSet(DeviceCmd::PressCollect, offMap);

    if (!isTestContinue) {
        markActiveTestCaseStepDone(false, QStringLiteral("测试中止"), QStringLiteral("失败"));
        return;
    }
    if (!airPressureSamplesReady()) {
        const QString detail =
            airPressureSampleBothChannels_
                ? QStringLiteral("气压上报不足 %1 点，左 %2 右 %3")
                      .arg(sampleCount)
                      .arg(airPressureSamples_.size())
                      .arg(airPressureRightSamples_.size())
                : QStringLiteral("气压上报不足 %1 点，实际 %2")
                      .arg(sampleCount)
                      .arg(airPressureSamples_.size());
        TestResult = failValue;
        markActiveTestCaseStepDone(false, detail, QStringLiteral("失败"));
        showlog(QStringLiteral("气压采样失败：%1").arg(detail));
        return;
    }

    bool pass = true;
    QStringList failParts;
    QStringList valueParts;
    QVector<TestItem> rows;
    rows.reserve(airPressureSampleBothChannels_ ? sampleCount * 2 : sampleCount);
    const QString ask = QStringLiteral("[%1,%2] kPa").arg(gateLo, 0, 'f', 3).arg(gateHi, 0, 'f', 3);
    const auto appendPressureRow = [&](const QString& label, double v) {
        const bool subPass = v >= gateLo && v <= gateHi;
        if (!subPass) {
            pass = false;
            failParts << QStringLiteral("%1=%2").arg(label).arg(v, 0, 'f', 3);
        }
        TestItem item;
        item.testItem = label;
        item.testData = QString::number(v, 'f', 3);
        item.ask = ask;
        item.testResult = subPass ? passValue : failValue;
        rows.append(item);
    };
    for (int i = 0; i < sampleCount; ++i) {
        if (airPressureSampleBothChannels_) {
            const double left = airPressureSamples_.at(i);
            const double right = airPressureRightSamples_.at(i);
            appendPressureRow(QStringLiteral("左气压%1").arg(i + 1), left);
            appendPressureRow(QStringLiteral("右气压%1").arg(i + 1), right);
            valueParts << QStringLiteral("L%1=%2,R%1=%3")
                              .arg(i + 1)
                              .arg(left, 0, 'f', 3)
                              .arg(right, 0, 'f', 3);
        } else {
            const double v = airPressureSamples_.at(i);
            valueParts << QString::number(v, 'f', 3);
            appendPressureRow(QStringLiteral("气压%1").arg(i + 1), v);
        }
    }
    testResultTableUpdate(rows);
    testCaseMultiGateTableEmitted_ = true;

    const QString testData = valueParts.join(QStringLiteral(","));
    if (!pass) {
        TestResult = failValue;
        const QString detail = QStringLiteral("逐点卡控失败：%1").arg(failParts.join(QStringLiteral("; ")));
        showlog(QStringLiteral("气压卡控失败：%1，允许 %2").arg(detail, ask));
        markActiveTestCaseStepDone(false, testData, ask);
        return;
    }
    showlog(QStringLiteral("气压卡控通过：%1").arg(testData));
    markActiveTestCaseStepDone(true, testData, ask);
}

void QFreeWork::runPlcSwitchTestDoneResetM() {
    const PlcV3RunResult result = runPlcV3(PlcV3Command::SwitchDoneReset);
    applyPlcStepResult(result, PlcV3Command::SwitchDoneReset);
}

void QFreeWork::runPlcModbusConnectTest() {
    const PlcV3RunResult result = runPlcV3(PlcV3Command::ModbusConnectTest);
    applyPlcStepResult(result, PlcV3Command::ModbusConnectTest);
}

void QFreeWork::runPlcV3TouchKeyFull(int keyIndex0To6, bool finishStepRuntime) {
    const PlcV3RunResult result = runPlcV3(PlcV3Command::TouchKey, keyIndex0To6, finishStepRuntime);
    applyPlcStepResult(result, PlcV3Command::TouchKey, finishStepRuntime);
}

void QFreeWork::runPlcV3TouchSwitchFull(bool finishStepRuntime) {
    Q_UNUSED(finishStepRuntime);
    const PlcV3RunResult result = runPlcV3(PlcV3Command::TouchSwitch);
    applyPlcStepResult(result, PlcV3Command::TouchSwitch, finishStepRuntime);
}

bool QFreeWork::runFreeInstrumentBleCmwBurstForBrushProfile(QString* detail, int brushProfile) {
    const CmwGprfRunResult result =
        runCmwGprf(CmwGprfCommand::ParallelBurstForProfile, QString(), brushProfile);
    if (detail) {
        *detail = result.detail;
    }
    return result.ok || result.skipped;
}
void QFreeWork::startProductInstrumentResetAndWaitAck(QString stepNameIn, int timeoutMs) {
    QString stepName = stepNameIn.trimmed();
    if (stepName.isEmpty()) {
        stepName = testCaseStepActive_ ? activeTestCaseStepLabel_
                                       : QStringLiteral("产品串口仪器复位应答");
    }
    if (stepName.isEmpty())
        stepName = QStringLiteral("产品串口仪器复位应答");
    clearProductInstrumentWatch();
    if (!ensureProductSerialForInstrumentStep(stepName)) {
        return;
    }
    product->clearProductSerialRxAccum();
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("等待040E0405030C00");

    const auto sendFn = [this, stepName]() {
        QString err;
        if (!product->writeRaw(Qproduct::cmdReset(), &err))
            showlog(stepName + QStringLiteral("失败：写串口 ") + err);
    };
    // timeoutMs<0：WaitReply=false，只发不等应答
    if (timeoutMs < 0) {
        sendFn();
        stepRuntime_.done = true;
        stepRuntime_.testData = QStringLiteral("已发送（不等待回包）");
        showlog(QStringLiteral("已发送（不等待回包）"));
        return;
    }

    productInstConn_ = connect(product, &Qproduct::instrumentAckResetSeen, this, [this, stepName]() {
        if (!isCurrentInstrumentStep(stepName)) {
            return;
        }
        if (stepRuntime_.done) {
            return;
        }
        disconnect(productInstConn_);
        productInstConn_ = QMetaObject::Connection();
        finishCommandRetryWait(true, QString());
        stepRuntime_.done = true;
        stepRuntime_.pass = true;
        stepRuntime_.testData = QStringLiteral("040E0405030C00");
        showlog(stepName + QStringLiteral("通过"));
    });
    setCommandWaitSource(CommandWaitSource::ProductSerial);
    sendCommandWithRetry(sendFn, timeoutMs > 0 ? timeoutMs : 30000);
}

void QFreeWork::startProductInstrumentStartReceiveForCatalog(const QString& stepNameIn, int profile, int timeoutMs) {
    // 用例库走 ProductSerial 时传入空名；须落到当前步骤名，否则回包守卫 isCurrentInstrumentStep 失败导致卡死
    QString stepName = stepNameIn.trimmed();
    if (stepName.isEmpty()) {
        stepName = testCaseStepActive_ ? activeTestCaseStepLabel_
                                       : QStringLiteral("产品串口开始接收");
    }
    if (stepName.isEmpty())
        stepName = QStringLiteral("产品串口开始接收");
    clearProductInstrumentWatch();
    if (!ensureProductSerialForInstrumentStep(stepName)) {
        return;
    }
    product->clearProductSerialRxAccum();
    const QByteArray frame = brushInstrumentStartCmdForProfile(profile);
    lastBrushInstrumentProfile_ = profile;
    cmwFacade_.clearBurstDoneSinceStartRx();
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("Profile=%1 等待040E0405332000").arg(profile);

    const auto sendFn = [this, stepName, frame]() {
        QString err;
        if (!product->writeRaw(frame, &err))
            showlog(stepName + QStringLiteral("失败：写串口 ") + err);
    };
    if (timeoutMs < 0) {
        sendFn();
        stepRuntime_.done = true;
        stepRuntime_.testData = QStringLiteral("已发送（不等待回包）");
        showlog(QStringLiteral("已发送（不等待回包）"));
        return;
    }

    productInstConn_ = connect(product, &Qproduct::instrumentAckStartReceiveSeen, this, [this, stepName]() {
        if (!isCurrentInstrumentStep(stepName)) {
            return;
        }
        if (stepRuntime_.done) {
            return;
        }
        disconnect(productInstConn_);
        productInstConn_ = QMetaObject::Connection();
        finishCommandRetryWait(true, QString());
        stepRuntime_.done = true;
        stepRuntime_.pass = true;
        stepRuntime_.testData = QStringLiteral("040E0405332000");
        showlog(stepName + QStringLiteral("通过"));
    });
    setCommandWaitSource(CommandWaitSource::ProductSerial);
    sendCommandWithRetry(sendFn, timeoutMs > 0 ? timeoutMs : 30000);
}

void QFreeWork::startProductInstrumentStopReceiveAndPer(QString stepNameIn, int timeoutMs) {
    QString stepName = stepNameIn.trimmed();
    if (stepName.isEmpty()) {
        stepName = testCaseStepActive_ ? activeTestCaseStepLabel_
                                       : QStringLiteral("产品串口停止接收与PER");
    }
    if (stepName.isEmpty())
        stepName = QStringLiteral("产品串口停止接收与PER");
    clearProductInstrumentWatch();
    if (!ensureProductSerialForInstrumentStep(stepName)) {
        return;
    }
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    const int waitPacketMs = SETTINGS.value(QStringLiteral("BrushInstrument/PacketPhaseWaitMs"), 2000).toInt();
    const int stopAckTimeout = SETTINGS.value(QStringLiteral("BrushInstrument/StopAckTimeoutMs"), 5000).toInt();

    product->clearProductSerialRxAccum();

    const QString wfPath = SETTINGS.value(QStringLiteral("BlePer/CmwWaveformFile")).toString().trimmed();
    if (wfPath.isEmpty()) {
        showlog(stepName +
                QStringLiteral("：BlePer/CmwWaveformFile 为空，并联 CMW 使用仪侧 ARB（若未就绪请配置该键）"));
    } else {
        showlog(stepName + QStringLiteral("：BlePer/CmwWaveformFile=%1").arg(wfPath));
    }

    bool ranCmwBurst = false;
    const CmwGprfRunResult cmwResult =
        runCmwGprf(CmwGprfCommand::BrushBurstOnStopPer, stepName, lastBrushInstrumentProfile_, waitPacketMs, &ranCmwBurst);
    if (!cmwResult.ok) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = cmwResult.detail;
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：CMW GPRF——") + cmwResult.detail);
        return;
    }

    const int delayBeforeStopMs = ranCmwBurst ? 0 : waitPacketMs;
    const int uartWaitMs = timeoutMs > 0 ? timeoutMs : stopAckTimeout;
    showlog(stepName +
            QStringLiteral("：写停止接收前延时 %1 ms（BrushInstrument/PacketPhaseWaitMs=%2；已实际打并联 CMW 突发则不再追加积包延时）")
                .arg(delayBeforeStopMs)
                .arg(waitPacketMs));

    QTimer::singleShot(delayBeforeStopMs, this, [this, stepName, uartWaitMs]() {
        if (!isCurrentInstrumentStep(stepName)) {
            return;
        }
        productInstrumentStopWaitStepName_ = stepName;
        setCommandWaitSource(CommandWaitSource::ProductSerial);
        sendCommandWithRetry([this, stepName]() {
            QString err;
            if (!product || !product->writeRaw(Qproduct::cmdStopReceive(), &err))
                showlog(stepName + QStringLiteral("失败：写停止 ") + (err.isEmpty() ? QStringLiteral("写停止接收失败") : err));
        }, uartWaitMs);
    });
}
void QFreeWork::startPlcSwitchPlcAndWaitRightRotate() {
    const QString rightEn = QStringLiteral("ProductInfo/KeyIdRightRotate_checkBox");
    if (!SETTINGS.value(rightEn).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("右旋按键配置未启用");
        stepRuntime_.ask = QStringLiteral("请检查配置");
        TestResult = failValue;
        showlog(QStringLiteral("PLC+V3旋钮右旋失败：右旋配置未启用"));
        return;
    }

    // phase 4：PLC 旋钮整步后对编码器「右旋」dir=2 校验（qfctp ENCODER_STATUS_REPORT）。
    plcSwitchBlePhase_ = 4;
    currentKeyTestName_ = QStringLiteral("PLC+V3旋钮右旋");
    currentKeyExpectedKey_ = QStringLiteral("ProductInfo/KeyIdRightRotate");
    freeWorkKeyWaiting_ = true;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = QStringLiteral("PLC旋钮整步与等待右旋上报");
    stepRuntime_.ask = SETTINGS.value(currentKeyExpectedKey_).toString();
    plcKeyBlePlcOkSummary_.clear();

    closeKeyWaitPrompt();
    showlog(QStringLiteral("PLC+V3旋钮右旋：治具将自动完成旋钮动作，等待设备上报右旋"));

    runPlcV3TouchSwitchFull(false);

    if (!stepRuntime_.pass) {
        ++plcKeyBleWaitSeq_;
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        closeKeyWaitPrompt();
        plcKeyBlePlcOkSummary_.clear();
        return;
    }

    if (stepRuntime_.done) {
        ++plcKeyBleWaitSeq_;
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        return;
    }

    waitPlcBleKeyReportBlocking();
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

void QFreeWork::runHikvisionScannerReadStep() {
    const TestCaseDefinition& def = activeTestCase();
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>()) {
        map = resolveTestCaseSendParamTree(def.send.param).toMap();
    }

    QString ip = map.value(QStringLiteral("ip")).toString().trimmed();
    int port = map.value(QStringLiteral("port"), 2001).toInt();
    int timeout = map.value(QStringLiteral("timeout"), 1000).toInt();

    if (ip.isEmpty()) {
        markActiveTestCaseStepDone(false, QStringLiteral("未配置 IP 地址"), QStringLiteral("失败"));
        showlog(QStringLiteral("扫码枪错误：步骤参数未配置 ip"));
        return;
    }

    showlog(QStringLiteral("正在触发扫码枪（IP: %1, Port: %2, Timeout: %3ms）").arg(ip).arg(port).arg(timeout));

    QString barcode;
    QString error;
    HikvisionScannerTcp scanner;
    bool ok = false;
    if (scanner.connectDevice(ip, port, timeout, &error)) {
        ok = scanner.sendStartAndRead(&barcode, timeout, &error);
        scanner.disconnectDevice();
    }

    if (ok) {
        showlog(QStringLiteral("扫码成功: %1").arg(barcode));
        // 用户要求：扫码结果回填入原本扫码输入的sn填窗中
        if (ui && ui->macInput) {
            ui->macInput->setText(barcode);
            macAddress = barcode;
        }
        markActiveTestCaseStepDone(true, barcode, QStringLiteral("通过"));
    } else {
        showlog(QStringLiteral("扫码失败: %1").arg(error));
        markActiveTestCaseStepDone(false, error, QStringLiteral("失败"));
    }
}

void QFreeWork::runGenerateWholeMachineSnStep() {
    // 图1 智能机整机 SN（Air1/G100=35）：产品4+备用4+SKU3+MAC12+工厂2+日期3+预留1+版本1+产线1+流水4
    // 图2 智能机 PCBA SN（28）：产品4+MAC12+工厂2+类型2+日期3+流水5；本步只吃主板（类型建议 MB）
    const TestCaseDefinition& def = activeTestCase();
    QVariantMap map;
    if (def.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(def.send.param).toMap();

    const auto takeParam = [&map](const QString& key) -> QString {
        if (!map.contains(key))
            return {};
        return map.value(key).toString().trimmed();
    };

    QString pcbaRaw = takeParam(QStringLiteral("pcbaSn"));
    if (pcbaRaw.isEmpty())
        pcbaRaw = resolvedPcbaSnText();
    QString pcba = CommonUtils::extractSnFromUrlOrText(pcbaRaw);
    pcba.remove(QRegularExpression(QStringLiteral("\\s+")));
    pcba = pcba.toUpper();

    if (pcba.size() != 28) {
        const QString tip = QStringLiteral("PCBA SN须为智能机28位（当前%1位：%2）").arg(pcba.size()).arg(pcba);
        showlog(QStringLiteral("生成整机SN失败：%1；非智能机/URL前缀机型暂未适配").arg(tip));
        markActiveTestCaseStepDone(false, tip, QStringLiteral("失败"));
        return;
    }

    const QString product = pcba.mid(0, 4);
    const QString mac = pcba.mid(4, 12);
    const QString factory = pcba.mid(16, 2);
    const QString pcbaType = pcba.mid(18, 2);
    const QString pcbaDate = pcba.mid(20, 3);
    if (!QRegularExpression(QStringLiteral("^[0-9A-F]{12}$")).match(mac).hasMatch()) {
        showlog(QStringLiteral("生成整机SN失败：PCBA SN 中 MAC 段非法：%1").arg(mac));
        markActiveTestCaseStepDone(false, QStringLiteral("MAC非法:%1").arg(mac), QStringLiteral("失败"));
        return;
    }

    const QString expectType = takeParam(QStringLiteral("pcbaType"));
    if (!expectType.isEmpty() && pcbaType.compare(expectType, Qt::CaseInsensitive) != 0) {
        const QString tip = QStringLiteral("PCBA类型=%1，期望=%2").arg(pcbaType, expectType.toUpper());
        showlog(QStringLiteral("生成整机SN失败：%1（主板一般为 MB）").arg(tip));
        markActiveTestCaseStepDone(false, tip, QStringLiteral("失败"));
        return;
    }

    QString spare = takeParam(QStringLiteral("spare"));
    if (spare.isEmpty())
        spare = QStringLiteral("0000"); // 图1 备用位暂无追溯意义
    QString sku = takeParam(QStringLiteral("sku"));
    if (sku.isEmpty())
        sku = pack.sku.trimmed();
    QString date = takeParam(QStringLiteral("date"));
    if (date.isEmpty())
        date = pcbaDate; // 无附表一编码规则时默认沿用 PCBA 生产日；组装日请填 Param_date
    QString reserved = takeParam(QStringLiteral("reserved")); // 图1 F：如 Air1 左右单撑
    QString version = takeParam(QStringLiteral("version"));   // 图1 N：附表三
    QString line = takeParam(QStringLiteral("line"));         // 图1 P：附表二
    QString serial = takeParam(QStringLiteral("serial"));     // 图1 整机日流水 4 位（≠PCBA 5 位流水）
    QString factoryOut = takeParam(QStringLiteral("factory"));
    if (factoryOut.isEmpty())
        factoryOut = factory;

    QStringList missing;
    if (sku.size() != 3)
        missing << QStringLiteral("sku(3位，MES ROOTSKU/Param_sku，附表二)");
    if (spare.size() != 4)
        missing << QStringLiteral("spare(4位备用，默认0000)");
    if (date.size() != 3)
        missing << QStringLiteral("date(3位YMD，附表一；或沿用PCBA)");
    if (reserved.size() != 1)
        missing << QStringLiteral("reserved(1位F，如Air1:1左2右3单撑)");
    if (version.size() != 1)
        missing << QStringLiteral("version(1位N，附表三生产版本)");
    if (line.size() != 1)
        missing << QStringLiteral("line(1位P，附表二产线)");
    if (serial.size() != 4 || !QRegularExpression(QStringLiteral("^\\d{4}$")).match(serial).hasMatch())
        missing << QStringLiteral("serial(4位日流水，需号段/MES计数，不能用PCBA5位流水)");
    if (factoryOut.size() != 2)
        missing << QStringLiteral("factory(2位工厂代码)");

    if (!missing.isEmpty()) {
        const QString tip = QStringLiteral("缺少：%1").arg(missing.join(QStringLiteral("；")));
        showlog(QStringLiteral("生成整机SN失败：%1；PCBA解析 product=%2 mac=%3 factory=%4 type=%5 date=%6")
                    .arg(tip, product, mac, factory, pcbaType, pcbaDate));
        markActiveTestCaseStepDone(false, tip, QStringLiteral("失败"));
        return;
    }

    const QString whole =
        product + spare.toUpper() + sku.toUpper() + mac + factoryOut.toUpper() + date.toUpper() + reserved.toUpper()
        + version.toUpper() + line.toUpper() + serial;
    if (whole.size() != 35) {
        showlog(QStringLiteral("生成整机SN失败：拼出长度=%1（期望35） sn=%2").arg(whole.size()).arg(whole));
        markActiveTestCaseStepDone(false, QStringLiteral("长度异常:%1").arg(whole.size()), QStringLiteral("失败"));
        return;
    }

    setWholeMachineSn(whole);
    stepRuntime_.testData = whole;
    showlog(QStringLiteral("已生成整机SN并回填界面SN栏：%1（来源PCBA=%2 type=%3 sku=%4 reserved=%5 version=%6 line=%7 serial=%8）")
                .arg(whole, pcba, pcbaType, sku, reserved, version, line, serial));
    markActiveTestCaseStepDone(true, whole, QStringLiteral("通过"));
}

void QFreeWork::runBowlCapDiffCheckStep() {
    if (!hasBowlEmptyCapCache_ || !hasBowlFullCapCache_) {
        QStringList miss;
        if (!hasBowlEmptyCapCache_)
            miss << QStringLiteral("空碗读值(请先执行读取空碗电容)");
        if (!hasBowlFullCapCache_)
            miss << QStringLiteral("满碗读值(请先执行读取满碗电容)");
        const QString tip = miss.join(QStringLiteral("；"));
        showlog(QStringLiteral("空满碗电容差值失败：缺少%1").arg(tip));
        markActiveTestCaseStepDone(false, tip, QStringLiteral("失败"));
        return;
    }

    ProtocolRoot2BowlCapDiffData diff;
    diff.topDiff = static_cast<double>(bowlFullCapCh0_ - bowlEmptyCapCh0_); // F0 顶
    diff.midDiff = static_cast<double>(bowlFullCapCh2_ - bowlEmptyCapCh2_); // F2 中
    diff.botDiff = static_cast<double>(bowlFullCapCh1_ - bowlEmptyCapCh1_); // F1 底

    const QString testData = QStringLiteral("TOP=%1(%2-%3) MID=%4(%5-%6) BOT=%7(%8-%9)")
                                 .arg(diff.topDiff, 0, 'f', 2)
                                 .arg(bowlFullCapCh0_)
                                 .arg(bowlEmptyCapCh0_)
                                 .arg(diff.midDiff, 0, 'f', 2)
                                 .arg(bowlFullCapCh2_)
                                 .arg(bowlEmptyCapCh2_)
                                 .arg(diff.botDiff, 0, 'f', 2)
                                 .arg(bowlFullCapCh1_)
                                 .arg(bowlEmptyCapCh1_);
    showlog(QStringLiteral("空满碗电容差值（取前序步骤缓存）：%1").arg(testData));

    if (evaluateActiveTestCaseGate(QStringLiteral("ProtocolRoot2BowlCapDiffData"), QVariant::fromValue(diff)))
        return;

    markActiveTestCaseStepDone(true, testData, QStringLiteral("通过"));
}
