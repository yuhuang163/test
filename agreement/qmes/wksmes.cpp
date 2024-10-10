#include "wksmes.h"

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
#    pragma execution_character_set("utf-8")
#endif
wksmes::wksmes() {
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    url = settings.value("Mes/NET", "http://IP/WIP/").toString();
    field = settings.value("Mes/FIELD", "BT_MAC").toString();
}
// sn和工站，站前检测
void wksmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory == "wks") {
        // 构建JSON请求体
        QJsonObject json;
        json["station_name"] = pack.machineNo;
        json["userID"] = pack.userNo;
        json["stepName"] = "SMT测试";
        json["SCAN_TYPE"] = 22;

        QJsonArray inputSerials;
        QJsonObject serial;
        serial["serialNumber"] = pack.sn;
        inputSerials.append(serial);
        json["inputSerials"] = inputSerials;

        // 将JSON对象转换为字符串
        QJsonDocument doc(json);
        QByteArray postDataByteArray = doc.toJson(QJsonDocument::Compact);

        // 输出请求内容到日志
        QString logMsg = QString("发送网络请求，URL：%1Check2，参数：%2").arg(url).arg(QString(postDataByteArray));
        qDebug() << logMsg;

        QNetworkAccessManager manager;
        QNetworkRequest request(url + "Check2");

        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
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

            // 解析响应JSON数据
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
            if (!responseDoc.isNull() && responseDoc.isObject()) {
                QJsonObject responseObject = responseDoc.object();
                int errorCode = responseObject.value("errorcode").toInt();

                if (errorCode == 0) {
                    emit operateMesSucess(pack.mechines);
                    qDebug() << "过站成功";
                } else {
                    emit operateMesError(pack.mechines, "过站失败：" + response);
                }
            } else {
                emit operateMesError(pack.mechines, "无效的JSON响应：" + response);
            }
        } else {
            emit operateMesError(pack.mechines, reply->errorString());
            qDebug() << "Error:" << reply->errorString() << pack.mechines;
        }

        // 清理资源
        reply->deleteLater();
    }
}
void wksmes::LogIn(MesPacketData pack) {}

// sn和上位机号获取mac
void wksmes::GetTestData(MesPacketData pack) {}
QString wksmes::transformString(const QString& input) {
    // 去掉开头和结尾的 '|'
    QString modifiedString = input;
    if (modifiedString.startsWith("|")) {
        modifiedString.remove(0, 1);
    }
    if (modifiedString.endsWith("|")) {
        modifiedString.chop(1);
    }

    // 将剩余的 '|' 替换为 '&'
    modifiedString.replace("|", "&");

    // 将 ':' 替换为 '='
    modifiedString.replace(":", "=");

    return modifiedString;
}
// status：PASS表示测试OK，MES正常过站；FAIL表示测试NG，MES记录并重测
// c(固定参数):ADD_RECORD
// model：机种名称（可在测试软件上配置）
// test_station：制程名称（可在测试软件上配置）
// station_id：站点名称（可在测试软件上配置，PS：station_id=接口1的tsid）
// error_code：不良代码（需和MES的不良代码一直）
// failure_message：错误信息,如果测试PASS可以为空。
// audit_mode:测试模式0或1(0代表正常测试,1代表验证模式)
// sn：主SN
// BT_MAC：物理MAC地址（格式用大写的12位字符串，不要加分隔符，例：CCBBDD0001CF）

void wksmes::TestPass(MesPacketData pack) {
    if (pack.factory == "wks") {
        // 构建JSON请求体
        QJsonObject json;

        json["station_name"] = pack.test_station;
        json["userID"] = pack.userNo;     // 添加用户ID
        json["stepName"] = "SMT-贴条码";  // 可以根据需要更改
        json["SCAN_TYPE"] = 22;           // 根据需要设置

        QJsonArray inputSerials;
        QJsonObject serial;
        serial["serialNumber"] = pack.sn;
        inputSerials.append(serial);
        json["inputSerials"] = inputSerials;

        // 将JSON对象转换为字符串
        QJsonDocument doc(json);
        QByteArray postDataByteArray = doc.toJson(QJsonDocument::Compact);

        // 输出请求内容到日志
        QString logMsg = QString("发送网络请求，URL：%1，参数：%2").arg(url).arg(QString(postDataByteArray));
        qDebug() << logMsg;

        QNetworkAccessManager manager;
        QNetworkRequest request(url);

        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
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

            // 解析响应JSON数据
            QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
            if (!responseDoc.isNull() && responseDoc.isObject()) {
                QJsonObject responseObject = responseDoc.object();
                int errorCode = responseObject.value("errorcode").toInt();

                if (errorCode == 0) {
                    emit operateMesSucess(pack.mechines);
                    qDebug() << "记录成功";
                } else {
                    emit operateMesError(pack.mechines, "记录失败：" + response);
                }
            } else {
                emit operateMesError(pack.mechines, "无效的JSON响应：" + response);
            }
        } else {
            emit operateMesError(pack.mechines, "Error:" + reply->errorString());
            qDebug() << "Error:" << reply->errorString() << pack.mechines;
        }

        // 清理资源
        reply->deleteLater();
    }
}
