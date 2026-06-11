#ifndef CMW_GPRF_FACADE_H
#define CMW_GPRF_FACADE_H

#include "qcmw.h"
#include "qvisa.h"

#include <functional>

/** CMW100 GPRF 统一入口：会话复位 / PER 伴随 burst / 独立并联播放步。 */
enum class CmwGprfCommand {
    ResetSession,
    BrushBurstOnStopPer,
    ParallelBurstForProfile,
};

struct CmwGprfRunParams {
    Qvisa* visa = nullptr;
    QString scenarioLabel;
    int brushProfile = -1;
    int alignedPostTrigHoldMs = -1;
    std::function<void(const QString& line)> log;
    std::function<void(int ms)> wait;
    bool* outRanBurst = nullptr;
    bool* outAlignedWaitDoneByCmw = nullptr;
};

struct CmwGprfRunResult {
    bool ok = true;
    bool skipped = false;
    QString detail;
    bool markBurstDoneSinceStartRx = false;
};

class CmwGprfFacade {
  public:
    CmwGprfRunResult run(CmwGprfCommand command, const CmwGprfRunParams& params);
    void resetSession();
    // clang-format off
    bool burstDoneSinceStartRx() const { return burstDoneSinceStartRx_; }
    void clearBurstDoneSinceStartRx() { burstDoneSinceStartRx_ = false; }
    // clang-format on

  private:
    using LogFn = std::function<void(const QString&)>;
    using WaitFn = std::function<void(int)>;

    struct Config {
        bool enableFixedInit = true;
        int arbCycles = 1000;
        QString arbRepetition = QStringLiteral("SINGle");
        double txPowerDbm = -50.0;
        QString waveformFile;
        bool queryCurrentArbFile = true;
        bool checkErrorAfterInit = false;
        int arbCompleteCycles = 0;
        int arbPollIntervalMs = 100;
        int arbTimeoutMs = 10000;
        bool verboseArbPollLog = false;
        bool stopAfterScenario = true;
        bool burstStatOffFirst = true;
        bool useGuiRfConfig = true;
        int triggerWaitMs = 1000;
        bool waitArbScountOnly = false;
        bool burstPollArbScount = true;
        bool checkErrorAfterScenario = false;

        static Config fromSettings();
        int holdMsAfterTrigger(int postTrigHoldMsOverride) const;
    };

    struct BrushProfile {
        int profile = 0;
        int freqMhz = 2402;
        QString bandLabel = QStringLiteral("2402_BLE1M");

        static BrushProfile fromProfile(int profile);
        static bool isValid(int profile);
    };

    void bindSession(Qvisa* visa, const LogFn& log, const WaitFn& wait);
    QString cmwVisaAddress() const;
    bool ensureVisaConnected(Qvisa* visa, const LogFn& log, QString* detail);
    bool primeGprf(QString* errorMessage);
    bool waitArbComplete(const QString& scenarioLabel, const Config& cfg, const LogFn& log, const WaitFn& wait,
                         QString* errorMessage, int* outElapsedMs);
    bool runSingleBurstAtMhz(int freqMhz, const QString& scenarioLabel, const Config& cfg, const LogFn& log,
                             const WaitFn& wait, QString* errorMessage, int postTrigHoldMsOverride);
    CmwGprfRunResult runBurstAtProfile(const CmwGprfRunParams& params, const LogFn& log, const WaitFn& wait,
                                       const QString& burstLabel, bool logWaveformHint);

    bool cmwSet(CmwGprfCmd cmd, const QVariant& data = {});
    bool cmwGet(CmwGprfCmd cmd, const QVariant& param = {}, QString* response = nullptr);
    static bool parseArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent);

    Qcmw cmw_;
    LogFn log_;
    WaitFn wait_;
    bool sessionReady_ = false;
    bool gprfPrimed_ = false;
    bool burstDoneSinceStartRx_ = false;
};

#endif // CMW_GPRF_FACADE_H
