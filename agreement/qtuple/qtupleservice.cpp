#include "qtupleservice.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QSysInfo>
#include <QDebug>

#include "my_set/my_typedef.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

QTupleService::QTupleService(const QString& baseUrl)
    : baseUrl_(baseUrl.isEmpty() ? SETTINGS.value("Tuple/BaseUrl", "http://192.168.200.140:8080").toString() : baseUrl) {}

bool QTupleService::login(const QString& userName, const QString& password, QString* error) {
    const QByteArray token = QString("%1:%2").arg(userName, password).toUtf8().toBase64();
    authHeader_ = "Basic " + token;

    QByteArray response;
    if (!requestGet("/api/mac-addresses/auth", QString(), &response, error)) {
        authHeader_.clear();
        return false;
    }
    qDebug().noquote() << "[Tuple] login response:" << QString::fromUtf8(response);
    return true;
}

TupleApplyResult QTupleService::applyTupleByMac(const QString& mac, const QString& sku, const QString& position) {
    TupleApplyResult result;
    qDebug().noquote() << "[Tuple] applyTupleByMac params:"
                       << "mac=" + mac
                       << "sku=" + sku
                       << "position=" + position;
    QUrlQuery query;
    query.addQueryItem("mac", mac);
    query.addQueryItem("sku", sku);
    query.addQueryItem("position", position);

    QByteArray response;
    if (!requestGet("/api/mac-addresses/applyTupleByMac", query.toString(QUrl::FullyEncoded), &response, &result.error)) {
        return result;
    }

    qDebug().noquote() << "[Tuple] applyTupleByMac response:" << QString::fromUtf8(response);
    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        result.error = "三元组响应不是 JSON 对象";
        return result;
    }

    const QJsonObject obj = doc.object();
    const QJsonObject dataObj = obj.value("data").isObject() ? obj.value("data").toObject() : obj;
    const int successCode = dataObj.contains("success") ? dataObj.value("success").toInt(-1) : obj.value("success").toInt(0);
    const int code = obj.value("code").toInt(200);
    if (code != 200 || successCode != 0) {
        result.error = obj.value("message").toString(obj.value("msg").toString("三元组接口返回失败"));
        return result;
    }

    result.mac = dataObj.value("mac").toString(mac);
    result.productKey = dataObj.value("productKey").toString();
    result.deviceName = dataObj.value("deviceName").toString();
    result.deviceSecret = dataObj.value("deviceSecret").toString();
    result.sn = dataObj.value("sn").toString();
    result.status = dataObj.value("status").toInt();
    result.availableCount = dataObj.value("availableCount").toInt();
    result.success = !result.productKey.isEmpty() && !result.deviceName.isEmpty() && !result.deviceSecret.isEmpty();
    if (!result.success) {
        result.error = "三元组字段缺失";
    }
    return result;
}

bool QTupleService::debugUpdateMacStatus(const QString& mac, int status, QString* error) {
    QJsonObject bodyObj;
    bodyObj.insert("mac", mac);
    bodyObj.insert("status", status);
    const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    QByteArray response;
    if (!requestPost("/api/mac-addresses", body, &response, error)) {
        return false;
    }

    qDebug().noquote() << "[Tuple] debugUpdateMacStatus response:" << QString::fromUtf8(response);
    return true;
}

bool QTupleService::reportWriteRecord(const TupleApplyResult& tuple, const QString& productSn, const QString& result,
                                      const QString& btRssi, bool btRssiPass,
                                      const QString& bleRssi, bool bleRssiPass,
                                      const QString& softwareVersion, bool softwareVersionPass, QString* error) {
    const bool pass = result == "OK" || result == "通过" || result == "true";
    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    const QString reportTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString sn =  productSn.trimmed();
    const QString tupleValue = tuple.productKey + tuple.deviceName + "***";
    const QString inspectionItem = QString("R_D_TUPLE:%1:%2:%3").arg(tupleValue, pass ? "true" : "false").arg(timestamp);

    QJsonArray inspectionItems;
    inspectionItems.append(inspectionItem);
    if (!btRssi.trimmed().isEmpty()) {
        inspectionItems.append(QString("R_BT_RSSI:%1:%2:%3").arg(btRssi.trimmed(), btRssiPass ? "true" : "false").arg(timestamp));
    }
    if (!bleRssi.trimmed().isEmpty()) {
        inspectionItems.append(QString("R_BLE_RSSI:%1:%2:%3").arg(bleRssi.trimmed(), bleRssiPass ? "true" : "false").arg(timestamp));
    }
    if (!softwareVersion.trimmed().isEmpty()) {
        inspectionItems.append(QString("R_MO_VAR:%1:%2:%3").arg(softwareVersion.trimmed(), softwareVersionPass ? "true" : "false").arg(timestamp));
    }

    QJsonObject bodyObj;
    bodyObj.insert("sn", sn);
    bodyObj.insert("reportTime", reportTime);
    bodyObj.insert("inspectionItems", inspectionItems);
    const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    // qDebug().noquote() << "[Tuple] reportWriteRecord body:" << QString::fromUtf8(body);
    QByteArray response;
    if (!requestPost("/api/inspection/report", body, &response, error)) {
        return false;
    }

    qDebug().noquote() << "[Tuple] reportWriteRecord response:" << QString::fromUtf8(response);
    return true;
}

QString QTupleService::normalizedBaseUrl() const {
    QString url = baseUrl_.trimmed();
    while (url.endsWith('/')) {
        url.chop(1);
    }
    return url;
}

bool QTupleService::requestGet(const QString& path, const QString& query, QByteArray* response, QString* error) {
    QNetworkAccessManager manager;
    QUrl url(normalizedBaseUrl() + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    setCommonHeaders(request);
    qDebug().noquote() << "[Tuple] GET api:" << url.toString(QUrl::FullyEncoded);

    QNetworkReply* reply = manager.get(request);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray body = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (response) {
        *response = body;
    }
    if (!ok && error) {
        *error = reply->errorString() + " " + QString::fromUtf8(body);
    }
    reply->deleteLater();
    return ok;
}

bool QTupleService::requestPost(const QString& path, const QByteArray& body, QByteArray* response, QString* error) {
    QNetworkAccessManager manager;
    const QUrl url(normalizedBaseUrl() + path);
    QNetworkRequest request(url);
    setCommonHeaders(request);
    qDebug().noquote() << "[Tuple] POST api:" << url.toString(QUrl::FullyEncoded)
                       << "body:" << QString::fromUtf8(body);

    QNetworkReply* reply = manager.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray responseBody = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (response) {
        *response = responseBody;
    }
    if (!ok && error) {
        *error = reply->errorString() + " " + QString::fromUtf8(responseBody);
    }
    reply->deleteLater();
    return ok;
}

void QTupleService::setCommonHeaders(QNetworkRequest& request) const {
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Device-Id", QSysInfo::machineHostName().toUtf8());
    request.setRawHeader("APP-Version", "factory-tool");
    if (!authHeader_.isEmpty()) {
        request.setRawHeader("Authorization", authHeader_);
    }
}
