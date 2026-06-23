#ifndef THRESHOLD_SYNC_SERVICE_H
#define THRESHOLD_SYNC_SERVICE_H

#include <QString>

/** 从云端拉取阈值并写入 SETTINGS */
class ThresholdSyncService {
  public:
    struct SyncResult {
        bool ok = false;
        QString message;
        QString version;
        int appliedCount = 0;
    };

    static SyncResult syncFromCloud(const QString& stationKey = QString(),
                                    const QString& productModel = QString());
};

#endif // THRESHOLD_SYNC_SERVICE_H
