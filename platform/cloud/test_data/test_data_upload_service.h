#ifndef TEST_DATA_UPLOAD_SERVICE_H
#define TEST_DATA_UPLOAD_SERVICE_H

#include "my_set/my_typedef.h"

#include <QJsonObject>
#include <QString>

/** 过站测试数据 JSON 上报工厂云；断网失败入本地队列，联网后补传 */
class TestDataUploadService {
  public:
    /** 同步上报；成功返回 recordId，失败返回 0 */
    static int uploadFromPack(const MesPacketData& pack, QString* message = nullptr);

    /** 用已组好的 JSON body 上报；成功返回 recordId */
    static int uploadBody(const QJsonObject& body, QString* message = nullptr);

    /** 测试结束后异步上报，不阻塞主流程 */
    static void tryUploadAsync(const MesPacketData& pack);

    /** 链式：先测试数据再会话日志（失败入队，心跳时补传） */
    static void tryUploadTestAndLogAsync(const MesPacketData& pack, int slot);

    /** 扫描本地队列并补传；心跳成功后调用 */
    static void flushPendingUploads();
};

#endif // TEST_DATA_UPLOAD_SERVICE_H
