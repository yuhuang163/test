#ifndef TEST_CASE_SYNC_SERVICE_H
#define TEST_CASE_SYNC_SERVICE_H

#include <QString>
#include <QVector>

/** 上位机 test_case 与云端同步：按工站上传草稿 / 下载正式 bundle / 心跳领命令（含远控 Agent） */
class TestCaseSyncService {
  public:
    struct SyncResult {
        bool ok = false;
        QString message;
        QString bundleVersion;
        QString profileVersion;
        QString stationKey;
        int fileCount = 0;
    };

    /** 云端已发布、可下载的工站条目 */
    struct PublishedProfile {
        QString stationKey;
        QString displayName;
        QString profileVersion;
    };

    struct ProfileListResult {
        bool ok = false;
        QString message;
        QString bundleVersion;
        QVector<PublishedProfile> items;
    };

    static QString testCaseRoot();
    /** 打包当前选中工站用例上传云端草稿（不自动发布） */
    static SyncResult uploadToCloud(const QString& remark);
    /** 打包指定工站用例上传草稿（网页拉取回传用） */
    static SyncResult uploadStationProfile(const QString& stationKey, const QString& displayName,
                                           const QString& source, const QString& remark = QString());
    /** 查询云端已发布可下载的工站列表 */
    static ProfileListResult listPublishedProfiles();
    /** 从云端下载指定工站已发布用例（只替换该工站目录） */
    static SyncResult syncStationFromCloud(const QString& stationKey, const QString& displayName = QString());
    /** 从云端下载当前选中工站用例（只替换本工站目录） */
    static SyncResult syncFromCloud();
    /** 打包本地 test_case/steps 共享用例库上传云端草稿（不自动发布） */
    static SyncResult uploadStepsLibrary(const QString& remark);
    /** 从云端下载已发布的共享用例库（只替换本地 test_case/steps） */
    static SyncResult syncStepsLibraryFromCloud();
    static void tryStartupSyncAsync();

    /** 心跳 + 领取命令；收到 pull_test_profile 时自动回传 Profile */
    static void heartbeatAndPollCommands();
    /** 仅拉取并执行设备命令（远控启动需高频轮询） */
    static void pollDeviceCommands();
    /** 启动后台定时心跳（进程内单例） */
    static void startDeviceAgent();
};

#endif // TEST_CASE_SYNC_SERVICE_H
