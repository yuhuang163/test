#include "bydmes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QNetworkReply>
#include <QDateTime>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>

#include "Abini.h"

bydmes::bydmes() {}

QString bydmes::settingsValue(const QString& key, const QString& fallback) const {
    QString value = SETTINGS.value("Mes/" + key).toString();
    if (value.isEmpty()) {
        value = SETTINGS.value("MES/" + key, fallback).toString();
    }
    if (value.isEmpty()) {
        value = fallback;
    }
    return value;
}

QString bydmes::baseUrl() const {
    QString url = settingsValue("NET", "http://192.168.11.157/Service.action").trimmed();
    if (!url.contains("Service.action")) {
        if (url.endsWith('/')) {
            url += "Service.action";
        } else {
            url += "/Service.action";
        }
    }
    return url;
}

QString bydmes::buildRemark(const MesPacketData& pack) const {
    QStringList parts;
    parts << ("RESULT=" + pack.result);
    if (!pack.error.isEmpty() && pack.error != "NULL") {
        parts << ("ERROR=" + pack.error);
    }
    if (!pack.itemvalue.isEmpty()) {
        parts << ("TESTDATA=" + pack.itemvalue);
    }
    if (!pack.model.isEmpty()) {
        parts << ("MODEL=" + pack.model);
    }
    return parts.join(" ; ");
}

QJsonObject bydmes::buildBydStartParam(const MesPacketData& pack) const {
    // 按 BYD 文档示例: Start 参数与字段名严格对应。
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["SFC"] = pack.sn;
    param["STATION_ID"] = settingsValue("StationID", "");
    param["LINE"] = settingsValue("Line", "");
    param["SHOPORDER"] = settingsValue("Resource", "");
    param["SCHEDULING_ID"] = settingsValue("SchedulingID", "");
    param["CLIENT_ID"] = "1";
    return param;
}

QJsonObject bydmes::buildBydGetCustomDataParam() const {
    // 按 BYD 文档示例: GetCustomData 仅使用以下字段。
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["CLIENT_ID"] = "1";
    param["STATION_ID"] = settingsValue("StationID", "");
    param["PRODUCT_ID"] = settingsValue("PRODUCT_ID", "");
    return param;
}

QString bydmes::normalizeTestResult(const QString& result) const {
    const QString upper = result.trimmed().toUpper();
    return (upper == "NG" || upper == "FAIL" || upper == "0") ? "FAIL" : "PASS";
}

QJsonArray bydmes::buildBydTestDataList(const MesPacketData& pack, const QString& testTime) const {
    QJsonObject item;
    item["NAME"] = "RESULT_DATA";
    item["VALUE"] = pack.itemvalue;
    item["MAX_VALUE"] = "";
    item["MIN_VALUE"] = "";
    item["STANDARD_VALUE"] = "";
    item["UNIT"] = "";
    item["TEST_TIME"] = testTime;
    item["TEST_USER"] = pack.Employee_ID.isEmpty() ? QString("guest") : pack.Employee_ID;
    item["LOCATION"] = "";
    item["TEST_END_TIME"] = testTime;
    item["TEST_RESULT"] = normalizeTestResult(pack.result);
    item["ERROR_CODE"] = (pack.error == "NULL") ? QString("") : pack.error;
    item["PN"] = pack.sn;
    item["REMARK"] = buildRemark(pack);
    item["TEXT"] = "";

    QJsonArray list;
    list.append(item);
    return list;
}

QJsonObject bydmes::buildBydTestDataCollectParam(const MesPacketData& pack) const {
    const QString testTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString testResult = normalizeTestResult(pack.result);
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["CLIENT_ID"] = "1";
    param["PROJECT_NAME"] = settingsValue("PROJECT", "");
    param["TEST_STATION"] = pack.test_station.isEmpty() ? settingsValue("Operation", "") : pack.test_station;
    param["TDS_NAME"] = settingsValue("TDS_NAME", "VH_C5");
    param["SHOPORDER_NO"] = pack.lotName.isEmpty() ? settingsValue("Resource", "") : pack.lotName;
    param["LINE_NO"] = pack.line.isEmpty() ? settingsValue("Line", "") : pack.line;
    param["STATION_ID"] = settingsValue("StationID", "");
    param["FIXTURE_NO"] = pack.machineNo.isEmpty() ? settingsValue("machineNo", "") : pack.machineNo;
    param["STARTIME"] = testTime;
    param["SN"] = pack.sn;
    param["TEST_RESULT"] = testResult;
    param["ELAPSE_TIME"] = "1";
    param["TEST_COUNT"] = "1";
    param["TEST_DATA_LIST"] = buildBydTestDataList(pack, testTime);
    return param;
}

QJsonObject bydmes::buildBydCompleteParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["SFC"] = pack.sn;
    param["SCHEDULING_ID"] = settingsValue("SchedulingID", "");
    param["STATION_ID"] = settingsValue("StationID", "");
    param["CLIENT_ID"] = "1";
    param["REMARK"] = buildRemark(pack);
    return param;
}

QJsonObject bydmes::buildBydNcCompleteParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["SFC"] = pack.sn;
    param["STATION_ID"] = settingsValue("StationID", "");
    const QString ncValue = pack.error.isEmpty() ? QString("TEST_ITEM_NG") : pack.error;
    param["NC_CODE"] = ncValue;
    param["NC_CONTEXT"] = "不良原因:" + ncValue + "; 测试结果:" + buildRemark(pack);
    param["NC_TYPE"] = ncValue;
    param["SCHEDULING_ID"] = settingsValue("SchedulingID", "");
    param["CLIENT_ID"] = "1";
    return param;
}

QString bydmes::normalizeMacString(QString value) const {
    value = value.trimmed();
    value.remove(' ');
    if (value.isEmpty()) {
        return {};
    }
    static const QRegularExpression withColons(R"(^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$)");
    if (withColons.match(value).hasMatch()) {
        return value.toUpper();
    }
    const QString compact = QString(value).remove(':');
    static const QRegularExpression hex12(R"(^[0-9A-Fa-f]{12}$)");
    if (hex12.match(compact).hasMatch()) {
        const QString u = compact.toUpper();
        return QString("%1:%2:%3:%4:%5:%6")
            .arg(u.mid(0, 2), u.mid(2, 2), u.mid(4, 2), u.mid(6, 2), u.mid(8, 2), u.mid(10, 2));
    }
    return {};
}

void bydmes::collectMacFromJsonObject(const QJsonObject& obj, QString* out) const {
    if (!out || !out->isEmpty()) {
        return;
    }
    static const QStringList keys = {"BT_MAC", "MAC", "Mac", "mac", "BLE_MAC", "BT_MAC_ADDRESS", "WIFI_MAC",
                                     "DATA", "CUSTOM_DATA", "CustomData"};
    for (const QString& k : keys) {
        if (!obj.contains(k)) {
            continue;
        }
        const QJsonValue v = obj.value(k);
        if (v.isString()) {
            const QString mac = normalizeMacString(v.toString());
            if (!mac.isEmpty()) {
                *out = mac;
                return;
            }
            const QJsonDocument nested = QJsonDocument::fromJson(v.toString().toUtf8());
            if (nested.isObject()) {
                collectMacFromJsonObject(nested.object(), out);
                if (!out->isEmpty()) {
                    return;
                }
            }
        } else if (v.isObject()) {
            collectMacFromJsonObject(v.toObject(), out);
            if (!out->isEmpty()) {
                return;
            }
        } else if (v.isArray()) {
            for (const QJsonValue& item : v.toArray()) {
                if (item.isObject()) {
                    collectMacFromJsonObject(item.toObject(), out);
                }
                if (!out->isEmpty()) {
                    return;
                }
            }
        }
    }
}

QString bydmes::extractMacForUi(const QByteArray& responseData) const {
    QString mac;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        collectMacFromJsonObject(doc.object(), &mac);
    }
    if (!mac.isEmpty()) {
        return mac;
    }
    const QString text = QString::fromUtf8(responseData);
    static const QRegularExpression reColon(R"((?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2})");
    const QRegularExpressionMatch m1 = reColon.match(text);
    if (m1.hasMatch()) {
        return m1.captured(0).toUpper();
    }
    static const QRegularExpression re12(R"(\b[0-9A-Fa-f]{12}\b)");
    const QRegularExpressionMatch m2 = re12.match(text);
    if (m2.hasMatch()) {
        return normalizeMacString(m2.captured(0));
    }
    return {};
}

QByteArray bydmes::sendRequest(const QString& method, const QJsonObject& param, QString* errorMessage) const {
    QUrl url(baseUrl());
    QUrlQuery query;
    query.addQueryItem("method", method);
    query.addQueryItem("param", QString::fromUtf8(QJsonDocument(param).toJson(QJsonDocument::Compact)));
    url.setQuery(query);

    qDebug() << "BYD MES request:" << url.toString(QUrl::FullyDecoded);

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray responseData;
    if (reply->error() == QNetworkReply::NoError) {
        responseData = reply->readAll();
        qDebug() << "BYD MES response:" << QString::fromUtf8(responseData);
    } else if (errorMessage != nullptr) {
        *errorMessage = reply->errorString();
    }

    reply->deleteLater();
    return responseData;
}

bool bydmes::isSuccessResponse(const QByteArray& responseData, QString* responseText, QString* errorMessage) const {
    const QString text = QString::fromUtf8(responseData);
    if (responseText != nullptr) {
        *responseText = text;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorMessage != nullptr) {
            *errorMessage = "BYD MES 返回不是有效 JSON: " + text;
        }
        return false;
    }

    const QJsonObject obj = doc.object();
    const QString result = obj.value("RESULT").toString().trimmed().toUpper();
    const int code = obj.value("CODE").toInt(-1);
    const QString message = obj.value("MESSAGE").toString();

    if (result == "OK" || result == "SUCCESS" || result == "PASS") {
        return true;
    }
    if (result == "FAIL") {
        if (errorMessage != nullptr) {
            *errorMessage = message.isEmpty() ? text : message;
        }
        return false;
    }
    if (code == 1) {
        return true;
    }
    if (code == 0) {
        if (errorMessage != nullptr) {
            *errorMessage = message.isEmpty() ? text : message;
        }
        return false;
    }

    if (errorMessage != nullptr) {
        *errorMessage = text;
    }
    return false;
}

void bydmes::LogIn(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    qDebug() << "BYD MES login init, current user:" << pack.userNo;
}

void bydmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    if (pack.sn.isEmpty()) {
        emit operateMesError(pack.mechines, "BYD MES Start：SN 为空");
        return;
    }

    const QJsonObject param = buildBydStartParam(pack);
    QString networkError;
    const QByteArray responseData = sendRequest("Start", param, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, "BYD MES 站前 Start 请求失败: " + networkError);
        return;
    }

    QString responseText;
    QString errorMessage;
    if (!isSuccessResponse(responseData, &responseText, &errorMessage)) {
        emit operateMesError(pack.mechines, "BYD MES 站前 Start 失败: " + errorMessage);
        return;
    }

    emit operateMesSucess(pack.mechines);
}

void bydmes::GetTestData(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    const QJsonObject param = buildBydGetCustomDataParam();
    QString networkError;
    const QByteArray responseData = sendRequest("GetCustomData", param, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, "BYD MES GetCustomData 请求失败: " + networkError);
        return;
    }
    QString responseText;
    QString errorMessage;
    if (!isSuccessResponse(responseData, &responseText, &errorMessage)) {
        emit operateMesError(pack.mechines, "BYD MES GetCustomData 失败: " + errorMessage);
        return;
    }

    const QString mac = extractMacForUi(responseData);
    if (!mac.isEmpty()) {
        emit sendMesTestvalue(pack.mechines, mac);
    }
}

void bydmes::TestPass(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    QJsonObject param = buildBydTestDataCollectParam(pack);

    QString networkError;
    QByteArray responseData = sendRequest("TestDataCollect2MainChild", param, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, "BYD MES TestDataCollect2MainChild 请求失败: " + networkError);
        return;
    }

    QString responseText;
    QString errorMessage;
    if (!isSuccessResponse(responseData, &responseText, &errorMessage)) {
        emit operateMesError(pack.mechines, "BYD MES TestDataCollect2MainChild 失败: " + errorMessage);
        return;
    }

    const QString normalizedResult = normalizeTestResult(pack.result);
    const bool isFailResult = (normalizedResult == "FAIL");
    const QString completeMethod = isFailResult ? "NcComplete" : "Complete";
    const QJsonObject completeParam = isFailResult ? buildBydNcCompleteParam(pack) : buildBydCompleteParam(pack);

    networkError.clear();
    responseData = sendRequest(completeMethod, completeParam, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, "BYD MES " + completeMethod + " 请求失败: " + networkError);
        return;
    }

    if (isSuccessResponse(responseData, &responseText, &errorMessage)) {
        emit operateMesSucess(pack.mechines);
    } else {
        emit operateMesError(pack.mechines, "BYD MES " + completeMethod + " 失败: " + errorMessage);
    }
}
