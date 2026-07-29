#include "test_case_sync_service.h"

#include "test_case.h"

#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QProcessEnvironment>
#include <QDateTime>
#include <QMetaObject>
#include <QSet>
#include <QSettings>
#include <QSysInfo>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QtConcurrent>
#include <qdebug.h>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kBackupDirName = ".backup";
constexpr int kHeartbeatIntervalMs = 20000;
/** 远控命令要快，单独高频拉命令，避免干等下一次 20s 心跳 */
constexpr int kCommandPollIntervalMs = 800;

QMutex g_remoteDesktopMutex;
QHash<QString, qint64> g_remoteDesktopPids; // sessionId -> pid

QString remoteAgentDirBesideBin() {
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("remote_agent"));
}

/** 开发机：build/.../bin → 仓库 remote_agent/ */
QString remoteAgentDirSourceTree() {
    return QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../remote_agent"));
}

bool remoteAgentDirUsable(const QString& dirPath) {
    const QDir dir(dirPath);
    if (!dir.exists()) {
        return false;
    }
    if (dir.exists(QStringLiteral("remote_agent.exe"))) {
        return true;
    }
    // 源码树：main.py + (venv 或 run.bat)
    if (!dir.exists(QStringLiteral("main.py"))) {
        return false;
    }
    return dir.exists(QStringLiteral(".venv/Scripts/python.exe")) || dir.exists(QStringLiteral("run.bat"));
}

QString remoteAgentDir() {
    const QString besideBin = remoteAgentDirBesideBin();
    if (remoteAgentDirUsable(besideBin)) {
        return besideBin;
    }
    const QString src = QDir(remoteAgentDirSourceTree()).absolutePath();
    if (remoteAgentDirUsable(src)) {
        return src;
    }
    // 默认仍返回 bin 旁路径，便于日志提示缺目录
    return besideBin;
}

bool remoteAgentAvailable() {
    return remoteAgentDirUsable(remoteAgentDir());
}

QString buildAgentSignalingUrl(const QString& sessionId, const QString& agentToken) {
    QUrl base(FactoryCloudClient::apiRoot() + QStringLiteral("/remote-desktop/ws"));
    QUrlQuery q;
    q.addQueryItem(QStringLiteral("sessionId"), sessionId);
    q.addQueryItem(QStringLiteral("role"), QStringLiteral("agent"));
    q.addQueryItem(QStringLiteral("token"), agentToken);
    base.setQuery(q);
    QString s = base.toString(QUrl::FullyEncoded);
    if (s.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive)) {
        s = QStringLiteral("wss://") + s.mid(8);
    } else if (s.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)) {
        s = QStringLiteral("ws://") + s.mid(7);
    }
    return s;
}

void stopRemoteDesktopSession(const QString& sessionId) {
    QMutexLocker locker(&g_remoteDesktopMutex);
    if (!g_remoteDesktopPids.contains(sessionId)) {
        return;
    }
    const qint64 pid = g_remoteDesktopPids.take(sessionId);
    locker.unlock();
    if (pid > 0) {
        QProcess killer;
        killer.start(QStringLiteral("taskkill"),
                     {QStringLiteral("/PID"), QString::number(pid), QStringLiteral("/T"), QStringLiteral("/F")});
        killer.waitForFinished(5000);
    }
    qDebug() << QStringLiteral("[RemoteDesktop] 已停止 session") << sessionId << "pid" << pid;
}

/** 上位机关闭时清理全部远控 Agent（含未登记/PyInstaller 残留） */
void stopAllRemoteDesktopSessions() {
    QList<qint64> pids;
    {
        QMutexLocker locker(&g_remoteDesktopMutex);
        pids = g_remoteDesktopPids.values();
        g_remoteDesktopPids.clear();
    }
    for (const qint64 pid : pids) {
        if (pid <= 0) {
            continue;
        }
        QProcess killer;
        killer.start(QStringLiteral("taskkill"),
                     {QStringLiteral("/PID"), QString::number(pid), QStringLiteral("/T"), QStringLiteral("/F")});
        killer.waitForFinished(5000);
    }
    // 兜底：按进程名杀干净，避免关闭上位机后 remote_agent.exe 仍挂后台
    QProcess byName;
    byName.start(QStringLiteral("taskkill"),
                 {QStringLiteral("/IM"), QStringLiteral("remote_agent.exe"), QStringLiteral("/T"),
                  QStringLiteral("/F")});
    byName.waitForFinished(8000);
    qDebug() << QStringLiteral("[RemoteDesktop] 已清理全部 Agent 进程");
}

void startRemoteDesktopSessionImpl(const QJsonObject& payload) {
    const qint64 t0Ms = QDateTime::currentMSecsSinceEpoch();
    const QString sessionId = payload.value(QStringLiteral("sessionId")).toString().trimmed();
    const QString agentToken = payload.value(QStringLiteral("agentToken")).toString().trimmed();
    if (sessionId.isEmpty() || agentToken.isEmpty()) {
        qDebug() << QStringLiteral("[RemoteDesktop] start 命令缺少 sessionId/agentToken");
        return;
    }
    if (!remoteAgentAvailable()) {
        qDebug() << QStringLiteral("[RemoteDesktop] 未找到远控 Agent。产线请部署 bin/remote_agent（exe+_internal）；"
                                   "开发可在仓库 remote_agent/ 执行 setup_dev.bat");
        return;
    }

    const QString workDir = remoteAgentDir();
    qDebug() << QStringLiteral("[RemoteDesktop] 使用 Agent 目录") << workDir;

    {
        QMutexLocker locker(&g_remoteDesktopMutex);
        if (g_remoteDesktopPids.contains(sessionId)) {
            locker.unlock();
            qDebug() << QStringLiteral("[RemoteDesktop] 会话已在运行，先结束旧 Agent 再按新参数启动") << sessionId;
            stopRemoteDesktopSession(sessionId);
            QThread::msleep(200);
        }
    }

    QJsonObject cfg;
    cfg.insert(QStringLiteral("sessionId"), sessionId);
    cfg.insert(QStringLiteral("agentToken"), agentToken);
    cfg.insert(QStringLiteral("signalingUrl"), buildAgentSignalingUrl(sessionId, agentToken));
    qDebug() << QStringLiteral("[RemoteDesktop] signaling") << cfg.value(QStringLiteral("signalingUrl")).toString();
    cfg.insert(QStringLiteral("iceServers"), payload.value(QStringLiteral("iceServers")));
    // 画质由管理端创建会话时下发；缺省与旧行为一致
    int maxWidth = payload.value(QStringLiteral("maxWidth")).toInt(1920);
    int fps = payload.value(QStringLiteral("fps")).toInt(30);
    qint64 maxBitrate = payload.value(QStringLiteral("maxBitrate")).toVariant().toLongLong();
    if (maxBitrate <= 0)
        maxBitrate = 12000000;
    maxWidth = qBound(maxWidth, 640, 1920);
    fps = qBound(fps, 5, 60);
    maxBitrate = qBound(maxBitrate, qint64(500000), qint64(25000000));
    cfg.insert(QStringLiteral("maxWidth"), maxWidth);
    cfg.insert(QStringLiteral("fps"), fps);
    cfg.insert(QStringLiteral("maxBitrate"), maxBitrate);

    qDebug() << QStringLiteral("[RemoteDesktop] Agent 推流参数 maxWidth=") << maxWidth << "fps=" << fps
             << "maxBitrate=" << maxBitrate;

    // 配置走环境变量，不写 session_*.json
    const QByteArray cfgBytes = QJsonDocument(cfg).toJson(QJsonDocument::Compact);
    auto* proc = new QProcess(qApp);
    proc->setWorkingDirectory(workDir);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("REMOTE_AGENT_CONFIG"), QString::fromUtf8(cfgBytes));
    proc->setProcessEnvironment(env);
    const QString agentExe = QDir(workDir).filePath(QStringLiteral("remote_agent.exe"));
    const QString venvPy = QDir(workDir).filePath(QStringLiteral(".venv/Scripts/python.exe"));
    const QString mainPy = QDir(workDir).filePath(QStringLiteral("main.py"));
    const QStringList agentArgs{QStringLiteral("--config"), QStringLiteral("env:REMOTE_AGENT_CONFIG"),
                                QStringLiteral("--log-dir"), workDir};
    // 产线：onedir 的 remote_agent.exe（需保留同目录 _internal）；开发机：venv / run.bat
    QString launchVia;
    if (QFileInfo::exists(agentExe)) {
        proc->setProgram(agentExe);
        proc->setArguments(agentArgs);
        launchVia = QStringLiteral("exe");
    } else if (QFileInfo::exists(venvPy) && QFileInfo::exists(mainPy)) {
        proc->setProgram(venvPy);
        proc->setArguments(QStringList{mainPy} + agentArgs);
        launchVia = QStringLiteral("venv");
    } else {
        proc->setProgram(QStringLiteral("cmd.exe"));
        proc->setArguments(QStringList{QStringLiteral("/c"), QStringLiteral("run.bat")} + agentArgs);
        launchVia = QStringLiteral("run.bat");
    }
    proc->setProcessChannelMode(QProcess::MergedChannels);
    QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc,
                     [proc, sessionId](int code, QProcess::ExitStatus status) {
                         const QByteArray tail = proc->readAllStandardOutput().trimmed().right(800);
                         {
                             QMutexLocker locker(&g_remoteDesktopMutex);
                             g_remoteDesktopPids.remove(sessionId);
                         }
                         qDebug() << QStringLiteral("[RemoteDesktop] Agent 退出") << sessionId << "code" << code
                                  << "status" << int(status);
                         if (!tail.isEmpty()) {
                             qDebug() << QStringLiteral("[RemoteDesktop] Agent 输出尾部:") << QString::fromUtf8(tail);
                         }
                         proc->deleteLater();
                     });
    qDebug() << QStringLiteral("[RemoteDesktop] 即将启动 Agent via=") << launchVia << "cwd=" << workDir
             << "距收到命令" << (QDateTime::currentMSecsSinceEpoch() - t0Ms) << "ms";
    proc->start();
    if (!proc->waitForStarted(8000)) {
        qDebug() << QStringLiteral("[RemoteDesktop] 启动 Agent 失败：") << proc->errorString();
        proc->deleteLater();
        return;
    }
    {
        QMutexLocker locker(&g_remoteDesktopMutex);
        g_remoteDesktopPids.insert(sessionId, proc->processId());
    }
    qDebug() << QStringLiteral("[RemoteDesktop] Agent 进程已拉起") << sessionId << "pid" << proc->processId()
             << "via" << launchVia << "耗时" << (QDateTime::currentMSecsSinceEpoch() - t0Ms) << "ms"
             << "（若距点击仍很久，多半是命令轮询；进程拉起后还要等 Agent 初始化）";
}

void startRemoteDesktopSession(const QJsonObject& payload) {
    // pollDeviceCommands 在 QtConcurrent 线程里跑，QProcess 必须在 GUI 主线程创建/启动
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) {
        return;
    }
    if (QThread::currentThread() != app->thread()) {
        const QJsonObject copy = payload;
        QTimer::singleShot(0, app, [copy]() { startRemoteDesktopSessionImpl(copy); });
        return;
    }
    startRemoteDesktopSessionImpl(payload);
}

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

    const QString profileDir = TestCasePaths::profileDir(key);
    if (!QDir(profileDir).exists()) {
        result.message = QStringLiteral("工站 Profile 目录不存在：") + profileDir;
        return result;
    }

    const QString profileIni = TestCasePaths::profileMetaPath(key);
    // 主动上传时本地草稿号 +1（仅作草稿痕迹）；正式号在网页合入时由服务器重写
    const int profileVersion =
        isPull ? readProfileVersion(profileIni) : bumpProfileVersion(profileIni);

    QString zipError;
    const QString zipPath =
        compressDirectoryContents(profileDir, QStringLiteral("profile_%1").arg(key), &zipError);
    if (zipPath.isEmpty()) {
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

    const QString path =
        QStringLiteral("/test-cases/profiles/%1/upload").arg(QString::fromUtf8(QUrl::toPercentEncoding(key)));
    const FactoryCloudClient::ApiResult api = FactoryCloudClient::uploadMultipart(path, zipPath, fields);
    QFile::remove(zipPath);

    if (!api.ok) {
        result.message = api.message.isEmpty() ? QStringLiteral("上传失败") : api.message;
        return result;
    }

    const QString remoteProfileVersion = api.data.value(QStringLiteral("profileVersion")).toString().trimmed();
    const int fileCount = api.data.value(QStringLiteral("fileCount")).toInt(0);

    result.ok = true;
    result.stationKey = key;
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
    result.message +=
        QStringLiteral("（本地草稿号 %1；网页合入后会由服务器分配正式 ProfileVersion）").arg(result.profileVersion);
    return result;
}

TestCaseSyncService::SyncResult TestCaseSyncService::uploadToCloud(const QString& remark) {
    const QString key = TestCaseStore::loadSelectedFlowStationKey();
    const QString name = TestCaseStore::loadSelectedFlowStationName();
    return uploadStationProfile(key, name, QStringLiteral("upload"), remark);
}

/** 下载前把工站登记进本地 FlowStations，保证 profiles/{显示名}/ 路径正确 */
static void ensureLocalFlowStationCatalog(const QString& stationKey, const QString& displayName) {
    const QString key = stationKey.trimmed();
    const QString name = displayName.trimmed().isEmpty() ? key : displayName.trimmed();
    if (key.isEmpty() || name.isEmpty()) {
        return;
    }

    QVector<TestFlowStationEntry> catalog = TestCaseStore::loadFlowStationCatalog();
    bool changed = false;
    bool found = false;
    for (TestFlowStationEntry& entry : catalog) {
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0) {
            found = true;
            if (entry.displayName != name) {
                entry.displayName = name;
                changed = true;
            }
            break;
        }
        if (entry.displayName == name) {
            found = true;
            if (entry.key.compare(key, Qt::CaseInsensitive) != 0) {
                entry.key = key;
                changed = true;
            }
            break;
        }
    }
    if (!found) {
        catalog.append({key, name});
        changed = true;
    }
    if (changed) {
        TestCaseStore::saveFlowStationCatalog(catalog);
    }
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
    if (key.isEmpty()) {
        result.message = QStringLiteral("未指定工站，无法下载用例");
        return result;
    }
    if (name.isEmpty()) {
        name = key;
    }

    const QString encodedKey = QString::fromUtf8(QUrl::toPercentEncoding(key));
    const FactoryCloudClient::ApiResult manifest =
        FactoryCloudClient::get(QStringLiteral("/test-cases/profiles/%1/manifest").arg(encodedKey));
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
    if (!remoteDisplayName.isEmpty()) {
        name = remoteDisplayName;
    }
    // 下载别的工站前先登记目录映射，避免落盘到错误路径
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
            QStringLiteral("/test-cases/profiles/%1/bundle").arg(encodedKey), QUrlQuery(), zipPath,
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

    result.ok = true;
    result.stationKey = key;
    result.profileVersion = remoteProfileVersion;
    result.bundleVersion = bundleVersion;
    result.fileCount = count;
    result.message = QStringLiteral("已下载工站「%1」正式用例").arg(name);
    if (!remoteProfileVersion.isEmpty()) {
        result.message += QStringLiteral("（profile=%1）").arg(remoteProfileVersion);
    }
    return result;
}

TestCaseSyncService::SyncResult TestCaseSyncService::syncFromCloud() {
    const QString key = TestCaseStore::loadSelectedFlowStationKey();
    const QString name = TestCaseStore::loadSelectedFlowStationName();
    return syncStationFromCloud(key, name);
}

void TestCaseSyncService::heartbeatAndPollCommands() {
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
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
    body.insert(QStringLiteral("remoteDesktop"), remoteAgentAvailable());
    const FactoryCloudClient::ApiResult hb = FactoryCloudClient::post(QStringLiteral("/device/heartbeat"), body);
    if (!hb.ok) {
        qDebug() << QStringLiteral("[TestCaseSync] heartbeat 失败：") << hb.message;
        return;
    }

    pollDeviceCommands();
}

void TestCaseSyncService::pollDeviceCommands() {
    QString readyError;
    if (!ensureCloudReady(&readyError)) {
        return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("deviceId"), FactoryCloudClient::deviceId());
    const FactoryCloudClient::ApiResult cmdRes =
        FactoryCloudClient::get(QStringLiteral("/device/commands"), query);
    if (!cmdRes.ok) {
        return;
    }

    const QJsonArray items = cmdRes.data.value(QStringLiteral("items")).toArray();
    for (const QJsonValue& value : items) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject cmd = value.toObject();
        const QString type = cmd.value(QStringLiteral("type")).toString().trimmed();
        const QJsonObject payload = cmd.value(QStringLiteral("payload")).toObject();
        if (type == QLatin1String("start_remote_desktop")) {
            startRemoteDesktopSession(payload);
            continue;
        }
        if (type == QLatin1String("stop_remote_desktop")) {
            stopRemoteDesktopSession(payload.value(QStringLiteral("sessionId")).toString().trimmed());
            continue;
        }
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
        qDebug() << QStringLiteral("[TestCaseSync] 网页拉取回传：") << uploadResult.message;
    }
}

void TestCaseSyncService::startDeviceAgent() {
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    static bool started = false;
    if (started) {
        return;
    }
    started = true;

    // 退出进程前必须杀掉远控 Agent，否则 remote_agent.exe 会残留后台
    QObject::connect(qApp, &QCoreApplication::aboutToQuit, qApp, []() { stopAllRemoteDesktopSessions(); });

    auto* hbTimer = new QTimer(qApp);
    hbTimer->setInterval(kHeartbeatIntervalMs);
    QObject::connect(hbTimer, &QTimer::timeout, qApp, []() {
        QtConcurrent::run([]() { heartbeatAndPollCommands(); });
    });
    hbTimer->start();

    auto* cmdTimer = new QTimer(qApp);
    cmdTimer->setInterval(kCommandPollIntervalMs);
    QObject::connect(cmdTimer, &QTimer::timeout, qApp, []() {
        QtConcurrent::run([]() { pollDeviceCommands(); });
    });
    cmdTimer->start();

    QtConcurrent::run([]() { heartbeatAndPollCommands(); });
}
