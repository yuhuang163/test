#ifndef HOST_OTA_VBS_H
#define HOST_OTA_VBS_H

#include <QList>
#include <QString>

/** 上位机 OTA 替换/回退用的 VBS 生成与 wscript 启动 */
class HostOtaVbs {
  public:
    struct ReplaceEntry {
        QString relPath;
        bool existedBefore = false;
    };
    struct BackupFile {
        QString backupPath;
        QString targetPath;
        bool existedBefore = true;
    };

    static bool writeReplaceScript(const QString& vbsFileName,
                                   const QString& appDir,
                                   const QString& exeName,
                                   const QString& logPath,
                                   const QString& stagingDir,
                                   const QString& backupDir,
                                   const QList<ReplaceEntry>& files);
    static bool writeRollbackScript(const QString& vbsFileName,
                                    const QString& appDir,
                                    const QString& exeName,
                                    const QString& logPath,
                                    const QList<BackupFile>& files);
    static bool launchAndExit(const QString& vbsFileName);
};

#endif // HOST_OTA_VBS_H
