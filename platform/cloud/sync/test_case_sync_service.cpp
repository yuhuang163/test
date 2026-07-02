#include "test_case_sync_service.h"

#include "test_case.h"

#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QProcess>
#include <QDateTime>
#include <QSysInfo>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kBackupDirName = ".backup";

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

bool copyPath(const QString& src, const QString& dest, QString* error) {
    const QFileInfo info(src);
    if (!info.exists()) {
        return true;
    }
    if (info.isDir()) {
        QDir destDir(dest);
        destDir.mkpath(QStringLiteral("."));
        const QFileInfoList children =
            QDir(src).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo& child : children) {
            if (!copyPath(child.absoluteFilePath(), destDir.filePath(child.fileName()), error)) {
                return false;
            }
        }
        return true;
    }
    if (QFile::exists(dest)) {
        QFile::remove(dest);
    }
    if (!QFile::copy(src, dest)) {
        if (error) {
            *error = QStringLiteral("复制失败：%1 -> %2").arg(src, dest);
        }
        return false;
    }
    return true;
}

bool backupTestCaseDir(const QString& root, QString* error) {
    const QString backup = root + QLatin1Char('/') + QLatin1String(kBackupDirName);
    QDir backupDir(backup);
    if (backupDir.exists()) {
        backupDir.removeRecursively();
    }
    if (!QDir(root).exists()) {
        return true;
    }
    QDir().mkpath(backup);
    const QFileInfoList entries = QDir(root).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.fileName() == QLatin1String(kBackupDirName)) {
            continue;
        }
        if (!copyPath(info.absoluteFilePath(), backupDir.filePath(info.fileName()), error)) {
            return false;
        }
    }
    return true;
}

void clearTestCaseDirExceptBackup(const QString& root) {
    QDir dir(root);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
        return;
    }
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.fileName() == QLatin1String(kBackupDirName)) {
            continue;
        }
        if (info.isDir()) {
            QDir(info.absoluteFilePath()).removeRecursively();
        } else {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

bool extractZip(const QString& zipPath, const QString& destDir, QString* error) {
    QDir().mkpath(destDir);
    const QString script =
        QStringLiteral("Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
            .arg(escapePs(QDir::toNativeSeparators(zipPath)), escapePs(QDir::toNativeSeparators(destDir)));
    return runPowerShell(script, error);
}

QString fileSha256Hex(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        hash.addData(file.read(65536));
    }
    return QString::fromLatin1(hash.result().toHex());
}

bool verifyManifestFiles(const QJsonArray& files, const QString& root, QString* error) {
    for (const QJsonValue& value : files) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject item = value.toObject();
        const QString rel = item.value(QStringLiteral("path")).toString().trimmed();
        const QString expected = item.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        if (rel.isEmpty() || expected.isEmpty()) {
            continue;
        }
        const QString abs = QDir(root).filePath(rel);
        if (!QFile::exists(abs)) {
            if (error) {
                *error = QStringLiteral("解压后缺少文件：%1").arg(rel);
            }
            return false;
        }
        const QString actual = fileSha256Hex(abs).toLower();
        if (actual != expected) {
            if (error) {
                *error = QStringLiteral("文件校验失败：%1").arg(rel);
            }
            return false;
        }
    }
    return true;
}

bool deployExtractedFiles(const QString& extractRoot, const QString& destRoot, int* count, QString* error) {
    const QFileInfoList entries = QDir(extractRoot).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    int n = 0;
    for (const QFileInfo& info : entries) {
        const QString target = QDir(destRoot).filePath(info.fileName());
        if (info.isDir()) {
            if (QDir(target).exists()) {
                QDir(target).removeRecursively();
            }
            if (!copyPath(info.absoluteFilePath(), target, error)) {
                return false;
            }
        } else {
            if (QFile::exists(target)) {
                QFile::remove(target);
            }
            if (!QFile::copy(info.absoluteFilePath(), target)) {
                if (error) {
                    *error = QStringLiteral("复制用例失败：%1").arg(info.fileName());
                }
                return false;
            }
        }
        ++n;
    }
    if (count) {
        *count = n;
    }
    return true;
}

QString compressTestCaseDir(const QString& root, QString* error) {
    const QDir srcDir(root);
    if (!srcDir.exists()) {
        if (error) {
            *error = QStringLiteral("test_case 目录不存在：") + root;
        }
        return {};
    }
    bool hasIni = false;
    const QFileInfoList entries = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.fileName() == QLatin1String(kBackupDirName)) {
            continue;
        }
        if (info.isFile() && info.suffix().compare(QLatin1String("ini"), Qt::CaseInsensitive) == 0) {
            hasIni = true;
            break;
        }
        if (info.isDir()) {
            hasIni = true;
            break;
        }
    }
    if (!hasIni) {
        if (error) {
            *error = QStringLiteral("test_case 目录下无 ini 文件可上传");
        }
        return {};
    }

    const QString zipPath = QDir(QDir::tempPath()).filePath(
        QStringLiteral("test_case_upload_%1.zip").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddhhmmss"))));
    if (QFile::exists(zipPath)) {
        QFile::remove(zipPath);
    }
    const QString script =
        QStringLiteral("$root='%1'; $zip='%2'; $items=@(Get-ChildItem -LiteralPath $root | Where-Object { $_.Name -ne '.backup' }); "
                       "if ($items.Count -eq 0) { throw 'no files' }; "
                       "Compress-Archive -LiteralPath ($items | ForEach-Object { $_.FullName }) -DestinationPath $zip -Force")
            .arg(escapePs(QDir::toNativeSeparators(root)), escapePs(QDir::toNativeSeparators(zipPath)));
    if (!runPowerShell(script, error)) {
        return {};
    }
    if (!QFile::exists(zipPath)) {
        if (error) {
            *error = QStringLiteral("打包 zip 失败");
        }
        return {};
    }
    return zipPath;
}

} // namespace

QString TestCaseSyncService::testCaseRoot() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test_case"));
}

bool TestCaseSyncService::isEnabled() {
    return SETTINGS.value(QStringLiteral("FactoryCloud/Feature/TestCaseSync"), true).toBool();
}

void TestCaseSyncService::tryStartupSyncAsync() {
    if (!isEnabled()) {
        return;
    }
    QtConcurrent::run([]() {
        const SyncResult result = syncFromCloud();
        if (!result.message.isEmpty()) {
            qDebug() << QStringLiteral("[TestCaseSync]") << result.message;
        }
    });
}

TestCaseSyncService::SyncResult TestCaseSyncService::uploadToCloud() {
    SyncResult result;
    if (!isEnabled()) {
        result.message = QStringLiteral("测试用例云同步未启用");
        return result;
    }
    if (AuthService::isOfflineSession()) {
        result.message = QStringLiteral("离线测试模式，无法上传测试用例");
        return result;
    }
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            result.message = login.message;
            return result;
        }
    }

    const QString root = testCaseRoot();
    QString zipError;
    const QString zipPath = compressTestCaseDir(root, &zipError);
    if (zipPath.isEmpty()) {
        result.message = zipError.isEmpty() ? QStringLiteral("打包 test_case 失败") : zipError;
        return result;
    }

    QList<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("deviceId"), FactoryCloudClient::deviceId()});
    fields.append({QStringLiteral("stationKey"), FactoryCloudClient::stationKey()});
    fields.append({QStringLiteral("hostName"), QSysInfo::machineHostName()});

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::uploadMultipart(QStringLiteral("/test-cases/upload"), zipPath, fields);
    QFile::remove(zipPath);

    if (!api.ok) {
        result.message = api.message.isEmpty() ? QStringLiteral("上传失败") : api.message;
        return result;
    }

    const QString bundleVersion = api.data.value(QStringLiteral("bundleVersion")).toString().trimmed();
    const int fileCount = api.data.value(QStringLiteral("fileCount")).toInt(0);
    if (!bundleVersion.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/TestCaseBundleVersion"), bundleVersion);
        SETTINGS.sync();
    }

    result.ok = true;
    result.bundleVersion = bundleVersion;
    result.fileCount = fileCount;
    result.message = QStringLiteral("测试用例上传成功");
    if (fileCount > 0) {
        result.message += QStringLiteral("，共 %1 个文件").arg(fileCount);
    }
    if (!bundleVersion.isEmpty()) {
        result.message += QStringLiteral("（bundle=%1）").arg(bundleVersion);
    }
    return result;
}

TestCaseSyncService::SyncResult TestCaseSyncService::syncFromCloud() {
    SyncResult result;
    if (!isEnabled()) {
        result.message = QStringLiteral("测试用例云同步未启用");
        return result;
    }
    if (AuthService::isOfflineSession()) {
        result.message = QStringLiteral("离线测试模式，无法从云端同步测试用例");
        return result;
    }
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
    if (!backupTestCaseDir(root, &backupError)) {
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

    const QJsonArray files = manifest.data.value(QStringLiteral("files")).toArray();
    QString verifyError;
    if (!files.isEmpty() && !verifyManifestFiles(files, extractRoot, &verifyError)) {
        result.message = verifyError;
        QDir(extractRoot).removeRecursively();
        return result;
    }

    QDir(root).mkpath(QStringLiteral("."));
    clearTestCaseDirExceptBackup(root);

    int count = 0;
    QString deployError;
    if (!deployExtractedFiles(extractRoot, root, &count, &deployError)) {
        result.message = deployError;
        QDir(extractRoot).removeRecursively();
        return result;
    }
    QDir(extractRoot).removeRecursively();

    if (!remoteVersion.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/TestCaseBundleVersion"), remoteVersion);
    }
    SETTINGS.sync();

    TestCaseStore::invalidateCloudItemNameCache();

    result.ok = true;
    result.bundleVersion = remoteVersion;
    result.fileCount = count;
    result.message = QStringLiteral("测试用例同步成功，已更新 %1 项").arg(count);
    if (!remoteVersion.isEmpty()) {
        result.message += QStringLiteral("（bundle=%1）").arg(remoteVersion);
    }
    return result;
}
