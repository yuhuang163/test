#include "xzwmes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

#include "qeventloop.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

QString normalizeSunwinonBaseUrl(QString url) {
    url = url.trimmed();
    if (url.isEmpty()) {
        url = QStringLiteral("https://hzznyjmes.sunwoda.com/ims-pms");
    }
    while (url.endsWith(QLatin1Char('/'))) {
        url.chop(1);
    }
    return url;
}

QString readSunwinonBaseUrl() {
    QString url = SETTINGS.value(QStringLiteral("Mes/NET"), QString()).toString();
    if (url.trimmed().isEmpty()) {
        url = SETTINGS.value(QStringLiteral("MES/NET"), QString()).toString();
    }
    return normalizeSunwinonBaseUrl(url);
}

} // namespace

xzwmes::xzwmes() {
}

QString xzwmes::apiUrl(const QString& path) const {
    return readSunwinonBaseUrl() + QStringLiteral("/api/device/") + path;
}

void xzwmes::LogIn(MesPacketData pack) {
    Q_UNUSED(pack);
    // xzw 与 xwd 产线一致：无 checkUserDo 登录，直接 groupTest / wipTest。
}

void xzwmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory != QStringLiteral("xzw")) {
        return;
    }

    const QString apiEndpoint = apiUrl(QStringLiteral("groupTest"));
    qDebug() << QStringLiteral("[xzw MES] 站前校验 URL：%1").arg(apiEndpoint);

    QJsonObject requestData;
    requestData[QStringLiteral("sn")] = pack.sn;
    requestData[QStringLiteral("emp")] = pack.Employee_ID;
    requestData[QStringLiteral("machineNo")] = pack.machineNo;

    const QByteArray jsonData = QJsonDocument(requestData).toJson();
    qDebug().noquote() << QStringLiteral("[xzw MES] groupTest request: ") + QString::fromUtf8(jsonData);

    QNetworkAccessManager manager;
    QNetworkRequest httpRequest((QUrl(apiEndpoint)));
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = manager.post(httpRequest, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray responseData = reply->readAll();
        qDebug().noquote() << QStringLiteral("[xzw MES] groupTest response: ") + QString::fromUtf8(responseData);
        const QJsonObject jsonObj = QJsonDocument::fromJson(responseData).object();
        const QString resultCode = jsonObj.value(QStringLiteral("resultCode")).toString();
        const QString resultMsg = jsonObj.value(QStringLiteral("resultMsg")).toString();

        if (resultCode == QStringLiteral("0000")) {
            qDebug() << QStringLiteral("[xzw MES] 站前校验通过 resultCode=") << resultCode << QStringLiteral("resultMsg=") << resultMsg;
            emit operateMesSucess(pack.mechines);
        } else {
            qDebug() << QStringLiteral("[xzw MES] 站前校验失败 resultCode=") << resultCode << QStringLiteral("resultMsg=") << resultMsg;
            emit operateMesError(pack.mechines, QStringLiteral("工序检验失败：") + resultMsg);
        }
    } else {
        qDebug() << QStringLiteral("[xzw MES] groupTest 网络错误：") << reply->errorString();
        emit operateMesError(pack.mechines, QStringLiteral("网络请求错误：") + reply->errorString());
    }

    reply->deleteLater();
}

void xzwmes::TestPass(MesPacketData pack) {
    if (pack.factory != QStringLiteral("xzw")) {
        return;
    }

    const QString apiEndpoint = apiUrl(QStringLiteral("wipTest"));
    qDebug() << QStringLiteral("[xzw MES] 过站上传 URL：%1").arg(apiEndpoint);

    // wipTest 仅接受 PASS/NG；工站 pack.result 可能是 PASS/NG，也可能是 FAIL/失败等
    QString mesResult = pack.result.trimmed().toUpper();
    if (mesResult != QStringLiteral("PASS") && mesResult != QStringLiteral("NG")) {
        if (mesResult == QStringLiteral("FAIL") || pack.result.trimmed() == QStringLiteral("失败")) {
            mesResult = QStringLiteral("NG");
        } else {
            mesResult = QStringLiteral("PASS");
        }
    }

    QJsonObject requestData;
    requestData[QStringLiteral("mSn")] = pack.sn;
    requestData[QStringLiteral("mResult")] = mesResult;
    requestData[QStringLiteral("mUserno")] = pack.Employee_ID;
    requestData[QStringLiteral("mMachineno")] = pack.machineNo;
    requestData[QStringLiteral("mError")] = pack.error;
    requestData[QStringLiteral("mItemvalue")] = pack.itemvalue;

    const QByteArray jsonData = QJsonDocument(requestData).toJson();
    qDebug().noquote() << QStringLiteral("[xzw MES] wipTest request: ") + QString::fromUtf8(jsonData);

    QNetworkAccessManager manager;
    QNetworkRequest httpRequest((QUrl(apiEndpoint)));
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = manager.post(httpRequest, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        const QByteArray responseData = reply->readAll();
        qDebug().noquote() << QStringLiteral("[xzw MES] wipTest response: ") + QString::fromUtf8(responseData);
        const QJsonObject jsonObj = QJsonDocument::fromJson(responseData).object();
        const QString resultCode = jsonObj.value(QStringLiteral("resultCode")).toString();
        const QString resultMsg = jsonObj.value(QStringLiteral("resultMsg")).toString();

        if (resultCode == QStringLiteral("0000")) {
            qDebug() << QStringLiteral("[xzw MES] 过站成功 resultCode=") << resultCode << QStringLiteral("resultMsg=") << resultMsg;
            emit operateMesSucess(pack.mechines);
        } else {
            qDebug() << QStringLiteral("[xzw MES] 过站失败 resultCode=") << resultCode << QStringLiteral("resultMsg=") << resultMsg;
            emit operateMesError(pack.mechines, resultMsg);
        }
    } else {
        qDebug() << QStringLiteral("[xzw MES] wipTest 网络错误：") << reply->errorString();
        emit operateMesError(pack.mechines, QStringLiteral("网络请求错误：") + reply->errorString());
    }

    reply->deleteLater();
}

void xzwmes::GetTestData(MesPacketData pack) {
    if (pack.factory != QStringLiteral("xzw")) {
        return;
    }
    // xzw：分项测试数据由工站写入 pack.itemvalue（|项名:值|…），过站时 wipTest 的 mItemvalue 一并上传，不调 getTestData。
    Q_UNUSED(pack);
}
