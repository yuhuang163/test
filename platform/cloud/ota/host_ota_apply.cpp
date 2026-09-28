#include "host_ota_apply.h"

#include "application_shutdown.h"
#include "factory_cloud_client.h"
#include "host_ota_service.h"
#include "host_ota_ui.h"
#include "host_ota_vbs.h"

#include <algorithm>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QStringList>
#include <QTextStream>
#include <QUrlQuery>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static QString sha256File(const QString& path) {
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

static bool isSha256Hex(const QString& s) {
    if (s.size() != 64) {
        return false;
    }
    for (const QChar& c : s) {
        const char u = c.toLatin1();
        const bool digit = u >= '0' && u <= '9';
        const bool hexLo = u >= 'a' && u <= 'f';
        const bool hexUp = u >= 'A' && u <= 'F';
        if (!digit && !hexLo && !hexUp) {
            return false;
        }
    }
    return true;
}

static bool isSafeRelativeOtaPath(const QString& path) {
    const QString raw = path.trimmed();
    if (raw.isEmpty() || QDir::isAbsolutePath(raw)) {
        return false;
    }
    const QString norm = QString(raw).replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (norm.startsWith(QLatin1Char('/')) || norm.contains(QLatin1Char(':'))) {
        return false;
    }
    const QString cleaned = QDir::cleanPath(norm);
    if (cleaned == QLatin1String("..") || cleaned.startsWith(QStringLiteral("../"))) {
        return false;
    }
    const QStringList parts = cleaned.split(QLatin1Char('/'), QString::SkipEmptyParts);
    if (parts.isEmpty()) {
        return false;
    }
    for (const QString& part : parts) {
        if (part == QLatin1String(".") || part == QLatin1String("..")) {
            return false;
        }
    }
    return true;
}

static bool isPathInsideRoot(const QString& path, const QString& root) {
    const QString p = QDir::cleanPath(QDir::fromNativeSeparators(path)).toLower();
    const QString r = QDir::cleanPath(QDir::fromNativeSeparators(root)).toLower();
    return p == r || p.startsWith(r + QLatin1Char('/'));
}

static bool isSkippedEnvManifestPath(const QString& path) {
    const QString norm = path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (norm.isEmpty()) {
        return true;
    }
    // 运行环境 zip 误打包 OTA 临时目录时，manifest 会出现 updates/staging 等脏路径
    if (norm.contains(QStringLiteral("/updates/"), Qt::CaseInsensitive)
        || norm.startsWith(QStringLiteral("updates/"), Qt::CaseInsensitive)
        || norm.contains(QStringLiteral("/backup/"), Qt::CaseInsensitive)
        || norm.startsWith(QStringLiteral("backup/"), Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

static QList<OtaFileSpec> normalizeManifestPaths(QList<OtaFileSpec> files) {
    if (files.isEmpty()) {
        return files;
    }
    for (;;) {
        QString commonRoot;
        for (const OtaFileSpec& f : files) {
            const int slash = f.path.indexOf(QLatin1Char('/'));
            if (slash <= 0) {
                return files;
            }
            const QString root = f.path.left(slash);
            if (commonRoot.isEmpty()) {
                commonRoot = root;
            } else if (commonRoot != root) {
                return files;
            }
        }
        if (commonRoot.isEmpty()) {
            return files;
        }
        for (OtaFileSpec& f : files) {
            f.path = f.path.mid(commonRoot.size() + 1);
        }
    }
}

static bool validatePackage(const HostOtaService::CheckResult& info, QString* message) {
    if (!info.hasUpdate) {
        if (message) {
            *message = QStringLiteral("无可用更新");
        }
        return false;
    }

    const QString localPkg = FactoryCloudClient::packageName();
    const QString pkg = info.packageName.trimmed();
    if (pkg.isEmpty()) {
        if (message) {
            *message = QStringLiteral("更新包 packageName 为空，拒绝升级");
        }
        return false;
    }
    if (pkg.compare(localPkg, Qt::CaseInsensitive) != 0) {
        if (message) {
            *message = QStringLiteral("packageName 不匹配（本地 %1，更新包 %2）").arg(localPkg, pkg);
        }
        return false;
    }
    const QString expectedSha = info.sha256.trimmed().toLower();
    if (!isSha256Hex(expectedSha)) {
        if (message) {
            *message = QStringLiteral("更新包 sha256 缺失或无效，拒绝升级");
        }
        return false;
    }
    return true;
}

static bool downloadExe(const HostOtaService::CheckResult& info,
                                 const QString& stagingDir,
                                 QString* outExeName,
                                 QString* outExeSha,
                                 QString* message) {
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    if (!isSafeRelativeOtaPath(exeName)) {
        if (message) {
            *message = QStringLiteral("exe 文件名非法");
        }
        return false;
    }
    const QString exeStagingPath = QDir(stagingDir).filePath(exeName);
    QString downloadError;
    const QString pkg = info.packageName.trimmed();
    const QString expectedSha = info.sha256.trimmed().toLower();
    const QString url = info.downloadUrl.trimmed();
    if (url.isEmpty()) {
        QUrlQuery downloadQuery;
        downloadQuery.addQueryItem(QStringLiteral("packageName"), pkg);
        if (!info.uploadedAt.isEmpty()) {
            downloadQuery.addQueryItem(QStringLiteral("uploadedAt"), info.uploadedAt);
        }
        if (!FactoryCloudClient::downloadToFile(QStringLiteral("/host-app/download/") + info.buildId, downloadQuery,
                                                exeStagingPath, &downloadError)) {
            if (message) {
                *message = downloadError;
            }
            return false;
        }
    } else if (!FactoryCloudClient::downloadToFile(url, QUrlQuery(), exeStagingPath, &downloadError)) {
        if (message) {
            *message = downloadError;
        }
        return false;
    }

    const QString exeSha = sha256File(exeStagingPath);
    if (exeSha.compare(expectedSha, Qt::CaseInsensitive) != 0) {
        qDebug() << "[OTA] exe sha256 不匹配: 期望=" << expectedSha << "实际=" << exeSha;
        QFile::remove(exeStagingPath);
        if (message) {
            *message = QStringLiteral("exe sha256 校验失败");
        }
        return false;
    }
    if (outExeName) {
        *outExeName = exeName;
    }
    if (outExeSha) {
        *outExeSha = exeSha;
    }
    return true;
}

static QList<OtaFileSpec> fetchEnvManifest(QString* err) {
    QList<OtaFileSpec> files;
    const FactoryCloudClient::ApiResult api =
        FactoryCloudClient::get(QStringLiteral("/host-app/runtime-env/manifest"));
    if (!api.ok) {
        if (err) {
            *err = api.message;
        }
        return files;
    }
    const QJsonArray arr = api.data.value(QStringLiteral("files")).toArray();
    for (const QJsonValue& v : arr) {
        const QJsonObject o = v.toObject();
        const QString path = o.value(QStringLiteral("path")).toString();
        if (path.isEmpty() || isSkippedEnvManifestPath(path)) {
            continue;
        }
        if (!isSafeRelativeOtaPath(path)) {
            if (err) {
                *err = QStringLiteral("环境清单路径非法：%1").arg(path);
            }
            return {};
        }
        OtaFileSpec spec;
        spec.path = path;
        spec.sha256 = o.value(QStringLiteral("sha256")).toString().trimmed().toLower();
        spec.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
        if (!isSha256Hex(spec.sha256)) {
            if (err) {
                *err = QStringLiteral("环境清单 %1 sha256 缺失或无效").arg(path);
            }
            return {};
        }
        files.append(spec);
    }
    files = normalizeManifestPaths(files);
    for (const OtaFileSpec& spec : files) {
        if (!isSafeRelativeOtaPath(spec.path) || !isSha256Hex(spec.sha256)) {
            if (err) {
                *err = QStringLiteral("环境清单规范化后仍非法：%1").arg(spec.path);
            }
            return {};
        }
    }
    return files;
}

static QList<OtaFileSpec> diffLocalFiles(const QList<OtaFileSpec>& serverFiles,
                                                   const QString& appDir) {
    QList<OtaFileSpec> changed;
    for (const OtaFileSpec& f : serverFiles) {
        const QString localSha = sha256File(QDir(appDir).filePath(f.path));
        if (localSha.isEmpty() || localSha.compare(f.sha256, Qt::CaseInsensitive) != 0) {
            changed.append(f);
        }
    }
    return changed;
}

static bool downloadDeltaFiles(const QList<OtaFileSpec>& files,
                                         const QString& stagingDir,
                                         QString* err) {
    for (const OtaFileSpec& f : files) {
        if (!isSafeRelativeOtaPath(f.path)) {
            if (err) {
                *err = QStringLiteral("下载路径非法：%1").arg(f.path);
            }
            return false;
        }
        if (!isSha256Hex(f.sha256)) {
            if (err) {
                *err = QStringLiteral("%1 sha256 缺失或无效，拒绝下载").arg(f.path);
            }
            return false;
        }
        const QString target = QDir(stagingDir).filePath(f.path);
        if (!isPathInsideRoot(QFileInfo(target).absolutePath(), stagingDir)
            && !isPathInsideRoot(target, stagingDir)) {
            if (err) {
                *err = QStringLiteral("下载目标超出 staging：%1").arg(f.path);
            }
            return false;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        QString dlErr;
        if (!FactoryCloudClient::downloadToFile(QStringLiteral("/host-app/files/") + f.sha256, QUrlQuery(),
                                                target, &dlErr)) {
            if (err) {
                *err = QStringLiteral("下载 %1 失败: %2").arg(f.path, dlErr);
            }
            return false;
        }
        const QString actual = sha256File(target);
        if (actual.compare(f.sha256, Qt::CaseInsensitive) != 0) {
            if (err) {
                *err = QStringLiteral("%1 sha256 校验失败（期望 %2，实际 %3）")
                           .arg(f.path, f.sha256,
                                actual.isEmpty() ? QStringLiteral("无法读取文件") : actual);
            }
            return false;
        }
    }
    return true;
}

static bool isUnderUpdatesTree(const QString& nativePath) {
    const QString norm = QDir::fromNativeSeparators(nativePath).toLower();
    return norm.contains(QStringLiteral("/updates/")) || norm.endsWith(QStringLiteral("/updates"));
}

static QList<RollbackFileEntry> parseRollbackInfoFile(const QString& appDir, const QString& infoPath) {
    QList<RollbackFileEntry> out;
    QFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::ReadOnly)) {
        return out;
    }
    const QJsonObject root = QJsonDocument::fromJson(infoFile.readAll()).object();
    const QString backupRoot = QDir(appDir).filePath(QStringLiteral("updates/backup"));
    QString backupDir = root.value(QStringLiteral("backupDir")).toString().trimmed();
    if (backupDir.isEmpty()) {
        backupDir = backupRoot;
    } else if (!QDir::isAbsolutePath(backupDir)) {
        backupDir = QDir(appDir).filePath(backupDir);
    }
    if (!isPathInsideRoot(backupDir, backupRoot)) {
        return out;
    }
    const QJsonArray files = root.value(QStringLiteral("files")).toArray();
    for (const QJsonValue& v : files) {
        QString rel;
        bool existedBefore = true;
        if (v.isString()) {
            rel = v.toString();
        } else {
            const QJsonObject o = v.toObject();
            rel = o.value(QStringLiteral("path")).toString();
            existedBefore = o.value(QStringLiteral("existedBefore")).toBool(true);
        }
        if (!isSafeRelativeOtaPath(rel)) {
            continue;
        }
        RollbackFileEntry entry;
        entry.targetPath = QDir(appDir).filePath(rel);
        entry.backupPath = QDir(backupDir).filePath(rel);
        entry.existedBefore = existedBefore;
        if (!isPathInsideRoot(entry.targetPath, appDir)) {
            continue;
        }
        if (existedBefore) {
            if (!QFileInfo::exists(entry.backupPath)
                || !isPathInsideRoot(entry.backupPath, backupDir)) {
                continue;
            }
        } else if (!QFileInfo::exists(entry.targetPath)) {
            continue;
        }
        out.append(entry);
    }
    return out;
}

static QList<RollbackFileEntry> collectBackupSnapshot(const QString& appDir, const QString& backupDir) {
    QList<RollbackFileEntry> out;
    const QString backupRoot = QDir(appDir).filePath(QStringLiteral("updates/backup"));
    if (!isPathInsideRoot(backupDir, backupRoot) || !QDir(backupDir).exists()) {
        return out;
    }
    QDirIterator backupIt(backupDir, QDir::Files, QDirIterator::Subdirectories);
    while (backupIt.hasNext()) {
        const QString backupPath = backupIt.next();
        const QString rel = QDir(backupDir).relativeFilePath(backupPath);
        if (rel.isEmpty() || !isSafeRelativeOtaPath(rel)) {
            continue;
        }
        const QString targetPath = QDir(appDir).filePath(rel);
        if (!isPathInsideRoot(targetPath, appDir)) {
            continue;
        }
        out.append({backupPath, targetPath, true});
    }
    return out;
}

static QList<OtaFileSpec> orderOtaReplaceFiles(const QList<OtaFileSpec>& changed, const QString& exeName) {
    QList<OtaFileSpec> ordered = changed;
    std::stable_sort(ordered.begin(), ordered.end(),
                     [&exeName](const OtaFileSpec& a, const OtaFileSpec& b) {
                         const bool aExe =
                             QFileInfo(a.path).fileName().compare(exeName, Qt::CaseInsensitive) == 0;
                         const bool bExe =
                             QFileInfo(b.path).fileName().compare(exeName, Qt::CaseInsensitive) == 0;
                         if (aExe == bExe) {
                             return false;
                         }
                         return !aExe && bExe;
                     });
    return ordered;
}

static void writeRollbackInfoJson(const QString& appDir,
                                               const QString& backupDir,
                                               const QString& fromVersion,
                                               const QString& fromBuildId,
                                               const QString& toVersion,
                                               const QString& toBuildId,
                                               const QList<OtaFileSpec>& changed) {
    QJsonObject root;
    root.insert(QStringLiteral("createdAt"),
               QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    root.insert(QStringLiteral("fromVersion"), fromVersion);
    root.insert(QStringLiteral("fromBuildId"), fromBuildId);
    root.insert(QStringLiteral("toVersion"), toVersion);
    root.insert(QStringLiteral("toBuildId"), toBuildId);
    root.insert(QStringLiteral("backupDir"), QDir::fromNativeSeparators(backupDir));
    QJsonArray files;
    for (const OtaFileSpec& spec : changed) {
        if (spec.path.isEmpty()) {
            continue;
        }
        QJsonObject item;
        item.insert(QStringLiteral("path"), spec.path);
        item.insert(QStringLiteral("existedBefore"), spec.existedBefore);
        files.append(item);
    }
    root.insert(QStringLiteral("files"), files);
    const QString infoPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.json"));
    const QString prevPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.prev.json"));
    QDir().mkpath(QFileInfo(infoPath).absolutePath());
    if (QFile::exists(infoPath)) {
        QFile::remove(prevPath);
        QFile::copy(infoPath, prevPath);
    }
    QFile infoFile(infoPath);
    if (infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        infoFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

QString HostOtaApply::readLastRollbackInfoSummary(const QString& appDir) {
    const QString infoPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.json"));
    QFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(infoFile.readAll()).object();
    const QString createdAt = root.value(QStringLiteral("createdAt")).toString().trimmed();
    const QString fromVersion = root.value(QStringLiteral("fromVersion")).toString().trimmed();
    const QString fromBuildId = root.value(QStringLiteral("fromBuildId")).toString().trimmed();
    const QString toVersion = root.value(QStringLiteral("toVersion")).toString().trimmed();
    const QString toBuildId = root.value(QStringLiteral("toBuildId")).toString().trimmed();
    const int fileCount = root.value(QStringLiteral("files")).toArray().size();
    if (createdAt.isEmpty() && fileCount <= 0 && fromVersion.isEmpty() && toVersion.isEmpty()) {
        return {};
    }
    QString summary = QStringLiteral("上次升级时间：%1，备份文件数：%2")
                          .arg(createdAt.isEmpty() ? QStringLiteral("未知") : createdAt)
                          .arg(fileCount);
    if (!fromVersion.isEmpty() || !toVersion.isEmpty()) {
        summary += QStringLiteral("\n版本：%1 (%2) → %3 (%4)")
                       .arg(fromVersion.isEmpty() ? QStringLiteral("未知") : fromVersion,
                            fromBuildId.isEmpty() ? QStringLiteral("未知") : fromBuildId,
                            toVersion.isEmpty() ? QStringLiteral("未知") : toVersion,
                            toBuildId.isEmpty() ? QStringLiteral("未知") : toBuildId);
    }
    return summary;
}

QList<RollbackFileEntry> HostOtaApply::collectRollbackFiles(const QString& appDir) {
    const QString infoPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.json"));
    QList<RollbackFileEntry> entries = parseRollbackInfoFile(appDir, infoPath);
    if (!entries.isEmpty()) {
        return entries;
    }
    const QString prevPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.prev.json"));
    entries = parseRollbackInfoFile(appDir, prevPath);
    if (!entries.isEmpty()) {
        return entries;
    }

    // 无清单时只取一个备份快照：最新事务目录，否则旧版扁平 backup，避免混入多代文件
    const QString backupRoot = QDir(appDir).filePath(QStringLiteral("updates/backup"));
    QDir backupDir(backupRoot);
    if (backupDir.exists()) {
        const QFileInfoList subdirs = backupDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
        if (!subdirs.isEmpty()) {
            entries = collectBackupSnapshot(appDir, subdirs.first().absoluteFilePath());
            if (!entries.isEmpty()) {
                return entries;
            }
        }
        entries = collectBackupSnapshot(appDir, backupRoot);
        if (!entries.isEmpty()) {
            return entries;
        }
    }

    QMap<QString, RollbackFileEntry> byTargetLower;
    QDirIterator bakIt(appDir, QStringList{QStringLiteral("*.bak")}, QDir::Files, QDirIterator::Subdirectories);
    while (bakIt.hasNext()) {
        const QString backupPath = bakIt.next();
        if (isUnderUpdatesTree(backupPath)) {
            continue;
        }
        QString targetPath = backupPath;
        if (targetPath.endsWith(QStringLiteral(".bak"), Qt::CaseInsensitive)) {
            targetPath.chop(4);
        }
        if (targetPath.isEmpty() || byTargetLower.contains(targetPath.toLower())) {
            continue;
        }
        byTargetLower.insert(targetPath.toLower(), {backupPath, targetPath, true});
    }
    return byTargetLower.values();
}

static bool createOtaTransaction(const QList<OtaFileSpec>& changed,
                                              const QString& updatesDir,
                                              const QString& toVersion,
                                              const QString& toBuildId,
                                              OtaTransaction* outTxn,
                                              QString* error) {
    if (!outTxn) {
        if (error) {
            *error = QStringLiteral("内部错误：事务输出为空");
        }
        return false;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QList<OtaFileSpec> orderedChanged = orderOtaReplaceFiles(changed, exeName);
    const QString stagingDir = QDir(updatesDir).filePath(QStringLiteral("staging"));
    const QString txnId = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMddHHmmss"))
                          + QLatin1Char('_')
                          + QString::number(QCoreApplication::applicationPid());
    const QString backupDir = QDir(updatesDir).filePath(QStringLiteral("backup/") + txnId);
    QDir().mkpath(backupDir);

    for (const OtaFileSpec& spec : orderedChanged) {
        if (!isSafeRelativeOtaPath(spec.path)
            || !isPathInsideRoot(QDir::cleanPath(QDir(appDir).filePath(spec.path)), appDir)
            || !isPathInsideRoot(QDir::cleanPath(QDir(stagingDir).filePath(spec.path)), stagingDir)
            || !isPathInsideRoot(QDir::cleanPath(QDir(backupDir).filePath(spec.path)), backupDir)) {
            if (error) {
                *error = QStringLiteral("不安全路径被拒绝：") + spec.path;
            }
            return false;
        }
    }

    outTxn->appDir = appDir;
    outTxn->updatesDir = updatesDir;
    outTxn->stagingDir = stagingDir;
    outTxn->backupDir = backupDir;
    outTxn->txnId = txnId;
    outTxn->logPath = QDir(appDir).filePath(QStringLiteral("ota_replace.log"));
    outTxn->vbsFileName = QDir(appDir).filePath(QStringLiteral("ota_replace.vbs"));
    outTxn->exeName = exeName;
    outTxn->toVersion = toVersion;
    outTxn->toBuildId = toBuildId;
    outTxn->orderedFiles = orderedChanged;
    QDir().mkpath(QFileInfo(outTxn->logPath).absolutePath());
    return true;
}

static bool startOtaReplaceVbs(const OtaTransaction& txn, QString* error) {
    if (txn.orderedFiles.isEmpty()) {
        if (error) {
            *error = QStringLiteral("替换清单为空");
        }
        return false;
    }
    QDir().mkpath(QFileInfo(txn.logPath).absolutePath());
    const QString oldPid = QString::number(QCoreApplication::applicationPid());
    const int n = txn.orderedFiles.size();
    QElapsedTimer hostTimer;
    hostTimer.start();
    {
        QFile hostLog(txn.logPath);
        if (hostLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&hostLog);
            const auto hostLine = [&](const QString& msg) {
                out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                    << QStringLiteral(" [host +") << hostTimer.elapsed() << QStringLiteral("ms] ") << msg
                    << "\r\n";
            };
            hostLine(QStringLiteral("OTA replace plan files=%1 pid=%2 backup=%3")
                         .arg(n)
                         .arg(oldPid)
                         .arg(txn.txnId));
            for (const OtaFileSpec& spec : txn.orderedFiles) {
                hostLine(QStringLiteral("  file: ") + spec.path
                         + (spec.existedBefore ? QStringLiteral(" (backup)") : QStringLiteral(" (new)")));
            }
        }
    }
    const auto appendHostLog = [&](const QString& msg) {
        QFile hostLog(txn.logPath);
        if (hostLog.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&hostLog);
            out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                << QStringLiteral(" [host +") << hostTimer.elapsed() << QStringLiteral("ms] ") << msg << "\r\n";
        }
    };

    QList<HostOtaVbs::ReplaceEntry> replaceFiles;
    replaceFiles.reserve(n);
    for (const OtaFileSpec& spec : txn.orderedFiles) {
        replaceFiles.append({spec.path, spec.existedBefore});
    }
    if (!HostOtaVbs::writeReplaceScript(txn.vbsFileName, txn.appDir, txn.exeName, txn.logPath,
                                        txn.stagingDir, txn.backupDir, replaceFiles)) {
        if (error) {
            *error = QStringLiteral("无法写入升级脚本");
        }
        return false;
    }

    writeRollbackInfoJson(txn.appDir, txn.backupDir, FactoryCloudClient::appVersion(),
                                              FactoryCloudClient::buildId(), txn.toVersion, txn.toBuildId,
                                              txn.orderedFiles);
    appendHostLog(QStringLiteral("vbs script written bytes=%1").arg(QFileInfo(txn.vbsFileName).size()));
    ApplicationShutdown::prepareForOtaReplace();
    appendHostLog(QStringLiteral("prepareForOtaReplace done"));
    appendHostLog(QStringLiteral("launching wscript"));
    if (!HostOtaVbs::launchAndExit(txn.vbsFileName)) {
        if (error) {
            *error = QStringLiteral("无法启动升级脚本");
        }
        return false;
    }
    return true;
}

QString HostOtaApply::sha256File(const QString& path) {
    return ::sha256File(path);
}

bool HostOtaApply::downloadAndApply(const HostOtaService::CheckResult& info,
                                    QWidget* parent,
                                    QString* message) {
    if (!validatePackage(info, message)) {
        return false;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString updatesDir = QDir(appDir).filePath(QStringLiteral("updates"));
    QDir().mkpath(updatesDir);

    const QString stagingDir = QDir(updatesDir).filePath(QStringLiteral("staging"));
    QDir(stagingDir).removeRecursively();
    QDir().mkpath(stagingDir);

    QString exeName;
    QString exeSha;
    if (!downloadExe(info, stagingDir, &exeName, &exeSha, message)) {
        return false;
    }

    QString envErr;
    const QList<OtaFileSpec> envFiles = fetchEnvManifest(&envErr);
    if (!envErr.isEmpty()) {
        if (message) {
            *message = envErr;
        }
        return false;
    }
    const QList<OtaFileSpec> changedEnv = diffLocalFiles(envFiles, appDir);
    QString downloadError;
    if (!downloadDeltaFiles(changedEnv, stagingDir, &downloadError)) {
        if (message) {
            *message = downloadError;
        }
        return false;
    }

    QList<OtaFileSpec> changed = changedEnv;
    for (OtaFileSpec& spec : changed) {
        spec.existedBefore = QFile::exists(QDir(appDir).filePath(spec.path));
    }
    OtaFileSpec exeSpec;
    exeSpec.path = exeName;
    exeSpec.sha256 = exeSha;
    exeSpec.existedBefore = QFile::exists(QDir(appDir).filePath(exeName));
    changed.append(exeSpec);

    OtaTransaction txn;
    QString txnErr;
    if (!createOtaTransaction(changed, updatesDir, info.appVersion, info.buildId, &txn, &txnErr)) {
        if (message) {
            *message = txnErr.isEmpty() ? QStringLiteral("无法创建升级事务") : txnErr;
        }
        return false;
    }

    HostOtaUi::showRestartCountdown(parent);
    // 重启前落盘新版本更新说明，供升级后上报测试结果时附带
    HostOtaService::saveLocalReleaseNotes(info.appVersion, info.releaseNotes);
    QString replaceErr;
    if (!startOtaReplaceVbs(txn, &replaceErr)) {
        if (message) {
            *message = replaceErr.isEmpty() ? QStringLiteral("无法启动升级脚本") : replaceErr;
        }
        return false;
    }
    if (message) {
        *message = QStringLiteral("正在安装新版本…");
    }
    return true;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
