#ifndef PLC_STATION_SYNC_BARRIER_H
#define PLC_STATION_SYNC_BARRIER_H

#include <QElapsedTimer>
#include <QHash>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QVariantMap>

class QFreeWork;
struct TestCaseDefinition;

/**
 * 一拖多物理治具共享线圈的多工站同步屏障（Barrier）
 *
 * 核心机制：
 * 1. 等齐执行：所有在测工站未全员到达本步骤前，先到的工站安全挂起等待，严禁擅自动作治具；
 * 2. 集中下发：写线圈全员到齐后由主控工站统一下发一次动作（支持脉冲写 ON -> 延时 -> 写 OFF），避免串口指令冲突；
 * 3. 集中监听：监控读取全员到齐后集中轮询线圈（如 M0/M1），信号触发后一键广播放行所有工站；
 * 4. 兼容性：若仅单工位开测（在测数 <= 1），自动退化为立即执行，绝不无谓等待。
 */
class PlcStationSyncBarrier {
  public:
    static PlcStationSyncBarrier& instance();

    /** 一拖多同步写线圈（如 M10/M11 脉冲动作） */
    void executeSyncWriteCoil(QFreeWork* ctx, const TestCaseDefinition& def, const QString& defaultAddr = QString());

    /** 一拖多同步监控读取线圈（如 M0/M1 按键信号卡控） */
    void executeSyncReadCoil(QFreeWork* ctx, const TestCaseDefinition& def, const QString& defaultAddr = QString());

    /**
     * XWD 一拖多控制板文本指令：走主页「连接PLC治具串口」共享口（115200 8N1）。
     * 多工位到齐后由 Leader 统一下发一次；单工位直接执行。
     * rawTextEscaped：如 DOWN?\\r\\n / SET_ALL_UP\\r\\n / RELAY_1_ON\\r\\n
     * pollUntilMatch：true 时超时内轮询至期望（QueryDown→ALL_DOWN_OK）
     * waitReply：false 时只下发不等待回包（继电器 Set）
     */
    void executeSyncXwdTextCmd(QFreeWork* ctx, const TestCaseDefinition& def, const QString& rawTextEscaped,
                               bool pollUntilMatch, const QString& defaultExpect, bool waitReply = true);

    /**
     * XWD 继电器按工位号立即下发（不等待其它工站）。
     * 工位 N → RELAY_N_ON / RELAY_N_OFF；流程里配「继电器1接通/断开」即可，2 号工站自动走继电器2。
     */
    void executeImmediateXwdRelayByStation(QFreeWork* ctx, const TestCaseDefinition& def, bool turnOn);

    /** @deprecated 请用 executeSyncXwdTextCmd；保留兼容旧调用 */
    void executeSyncXwdDownUp(QFreeWork* ctx, const TestCaseDefinition& def, bool isQueryDown);

  private:
    PlcStationSyncBarrier() = default;
    ~PlcStationSyncBarrier() = default;
    Q_DISABLE_COPY(PlcStationSyncBarrier)

    struct SyncSession {
        QString key;
        QString stepName;
        int requiredCount = 0;
        QList<QFreeWork*> arrived;
        bool finished = false;
        bool success = false;
        QString resultData;
        QString errorString;
    };

    QMutex mutex_;
    QHash<QString, SyncSession*> activeSessions_;

    QList<QFreeWork*> collectConfiguredStations(QFreeWork* ctx);
    QString resolveTargetCoilAddress(const TestCaseDefinition& def, const QString& defaultAddr);
    void scheduleSessionTimeout(const QString& sessionKey, SyncSession* session, int timeoutMs);
    void completeSession(const QString& sessionKey, SyncSession* session, bool success,
                         const QString& resultData, const QString& logText);
};

#endif // PLC_STATION_SYNC_BARRIER_H
