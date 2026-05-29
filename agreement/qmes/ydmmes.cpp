#include "ydmmes.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostInfo>
#include <QList>
#include <QNetworkInterface>
#include <QSysInfo>

#include "Abini.h"
#include "qdebug.h"
#include "qeventloop.h"
#include "qlibrary.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

QString getLocalIPAddress() {
    QList<QHostAddress> list = QNetworkInterface::allAddresses();

    for (const QHostAddress& address : list) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && !address.isLoopback()) {
            return address.toString();
        }
    }
    return QString();
}
ydmmes::ydmmes() {
    url = SETTINGS
              .value("Mes/NET", "http://8.210.197.30:88/TIMS.API.WEBSERVICE/TimsApiDatatrans.asmx/TIMS_API_DATATRANS?")
              .toString();
    field = SETTINGS.value("Mes/FIELD", "BT_MAC").toString();
}
QString convertToUrlEncodedString(const QJsonObject& json) {
    QUrlQuery query;
    for (auto it = json.begin(); it != json.end(); ++it) {
        query.addQueryItem(it.key(), it.value().toString());
    }
    return query.toString(QUrl::FullyEncoded);
}

// http://192.168.0.140:88//TIMS.API.WEBSERVICE/TimsApiDatatrans.asmx/TIMS_API_DATATRANS?sMesUser=YDM&sLanguage=S&sF
void ydmmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory == "ydm") {
        // 构建内部 JSON 数据
        QJsonObject internalJson;
        QJsonArray dataArray;
        QJsonObject serial;
        serial["SN"] = pack.sn;
        serial["SN_TYPE"] = "SN";
        serial["WO"] = pack.lotName;
        serial["TERMINAL_NO"] = pack.machineNo;
        dataArray.append(serial);
        internalJson["data"] = dataArray;

        // 将内部 JSON 对象转换为字符串
        QJsonDocument internalDoc(internalJson);
        QString internalJsonString = internalDoc.toJson(QJsonDocument::Compact);

        // 构建 URL 编码的字符串
        QUrlQuery query;
        query.addQueryItem("sMesUser", "YDM");
        query.addQueryItem("sLanguage", "S");
        query.addQueryItem("sFactoryNo", "0");
        query.addQueryItem("sInterfaceType", "TIMS_CHECK_SN_STATION");
        query.addQueryItem("sJson", internalJsonString);
        query.addQueryItem("sUserNo", "SA");
        query.addQueryItem("sClientIp", "127.0.0.0");

        QString urlEncodedString = query.toString(QUrl::FullyEncoded);
        qDebug() << internalJsonString;
        // 输出请求内容到日志
        QString logMsg =
            QString("发送网络请求，URL：%1，参数：%2").arg(url).arg(urlEncodedString);  //（如 %7B 代表 {，%22 代表 "）
        qDebug() << logMsg;

        QNetworkAccessManager manager;
        QNetworkRequest request(url);

        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        // 发送 POST 请求
        QByteArray postDataByteArray = urlEncodedString.toUtf8();
        QNetworkReply* reply = manager.post(request, postDataByteArray);

        // 创建事件循环
        QEventLoop loop;

        // 连接请求完成信号和事件循环退出
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        // 开始事件循环，等待请求完成
        loop.exec();

        // 请求完成后的处理
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString response = QString::fromUtf8(responseData);

            // 输出响应内容到日志
            QString responseDataLog = QString("收到网络响应：%1").arg(response);
            qDebug() << responseDataLog;

            // 提取 XML 中的 JSON 部分
            QRegExp regex("<string[^>]*>(.*)</string>");
            if (regex.indexIn(response) != -1) {
                QString jsonString = regex.cap(1);  // 提取出 JSON 字符串部分

                // 解析提取出来的 JSON 字符串
                QJsonDocument responseDoc = QJsonDocument::fromJson(jsonString.toUtf8());
                qDebug() << "responseDoc" << responseDoc;

                if (!responseDoc.isNull() && responseDoc.isObject()) {
                    QJsonObject responseObject = responseDoc.object();
                    QString errorCode = responseObject.value("status").toString();  // 使用 toString() 方法

                    if (errorCode == "OK") {
                        emit operateMesSucess(pack.mechines);
                        qDebug() << "工站检测";
                    } else {
                        emit operateMesError(pack.mechines, "工站检测：" + response);
                    }
                } else {
                    emit operateMesError(pack.mechines, "无效的 JSON 响应：" + jsonString);
                }
            } else {
                emit operateMesError(pack.mechines, "无法提取 JSON 数据");
            }
        } else {
            emit operateMesError(pack.mechines, reply->errorString());
            qDebug() << "Error:" << reply->errorString() << pack.mechines;
        }

        // 清理资源
        reply->deleteLater();
    }
}

QJsonArray ydmmes::transformString(const QString& input) {
    // 去掉开头和结尾的 '|'
    QString modifiedString = input;
    if (modifiedString.startsWith("|")) {
        modifiedString.remove(0, 1);
    }
    if (modifiedString.endsWith("|")) {
        modifiedString.chop(1);
    }

    // 创建 JSON 数组
    QJsonArray jsonArray;

    // 使用 '|' 分割字符串为多个项目
    QStringList items = modifiedString.split('|');

    // 遍历每个测试项目
    for (const QString& item : items) {
        // 查找键和值
        int colonIndex = item.indexOf(':');
        if (colonIndex != -1) {
            QString key = item.left(colonIndex);
            QString value = item.mid(colonIndex + 1);

            // 构建每个测试项目的 JSON 对象
            QJsonObject jsonObject;
            jsonObject["TestItem"] = key;
            jsonObject["Location"] = "";
            jsonObject["Unit"] = "";
            jsonObject["ValueLow"] = "";
            jsonObject["ValueHigh"] = "";
            jsonObject["ValueTest"] = "";
            jsonObject["TestData"] = value;
            jsonObject["TestResult"] = "OK";
            jsonObject["NgCodeTest"] = "";
            jsonObject["Skip"] = "N";
            jsonObject["Result"] = "OK";

            // 将当前 JSON 对象添加到 JSON 数组中
            jsonArray.append(jsonObject);
        }
    }

    return jsonArray;
}

void ydmmes::TestPass(MesPacketData pack) {
    if (pack.factory == "ydm") {
        // 构建内部 JSON 数据
        QJsonObject internalJson;
        QJsonArray dataArray;

        // 生成数据对象
        QJsonObject dataObject;
        dataObject["SN"] = pack.sn;
        dataObject["SnType"] = "SN";
        dataObject["ModelNo"] = pack.model;  // 机种"P20 Pro"
        dataObject["WO"] = pack.lotName;     //工单
        dataObject["PassLive"] = "MO";
        dataObject["TerminalNo"] = pack.machineNo;
        dataObject["CheckRoute"] = "Y";
        dataObject["BoardCount"] = "1";
        dataObject["QtyOk"] = "1";
        dataObject["QtyNg"] = "0";
        dataObject["FixtureNo"] = "ICT001";
        dataObject["StartTime"] = "2024-12-10 16:50:01";  // 根据实际情况启用
        dataObject["EndTime"] = "2024-12-10 16:50:01";    // 根据实际情况启用
        dataObject["ElapseTime"] = 64;                    // 根据实际情况启用
        dataObject["TestResult"] = pack.result;
        dataObject["Result"] = pack.result;
        dataObject["Detail"] = transformString(pack.itemvalue);

        // 将数据对象添加到 data 数组中
        dataArray.append(dataObject);
        internalJson["data"] = dataArray;

        // 将内部 JSON 对象转换为字符串
        QJsonDocument internalDoc(internalJson);
        QString internalJsonString = internalDoc.toJson(QJsonDocument::Compact);

        // 构建 URL 编码的字符串
        QUrlQuery query;
        query.addQueryItem("sMesUser", "YDM");  // 客户代码
        query.addQueryItem("sLanguage", "S");
        query.addQueryItem("sFactoryNo", "0");
        query.addQueryItem("sInterfaceType", "TIMS_DATA_COLLECT");
        query.addQueryItem("sJson", internalJsonString);
        query.addQueryItem("sUserNo", "SA");
        query.addQueryItem("sClientIp", "127.0.0.0");  // getLocalIPAddress()

        QString urlEncodedString = query.toString(QUrl::FullyEncoded);
        qDebug() << internalJsonString;
        // 输出请求内容到日志
        QString logMsg = QString("发送网络请求，URL：%1，参数：%2").arg(url).arg(urlEncodedString);
        qDebug() << logMsg;

        // 创建 QNetworkAccessManager 并发送 POST 请求
        QNetworkAccessManager manager;
        QNetworkRequest request(url);
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        // 发送 POST 请求
        QByteArray postDataByteArray = urlEncodedString.toUtf8();
        QNetworkReply* reply = manager.post(request, postDataByteArray);

        // 创建事件循环
        QEventLoop loop;

        // 连接请求完成信号和事件循环退出
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

        // 开始事件循环，等待请求完成
        loop.exec();

        // 请求完成后的处理
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString response = QString::fromUtf8(responseData);

            // 输出响应内容到日志
            QString responseDataLog = QString("收到网络响应：%1").arg(response);
            qDebug() << responseDataLog;

            // 提取 XML 中的 JSON 部分
            QRegExp regex("<string[^>]*>(.*)</string>");
            if (regex.indexIn(response) != -1) {
                QString jsonString = regex.cap(1);  // 提取出 JSON 字符串部分

                // 解析提取出来的 JSON 字符串
                QJsonDocument responseDoc = QJsonDocument::fromJson(jsonString.toUtf8());
                qDebug() << "responseDoc" << responseDoc;

                if (!responseDoc.isNull() && responseDoc.isObject()) {
                    QJsonObject responseObject = responseDoc.object();
                    QString status = responseObject.value("status").toString();

                    if (status == "OK") {
                        emit operateMesSucess(pack.mechines);
                        qDebug() << "条码数据采集成功！";
                    } else {
                        emit operateMesError(pack.mechines, "条码数据采集失败：" + response);
                    }

                } else {
                    emit operateMesError(pack.mechines, "无效的 JSON 响应：" + jsonString);
                }
            } else {
                emit operateMesError(pack.mechines, "无法提取 JSON 数据");
            }
        } else {
            qDebug() << "请求失败：" << reply->errorString();
        }

        // 清理资源
        reply->deleteLater();
    }
}

void ydmmes::LogIn(MesPacketData pack) {Q_UNUSED(pack);}
void ydmmes::GetTestData(MesPacketData pack) {Q_UNUSED(pack);}
