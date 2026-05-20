// BYD MES2 实现分区：匿名空间 ①～③，成员实现 ④～⑧（与 bydmes.h 类注释一致）。
#include "bydmes.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkReply>
#include <QSettings>
#include <QTextCodec>
#include <QUrl>
#include <QUrlQuery>

#include "Abini.h"

// =============================================================================
// 文件内全局：外部 mes_config 路径（与 load/settingsValue 共用一把锁）
// =============================================================================
static QMutex g_externalMesConfigMutex;
static QString g_externalMesResolvedIniPath;

static QString bydMesPeekExternalResolvedIniPath() {
    QMutexLocker locker(&g_externalMesConfigMutex);
    return g_externalMesResolvedIniPath;
}

namespace {

// =============================================================================
// ① 调试打印用---- 消除打印乱码 （方括号/花括号、键值无引号）
// =============================================================================

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

// BYD Service.action 简单参数：花括号 {KEY:value,...}（Start/Complete/GetSfcKeyBySfc 等）
QString bydMesCurlyServiceParam(const QJsonObject& param) {
    return bydMesServiceParamEnvelope(param, QLatin1Char('{'), QLatin1Char('}'));
}

QString bydMesBuildServiceParam(const QString& method, const QJsonObject& param) {
    if (method.compare(QLatin1String("TestDataCollect2MainChild"), Qt::CaseInsensitive) == 0
        || method.compare(QLatin1String("AddSfcKey"), Qt::CaseInsensitive) == 0) {
        return QString::fromUtf8(QJsonDocument(param).toJson(QJsonDocument::Compact));
    }
    // 勿用方括号 []：现场 MES 对 Start/Complete 等会异常或回显 param，响应无法按 JSON 解析
    return bydMesCurlyServiceParam(param);
}

// =============================================================================
// ③ 外部 mes_config.ini：主 ini 定位路径、解析绝对路径、编码、QSettings 读键
// =============================================================================

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
    if (selectedInfo.isFile()) {
        const QString abs = selectedInfo.absoluteFilePath();
        const QString lower = abs.toLower();
        if (lower.endsWith(QLatin1String(".ini")) || lower.contains(QLatin1String("mes_config"))) {
            return abs;
        }
    }
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

QTextCodec* bydMesPickIniFileCodec(const QString& filePath) {
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) {
        QTextCodec* gbk = QTextCodec::codecForName("GBK");
        if (gbk != nullptr) {
            return gbk;
        }
        QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
        return utf8 ? utf8 : QTextCodec::codecForLocale();
    }
    const QByteArray sample = f.read(65536);
    f.close();

    QTextCodec* fromBom = QTextCodec::codecForUtfText(sample);
    if (fromBom != nullptr) {
        const QString codecName = QString::fromLatin1(fromBom->name()).toUpper();
        if (codecName.contains(QLatin1String("UTF-8")) || codecName.contains(QLatin1String("UTF-16"))
            || codecName.contains(QLatin1String("UTF-32"))) {
            return fromBom;
        }
    }

    QTextCodec* utf8Codec = QTextCodec::codecForName("UTF-8");
    if (utf8Codec != nullptr && !sample.isEmpty()) {
        QTextCodec::ConverterState st;
        const QString asUtf8 = utf8Codec->toUnicode(sample.constData(), static_cast<int>(sample.size()), &st);
        if (st.invalidChars == 0 && st.remainingChars == 0) {
            if (asUtf8.contains(QLatin1Char('[')) && (asUtf8.contains(QLatin1Char('=')) || asUtf8.contains(QLatin1Char(']')))) {
                return utf8Codec;
            }
        }
    }

    static const char* const kGbNames[] = {"GB2312", "gb2312", "GBK", "gb18030", "GB18030"};
    for (const char* name : kGbNames) {
        QTextCodec* c = QTextCodec::codecForName(name);
        if (c != nullptr) {
            return c;
        }
    }
    QTextCodec* utf8 = QTextCodec::codecForName("UTF-8");
    return utf8 ? utf8 : QTextCodec::codecForLocale();
}

QString bydMesExternalIniValue(QSettings& s, const QString& key) {
    const QString k = key.trimmed();
    if (k.isEmpty()) {
        return {};
    }
    // 配置文件只有Config与SYSTEM节
    const QStringList tryPaths = {QStringLiteral("Config/") + k, k};
    for (const QString& p : tryPaths) {
        const QString v = s.value(p).toString().trimmed();
        if (!v.isEmpty()) {
            return v;
        }
    }
    // SYSTEM节只有JSONURL数据
    if (k.compare(QStringLiteral("NET"), Qt::CaseInsensitive) == 0) {
        for (const QString& p : {QStringLiteral("SYSTEM/JSONURL")}) {
            const QString v = s.value(p).toString().trimmed();
            if (!v.isEmpty()) {
                return v;
            }
        }
    }
    return {};
}

bool bydMesJsonStringFieldEmpty(const QJsonObject& o, const QString& key) {
    return o.value(key).toString().trimmed().isEmpty();
}

}  // namespace

// =============================================================================
// ④ 构造与外部配置 API（登记 / 清除 / 读键 / 服务根 URL）
// =============================================================================

bydmes::bydmes() {
    bydmes::loadExternalMesConfig(nullptr);
}

bool bydmes::loadExternalMesConfig(QString* errorMessage) {
    const QString configFilePath = bydMesReadExternalConfigPathFromMainIni();

    if (configFilePath.isEmpty()) {
        {
            QMutexLocker locker(&g_externalMesConfigMutex);
            g_externalMesResolvedIniPath.clear();
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
            g_externalMesResolvedIniPath.clear();
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
            g_externalMesResolvedIniPath.clear();
        }
        if (errorMessage) {
            *errorMessage = QStringLiteral("MES配置文件不可读: %1").arg(mesConfigPath);
        }
        qWarning() << QStringLiteral("[BYD MES 外部配置] 配置文件不可读: %1").arg(mesConfigPath);
        return false;
    }

    {
        QMutexLocker locker(&g_externalMesConfigMutex);
        g_externalMesResolvedIniPath = mesConfigPath;
    }

    QTextCodec* iniCodec = bydMesPickIniFileCodec(mesConfigPath);
    qDebug() << QStringLiteral("[BYD MES 外部配置] 已登记外部 ini：%1，解析编码: %2；取参时按键 QSettings 读盘，不整表进内存")
                    .arg(mesConfigPath)
                    .arg(QString::fromLatin1(iniCodec->name()));
    return true;
}

void bydmes::clearExternalMesConfig() {
    QMutexLocker locker(&g_externalMesConfigMutex);
    if (!g_externalMesResolvedIniPath.isEmpty()) {
        qDebug() << QStringLiteral("[BYD MES 外部配置] 已清除外部 ini 路径: %1").arg(g_externalMesResolvedIniPath);
    }
    g_externalMesResolvedIniPath.clear();
}

QString bydmes::settingsValue(const QString& key, const QString& fallback) const {
    QString iniPathCopy;
    {
        QMutexLocker locker(&g_externalMesConfigMutex);
        if (g_externalMesResolvedIniPath.isEmpty()) {
            if (!key.isEmpty()) {
                qWarning() << QStringLiteral("[BYD MES 外部配置] 外部 ini 未登记，无法读取键「%1」").arg(key);
            }
            return fallback;
        }
        iniPathCopy = g_externalMesResolvedIniPath;
        QTextCodec* iniCodec = bydMesPickIniFileCodec(iniPathCopy);
        QSettings s(iniPathCopy, QSettings::IniFormat);
        s.setIniCodec(iniCodec);
        s.sync();
        const QString value = bydMesExternalIniValue(s, key);
        if (!value.isEmpty()) {
            qDebug() << "BYD MES settingsValue:" << key << "=" << value;
            return value;
        }
    }
    qWarning() << QStringLiteral("[BYD MES 外部配置] 外部文件未读到参数「%1」（路径：%2），将使用占位/空值")
                        .arg(key, iniPathCopy);
    return fallback;
}

QString bydmes::baseUrl() const {
    QString url = settingsValue("NET").trimmed();
    if (!url.contains("Service.action")) {
        if (url.endsWith('/')) {
            url += "Service.action";
        } else {
            url += "/Service.action";
        }
    }
    return url;
}

// =============================================================================
// ⑤ bydmes参数组包
// =============================================================================


QJsonObject bydmes::buildBydStartParam(const MesPacketData& pack) const {
    // 按 BYD 文档：Start 字段名与示例一致
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID");
    param["SFC"] = pack.sn;
    param["STATION_ID"] = settingsValue("StationID");
    param["LINE"] = settingsValue("Line");
    param["SHOPORDER"] = settingsValue("Resource");
    param["SCHEDULING_ID"] = settingsValue("SchedulingID");
    param["CLIENT_ID"] = settingsValue("ClientID");
    return param;
}

QJsonObject bydmes::buildBydGetCustomDataParam() const {
    // 按 BYD 文档：GetCustomData 仅以下字段
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID");
    param["CLIENT_ID"] = settingsValue("ClientID");
    param["STATION_ID"] = settingsValue("StationID");
    param["PRODUCT_ID"] = settingsValue("PRODUCT_ID");
    return param;
}

QJsonObject bydmes::buildBydGetSnByProcessCodeParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID");
    param["CLIENT_ID"] = settingsValue("ClientID");
    param["SFC"] = pack.sn;
    return param;
}

QJsonArray bydmes::buildBydTestDataList(const MesPacketData& pack, const QString& testTime) const {
    QJsonArray list;
    const QString rowTestTime = pack.testTime.isEmpty() ? testTime : pack.testTime;
    qDebug() << "BYD MES itemvalue:" << pack.itemvalue;

    // itemvalue：多段用 | 分隔；整串可带首尾 |（与 hqmes 一致），也可直接「项1|项2」
    QString inner = pack.itemvalue.trimmed();
    if (inner.length() >= 2 && inner.startsWith(QLatin1Char('|')) && inner.endsWith(QLatin1Char('|'))) {
        inner = inner.mid(1, inner.length() - 2);
    }
    const QStringList keyValuePairs = inner.split(QLatin1Char('|'), Qt::SkipEmptyParts);
    for (const QString& keyValue : keyValuePairs) {
        const QString kv = keyValue.trimmed();
        if (kv.isEmpty()) {
            continue;
        }
        QJsonObject item;
        QString name;
        QString value;
        QString rowMax = pack.maxValue;
        QString rowMin = pack.minValue;
        QString rowStd = pack.standardValue;
        QString rowUnit = pack.unit;
        // 恰好 6 段「NAME:VALUE:MAX:MIN:STANDARD:UNIT」时按 BYD 文档分项填卡控；否则 NAME 仅取首段，VALUE 为首个冒号后全文（值内可含冒号）
        const QStringList parts = kv.split(QLatin1Char(':'), QString::KeepEmptyParts);
        if (parts.size() >= 6) {
            name = parts.at(0).trimmed();
            value = parts.at(1).trimmed();
            rowMax = parts.at(2);
            rowMin = parts.at(3);
            rowStd = parts.at(4);
            rowUnit = parts.at(5);
        } else if (parts.size() == 2) {
            name = parts.at(0).trimmed();
            value = parts.at(1).trimmed();
        } else {
            const int colon = kv.indexOf(QLatin1Char(':'));
            if (colon <= 0) {
                continue;
            }
            name = kv.left(colon).trimmed();
            value = kv.mid(colon + 1).trimmed();
        }
        if (name.isEmpty()) {
            continue;
        }
        item[QStringLiteral("NAME")] = name;
        item[QStringLiteral("VALUE")] = value;
        item[QStringLiteral("MAX_VALUE")] = rowMax;
        item[QStringLiteral("MIN_VALUE")] = rowMin;
        item[QStringLiteral("STANDARD_VALUE")] = rowStd;
        item[QStringLiteral("UNIT")] = rowUnit;
        item[QStringLiteral("TEST_TIME")] = rowTestTime;
        item[QStringLiteral("TEST_USER")] = pack.Employee_ID.isEmpty() ? QStringLiteral("guest") : pack.Employee_ID;
        item[QStringLiteral("LOCATION")] = QString();
        item[QStringLiteral("TEST_END_TIME")] = testTime;
        item[QStringLiteral("TEST_RESULT")] = pack.result;
        item[QStringLiteral("ERROR_CODE")] = (pack.error == QStringLiteral("NULL")) ? QString() : pack.error;
        item[QStringLiteral("PN")] = pack.sn;
        item[QStringLiteral("REMARK")] = pack.remark.isEmpty() ? QString("备注信息"):pack.remark;
        item[QStringLiteral("TEXT")] = QString("备注");
        list.append(item);
        qDebug() << "BYD MES TestDataList item:" << item;
    }

    qDebug() << "BYD MES TestDataList:" << list;
    return list;
}

QJsonObject bydmes::buildBydTestDataCollectParam(const MesPacketData& pack) const {
    const QString testTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString testResult = pack.result;
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID");
    param["CLIENT_ID"] = settingsValue("ClientID");
    param["PROJECT_NAME"] = settingsValue("PROJECT");
    param["TEST_STATION"] = settingsValue(QStringLiteral("Operation"));
    param["TDS_NAME"] = "VH_C5";
    param["SHOPORDER_NO"] = settingsValue("Resource");
    param["LINE_NO"] = settingsValue("Line");
    param["STATION_ID"] = settingsValue("StationID");
    param["FIXTURE_NO"] = pack.machineNo;
    param["STARTIME"] = pack.testTime.isEmpty() ? testTime : pack.testTime;
    param["SN"] = pack.sn;
    param["TEST_RESULT"] = testResult;
    param["ELAPSE_TIME"] = QString::number(qMax(1, pack.elapseTime));
    param["TEST_COUNT"] = QString::number(qMax(1, pack.testCount));
    param["TEST_DATA_LIST"] = buildBydTestDataList(pack, testTime);
    return param;
}

QJsonObject bydmes::buildBydCompleteParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID");
    param["SFC"] = pack.sn;
    param["SCHEDULING_ID"] = settingsValue("SchedulingID");
    param["STATION_ID"] = settingsValue("StationID");
    param["CLIENT_ID"] = settingsValue("ClientID");
    param["REMARK"] = pack.remark.isEmpty() ? QString("备注信息"):pack.remark;
    return param;
}

QJsonObject bydmes::buildBydNcCompleteParam(const MesPacketData& pack) const {
    QJsonObject param;
    param["LOGIN_ID"] = settingsValue("LoginID");
    param["SFC"] = pack.sn;
    param["STATION_ID"] = settingsValue("StationID");
    const QString ncValue = pack.error.isEmpty() ? QString("TEST_ITEM_NG") : pack.error;
    param["NC_CODE"] = ncValue;
    param["NC_CONTEXT"] = "不良原因:" + ncValue + "; 测试结果:" + pack.result;
    param["NC_TYPE"] = ncValue;
    param["SCHEDULING_ID"] = settingsValue("SchedulingID");
    param["CLIENT_ID"] = settingsValue("ClientID");
    return param;
}

QJsonObject bydmes::buildBydAddSfcKeyParam(const MesPacketData& pack) const {
    QJsonObject param;
    param[QStringLiteral("LOGIN_ID")] = settingsValue(QStringLiteral("LoginID"));
    param[QStringLiteral("CLIENT_ID")] = settingsValue(QStringLiteral("ClientID"));
    param[QStringLiteral("SFC")] = pack.sn;
    param[QStringLiteral("DATA_NAME")] = pack.instruct_num;
    param[QStringLiteral("DATA_VALUE")] = pack.itemvalue.trimmed();
    param[QStringLiteral("STATION_NAME")] = settingsValue(QStringLiteral("Operation"));
    param[QStringLiteral("SHOPORDER")] = settingsValue("Resource");
    param[QStringLiteral("QTY")] = pack.testCount;
    return param;
}

// =============================================================================
// ⑥ 过程码mes获取并解析SN
// =============================================================================

QString bydmes::parseSnFromGetSnByProcessCodeResponse(const QByteArray& responseData) const {
    // DATA 为数组或单对象：仅 station==「主板绑定」时取该行 value（避免误用每行自带的 sfc 过程码）
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return {};
    }
    const QJsonObject root = doc.object();
    const QJsonValue dataV = root.value(QStringLiteral("DATA"));
    QJsonArray rows;
    if (dataV.isArray()) {
        rows = dataV.toArray();
    } else if (dataV.isObject()) {
        rows.append(dataV.toObject());
    } else {
        return {};
    }
    static const QString kStationMainBoard = SETTINGS.value(QStringLiteral("Mes/GetSfcKeyBindingItemName"), QStringLiteral("主板")).toString().trimmed();
    for (const QJsonValue& v : rows) {
        if (!v.isObject()) {
            continue;
        }
        const QJsonObject o = v.toObject();
        QString st = o.value(QStringLiteral("name")).toString().trimmed();
        if (st.isEmpty()) {
            st = o.value(QStringLiteral("NAME")).toString().trimmed();
        }
        if (st.isEmpty() || st.compare(kStationMainBoard, Qt::CaseInsensitive) != 0) {
            continue;
        }
        QString val = o.value(QStringLiteral("value")).toString().trimmed();
        if (val.isEmpty()) {
            val = o.value(QStringLiteral("VALUE")).toString().trimmed();
        }
        if (!val.isEmpty()) {
            qDebug() << QStringLiteral("[BYD MES] DATA 行 name=主板，解析 value(SN)=%1").arg(val);
            return val;
        }
    }
    return {};
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

// =============================================================================
// ⑦ HTTP：GET Service.action?method=&param=    bydmes系统的接口请求方法实现
// =============================================================================
//简单校验接口参数的读取情况    参数缺失报错（暂时是日志查看  没有弹窗提示）
bool bydmes::emitIfMissingLoginClientOrNet(const MesPacketData& pack, const QJsonObject& param, const QString& sceneLabel) {
    const QString net = settingsValue(QStringLiteral("NET")).trimmed();
    if (net.isEmpty()) {
        qWarning().noquote() << QStringLiteral("[BYD MES] %1：外部 mes_config 中 NET/JSONURL 未配置或为空").arg(sceneLabel);
        emit operateMesError(pack.mechines,
                             QStringLiteral("BYD MES %1：未配置 NET/JSONURL，无法请求 MES（详见日志）").arg(sceneLabel));
        return true;
    }
    if (bydMesJsonStringFieldEmpty(param, QStringLiteral("LOGIN_ID"))
        || bydMesJsonStringFieldEmpty(param, QStringLiteral("CLIENT_ID"))) {
        qWarning().noquote() << QStringLiteral("[BYD MES] %1：LoginID 或 CLIENT_ID 在外部 mes_config 中未配置或为空").arg(sceneLabel);
        emit operateMesError(pack.mechines,
                             QStringLiteral("BYD MES %1：未配置 LoginID 或 CLIENT_ID（详见日志）").arg(sceneLabel));
        return true;
    }
    return false;
}

QByteArray bydmes::sendRequest(const QString& method, const QJsonObject& param, QString* errorMessage) const {
    QUrl url(baseUrl());
    QUrlQuery query;
    query.addQueryItem("method", method);
    const QString paramStr = bydMesBuildServiceParam(method, param);
    query.addQueryItem("param", paramStr);
    url.setQuery(query);

    // qDebug 默认对 QString 转义内部引号；noquote 便于核对 JSON。URL 中 %7B、%E8… 为百分号编码。
    qDebug().noquote() << QStringLiteral("BYD MES request (param): ") + paramStr;
    qDebug().noquote() << QStringLiteral("BYD MES request (URL): ") + url.toString();
    // const QUrlQuery urlQuery(url);
    // const QString paramDecoded = urlQuery.queryItemValue(QStringLiteral("param"));
    // qDebug().noquote() << QStringLiteral("BYD MES request (param 从 URL 解码): ") + paramDecoded;

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

// =============================================================================
// ⑧ Qmes 对外槽：站前 / 取过程码 SN / 过站上报
// =============================================================================

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
    if (emitIfMissingLoginClientOrNet(pack, param, QStringLiteral("Start"))) {
        return;
    }
    QString networkError;
    const QByteArray responseData = sendRequest("Start", param, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, QStringLiteral("网络请求错误：") + networkError);
        return;
    }

    QString responseText;
    QString errorMessage;
    if (!isSuccessResponse(responseData, &responseText, &errorMessage)) {
        // 与 hqmes/lxmes 一致：仅发 operateMesError，由 QMesManager→test_base::solveMesData 统一 showlog/停测
        qDebug() << "BYD MES ProcessInspection(Start) failed:" << errorMessage;
        emit operateMesError(pack.mechines, QStringLiteral("站前检查失败：") + errorMessage);
        return;
    }

    emit operateMesSucess(pack.mechines);
}

void bydmes::GetTestData(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }
    if (pack.iskeydata == 1) {
        AddSfcKey(pack);
        return;
    }
    QString configError;
    if (!bydmes::loadExternalMesConfig(&configError)) {
        emit operateMesError(pack.mechines, configError);
        return;
    }
    const QString externalIniPath = bydMesPeekExternalResolvedIniPath();
    if (!externalIniPath.isEmpty()) {
        const QString fileName = QFileInfo(externalIniPath).fileName();
        // 文件名不含 mes_config 时（如 autoRead.ini）：易与产线 mes_config 混淆，此处拒绝请求（仅日志 + operateMesError）
        if (!fileName.contains(QStringLiteral("mes_config"), Qt::CaseInsensitive)) {
            qWarning().noquote()
                << QStringLiteral("[BYD MES] 外部文件名「%1」不含 mes_config，已拒绝过程码换 SN 请求，路径：%2")
                       .arg(fileName, externalIniPath);
            emit operateMesError(
                pack.mechines,
                QStringLiteral("BYD MES：外部文件「%1」不是 mes_config 类 ini，已拒绝 GetSfcKeyBySfc（请修正 MES 配置文件路径）")
                    .arg(fileName));
            return;
        }
    }
    if (pack.sn.trimmed().isEmpty()) {
        qWarning().noquote()
            << QStringLiteral("[BYD MES] 过程码/SN 为空，无法调用 GetSfcKeyBySfc。已登记外部文件：%1")
                   .arg(externalIniPath.isEmpty() ? QStringLiteral("(未登记)") : externalIniPath);
        emit operateMesError(pack.mechines,
                             QStringLiteral("BYD MES GetSfcKeyBySfc：pack.sn 为空（请在上位机填入过程码/SN）"));
        return;
    }
    QString method = QStringLiteral("GetSfcKeyBySfc");
    const QJsonObject param = buildBydGetSnByProcessCodeParam(pack);
    if (emitIfMissingLoginClientOrNet(pack, param, method)) {
        return;
    }
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
            QStringLiteral("BYD MES %1 成功但未解析到 SN（请检查 DATA 中是否存在 station=主板绑定 且 value 非空）")
                .arg(method));
        return;
    }
    // 解析到的 SN 通过 sendMesTestvalue 下发工站
    qDebug() << "BYD QuerySnByProcessCode 下发 value(SN)=" << sn;
    emit sendMesTestvalue(pack.mechines, sn);
}

void bydmes::AddSfcKey(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    QString configError;
    if (!bydmes::loadExternalMesConfig(&configError)) {
        emit operateMesError(pack.mechines, configError);
        return;
    }

    if (pack.sn.trimmed().isEmpty()) {
        emit operateMesError(pack.mechines, QStringLiteral("BYD MES AddSfcKey：SFC 为空"));
        return;
    }
    if (pack.itemvalue.trimmed().isEmpty()) {
        emit operateMesError(pack.mechines, QStringLiteral("BYD MES AddSfcKey：DATA_VALUE 为空"));
        return;
    }

    const QJsonObject param = buildBydAddSfcKeyParam(pack);
    if (emitIfMissingLoginClientOrNet(pack, param, QStringLiteral("AddSfcKey"))) {
        return;
    }
    if (bydMesJsonStringFieldEmpty(param, QStringLiteral("DATA_NAME"))
        || bydMesJsonStringFieldEmpty(param, QStringLiteral("STATION_NAME"))
        || bydMesJsonStringFieldEmpty(param, QStringLiteral("SHOPORDER"))) {
        emit operateMesError(pack.mechines,
                             QStringLiteral("BYD MES AddSfcKey：DATA_NAME/STATION_NAME/SHOPORDER 参数缺失"));
        return;
    }

    QString networkError;
    const QByteArray responseData = sendRequest(QStringLiteral("AddSfcKey"), param, &networkError);
    if (!networkError.isEmpty()) {
        emit operateMesError(pack.mechines, QStringLiteral("BYD MES AddSfcKey 请求失败: ") + networkError);
        return;
    }

    QString responseText;
    QString errorMessage;
    if (isSuccessResponse(responseData, &responseText, &errorMessage)) {
        emit operateMesSucess(pack.mechines);
    } else {
        emit operateMesError(pack.mechines, QStringLiteral("BYD MES AddSfcKey 失败: ") + errorMessage);
    }
}

void bydmes::TestPass(MesPacketData pack) {
    if (pack.factory != "byd") {
        return;
    }

    QString configError;
    if (!bydmes::loadExternalMesConfig(&configError)) {
        emit operateMesError(pack.mechines, configError);
        return;
    }

    QJsonObject param = buildBydTestDataCollectParam(pack);
    if (emitIfMissingLoginClientOrNet(pack, param, QStringLiteral("TestDataCollect2MainChild"))) {
        return;
    }

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

    const QString normalizedResult = pack.result;
    const bool isFailResult = (normalizedResult == "FAIL");
    const QString completeMethod = isFailResult ? "NcComplete" : "Complete";
    const QJsonObject completeParam = isFailResult ? buildBydNcCompleteParam(pack) : buildBydCompleteParam(pack);
    if (emitIfMissingLoginClientOrNet(pack, completeParam, completeMethod)) {
        return;
    }

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
