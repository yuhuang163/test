#include "test_data_upload_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
#include "log_upload_service.h"
#include "test_case.h"
#include "test_record_store.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QSysInfo>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

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

} // namespace

int TestDataUploadService::uploadFromPack(const MesPacketData& pack, QString* message) {
    QString factoryName = pack.factory.trimmed();
    if (factoryName.isEmpty()) {
        factoryName = SETTINGS.value(QStringLiteral("Mes/FACTORY")).toString().trimmed();
    }
    if (factoryName.isEmpty()) {
        if (message) {
            *message = QStringLiteral("工厂名为空，跳过测试数据上报");
        }
        return 0;
    }
    if (AuthService::isOfflineSession()) {
        if (message) {
            *message = QStringLiteral("离线测试模式，跳过测试数据上报");
        }
        return 0;
    }
    if (!AuthService::isLoggedIn()) {
        (void)AuthService::loginWithSavedCredentials();
    }

    // 工站名称与自由工站 / 上报 stationKey 一致（SelectedStationName）
    const QString station = FactoryCloudClient::stationKey();
    QJsonObject body;
    body.insert(QStringLiteral("factoryName"), factoryName);
    body.insert(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    body.insert(QStringLiteral("hostName"), QSysInfo::machineHostName());
    body.insert(QStringLiteral("station"), station);
    body.insert(QStringLiteral("stationKey"), station);
    body.insert(QStringLiteral("sn"), pack.sn.trimmed());
    body.insert(QStringLiteral("mac"), pack.mac.trimmed());
    body.insert(QStringLiteral("testResult"), pack.result.trimmed());
    body.insert(QStringLiteral("machineNo"), pack.machineNo.trimmed());
    body.insert(QStringLiteral("product"), pack.product.trimmed());
    body.insert(QStringLiteral("lotName"), pack.lotName.trimmed());
    body.insert(QStringLiteral("userNo"), pack.userNo.trimmed());
    body.insert(QStringLiteral("clientVersion"), FactoryCloudClient::appVersion());
    body.insert(QStringLiteral("testedAt"),QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QJsonArray items;
    const QVector<TestRecordStore::ParsedItem> parsed = TestRecordStore::parseItemValue(pack);
    for (TestRecordStore::ParsedItem item : parsed) {
        // MES 分项仍用 MesTag；云端上报改为中文 DisplayName（见 TestCaseStore::cloudDisplayNameForItemKey）
        item.name = TestCaseStore::cloudDisplayNameForItemKey(item.name);
        items.append(itemToJson(item));
    }
    body.insert(QStringLiteral("items"), items);
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
        LogUploadService::uploadSessionFromPack(copy, slot, recordId, &message);
        if (!message.isEmpty()) {
            qDebug() << QStringLiteral("[LogUpload]") << message;
        }
    });
}
