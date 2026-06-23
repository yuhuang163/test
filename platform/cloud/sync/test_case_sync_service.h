#ifndef TEST_CASE_SYNC_SERVICE_H
#define TEST_CASE_SYNC_SERVICE_H

#include <QString>

/** 从云端拉取 test_case bundle 并解压到 bin/test_case */
class TestCaseSyncService {
  public:
    struct SyncResult {
        bool ok = false;
        QString message;
        QString bundleVersion;
        int fileCount = 0;
    };

    static QString testCaseRoot();
    static SyncResult syncFromCloud();
};

#endif // TEST_CASE_SYNC_SERVICE_H
