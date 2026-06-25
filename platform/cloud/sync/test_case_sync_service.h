#ifndef TEST_CASE_SYNC_SERVICE_H
#define TEST_CASE_SYNC_SERVICE_H

#include <QString>

/** 上位机 test_case 与云端同步：上传（打包 zip）/ 下载（拉取 bundle） */
class TestCaseSyncService {
  public:
    struct SyncResult {
        bool ok = false;
        QString message;
        QString bundleVersion;
        int fileCount = 0;
    };

    static QString testCaseRoot();
    /** 打包本地 test_case 上传云端并发布 bundle */
    static SyncResult uploadToCloud();
    /** 从云端下载 bundle 覆盖本地 test_case */
    static SyncResult syncFromCloud();
    static bool isEnabled();
    static void tryStartupSyncAsync();
};

#endif // TEST_CASE_SYNC_SERVICE_H
