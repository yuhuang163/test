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
        bool hostNewer = false;
        bool forceUpgrade = false;
        QString message;
        QString packageName;
        QString appVersion;
        QString buildId;
        QString downloadUrl;
        QString sha256;
        QString releaseNotes;
        QString uploadedAt;
    };

    static CheckResult checkUpdate();
    static bool downloadAndApply(const CheckResult& info, QWidget* parent, QString* message);

    /** 将当前运行中的 exe 上传至服务器（用于首次注册版本）。*/
    static bool uploadCurrentExe(QString* message = nullptr);

    /** 弹出版本选择对话框，让用户选择要升级的版本。*/
    static bool showVersionPicker(QWidget* parent,
                                  const std::function<void(const QString&)>& logFn = nullptr);

    /** BaseUrl 已配置且开启 OTA 时走云端；返回 true 表示已处理（含「已是最新」），false 表示应走旧版目录扫描 */
    static bool tryInteractiveUpdate(QWidget* parent,
                                     const std::function<void(const QString&)>& logFn = nullptr);
};

#endif // HOST_OTA_SERVICE_H
