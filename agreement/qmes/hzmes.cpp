#include "hzmes.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>

#include "Abini.h"
#include "qdebug.h"
#include "qeventloop.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QString mesIniString(const QString& mesKey, const QString& altKey, const QString& defaultValue)
{
    QString v = SETTINGS.value(mesKey).toString().trimmed();
    if (v.isEmpty() && mesKey != altKey)
        v = SETTINGS.value(altKey).toString().trimmed();
    return v.isEmpty() ? defaultValue : v;
}

}  // namespace

hzmes::hzmes()
{
    url = mesIniString(QStringLiteral("Mes/NET"), QStringLiteral("MES/NET"),
                       QStringLiteral("http://10.0.2.5/mrs"));
    field = mesIniString(QStringLiteral("Mes/FIELD"), QStringLiteral("MES/FIELD"), QStringLiteral("wifimac"));
}

void hzmes::reloadMesConfig()
{
    url = mesIniString(QStringLiteral("Mes/NET"), QStringLiteral("MES/NET"),
                       QStringLiteral("http://10.0.2.5/mrs"));
    field = mesIniString(QStringLiteral("Mes/FIELD"), QStringLiteral("MES/FIELD"), QStringLiteral("wifimac"));
}

void hzmes::LogIn(MesPacketData pack) {
    Q_UNUSED(pack);
}

void hzmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory != QStringLiteral("hz")) {
        return;
    }
    reloadMesConfig();

    QString baseUrl = url.trimmed();
    while (baseUrl.endsWith(QLatin1Char('/'))) {
        baseUrl.chop(1);
    }
    if (!baseUrl.endsWith(QStringLiteral("/mrs"))) {
        baseUrl += QStringLiteral("/mrs");
    }

    const QString prodNo = pack.lotName.trimmed().isEmpty() ? QStringLiteral("auto") : pack.lotName.trimmed();
    const bool retest = SETTINGS.value(QStringLiteral("Mes/RETEST"), false).toBool();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("pcbSeq"), pack.sn.trimmed());
    query.addQueryItem(QStringLiteral("prodNo"), prodNo);
    query.addQueryItem(QStringLiteral("stationNo"), pack.machineNo.trimmed());
    query.addQueryItem(QStringLiteral("retest"), retest ? QStringLiteral("true") : QStringLiteral("false"));

    QUrl requestUrl(baseUrl + QStringLiteral("/checkRoute"));
    requestUrl.setQuery(query);
    qDebug() << QStringLiteral("华庄MES站前检查 URL：") << requestUrl.toString();

    QNetworkAccessManager manager;
    QNetworkRequest request(requestUrl);
    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        emit operateMesError(pack.mechines, QStringLiteral("站前检查网络错误：") + reply->errorString());
        reply->deleteLater();
        return;
    }

    const QString response = QString::fromUtf8(reply->readAll()).trimmed();
    reply->deleteLater();
    qDebug() << QStringLiteral("华庄MES站前检查响应：") << response;

    const QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (!doc.isObject()) {
        emit operateMesError(pack.mechines, QStringLiteral("站前检查失败：") + response);
        return;
    }

    const QJsonObject obj = doc.object();
    const QJsonValue msgIdValue = obj.value(QStringLiteral("msgId"));
    const bool ok = (msgIdValue.isString() && msgIdValue.toString() == QStringLiteral("0")) || (msgIdValue.isDouble() && msgIdValue.toInt() == 0);
    if (ok) {
        emit operateMesSucess(pack.mechines);
        qDebug() << QStringLiteral("华庄MES站前检查成功");
        return;
    }

    const QString msgStr = obj.value(QStringLiteral("msgStr")).toString();
    emit operateMesError(pack.mechines,
                         QStringLiteral("站前检查失败：") + (msgStr.isEmpty() ? response : msgStr));
}

void hzmes::GetTestData(MesPacketData pack) {
    // MAC 由各工站扫 SN 后 parseMacFromSn 本地解析，不再请求 getField
    Q_UNUSED(pack);
}

void hzmes::TestPass(MesPacketData pack) {
    if (pack.factory != QStringLiteral("hz")) {
        return;
    }
    reloadMesConfig();

    QString baseUrl = url.trimmed();
    while (baseUrl.endsWith(QLatin1Char('/'))) {
        baseUrl.chop(1);
    }
    if (!baseUrl.endsWith(QStringLiteral("/mrs"))) {
        baseUrl += QStringLiteral("/mrs");
    }

    const QString prodNo = pack.lotName.trimmed().isEmpty() ? QStringLiteral("auto") : pack.lotName.trimmed();
    const QString mesResult = (pack.result.compare(QStringLiteral("NG"), Qt::CaseInsensitive) == 0 || pack.result.compare(QStringLiteral("FAIL"), Qt::CaseInsensitive) == 0)
        ? QStringLiteral("FAIL")
        : QStringLiteral("PASS");

    // 华庄 testItem：测试项目日志，逗号隔开，可空；工站 itemvalue 用 | 分段
    QString testItem = pack.itemvalue.trimmed();
    if (!testItem.isEmpty()) {
        if (testItem.startsWith(QLatin1Char('|'))) {
            testItem.remove(0, 1);
        }
        if (testItem.endsWith(QLatin1Char('|'))) {
            testItem.chop(1);
        }
        testItem.replace(QLatin1Char('|'), QLatin1Char(','));
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("pcbSeq"), pack.sn.trimmed());
    query.addQueryItem(QStringLiteral("prodNo"), prodNo);
    query.addQueryItem(QStringLiteral("stationNo"), pack.machineNo.trimmed());
    query.addQueryItem(QStringLiteral("result"), mesResult);
    query.addQueryItem(QStringLiteral("testItem"), testItem);
    query.addQueryItem(QStringLiteral("userNo"), pack.userNo.trimmed());
    query.addQueryItem(QStringLiteral("weight"), QStringLiteral("0"));
    query.addQueryItem(QStringLiteral("packNo"), QString());
    query.addQueryItem(QStringLiteral("rmk1"), QString());
    query.addQueryItem(QStringLiteral("rmk2"), QString());
    query.addQueryItem(QStringLiteral("rmk3"), QString());
    query.addQueryItem(QStringLiteral("rmk4"), QString());

    QUrl requestUrl(baseUrl + QStringLiteral("/createRoute"));
    requestUrl.setQuery(query);
    qDebug() << QStringLiteral("华庄MES过站 URL：") << requestUrl.toString();

    QNetworkAccessManager manager;
    QNetworkRequest request(requestUrl);
    QNetworkReply* reply = manager.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        emit operateMesError(pack.mechines, QStringLiteral("过站网络错误：") + reply->errorString());
        reply->deleteLater();
        return;
    }

    const QString response = QString::fromUtf8(reply->readAll()).trimmed();
    reply->deleteLater();
    qDebug() << QStringLiteral("华庄MES过站响应：") << response;

    const QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
    if (!doc.isObject()) {
        emit operateMesError(pack.mechines, QStringLiteral("过站失败：") + response);
        return;
    }

    const QJsonObject obj = doc.object();
    const QJsonValue msgIdValue = obj.value(QStringLiteral("msgId"));
    const bool ok = (msgIdValue.isString() && msgIdValue.toString() == QStringLiteral("0")) || (msgIdValue.isDouble() && msgIdValue.toInt() == 0);
    if (ok) {
        emit operateMesSucess(pack.mechines);
        qDebug() << QStringLiteral("华庄MES过站数据上报成功，上报测试结果：%1").arg(mesResult);
        return;
    }

    const QString msgStr = obj.value(QStringLiteral("msgStr")).toString();
    emit operateMesError(pack.mechines, QStringLiteral("过站失败：") + (msgStr.isEmpty() ? response : msgStr));
}
