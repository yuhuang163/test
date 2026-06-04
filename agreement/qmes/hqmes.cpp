#include "hqmes.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
hqmes::hqmes() {}
void hqmes::GetTestData(MesPacketData pack) {
    if (pack.factory == "hq") {
        ProcessInspection(pack);
    }
}

void hqmes::getLocalInfo() {
    // 获取本地主机名
    hostName = QHostInfo::localHostName();

    qDebug() << "Host Name: " << hostName;

    // 获取本地IP地址
    QList<QHostAddress> ipAddressesList = QHostInfo::fromName(hostName).addresses();
    for (int i = 0; i < ipAddressesList.size(); ++i) {
        if (ipAddressesList.at(i).toIPv4Address()) {
            IP_Adress = ipAddressesList.at(i).toString();
            qDebug() << "IPv4 Address: " << ipAddressesList.at(i).toString();
        }
    }

    // 获取本地MAC地址
    QList<QNetworkInterface> interfaceList = QNetworkInterface::allInterfaces();
    for (int i = 0; i < interfaceList.size(); ++i) {
        QNetworkInterface currentInterface = interfaceList.at(i);
        // qDebug() << "Interface Name: " << currentInterface.name();
        // qDebug() << "MAC Address: " << currentInterface.hardwareAddress();
        MAC_Adress = currentInterface.hardwareAddress();
    }
    // 获取MAC地址并去掉冒号
    MAC_Adress = MAC_Adress.remove(":");
}

void hqmes::LogIn(MesPacketData pack) {
    if (pack.factory == "hq") {
        getLocalInfo();
        QNetworkAccessManager* manager = new QNetworkAccessManager(this);
        QNetworkRequest request(QUrl("http://10.51.8.48/API.MD1D/HQAPI/Token"));

        // 设置请求头
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 构建请求参数
        QJsonObject requestData;
        requestData["HEAD"] = QJsonObject{{"H_GUID", "F60A9855F87E4510A3CA39045C57325E"}, {"H_OP", "MesGetToken"}};
        requestData["MAIN"] =
            QJsonObject{{"G_USER", pack.userNo},
                        {"G_PASSWORD", pack.password},  // encryption(password)
                        {"G_OP_PC", hostName},
                        {"G_HOSTINFO", QJsonArray{QJsonObject{{"G_OP_IP", IP_Adress}, {"G_OP_MAC", MAC_Adress}}}}};

        // 将 JSON 数据转换为 QByteArray
        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();

        // 发送 POST 请求
        QNetworkReply* reply = manager->post(request, jsonData);
        connect(reply, SIGNAL(finished()), this, SLOT(onNetworkReplyFinished()));
    }
}
void hqmes::onNetworkReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (reply && reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();

        // 解析 JSON 数据
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc["HEAD"].toObject();
        QJsonObject MjsonObj = jsonDoc["MAIN"].toObject();

        G_TOKEN = MjsonObj.value("G_TOKEN").toString();
        // 获取返回的结果
        QString resultCode = jsonObj.value("H_RET").toString();

        if (resultCode == "00001") {
            // 登录成功
            qDebug() << "LogIn successful";
            MesGetGlobalInfo_hq();
            emit sendMesState(1);
        } else {
            emit sendMesState(0);
            qDebug() << "数值为" << resultCode;
            // 登录失败，处理错误信息
            if (jsonObj.contains("H_MSG")) {
                QString resultMsg = jsonObj.value("H_MSG").toString();
                qDebug() << "LogIn failed. Error: " << resultMsg;
                emit operateMesError(1, "登陆失败：" + resultMsg);
            } else {
                emit operateMesError(1, "登陆失败，没有报错信息");
            }
            // 打印整个 JSON 对象的字符串表示形式
            qDebug() << "Full JSON response:" << jsonDoc.toJson(QJsonDocument::Indented);
        }

        reply->deleteLater();
    } else {
        // 处理网络请求错误
        emit operateMesError(1, "Network request error" + reply->errorString());
        qDebug() << "Network request error:" << reply->errorString();
    }
}

void hqmes::MesGetGlobalInfo_hq() {
    // 接口地址
    QString url = "http://10.51.8.48/API.MD1D/HQAPI/MES";

    // 构建请求参数
    QJsonObject head;
    head["H_GUID"] = "62A2107CB0874BD7A39B9D3A7E206551";
    head["H_OP"] = "MesGetGlobalInfo";
    head["H_TOKEN"] = G_TOKEN;

    QJsonObject main;
    main["G_SCT_TYPE"] = 0;
    main["G_OP_PC"] = hostName;

    QJsonArray hostInfoArray;
    QJsonObject hostInfo;
    hostInfo["G_OP_IP"] = IP_Adress;
    hostInfo["G_OP_MAC"] = MAC_Adress;
    hostInfoArray.append(hostInfo);

    main["G_HOSTINFO"] = hostInfoArray;

    QJsonObject requestData;
    requestData["HEAD"] = head;
    requestData["MAIN"] = main;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply* reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc["HEAD"].toObject();
        QString resultCode = jsonObj.value("H_RET").toString();

        if (resultCode == "00001") {
            // emit operateMesSucess(1);
            qDebug() << "操作成功";
        } else {
            if (jsonObj.contains("H_MSG")) {
                QString resultMsg = jsonObj.value("H_MSG").toString();
                qDebug() << "MesGetGlobalInfo_hq failed. Error: " << resultMsg;
                emit operateMesError(1, "检查失败：" + resultMsg);
            } else {
                emit operateMesError(1, "检查失败，没有报错信息");
            }
        }
    } else {
        // 处理网络错误
        qDebug() << "网络请求错误：" << reply->errorString();
    }

    reply->deleteLater();
}

void hqmes::ProcessInspection(MesPacketData pack) {
    if (pack.factory == "hq") {
        // 接口地址
        QString url = "http://10.51.8.48/API.MD1D/HQAPI/MES";

        // 构建请求参数
        QJsonObject head;
        head["H_GUID"] = "62A2107CB0874BD7A39B9D3A7E206551";
        head["H_OP"] = "MesCheckFlow";
        head["H_ACTION"] = pack.action;
        head["H_TOKEN"] = G_TOKEN;

        QJsonObject main;
        main["G_GROUP"] = pack.line;

        main["G_WS"] = pack.machineNo;  // 站位
        main["G_OP_LINE"] = pack.line;
        main["G_OP_PC"] = hostName;
        main["G_OP_SHIFT"] = "D";  // 白班

        QJsonArray hostInfoArray;
        QJsonObject hostInfo;
        hostInfo["G_OP_IP"] = IP_Adress;
        hostInfo["G_OP_MAC"] = MAC_Adress;
        hostInfoArray.append(hostInfo);

        main["G_HOSTINFO"] = hostInfoArray;
        main["OP"] = "MesCheckFlow";
        main["G_ASSIGN_METHOD"] = 0;
        main["G_ASSIGN_SN"] = "";
        // main["ToolVersion"] = "HRSMES_5ATM_UpperCover";
        main["G_SN"] = pack.sn;
        main["G_SN_TYPE"] = "1";
        main["H_ACTION"] = pack.action;

        QJsonObject requestData;
        requestData["HEAD"] = head;
        requestData["MAIN"] = main;

        qDebug() << "ProcessInspection请求内容" << requestData;

        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();

        QNetworkAccessManager manager;
        QNetworkRequest request((QUrl(url)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
        QNetworkReply* reply = manager.post(request, jsonData);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
            // 将QJsonDocument转换为QByteArray
            QByteArray jsonData = jsonDoc.toJson();
            // 将QByteArray转换为QString
            QString jsonString = QString::fromUtf8(jsonData);

            QJsonObject jsonObj = jsonDoc["HEAD"].toObject();
            QString resultCode = jsonObj.value("H_RET").toString();
            QJsonObject MAINjsonObj = jsonDoc["MAIN"].toObject();
            QString DATA_MES = MAINjsonObj.value("BTMAC").toString();

            if (resultCode == "00001") {
                if (pack.is_hq_send_mac)
                    emit sendMesTestvalue(pack.mechines, jsonString);
                else
                    emit operateMesSucess(pack.mechines);
            } else {
                if (jsonObj.contains("H_MSG")) {
                    QString resultMsg = jsonObj.value("H_MSG").toString();
                    qDebug() << "ProcessInspection failed. Error: " << resultMsg;
                    emit operateMesError(pack.mechines, "站前检查失败：" + resultMsg);
                } else {
                    emit operateMesError(pack.mechines, "站前检查失败，没有报错信息");
                }
            }
        } else {
            // 处理网络错误
            emit operateMesError(pack.mechines, "网络请求错误：" + reply->errorString());
            qDebug() << "网络请求错误：" << reply->errorString();
        }

        reply->deleteLater();
    }
}

void hqmes::TestPass(MesPacketData pack) {
    if (pack.factory == "hq") {
        // 接口地址
        QString url = "http://10.51.8.48/API.MD1D/HQAPI/MES";

        // 构建请求参数
        QJsonObject head;

        head["H_GUID"] = "62A2107CB0874BD7A39B9D3A7E206551";
        head["H_OP"] = "MesUpdateInfo";
        head["H_ACTION"] = pack.action;
        head["H_TOKEN"] = G_TOKEN;

        QJsonObject main;
        main["G_GROUP"] = pack.line;

        main["G_SN"] = pack.sn;
        main["G_SN_TYPE"] = "1";
        main["G_WS"] = pack.machineNo;
        main["G_USER"] = pack.userNo;

        main["G_OP_LINE"] = pack.line;
        main["G_OP_PC"] = hostName;
        main["G_OP_SHIFT"] = "D";
        qDebug() << "请求main内容" << main;

        QJsonArray errorArray;
        QJsonObject errorObject;
        errorObject["G_CODE"] = pack.result;
        errorObject["G_DESC"] = "";
        errorArray.append(errorObject);
        main["G_ERROR"] = errorArray;

        QJsonObject allData;
        QString inputString;
        inputString = pack.itemvalue.mid(1, pack.itemvalue.length() - 2);
        // 分割字符串成键值对
        QStringList keyValuePairs = inputString.split("|");
        // 遍历键值对并添加到 JSON 对象
        for (const QString& keyValue : keyValuePairs) {
            QStringList pair = keyValue.split(":");

            if (pair.length() == 2) {
                QString key = pair[0].trimmed();
                QString value = pair[1].trimmed();
                allData[key] = value;
            } else {
                int index = keyValue.indexOf(":");
                QString key = keyValue.left(index);
                QString value = keyValue.mid(index + 1);
                allData[key] = value;
            }
        }

        qDebug() << "请求内容" << allData;

        main["G_ALL_DATA"] = allData;

        QJsonArray hostInfoArray;
        QJsonObject hostInfo;
        hostInfo["G_OP_IP"] = IP_Adress;
        hostInfo["G_OP_MAC"] = MAC_Adress;
        hostInfoArray.append(hostInfo);
        main["G_HOSTINFO"] = hostInfoArray;

        main["OP"] = "MesUpdateInfo";
        main["H_ACTION"] = pack.action;
        main["G_ERROR_G_CODE"] = "0";
        main["G_ERROR_G_DESC"] = "";

        QJsonObject requestData;
        requestData["HEAD"] = head;
        requestData["MAIN"] = main;

        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();

        QNetworkAccessManager manager;
        QNetworkRequest request((QUrl(url)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
        QNetworkReply* reply = manager.post(request, jsonData);

        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError) {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
            QJsonObject jsonObj = jsonDoc["HEAD"].toObject();
            QString resultCode = jsonObj.value("H_RET").toString();

            if (resultCode == "00001") {
                emit operateMesSucess(pack.mechines);
            } else {
                if (jsonObj.contains("H_MSG")) {
                    QString resultMsg = jsonObj.value("H_MSG").toString();
                    qDebug() << "TestPass failed. Error: " << resultMsg;
                    emit operateMesError(pack.mechines, "测试上传失败：" + resultMsg);
                } else {
                    emit operateMesError(pack.mechines, "测试上传失败，没有报错信息");
                }
            }
        } else {
            // 处理网络错误
            qDebug() << "网络请求错误：" << reply->errorString();
        }
        reply->deleteLater();
    }
}

// extern "C" __declspec(dllimport) const char* api_set_password(const char* inputStr);
// QString hqmes::encryption(QString password) {

//     const char* inputStr = "Hello World";
//     const char* result = api_set_password(inputStr);
//     qDebug() << "The encrypted string is:" << result<<"结束";
//     const char* passwordStr = password.toUtf8().constData();
//     return api_set_password(passwordStr); // 返回一个空字符串或其他适当的值
// }
