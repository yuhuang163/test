#include "host_ota_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QTextStream>
#include <QTimer>
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

bool startDeleteSelfBat(const QString& savePath) {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString appFilePath = QCoreApplication::applicationFilePath();
    const QString appFileStem = QFileInfo(appFilePath).completeBaseName();
    const QString batFileName = QDir(appDir).filePath(QStringLiteral("delete_self.bat"));
    const QString tempExePath = savePath + QStringLiteral(".tmp");

    QFile batFile(batFileName);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&batFile);
    out << "@echo off\n";
    out << "timeout /t 2 /nobreak >nul\n";
    out << "rename \"" << QDir::toNativeSeparators(appFilePath)
        << "\" \"" << appFileStem << ".bak\"\n";
    out << "rename \"" << QDir::toNativeSeparators(tempExePath)
        << "\" \"" << QFileInfo(savePath).fileName() << "\"\n";
    out << "start \"\" \"" << QDir::toNativeSeparators(savePath) << "\"\n";
    out << "del \"" << appFileStem << ".bak\"\n";
    out << "del \"" << QDir::toNativeSeparators(batFileName) << "\"\n";
    batFile.close();

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
            qDebug() << "[OTA] 登录失败:" << login.message;
            return result;
        }
    }

    const QString pkg = FactoryCloudClient::packageName();
    const QString bid = FactoryCloudClient::buildId();
    const QString ver = FactoryCloudClient::appVersion();
    qDebug() << "[OTA] 检查更新 发送: packageName=" << pkg << "buildId=" << bid << "appVersion=" << ver;

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
    result.uploadedAt = api.data.value(QStringLiteral("uploadedAt")).toString();

    qDebug() << "[OTA] 检查结果:"
             << "hostNewer=" << result.hostNewer
             << "hasUpdate=" << result.hasUpdate
             << "latestAppVersion=" << result.appVersion
             << "latestBuildId=" << result.buildId;

    if (result.hasUpdate) {
        result.message = QStringLiteral("发现新版本 %1 (buildId=%2)").arg(result.appVersion, result.buildId);
    } else if (result.hostNewer) {
        result.message = QStringLiteral("本地版本比服务器新，正在上传…");
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

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString fileName =
        QStringLiteral("%1_%2.exe").arg(FactoryCloudClient::packageName(), info.buildId);
    const QString savePath = QDir(appDir).filePath(fileName);
    const QString tempSavePath = savePath + QStringLiteral(".tmp");

    QString downloadError;
    const QString url = info.downloadUrl.trimmed();
    if (url.isEmpty()) {
        QUrlQuery downloadQuery;
        if (!info.uploadedAt.isEmpty()) {
            downloadQuery.addQueryItem(QStringLiteral("uploadedAt"), info.uploadedAt);
        }
        if (!FactoryCloudClient::downloadToFile(QStringLiteral("/host-app/download/") + info.buildId, downloadQuery,
                                                tempSavePath, &downloadError)) {
            if (message) {
                *message = downloadError;
            }
            return false;
        }
    } else if (!FactoryCloudClient::downloadToFile(url, QUrlQuery(), tempSavePath, &downloadError)) {
        if (message) {
            *message = downloadError;
        }
        return false;
    }

    if (!info.sha256.isEmpty()) {
        const QString actual = sha256File(tempSavePath);
        if (actual.compare(info.sha256, Qt::CaseInsensitive) != 0) {
            qDebug() << "[OTA] sha256 不匹配: 期望=" << info.sha256 << "实际=" << actual;
            QFile::remove(tempSavePath);
            if (message) {
                *message = QStringLiteral("sha256 校验失败");
            }
            return false;
        }
    }

    // 不再写入 settings —— appVersion() 和 buildId() 始终从 exe 解析

    if (parent) {
        QMessageBox* msgBox = new QMessageBox(parent);
        msgBox->setWindowTitle(QStringLiteral("软件更新"));
        msgBox->setText(QStringLiteral("下载完成，即将重启并安装新版本…\n\n5秒后自动关闭"));
        msgBox->setStandardButtons(QMessageBox::Ok);
        msgBox->setDefaultButton(QMessageBox::Ok);
        QTimer::singleShot(5000, msgBox, &QMessageBox::accept);
        msgBox->exec();
        msgBox->deleteLater();
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

bool HostOtaService::uploadCurrentExe(QString* message) {
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            if (message) {
                *message = login.message;
            }
            qDebug() << "[OTA] 上传失败: 登录失败" << (message ? *message : "");
            return false;
        }
    }

    const QString exePath = QCoreApplication::applicationFilePath();
    if (!QFile::exists(exePath)) {
        if (message) {
            *message = QStringLiteral("当前 exe 不存在");
        }
        qDebug() << "[OTA] 上传失败: exe 不存在" << exePath;
        return false;
    }

    const QString sha256 = sha256File(exePath);
    const QString appVer = FactoryCloudClient::appVersion();
    const QString bid = FactoryCloudClient::buildId();
    const QString pkg = FactoryCloudClient::packageName();
    qDebug() << "[OTA] 上传 exe:" << exePath << "appVersion=" << appVer << "buildId=" << bid << "packageName=" << pkg;

    QList<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("appVersion"), appVer});
    fields.append({QStringLiteral("buildId"), bid});
    fields.append({QStringLiteral("packageName"), pkg});
    if (!sha256.isEmpty()) {
        fields.append({QStringLiteral("sha256"), sha256});
    }

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::uploadExe(exePath, fields);
    if (!api.ok) {
        if (message) {
            *message = api.message;
        }
        qDebug() << "[OTA] 上传失败:" << (message ? *message : "");
        return false;
    }
    if (message) {
        *message = QStringLiteral("上位机版本已上报（%1 buildId=%2）").arg(appVer, bid);
    }
    qDebug() << "[OTA] 上传成功:" << appVer << bid;
    return true;
}

bool HostOtaService::showVersionPicker(QWidget* parent,
                                       const std::function<void(const QString&)>& logFn) {
    auto log = [&](const QString& text) {
        if (logFn) {
            logFn(text);
        }
    };

    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            if (parent) {
                QMessageBox::warning(parent, QStringLiteral("检查更新"), login.message);
            }
            log(login.message);
            return false;
        }
    }

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/host-app/versions"));
    if (!api.ok) {
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("检查更新"), api.message);
        }
        log(api.message);
        return false;
    }

    const QJsonArray items = api.data.value(QStringLiteral("items")).toArray();
    if (items.isEmpty()) {
        if (parent) {
            QMessageBox::information(parent, QStringLiteral("检查更新"),
                                     QStringLiteral("服务器上暂无可用版本"));
        }
        return true;
    }

    // 构建版本列表显示文字
    QStringList displayItems;
    QList<QJsonObject> versionObjects;
    for (const auto& val : items) {
        const QJsonObject v = val.toObject();
        const QString ver = v.value(QStringLiteral("appVersion")).toString();
        const QString bid = v.value(QStringLiteral("buildId")).toString();
        const QString notes = v.value(QStringLiteral("releaseNotes")).toString();
        const QString time = v.value(QStringLiteral("uploadedAt")).toString();
        QString label = QStringLiteral("%1 (buildId=%2)").arg(ver, bid);
        if (!notes.isEmpty()) {
            label += QStringLiteral(" - %1").arg(notes);
        }
        displayItems.append(label);
        versionObjects.append(v);
    }

    // 弹出版本选择对话框
    bool ok = false;
    const QString selected = QInputDialog::getItem(parent, QStringLiteral("选择版本"),
                                                    QStringLiteral("请选择要升级的版本："),
                                                    displayItems, 0, false, &ok);
    if (!ok || selected.isEmpty()) {
        return true;
    }

    const int idx = displayItems.indexOf(selected);
    if (idx < 0 || idx >= versionObjects.size()) {
        return true;
    }

    const QJsonObject chosen = versionObjects[idx];
    CheckResult info;
    info.hasUpdate = true;
    info.appVersion = chosen.value(QStringLiteral("appVersion")).toString();
    info.buildId = chosen.value(QStringLiteral("buildId")).toString();
    info.sha256 = chosen.value(QStringLiteral("sha256")).toString();
    info.forceUpgrade = chosen.value(QStringLiteral("forceUpgrade")).toBool();
    info.releaseNotes = chosen.value(QStringLiteral("releaseNotes")).toString();
    info.packageName = chosen.value(QStringLiteral("packageName")).toString();
    info.uploadedAt = chosen.value(QStringLiteral("uploadedAt")).toString();

    QString message;
    downloadAndApply(info, parent, &message);
    if (!message.isEmpty()) {
        log(message);
    }
    return true;
}

bool HostOtaService::tryInteractiveUpdate(QWidget* parent,
                                          const std::function<void(const QString&)>& logFn) {
    const QString baseUrl = FactoryCloudClient::baseUrl();
    qDebug() << "[OTA] tryInteractiveUpdate baseUrl=" << baseUrl;
    if (baseUrl.isEmpty() ||
        !SETTINGS.value(QStringLiteral("FactoryCloud/Feature/HostOtaCheck"), true).toBool()) {
        qDebug() << "[OTA] OTA 未配置，跳过";
        return false;
    }

    auto log = [&](const QString& text) {
        if (logFn) {
            logFn(text);
        }
    };

    log(QStringLiteral("[OTA] 正在检查更新…"));
    const CheckResult check = checkUpdate();
    if (!check.ok) {
        log(QStringLiteral("[OTA] 检查失败: ") + check.message);
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("检查更新"), check.message);
        }
        return false;
    }

    if (check.hostNewer) {
        log(QStringLiteral("[OTA] hostNewer=true，尝试上传当前版本"));
        QString uploadMsg;
        const bool uploaded = uploadCurrentExe(&uploadMsg);
        if (parent) {
            if (uploaded) {
                QMessageBox::information(parent, QStringLiteral("检查更新"),
                                         QStringLiteral("已将本地版本上传至服务器"));
            } else {
                QMessageBox::warning(parent, QStringLiteral("检查更新"),
                                     QStringLiteral("上传失败：") + uploadMsg);
            }
        }
        log(QStringLiteral("[OTA] 上传结果: ") + uploadMsg);
        return true;
    }

    log(QStringLiteral("[OTA] hostNewer=false，弹出版本选择列表"));
    return showVersionPicker(parent, logFn);
}
