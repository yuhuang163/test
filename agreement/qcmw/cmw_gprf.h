#ifndef CMW_GPRF_H
#define CMW_GPRF_H

#include <QString>
#include <functional>

/** CMW100 GPRF 射频发包协议（VISA SCPI），与工站解耦，经回调下发读写。 */
class CmwGprfProtocol {
public:
    using WriteFn = std::function<bool(const QString& cmd)>;
    using QueryFn = std::function<bool(const QString& cmd, QString* response)>;
    using LogFn = std::function<void(const QString& line)>;
    using WaitFn = std::function<void(int ms)>;

    CmwGprfProtocol();
    CmwGprfProtocol(WriteFn write, QueryFn query, LogFn log = {}, WaitFn wait = {});

    static QString arbFileWriteCommand(const QString& rawPath);
    static int brushProfileToMHz(int profile);
    static QString brushBandLabel(int profile);
    static bool parseArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent);

    void resetPrimed() { gprfPrimed_ = false; }
    bool isPrimed() const { return gprfPrimed_; }

    bool primeGprf(QString* errorMessage = nullptr);
    bool waitArbComplete(const QString& scenarioLabel, QString* errorMessage = nullptr, int* outElapsedMs = nullptr);
    bool runSingleBurstAtMhz(int freqMhz, const QString& scenarioLabel, QString* errorMessage = nullptr,
                             int postTrigHoldMsOverride = -1);

private:
    void logLine(const QString& line) const;

    WriteFn write_;
    QueryFn query_;
    LogFn log_;
    WaitFn wait_;
    bool gprfPrimed_ = false;
};

#endif  // CMW_GPRF_H
