#ifndef HOST_OTA_ROLLBACK_H
#define HOST_OTA_ROLLBACK_H
#include <QString>

class QWidget;

class HostOtaRollback {
  public:
    static bool hasPreviousVersionBackup();
    static bool rollbackToPreviousVersion(QWidget* parent, QString* message = nullptr);
    static void cleanupStaleBackupProcess();
};
#endif // HOST_OTA_ROLLBACK_H
