#include "host_ota_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
#include "host_ota_apply.h"
#include "host_ota_rollback.h"
#include "host_ota_ui.h"

#include <functional>

#include <QCoreApplication>
#include <QDebug>
#include <QJsonObject>
#include <QUrlQuery>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

HostOtaService::CheckResult HostOtaService::checkUpdate() {
    CheckResult result;
    if (AuthService::isOfflineSession()) {
        result.message = QStringLiteral("离线测试模式，跳过 OTA 检查");
        qDebug() << "[OTA]" << result.message;
        return result;
    }
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            result.message = login.message;
            qDebug() << "[OTA] 登录失败:" << login.message;
            return result;
        }
    }

    const QString pkg = FactoryCloudClient::packageName();
    const QString bid = FactoryCloudClient::buildId();
    const QString ver = FactoryCloudClient::appVersion();
    const QString exePath = QCoreApplication::applicationFilePath();
    qDebug() << "[OTA] 检查更新 当前exe=" << exePath << "packageName=" << pkg << "buildId=" << bid
             << "appVersion=" << ver;

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("packageName"), pkg);
    query.addQueryItem(QStringLiteral("buildId"), bid);
    query.addQueryItem(QStringLiteral("appVersion"), ver);
    query.addQueryItem(QStringLiteral("stationKey"), FactoryCloudClient::stationKey());
    query.addQueryItem(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/host-app/check"), query);
    if (!api.ok) {
        result.message = api.message;
        qDebug() << "[OTA] 请求失败:" << api.message;
        return result;
    }

    result.ok = true;
    result.hostNewer = api.data.value(QStringLiteral("hostNewer")).toBool();
    result.hasUpdate = api.data.value(QStringLiteral("hasUpdate")).toBool();
    if (api.data.contains(QStringLiteral("latest"))) {
        const QJsonObject latest = api.data.value(QStringLiteral("latest")).toObject();
        if (!latest.isEmpty()) {
            result.appVersion = latest.value(QStringLiteral("appVersion")).toString();
            result.buildId = latest.value(QStringLiteral("buildId")).toString();
            result.downloadUrl = latest.value(QStringLiteral("downloadUrl")).toString();
            result.sha256 = latest.value(QStringLiteral("sha256")).toString();
            result.forceUpgrade = latest.value(QStringLiteral("forceUpgrade")).toBool();
            result.releaseNotes = latest.value(QStringLiteral("releaseNotes")).toString();
            result.packageName = latest.value(QStringLiteral("packageName")).toString();
            result.packageKind = latest.value(QStringLiteral("packageKind")).toString();
            result.fileCount = latest.value(QStringLiteral("fileCount")).toInt();
            result.uploadedAt = latest.value(QStringLiteral("uploadedAt")).toString();
        }
    } else {
        result.appVersion = api.data.value(QStringLiteral("latestVersion")).toString();
        result.buildId = api.data.value(QStringLiteral("buildId")).toString();
        result.downloadUrl = api.data.value(QStringLiteral("downloadUrl")).toString();
        result.sha256 = api.data.value(QStringLiteral("sha256")).toString();
        result.forceUpgrade = api.data.value(QStringLiteral("forceUpgrade")).toBool();
        result.releaseNotes = api.data.value(QStringLiteral("releaseNotes")).toString();
        result.packageName = api.data.value(QStringLiteral("packageName")).toString();
    }
    if (result.uploadedAt.isEmpty()) {
        result.uploadedAt = api.data.value(QStringLiteral("uploadedAt")).toString();
    }

    qDebug() << "[OTA] 检查结果:"
             << "hostNewer=" << result.hostNewer
             << "hasUpdate=" << result.hasUpdate
             << "latestAppVersion=" << result.appVersion
             << "latestBuildId=" << result.buildId;

    if (result.hasUpdate) {
        result.message = QStringLiteral("发现新版本 %1 (buildId=%2)").arg(result.appVersion, result.buildId);
    } else if (result.hostNewer) {
        result.message = QStringLiteral("本地版本比服务器新");
    } else {
        result.message = QStringLiteral("当前已是最新版本");
    }
    return result;
}

bool HostOtaService::downloadAndApply(const CheckResult& info, QWidget* parent, QString* message) {
    return HostOtaApply::downloadAndApply(info, parent, message);
}

void HostOtaService::cleanupStaleBackupProcess() {
    HostOtaRollback::cleanupStaleBackupProcess();
}

bool HostOtaService::uploadCurrentExe(QString* message, const QString& releaseNotes) {
    return HostOtaUi::uploadCurrentExe(message, releaseNotes);
}

bool HostOtaService::showVersionPicker(QWidget* parent,
                                       const std::function<void(const QString&)>& logFn) {
    return HostOtaUi::showVersionPicker(parent, logFn);
}

bool HostOtaService::hasPreviousVersionBackup() {
    return HostOtaRollback::hasPreviousVersionBackup();
}

bool HostOtaService::rollbackToPreviousVersion(QWidget* parent, QString* message) {
    return HostOtaRollback::rollbackToPreviousVersion(parent, message);
}

bool HostOtaService::tryInteractiveUpdate(QWidget* parent,
                                          const std::function<void(const QString&)>& logFn) {
    return HostOtaUi::tryInteractiveUpdate(parent, logFn);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
