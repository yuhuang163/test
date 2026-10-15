#ifndef FACTORY_CLOUD_CLIENT_H
#define FACTORY_CLOUD_CLIENT_H

#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QUrlQuery>

class QNetworkRequest;

/** 路特产线管理平台 HTTP 客户端：BaseUrl、统一请求头、JSON 包络 */
class FactoryCloudClient {
  public:
    struct ApiResult {
        bool ok = false;
        int code = -1;
        QString message;
        QJsonObject data;
    };

    static QString baseUrl();
    static QString apiRoot();
    static QString deviceId();
    static QString stationKey();
    static QString appVersion();
    /** 写入本地 FactoryCloud/HostAppVersion；空串忽略 */
    static void setAppVersion(const QString& version);
    static QString buildId();
    static QString packageName();
    static QString accessToken();

    static void applyFactoryHeaders(QNetworkRequest& request, bool jsonContentType = true);

    /** timeoutMs < 0 时用默认 120s；开测门禁等短交互请传较小值，避免嵌套事件循环假死 */
    static ApiResult get(const QString& path, const QUrlQuery& query = QUrlQuery(), int timeoutMs = -1);
    static ApiResult post(const QString& path, const QJsonObject& body);
    static ApiResult uploadMultipart(const QString& path, const QString& zipPath,
                                     const QList<QPair<QString, QString>>& formFields = {});
    static ApiResult uploadExe(const QString& exePath,
                               const QList<QPair<QString, QString>>& formFields);
    static bool downloadToFile(const QString& path, const QUrlQuery& query, const QString& destPath,
                               QString* error);

    static QString defaultBaseUrl();
    static QString defaultLogUploadUrl();
};

#endif // FACTORY_CLOUD_CLIENT_H
