#include "bydmes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QJsonValue>
#include <QNetworkReply>
#include <QDateTime>
#include <QDebug>
#include <QEventLoop>
#include <QUrl>
#include <QUrlQuery>

#include "Abini.h"

static QMutex g_bydMesRuntimeMutex;
static QMap<QString, QString> g_bydMesRuntimeMap;

namespace {

/// BYD MES2 常见约定：query 中 param 为 [KEY:value,...]，键名与字符串值均不加双引号；嵌套对象用 {...}，数组用 [...]。
QString bydParamValueToken(const QJsonValue& v) {
    if (v.isBool()) {
        return v.toBool() ? QStringLiteral("1") : QStringLiteral("0");
    }
    if (v.isNull() || v.isUndefined()) {
        return QString();
    }
    if (v.isDouble()) {
        const double d = v.toDouble();
        const qint64 i = static_cast<qint64>(d);
        if (d == static_cast<double>(i)) {
            return QString::number(i);
        }
        return QString::number(d, 'g', 15);
    }
    if (v.isString()) {
        return v.toString();
    }
    if (v.isArray()) {
        QStringList elems;
        for (const QJsonValue& e : v.toArray()) {
            elems << bydParamValueToken(e);
        }
        return QStringLiteral("[%1]").arg(elems.join(QLatin1Char(',')));
    }
    if (v.isObject()) {
        QStringList pairs;
        const QJsonObject o = v.toObject();
        for (auto it = o.constBegin(); it != o.constEnd(); ++it) {
            pairs << (it.key() + QLatin1Char(':') + bydParamValueToken(it.value()));
        }
        return QStringLiteral("{%1}").arg(pairs.join(QLatin1Char(',')));
    }
    return QString();
}

QString bydMesServiceParamEnvelope(const QJsonObject& param, QChar open, QChar close) {
    QStringList pairs;
    for (auto it = param.constBegin(); it != param.constEnd(); ++it) {
        pairs << (it.key() + QLatin1Char(':') + bydParamValueToken(it.value()));
    }
    return QString(open) + pairs.join(QLatin1Char(',')) + QString(close);
}

QString bydRootBracketParam(const QJsonObject& param) {
    return bydMesServiceParamEnvelope(param, QLatin1Char('['), QLatin1Char(']'));
}

/// 产线 MES2 部分接口要求 param 为 {K:v,...}（与 GetSfcKeyBySfc 一致），其余仍用 [K:v,...]；TestDataCollect2MainChild 为标准 JSON 对象串。
bool bydMesMethodUsesCurlyServiceParam(const QString& method) {
    static const QStringList kCurly = {QStringLiteral("GetCustomData"), QStringLiteral("GetSfcKeyBySfc")};
    for (const QString& m : kCurly) {
        if (method.compare(m, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

/// GetCustomData / 部分按过程码接口：DATA 为 { name, value } 行列表（键名常见小写），按 name 取 value。
QString bydMesNamedRowValue(const QJsonArray& arr, const QString& wantName) {
    if (wantName.isEmpty()) {
        return {};
    }
    const QString w = wantName.trimmed();
    for (const QJsonValue& v : arr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        QString itemName = o.value(QStringLiteral("name")).toString();
        if (itemName.isEmpty()) {
            itemName = o.value(QStringLiteral("NAME")).toString();
        }
        if (itemName.trimmed().compare(w, Qt::CaseInsensitive) != 0) {
            continue;
        }
        QString val = o.value(QStringLiteral("value")).toString();
        if (val.isEmpty()) {
            val = o.value(QStringLiteral("VALUE")).toString();
        }
        val = val.trimmed();
        if (!val.isEmpty()) {
            return val;
        }
    }
    return {};
}

}  // namespace

bydmes::bydmes() {}

void bydmes::setRuntimeMesParamMap(const QMap<QString, QString>& params) {
    QMutexLocker locker(&g_bydMesRuntimeMutex);
    g_bydMesRuntimeMap = params;
    // 外部产线 ini 解析结果：逐项打印，便于对照文件与 bydmes::settingsValue 实际生效值
    qDebug() << QStringLiteral("[BYD MES 产线配置] 已载入内存映射，共 %1 条（优先于本机 Mes/MES）")
                    .arg(g_bydMesRuntimeMap.size());
    for (auto it = g_bydMesRuntimeMap.constBegin(); it != g_bydMesRuntimeMap.constEnd(); ++it) {
        qDebug() << QStringLiteral("[BYD MES 产线配置]") << it.key() << QLatin1Char('=') << it.value();
    }
}

void bydmes::clearRuntimeMesParamMap() {
    QMutexLocker locker(&g_bydMesRuntimeMutex);
    const int n = g_bydMesRuntimeMap.size();
    g_bydMesRuntimeMap.clear();
    if (n > 0) {
        qDebug() << QStringLiteral("[BYD MES 产线配置] 已清除内存映射，原共 %1 条").arg(n);
    }
}

QString bydmes::settingsValue(const QString& key, const QString& fallback) const {
    {
        QMutexLocker locker(&g_bydMesRuntimeMutex);
        // 产线文件仅驻内存、不写本机 ini；有键则直接用于 MES 请求
        if (g_bydMesRuntimeMap.contains(key)) {
            return g_bydMesRuntimeMap.value(key);
        }
    }
    // 未在产线映射中的键：保持原先 Mes/ → MES/ → 代码 fallback
    QString value = SETTINGS.value(QStringLiteral("Mes/") + key).toString();
    if (value.isEmpty()) {
        value = SETTINGS.value(QStringLiteral("MES/") + key, fallback).toString();
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
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
    return param;
}

QJsonObject bydmes::buildBydGetCustomDataParam() const {
    // 按 BYD 文档示例: GetCustomData 仅使用以下字段。
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
    param["STATION_ID"] = settingsValue("StationID", "");
    param["PRODUCT_ID"] = settingsValue("PRODUCT_ID", "");
    return param;
}

QJsonObject bydmes::buildBydGetSnByProcessCodeParam(const QString& processCode) const {
    // 与产线 MES2 文档对齐：过程码字段名可通过 Mes/GetSnByProcessCodeField 覆盖（默认 PROCESS_CODE）
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
    param["SFC"] = processCode;
    const QString fieldKey = settingsValue("GetSnByProcessCodeField", "PROCESS_CODE").trimmed();
    const QString key = fieldKey.isEmpty() ? QStringLiteral("PROCESS_CODE") : fieldKey;
    param[key] = processCode;
    return param;
}

QString bydmes::parseSnFromGetSnByProcessCodeResponse(const QByteArray& responseData) const {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonObject root = doc.object();
    const QJsonValue dataV = root.value(QStringLiteral("DATA"));
    if (dataV.isObject()) {
        const QJsonObject o = dataV.toObject();
        static const QStringList kSnKeys = {QStringLiteral("SN"), QStringLiteral("SFC"), QStringLiteral("SERIAL_NO"),
                                            QStringLiteral("SERIAL"), QStringLiteral("PRODUCT_SN")};
        for (const QString& k : kSnKeys) {
            const QString s = o.value(k).toString().trimmed();
            if (!s.isEmpty()) {
                return s;
            }
        }
    }
    if (dataV.isString()) {
        const QString s = dataV.toString().trimmed();
        if (!s.isEmpty()) {
            return s;
        }
    }
    if (dataV.isArray()) {
        const QJsonArray arr = dataV.toArray();
        // 产线常见：DATA 为 name/value 行表（与 GetCustomData 同形）；勿按行盲取 VALUE，否则会误取 deviceSecret 等首行。
        const QString preferItem = settingsValue(QStringLiteral("GetSnByProcessCodeItemName"), QString()).trimmed();
        if (!preferItem.isEmpty()) {
            const QString fromPrefer = bydMesNamedRowValue(arr, preferItem);
            if (!fromPrefer.isEmpty()) {
                return fromPrefer;
            }
        }
        for (const QString& rowName : {QStringLiteral("SN"), QStringLiteral("SFC")}) {
            const QString fromRow = bydMesNamedRowValue(arr, rowName);
            if (!fromRow.isEmpty()) {
                return fromRow;
            }
        }
        for (const QJsonValue& v : arr) {
            if (!v.isObject()) {
                continue;
            }
            const QJsonObject o = v.toObject();
            static const QStringList kRowKeys = {QStringLiteral("SN"), QStringLiteral("SFC"), QStringLiteral("SERIAL_NO"),
                                                 QStringLiteral("SERIAL"), QStringLiteral("PRODUCT_SN")};
            for (const QString& k : kRowKeys) {
                QString s = o.value(k).toString().trimmed();
                if (s.isEmpty() && k == QStringLiteral("SN")) {
                    s = o.value(QStringLiteral("sn")).toString().trimmed();
                }
                if (s.isEmpty() && k == QStringLiteral("SFC")) {
                    s = o.value(QStringLiteral("sfc")).toString().trimmed();
                }
                if (!s.isEmpty()) {
                    return s;
                }
            }
        }
    }
    QString s = root.value(QStringLiteral("SN")).toString().trimmed();
    if (!s.isEmpty()) {
        return s;
    }
    s = root.value(QStringLiteral("SFC")).toString().trimmed();
    return s;
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
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
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
    param["ELAPSE_TIME"] = QString::number(qMax(1, pack.elapseTime));
    param["TEST_COUNT"] = QString::number(qMax(1, pack.testCount));
    param["TEST_DATA_LIST"] = buildBydTestDataList(pack, testTime);
    return param;
}

QJsonObject bydmes::buildBydCompleteParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["SFC"] = pack.sn;
    param["SCHEDULING_ID"] = settingsValue("SchedulingID", "");
    param["STATION_ID"] = settingsValue("StationID", "");
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
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
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
    return param;
}

QString bydmes::formatGetCustomDataItemsJson(const QByteArray& responseData) const {
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonObject root = doc.object();
    const QJsonArray dataArr = root.value(QStringLiteral("DATA")).toArray();
    if (dataArr.isEmpty()) {
        return {};
    }
    QJsonArray out;
    for (const QJsonValue& v : dataArr) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject item = v.toObject();
        QJsonObject row;
        row[QStringLiteral("NAME")] = item.value(QStringLiteral("NAME")).toString();
        row[QStringLiteral("VALUE")] = item.value(QStringLiteral("VALUE")).toString();
        row[QStringLiteral("REMARK")] = item.value(QStringLiteral("REMARK")).toString();
        out.append(row);
        qDebug() << "BYD GetCustomData item:" << row[QStringLiteral("NAME")].toString()
                 << "VALUE=" << row[QStringLiteral("VALUE")].toString()
                 << "REMARK=" << row[QStringLiteral("REMARK")].toString();
    }
    if (out.isEmpty()) {
        return {};
    }
    return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact));
}

QByteArray bydmes::sendRequest(const QString& method, const QJsonObject& param, QString* errorMessage) const {
    QUrl url(baseUrl());
    QUrlQuery query;
    query.addQueryItem("method", method);
    // TestDataCollect2MainChild：标准紧凑 JSON；GetCustomData / GetSfcKeyBySfc 等：{KEY:value,...}；其余接口 [KEY:value,...]
    QString paramStr;
    if (method == QLatin1String("TestDataCollect2MainChild")) {
        paramStr = QString::fromUtf8(QJsonDocument(param).toJson(QJsonDocument::Compact));
    } else if (bydMesMethodUsesCurlyServiceParam(method)) {
        paramStr = bydMesServiceParamEnvelope(param, QLatin1Char('{'), QLatin1Char('}'));
    } else {
        paramStr = bydRootBracketParam(param);
    }
    query.addQueryItem("param", paramStr);
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
    // 工站 emit getMesTestValue 且 MesPacketData::processCode 已填时：走按过程码换 SN（与 GetCustomData 互斥）
    if (!pack.sn.trimmed().isEmpty()) {
        QuerySnByProcessCode(pack);
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

    const QString customJson = formatGetCustomDataItemsJson(responseData);
    if (!customJson.isEmpty()) {
        emit sendMesTestvalue(pack.mechines, customJson);
    }
}

void bydmes::QuerySnByProcessCode(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }
    // 优先 processCode；未填时兼容用 itemvalue 传过程码（少改调用方）
    const QString code =
        pack.sn.trimmed().isEmpty() ? pack.itemvalue.trimmed() : pack.sn.trimmed();
    if (code.isEmpty()) {
        emit operateMesError(pack.mechines,
                             QStringLiteral("BYD MES 按过程码查 SN：过程码为空（请设置 MesPacketData::processCode 或 itemvalue）"));
        return;
    }
    const QString method = settingsValue("GetSnByProcessCodeMethod", "GetSnByProcessCode");
    const QJsonObject param = buildBydGetSnByProcessCodeParam(code);
    QString networkError;
    const QByteArray responseData = sendRequest(method, param, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, QStringLiteral("BYD MES %1 请求失败: %2").arg(method, networkError));
        return;
    }
    QString responseText;
    QString errorMessage;
    if (!isSuccessResponse(responseData, &responseText, &errorMessage)) {
        emit operateMesError(pack.mechines, QStringLiteral("BYD MES %1 失败: %2").arg(method, errorMessage));
        return;
    }
    const QString sn = parseSnFromGetSnByProcessCodeResponse(responseData);
    if (sn.isEmpty()) {
        emit operateMesError(
            pack.mechines,
            QStringLiteral("BYD MES %1 成功但未解析到 SN，请对照产线返回 JSON 调整解析或 Mes/GetSnByProcessCodeField")
                .arg(method));
        return;
    }
    QJsonArray out;
    QJsonObject row;
    row[QStringLiteral("NAME")] = QStringLiteral("SN");
    row[QStringLiteral("VALUE")] = sn;
    row[QStringLiteral("REMARK")] = QStringLiteral("BYD_QUERY_SN_BY_PROCESS_CODE");
    out.append(row);
    const QString payload = QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact));
    qDebug() << "BYD QuerySnByProcessCode SN=" << sn;
    emit sendMesTestvalue(pack.mechines, payload);
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
