#ifndef TEST_CASE_POLICY_SERVICE_H
#define TEST_CASE_POLICY_SERVICE_H

#include <QString>
#include <QVariantMap>

/**
 * 用例审核策略（按工站）：试产/量产缓存、本地 dirty/pending、开测卡控。
 * 量产工站：本地改动或与云端正式内容指纹不一致则禁止开测；试产工站：允许但告警。
 * 上传并网页合入后：若本地内容已与云端一致（含仅差 ProfileVersion 等元数据），开测时自动对齐，无需再下载。
 */
class TestCasePolicyService {
  public:
    enum class Phase {
        Trial = 0,
        Mass = 1,
    };

    struct GateResult {
        bool allowed = true;
        /** 试产告警：允许继续，调用方仅本地打印即可 */
        bool warningOnly = false;
        Phase phase = Phase::Trial;
        QString message;
    };

    /** UI 保存某工站流程后：标 dirty */
    static void markDirtyAfterUiEdit(const QString& stationKey = QString());
    /** 上传草稿成功：待审 */
    static void markPendingAfterUpload(const QString& stationKey);
    /** 下载正式工站用例成功：对齐指纹，清 dirty/pending */
    static void markSyncedClean(const QString& stationKey, const QString& contentFingerprint,
                                const QString& bundleVersion = QString());
    static void clearCache(const QString& stationKey = QString());

    /** 本地工站 Profile 内容指纹（与云端 contentFingerprint 同算法；不计 profile.ini [Policy]） */
    static QString localProfileFingerprint(const QString& stationKey);

    /**
     * 开测前门禁（按当前工站）。优先用本地状态；必要时拉一次 GET /test-cases/policy。
     * 策略缓存在各工站 profile.ini 的 [Policy] 段。
     */
    static GateResult evaluateStartGate(const QString& stationKey, const QString& displayName,
                                        QString* exemptionCode = nullptr);

    /** 上传打包前暂存并清空 [Policy]，避免本机策略进云端草稿；上传后 reattach */
    static QVariantMap detachPolicyForUpload(const QString& stationKey);
    static void reattachPolicyAfterUpload(const QString& stationKey, const QVariantMap& saved);
};

#endif // TEST_CASE_POLICY_SERVICE_H
