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

  private:
    PlcStationSyncBarrier() = default;
    ~PlcStationSyncBarrier() = default;
    Q_DISABLE_COPY(PlcStationSyncBarrier)

    struct SyncSession {
        QString key;
        int requiredCount = 0;
        int remainingExitCount = 0;
        QList<QFreeWork*> arrived;
        bool finished = false;
        bool success = false;
        QString resultData;
        QString errorString;
    };

    QMutex mutex_;
    QHash<QString, SyncSession*> activeSessions_;

    QList<QFreeWork*> collectActiveTestingStations(QFreeWork* ctx);
    QString resolveTargetCoilAddress(const TestCaseDefinition& def, const QString& defaultAddr);
    QVariantMap resolvePlcExecutionParams(QFreeWork* ctx, const TestCaseDefinition& def, const QString& addr,
                                          const QVariantMap& extra = QVariantMap());
    void releaseSessionRef(const QString& sessionKey, SyncSession* session);
};

#endif // PLC_STATION_SYNC_BARRIER_H
