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

hzmes::hzmes() {
    url = SETTINGS.value(QStringLiteral("Mes/NET"), QStringLiteral("http://10.0.2.5/mrs")).toString().trimmed();
    field = SETTINGS.value(QStringLiteral("Mes/FIELD"), QStringLiteral("wifimac")).toString().trimmed();
}

void hzmes::LogIn(MesPacketData pack) {
    Q_UNUSED(pack);
}

void hzmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory != QStringLiteral("hz")) {
        return;
    }

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

    QString testItem = pack.itemvalue.trimmed();
    if (testItem.startsWith(QLatin1Char('|'))) {
        testItem.remove(0, 1);
    }
    if (testItem.endsWith(QLatin1Char('|'))) {
        testItem.chop(1);
    }
    testItem.replace(QLatin1Char('|'), QLatin1Char(','));
    testItem.replace(QLatin1Char(':'), QLatin1Char('='));

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
        qDebug() << QStringLiteral("华庄MES过站成功");
        return;
    }

    const QString msgStr = obj.value(QStringLiteral("msgStr")).toString();
    emit operateMesError(pack.mechines, QStringLiteral("过站失败：") + (msgStr.isEmpty() ? response : msgStr));
}
