#include "host_ota_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>
#include <QTextStream>
#include <QTimer>
#include <QJsonObject>
#include <functional>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QString sha256File(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        return {};
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool startDeleteSelfBat(const QString& newExePath) {
    const QString batFileName = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("delete_self.bat"));
    QFile batFile(batFileName);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&batFile);
    out << "@echo off\n";
    out << "timeout /t 2 /nobreak >nul\n";
    out << "del \"" << QCoreApplication::applicationFilePath() << "\"\n";
    out << "del \"" << batFileName << "\"\n";
    batFile.close();

    QProcess::startDetached(newExePath);
    QProcess::startDetached(batFileName);
    QTimer::singleShot(1000, []() {
        qApp->quit();
        QProcess::startDetached(QStringLiteral("cmd.exe"),
                                {QStringLiteral("/c"), QStringLiteral("taskkill /f /pid ") + QString::number(QCoreApplication::applicationPid())});
    });
    return true;
}

} // namespace

HostOtaService::CheckResult HostOtaService::checkUpdate() {
    CheckResult result;
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            result.message = login.message;
            return result;
        }
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("packageName"), FactoryCloudClient::packageName());
    query.addQueryItem(QStringLiteral("buildId"), FactoryCloudClient::buildId());
    query.addQueryItem(QStringLiteral("appVersion"), FactoryCloudClient::appVersion());
    query.addQueryItem(QStringLiteral("stationKey"), FactoryCloudClient::stationKey());
    query.addQueryItem(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/host-app/check"), query);
    if (!api.ok) {
        result.message = api.message;
        return result;
    }

    result.ok = true;
    result.hasUpdate = api.data.value(QStringLiteral("hasUpdate")).toBool();
    if (!result.hasUpdate && api.data.contains(QStringLiteral("latest"))) {
        const QJsonObject latest = api.data.value(QStringLiteral("latest")).toObject();
        result.hasUpdate = !latest.isEmpty();
        result.appVersion = latest.value(QStringLiteral("appVersion")).toString();
        result.buildId = latest.value(QStringLiteral("buildId")).toString();
        result.downloadUrl = latest.value(QStringLiteral("downloadUrl")).toString();
        result.sha256 = latest.value(QStringLiteral("sha256")).toString();
        result.forceUpgrade = latest.value(QStringLiteral("forceUpgrade")).toBool();
        result.releaseNotes = latest.value(QStringLiteral("releaseNotes")).toString();
        result.packageName = latest.value(QStringLiteral("packageName")).toString();
    } else {
        result.appVersion = api.data.value(QStringLiteral("latestVersion")).toString();
        result.buildId = api.data.value(QStringLiteral("buildId")).toString();
        result.downloadUrl = api.data.value(QStringLiteral("downloadUrl")).toString();
        result.sha256 = api.data.value(QStringLiteral("sha256")).toString();
        result.forceUpgrade = api.data.value(QStringLiteral("forceUpgrade")).toBool();
        result.releaseNotes = api.data.value(QStringLiteral("releaseNotes")).toString();
        result.packageName = api.data.value(QStringLiteral("packageName")).toString();
    }

    if (result.hasUpdate) {
        result.message = QStringLiteral("发现新版本 %1 (buildId=%2)").arg(result.appVersion, result.buildId);
    } else {
        result.message = QStringLiteral("当前已是最新版本");
    }
    return result;
}

bool HostOtaService::downloadAndApply(const CheckResult& info, QWidget* parent, QString* message) {
    if (!info.hasUpdate) {
        if (message) {
            *message = QStringLiteral("无可用更新");
        }
        return false;
    }

    const QString updatesDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("updates"));
    QDir().mkpath(updatesDir);
    const QString fileName =
        QStringLiteral("%1_%2.exe").arg(FactoryCloudClient::packageName(), info.buildId);
    const QString savePath = QDir(updatesDir).filePath(fileName);

    QString downloadError;
    const QString url = info.downloadUrl.trimmed();
    if (url.isEmpty()) {
        if (!FactoryCloudClient::downloadToFile(QStringLiteral("/host-app/download/") + info.buildId, QUrlQuery(),
                                                savePath, &downloadError)) {
            if (message) {
                *message = downloadError;
            }
            return false;
        }
    } else if (!FactoryCloudClient::downloadToFile(url, QUrlQuery(), savePath, &downloadError)) {
        if (message) {
            *message = downloadError;
        }
        return false;
    }

    if (!info.sha256.isEmpty()) {
        const QString actual = sha256File(savePath);
        if (actual.compare(info.sha256, Qt::CaseInsensitive) != 0) {
            QFile::remove(savePath);
            if (message) {
                *message = QStringLiteral("sha256 校验失败");
            }
            return false;
        }
    }

    SETTINGS.setValue(QStringLiteral("FactoryCloud/HostAppVersion"), info.appVersion);
    SETTINGS.setValue(QStringLiteral("FactoryCloud/HostAppBuildId"), info.buildId);
    SETTINGS.setValue(QStringLiteral("HostAppVersion"), info.appVersion);
    SETTINGS.setValue(QStringLiteral("HostAppBuildId"), info.buildId);
    SETTINGS.sync();

    if (parent) {
        QMessageBox::information(parent, QStringLiteral("软件更新"),
                                 QStringLiteral("下载完成，即将重启并安装新版本…"));
    }
    if (!startDeleteSelfBat(savePath)) {
        if (message) {
            *message = QStringLiteral("无法启动升级脚本");
        }
        return false;
    }
    if (message) {
        *message = QStringLiteral("正在安装新版本…");
    }
    return true;
}

bool HostOtaService::tryInteractiveUpdate(QWidget* parent,
                                          const std::function<void(const QString&)>& logFn) {
    const QString baseUrl = SETTINGS.value(QStringLiteral("FactoryCloud/BaseUrl")).toString().trimmed();
    if (baseUrl.isEmpty() ||
        !SETTINGS.value(QStringLiteral("FactoryCloud/Feature/HostOtaCheck"), true).toBool()) {
        return false;
    }

    auto log = [&](const QString& text) {
        if (logFn) {
            logFn(text);
        }
    };

    const CheckResult check = checkUpdate();
    if (!check.ok) {
        log(check.message);
        return false;
    }
    if (!check.hasUpdate) {
        log(check.message);
        return true;
    }

    const QString text = check.releaseNotes.isEmpty()
        ? check.message
        : check.message + QStringLiteral("\n\n") + check.releaseNotes;
    if (!check.forceUpgrade) {
        const auto answer = QMessageBox::question(parent, QStringLiteral("软件更新"), text);
        if (answer != QMessageBox::Yes) {
            return true;
        }
    } else if (parent) {
        QMessageBox::information(parent, QStringLiteral("软件更新"),
                                 QStringLiteral("检测到强制升级版本，即将下载…\n\n") + text);
    }

    QString message;
    downloadAndApply(check, parent, &message);
    if (!message.isEmpty()) {
        log(message);
    }
    return true;
}
