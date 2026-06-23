#ifndef HOST_OTA_SERVICE_H
#define HOST_OTA_SERVICE_H

#include <functional>

#include <QObject>
#include <QString>

class QWidget;

/** 上位机 exe OTA：check / download / sha256 / 替换 */
class HostOtaService {
  public:
    struct CheckResult {
        bool ok = false;
        bool hasUpdate = false;
        bool forceUpgrade = false;
        QString message;
        QString packageName;
        QString appVersion;
        QString buildId;
        QString downloadUrl;
        QString sha256;
        QString releaseNotes;
    };

    static CheckResult checkUpdate();
    static bool downloadAndApply(const CheckResult& info, QWidget* parent, QString* message);

    /** BaseUrl 已配置且开启 OTA 时走云端；返回 true 表示已处理（含「已是最新」），false 表示应走旧版目录扫描 */
    static bool tryInteractiveUpdate(QWidget* parent,
                                     const std::function<void(const QString&)>& logFn = nullptr);
};

#endif // HOST_OTA_SERVICE_H
