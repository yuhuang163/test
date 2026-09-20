#include "host_ota_rollback.h"

#include "host_ota_apply.h"
#include "host_ota_vbs.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QObject>
#include <QProcess>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

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

    QList<HostOtaVbs::BackupFile> files;
    files.reserve(entries.size());
    for (const RollbackFileEntry& entry : entries) {
        files.append({entry.backupPath, entry.targetPath, entry.existedBefore});
    }
    if (!HostOtaVbs::writeRollbackScript(vbsFileName, appDir, exeName, logPath, files)) {
        return false;
    }
    return HostOtaVbs::launchAndExit(vbsFileName);
}

bool HostOtaRollback::hasPreviousVersionBackup() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QList<RollbackFileEntry> entries = HostOtaApply::collectRollbackFiles(appDir);
    for (const RollbackFileEntry& entry : entries) {
        if (entry.existedBefore && QFileInfo::exists(entry.backupPath)) {
            return true;
        }
        if (!entry.existedBefore && QFileInfo::exists(entry.targetPath)) {
            return true;
        }
    }
    return false;
}

bool HostOtaRollback::rollbackToPreviousVersion(QWidget* parent, QString* message) {
    const QString appDir = QCoreApplication::applicationDirPath();
    QList<RollbackFileEntry> entries;
    const QList<RollbackFileEntry> collected = HostOtaApply::collectRollbackFiles(appDir);
    for (const RollbackFileEntry& entry : collected) {
        if (entry.existedBefore && QFileInfo::exists(entry.backupPath)) {
            entries.append(entry);
        } else if (!entry.existedBefore && QFileInfo::exists(entry.targetPath)) {
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
    const QString summary = HostOtaApply::readLastRollbackInfoSummary(appDir);
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

void HostOtaRollback::cleanupStaleBackupProcess() {
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

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
