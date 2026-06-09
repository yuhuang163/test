#include "jjmes.h"
#include "QByteArray"
#include "qdebug.h"
#include "qeventloop.h"
#include "qlibrary.h"
#include <QCoreApplication>
#include <QDebug>
#include <QHostInfo>
#include <QList>
#include <QNetworkInterface>
#include <QRegularExpression>
#include <QSysInfo>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
jjmes::jjmes() {
}
// 定义函数来处理字符串
QString processString(const QString& input) {
    // 检查字符串是否以 '|' 开头和结尾，并删除它们
    QString result = input;
    if (result.startsWith('|')) {
        result.remove(0, 1);
    }
    if (result.endsWith('|')) {
        result.chop(1);
    }

    // 将剩余的 '|' 替换为 '@'
    result.replace('|', '@');
    // 将 ':' 替换为 '*'
    result.replace(':', '*');

    return result;
}
void jjmes::LogIn(MesPacketData pack) {
    Q_UNUSED(pack);
}
// sn和工站，站前检测
void jjmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory == "jj") {
        // 构建请求参数
        if (pack.instruct_num.compare("076") == 0) { // 静态电流及mac绑定工站s
            pack.itemvalue = pack.sn + "," + "57";
        } else if (pack.instruct_num.compare("079") == 0) {
            pack.itemvalue = pack.sn + "," + "68"; // 蓝牙wifi充电测试工站
        } else if (pack.instruct_num.compare("083") == 0) {
            pack.itemvalue = pack.sn + "," + "66"; // 老化测试
        } else if (pack.instruct_num.compare("084") == 0) {
            pack.itemvalue = pack.sn + "," + "73"; // 六轴校准指令
        } else {
            QString error_instruct = QString("wrong instruct:%1").arg(pack.instruct_num);
            qDebug() << error_instruct;
        }

        QString url = QString("http://10.144.6.60:8089/print.asmx/BarCodeTable?barcode=085,%1,")
                          .arg(pack.itemvalue);

        qDebug() << url;

        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkRequest request((QUrl(url)));
        QNetworkReply* reply = manager->get(request);

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
            QString responseDataLog = QString("receiving response:%1").arg(response);
            qDebug() << responseDataLog;

            QRegularExpression regex("<value>(.*?)</value>");
            QRegularExpressionMatchIterator i = regex.globalMatch(responseDataLog);

            // 遍历匹配结果，并将匹配到的文本内容保存在字符串变量中
            if (i.hasNext()) {
                QRegularExpressionMatch match = i.next();
                response = match.captured(1); // 提取匹配到的文本内容
                qDebug() << response;
            }
            if (response.contains("OK")) {
                emit operateMesSucess(pack.mechines);
                qDebug() << "过站成功";
            } else {
                emit operateMesError(pack.mechines, "过站失败:" + response);
            }
        } else {
            qDebug() << "Error:" << reply->errorString();
        }

        // 清理资源
        reply->deleteLater();
    }
}
// 通过sn获得绑定的mac地址
void jjmes::GetTestData(MesPacketData pack) {
    if (pack.factory == "jj") {
        QString url =
            QString("http://10.144.6.60:8089/print.asmx/BarCodeTable?barcode=082,%1,").arg(pack.sn);

        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkRequest request((QUrl(url)));
        QNetworkReply* reply = manager->get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString response = QString::fromUtf8(responseData);

            // 输出接收到的响应内容到日志
            QString responseDataLog = QString("收到的mac:%1 ").arg(response);
            qDebug() << responseDataLog;

            QRegularExpression regex(
                "(?<=<value>)(?:[0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}(?=<\\/value>)");
            QRegularExpressionMatchIterator i = regex.globalMatch(responseDataLog);

            // 遍历匹配结果，并输出 MAC 地址
            if (i.hasNext()) {
                QRegularExpressionMatch match = i.next();
                QString macAddress = match.captured();
                qDebug() << "MAC 地址:" << macAddress;
                emit sendMesTestvalue(pack.mechines, macAddress);
                return;
            }

            emit operateMesError(pack.mechines, "无绑定记录 ");
        } else {
            emit operateMesError(pack.mechines, "Error:" + reply->errorString());
            qWarning() << "网络请求错误:" << reply->errorString();
        }
        reply->deleteLater();
    }
}

// 上传结果
void jjmes::TestPass(MesPacketData pack) {
    if (pack.factory == "jj") {
        pack.itemvalue = processString(pack.itemvalue);
        if (pack.instruct_num.compare("076") == 0) { // mac绑定工站
            // pack.itemvalue = pack.sn + "," + pack.mac;
            QRegularExpression re("BTMAC:([0-9A-Fa-f:]{17})");
            QRegularExpressionMatch match = re.match(pack.itemvalue);

            if (match.hasMatch()) {
                pack.itemvalue = pack.sn + "," + match.captured(1);

                qDebug() << "MAC Address:" << match.captured(1);
            } else {
                qDebug() << "No MAC Address found.";
            }
        } else if (pack.instruct_num.compare("079") == 0) { // 蓝牙wifi充电测试工站
            // 外部传入

            pack.itemvalue = pack.sn + "," + pack.result + "," + pack.itemvalue;
        } else if (pack.instruct_num.compare("082") == 0) { // sn获取mac地址
            pack.itemvalue = pack.sn;
        } else if (pack.instruct_num.compare("083") == 0) { // 老化测试
            pack.itemvalue = pack.sn + "," + pack.result + "," + "AGING_TEST_RESULT" + "*" + "1";
        } else if (pack.instruct_num.compare("084") == 0) { // 六轴校准指令
            // 外部传入
        } else {
            QString error_instruct = QString("wrong instruct:%1").arg(pack.instruct_num);
            qDebug() << error_instruct;
        }

        pack.instruct_num = pack.instruct_num.toUtf8();
        pack.itemvalue = pack.itemvalue.toUtf8();

        QString url = QString("http://10.144.6.60:8089/print.asmx/BarCodeTable?barcode=%1,%2,")
                          .arg(QUrl::toPercentEncoding(pack.instruct_num),
                               QUrl::toPercentEncoding(pack.itemvalue));

        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkRequest request((QUrl(url)));
        QNetworkReply* reply = manager->get(request);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        // 请求完成后的处理
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QString response = QString::fromUtf8(responseData);

            // 输出响应内容到日志
            QString responseDataLog = QString("receiving response:%1").arg(response);
            // qDebug() << responseDataLog;

            QRegularExpression regex("<value>(.*?)</value>");
            QRegularExpressionMatchIterator i = regex.globalMatch(responseDataLog);

            // 遍历匹配结果，并将匹配到的文本内容保存在字符串变量中
            if (i.hasNext()) {
                QRegularExpressionMatch match = i.next();
                response = match.captured(1); // 提取匹配到的文本内容
                qDebug() << response;
            }

            if (response.contains("OK")) {
                emit operateMesSucess(pack.mechines);
                qDebug() << "记录OK:";
            } else {
                emit operateMesError(pack.mechines, "记录失败-> " + response);
            }
        } else {
            emit operateMesError(pack.mechines, "Error:" + reply->errorString());
            qDebug() << "Error:" << reply->errorString();
        }
    }
}
