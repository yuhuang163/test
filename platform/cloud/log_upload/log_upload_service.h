#ifndef LOG_UPLOAD_SERVICE_H
#define LOG_UPLOAD_SERVICE_H

#include <QString>

/** 打包 所有log 目录并上传到 factory-tool 日志接口 */
class LogUploadService {
  public:
    struct UploadConfig {
        QString uploadUrl;
        QString deviceId;
        QString station;
        QString factoryName; // 工厂名，与 SETTINGS Mes/FACTORY 同源
        QString hostName;    // 电脑名，审计用
        QString sn;
        QString testResult;
        QString clientVersion;
    };

    static QString defaultLogRootPath();
    static QString defaultUploadUrl();
    static QString defaultDeviceId();

    /** 从 SETTINGS 组装上传配置；失败时 error 有说明 */
    static bool configFromSettings(UploadConfig* cfg, QString* error = nullptr);

    /** 日志上传功能是否启用（FactoryCloud/Feature/LogUpload，默认 true） */
    static bool isUploadEnabled();

    /** 将 logRoot 目录压缩为 zip，返回 zip 路径；失败时 error 有说明 */
    static QString compressLogDirectory(const QString& logRoot, QString* error, QString* warning = nullptr);

    /** 上传已打包的 zip */
    static bool uploadLogArchive(const QString& zipPath, const UploadConfig& cfg, QString* message,
                                 QString* responseBody = nullptr);

    /** 打包并上传；成功返回 true */
    static bool packAndUpload(const UploadConfig& cfg, QString* message);
};

#endif // LOG_UPLOAD_SERVICE_H
