#include "test_case_sync_service.h"

#include "test_case.h"

#include "auth_service.h"
#include "factory_cloud_client.h"
#include "test_case_policy_service.h"
#include "test_data_upload_service.h"
#include "qlog.h"

#include "my_set/my_typedef.h"

#include <QAtomicInt>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QDateTime>
#include <QThreadPool>
#include <QMetaObject>
#include <QSet>
#include <QSettings>
#include <QVariantMap>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kBackupDirName = ".backup";
constexpr int kHeartbeatIntervalMs = 20000;
/** 设备命令轮询（网页拉配置等） */
constexpr int kCommandPollIntervalMs = 3000;

QAtomicInt g_heartbeatBusy(0);
QAtomicInt g_commandPollBusy(0);
QAtomicInt g_agentStopped(0);
QTimer* g_hbTimer = nullptr;
QTimer* g_cmdTimer = nullptr;

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

/** 本地 Profile 与云端清单内容一致才视为已最新（不用 ProfileVersion 数字相等判定） */
bool localProfileMatchesRemoteFiles(const QString& profileDir, const QJsonArray& remoteFiles) {
    if (!QDir(profileDir).exists() || remoteFiles.isEmpty()) {
        return false;
    }

    QSet<QString> remoteRels;
    for (const QJsonValue& value : remoteFiles) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject item = value.toObject();
        const QString rel = item.value(QStringLiteral("path")).toString().trimmed().replace(QLatin1Char('\\'),
                                                                                            QLatin1Char('/'));
        const QString expected = item.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        if (rel.isEmpty() || expected.isEmpty()) {
            continue;
        }
        remoteRels.insert(rel);
        const QString abs = QDir(profileDir).filePath(rel);
        if (!QFile::exists(abs)) {
            return false;
        }
        if (fileSha256Hex(abs).toLower() != expected) {
            return false;
        }
    }
    if (remoteRels.isEmpty()) {
        return false;
    }

    // 本地多出的 .ini 也算不一致，避免漏改文件却因版本号相同而跳过下载
    QDirIterator it(profileDir, QStringList{QStringLiteral("*.ini")}, QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString rel = QDir(profileDir).relativeFilePath(it.filePath()).replace(QLatin1Char('\\'),
                                                                                     QLatin1Char('/'));
        if (!remoteRels.contains(rel)) {
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

QString compressDirectoryContents(const QString& root, const QString& zipNamePrefix, QString* error) {
    const QDir srcDir(root);
    if (!srcDir.exists()) {
        if (error) {
            *error = QStringLiteral("目录不存在：") + root;
        }
        return {};
    }
    const QFileInfoList entries = srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    if (entries.isEmpty()) {
        if (error) {
            *error = QStringLiteral("目录为空：") + root;
        }
        return {};
    }

    const QString zipPath = QDir(QDir::tempPath())
                                .filePath(QStringLiteral("%1_%2.zip")
                                              .arg(zipNamePrefix, QDateTime::currentDateTime().toString(
                                                                      QStringLiteral("yyyyMMddhhmmss"))));
    if (QFile::exists(zipPath)) {
        QFile::remove(zipPath);
    }
    const QString script =
        QStringLiteral("$root='%1'; $zip='%2'; $items=@(Get-ChildItem -LiteralPath $root); "
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

int readProfileVersion(const QString& profileIniPath) {
    if (!QFile::exists(profileIniPath)) {
        return 1;
    }
    QSettings meta(profileIniPath, QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    return qMax(1, meta.value(QStringLiteral("Profile/ProfileVersion"), 1).toInt());
}

int bumpProfileVersion(const QString& profileIniPath) {
    QSettings meta(profileIniPath, QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    const int next = qMax(1, meta.value(QStringLiteral("Profile/ProfileVersion"), 1).toInt()) + 1;
    meta.setValue(QStringLiteral("Profile/ProfileVersion"), next);
    meta.sync();
    return next;
}

static void writeProfileVersion(const QString& profileIniPath, int version) {
    QSettings meta(profileIniPath, QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    meta.setValue(QStringLiteral("Profile/ProfileVersion"), qMax(1, version));
    meta.sync();
}

bool ensureCloudReady(QString* message) {
    if (AuthService::isOfflineSession()) {
        if (message) {
            *message = QStringLiteral("离线测试模式，无法访问云端测试用例");
        }
        return false;
    }
    if (!AuthService::isLoggedIn()) {
        const AuthService::LoginResult login = AuthService::loginWithSavedCredentials();
        if (!login.ok) {
            if (message) {
                *message = login.message;
            }
            return false;
        }
    }
    return true;
}

/** 上传/下载前：向云端申请正式编号，若与本地不同则自动修正 catalog 与 profile.ini。 */
static QString alignLocalStationKeyWithCloud(const QString& displayName, const QString& localKey,
                                             QString* detailOut);

static void applyCloudCanonicalStationKey(const QString& displayName, const QString& canonicalKey,
                                          QString& keyInOut, QString* detailOut) {
    const QString canonical = canonicalKey.trimmed();
    if (canonical.isEmpty() || canonical.compare(keyInOut, Qt::CaseInsensitive) == 0)
        return;
    const QString oldKey = keyInOut;
    QString alignErr;
    if (!TestCaseStore::alignFlowStationKey(displayName, canonical, &alignErr))
        return;
    keyInOut = canonical;
    if (detailOut) {
        const QString extra = alignErr.isEmpty()
                                  ? QStringLiteral("本地编号已由 %1 对齐为 %2").arg(oldKey, canonical)
                                  : alignErr;
        if (detailOut->isEmpty())
            *detailOut = extra;
        else
            *detailOut += QStringLiteral("；") + extra;
    }
}

} // namespace

QString TestCaseSyncService::testCaseRoot() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("test_case"));
}

void TestCaseSyncService::tryStartupSyncAsync() {
    QtConcurrent::run([]() {
        const SyncResult result = syncFromCloud();
        if (!result.message.isEmpty()) {
            qDebug() << QStringLiteral("[TestCaseSync]") << result.message;
        }
    });
}

TestCaseSyncService::SyncResult TestCaseSyncService::uploadStationProfile(const QString& stationKey,
                                                                            const QString& displayName,
                                                                            const QString& source,
                                                                            const QString& remark) {
    SyncResult result;
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        result.message = readyError;
        return result;
    }

    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty()) {
        result.message = QStringLiteral("未选择工站，无法上传用例");
        return result;
    }
    QString name = displayName.trimmed();
    if (name.isEmpty()) {
        name = TestCaseStore::flowStationDisplayName(key);
    }
    if (name.isEmpty()) {
        name = key;
    }

    QString alignDetail;
    QString uploadKey = alignLocalStationKeyWithCloud(name, key, &alignDetail);

    const QString trimmedRemark = remark.trimmed();
    const bool isPull = source.compare(QStringLiteral("pull"), Qt::CaseInsensitive) == 0;
    if (!isPull && trimmedRemark.isEmpty()) {
        result.message = QStringLiteral("请填写上传说明");
        return result;
    }
    if (trimmedRemark.size() > 500) {
        result.message = QStringLiteral("上传说明最多 500 字");
        return result;
    }

    const QString profileDir = TestCasePaths::profileDir(uploadKey);
    if (!QDir(profileDir).exists()) {
        result.message = QStringLiteral("工站 Profile 目录不存在：") + profileDir;
        return result;
    }

    const QString profileIni = TestCasePaths::profileMetaPath(uploadKey);
    // 主动上传：成功后再保留草稿号 +1；失败回滚，不得改 ProfileVersion
    const int previousVersion = readProfileVersion(profileIni);
    const int profileVersion = isPull ? previousVersion : bumpProfileVersion(profileIni);

    // 上传 zip 不含本机 [Policy]（豁免码/脏标记等），打包后再写回
    const QVariantMap detachedPolicy = TestCasePolicyService::detachPolicyForUpload(uploadKey);
    QString zipError;
    const QString zipPath =
        compressDirectoryContents(profileDir, QStringLiteral("profile_%1").arg(uploadKey), &zipError);
    TestCasePolicyService::reattachPolicyAfterUpload(uploadKey, detachedPolicy);
    if (zipPath.isEmpty()) {
        if (!isPull)
            writeProfileVersion(profileIni, previousVersion);
        result.message = zipError.isEmpty() ? QStringLiteral("打包工站 Profile 失败") : zipError;
        return result;
    }

    QList<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("deviceId"), FactoryCloudClient::deviceId()});
    fields.append({QStringLiteral("hostName"), QSysInfo::machineHostName()});
    fields.append({QStringLiteral("displayName"), name});
    fields.append({QStringLiteral("profileVersion"), QString::number(profileVersion)});
    fields.append({QStringLiteral("source"), source.isEmpty() ? QStringLiteral("upload") : source});
    fields.append({QStringLiteral("remark"),
                   trimmedRemark.isEmpty() ? QStringLiteral("网页拉取回传") : trimmedRemark});

    const QString path = QStringLiteral("/test-cases/profiles/%1/upload")
                             .arg(QString::fromUtf8(QUrl::toPercentEncoding(uploadKey)));
    const FactoryCloudClient::ApiResult api = FactoryCloudClient::uploadMultipart(path, zipPath, fields);
    QFile::remove(zipPath);

    if (!api.ok) {
        if (!isPull)
            writeProfileVersion(profileIni, previousVersion);
        result.message = api.message.isEmpty() ? QStringLiteral("上传失败") : api.message;
        return result;
    }

    applyCloudCanonicalStationKey(name, api.data.value(QStringLiteral("canonicalStationKey")).toString(),
                                  uploadKey, &alignDetail);

    const QString remoteProfileVersion = api.data.value(QStringLiteral("profileVersion")).toString().trimmed();
    const int fileCount = api.data.value(QStringLiteral("fileCount")).toInt(0);

    result.ok = true;
    result.stationKey = uploadKey;
    result.profileVersion = remoteProfileVersion.isEmpty() ? QString::number(profileVersion) : remoteProfileVersion;
    result.fileCount = fileCount;
    result.message = QStringLiteral("工站「%1」已上传为云端草稿").arg(name);
    if (fileCount > 0) {
        result.message += QStringLiteral("，共 %1 个文件").arg(fileCount);
    }
    const QString remoteRemark = api.data.value(QStringLiteral("remark")).toString().trimmed();
    if (!remoteRemark.isEmpty()) {
        result.message += QStringLiteral("；说明：%1").arg(remoteRemark);
    }
    if (!alignDetail.isEmpty()) {
        result.message += QStringLiteral("；%1").arg(alignDetail);
    }
    result.message +=
        QStringLiteral("（本地草稿号 %1；网页合入后会由服务器分配正式 ProfileVersion）").arg(result.profileVersion);
    TestCasePolicyService::markPendingAfterUpload(uploadKey);
    return result;
}

TestCaseSyncService::SyncResult TestCaseSyncService::uploadToCloud(const QString& remark) {
    const QString key = TestCaseStore::loadSelectedFlowStationKey();
    const QString name = TestCaseStore::loadSelectedFlowStationName();
    return uploadStationProfile(key, name, QStringLiteral("upload"), remark);
}

/** 下载前把工站登记进本地 FlowStations；同显示名只保留云端 StationKey。 */
static void ensureLocalFlowStationCatalog(const QString& stationKey, const QString& displayName) {
    const QString key = stationKey.trimmed();
    const QString name = displayName.trimmed().isEmpty() ? key : displayName.trimmed();
    if (key.isEmpty() || name.isEmpty()) {
        return;
    }

    QVector<TestFlowStationEntry> catalog = TestCaseStore::loadFlowStationCatalog();
    QVector<TestFlowStationEntry> next;
    next.reserve(catalog.size() + 1);
    bool found = false;
    bool changed = false;
    for (const TestFlowStationEntry& entry : catalog) {
        const bool sameKey = entry.key.compare(key, Qt::CaseInsensitive) == 0;
        const bool sameName = entry.displayName == name;
        if (sameName || sameKey) {
            if (!found) {
                next.append({key, name});
                found = true;
                if (!sameKey || entry.displayName != name)
                    changed = true;
            } else {
                // 丢掉同名/同 key 的重复行
                changed = true;
            }
            continue;
        }
        next.append(entry);
    }
    if (!found) {
        next.append({key, name});
        changed = true;
    }
    if (changed) {
        TestCaseStore::saveFlowStationCatalog(next);
    }
}

TestCaseSyncService::SyncResult TestCaseSyncService::allocateStationKeyFromCloud(const QString& displayName,
                                                                                  const QString& preferredKey) {
    SyncResult result;
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        result.message = readyError;
        return result;
    }

    const QString name = displayName.trimmed();
    if (name.isEmpty()) {
        result.message = QStringLiteral("工站名称不能为空");
        return result;
    }

    QJsonObject body;
    body.insert(QStringLiteral("displayName"), name);
    const QString preferred = preferredKey.trimmed();
    if (!preferred.isEmpty())
        body.insert(QStringLiteral("preferredKey"), preferred);

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::post(QStringLiteral("/test-cases/profiles/allocate-station-key"), body);
    if (!api.ok) {
        result.message = api.message.isEmpty() ? QStringLiteral("向云端申请工站编号失败") : api.message;
        return result;
    }

    const QString key = api.data.value(QStringLiteral("stationKey")).toString().trimmed();
    if (key.isEmpty()) {
        result.message = QStringLiteral("云端未返回 stationKey");
        return result;
    }

    result.ok = true;
    result.stationKey = key;
    const QString remoteName = api.data.value(QStringLiteral("displayName")).toString().trimmed();
    result.message = QStringLiteral("云端已分配工站编号 %1").arg(key);
    if (!remoteName.isEmpty() && remoteName != name)
        result.message += QStringLiteral("（显示名=%1）").arg(remoteName);
    return result;
}

TestCaseSyncService::ProfileListResult TestCaseSyncService::listPublishedProfiles() {
    ProfileListResult result;
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        result.message = readyError;
        return result;
    }

    const FactoryCloudClient::ApiResult api = FactoryCloudClient::get(QStringLiteral("/test-cases/profiles"));
    if (!api.ok) {
        result.message = api.message.isEmpty() ? QStringLiteral("获取云端工站列表失败") : api.message;
        return result;
    }

    result.bundleVersion = api.data.value(QStringLiteral("bundleVersion")).toString().trimmed();
    const QJsonArray items = api.data.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject obj = value.toObject();
        PublishedProfile item;
        item.stationKey = obj.value(QStringLiteral("stationKey")).toString().trimmed();
        item.displayName = obj.value(QStringLiteral("displayName")).toString().trimmed();
        item.profileVersion = obj.value(QStringLiteral("profileVersion")).toString().trimmed();
        if (item.stationKey.isEmpty()) {
            continue;
        }
        if (item.displayName.isEmpty()) {
            item.displayName = item.stationKey;
        }
        result.items.append(item);
    }
    result.ok = true;
    if (result.items.isEmpty()) {
        result.message = QStringLiteral("云端尚无已发布的工站用例");
    }
    return result;
}

namespace {

static bool tryAlignLocalKey(const QString& displayName, const QString& targetKey, const QString& oldKey,
                             QString* uploadKeyInOut, QString* detailOut, const QString& okMessage) {
    const QString target = targetKey.trimmed();
    if (target.isEmpty() || !uploadKeyInOut)
        return false;
    if (target.compare(uploadKeyInOut->trimmed(), Qt::CaseInsensitive) == 0)
        return false;
    QString alignErr;
    if (!TestCaseStore::alignFlowStationKey(displayName, target, &alignErr))
        return false;
    if (detailOut && !okMessage.isEmpty())
        *detailOut = okMessage.arg(oldKey, target);
    *uploadKeyInOut = target;
    return true;
}

QString alignLocalStationKeyWithCloud(const QString& displayName, const QString& localKey, QString* detailOut) {
    if (detailOut)
        detailOut->clear();
    QString uploadKey = localKey.trimmed();
    const QString oldKey = uploadKey;
    QStringList notes;

    const TestCaseSyncService::SyncResult alloc =
        TestCaseSyncService::allocateStationKeyFromCloud(displayName, localKey);
    if (alloc.ok && !alloc.stationKey.isEmpty()) {
        QString msg;
        if (tryAlignLocalKey(displayName, alloc.stationKey, oldKey, &uploadKey, &msg,
                             QStringLiteral("本地编号已由 %1 对齐为 %2"))) {
            notes.append(msg);
        } else if (alloc.stationKey.compare(uploadKey, Qt::CaseInsensitive) == 0) {
            notes.append(QStringLiteral("云端确认编号 %1").arg(uploadKey));
        }
    } else if (!alloc.message.isEmpty()) {
        notes.append(alloc.message);
    }

    const TestCaseSyncService::ProfileListResult published = TestCaseSyncService::listPublishedProfiles();
    if (published.ok) {
        for (const TestCaseSyncService::PublishedProfile& item : published.items) {
            if (item.displayName == displayName && !item.stationKey.isEmpty()
                && item.stationKey.compare(uploadKey, Qt::CaseInsensitive) != 0) {
                QString msg;
                if (tryAlignLocalKey(displayName, item.stationKey, oldKey, &uploadKey, &msg,
                                     QStringLiteral("本地编号已由 %1 对齐为云端已发布编号 %2"))) {
                    notes.append(msg);
                }
                break;
            }
        }
        for (const TestCaseSyncService::PublishedProfile& item : published.items) {
            if (item.stationKey.compare(uploadKey, Qt::CaseInsensitive) != 0)
                continue;
            if (item.displayName == displayName)
                continue;
            const TestCaseSyncService::SyncResult reAlloc =
                TestCaseSyncService::allocateStationKeyFromCloud(displayName, QString());
            if (reAlloc.ok && !reAlloc.stationKey.isEmpty()
                && reAlloc.stationKey.compare(uploadKey, Qt::CaseInsensitive) != 0) {
                QString alignErr;
                if (TestCaseStore::alignFlowStationKey(displayName, reAlloc.stationKey, &alignErr)) {
                    notes.append(QStringLiteral("编号 %1 与云端工站「%2」冲突，已对齐为 %3")
                                     .arg(oldKey, item.displayName, reAlloc.stationKey));
                    uploadKey = reAlloc.stationKey;
                } else if (!alignErr.isEmpty()) {
                    notes.append(alignErr);
                }
            } else {
                notes.append(QStringLiteral("编号 %1 与云端工站「%2」冲突，请部署 allocate-station-key 或网页合入后下载")
                                 .arg(uploadKey, item.displayName));
            }
            break;
        }
    }

    if (detailOut && !notes.isEmpty())
        *detailOut = notes.join(QStringLiteral("；"));
    return uploadKey;
}

} // namespace

TestCaseSyncService::SyncResult TestCaseSyncService::syncStationFromCloud(const QString& stationKey,
                                                                           const QString& displayName) {
    SyncResult result;
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        result.message = readyError;
        return result;
    }

    QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    QString name = displayName.trimmed();
    if (name.isEmpty() && !key.isEmpty()) {
        name = TestCaseStore::flowStationDisplayName(key);
    }
    if (key.isEmpty() && name.isEmpty()) {
        result.message = QStringLiteral("未指定工站，无法下载用例");
        return result;
    }
    if (key.isEmpty()) {
        key = name;
    }
    if (name.isEmpty()) {
        name = key;
    }

    const QString encodedKey = QString::fromUtf8(QUrl::toPercentEncoding(key));
    QUrlQuery profileQuery;
    if (!name.isEmpty()) {
        profileQuery.addQueryItem(QStringLiteral("displayName"), name);
    }
    const FactoryCloudClient::ApiResult manifest = FactoryCloudClient::get(
        QStringLiteral("/test-cases/profiles/%1/manifest").arg(encodedKey), profileQuery);
    if (!manifest.ok) {
        result.message = manifest.message.isEmpty()
                             ? QStringLiteral("云端尚无该工站已发布用例，请先在网页合入并发布")
                             : manifest.message;
        return result;
    }

    const QString remoteProfileVersion =
        manifest.data.value(QStringLiteral("profileVersion")).toString().trimmed();
    const QString remoteDisplayName =
        manifest.data.value(QStringLiteral("displayName")).toString().trimmed();
    const QString remoteStationKey =
        manifest.data.value(QStringLiteral("stationKey")).toString().trimmed();
    if (!remoteDisplayName.isEmpty()) {
        name = remoteDisplayName;
    }
    // 以云端正式 StationKey 为准对齐本地目录表（同显示名统一编号）
    if (!remoteStationKey.isEmpty()) {
        key = remoteStationKey;
    }
    QString alignDetail;
    applyCloudCanonicalStationKey(name, key, key, &alignDetail);
    ensureLocalFlowStationCatalog(key, name);

    // 是否最新：比文件 sha256，不比各电脑可能撞车的 ProfileVersion 数字
    const QJsonArray remoteFiles = manifest.data.value(QStringLiteral("files")).toArray();
    if (localProfileMatchesRemoteFiles(TestCasePaths::profileDir(key), remoteFiles)) {
        result.ok = true;
        result.stationKey = key;
        result.profileVersion = remoteProfileVersion;
        result.bundleVersion = manifest.data.value(QStringLiteral("bundleVersion")).toString();
        result.message =
            QStringLiteral("工站「%1」用例已是最新（profile=%2）")
                .arg(name, remoteProfileVersion.isEmpty() ? QStringLiteral("content") : remoteProfileVersion);
        if (!alignDetail.isEmpty())
            result.message += QStringLiteral("；%1").arg(alignDetail);
        return result;
    }

    const QString zipPath = QDir(QDir::tempPath())
                                .filePath(QStringLiteral("profile_download_%1.zip")
                                              .arg(QDateTime::currentDateTime().toString(
                                                  QStringLiteral("yyyyMMddhhmmss"))));
    if (QFile::exists(zipPath)) {
        QFile::remove(zipPath);
    }

    QString downloadError;
    if (!FactoryCloudClient::downloadToFile(
            QStringLiteral("/test-cases/profiles/%1/bundle").arg(encodedKey), profileQuery, zipPath,
            &downloadError)) {
        result.message = downloadError;
        return result;
    }

    const QString extractRoot =
        QDir(QDir::tempPath()).filePath(QStringLiteral("profile_extract_%1").arg(key));
    QDir(extractRoot).removeRecursively();
    QString extractError;
    if (!extractZip(zipPath, extractRoot, &extractError)) {
        result.message = QStringLiteral("解压工站用例失败：") + extractError;
        QFile::remove(zipPath);
        return result;
    }
    QFile::remove(zipPath);

    // 仅替换该工站 profiles/{显示名}/，不动其它工站与共享 steps 库
    const QString destProfileDir = TestCasePaths::profileDir(key);
    QString backupError;
    if (QDir(destProfileDir).exists()) {
        const QString backupDir =
            QDir(testCaseRoot()).filePath(QStringLiteral("%1/profile_%2_%3")
                                              .arg(QLatin1String(kBackupDirName), key,
                                                   QDateTime::currentDateTime().toString(
                                                       QStringLiteral("yyyyMMddhhmmss"))));
        QDir().mkpath(QFileInfo(backupDir).absolutePath());
        if (!copyPath(destProfileDir, backupDir, &backupError)) {
            result.message = QStringLiteral("备份工站用例失败：") + backupError;
            QDir(extractRoot).removeRecursively();
            return result;
        }
        QDir(destProfileDir).removeRecursively();
    }

    QString deployError;
    int count = 0;
    if (!copyPath(extractRoot, destProfileDir, &deployError)) {
        result.message = deployError.isEmpty() ? QStringLiteral("部署工站用例失败") : deployError;
        QDir(extractRoot).removeRecursively();
        return result;
    }
    const QFileInfoList deployed =
        QDir(destProfileDir).entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    count = deployed.size();
    QDir(extractRoot).removeRecursively();

    const QString bundleVersion = manifest.data.value(QStringLiteral("bundleVersion")).toString().trimmed();
    if (!bundleVersion.isEmpty()) {
        SETTINGS.setValue(QStringLiteral("FactoryCloud/TestCaseBundleVersion"), bundleVersion);
        SETTINGS.sync();
    }
    TestCaseStore::invalidateCloudItemNameCache();
    // 覆盖 profiles 后按目录补登记 FlowStations，避免仅内存/启动一次迁移导致下拉不出现新工站
    TestCaseStore::reregisterFlowStationsFromProfiles();

    result.ok = true;
    result.stationKey = key;
    result.profileVersion = remoteProfileVersion;
    result.bundleVersion = bundleVersion;
    result.fileCount = count;
    result.message = QStringLiteral("已下载工站「%1」正式用例").arg(name);
    if (!remoteProfileVersion.isEmpty()) {
        result.message += QStringLiteral("（profile=%1）").arg(remoteProfileVersion);
    }
    if (!alignDetail.isEmpty()) {
        result.message += QStringLiteral("；%1").arg(alignDetail);
    }
    TestCasePolicyService::markSyncedClean(
        key, manifest.data.value(QStringLiteral("contentFingerprint")).toString(), bundleVersion);
    return result;
}

TestCaseSyncService::SyncResult TestCaseSyncService::syncFromCloud() {
    const QString key = TestCaseStore::loadSelectedFlowStationKey();
    const QString name = TestCaseStore::loadSelectedFlowStationName();
    return syncStationFromCloud(key, name);
}

TestCaseSyncService::SyncResult TestCaseSyncService::uploadStepsLibrary(const QString& remark) {
    SyncResult result;
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        result.message = readyError;
        return result;
    }

    const QString trimmedRemark = remark.trimmed();
    if (trimmedRemark.isEmpty()) {
        result.message = QStringLiteral("请填写上传说明");
        return result;
    }
    if (trimmedRemark.size() > 500) {
        result.message = QStringLiteral("上传说明最多 500 字");
        return result;
    }

    TestCasePaths::ensureRootDir();
    const QString stepsDir = TestCasePaths::stepsDir();
    if (!QDir(stepsDir).exists()) {
        result.message = QStringLiteral("本地用例库目录不存在：") + stepsDir;
        return result;
    }
    const QFileInfoList iniFiles =
        QDir(stepsDir).entryInfoList({QStringLiteral("*.ini")}, QDir::Files | QDir::NoDotAndDotDot);
    if (iniFiles.isEmpty()) {
        result.message = QStringLiteral("本地用例库为空，无可上传的 ini");
        return result;
    }

    // 主动上传时本地库版本 +1（草稿痕迹）；正式号在网页合入时由服务器重写
    const QString libraryIni = QDir(stepsDir).filePath(QStringLiteral("library.ini"));
    QSettings meta(libraryIni, QSettings::IniFormat);
    meta.setIniCodec("UTF-8");
    const int libraryVersion = qMax(1, meta.value(QStringLiteral("Library/LibraryVersion"), 1).toInt()) + 1;
    meta.setValue(QStringLiteral("Library/LibraryVersion"), libraryVersion);
    meta.sync();

    QString zipError;
    const QString zipPath =
        compressDirectoryContents(stepsDir, QStringLiteral("steps_library"), &zipError);
    if (zipPath.isEmpty()) {
        result.message = zipError.isEmpty() ? QStringLiteral("打包用例库失败") : zipError;
        return result;
    }

    QList<QPair<QString, QString>> fields;
    fields.append({QStringLiteral("deviceId"), FactoryCloudClient::deviceId()});
    fields.append({QStringLiteral("hostName"), QSysInfo::machineHostName()});
    fields.append({QStringLiteral("libraryVersion"), QString::number(libraryVersion)});
    fields.append({QStringLiteral("source"), QStringLiteral("upload")});
    fields.append({QStringLiteral("remark"), trimmedRemark});

    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::uploadMultipart(QStringLiteral("/test-cases/steps/upload"), zipPath, fields);
    QFile::remove(zipPath);

    if (!api.ok) {
        result.message = api.message.isEmpty() ? QStringLiteral("上传用例库失败") : api.message;
        return result;
    }

    const int fileCount = api.data.value(QStringLiteral("fileCount")).toInt(0);
    result.ok = true;
    result.stationKey = QStringLiteral("__STEPS__");
    result.profileVersion = api.data.value(QStringLiteral("libraryVersion")).toString().trimmed();
    if (result.profileVersion.isEmpty()) {
        result.profileVersion = QString::number(libraryVersion);
    }
    result.fileCount = fileCount;
    result.message = QStringLiteral("用例库已上传为云端草稿");
    if (fileCount > 0) {
        result.message += QStringLiteral("，共 %1 个文件").arg(fileCount);
    }
    result.message +=
        QStringLiteral("（本地草稿号 %1；网页合入后会由服务器分配正式 LibraryVersion）")
            .arg(result.profileVersion);
    return result;
}

TestCaseSyncService::SyncResult TestCaseSyncService::syncStepsLibraryFromCloud() {
    SyncResult result;
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        result.message = readyError;
        return result;
    }

    const FactoryCloudClient::ApiResult manifest =
        FactoryCloudClient::get(QStringLiteral("/test-cases/steps/manifest"));
    if (!manifest.ok) {
        result.message = manifest.message.isEmpty()
                             ? QStringLiteral("云端尚无已发布用例库，请先在网页合入并发布")
                             : manifest.message;
        return result;
    }

    const QString remoteLibraryVersion =
        manifest.data.value(QStringLiteral("libraryVersion")).toString().trimmed();
    const QJsonArray remoteFiles = manifest.data.value(QStringLiteral("files")).toArray();
    TestCasePaths::ensureRootDir();
    const QString stepsDir = TestCasePaths::stepsDir();
    if (localProfileMatchesRemoteFiles(stepsDir, remoteFiles)) {
        result.ok = true;
        result.stationKey = QStringLiteral("__STEPS__");
        result.profileVersion = remoteLibraryVersion;
        result.bundleVersion = manifest.data.value(QStringLiteral("bundleVersion")).toString();
        result.message =
            QStringLiteral("用例库已是最新（library=%1）")
                .arg(remoteLibraryVersion.isEmpty() ? QStringLiteral("content") : remoteLibraryVersion);
        return result;
    }

    const QString zipPath = QDir(QDir::tempPath())
                                .filePath(QStringLiteral("steps_download_%1.zip")
                                              .arg(QDateTime::currentDateTime().toString(
                                                  QStringLiteral("yyyyMMddhhmmss"))));
    if (QFile::exists(zipPath)) {
        QFile::remove(zipPath);
    }

    QString downloadError;
    if (!FactoryCloudClient::downloadToFile(QStringLiteral("/test-cases/steps/bundle"), QUrlQuery(), zipPath,
                                            &downloadError)) {
        result.message = downloadError;
        return result;
    }

    const QString extractRoot =
        QDir(QDir::tempPath()).filePath(QStringLiteral("steps_extract_%1")
                                            .arg(QDateTime::currentDateTime().toString(
                                                QStringLiteral("yyyyMMddhhmmss"))));
    QDir(extractRoot).removeRecursively();
    QString extractError;
    if (!extractZip(zipPath, extractRoot, &extractError)) {
        result.message = QStringLiteral("解压用例库失败：") + extractError;
        QFile::remove(zipPath);
        return result;
    }
    QFile::remove(zipPath);

    // 仅替换共享 steps/，不动各工站 profiles
    QString backupError;
    if (QDir(stepsDir).exists()) {
        const QString backupDir =
            QDir(testCaseRoot()).filePath(QStringLiteral("%1/steps_%2")
                                              .arg(QLatin1String(kBackupDirName),
                                                   QDateTime::currentDateTime().toString(
                                                       QStringLiteral("yyyyMMddhhmmss"))));
        QDir().mkpath(QFileInfo(backupDir).absolutePath());
        if (!copyPath(stepsDir, backupDir, &backupError)) {
            result.message = QStringLiteral("备份用例库失败：") + backupError;
            QDir(extractRoot).removeRecursively();
            return result;
        }
        // 只清根目录 ini，保留目录本身
        const QFileInfoList oldInis =
            QDir(stepsDir).entryInfoList({QStringLiteral("*.ini")}, QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : oldInis) {
            QFile::remove(fi.absoluteFilePath());
        }
    } else {
        QDir().mkpath(stepsDir);
    }

    QString deployError;
    int count = 0;
    if (!deployExtractedFiles(extractRoot, stepsDir, &count, &deployError)) {
        result.message = deployError.isEmpty() ? QStringLiteral("部署用例库失败") : deployError;
        QDir(extractRoot).removeRecursively();
        return result;
    }
    QDir(extractRoot).removeRecursively();

    TestCaseStore::invalidateCloudItemNameCache();

    result.ok = true;
    result.stationKey = QStringLiteral("__STEPS__");
    result.profileVersion = remoteLibraryVersion;
    result.bundleVersion = manifest.data.value(QStringLiteral("bundleVersion")).toString();
    result.fileCount = count;
    result.message = QStringLiteral("已下载云端正式用例库");
    if (!remoteLibraryVersion.isEmpty()) {
        result.message += QStringLiteral("（library=%1）").arg(remoteLibraryVersion);
    }
    if (count > 0) {
        result.message += QStringLiteral("，共 %1 项").arg(count);
    }
    return result;
}

void TestCaseSyncService::heartbeatAndPollCommands() {
    if (g_agentStopped.loadRelaxed()) {
        return;
    }
    if (!g_heartbeatBusy.testAndSetRelaxed(0, 1)) {
        Qlog::saveResidentLog(QStringLiteral("heartbeat"),
                              QStringLiteral("跳过：上一次心跳仍在执行（可能堆积占用）"));
        return;
    }

    QElapsedTimer timer;
    timer.start();
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        Qlog::saveResidentLog(QStringLiteral("heartbeat"),
                              QStringLiteral("未就绪 cost=%1ms err=%2").arg(timer.elapsed()).arg(readyError));
        g_heartbeatBusy.storeRelaxed(0);
        return;
    }

    QJsonObject body;
    body.insert(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    body.insert(QStringLiteral("hostName"), QSysInfo::machineHostName());
    body.insert(QStringLiteral("stationKey"), TestCaseStore::loadSelectedFlowStationKey());
    body.insert(QStringLiteral("stationName"), TestCaseStore::loadSelectedFlowStationName());
    body.insert(QStringLiteral("appVersion"), FactoryCloudClient::appVersion());
    // 上报本机工站目录，供网页「从产线拉取」下拉选择
    QJsonArray stations;
    for (const TestFlowStationEntry& entry : TestCaseStore::loadFlowStationCatalog()) {
        if (entry.key.trimmed().isEmpty() && entry.displayName.trimmed().isEmpty()) {
            continue;
        }
        QJsonObject one;
        one.insert(QStringLiteral("stationKey"), entry.key.trimmed());
        one.insert(QStringLiteral("displayName"),
                   entry.displayName.trimmed().isEmpty() ? entry.key.trimmed() : entry.displayName.trimmed());
        stations.append(one);
    }
    body.insert(QStringLiteral("stations"), stations);

    QElapsedTimer httpTimer;
    httpTimer.start();
    const FactoryCloudClient::ApiResult hb = FactoryCloudClient::post(QStringLiteral("/device/heartbeat"), body);
    const qint64 httpMs = httpTimer.elapsed();
    if (!hb.ok) {
        Qlog::saveResidentLog(QStringLiteral("heartbeat"),
                              QStringLiteral("失败 http=%1ms total=%2ms err=%3")
                                  .arg(httpMs)
                                  .arg(timer.elapsed())
                                  .arg(hb.message));
        g_heartbeatBusy.storeRelaxed(0);
        return;
    }

    // 心跳通畅说明网络可用，顺带补传断网期间积压的测试数据
    QElapsedTimer flushTimer;
    flushTimer.start();
    TestDataUploadService::flushPendingUploads();
    const qint64 flushMs = flushTimer.elapsed();

    pollDeviceCommands();
    Qlog::saveResidentLog(QStringLiteral("heartbeat"),
                          QStringLiteral("成功 http=%1ms flush=%2ms total=%3ms stations=%4")
                              .arg(httpMs)
                              .arg(flushMs)
                              .arg(timer.elapsed())
                              .arg(stations.size()));
    g_heartbeatBusy.storeRelaxed(0);
}

void TestCaseSyncService::pollDeviceCommands() {
    if (g_agentStopped.loadRelaxed()) {
        return;
    }
    if (!g_commandPollBusy.testAndSetRelaxed(0, 1)) {
        Qlog::saveResidentLog(QStringLiteral("cmdPoll"),
                              QStringLiteral("跳过：上一次命令轮询仍在执行（可能堆积占用）"));
        return;
    }

    QElapsedTimer timer;
    timer.start();
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        Qlog::saveResidentLog(QStringLiteral("cmdPoll"),
                              QStringLiteral("问云端命令前未就绪 耗时=%1ms 原因=%2")
                                  .arg(timer.elapsed())
                                  .arg(readyError));
        g_commandPollBusy.storeRelaxed(0);
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    QElapsedTimer httpTimer;
    httpTimer.start();
    const FactoryCloudClient::ApiResult cmdRes =
        FactoryCloudClient::get(QStringLiteral("/device/commands"), query);
    const qint64 httpMs = httpTimer.elapsed();
    if (!cmdRes.ok) {
        Qlog::saveResidentLog(QStringLiteral("cmdPoll"),
                              QStringLiteral("问云端有无下发命令失败 请求=%1ms 总耗时=%2ms 原因=%3")
                                  .arg(httpMs)
                                  .arg(timer.elapsed())
                                  .arg(cmdRes.message));
        g_commandPollBusy.storeRelaxed(0);
        return;
    }

    const QJsonArray items = cmdRes.data.value(QStringLiteral("items")).toArray();
    int handled = 0;
    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject cmd = value.toObject();
        const QString type = cmd.value(QStringLiteral("type")).toString().trimmed();
        const QJsonObject payload = cmd.value(QStringLiteral("payload")).toObject();
        if (type != QLatin1String("pull_test_profile")) {
            continue;
        }
        QString stationKey = payload.value(QStringLiteral("stationKey")).toString().trimmed();
        QString displayName = payload.value(QStringLiteral("displayName")).toString().trimmed();
        if (stationKey.isEmpty()) {
            stationKey = TestCaseStore::loadSelectedFlowStationKey();
            displayName = TestCaseStore::loadSelectedFlowStationName();
        }
        const SyncResult uploadResult = uploadStationProfile(stationKey, displayName, QStringLiteral("pull"));
        Qlog::saveResidentLog(QStringLiteral("cmdPoll"),
                              QStringLiteral("网页拉取配置回传 工站=%1 成功=%2 说明=%3")
                                  .arg(stationKey)
                                  .arg(uploadResult.ok ? QStringLiteral("是") : QStringLiteral("否"),
                                       uploadResult.message));
        ++handled;
    }

    // 空轮询也记耗时，便于对照卡顿时段占用
    Qlog::saveResidentLog(QStringLiteral("cmdPoll"),
                          QStringLiteral("问云端有无下发命令 请求=%1ms 总耗时=%2ms 云端命令数=%3 本机处理数=%4")
                              .arg(httpMs)
                              .arg(timer.elapsed())
                              .arg(items.size())
                              .arg(handled));
    g_commandPollBusy.storeRelaxed(0);
}

void TestCaseSyncService::startDeviceAgent() {
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    Qlog::saveResidentLog(QStringLiteral("agent"),
                          QStringLiteral("启动常驻：heartbeat=%1ms cmdPoll=%2ms")
                              .arg(kHeartbeatIntervalMs)
                              .arg(kCommandPollIntervalMs));

    g_hbTimer = new QTimer(qApp);
    g_hbTimer->setInterval(kHeartbeatIntervalMs);
    QObject::connect(g_hbTimer, &QTimer::timeout, qApp, []() {
        QtConcurrent::run([]() { heartbeatAndPollCommands(); });
    });
    g_hbTimer->start();

    g_cmdTimer = new QTimer(qApp);
    g_cmdTimer->setInterval(kCommandPollIntervalMs);
    QObject::connect(g_cmdTimer, &QTimer::timeout, qApp, []() {
        QtConcurrent::run([]() { pollDeviceCommands(); });
    });
    g_cmdTimer->start();

    QtConcurrent::run([]() { heartbeatAndPollCommands(); });
}

void TestCaseSyncService::stopDeviceAgent(bool quickStop) {
    if (!g_agentStopped.testAndSetRelaxed(0, 1)) {
        return;
    }
    Qlog::saveResidentLog(QStringLiteral("agent"), QStringLiteral("停止常驻心跳/命令轮询"));
    if (g_hbTimer) {
        g_hbTimer->stop();
        delete g_hbTimer;
        g_hbTimer = nullptr;
    }
    if (g_cmdTimer) {
        g_cmdTimer->stop();
        delete g_cmdTimer;
        g_cmdTimer = nullptr;
    }
    if (!quickStop) {
        QThreadPool::globalInstance()->waitForDone(3000);
    }
}
