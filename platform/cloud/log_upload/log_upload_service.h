#ifndef LOG_UPLOAD_SERVICE_H
#define LOG_UPLOAD_SERVICE_H

#include "my_set/my_typedef.h"
#include "qlog.h"

#include <QString>

/** 打包会话日志并上传到 factory-tool 日志接口 */
class LogUploadService {
  public:
    struct UploadConfig {
        QString uploadUrl;
        QString deviceId;
        QString station;
        QString factoryName;
        QString hostName;
        QString sn;
        QString mac;
        QString testResult;
        QString clientVersion;
        int testRecordId = 0;
    };

    static QString defaultLogRootPath();
    static QString defaultUploadUrl();
    static QString defaultDeviceId();

    static bool configFromSettings(UploadConfig* cfg, QString* error = nullptr);
    static bool configFromPack(const MesPacketData& pack, UploadConfig* cfg, QString* error = nullptr);

    static bool isUploadEnabled();
    static bool isUploadOnTestCompleteEnabled();

    static QString compressLogDirectory(const QString& logRoot, QString* error, QString* warning = nullptr);
    static QString compressSessionArchive(const QlogSessionInfo& info, QString* error, QString* warning = nullptr);

    static bool uploadLogArchive(const QString& zipPath, const UploadConfig& cfg, QString* message,
                                 QString* responseBody = nullptr);

    static bool packAndUpload(const UploadConfig& cfg, QString* message);
    static bool uploadSessionFromPack(const MesPacketData& pack, int slot, int testRecordId, QString* message = nullptr);
};

#endif // LOG_UPLOAD_SERVICE_H
