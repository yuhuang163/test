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
#include <QPushButton>
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
    const QString appFileName = QFileInfo(appFilePath).fileName();
    const QString bakFileName = appFileName + QStringLiteral(".bak");
    const QString batFileName = QDir(appDir).filePath(QStringLiteral("delete_self.bat"));
    const QString tempExePath = savePath + QStringLiteral(".tmp");
    const QString logPath = QDir(appDir).filePath(QStringLiteral("ota_replace.log"));

    // 旧 exe 仍被占用时，Windows rename 会失败，留下 .tmp 且不会启动
    QFile batFile(batFileName);
    if (!batFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&batFile);
    out << "@echo off\r\n";
    out << "cd /d \"" << QDir::toNativeSeparators(appDir) << "\"\r\n";
    out << "echo [%date% %time%] OTA replace start > \"" << QDir::toNativeSeparators(logPath) << "\"\r\n";
    out << "timeout /t 3 /nobreak >nul\r\n";
    // 重试：等旧进程真正释放文件锁
    out << "set /a tries=0\r\n";
    out << ":retry_bak\r\n";
    out << "set /a tries+=1\r\n";
    out << "if exist \"" << QDir::toNativeSeparators(QDir(appDir).filePath(bakFileName)) << "\" del /f /q \""
        << QDir::toNativeSeparators(QDir(appDir).filePath(bakFileName)) << "\" >nul 2>&1\r\n";
    out << "move /y \"" << QDir::toNativeSeparators(appFilePath) << "\" \""
        << QDir::toNativeSeparators(QDir(appDir).filePath(bakFileName)) << "\" >> \""
        << QDir::toNativeSeparators(logPath) << "\" 2>&1\r\n";
    out << "if errorlevel 1 (\r\n";
    out << "  if %tries% geq 20 (\r\n";
    out << "    echo move old exe to bak failed >> \"" << QDir::toNativeSeparators(logPath) << "\"\r\n";
    out << "    goto fail\r\n";
    out << "  )\r\n";
    out << "  timeout /t 1 /nobreak >nul\r\n";
    out << "  goto retry_bak\r\n";
    out << ")\r\n";
    out << "set /a tries=0\r\n";
    out << ":retry_new\r\n";
    out << "set /a tries+=1\r\n";
    out << "move /y \"" << QDir::toNativeSeparators(tempExePath) << "\" \""
        << QDir::toNativeSeparators(savePath) << "\" >> \"" << QDir::toNativeSeparators(logPath) << "\" 2>&1\r\n";
    out << "if errorlevel 1 (\r\n";
    out << "  if %tries% geq 20 (\r\n";
    out << "    echo move tmp to exe failed >> \"" << QDir::toNativeSeparators(logPath) << "\"\r\n";
    out << "    goto fail\r\n";
    out << "  )\r\n";
    out << "  timeout /t 1 /nobreak >nul\r\n";
    out << "  goto retry_new\r\n";
    out << ")\r\n";
    out << "echo replace ok, starting >> \"" << QDir::toNativeSeparators(logPath) << "\"\r\n";
    out << "start \"\" \"" << QDir::toNativeSeparators(savePath) << "\"\r\n";
    out << "del /f /q \"" << QDir::toNativeSeparators(QDir(appDir).filePath(bakFileName)) << "\" >nul 2>&1\r\n";
    out << "del /f /q \"%~f0\" >nul 2>&1\r\n";
    out << "exit /b 0\r\n";
    out << ":fail\r\n";
    out << "echo OTA replace failed, keep .tmp for manual recover >> \"" << QDir::toNativeSeparators(logPath)
        << "\"\r\n";
    out << "exit /b 1\r\n";
    batFile.close();

    // 必须用 cmd + 工作目录，否则部分电脑直接 startDetached(.bat) 会静默失败
    if (!QProcess::startDetached(QStringLiteral("cmd.exe"),
                                 {QStringLiteral("/c"), QDir::toNativeSeparators(batFileName)}, appDir)) {
        return false;
    }
    QTimer::singleShot(1000, []() {
        qApp->quit();
        QProcess::startDetached(QStringLiteral("cmd.exe"),
                                {QStringLiteral("/c"),
                                 QStringLiteral("taskkill /f /pid ") + QString::number(QCoreApplication::applicationPid())});
    });
    return true;
}

} // namespace

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
        result.message = QStringLiteral("本地版本比服务器新");
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
    // 固定覆盖当前运行的 exe 名（TARGET=new_production），不再落地为 package_buildId.exe
    const QString fileName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString savePath = QDir(appDir).filePath(fileName);
    const QString tempSavePath = savePath + QStringLiteral(".tmp");
    // 上次替换失败残留的 .tmp 先清掉，避免下载写不进/脚本误用旧包
    if (QFile::exists(tempSavePath))
        QFile::remove(tempSavePath);

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

    // 版本号来自 host_ota_version.h 编译进新包，无需写 settings

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

static QString promptUploadReleaseNotes(QWidget* parent) {
    for (;;) {
        bool ok = false;
        const QString notes = QInputDialog::getMultiLineText(
            parent, QStringLiteral("上传上位机"), QStringLiteral("请填写本次修改内容："), QString(), &ok);
        if (!ok) {
            return QString();
        }
        const QString trimmed = notes.trimmed();
        if (!trimmed.isEmpty()) {
            return trimmed;
        }
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("上传上位机"), QStringLiteral("修改内容不能为空，请填写后再上传。"));
        }
    }
}

static bool confirmDownloadWithReleaseNotes(QWidget* parent, const HostOtaService::CheckResult& info) {
    if (!parent) {
        return true;
    }
    const QString notes = info.releaseNotes.trimmed();
    const QString notesText =
        notes.isEmpty() ? QStringLiteral("（上传时未填写修改说明）") : notes;
    const QString text = QStringLiteral("版本：%1\nbuildId：%2\n\n修改内容：\n%3\n\n是否下载并安装？")
                             .arg(info.appVersion, info.buildId, notesText);
    return QMessageBox::question(parent, QStringLiteral("下载上位机"), text, QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) == QMessageBox::Yes;
}

bool HostOtaService::uploadCurrentExe(QString* message, const QString& releaseNotes) {
    if (AuthService::isOfflineSession()) {
        if (message) {
            *message = QStringLiteral("离线测试模式，无法上传 exe");
        }
        return false;
    }
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
    // 与服务端 POST /host-app/upload 一致：仅 admin 可上传当前上位机版本
    if (!AuthService::isAdmin()) {
        if (message) {
            *message = QStringLiteral("仅管理员可上传当前上位机版本");
        }
        qDebug() << "[OTA] 上传失败: 非 admin";
        return false;
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
    qDebug() << "[OTA] 上传 exe:" << exePath << "appVersion=" << appVer << "buildId=" << bid << "packageName=" << pkg
             << "releaseNotes=" << releaseNotes.trimmed();

    QList<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("appVersion"), appVer});
    fields.append({QStringLiteral("buildId"), bid});
    fields.append({QStringLiteral("packageName"), pkg});
    fields.append({QStringLiteral("releaseNotes"), releaseNotes.trimmed()});
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

    if (AuthService::isOfflineSession()) {
        const QString msg = QStringLiteral("离线测试模式，无法检查更新");
        if (parent) {
            QMessageBox::information(parent, QStringLiteral("检查更新"), msg);
        }
        log(msg);
        return false;
    }
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
        const QString time = v.value(QStringLiteral("uploadedAt")).toString();
        QString label = QStringLiteral("%1 (buildId=%2)").arg(ver, bid);
        if (!time.isEmpty()) {
            label += QStringLiteral("  %1").arg(time);
        }
        displayItems.append(label);
        versionObjects.append(v);
    }

    // 弹出版本选择对话框
    bool ok = false;
    const QString selected = QInputDialog::getItem(parent, QStringLiteral("选择版本"),
                                                    QStringLiteral("请选择要升级的版本（选定后将显示修改内容）："),
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

    if (!confirmDownloadWithReleaseNotes(parent, info)) {
        log(QStringLiteral("[OTA] 用户取消下载"));
        return true;
    }

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
    if (baseUrl.isEmpty()) {
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

    if (check.hasUpdate) {
        log(QStringLiteral("[OTA] 发现服务器有新版本，弹出版本选择列表"));
        return showVersionPicker(parent, logFn);
    }

    // 已是最新或本地更新：admin 可上传当前包；均可从服务器选择任意历史版本下载（含降级）
    const QString localVer = FactoryCloudClient::appVersion();
    const QString localBid = FactoryCloudClient::buildId();
    const QString statusText =
        check.hostNewer
            ? QStringLiteral("本地版本（%1，buildId=%2）比服务器新。").arg(localVer, localBid)
            : QStringLiteral("当前已是最新版本（%1，buildId=%2）。").arg(localVer, localBid);
    log(QStringLiteral("[OTA] ") + check.message);

    if (!parent) {
        log(QStringLiteral("[OTA] 无界面，跳过"));
        return true;
    }

    const bool canUpload = AuthService::isAdmin();
    QMessageBox box(parent);
    box.setWindowTitle(QStringLiteral("检查更新"));
    box.setText(statusText + QStringLiteral("\n\n请选择操作："));
    QPushButton* uploadBtn =
        canUpload ? box.addButton(QStringLiteral("上传当前版本"), QMessageBox::ActionRole) : nullptr;
    QPushButton* downloadBtn = box.addButton(QStringLiteral("下载其他版本"), QMessageBox::ActionRole);
    QPushButton* cancelBtn = box.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
    box.setDefaultButton(cancelBtn);
    box.exec();

    if (box.clickedButton() == cancelBtn || box.clickedButton() == nullptr) {
        log(QStringLiteral("[OTA] 用户取消"));
        return true;
    }
    if (box.clickedButton() == downloadBtn) {
        log(QStringLiteral("[OTA] 用户选择下载其他版本"));
        return showVersionPicker(parent, logFn);
    }
    if (!canUpload || box.clickedButton() != uploadBtn) {
        log(QStringLiteral("[OTA] 未选择上传（非管理员无上传入口）"));
        return true;
    }

    const QString releaseNotes = promptUploadReleaseNotes(parent);
    if (releaseNotes.isEmpty()) {
        log(QStringLiteral("[OTA] 用户取消上传或未填写修改内容"));
        return true;
    }

    QString uploadMsg;
    const bool uploaded = uploadCurrentExe(&uploadMsg, releaseNotes);
    if (uploaded) {
        QMessageBox::information(parent, QStringLiteral("检查更新"),
                                 QStringLiteral("已将目前版本上传至服务器：\n") + uploadMsg);
    } else {
        QMessageBox::warning(parent, QStringLiteral("检查更新"),
                             QStringLiteral("上传失败：") + uploadMsg);
    }
    log(QStringLiteral("[OTA] 上传结果: ") + uploadMsg);
    return true;
}
