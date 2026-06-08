#ifndef CMW_GPRF_FACADE_H
#define CMW_GPRF_FACADE_H

#include "cmw_gprf.h"
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
    bool burstDoneSinceStartRx() const {
        return burstDoneSinceStartRx_;
    }
    void clearBurstDoneSinceStartRx() {
        burstDoneSinceStartRx_ = false;
    }

  private:
    void ensureProtocol(Qvisa* visa, const std::function<void(const QString&)>& log,
                        const std::function<void(int)>& wait);

    CmwGprfProtocol gprf_;
    bool protocolReady_ = false;
    bool burstDoneSinceStartRx_ = false;
};

#endif // CMW_GPRF_FACADE_H
