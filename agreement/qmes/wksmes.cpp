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
#pragma execution_character_set(push, "utf-8")
#endif
wksmes::wksmes() {
    url = SETTINGS.value("Mes/NET", "http://218.14.127.107:8880").toString();
    field = SETTINGS.value("Mes/FIELD", "BT_MAC").toString();
}
// sn和工站，站前检测
void wksmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory == "wks") {
        // 构建JSON请求体
        QJsonObject json;

        json["lotName"] = pack.lotName;
        json["station_name"] = pack.machineNo;
        json["userID"] = pack.userNo;
        json["stepName"] = pack.test_station; //工序名称
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
        QNetworkRequest request(url + "/WKSwip/Check2");

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
                QJsonObject infoObject = responseObject.value("info").toObject();
                QString productversion = infoObject.value("productVersion").toString();
                QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
                if (errorCode == 0) {
                    if (productversion == softwareVersion)
                        emit operateMesSucess(pack.mechines);
                    else
                        emit operateMesError(pack.mechines,
                                             "版本比对有问题:" + productversion + "本地:" + softwareVersion);

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
void wksmes::LogIn(MesPacketData pack) {
    Q_UNUSED(pack);
}

// sn和上位机号获取mac
void wksmes::GetTestData(MesPacketData pack) {
    Q_UNUSED(pack);
}
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
        if (pack.result == "NG") {
            pack.itemvalue = "";
        }
        // 构建 JSON 请求体
        QJsonObject json;
        json["lotName"] = pack.lotName;
        json["station_name"] = pack.machineNo;
        json["userID"] = pack.userNo;
        json["stepName"] = pack.test_station;
        json["SCAN_TYPE"] = 22;

        // 构建 inputSerials 项
        QJsonObject inputSerial;

        // 1. 添加 serialNumber
        if (!pack.sn.isEmpty()) {
            inputSerial["serialNumber"] = pack.sn;
        }
        qDebug() << "mes数据为" << pack.itemvalue << pack.error;
        // 2. 添加 testdatas (如果 pack.itemvalue 存在)
        if (!pack.itemvalue.isEmpty()) {
            QJsonArray testdatasArray;
            QString inputString = pack.itemvalue.mid(1, pack.itemvalue.length() - 2); // 移除起始和结束的'|'

            // 分割字符串成键值对
            QStringList keyValuePairs = inputString.split("|");

            // 遍历键值对并添加到 JSON 对象
            for (const QString& keyValue : keyValuePairs) {
                int index = keyValue.indexOf(":");
                if (index != -1) {
                    QString key = keyValue.left(index).trimmed();
                    QString value = keyValue.mid(index + 1).trimmed();
                    QJsonObject interTestdata;
                    interTestdata["key"] = key;
                    interTestdata["value"] = value;
                    testdatasArray.append(interTestdata);
                }
            }

            // 如果 testdatasArray 不为空，添加到 inputSerial
            if (!testdatasArray.isEmpty()) {
                inputSerial["testdatas"] = testdatasArray;
            }

            // 4. 添加 assemblys (如果 pack.mac 存在)
            if (!pack.mac.isEmpty()) {
                QJsonArray assemblysArray;
                QJsonObject interTestdata;
                interTestdata["key"] = "MAC";
                interTestdata["value"] = pack.mac;
                assemblysArray.append(interTestdata);
                // 如果 assemblysArray 不为空，添加到 inputSerial
                if (!assemblysArray.isEmpty()) {
                    inputSerial["assemblys"] = assemblysArray;
                }
            }
        }

        // 3. 添加 defects (如果 pack.error 存在)
        if (!pack.error.isEmpty()) {
            QJsonArray defectsArray;
            QString errorinputString = pack.error.mid(1, pack.error.length() - 2);

            // 分割字符串成键值对
            QStringList defectsPairs = errorinputString.split("|");

            // 遍历键值对并添加到 JSON 对象
            for (const QString& Defects : defectsPairs) {
                int index = Defects.indexOf(":");
                if (index != -1) {
                    QJsonObject interDefect;
                    QString key = Defects.left(index).trimmed();
                    QString value = Defects.mid(index + 1).trimmed();
                    interDefect["defectType"] = key;
                    interDefect["defectCode"] = value;
                    defectsArray.append(interDefect);
                }
            }

            // 如果 defectsArray 不为空，添加到 inputSerial
            if (!defectsArray.isEmpty()) {
                inputSerial["defects"] = defectsArray;
            }
        }

        // 5. 将 inputSerial 添加到 inputSerials 数组中
        QJsonArray inputSerials;
        inputSerials.append(inputSerial);

        // 添加 inputSerials 到主 JSON 对象
        json["inputSerials"] = inputSerials;

        // 将JSON对象转换为字符串
        QJsonDocument doc(json);
        QByteArray postDataByteArray = doc.toJson(QJsonDocument::Compact);

        // 输出请求内容到日志
        QString logMsg = QString("发送网络请求，URL：%1Complete2，参数：%2").arg(url).arg(QString(postDataByteArray));
        qDebug() << logMsg;

        QNetworkAccessManager manager;
        QNetworkRequest request(url + "/WIP/Complete2");

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
