#ifndef TEST_RECORD_STORE_H
#define TEST_RECORD_STORE_H

#include "my_set/my_typedef.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

/**
 * 过站时本地 SQLite：所有log/test_pass_data.db
 * - mes_test_pass：每次过站流水
 * - mes_<工站名>：按 SYSTEM/station 分表，sn 唯一，一行汇总 itemvalue 分项列
 * - cloud_upload_queue：断网时待补传的云端测试数据 JSON
 */
class TestRecordStore {
  public:
    struct ParsedItem {
        QString name;
        QString value;
        QString maxValue;
        QString minValue;
        QString standardValue;
        QString unit;
        QString result;
    };

    struct PendingCloudUpload {
        qint64 id = 0;
        QJsonObject payload;
        int retryCount = 0;
    };

    static TestRecordStore& instance();

    bool saveOnTestPass(const MesPacketData& pack);

    static QVector<ParsedItem> parseItemValue(const MesPacketData& pack);

    /** 写入待补传队列；成功返回队列 id，失败返回 0 */
    qint64 enqueueCloudUpload(const QJsonObject& payload);

    /** 将 pending 置为 uploading，供即时上报/补传认领，避免并发重复传 */
    bool claimCloudUpload(qint64 id);

    /** 长时间卡在 uploading 的记录恢复为 pending（进程中断等） */
    void recoverStaleCloudUploads(int olderThanSeconds = 120);

    /** 取出待补传记录（按 id 升序），最多 limit 条 */
    QVector<PendingCloudUpload> listPendingCloudUploads(int limit = 20);

    bool markCloudUploadDone(qint64 id, int cloudRecordId);
    bool markCloudUploadRetry(qint64 id, const QString& error);
    /** 超过重试上限时标记永久失败，避免无限重试 */
    bool markCloudUploadFailed(qint64 id, const QString& error);

    int pendingCloudUploadCount();

  private:
    TestRecordStore();
    TestRecordStore(const TestRecordStore&) = delete;
    TestRecordStore& operator=(const TestRecordStore&) = delete;

    bool ensureOpen();
    bool ensurePassTable();
    bool ensureUploadQueueTable();
    bool upsertStationRow(const QString& workStation, const MesPacketData& pack, const QVector<ParsedItem>& items);
    QString databasePath() const;

    bool opened_ = false;
};

#endif // TEST_RECORD_STORE_H
