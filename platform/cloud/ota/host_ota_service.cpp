#include "host_ota_service.h"

#include "application_shutdown.h"
#include "auth_service.h"
#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <algorithm>

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QDateTime>
#include <QDirIterator>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>
#include <functional>
#include <string>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
#endif

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

static bool launchVbsAndExit(const QString& vbsFileName);

#ifdef Q_OS_WIN
/** 枚举当前所有同名 exe 的 PID，供 OTA VBS 按 PID 异步 taskkill（避免 /IM 同步等待十几秒）。 */
static QString collectMatchingProcessPidsCsv(const QString& imageName) {
    QStringList pids;
    const std::wstring nameW = imageName.toStdWString();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return {};
    }
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, nameW.c_str()) == 0) {
                pids << QString::number(static_cast<qint64>(pe.th32ProcessID));
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pids.join(QLatin1Char(','));
}
#endif

static void appendVbsKillHostProcesses(QTextStream& vbs) {
    vbs << "Sub KillHostProcesses\r\n";
    vbs << "  Dim pidArr, p, nKilled\r\n";
    vbs << "  nKilled = 0\r\n";
    vbs << "  pidArr = Split(killPids, \",\")\r\n";
    vbs << "  For Each p In pidArr\r\n";
    vbs << "    p = Trim(p)\r\n";
    vbs << "    If Len(p) > 0 Then\r\n";
    vbs << "      sh.Run \"taskkill /F /PID \" & p & \" /T\", 0, False\r\n";
    vbs << "      nKilled = nKilled + 1\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "  If nKilled = 0 Then\r\n";
    vbs << "    sh.Run \"taskkill /F /IM \"\"\" & exeName & \"\"\" /T\", 0, False\r\n";
    vbs << "  End If\r\n";
    vbs << "End Sub\r\n";
}

static void appendVbsIsProcessAlive(QTextStream& vbs) {
    vbs << "Function IsProcessAlive\r\n";
    vbs << "  Dim exec, out\r\n";
    vbs << "  IsProcessAlive = False\r\n";
    vbs << "  Set exec = sh.Exec(\"cmd /c tasklist /NH /FI \"\"IMAGENAME eq \" & exeName & \"\"\"\")\r\n";
    vbs << "  Do While exec.Status = 0\r\n";
    vbs << "    WScript.Sleep 30\r\n";
    vbs << "  Loop\r\n";
    vbs << "  out = exec.StdOut.ReadAll\r\n";
    vbs << "  IsProcessAlive = (InStr(1, out, exeName, vbTextCompare) > 0)\r\n";
    vbs << "End Function\r\n";
}

struct RollbackFileEntry {
    QString backupPath;
    QString targetPath;
};

static bool isUnderUpdatesTree(const QString& nativePath) {
    const QString norm = QDir::fromNativeSeparators(nativePath).toLower();
    return norm.contains(QStringLiteral("/updates/")) || norm.endsWith(QStringLiteral("/updates"));
}

static QList<RollbackFileEntry> collectRollbackFiles(const QString& appDir) {
    QMap<QString, RollbackFileEntry> byTargetLower;
    const QString backupRoot = QDir(appDir).filePath(QStringLiteral("updates/backup"));
    if (QDir(backupRoot).exists()) {
        QDirIterator backupIt(backupRoot, QDir::Files, QDirIterator::Subdirectories);
        while (backupIt.hasNext()) {
            const QString backupPath = backupIt.next();
            const QString rel = QDir(backupRoot).relativeFilePath(backupPath);
            if (rel.isEmpty()) {
                continue;
            }
            const QString targetPath = QDir(appDir).filePath(rel);
            byTargetLower.insert(targetPath.toLower(), {backupPath, targetPath});
        }
    }

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
        byTargetLower.insert(targetPath.toLower(), {backupPath, targetPath});
    }
    return byTargetLower.values();
}

static bool launchVbsAndExit(const QString& vbsFileName) {
    const QString appDir = QCoreApplication::applicationDirPath();
    bool launched = false;
#ifdef Q_OS_WIN
    {
        const QString params = QStringLiteral("//B //Nologo \"%1\"").arg(QDir::toNativeSeparators(vbsFileName));
        const std::wstring paramsW = params.toStdWString();
        const std::wstring dirW = QDir::toNativeSeparators(appDir).toStdWString();
        SHELLEXECUTEINFOW sei;
        ZeroMemory(&sei, sizeof(sei));
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = L"open";
        sei.lpFile = L"wscript.exe";
        sei.lpParameters = paramsW.c_str();
        sei.lpDirectory = dirW.c_str();
        sei.nShow = SW_HIDE;
        if (ShellExecuteExW(&sei)) {
            launched = true;
            if (sei.hProcess)
                CloseHandle(sei.hProcess);
        }
    }
#endif
    if (!launched) {
        QProcess helper;
        helper.setProgram(QStringLiteral("wscript.exe"));
        helper.setArguments(
            {QStringLiteral("//B"), QStringLiteral("//Nologo"), QDir::toNativeSeparators(vbsFileName)});
        helper.setWorkingDirectory(appDir);
#ifdef Q_OS_WIN
        helper.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
            args->flags |= CREATE_NO_WINDOW | CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS;
            args->flags |= CREATE_BREAKAWAY_FROM_JOB;
            args->inheritHandles = false;
        });
#endif
        if (!helper.startDetached())
            return false;
    }

#ifdef Q_OS_WIN
    // 给 wscript 一点启动时间后立刻硬杀本进程，跳过 Qt 缓慢退场
    Sleep(80);
    TerminateProcess(GetCurrentProcess(), 0);
#endif
    QTimer::singleShot(0, []() { qApp->quit(); });
    return true;
}

// 增量清单里的单个文件：path 为相对部署根目录（applicationDirPath()）的相对路径
struct OtaFileSpec {
    QString path;
    QString sha256;
    qint64 size = 0;
};

static void writeRollbackInfoJson(const QString& appDir, const QList<OtaFileSpec>& changed) {
    QJsonObject root;
    root.insert(QStringLiteral("createdAt"),
               QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    QJsonArray files;
    for (const OtaFileSpec& spec : changed) {
        if (!spec.path.isEmpty()) {
            files.append(spec.path);
        }
    }
    root.insert(QStringLiteral("files"), files);
    const QString infoPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.json"));
    QDir().mkpath(QFileInfo(infoPath).absolutePath());
    QFile infoFile(infoPath);
    if (infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        infoFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

static QString readLastRollbackInfoSummary(const QString& appDir) {
    const QString infoPath = QDir(appDir).filePath(QStringLiteral("updates/rollback_info.json"));
    QFile infoFile(infoPath);
    if (!infoFile.open(QIODevice::ReadOnly)) {
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(infoFile.readAll()).object();
    const QString createdAt = root.value(QStringLiteral("createdAt")).toString().trimmed();
    const int fileCount = root.value(QStringLiteral("files")).toArray().size();
    if (createdAt.isEmpty() && fileCount <= 0) {
        return {};
    }
    return QStringLiteral("上次升级时间：%1，备份文件数：%2").arg(createdAt.isEmpty() ? QStringLiteral("未知") : createdAt)
        .arg(fileCount);
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

// 去掉 manifest 路径上连续的公共顶层目录（zip 允许多层套文件夹，最终与 appDir 相对路径对齐）
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
        OtaFileSpec spec;
        spec.path = path;
        spec.sha256 = o.value(QStringLiteral("sha256")).toString().toLower();
        spec.size = static_cast<qint64>(o.value(QStringLiteral("size")).toDouble());
        files.append(spec);
    }
    return normalizeManifestPaths(files);
}

// 服务端环境清单 vs 本地磁盘：逐文件算本地 sha256，只挑缺失或内容不一致的文件（v1 不删本地多余文件）
static QList<OtaFileSpec> diffLocalFiles(const QList<OtaFileSpec>& serverFiles, const QString& appDir) {
    QList<OtaFileSpec> changed;
    for (const OtaFileSpec& f : serverFiles) {
        const QString localSha = sha256File(QDir(appDir).filePath(f.path));
        if (localSha.isEmpty() || localSha.compare(f.sha256, Qt::CaseInsensitive) != 0) {
            changed.append(f);
        }
    }
    return changed;
}

static bool downloadDeltaFiles(const QList<OtaFileSpec>& files, const QString& stagingDir, QString* err) {
    for (const OtaFileSpec& f : files) {
        const QString target = QDir(stagingDir).filePath(f.path);
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

static void showRestartCountdown(QWidget* parent) {
    if (!parent) {
        return;
    }
    QMessageBox* msgBox = new QMessageBox(parent);
    msgBox->setWindowTitle(QStringLiteral("软件更新"));
    msgBox->setStandardButtons(QMessageBox::Ok);
    msgBox->setDefaultButton(QMessageBox::Ok);
    // 点确定立即继续；未点则倒计时结束后自动关闭
    int remainSec = 1;
    auto refreshText = [msgBox, &remainSec]() {
        msgBox->setText(QStringLiteral("下载完成，即将重启并安装新版本。\n\n"
                                       "点击「确定」立即重启；%1 秒后自动关闭。")
                            .arg(remainSec));
    };
    refreshText();
    QTimer* countdown = new QTimer(msgBox);
    countdown->setInterval(1000);
    QObject::connect(countdown, &QTimer::timeout, msgBox, [msgBox, countdown, &remainSec, refreshText]() {
        --remainSec;
        if (remainSec <= 0) {
            countdown->stop();
            msgBox->accept();
            return;
        }
        refreshText();
    });
    countdown->start();
    msgBox->exec();
    countdown->stop();
    msgBox->deleteLater();
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

static bool startOtaReplaceBat(const QList<OtaFileSpec>& changed, const QString& updatesDir) {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QList<OtaFileSpec> orderedChanged = orderOtaReplaceFiles(changed, exeName);
    const QString stagingDir = QDir(updatesDir).filePath(QStringLiteral("staging"));
    const QString backupDir = QDir(updatesDir).filePath(QStringLiteral("backup"));
    const QString vbsFileName = QDir(appDir).filePath(QStringLiteral("ota_replace.vbs"));
    const QString logPath = QDir(appDir).filePath(QStringLiteral("ota_replace.log"));
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    const QString oldPid = QString::number(QCoreApplication::applicationPid());
    const int n = orderedChanged.size();
    QElapsedTimer hostTimer;
    hostTimer.start();
    {
        QFile hostLog(logPath);
        if (hostLog.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&hostLog);
            const auto hostLine = [&](const QString& msg) {
                out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                    << QStringLiteral(" [host +") << hostTimer.elapsed() << QStringLiteral("ms] ") << msg
                    << "\r\n";
            };
            hostLine(QStringLiteral("OTA replace plan files=%1 pid=%2").arg(n).arg(oldPid));
            for (const OtaFileSpec& spec : orderedChanged) {
                hostLine(QStringLiteral("  file: ") + spec.path);
            }
        }
    }
    const auto appendHostLog = [&](const QString& msg) {
        QFile hostLog(logPath);
        if (hostLog.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&hostLog);
            out << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                << QStringLiteral(" [host +") << hostTimer.elapsed() << QStringLiteral("ms] ") << msg << "\r\n";
        }
    };

    auto vbsQuote = [](const QString& path) {
        return QDir::toNativeSeparators(path).replace(QLatin1Char('"'), QStringLiteral("\"\""));
    };
    const QString dirV = vbsQuote(appDir);
    const QString exeV = vbsQuote(exeName);
    const QString logV = vbsQuote(logPath);
    const QString backupDirV = vbsQuote(backupDir);
    const QString vbsSelfV = vbsQuote(vbsFileName);
#ifdef Q_OS_WIN
    const QString killPidsCsv = collectMatchingProcessPidsCsv(exeName);
#else
    const QString killPidsCsv;
#endif

    QFile vbsFile(vbsFileName);
    if (!vbsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream vbs(&vbsFile);
    vbs << "On Error Resume Next\r\n";
    vbs << "Set sh = CreateObject(\"WScript.Shell\")\r\n";
    vbs << "Set fso = CreateObject(\"Scripting.FileSystemObject\")\r\n";
    vbs << "sh.CurrentDirectory = \"" << dirV << "\"\r\n";
    vbs << "Dim pid, exeName, logPath, backupDir, vbsPath, killPids\r\n";
    vbs << "pid = \"" << oldPid << "\"\r\n";
    vbs << "exeName = \"" << exeV << "\"\r\n";
    vbs << "logPath = \"" << logV << "\"\r\n";
    vbs << "backupDir = \"" << backupDirV << "\"\r\n";
    vbs << "vbsPath = \"" << vbsSelfV << "\"\r\n";
    vbs << "killPids = \"" << killPidsCsv << "\"\r\n";
    vbs << "Sub EnsureDir(p)\r\n";
    vbs << "  Dim parts, cur, j\r\n";
    vbs << "  parts = Split(p, \"\\\")\r\n";
    vbs << "  cur = parts(0)\r\n";
    vbs << "  For j = 1 To UBound(parts)\r\n";
    vbs << "    cur = cur & \"\\\" & parts(j)\r\n";
    vbs << "    If Not fso.FolderExists(cur) Then fso.CreateFolder(cur)\r\n";
    vbs << "  Next\r\n";
    vbs << "End Sub\r\n";
    vbs << "Dim tStart, logf\r\n";
    vbs << "tStart = Timer\r\n";
    vbs << "Sub LogStep(msg)\r\n";
    vbs << "  Dim dt\r\n";
    vbs << "  dt = Timer - tStart\r\n";
    vbs << "  If dt < 0 Then dt = dt + 86400\r\n";
    vbs << "  logf.WriteLine Now & \" [vbs +\" & Round(dt, 3) & \"s] \" & msg\r\n";
    vbs << "End Sub\r\n";
    vbs << "Dim nFiles\r\n";
    vbs << "nFiles = " << n << "\r\n";
    vbs << "EnsureDir fso.GetParentFolderName(logPath)\r\n";
    vbs << "Set logf = fso.OpenTextFile(logPath, 8, True)\r\n";
    vbs << "LogStep \"vbs start pid=\" & pid & \" nFiles=\" & nFiles\r\n";
    vbs << "Dim staging(), target(), backup(), relPath()\r\n";
    vbs << "ReDim staging(nFiles - 1)\r\n";
    vbs << "ReDim target(nFiles - 1)\r\n";
    vbs << "ReDim backup(nFiles - 1)\r\n";
    vbs << "ReDim relPath(nFiles - 1)\r\n";
    for (int i = 0; i < n; ++i) {
        const QString rel = orderedChanged.at(i).path;
        vbs << "relPath(" << i << ") = \"" << vbsQuote(rel) << "\"\r\n";
        vbs << "staging(" << i << ") = \"" << vbsQuote(QDir(stagingDir).filePath(rel)) << "\"\r\n";
        vbs << "target(" << i << ") = \"" << vbsQuote(QDir(appDir).filePath(rel)) << "\"\r\n";
        vbs << "backup(" << i << ") = \"" << vbsQuote(QDir(backupDir).filePath(rel)) << "\"\r\n";
    }

    vbs << "Sub RollbackAll\r\n";
    vbs << "  Dim k\r\n";
    vbs << "  For k = 0 To nFiles - 1\r\n";
    vbs << "    If fso.FileExists(backup(k)) Then\r\n";
    vbs << "      EnsureDir fso.GetParentFolderName(target(k))\r\n";
    vbs << "      If fso.FileExists(target(k)) Then fso.DeleteFile target(k), True\r\n";
    vbs << "      fso.MoveFile backup(k), target(k)\r\n";
    vbs << "    End If\r\n";
    vbs << "  Next\r\n";
    vbs << "  logf.WriteLine \"rollback done\"\r\n";
    vbs << "End Sub\r\n";

    // host 已 TerminateProcess；VBS 按枚举 PID 异步 taskkill，避免 /IM 同步等待十几秒。
    appendVbsKillHostProcesses(vbs);
    appendVbsIsProcessAlive(vbs);
    vbs << "LogStep \"taskkill begin pids=\" & killPids\r\n";
    vbs << "KillHostProcesses\r\n";
    vbs << "LogStep \"taskkill async sent, sleep 400ms\"\r\n";
    vbs << "WScript.Sleep 400\r\n";
    vbs << "Dim i, okMove, retry, hc, alive\r\n";
    vbs << "For i = 0 To nFiles - 1\r\n";
    vbs << "  LogStep \"file \" & (i + 1) & \"/\" & nFiles & \" begin \" & relPath(i)\r\n";
    vbs << "  EnsureDir fso.GetParentFolderName(backup(i))\r\n";
    vbs << "  If fso.FileExists(backup(i)) Then fso.DeleteFile backup(i), True\r\n";
    vbs << "  If fso.FileExists(target(i)) Then\r\n";
    vbs << "    okMove = False\r\n";
    vbs << "    For retry = 1 To 12\r\n";
    vbs << "      Err.Clear\r\n";
    vbs << "      fso.MoveFile target(i), backup(i)\r\n";
    vbs << "      If Err.Number = 0 Then\r\n";
    vbs << "        okMove = True\r\n";
    vbs << "        Exit For\r\n";
    vbs << "      End If\r\n";
    vbs << "      If retry = 5 Or retry = 10 Then\r\n";
    vbs << "        LogStep \"file \" & relPath(i) & \" backup retry=\" & retry & \" err=\" & Err.Number\r\n";
    vbs << "        KillHostProcesses\r\n";
    vbs << "      End If\r\n";
    vbs << "      WScript.Sleep 100\r\n";
    vbs << "    Next\r\n";
    vbs << "    If Not okMove Then\r\n";
    vbs << "      LogStep \"move target to backup FAILED \" & target(i)\r\n";
    vbs << "      logf.Close\r\n";
    vbs << "      If fso.FileExists(vbsPath) Then fso.DeleteFile vbsPath, True\r\n";
    vbs << "      WScript.Quit 2\r\n";
    vbs << "    End If\r\n";
    vbs << "    LogStep \"file \" & relPath(i) & \" backup ok retry=\" & retry\r\n";
    vbs << "  Else\r\n";
    vbs << "    LogStep \"file \" & relPath(i) & \" backup skip (target missing)\"\r\n";
    vbs << "  End If\r\n";
    vbs << "  EnsureDir fso.GetParentFolderName(target(i))\r\n";
    vbs << "  okMove = False\r\n";
    vbs << "  For retry = 1 To 10\r\n";
    vbs << "    Err.Clear\r\n";
    vbs << "    fso.MoveFile staging(i), target(i)\r\n";
    vbs << "    If Err.Number = 0 Then\r\n";
    vbs << "      okMove = True\r\n";
    vbs << "      Exit For\r\n";
    vbs << "    End If\r\n";
    vbs << "    If retry = 1 Or retry = 5 Or retry = 10 Then\r\n";
    vbs << "      LogStep \"file \" & relPath(i) & \" replace retry=\" & retry & \" err=\" & Err.Number\r\n";
    vbs << "    End If\r\n";
    vbs << "    WScript.Sleep 80\r\n";
    vbs << "  Next\r\n";
    vbs << "  If Not okMove Then\r\n";
    vbs << "    LogStep \"move staging to target FAILED \" & target(i)\r\n";
    vbs << "    RollbackAll\r\n";
    vbs << "    logf.Close\r\n";
    vbs << "    If fso.FileExists(vbsPath) Then fso.DeleteFile vbsPath, True\r\n";
    vbs << "    WScript.Quit 3\r\n";
    vbs << "  End If\r\n";
    vbs << "  LogStep \"file \" & relPath(i) & \" replace ok retry=\" & retry\r\n";
    vbs << "Next\r\n";
    vbs << "LogStep \"all files replaced, launching exe\"\r\n";

    // 先替换完 dll 再换 exe；tasklist 轮询比 WMI 首次连接快一个数量级
    vbs << "sh.Run \"\"\"\" & sh.CurrentDirectory & \"\\\" & exeName & \"\"\"\", 1, False\r\n";
    vbs << "LogStep \"exe launch requested\"\r\n";
    vbs << "alive = False\r\n";
    vbs << "For hc = 1 To 12\r\n";
    vbs << "  WScript.Sleep 300\r\n";
    vbs << "  If IsProcessAlive Then\r\n";
    vbs << "    alive = True\r\n";
    vbs << "    LogStep \"health check ok poll=\" & hc\r\n";
    vbs << "    Exit For\r\n";
    vbs << "  End If\r\n";
    vbs << "  If hc = 1 Or hc = 4 Or hc = 8 Or hc = 12 Then LogStep \"health check waiting poll=\" & hc\r\n";
    vbs << "Next\r\n";
    vbs << "If alive Then\r\n";
    vbs << "  LogStep \"health check ok, keep backup for manual rollback\"\r\n";
    vbs << "Else\r\n";
    vbs << "  LogStep \"health check failed, rolling back\"\r\n";
    vbs << "  RollbackAll\r\n";
    vbs << "  sh.Run \"\"\"\" & sh.CurrentDirectory & \"\\\" & exeName & \"\"\"\", 1, False\r\n";
    vbs << "End If\r\n";
    vbs << "LogStep \"vbs finished\"\r\n";
    vbs << "logf.Close\r\n";
    vbs << "If fso.FileExists(vbsPath) Then fso.DeleteFile vbsPath, True\r\n";
    vbs << "WScript.Quit 0\r\n";
    vbsFile.close();

    writeRollbackInfoJson(appDir, changed);
    appendHostLog(QStringLiteral("vbs script written bytes=%1").arg(QFileInfo(vbsFileName).size()));
    ApplicationShutdown::prepareForOtaReplace();
    appendHostLog(QStringLiteral("prepareForOtaReplace done"));
    appendHostLog(QStringLiteral("launching wscript"));
    return launchVbsAndExit(vbsFileName);
}

static bool startOtaRollbackVbs(const QList<RollbackFileEntry>& entries) {
    if (entries.isEmpty()) {
        return false;
    }
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString vbsFileName = QDir(appDir).filePath(QStringLiteral("ota_rollback.vbs"));
    const QString logPath =
        QDir(appDir).filePath(QStringLiteral("所有log/上位机log/ota_rollback.log"));
    QDir().mkpath(QFileInfo(logPath).absolutePath());
    const QString oldPid = QString::number(QCoreApplication::applicationPid());

    auto vbsQuote = [](const QString& path) {
        return QDir::toNativeSeparators(path).replace(QLatin1Char('"'), QStringLiteral("\"\""));
    };
    const QString dirV = vbsQuote(appDir);
    const QString exeV = vbsQuote(exeName);
    const QString logV = vbsQuote(logPath);
    const QString vbsSelfV = vbsQuote(vbsFileName);
#ifdef Q_OS_WIN
    const QString killPidsCsv = collectMatchingProcessPidsCsv(exeName);
#else
    const QString killPidsCsv;
#endif

    QFile vbsFile(vbsFileName);
    if (!vbsFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream vbs(&vbsFile);
    vbs << "On Error Resume Next\r\n";
    vbs << "Set sh = CreateObject(\"WScript.Shell\")\r\n";
    vbs << "Set fso = CreateObject(\"Scripting.FileSystemObject\")\r\n";
    vbs << "sh.CurrentDirectory = \"" << dirV << "\"\r\n";
    vbs << "Dim pid, exeName, logPath, vbsPath, killPids\r\n";
    vbs << "pid = \"" << oldPid << "\"\r\n";
    vbs << "exeName = \"" << exeV << "\"\r\n";
    vbs << "logPath = \"" << logV << "\"\r\n";
    vbs << "vbsPath = \"" << vbsSelfV << "\"\r\n";
    vbs << "killPids = \"" << killPidsCsv << "\"\r\n";
    vbs << "Sub EnsureDir(p)\r\n";
    vbs << "  Dim parts, cur, j\r\n";
    vbs << "  parts = Split(p, \"\\\")\r\n";
    vbs << "  cur = parts(0)\r\n";
    vbs << "  For j = 1 To UBound(parts)\r\n";
    vbs << "    cur = cur & \"\\\" & parts(j)\r\n";
    vbs << "    If Not fso.FolderExists(cur) Then fso.CreateFolder(cur)\r\n";
    vbs << "  Next\r\n";
    vbs << "End Sub\r\n";
    vbs << "EnsureDir fso.GetParentFolderName(logPath)\r\n";
    vbs << "Set logf = fso.CreateTextFile(logPath, True)\r\n";
    vbs << "logf.WriteLine Now & \" OTA rollback start pid=\" & pid\r\n";

    const int n = entries.size();
    vbs << "Dim nFiles\r\n";
    vbs << "nFiles = " << n << "\r\n";
    vbs << "Dim backup(), target()\r\n";
    vbs << "ReDim backup(nFiles - 1)\r\n";
    vbs << "ReDim target(nFiles - 1)\r\n";
    for (int i = 0; i < n; ++i) {
        vbs << "backup(" << i << ") = \"" << vbsQuote(entries.at(i).backupPath) << "\"\r\n";
        vbs << "target(" << i << ") = \"" << vbsQuote(entries.at(i).targetPath) << "\"\r\n";
    }

    appendVbsKillHostProcesses(vbs);
    vbs << "KillHostProcesses\r\n";
    vbs << "WScript.Sleep 400\r\n";
    vbs << "Dim i, okMove, retry\r\n";
    vbs << "For i = 0 To nFiles - 1\r\n";
    vbs << "  If fso.FileExists(backup(i)) Then\r\n";
    vbs << "    EnsureDir fso.GetParentFolderName(target(i))\r\n";
    vbs << "    If fso.FileExists(target(i)) Then fso.DeleteFile target(i), True\r\n";
    vbs << "    okMove = False\r\n";
    vbs << "    For retry = 1 To 40\r\n";
    vbs << "      Err.Clear\r\n";
    vbs << "      fso.MoveFile backup(i), target(i)\r\n";
    vbs << "      If Err.Number = 0 Then\r\n";
    vbs << "        okMove = True\r\n";
    vbs << "        Exit For\r\n";
    vbs << "      End If\r\n";
    vbs << "      KillHostProcesses\r\n";
    vbs << "      WScript.Sleep 150\r\n";
    vbs << "    Next\r\n";
    vbs << "    If okMove Then\r\n";
    vbs << "      logf.WriteLine \"restored: \" & target(i)\r\n";
    vbs << "    Else\r\n";
    vbs << "      logf.WriteLine \"restore failed: \" & target(i)\r\n";
    vbs << "    End If\r\n";
    vbs << "  End If\r\n";
    vbs << "Next\r\n";
    vbs << "sh.Run \"\"\"\" & sh.CurrentDirectory & \"\\\" & exeName & \"\"\"\", 1, False\r\n";
    vbs << "logf.WriteLine \"rollback finished\"\r\n";
    vbs << "logf.Close\r\n";
    vbs << "If fso.FileExists(vbsPath) Then fso.DeleteFile vbsPath, True\r\n";
    vbs << "WScript.Quit 0\r\n";
    vbsFile.close();

    return launchVbsAndExit(vbsFileName);
}

bool HostOtaService::hasPreviousVersionBackup() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QList<RollbackFileEntry> entries = collectRollbackFiles(appDir);
    for (const RollbackFileEntry& entry : entries) {
        if (QFileInfo::exists(entry.backupPath)) {
            return true;
        }
    }
    return false;
}

bool HostOtaService::rollbackToPreviousVersion(QWidget* parent, QString* message) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QList<RollbackFileEntry> entries;
    const QList<RollbackFileEntry> collected = collectRollbackFiles(appDir);
    for (const RollbackFileEntry& entry : collected) {
        if (QFileInfo::exists(entry.backupPath)) {
            entries.append(entry);
        }
    }
    if (entries.isEmpty()) {
        if (message) {
            *message = QStringLiteral("未找到可回退的备份（updates/backup 或 *.bak）");
        }
        if (parent) {
            QMessageBox::information(parent, QStringLiteral("回退上一版本"), message ? *message : QString());
        }
        return false;
    }

    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString summary = readLastRollbackInfoSummary(appDir);
    QString detail = QStringLiteral("将把 %1 个文件还原为升级前版本（含 %2）。\n\n确认后程序将退出并重启。")
                         .arg(entries.size())
                         .arg(exeName);
    if (!summary.isEmpty()) {
        detail = summary + QStringLiteral("\n\n") + detail;
    }
    if (parent) {
        if (QMessageBox::question(parent, QStringLiteral("回退上一版本"), detail, QMessageBox::Yes | QMessageBox::No,
                                  QMessageBox::No) != QMessageBox::Yes) {
            if (message) {
                *message = QStringLiteral("用户取消回退");
            }
            return false;
        }
    }

    if (!startOtaRollbackVbs(entries)) {
        if (message) {
            *message = QStringLiteral("无法启动回退脚本");
        }
        if (parent) {
            QMessageBox::warning(parent, QStringLiteral("回退上一版本"), message ? *message : QString());
        }
        return false;
    }
    if (message) {
        *message = QStringLiteral("正在回退并重启…");
    }
    return true;
}

void HostOtaService::cleanupStaleBackupProcess() {
    const QString bakName = QFileInfo(QCoreApplication::applicationFilePath()).fileName() + QStringLiteral(".bak");
    // 无残留时 taskkill 会往控制台打「没有找到进程」，启动时很碍眼：静默执行即可
    auto* proc = new QProcess(qApp);
    proc->setProgram(QStringLiteral("taskkill.exe"));
    proc->setArguments({QStringLiteral("/F"), QStringLiteral("/T"), QStringLiteral("/IM"), bakName});
    proc->setStandardOutputFile(QProcess::nullDevice());
    proc->setStandardErrorFile(QProcess::nullDevice());
#ifdef Q_OS_WIN
    proc->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif
    QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), proc, &QObject::deleteLater);
    proc->start();
}

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
    if (!info.hasUpdate) {
        if (message) {
            *message = QStringLiteral("无可用更新");
        }
        return false;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString updatesDir = QDir(appDir).filePath(QStringLiteral("updates"));
    QDir().mkpath(updatesDir);

    // 清理并重建 staging 目录，避免残留旧文件被误用
    const QString stagingDir = QDir(updatesDir).filePath(QStringLiteral("staging"));
    QDir(stagingDir).removeRecursively();
    QDir().mkpath(stagingDir);

    // 1. 下载新 exe 到 staging（固定覆盖当前运行的 exe 名，TARGET=new_production）
    const QString exeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString exeStagingPath = QDir(stagingDir).filePath(exeName);
    QString downloadError;
    const QString url = info.downloadUrl.trimmed();
    if (url.isEmpty()) {
        QUrlQuery downloadQuery;
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
    if (!info.sha256.isEmpty() && exeSha.compare(info.sha256, Qt::CaseInsensitive) != 0) {
        qDebug() << "[OTA] exe sha256 不匹配: 期望=" << info.sha256 << "实际=" << exeSha;
        QFile::remove(exeStagingPath);
        if (message) {
            *message = QStringLiteral("exe sha256 校验失败");
        }
        return false;
    }

    // 2. 拉远端环境清单，与本地磁盘逐文件算 sha256 做差集，只下缺失/变化的 dll 等文件
    QString envErr;
    const QList<OtaFileSpec> envFiles = fetchEnvManifest(&envErr);
    if (!envErr.isEmpty()) {
        if (message) {
            *message = envErr;
        }
        return false;
    }
    const QList<OtaFileSpec> changedEnv = diffLocalFiles(envFiles, appDir);
    if (!downloadDeltaFiles(changedEnv, stagingDir, &downloadError)) {
        if (message) {
            *message = downloadError;
        }
        return false;
    }

    // 3. 组合替换清单：exe 打头 + 差异环境文件（exe 由版本管理单独管，不计入环境清单）
    QList<OtaFileSpec> changed;
    OtaFileSpec exeSpec;
    exeSpec.path = exeName;
    exeSpec.sha256 = exeSha;
    changed.append(changedEnv);
    changed.append(exeSpec);

    // 版本号来自 host_ota_version.h 编译进新包，无需写 settings

    showRestartCountdown(parent);
    if (!startOtaReplaceBat(changed, updatesDir)) {
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

    // 弹出版本选择列表：鼠标悬停每个版本项即显示该版本的修改说明
    QDialog dialog(parent);
    dialog.setWindowTitle(QStringLiteral("选择版本"));
    auto* pickerLayout = new QVBoxLayout(&dialog);
    pickerLayout->addWidget(new QLabel(QStringLiteral("请选择要升级的版本（鼠标悬停可查看修改内容）："), &dialog));

    auto* versionList = new QListWidget(&dialog);
    versionList->setSelectionMode(QAbstractItemView::SingleSelection);
    versionList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    for (int i = 0; i < items.size(); ++i) {
        const QJsonObject v = items.at(i).toObject();
        const QString ver = v.value(QStringLiteral("appVersion")).toString();
        const QString bid = v.value(QStringLiteral("buildId")).toString();
        const QString time = v.value(QStringLiteral("uploadedAt")).toString();
        const QString notes = v.value(QStringLiteral("releaseNotes")).toString().trimmed();
        QString label = QStringLiteral("%1 (buildId=%2)").arg(ver, bid);
        if (!time.isEmpty()) {
            label += QStringLiteral("  %1").arg(time);
        }
        auto* item = new QListWidgetItem(label, versionList);
        item->setToolTip(notes.isEmpty() ? QStringLiteral("（上传时未填写修改说明）") : notes);
        item->setData(Qt::UserRole, i);
    }
    versionList->setCurrentRow(0);
    pickerLayout->addWidget(versionList);

    auto* pickerButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(pickerButtons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(pickerButtons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    QObject::connect(versionList, &QListWidget::itemDoubleClicked, &dialog, &QDialog::accept);
    pickerLayout->addWidget(pickerButtons);
    dialog.resize(420, 320);

    if (dialog.exec() != QDialog::Accepted) {
        return true;
    }
    const int row = versionList->currentRow();
    if (row < 0 || row >= versionList->count()) {
        return true;
    }
    const int idx = versionList->item(row)->data(Qt::UserRole).toInt();
    const QJsonObject chosen = items.at(idx).toObject();
    CheckResult info;
    info.hasUpdate = true;
    info.appVersion = chosen.value(QStringLiteral("appVersion")).toString();
    info.buildId = chosen.value(QStringLiteral("buildId")).toString();
    info.sha256 = chosen.value(QStringLiteral("sha256")).toString();
    info.forceUpgrade = chosen.value(QStringLiteral("forceUpgrade")).toBool();
    info.releaseNotes = chosen.value(QStringLiteral("releaseNotes")).toString();
    info.packageName = chosen.value(QStringLiteral("packageName")).toString();
    info.packageKind = chosen.value(QStringLiteral("packageKind")).toString();
    info.fileCount = chosen.value(QStringLiteral("fileCount")).toInt();
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
