#include "qfreework.h"

#include <algorithm>

#include <QElapsedTimer>
#include <QSettings>
#include <QTimer>
#include <QVariantMap>

#include "qlog.h"
#include "qproduct.h"
#include "qprotocol_types.h"
#include "test_case.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

struct DeferredSuctionLogFlush {
    DeferredSuctionLogFlush() { Qlog::setBufferedLogFlushDeferred(true); }
    ~DeferredSuctionLogFlush() { Qlog::setBufferedLogFlushDeferred(false); }
};

QVector<double> extractSuctionCyclePeaks(const QVector<double>& samples, double baseline, double dipStart) {
    QVector<double> cyclePeaks;
    enum class PeakPhase { AtBaseline, InCycle };
    PeakPhase phase = PeakPhase::AtBaseline;
    bool cycleMinInit = false;
    double cycleMinKpa = 0.0;
    for (double kpa : samples) {
        if (kpa >= baseline) {
            if (phase == PeakPhase::InCycle && cycleMinInit && cycleMinKpa <= dipStart)
                cyclePeaks.append(cycleMinKpa);
            phase = PeakPhase::AtBaseline;
            cycleMinInit = false;
            continue;
        }
        if (kpa < dipStart) {
            if (phase == PeakPhase::AtBaseline) {
                phase = PeakPhase::InCycle;
                cycleMinKpa = kpa;
                cycleMinInit = true;
            } else if (!cycleMinInit || kpa < cycleMinKpa) {
                cycleMinKpa = kpa;
                cycleMinInit = true;
            }
            continue;
        }
        if (phase == PeakPhase::InCycle && (!cycleMinInit || kpa < cycleMinKpa)) {
            cycleMinKpa = kpa;
            cycleMinInit = true;
        }
    }
    return cyclePeaks;
}

} // namespace

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
    dongleSuctionSampleTimer_.start();
    dongleSuctionSampleActive_ = true;
    DeferredSuctionLogFlush deferDongleLogFlush;
    QElapsedTimer sampleTimer;
    sampleTimer.start();
    // 取样点密度由 AT+SUCTION_DATA 决定；waitWorkIdle 只泵事件且不占满 CPU，不丢点。
    // 采样期间不刷界面日志、不实时画曲线，结束再落盘并绘制完整曲线。
    while (sampleTimer.elapsed() < durationMs) {
        if (!isTestContinue) {
            dongleSuctionSampleActive_ = false;
            if (restoreOff)
                setDongleSuctionReadEnabled(false);
            finalizeSuctionChartPlot();
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("测试中止");
            TestResult = failValue;
            showlog(QStringLiteral("采集双通道吸力已中止"));
            markActiveTestCaseStepDone(false, stepRuntime_.testData, QStringLiteral("失败"));
            return;
        }
        waitWorkIdle(qMin(intervalMs, durationMs - static_cast<int>(sampleTimer.elapsed())));
    }
    dongleSuctionSampleActive_ = false;
    if (restoreOff)
        setDongleSuctionReadEnabled(false);
    finalizeSuctionChartPlot();
    // 交 Qlog 暂存，测完随会话日志包导出 CSV（此时还不知 PASS/NG，不能定文件名）
    Qlog::setSuctionSamples(getIndex(), dongleSuctionSampleTimeSec_, dongleSuctionCh1Samples_,
                            dongleSuctionCh2Samples_, dongleSuctionCh3Samples_);

    if (dongleSuctionCh1Samples_.isEmpty() || dongleSuctionCh2Samples_.isEmpty()) {
        showlog(QStringLiteral("采集双通道吸力失败：采样窗口内未收到 CH1/CH2 AT+SUCTION_DATA"));
        markActiveTestCaseStepDone(false, QStringLiteral("无吸力采样点"), QStringLiteral("失败"));
        return;
    }

    // 与单通道相同：完整「吸→回基线」周期才记峰，拦截一直保持吸力不放气
    const double baseline = suctionPeakBaselineKpa_ + suctionOffsetKpa_;
    const double dipStart = suctionPeakDipStartKpa_ + suctionOffsetKpa_;
    const QVector<double> ch1Peaks = extractSuctionCyclePeaks(dongleSuctionCh1Samples_, baseline, dipStart);
    const QVector<double> ch2Peaks = extractSuctionCyclePeaks(dongleSuctionCh2Samples_, baseline, dipStart);
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
    showlog(QStringLiteral("采样完成：点 %1，CH1完整峰 %2 个 最强=%3（最弱=%4），CH2完整峰 %5 个 最强=%6（最弱=%7），通道峰差=%8")
                .arg(dongleSuctionCh1Samples_.size())
                .arg(ch1Peaks.size())
                .arg(peak.ch1PeakKpa, 0, 'f', 3)
                .arg(ch1Weak, 0, 'f', 3)
                .arg(ch2Peaks.size())
                .arg(peak.ch2PeakKpa, 0, 'f', 3)
                .arg(ch2Weak, 0, 'f', 3)
                .arg(peak.sideDiffKpa, 0, 'f', 3));

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
    dongleSuctionSampleTimer_.start();
    dongleSuctionSampleActive_ = true;
    DeferredSuctionLogFlush deferDongleLogFlush;
    QElapsedTimer sampleTimer;
    sampleTimer.start();
    // 同双通道：入库跟 AT 帧；intervalMs 只用于泵事件，不降采样
    while (sampleTimer.elapsed() < durationMs) {
        if (!isTestContinue) {
            dongleSuctionSampleActive_ = false;
            if (restoreOff)
                setDongleSuctionReadEnabled(false);
            finalizeSuctionChartPlot();
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("测试中止");
            TestResult = failValue;
            showlog(QStringLiteral("采集单通道吸力已中止"));
            markActiveTestCaseStepDone(false, stepRuntime_.testData, QStringLiteral("失败"));
            return;
        }
        waitWorkIdle(qMin(intervalMs, durationMs - static_cast<int>(sampleTimer.elapsed())));
    }
    dongleSuctionSampleActive_ = false;
    if (restoreOff)
        setDongleSuctionReadEnabled(false);
    finalizeSuctionChartPlot();
    // 交 Qlog 暂存，测完随会话日志包导出 CSV（此时还不知 PASS/NG，不能定文件名）
    Qlog::setSuctionSamples(getIndex(), dongleSuctionSampleTimeSec_, dongleSuctionCh1Samples_,
                            dongleSuctionCh2Samples_, dongleSuctionCh3Samples_);

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

    // 周期峰值：吸气回基线后取本周期最低点（真正的吸力峰）；峰值差=各峰中最大-最小，
    // 切勿用窗口绝对值最高-最低（会把基线≈0 算进去，diff≈40+）
    const double baseline = suctionPeakBaselineKpa_ + suctionOffsetKpa_;
    const double dipStart = suctionPeakDipStartKpa_ + suctionOffsetKpa_;
    const QVector<double> cyclePeaks = extractSuctionCyclePeaks(samples, baseline, dipStart);
    const int minPeaks = qMax(1, suctionMinPeakCount_);

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
    showlog(QStringLiteral("采样完成：点 %1，完整峰 %2 个（要求≥%3），最强=%4 最弱=%5 峰值差=%6")
                .arg(samples.size())
                .arg(cyclePeaks.size())
                .arg(minPeaks)
                .arg(peak.peakKpa, 0, 'f', 3)
                .arg(peak.highKpa, 0, 'f', 3)
                .arg(peak.peakDiffKpa, 0, 'f', 3));

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
