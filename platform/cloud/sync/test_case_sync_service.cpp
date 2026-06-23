#include "test_case_sync_service.h"

#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QProcess>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

bool runPowerShell(const QString& script, QString* error, int timeoutMs = 180000) {
    QProcess process;
    process.setProgram(QStringLiteral("powershell"));
    process.setArguments({QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                          QStringLiteral("-Command"), script});
    process.start();
    if (!process.waitForStarted(5000)) {
        if (error) {
            *error = QStringLiteral("无法启动 PowerShell：") + process.errorString();
        }
        return false;
    }
    if (!process.waitForFinished(timeoutMs)) {
        process.kill();
        process.waitForFinished(3000);
        if (error) {
            *error = QStringLiteral("PowerShell 执行超时");
        }
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) {
            *error = QString::fromUtf8(process.readAllStandardError()).trimmed();
        }
        return false;
    }
    return true;
}

QString escapePs(const QString& text) {
    return QString(text).replace(QLatin1Char('\''), QLatin1String("''"));
}

bool backupDir(const QString& root, QString* error) {
    const QString backup = root + QStringLiteral("/.backup");
    QDir backupDir(backup);
    if (backupDir.exists()) {
        backupDir.removeRecursively();
    }
    if (!QDir(root).exists()) {
        return true;
    }
    const QString script =
        QStringLiteral("Copy-Item -LiteralPath '%1' -Destination '%2' -Recurse -Force")
            .arg(escapePs(QDir::toNativeSeparators(root)), escapePs(QDir::toNativeSeparators(backup)));
    return runPowerShell(script, error);
}

bool extractZip(const QString& zipPath, const QString& destDir, QString* error) {
    QDir().mkpath(destDir);
    const QString script =
        QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
            .arg(escapePs(QDir::toNativeSeparators(zipPath)), escapePs(QDir::toNativeSeparators(destDir)));
    return runPowerShell(script, error);
}

} // namespace

QString TestCaseSyncService::testCaseRoot() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test_case"));
}

TestCaseSyncService::SyncResult TestCaseSyncService::syncFromCloud() {
    SyncResult result;
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            result.message = login.message;
            return result;
        }
    }

    const FactoryCloudClient::ApiResult manifest =
        FactoryCloudClient::get(QStringLiteral("/test-cases/manifest"));
    if (!manifest.ok) {
        result.message = manifest.message;
        return result;
    }

    const QString remoteVersion = manifest.data.value(QStringLiteral("bundleVersion")).toString().trimmed();
    const QString localVersion = SETTINGS.value(QStringLiteral("FactoryCloud/TestCaseBundleVersion")).toString().trimmed();
    if (!remoteVersion.isEmpty() && remoteVersion == localVersion) {
        result.ok = true;
        result.bundleVersion = remoteVersion;
        result.message = QStringLiteral("测试用例已是最新（bundle=%1）").arg(remoteVersion);
        return result;
    }

    const QString root = testCaseRoot();
    QString backupError;
    if (!backupDir(root, &backupError)) {
        result.message = QStringLiteral("备份 test_case 失败：") + backupError;
        return result;
    }

    const QString zipPath = QDir(QDir::tempPath()).filePath(QStringLiteral("test_case_bundle.zip"));
    if (QFile::exists(zipPath)) {
        QFile::remove(zipPath);
    }

    QString downloadError;
    if (!FactoryCloudClient::downloadToFile(QStringLiteral("/test-cases/bundle"), QUrlQuery(), zipPath,
                                            &downloadError)) {
        result.message = downloadError;
        return result;
    }

    const QString extractRoot = QDir(QDir::tempPath()).filePath(QStringLiteral("test_case_extract"));
    QDir(extractRoot).removeRecursively();
    QString extractError;
    if (!extractZip(zipPath, extractRoot, &extractError)) {
        result.message = QStringLiteral("解压 bundle 失败：") + extractError;
        QFile::remove(zipPath);
        return result;
    }
    QFile::remove(zipPath);

    QDir dest(root);
    dest.mkpath(QStringLiteral("."));
    const QFileInfoList entries = QDir(extractRoot).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    int count = 0;
    for (const QFileInfo& info : entries) {
        const QString target = dest.filePath(info.fileName());
        if (info.isDir()) {
            QDir(target).removeRecursively();
            QDir().mkpath(target);
            const QString script =
                QStringLiteral("Copy-Item -LiteralPath '%1\\*' -Destination '%2' -Recurse -Force")
                    .arg(escapePs(QDir::toNativeSeparators(info.absoluteFilePath())),
                         escapePs(QDir::toNativeSeparators(target)));
            QString copyError;
            if (!runPowerShell(script, &copyError)) {
                result.message = QStringLiteral("复制用例失败：") + copyError;
                return result;
            }
        } else {
            if (QFile::exists(target)) {
                QFile::remove(target);
            }
            QFile::copy(info.absoluteFilePath(), target);
        }
        ++count;
    }

    if (!remoteVersion.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/TestCaseBundleVersion"), remoteVersion);
    }
    SETTINGS.sync();

    result.ok = true;
    result.bundleVersion = remoteVersion;
    result.fileCount = count;
    result.message = QStringLiteral("测试用例同步成功，已更新 %1 项").arg(count);
    if (!remoteVersion.isEmpty()) {
        result.message += QStringLiteral("（bundle=%1）").arg(remoteVersion);
    }
    return result;
}
