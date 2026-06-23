#include "threshold_sync_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QJsonArray>
#include <QUrlQuery>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

ThresholdSyncService::SyncResult ThresholdSyncService::syncFromCloud(const QString& stationKey,
                                                                     const QString& productModel) {
    SyncResult result;
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            result.message = login.message;
            return result;
        }
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("stationKey"),
                       stationKey.isEmpty() ? FactoryCloudClient::stationKey() : stationKey);
    if (!productModel.isEmpty()) {
        query.addQueryItem(QStringLiteral("productModel"), productModel);
    }

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/thresholds"), query);
    if (!api.ok) {
        result.message = api.message;
        return result;
    }

    const QString remoteVersion = api.data.value(QStringLiteral("version")).toVariant().toString().trimmed();
    const QString localVersion = SETTINGS.value(QStringLiteral("FactoryCloud/ThresholdVersion")).toString().trimmed();
    if (!remoteVersion.isEmpty() && remoteVersion == localVersion) {
        result.ok = true;
        result.message = QStringLiteral("阈值已是最新（version=%1）").arg(remoteVersion);
        result.version = remoteVersion;
        return result;
    }

    const QJsonArray items = api.data.value(QStringLiteral("items")).toArray();
    if (items.isEmpty()) {
        result.message = QStringLiteral("云端未返回阈值项");
        return result;
    }

    int count = 0;
    for (const QJsonValue& v : items) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject item = v.toObject();
        const QString key = item.value(QStringLiteral("settingsKey")).toString().trimmed();
        if (key.isEmpty()) {
            continue;
        }
        const QVariant value = item.value(QStringLiteral("value")).toVariant();
        SETTINGS.setValue(key, value);
        ++count;
    }
    if (!remoteVersion.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/ThresholdVersion"), remoteVersion);
    }
    SETTINGS.sync();

    result.ok = true;
    result.version = remoteVersion;
    result.appliedCount = count;
    result.message = QStringLiteral("阈值同步成功，已写入 %1 项").arg(count);
    if (!remoteVersion.isEmpty()) {
        result.message += QStringLiteral("（version=%1）").arg(remoteVersion);
    }
    return result;
}
