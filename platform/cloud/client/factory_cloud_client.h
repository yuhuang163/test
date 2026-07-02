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
    static QString buildId();
    static QString packageName();
    static QString accessToken();

    static void applyFactoryHeaders(QNetworkRequest& request, bool jsonContentType = true);

    static ApiResult get(const QString& path, const QUrlQuery& query = QUrlQuery());
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
