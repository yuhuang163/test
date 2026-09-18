#ifndef HOST_OTA_APPLY_H
#define HOST_OTA_APPLY_H

#include "host_ota_service.h"
#include "host_ota_types.h"

#include <QList>
#include <QString>

class QWidget;

/** 上位机 OTA 升级：校验/下载/清单差分/事务/替换 */
class HostOtaApply {
  public:
    static QString sha256File(const QString& path);
    static bool downloadAndApply(const HostOtaService::CheckResult& info,
                                 QWidget* parent,
                                 QString* message);
    static QList<RollbackFileEntry> collectRollbackFiles(const QString& appDir);
    static QString readLastRollbackInfoSummary(const QString& appDir);
};

#endif // HOST_OTA_APPLY_H
