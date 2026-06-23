#ifndef TEST_DATA_UPLOAD_SERVICE_H
#define TEST_DATA_UPLOAD_SERVICE_H

#include "my_set/my_typedef.h"

#include <QString>

/** 过站测试数据 JSON 上报工厂云 */
class TestDataUploadService {
  public:
    static bool isUploadEnabled();

    /** 同步上报；失败时 message 有说明 */
    static bool uploadFromPack(const MesPacketData& pack, QString* message = nullptr);

    /** 测试结束后异步上报，不阻塞主流程 */
    static void tryUploadAsync(const MesPacketData& pack);
};

#endif // TEST_DATA_UPLOAD_SERVICE_H
