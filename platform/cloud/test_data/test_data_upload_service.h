#ifndef TEST_DATA_UPLOAD_SERVICE_H
#define TEST_DATA_UPLOAD_SERVICE_H

#include "my_set/my_typedef.h"

#include <QString>

/** 过站测试数据 JSON 上报工厂云 */
class TestDataUploadService {
  public:
    /** 同步上报；成功返回 recordId，失败返回 0 */
    static int uploadFromPack(const MesPacketData& pack, QString* message = nullptr);

    /** 测试结束后异步上报，不阻塞主流程 */
    static void tryUploadAsync(const MesPacketData& pack);

    /** 链式：先测试数据再会话日志 */
    static void tryUploadTestAndLogAsync(const MesPacketData& pack, int slot);
};

#endif // TEST_DATA_UPLOAD_SERVICE_H
