#include "test_data_upload_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
#include "log_upload_service.h"
#include "test_case.h"
#include "test_record_store.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMutex>
#include <QSysInfo>
#include <QThread>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr int kFlushBatchSize = 20;
constexpr int kMaxRetryCount = 100;

QMutex& flushMutex() {
    static QMutex m;
    return m;
}

bool isTestDataUploadEnabled() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/Feature/TestDataUpload"), true).toBool();
}

QJsonObject itemToJson(const TestRecordStore::ParsedItem& item) {
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), item.name);
    if (!item.value.isEmpty()) {
        obj.insert(QStringLiteral("value"), item.value);
    }
    if (!item.maxValue.isEmpty()) {
        obj.insert(QStringLiteral("maxValue"), item.maxValue);
    }
    if (!item.minValue.isEmpty()) {
        obj.insert(QStringLiteral("minValue"), item.minValue);
    }
    if (!item.standardValue.isEmpty()) {
        obj.insert(QStringLiteral("standardValue"), item.standardValue);
    }
    if (!item.unit.isEmpty()) {
        obj.insert(QStringLiteral("unit"), item.unit);
    }
    if (!item.result.isEmpty()) {
        obj.insert(QStringLiteral("result"), item.result);
    }
    return obj;
}

/** 组上报 JSON；testedAt 在组包时固定，补传沿用同一时间 */
bool buildUploadBody(const MesPacketData& pack, QJsonObject* body, QString* message) {
    if (!body) {
        return false;
    }

    QString factoryName = pack.factory.trimmed();
    if (factoryName.isEmpty()) {
        factoryName = SETTINGS.value(QStringLiteral("Mes/FACTORY")).toString().trimmed();
    }
    if (factoryName.isEmpty()) {
        if (message) {
            *message = QStringLiteral("工厂名为空，跳过测试数据上报");
        }
        return false;
    }

    const QString station = FactoryCloudClient::stationKey();
    body->insert(QStringLiteral("factoryName"), factoryName);
    body->insert(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    body->insert(QStringLiteral("hostName"), QSysInfo::machineHostName());
    body->insert(QStringLiteral("station"), station);
    body->insert(QStringLiteral("stationKey"), station);
    body->insert(QStringLiteral("sn"), pack.sn.trimmed());
    body->insert(QStringLiteral("mac"), pack.mac.trimmed());
    body->insert(QStringLiteral("testResult"), pack.result.trimmed());
    body->insert(QStringLiteral("machineNo"), pack.machineNo.trimmed());
    body->insert(QStringLiteral("product"), pack.product.trimmed());
    body->insert(QStringLiteral("lotName"), pack.lotName.trimmed());
    body->insert(QStringLiteral("userNo"), pack.userNo.trimmed());
    body->insert(QStringLiteral("clientVersion"), FactoryCloudClient::appVersion());
    body->insert(QStringLiteral("testedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QJsonArray items;
    const QVector<TestRecordStore::ParsedItem> parsed = TestRecordStore::parseItemValue(pack);
    for (TestRecordStore::ParsedItem item : parsed) {
        // MES 分项仍用 MesTag；云端上报改为中文 DisplayName
        item.name = TestCaseStore::cloudDisplayNameForItemKey(item.name);
        items.append(itemToJson(item));
    }
    body->insert(QStringLiteral("items"), items);
    return true;
}

bool looksLikeNetworkError(const QString& message) {
    const QString m = message.toLower();
    return m.contains(QStringLiteral("网络")) || m.contains(QStringLiteral("network")) ||
        m.contains(QStringLiteral("timeout")) || m.contains(QStringLiteral("timed out")) ||
        m.contains(QStringLiteral("connection")) || m.contains(QStringLiteral("连接")) ||
        m.contains(QStringLiteral("host not found")) || m.contains(QStringLiteral("ssl"));
}

/** 业务拒绝（如参数错误）不入队；网络/服务端临时失败可补传 */
bool shouldEnqueueForRetry(const FactoryCloudClient::ApiResult& api) {
    if (api.ok) {
        return false;
    }
    // 未拿到业务包络：多为断网/超时
    if (api.code < 0) {
        return true;
    }
    // 5xx 或明确网络文案
    if (api.code >= 500 || looksLikeNetworkError(api.message)) {
        return true;
    }
    // 登录失效等：入队，等心跳重登后再补传
    if (api.code == 401 || api.code == 403) {
        return true;
    }
    return false;
}

int uploadBodyOnce(const QJsonObject& body, QString* message) {
    if (!AuthService::isLoggedIn()) {
        (void)AuthService::loginWithSavedCredentials();
    }

    qDebug() << body;
    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::post(QStringLiteral("/test-records"), body);
    if (!api.ok) {
        if (message) {
            *message = api.message.isEmpty() ? QStringLiteral("测试数据上报失败") : api.message;
        }
        return 0;
    }

    const int recordId = api.data.value(QStringLiteral("recordId")).toInt(0);
    if (message) {
        *message = recordId > 0 ? QStringLiteral("测试数据上报成功（recordId=%1）").arg(recordId)
                                : QStringLiteral("测试数据上报成功");
    }
    return recordId;
}

/**
 * 先入队再认领上报：成功删除队列；可重试失败回 pending；不可重试则 failed。
 * 返回 recordId（成功）或 0。
 */
int enqueueThenUpload(const QJsonObject& body, QString* message) {
    const qint64 queueId = TestRecordStore::instance().enqueueCloudUpload(body);
    if (queueId <= 0) {
        // 入队失败仍尝试直传，避免本地库异常时完全丢上报
        return uploadBodyOnce(body, message);
    }

    if (!TestRecordStore::instance().claimCloudUpload(queueId)) {
        // 已被补传线程认领，交给 flush
        if (message) {
            *message = QStringLiteral("测试数据已入队待补传 queue_id=%1").arg(queueId);
        }
        return 0;
    }

    if (!AuthService::isLoggedIn()) {
        (void)AuthService::loginWithSavedCredentials();
    }
    qDebug() << body;
    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::post(QStringLiteral("/test-records"), body);
    if (api.ok) {
        const int recordId = api.data.value(QStringLiteral("recordId")).toInt(0);
        TestRecordStore::instance().markCloudUploadDone(queueId, recordId);
        if (message) {
            *message = recordId > 0 ? QStringLiteral("测试数据上报成功（recordId=%1）").arg(recordId)
                                    : QStringLiteral("测试数据上报成功");
        }
        return recordId;
    }

    const QString err = api.message.isEmpty() ? QStringLiteral("测试数据上报失败") : api.message;
    if (shouldEnqueueForRetry(api)) {
        TestRecordStore::instance().markCloudUploadRetry(queueId, err);
        if (message) {
            *message = QStringLiteral("%1（已入队待补传 queue_id=%2）").arg(err).arg(queueId);
        }
    } else {
        TestRecordStore::instance().markCloudUploadFailed(queueId, err);
        if (message) {
            *message = err;
        }
    }
    return 0;
}

} // namespace

int TestDataUploadService::uploadBody(const QJsonObject& body, QString* message) {
    if (AuthService::isOfflineSession()) {
        if (message) {
            *message = QStringLiteral("离线测试模式，跳过测试数据上报");
        }
        return 0;
    }
    if (!isTestDataUploadEnabled()) {
        if (message) {
            *message = QStringLiteral("测试数据上报已关闭");
        }
        return 0;
    }
    return uploadBodyOnce(body, message);
}

int TestDataUploadService::uploadFromPack(const MesPacketData& pack, QString* message) {
    if (AuthService::isOfflineSession()) {
        if (message) {
            *message = QStringLiteral("离线测试模式，跳过测试数据上报");
        }
        return 0;
    }
    if (!isTestDataUploadEnabled()) {
        if (message) {
            *message = QStringLiteral("测试数据上报已关闭");
        }
        return 0;
    }

    QJsonObject body;
    if (!buildUploadBody(pack, &body, message)) {
        return 0;
    }
    return enqueueThenUpload(body, message);
}

void TestDataUploadService::tryUploadAsync(const MesPacketData& pack) {
    const MesPacketData copy = pack;
    QtConcurrent::run([copy]() {
        QString message;
        const int recordId = uploadFromPack(copy, &message);
        Q_UNUSED(recordId);
        if (!message.isEmpty()) {
            qDebug() << QStringLiteral("[TestDataUpload]") << message;
        }
    });
}

void TestDataUploadService::tryUploadTestAndLogAsync(const MesPacketData& pack, int slot) {
    const MesPacketData copy = pack;
    QtConcurrent::run([copy, slot]() {
        QString message;
        const int recordId = uploadFromPack(copy, &message);
        if (!message.isEmpty()) {
            qDebug() << QStringLiteral("[TestDataUpload]") << message;
        }
        // 上报失败时 recordId=0，日志仍尝试上传（与原逻辑一致）
        LogUploadService::uploadSessionFromPack(copy, slot, recordId, &message);
        if (!message.isEmpty()) {
            qDebug() << QStringLiteral("[LogUpload]") << message;
        }
    });
}

void TestDataUploadService::flushPendingUploads() {
    if (AuthService::isOfflineSession() || !isTestDataUploadEnabled()) {
        return;
    }

    // SQLite 连接建在主线程；心跳在 QtConcurrent 工作线程，必须切回主线程再访问库
    if (QCoreApplication::instance()
        && QThread::currentThread() != QCoreApplication::instance()->thread()) {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(), []() { TestDataUploadService::flushPendingUploads(); },
            Qt::QueuedConnection);
        return;
    }

    // 避免心跳并发重复补传同一批（Qt5 无 adopt_lock，手动 tryLock/unlock）
    if (!flushMutex().tryLock()) {
        return;
    }

    int okCount = 0;
    int failCount = 0;
    do {
        // 进程异常退出时，uploading 可能卡住，先回收再扫
        TestRecordStore::instance().recoverStaleCloudUploads(120);

        const int pending = TestRecordStore::instance().pendingCloudUploadCount();
        if (pending <= 0) {
            break;
        }

        if (!AuthService::isLoggedIn()) {
            const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
            if (!login.ok) {
                qDebug() << QStringLiteral("[TestDataUpload] 补传前登录失败：") << login.message;
                break;
            }
        }

        const QVector<TestRecordStore::PendingCloudUpload> batch =
            TestRecordStore::instance().listPendingCloudUploads(kFlushBatchSize);
        if (batch.isEmpty()) {
            break;
        }

        qDebug() << QStringLiteral("[TestDataUpload] 开始补传，待传=%1，本批=%2").arg(pending).arg(batch.size());

        for (const TestRecordStore::PendingCloudUpload& item : batch) {
            if (item.id <= 0 || item.payload.isEmpty()) {
                continue;
            }
            if (item.retryCount >= kMaxRetryCount) {
                TestRecordStore::instance().markCloudUploadFailed(
                    item.id, QStringLiteral("超过最大重试次数 %1").arg(kMaxRetryCount));
                ++failCount;
                continue;
            }
            if (!TestRecordStore::instance().claimCloudUpload(item.id)) {
                continue;
            }

            const FactoryCloudClient::ApiResult api =
                FactoryCloudClient::post(QStringLiteral("/test-records"), item.payload);
            if (api.ok) {
                const int recordId = api.data.value(QStringLiteral("recordId")).toInt(0);
                TestRecordStore::instance().markCloudUploadDone(item.id, recordId);
                ++okCount;
                qDebug() << QStringLiteral("[TestDataUpload] 补传成功 queue_id=%1 recordId=%2")
                                .arg(item.id)
                                .arg(recordId);
                continue;
            }

            const QString err = api.message.isEmpty() ? QStringLiteral("补传失败") : api.message;
            if (shouldEnqueueForRetry(api)) {
                TestRecordStore::instance().markCloudUploadRetry(item.id, err);
            } else {
                TestRecordStore::instance().markCloudUploadFailed(item.id, err);
            }
            ++failCount;
            qDebug() << QStringLiteral("[TestDataUpload] 补传失败 queue_id=%1：%2").arg(item.id).arg(err);
            // 网络仍不通时尽快结束本批，等下次心跳
            if (api.code < 0 || looksLikeNetworkError(err)) {
                break;
            }
        }

        if (okCount > 0 || failCount > 0) {
            qDebug() << QStringLiteral("[TestDataUpload] 本批补传结束：成功=%1 失败=%2 剩余=%3")
                            .arg(okCount)
                            .arg(failCount)
                            .arg(TestRecordStore::instance().pendingCloudUploadCount());
        }
    } while (false);

    flushMutex().unlock();
}
