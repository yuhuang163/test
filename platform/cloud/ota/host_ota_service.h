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
        QString packageKind;  // "exe"（单文件旧格式）| "manifest"（增量清单）
        int fileCount = 0;
    };

    static CheckResult checkUpdate();
    static bool downloadAndApply(const CheckResult& info, QWidget* parent, QString* message);
    /** 结束 OTA 改名后仍在跑的 exe.bak，避免关窗口后后台残留 */
    static void cleanupStaleBackupProcess();

    /** 将当前运行中的 exe 上传至服务器（用于首次注册版本）。releaseNotes 为本次修改说明。 */
    static bool uploadCurrentExe(QString* message = nullptr, const QString& releaseNotes = QString());

    /** 本机落盘当前版本更新说明（升级下载成功后 / 管理员上传成功后）。 */
    static void saveLocalReleaseNotes(const QString& appVersion, const QString& releaseNotes);
    /** 读取与当前运行版本匹配的更新说明；版本不一致或未存则返回空。 */
    static QString localReleaseNotesForCurrentVersion();

    /** 弹出版本选择对话框，让用户选择要升级的版本。*/
    static bool showVersionPicker(QWidget* parent,
                                  const std::function<void(const QString&)>& logFn = nullptr);

    /** 是否存在可回退的上一次升级备份（updates/backup 或运行目录 *.bak）。 */
    static bool hasPreviousVersionBackup();
    /** 将备份还原并重启上位机；成功时会退出当前进程。 */
    static bool rollbackToPreviousVersion(QWidget* parent, QString* message = nullptr);

    /** BaseUrl 已配置且开启 OTA 时走云端；返回 true 表示已处理（含「已是最新」），false 表示应走旧版目录扫描 */
    static bool tryInteractiveUpdate(QWidget* parent,
                                     const std::function<void(const QString&)>& logFn = nullptr);
};

#endif // HOST_OTA_SERVICE_H
