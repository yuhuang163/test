#include "lxmes.h"

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
lxmes::lxmes() {
    QSettings settings(SETTING_NAME, QSettings::IniFormat);   settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
  settings.setIniCodec(QTextCodec::codecForName("UTF-8"));
    url = settings.value("Mes/NET", "http://10.16.204.138/Bobcat_CWS_TEST/sfc_response.aspx?").toString();
    field = settings.value("Mes/FIELD", "BT_MAC").toString();
}
// sn和工站，站前检测
void lxmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory == "lx") {
        // 构建请求参数
        QString postData =
            QString("c=QUERY_RECORD&sn=%1&p=unit_process_check&tsid=%2").arg(pack.sn).arg(pack.machineNo);

        // 输出请求内容到日志
        QString logMsg = QString("发送网络请求，URL：%1，参数：%2").arg(url).arg(postData);
        qDebug() << logMsg;

        QNetworkAccessManager manager;
        QNetworkRequest request(url);

        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        // 发送POST请求
        QNetworkReply* reply = manager.post(request, postData.toUtf8());

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

            if (response.contains("0 SFC_OK")) {
                emit operateMesSucess(pack.mechines);
                qDebug() << "过站成功";
            } else {
                emit operateMesError(pack.mechines, "过站失败：" + response);
            }
        } else {
            emit operateMesError(pack.mechines, reply->errorString());
            qDebug() << "Error:" << reply->errorString() << pack.mechines;
        }

        // 清理资源
        reply->deleteLater();
    }
}
void lxmes::LogIn(MesPacketData pack) {}

// sn和上位机号获取mac
void lxmes::GetTestData(MesPacketData pack) {
    if (pack.factory == "lx") {
        // 构建日志信息，记录发送的请求内容
        QString requestData = QString("发送网络请求，URL：%1，参数：sn=%2，p=%3").arg(url).arg(pack.sn).arg(field);
        qDebug() << requestData;

        QNetworkAccessManager manager;
        QNetworkRequest request(url);

        // 设置请求头和请求体
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
        QByteArray postData;
        postData.append("c=QUERY_RECORD");
        postData.append("&sn=" + QUrl::toPercentEncoding(pack.sn));
        postData.append("&p=" + QUrl::toPercentEncoding(field));

        // 发送POST请求
        QNetworkReply* reply = manager.post(request, postData);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString response = QString::fromUtf8(responseData);

            // 输出接收到的响应内容到日志
            QString responseDataLog = QString("收到网络响应：%1").arg(response);
            qDebug() << response;

            if (response.contains("0 SFC_OK")) {
                QStringList lines = response.split("\n\n");
                qDebug() << lines.size();
                if (lines.size() > 1) {
                    qDebug() << lines.size();
                    if (lines[1].startsWith(field + "=") && lines[1].length() >= 7) {
                        QString macID = lines[1].mid(field.length() + 1);  // 获取MAC ID
                        if (!macID.isEmpty()) {
                            qDebug() << "绑定记录BT_MAC=" << macID;
                            emit sendMesTestvalue(pack.mechines, macID);
                            return;
                        }
                    }
                }
            }
            emit operateMesError(pack.mechines, "无绑定记录");
        } else {
            emit operateMesError(pack.mechines, "Error:" + reply->errorString());
            qWarning() << "网络请求错误:" << reply->errorString();
        }
        reply->deleteLater();
    }
}
QString transformString(const QString& input) {
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

void lxmes::TestPass(MesPacketData pack) {
    if (pack.factory == "lx") {
        // 构建请求参数
        pack.itemvalue = transformString(pack.itemvalue);
        QString postData = QString("status=%1&c=ADD_RECORD&model=%2&test_station=%3&station_id=%4&error_code=%5&"
                                   "failure_message=%6&audit_mode=%7&sn=%8&%9")
                               .arg(pack.result == "NG" ? "FAIL" : "PASS")
                               .arg(pack.model)
                               .arg(pack.test_station)
                               .arg(pack.machineNo)
                               .arg("123")  // error_code
                               .arg("")     // failure_message
                               .arg("0")    // 测试模式
                               .arg(pack.sn)
                               .arg(pack.itemvalue);

        // 输出请求内容到日志
        QString logMsg = QString("发送网络请求，URL：%1，参数：%2").arg(url).arg(postData);
        qDebug() << logMsg;

        QNetworkAccessManager manager;
        QNetworkRequest request(url);

        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

        // 发送POST请求
        QNetworkReply* reply = manager.post(request, postData.toUtf8());

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
            // qDebug() << responseDataLog;

            if (response.contains("0 SFC_OK")) {
                emit operateMesSucess(pack.mechines);
                qDebug() << "记录OK:";
            } else {
                emit operateMesError(pack.mechines, "记录失败：" + response);
            }
        } else {
            emit operateMesError(pack.mechines, "Error:" + reply->errorString());
            qDebug() << "Error:" << reply->errorString() << pack.mechines;
        }

        // 清理资源
        reply->deleteLater();
    }
}
