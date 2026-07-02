#include "factory_cloud_client.h"

#include "my_set/my_typedef.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHttpMultiPart>
#include <QHttpPart>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSslError>
#include <QSysInfo>
#include <QTimer>
#include <QUrl>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr const char* kDefaultBaseUrl = "https://fctp.luteos.com";
constexpr int kHttpTimeoutMs = 120000;

QString trimSlash(const QString& url) {
    QString s = url.trimmed();
    while (s.endsWith(QLatin1Char('/'))) {
        s.chop(1);
    }
    return s;
}

QString readSetting(const char* key, const QString& fallback = QString()) {
    const QString v = SETTINGS.value(QString::fromUtf8(key)).toString().trimmed();
    return v.isEmpty() ? fallback : v;
}

QString parseVersionFromMacro() {
    const QString macro = QString::fromUtf8(DEBUG_VER);
    const QRegularExpression re(QStringLiteral("V(\\d+\\.\\d+\\.\\d+)"));
    const QRegularExpressionMatch m = re.match(macro);
    if (m.hasMatch()) {
        return m.captured(1);
    }
    return QStringLiteral("0.0.0");
}

QString parseBuildIdFromExe() {
    const QString exe = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QRegularExpression re(QStringLiteral("new_production_(\\d{8})"));
    const QRegularExpressionMatch m = re.match(exe);
    if (m.hasMatch()) {
        return m.captured(1);
    }
    return QString();
}

FactoryCloudClient::ApiResult parseEnvelope(const QByteArray& body, int httpStatus, const QString& qtError) {
    FactoryCloudClient::ApiResult result;
    const QJsonDocument doc = QJsonDocument::fromJson(body);
    if (!doc.isObject()) {
        if (httpStatus == 404) {
            result.message =
                QStringLiteral("接口不存在（HTTP 404）。请确认 BaseUrl 正确且服务器已部署 factory-tool API；"
                               "本地联调请填 http://127.0.0.1:8800 并先运行「启动管理平台.bat」");
        } else {
            result.message = QStringLiteral("响应不是 JSON（HTTP %1）：%2").arg(httpStatus).arg(qtError);
        }
        return result;
    }
    const QJsonObject obj = doc.object();
    result.code = obj.value(QStringLiteral("code")).toInt(-1);
    result.message = obj.value(QStringLiteral("message")).toString();
    if (obj.value(QStringLiteral("data")).isObject()) {
        result.data = obj.value(QStringLiteral("data")).toObject();
    } else if (obj.value(QStringLiteral("data")).isArray()) {
        QJsonObject wrap;
        wrap.insert(QStringLiteral("items"), obj.value(QStringLiteral("data")));
        result.data = wrap;
    }
    result.ok = (result.code == 0) && (httpStatus >= 200 && httpStatus < 300);
    if (!result.ok && result.message.isEmpty()) {
        if (httpStatus == 404) {
            result.message =
                QStringLiteral("接口不存在（HTTP 404）。本地联调 BaseUrl 请填 http://127.0.0.1:8800");
        } else {
            result.message = QStringLiteral("请求失败 HTTP %1").arg(httpStatus);
        }
    }
    qDebug() << "自己服务器上传完成";
    return result;
}

QByteArray waitReply(QNetworkReply* reply, QString* qtError, int* httpStatus) {
    QNetworkAccessManager* manager = qobject_cast<QNetworkAccessManager*>(reply->manager());
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(kHttpTimeoutMs);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, QOverload<const QList<QSslError>&>::of(&QNetworkReply::sslErrors), reply,
                     QOverload<>::of(&QNetworkReply::ignoreSslErrors));
    timer.start();
    loop.exec();
    if (!reply->isFinished()) {
        reply->abort();
        if (qtError) {
            *qtError = QStringLiteral("请求超时");
        }
        if (httpStatus) {
            *httpStatus = 0;
        }
        return {};
    }
    if (httpStatus) {
        *httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    if (qtError) {
        *qtError = reply->errorString();
    }
    return reply->readAll();
}

} // namespace

QString FactoryCloudClient::defaultBaseUrl() {
    return QString::fromUtf8(kDefaultBaseUrl);
}

QString FactoryCloudClient::defaultLogUploadUrl() {
    const QString base = baseUrl();
    if (!base.isEmpty()) {
        return base + QStringLiteral("/api/factory-tool/logs/upload");
    }
    return trimSlash(QString::fromUtf8(kDefaultBaseUrl)) + QStringLiteral("/api/factory-tool/logs/upload");
}

QString FactoryCloudClient::baseUrl() {
    return trimSlash(readSetting("FactoryCloud/BaseUrl", defaultBaseUrl()));
}

QString FactoryCloudClient::apiRoot() {
    return baseUrl() + QStringLiteral("/api/factory-tool");
}

QString FactoryCloudClient::deviceId() {
    QString id = readSetting("FactoryCloud/DeviceId");
    if (id.isEmpty()) {
        id = QSysInfo::machineHostName();
    }
    return id;
}

QString FactoryCloudClient::stationKey() {
    QString key = readSetting("FactoryCloud/StationKey");
    if (key.isEmpty()) {
        // 云端工站授权与设置页「工站类型」一致（SYSTEM/station），勿用测试流程 SelectedStation
        key = SETTINGS.value(QStringLiteral("SYSTEM/station")).toString().trimmed();
    }
    if (key.isEmpty()) {
        key = SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString().trimmed();
    }
    if (key.isEmpty()) {
        key = QStringLiteral("DEFAULT");
    }
    return key;
}

QString FactoryCloudClient::appVersion() {
    return parseVersionFromMacro();
}

QString FactoryCloudClient::buildId() {
    return parseBuildIdFromExe();
}

QString FactoryCloudClient::packageName() {
    return readSetting("FactoryCloud/HostPackageName", QStringLiteral("new_production"));
}

QString FactoryCloudClient::accessToken() {
    return readSetting("FactoryCloud/Token");
}

void FactoryCloudClient::applyFactoryHeaders(QNetworkRequest& request, bool jsonContentType) {
    if (jsonContentType) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    }
    request.setRawHeader("Device-Id", deviceId().toUtf8());
    request.setRawHeader("Station-Key", stationKey().toUtf8());
    request.setRawHeader("APP-Version", appVersion().toUtf8());
    const QString bid = buildId();
    if (!bid.isEmpty()) {
        request.setRawHeader("X-Client-Build", bid.toUtf8());
    }
    const QString token = accessToken();
    if (!token.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + token.toUtf8());
    }
}

FactoryCloudClient::ApiResult FactoryCloudClient::get(const QString& path, const QUrlQuery& query) {
    QUrl url(apiRoot() + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    applyFactoryHeaders(request, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(kHttpTimeoutMs);
#endif
    QNetworkReply* reply = manager.get(request);
    QString qtError;
    int httpStatus = 0;
    const QByteArray body = waitReply(reply, &qtError, &httpStatus);
    const bool netOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!netOk && body.isEmpty()) {
        ApiResult r;
        r.message = QStringLiteral("网络错误：") + qtError;
        return r;
    }
    return parseEnvelope(body, httpStatus, qtError);
}

FactoryCloudClient::ApiResult FactoryCloudClient::post(const QString& path, const QJsonObject& body) {
    QUrl url(apiRoot() + path);
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    qDebug() << "自己服务器上传路径：" << url;
    applyFactoryHeaders(request, true);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(kHttpTimeoutMs);
#endif
    const QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = manager.post(request, payload);
    QString qtError;
    int httpStatus = 0;
    const QByteArray resp = waitReply(reply, &qtError, &httpStatus);
    const bool netOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!netOk && resp.isEmpty()) {
        ApiResult r;
        r.message = QStringLiteral("网络错误：") + qtError;
        return r;
    }
    return parseEnvelope(resp, httpStatus, qtError);
}

FactoryCloudClient::ApiResult FactoryCloudClient::uploadMultipart(
    const QString& path, const QString& zipPath, const QList<QPair<QString, QString>>& formFields) {
    ApiResult failResult;
    if (!QFile::exists(zipPath)) {
        failResult.message = QStringLiteral("压缩包不存在：") + zipPath;
        return failResult;
    }
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) {
        failResult.message = QStringLiteral("无法打开压缩包：") + zipPath;
        return failResult;
    }
    const QByteArray zipData = file.readAll();
    file.close();

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    for (const auto& field : formFields) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"%1\"").arg(field.first)));
        part.setBody(field.second.toUtf8());
        multiPart->append(part);
    }
    QHttpPart filePart;
    const QString fileName = QFileInfo(zipPath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant(QStringLiteral("application/zip")));
    filePart.setBody(zipData);
    multiPart->append(filePart);

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(apiRoot() + path));
    applyFactoryHeaders(request, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(kHttpTimeoutMs);
#endif
    QNetworkReply* reply = manager.post(request, multiPart);
    multiPart->setParent(reply);
    QObject::connect(reply, QOverload<const QList<QSslError>&>::of(&QNetworkReply::sslErrors), reply,
                     QOverload<>::of(&QNetworkReply::ignoreSslErrors));

    QString qtError;
    int httpStatus = 0;
    const QByteArray resp = waitReply(reply, &qtError, &httpStatus);
    const bool netOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!netOk && resp.isEmpty()) {
        failResult.message = QStringLiteral("网络错误：") + qtError;
        return failResult;
    }
    return parseEnvelope(resp, httpStatus, qtError);
}

FactoryCloudClient::ApiResult FactoryCloudClient::uploadExe(
    const QString& exePath, const QList<QPair<QString, QString>>& formFields) {
    ApiResult failResult;
    QFile file(exePath);
    if (!file.open(QIODevice::ReadOnly)) {
        failResult.message = QStringLiteral("无法打开 exe：") + exePath;
        return failResult;
    }
    const QByteArray exeData = file.readAll();
    file.close();

    QHttpMultiPart* multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);
    for (const auto& field : formFields) {
        QHttpPart part;
        part.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"%1\"").arg(field.first)));
        part.setBody(field.second.toUtf8());
        multiPart->append(part);
    }
    QHttpPart filePart;
    const QString fileName = QFileInfo(exePath).fileName();
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant(QStringLiteral("form-data; name=\"file\"; filename=\"%1\"").arg(fileName)));
    filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                       QVariant(QStringLiteral("application/octet-stream")));
    filePart.setBody(exeData);
    multiPart->append(filePart);

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(apiRoot() + QStringLiteral("/host-app/upload")));
    applyFactoryHeaders(request, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(kHttpTimeoutMs);
#endif
    QNetworkReply* reply = manager.post(request, multiPart);
    multiPart->setParent(reply);
    QObject::connect(reply, QOverload<const QList<QSslError>&>::of(&QNetworkReply::sslErrors), reply,
                     QOverload<>::of(&QNetworkReply::ignoreSslErrors));

    QString qtError;
    int httpStatus = 0;
    const QByteArray resp = waitReply(reply, &qtError, &httpStatus);
    const bool netOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!netOk && resp.isEmpty()) {
        failResult.message = QStringLiteral("上传 exe 网络错误：") + qtError;
        return failResult;
    }
    return parseEnvelope(resp, httpStatus, qtError);
}

bool FactoryCloudClient::downloadToFile(const QString& path, const QUrlQuery& query, const QString& destPath,
                                        QString* error) {
    QUrl url(path.startsWith(QStringLiteral("http")) ? path : (apiRoot() + path));
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    applyFactoryHeaders(request, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    request.setTransferTimeout(kHttpTimeoutMs);
#endif
    QNetworkReply* reply = manager.get(request);
    QString qtError;
    int httpStatus = 0;
    const QByteArray body = waitReply(reply, &qtError, &httpStatus);
    const bool netOk = reply->error() == QNetworkReply::NoError;
    reply->deleteLater();
    if (!netOk || httpStatus < 200 || httpStatus >= 300) {
        if (error) {
            *error = QStringLiteral("下载失败 HTTP %1：%2").arg(httpStatus).arg(qtError);
        }
        return false;
    }
    QFile file(destPath);
    if (!file.open(QIODevice::WriteOnly)) {
        if (error) {
            *error = QStringLiteral("无法写入：") + destPath;
        }
        return false;
    }
    file.write(body);
    file.close();
    return true;
}
