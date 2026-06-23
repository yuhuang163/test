#include "test_data_upload_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
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

bool TestDataUploadService::isUploadEnabled() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/Feature/TestDataUpload"), true).toBool();
}

bool TestDataUploadService::uploadFromPack(const MesPacketData& pack, QString* message) {
    if (!isUploadEnabled()) {
        if (message) {
            *message = QStringLiteral("测试数据上传已关闭");
        }
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
    if (!AuthService::isLoggedIn()) {
        (void)AuthService::loginWithSavedCredentials();
    }

    QString station = SETTINGS.value(QStringLiteral("SYSTEM/station")).toString().trimmed();
    if (station.isEmpty()) {
        station = FactoryCloudClient::stationKey();
    }
    QJsonObject body;
    body.insert(QStringLiteral("factoryName"), factoryName);
    body.insert(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    body.insert(QStringLiteral("hostName"), QSysInfo::machineHostName());
    body.insert(QStringLiteral("station"), station);
    body.insert(QStringLiteral("stationKey"), FactoryCloudClient::stationKey());
    body.insert(QStringLiteral("sn"), pack.sn.trimmed());
    body.insert(QStringLiteral("testResult"), pack.result.trimmed());
    body.insert(QStringLiteral("machineNo"), pack.machineNo.trimmed());
    body.insert(QStringLiteral("product"), pack.product.trimmed());
    body.insert(QStringLiteral("lotName"), pack.lotName.trimmed());
    body.insert(QStringLiteral("userNo"), pack.userNo.trimmed());
    body.insert(QStringLiteral("clientVersion"), FactoryCloudClient::appVersion());
    body.insert(QStringLiteral("testedAt"),
                QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));

    QJsonArray items;
    const QVector<TestRecordStore::ParsedItem> parsed = TestRecordStore::parseItemValue(pack);
    for (const TestRecordStore::ParsedItem& item : parsed) {
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
        return false;
    }

    const int recordId = api.data.value(QStringLiteral("recordId")).toInt(0);
    if (message) {
        *message = recordId > 0 ? QStringLiteral("测试数据上报成功（recordId=%1）").arg(recordId)
                                : QStringLiteral("测试数据上报成功");
    }
    return true;
}

void TestDataUploadService::tryUploadAsync(const MesPacketData& pack) {
    if (!isUploadEnabled()) {
        qDebug() << "没有使能上传";
        return;
    }
    const MesPacketData copy = pack;
    QtConcurrent::run([copy]() {
        QString message;
        uploadFromPack(copy, &message);
        if (!message.isEmpty()) {
            qDebug() << QStringLiteral("[TestDataUpload]") << message;
        }
    });
}
