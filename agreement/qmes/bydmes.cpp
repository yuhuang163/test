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
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QTextCodec>

#include "Abini.h"

static QMutex g_bydMesRuntimeMutex;
static QMap<QString, QString> g_bydMesRuntimeMap;

static QMutex g_externalMesConfigMutex;
static QMap<QString, QString> g_externalMesConfigMap;

namespace {

/// 上位机设置.ini：与 qsetting 一致支持 [mes]/[Mes]/[MES] 下的 ConfigFilePath/configfilepath
QString bydMesReadExternalConfigPathFromMainIni() {
    QString p = SETTINGS.value(QStringLiteral("mes/ConfigFilePath")).toString().trimmed();
    if (p.isEmpty()) {
        p = SETTINGS.value(QStringLiteral("Mes/ConfigFilePath")).toString().trimmed();
    }
    if (p.isEmpty()) {
        p = SETTINGS.value(QStringLiteral("MES/ConfigFilePath")).toString().trimmed();
    }
    if (p.isEmpty()) {
        p = SETTINGS.value(QStringLiteral("mes/configfilepath")).toString().trimmed();
    }
    if (p.isEmpty()) {
        p = SETTINGS.value(QStringLiteral("Mes/configfilepath")).toString().trimmed();
    }
    if (p.isEmpty()) {
        p = SETTINGS.value(QStringLiteral("MES/configfilepath")).toString().trimmed();
    }
    return p;
}

QString bydMesResolveMesConfigFilePath(const QString& configFilePath) {
    const QFileInfo selectedInfo(configFilePath);
    const QDir dir = selectedInfo.isDir() ? QDir(selectedInfo.absoluteFilePath()) : selectedInfo.dir();
    const QStringList candidates = {dir.filePath(QStringLiteral("mes_config.ini")),
                                    dir.filePath(QStringLiteral("mes_config_ini"))};
    for (const QString& candidate : candidates) {
        if (QFileInfo::exists(candidate)) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return candidates.first();
}


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

/// 外部 mes ini：按 BOM / 合法 UTF-8 选 UTF-8，否则用系统区域编码（常见 GBK），避免中文成问号
QTextCodec* bydMesPickIniFileCodec(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
        return utf8 ? utf8 : QTextCodec::codecForLocale();
    }
    const QByteArray sample = f.read(65536);
    f.close();
    QTextCodec* detected = QTextCodec::codecForUtfText(sample);
    if (detected != nullptr) {
        return detected;
    }
    QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
    return utf8 ? utf8 : QTextCodec::codecForLocale();
}

}  // namespace

bydmes::bydmes() {
    // QMesManager 成员构造时 QApplication 已就绪，按上位机设置.ini 的 ConfigFilePath 预载 BYD 外部 MES ini
    bydmes::loadExternalMesConfig(nullptr);
}

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

bool bydmes::loadExternalMesConfig(QString* errorMessage) {
    const QString configFilePath = bydMesReadExternalConfigPathFromMainIni();

    if (configFilePath.isEmpty()) {
        {
            QMutexLocker locker(&g_externalMesConfigMutex);
            g_externalMesConfigMap.clear();
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("请选择MES配置文件");
        }
        qWarning() << QStringLiteral("[BYD MES 外部配置] ConfigFilePath 未配置，请选择MES配置文件");
        return false;
    }

    const QString mesConfigPath = bydMesResolveMesConfigFilePath(configFilePath);
    QFileInfo fileInfo(mesConfigPath);
    if (!fileInfo.exists()) {
        {
            QMutexLocker locker(&g_externalMesConfigMutex);
            g_externalMesConfigMap.clear();
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("MES配置文件不存在: %1").arg(mesConfigPath);
        }
        qWarning() << QStringLiteral("[BYD MES 外部配置] ConfigFilePath=%1，对应 mes_config.ini 不存在: %2")
                          .arg(configFilePath, mesConfigPath);
        return false;
    }

    if (!fileInfo.isReadable()) {
        {
            QMutexLocker locker(&g_externalMesConfigMutex);
            g_externalMesConfigMap.clear();
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("MES配置文件不可读: %1").arg(mesConfigPath);
        }
        qWarning() << QStringLiteral("[BYD MES 外部配置] 配置文件不可读: %1").arg(mesConfigPath);
        return false;
    }
    
    QTextCodec* iniCodec = bydMesPickIniFileCodec(mesConfigPath);
    QSettings externalConfig(mesConfigPath, QSettings::IniFormat);
    externalConfig.setIniCodec(iniCodec);

    QMap<QString, QString> configMap;
    const QStringList keys = externalConfig.allKeys();
    for (const QString& key : keys) {
        configMap.insert(key, externalConfig.value(key).toString());
    }

    {
        QMutexLocker locker(&g_externalMesConfigMutex);
        g_externalMesConfigMap = configMap;
    }

    qDebug() << QStringLiteral("[BYD MES 外部配置] 已按 ConfigFilePath 目录从 %1 加载 %2 条，INI 解析编码: %3")
                    .arg(mesConfigPath)
                    .arg(configMap.size())
                    .arg(QString::fromLatin1(iniCodec->name()));
    return true;
}

void bydmes::clearExternalMesConfig() {
    QMutexLocker locker(&g_externalMesConfigMutex);
    const int n = g_externalMesConfigMap.size();
    g_externalMesConfigMap.clear();
    if (n > 0) {
        qDebug() << QStringLiteral("[BYD MES 外部配置] 已清除内存映射，原共 %1 条").arg(n);
    }
}

QString bydmes::settingsValue(const QString& key, const QString& fallback) const {
    {
        QMutexLocker locker(&g_bydMesRuntimeMutex);
        if (g_bydMesRuntimeMap.contains(key)) {
            return g_bydMesRuntimeMap.value(key);
        }
    }
    // 外部 mes 文件由 loadExternalMesConfig 载入；allKeys 为 Mes/xxx、MES/xxx，与主程序键名一致
    {
        QMutexLocker locker(&g_externalMesConfigMutex);
        if (!g_externalMesConfigMap.isEmpty()) {
            QString value = g_externalMesConfigMap.value(QStringLiteral("Mes/") + key);
            if (value.isEmpty()) {
                value = g_externalMesConfigMap.value(QStringLiteral("MES/") + key);
            }
            if (value.isEmpty()) {
                value = g_externalMesConfigMap.value(key);
            }
            if (!value.isEmpty()) {
                return value;
            }
        }
    }
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

QJsonObject bydmes::buildBydGetSnByProcessCodeParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID", "-1");
    param["CLIENT_ID"] = settingsValue("CLIENT_ID", "1");
    param["SFC"] = pack.sn;
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
        // GetSfcKeyBySfc：DATA 每行有 name（如「主板」）与 station（如「主板绑定」），须与 bindingLabel 任一匹配再取 value，避免仅用 name 时永远对不上 station 文案
        const QString bindingLabel = settingsValue(QStringLiteral("GetSfcKeyBindingItemName"), QStringLiteral("主板绑定")).trimmed();
        if (!bindingLabel.isEmpty()) {
            for (const QJsonValue& v : arr) {
                if (!v.isObject()) {
                    continue;
                }
                const QJsonObject o = v.toObject();
                const auto fieldMatches = [&](const QString& fieldVal) {
                    return !fieldVal.isEmpty() && fieldVal.compare(bindingLabel, Qt::CaseInsensitive) == 0;
                };
                const QString nameVal = o.value(QStringLiteral("name")).toString().trimmed();
                const QString nameUpper = o.value(QStringLiteral("NAME")).toString().trimmed();
                const QString stationVal = o.value(QStringLiteral("station")).toString().trimmed();
                const QString stationUpper = o.value(QStringLiteral("STATION")).toString().trimmed();
                if (!(fieldMatches(nameVal) || fieldMatches(nameUpper) || fieldMatches(stationVal) || fieldMatches(stationUpper))) {
                    continue;
                }
                QString val = o.value(QStringLiteral("value")).toString().trimmed();
                if (val.isEmpty()) {
                    val = o.value(QStringLiteral("VALUE")).toString().trimmed();
                }
                if (!val.isEmpty()) {
                    qDebug() << QStringLiteral("[BYD MES] DATA 行匹配「%1」提取 value=%2").arg(bindingLabel, val);
                    return val;
                }
            }
        }
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


}

void bydmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    QString configError;
    if (!bydmes::loadExternalMesConfig(&configError)) {
        emit operateMesError(pack.mechines, configError);
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
    if (pack.sn.trimmed().isEmpty()) {
        emit operateMesError(pack.mechines,
                             QStringLiteral("BYD MES GetSfcKeyBySfc：pack.sn 为空（请在上位机填入过程码/SN）"));
        return;
    }
    const QString method = settingsValue(QStringLiteral("GetSnByProcessCodeMethod"), QStringLiteral("GetSfcKeyBySfc"));
    const QJsonObject param = buildBydGetSnByProcessCodeParam(pack);
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
    // 优先 DATA 中行名「主板绑定」（可 Mes/GetSfcKeyBindingItemName 覆盖）的 value 字段，作为整机 SN 原样下发
    const QString sn = parseSnFromGetSnByProcessCodeResponse(responseData);
    if (sn.isEmpty()) {
        emit operateMesError(
            pack.mechines,
            QStringLiteral("BYD MES %1 成功但未解析到 SN（请检查 DATA 是否含「%2」行的 value）")
                .arg(method, settingsValue(QStringLiteral("GetSfcKeyBindingItemName"), QStringLiteral("主板绑定"))));
        return;
    }
    qDebug() << "BYD QuerySnByProcessCode 下发 value(SN)=" << sn;
    emit sendMesTestvalue(pack.mechines, sn);
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
