#ifndef HOST_OTA_TYPES_H
#define HOST_OTA_TYPES_H

#include <QList>
#include <QString>

/** 增量清单里的单个文件：path 为相对部署根目录的相对路径 */
struct OtaFileSpec {
    QString path;
    QString sha256;
    qint64 size = 0;
    bool existedBefore = false;
};

struct RollbackFileEntry {
    QString backupPath;
    QString targetPath;
    bool existedBefore = true;
};

/** 一次 OTA 替换事务：staging / backup / 已排序替换清单 */
struct OtaTransaction {
    QString appDir;
    QString updatesDir;
    QString stagingDir;
    QString backupDir;
    QString txnId;
    QString logPath;
    QString vbsFileName;
    QString exeName;
    QString toVersion;
    QString toBuildId;
    QList<OtaFileSpec> orderedFiles;
};

#endif // HOST_OTA_TYPES_H
