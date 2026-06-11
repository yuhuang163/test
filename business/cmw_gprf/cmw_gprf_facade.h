#ifndef CMW_GPRF_FACADE_H
#define CMW_GPRF_FACADE_H

#include <QString>

#include "rs_cmw100_scpi_types.h"

#include <QVariant>
#include <functional>

class QScpiManager;

/** CMW100 GPRF 统一入口：会话复位 / PER 伴随 burst / 独立并联播放步。 */
enum class CmwGprfCommand {
    ResetSession,
    BrushBurstOnStopPer,
    ParallelBurstForProfile,
};

struct CmwGprfRunParams {
    QScpiManager* scpi = nullptr;
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

/**
 * CMW100 GPRF 工站门面（business/，与 PlcV3Facade 同级）。
 * 协议 Cmd 在 agreement/scpi/device/rs_cmw100_scpi；本类编排 PER/burst 场景。
 */
class CmwGprfFacade {
  public:
    CmwGprfRunResult run(CmwGprfCommand command, const CmwGprfRunParams& params);
    void resetSession();
    bool burstDoneSinceStartRx() const { return burstDoneSinceStartRx_; }
    void clearBurstDoneSinceStartRx() { burstDoneSinceStartRx_ = false; }

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

    void bindSession(QScpiManager* scpi, const LogFn& log, const WaitFn& wait);
    QString cmwVisaAddress() const;
    bool ensureVisaConnected(QScpiManager* scpi, const LogFn& log, QString* detail);
    bool primeGprf(QString* errorMessage);
    bool waitArbComplete(const QString& scenarioLabel, const Config& cfg, const LogFn& log, const WaitFn& wait,
                         QString* errorMessage, int* outElapsedMs);
    bool runSingleBurstAtMhz(int freqMhz, const QString& scenarioLabel, const Config& cfg, const LogFn& log,
                             const WaitFn& wait, QString* errorMessage, int postTrigHoldMsOverride);
    CmwGprfRunResult runBurstAtProfile(const CmwGprfRunParams& params, const LogFn& log, const WaitFn& wait,
                                       const QString& burstLabel, bool logWaveformHint);

    bool cmwSet(CmwScpiCmd cmd, const QVariant& data = {});
    bool cmwGet(CmwScpiCmd cmd, const QVariant& param = {}, QString* response = nullptr);
    static bool parseArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent);

    bool sessionReady_ = false;
    bool gprfPrimed_ = false;
    bool burstDoneSinceStartRx_ = false;
    QScpiManager* scpi_ = nullptr;
    LogFn log_;
    WaitFn wait_;
};

#endif // CMW_GPRF_FACADE_H
