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
    };

    static QString defaultLogRootPath();
    static QString defaultUploadUrl();
    static QString defaultDeviceId();

    /** 将 logRoot 目录压缩为 zip，返回 zip 路径；失败时 error 有说明 */
    static QString compressLogDirectory(const QString& logRoot, QString* error);

    /** 上传已打包的 zip */
    static bool uploadLogArchive(const QString& zipPath, const UploadConfig& cfg, QString* message,
                                 QString* responseBody = nullptr);

    /** 打包并上传；成功返回 true */
    static bool packAndUpload(const UploadConfig& cfg, QString* message);
};

#endif // LOG_UPLOAD_SERVICE_H
