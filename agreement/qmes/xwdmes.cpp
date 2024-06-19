#include "xwdmes.h"
#include "qeventloop.h"

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
xwdmes::xwdmes() {}
// 标签不存在，表示sn有问题
// QString M_USERNO, QString M_PASSWORD, QString M_MACHINENO

void xwdmes::LogIn(MesPacketData pack)
{
    if (pack.factory == "xwd")
    {
        QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/checkUserDo";
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        QNetworkRequest request((QUrl(url)));
        // 设置请求头，使用 setHeader 方法
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
        // 构建请求参数
        QJsonObject requestData;
        requestData["userNo"] = pack.userNo;
        requestData["password"] = pack.password;
        requestData["machineNo"] = pack.machineNo;
        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();
        // 发送 POST 请求
        QNetworkReply *reply = manager->post(request, jsonData);
        connect(reply, SIGNAL(finished()), this, SLOT(onNetworkReplyFinished()));
    }
}
void xwdmes::onNetworkReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());

    if (reply && reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();

        // 解析 JSON 数据
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        // 获取返回的结果
        QString resultCode = jsonObj.value("resultCode").toString();

        if (resultCode == "0000")
        {
            // 登录成功
            qDebug() << "LogIn successful";
            emit sendMesState(1);
        }
        else
        {
            emit sendMesState(0);
            qDebug() << resultCode;
            // 登录失败，处理错误信息
            if (jsonObj.contains("resultMsg"))
            {
                QString resultMsg = jsonObj.value("resultMsg").toString();
                qDebug() << "LogIn failed. Error: " << resultMsg;
                emit operateMesError(0, "登陆失败：" + resultMsg);
            }
            else
            {
                emit operateMesError(0, "登陆失败，没有报错信息");
            }
            // 打印整个 JSON 对象的字符串表示形式
            qDebug() << "Full JSON response:" << jsonDoc.toJson(QJsonDocument::Indented);
        }

        reply->deleteLater();
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(0, "Network request error");
        qDebug() << "Network request error:" << reply->errorString();
    }
}

// const int mechines, const QString &sn, const QString &emp, const QString &machineNo

void xwdmes::ProcessInspection(MesPacketData pack)
{
    if (pack.factory == "xwd")
    {
        // 接口地址
        QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/groupTest";

        // 构建请求参数
        QJsonObject requestData;
        requestData["sn"] = pack.sn;
        requestData["emp"] = pack.Employee_ID;
        requestData["machineNo"] = pack.machineNo;

        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();

        QNetworkAccessManager manager;
        QNetworkRequest request((QUrl(url)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
        QNetworkReply *reply = manager.post(request, jsonData);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
            QJsonObject jsonObj = jsonDoc.object();

            QString resultCode = jsonObj.value("resultCode").toString();
            QString resultMsg = jsonObj.value("resultMsg").toString();

            if (resultCode == "0000")
            {
                // 工序检验成功
                qDebug() << "工序检验成功";
                emit operateMesSucess(pack.mechines);

                // 进行其他操作...
            }
            else
            {
                // 工序检验失败
                emit operateMesError(pack.mechines, "工序检验失败：" + resultMsg);
                // 进行其他操作...
            }
        }
        else
        {
            // 处理网络请求错误
            emit operateMesError(pack.mechines, "网络请求错误：" + reply->errorString());
        }

        reply->deleteLater();
    }
}

void xwdmes::collectPass(const int mechines, const QString &sn, const QString &mo,
                         const QString &userno, const QString &machineno, const QString &result,
                         const QString &itemvalue)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/weldInputTest";

    // 构建请求参数
    QJsonObject requestData;
    requestData["mSn"] = sn;
    requestData["mMo"] = mo;
    requestData["mUserno"] = userno;
    requestData["mMachineno"] = machineno;
    requestData["mResult"] = result;
    requestData["mItemvalue"] = itemvalue;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            // 过站成功
            qDebug() << "过站成功";
            emit operateMesSucess(mechines);
            // 进行其他操作...
        }
        else
        {
            emit operateMesError(mechines, "过站失败：" + resultMsg);
            // 进行其他操作...
        }
    }
    else
    {
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}

// const int mechines, const QString &sn, const QString &result,
//     const QString &userno, const QString &machineno, const QString &error,
//     const QString &itemvalue
void xwdmes::TestPass(MesPacketData pack)
{
    if (pack.factory == "xwd")
    {
        // 接口地址
        QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/wipTest";

        // 构建请求参数
        QJsonObject requestData;
        requestData["mSn"] = pack.sn;
        requestData["mResult"] = pack.result;
        requestData["mUserno"] = pack.Employee_ID;
        requestData["mMachineno"] = pack.machineNo;
        requestData["mError"] = pack.error;
        requestData["mItemvalue"] = pack.itemvalue;

        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();

        QNetworkAccessManager manager;
        QNetworkRequest request((QUrl(url)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
        QNetworkReply *reply = manager.post(request, jsonData);
        QEventLoop loop;
        QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
        loop.exec();

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
            QJsonObject jsonObj = jsonDoc.object();

            QString resultCode = jsonObj.value("resultCode").toString();
            QString resultMsg = jsonObj.value("resultMsg").toString();

            if (resultCode == "0000")
            {
                // 过站成功
                qDebug() << "过站成功";
                emit operateMesSucess(pack.mechines);
                // 进行其他操作...
            }
            else
            {
                // 过站失败
                qDebug() << "过站失败：" << resultMsg;
                emit operateMesError(pack.mechines, resultMsg);
                // 进行其他操作...
            }
        }
        else
        {
            emit operateMesError(pack.mechines, "网络请求错误：" + reply->errorString());
        }

        reply->deleteLater();
    }
}
void xwdmes::assemblePass(const int mechines, const QString &machineNo, const QString &productSN,
                          const QString &mo, const QString &emp, const QString &kpItemSnAll)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/hwInterface";

    // 构建请求参数
    QJsonObject requestData;
    requestData["machineNo"] = machineNo;
    requestData["productSN"] = productSN;
    requestData["mo"] = mo;
    requestData["emp"] = emp;
    requestData["kpItemSnAll"] = kpItemSnAll;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            // 过站成功
            qDebug() << "过站成功";
            emit operateMesSucess(mechines);
            // 进行其他操作...
        }
        else
        {
            // 过站失败
            qDebug() << "过站失败：" << resultMsg;
            emit operateMesError(mechines, resultMsg);
            // 进行其他操作...
        }
    }
    else
    {
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}
void xwdmes::uploadOfflineData(const int mechines, const QString &mSn, const QString &mResult,
                               const QString &mUserno, const QString &mMachineno,
                               const QString &mError, const QString &mItemvalue)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/offlineData";

    // 构建请求参数
    QJsonObject requestData;
    requestData["mSn"] = mSn;
    requestData["mResult"] = mResult;
    requestData["mUserno"] = mUserno;
    requestData["mMachineno"] = mMachineno;
    requestData["mError"] = mError;
    requestData["mItemvalue"] = mItemvalue;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            // 上传成功
            qDebug() << "上传成功";
            emit operateMesSucess(mechines);
            // 进行其他操作...
        }
        else
        {
            // 上传失败
            qDebug() << "上传失败：" << resultMsg;
            emit operateMesError(mechines, "上传失败：" + resultMsg);
            // 进行其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}
// (const int mechines, const QString &sn, const QString &wpCode,
//  const QString &testItemName)

void xwdmes::GetTestData(MesPacketData pack)
{
    if (pack.factory == "xwd")
    {
        if (pack.product == "Q20")
        {
            pack.machineNo = "Q20-JTDLTEST";
            pack.itemvalue = "BTMAC";
        }
        if (pack.product == "Y20")
        {
            pack.machineNo = "Y20_CURRENT1_TEST";
            pack.itemvalue = "BTMAC";
        }

        // 接口地址
        QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/getTestData";

        // 构建请求参数
        QJsonObject requestData;
        requestData["sn"] = pack.sn;
        requestData["wpCode"] = pack.machineNo;
        requestData["testItemName"] = pack.itemvalue;
        QJsonDocument jsonDoc(requestData);
        QByteArray jsonData = jsonDoc.toJson();

        QNetworkAccessManager manager;
        QNetworkRequest request((QUrl(url)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // 发送POST请求
        QNetworkReply *reply = manager.post(request, jsonData);
        QEventLoop loop;
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();

        if (reply->error() == QNetworkReply::NoError)
        {
            QByteArray responseData = reply->readAll();
            QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
            QJsonObject jsonObj = jsonDoc.object();

            QString resultCode = jsonObj.value("resultCode").toString();
            QString resultMsg = jsonObj.value("resultMsg").toString();

            if (resultCode == "0000")
            {
                QJsonArray dataArray = jsonObj.value("data").toArray();
                foreach(const QJsonValue &value, dataArray)
                {
                    QJsonObject itemObj = value.toObject();
                    QString moCode = itemObj.value("moCode").toString();
                    QString mtrlCode = itemObj.value("mtrlCode").toString();
                    QString wpCode = itemObj.value("wpCode").toString();
                    QString devCode = itemObj.value("devCode").toString();
                    QString sn = itemObj.value("sn").toString();
                    QString testItemName = itemObj.value("testItemName").toString();
                    QString testItemValue = itemObj.value("testItemValue").toString();
                    QString testDate = itemObj.value("testDate").toString();

                    qDebug() << "moCode:" << moCode;
                    qDebug() << "mtrlCode:" << mtrlCode;
                    qDebug() << "wpCode:" << wpCode;
                    qDebug() << "devCode:" << devCode;
                    qDebug() << "sn:" << sn;
                    qDebug() << "testItemName:" << testItemName;
                    qDebug() << "testItemValue:" << testItemValue;
                    // emit operateMesSucess(pack.mechines);
                    emit sendMesTestvalue(pack.mechines, testItemValue);
                    qDebug() << "testDate:" << testDate;
                    break;

                    // 根据需求处理获取到的测试数据
                    // 可以将数据存储到数据结构中，或进行其他操作
                }
                qDebug() << "mes内容:" << resultMsg;
                // 其他操作...
            }
            else
            {
                emit operateMesError(pack.mechines, "请求失败：" + resultMsg);
                // 其他操作...
            }
        }
        else
        {
            // 处理网络请求错误
            emit operateMesError(pack.mechines, "网络请求错误：" + reply->errorString());
        }

        reply->deleteLater();
    }
}

void xwdmes::getSoftwareVersionBySn(const int mechines, const QString &sn)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/getSoftwareVersionBySn";

    // 构建请求参数
    QJsonObject requestData;
    requestData["sn"] = sn;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            QJsonObject dataObj = jsonObj.value("data").toObject();
            QStringList testItems = dataObj.keys();
            foreach(const QString &testItem, testItems)
            {
                QString testValue = dataObj.value(testItem).toString();

                qDebug() << "testValue: " << testValue;
                emit operateMesSucess(mechines);
                // 根据需求处理获取到的测试项和测试值
                // 可以将数据存储到数据结构中，或进行其他操作
            }

            // 其他操作...
        }
        else
        {
            emit operateMesError(mechines, "请求失败：" + resultMsg);
            // 其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}

void xwdmes::getKeyToProduct(const int mechines, const QString &productKey, const QString &itemCode)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/getKeyToProduct";

    // 构建请求参数
    QJsonObject requestData;
    requestData["productKey"] = productKey;
    requestData["itemCode"] = itemCode;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            QString data = jsonObj.value("data").toString();

            qDebug() << "data: " << data;
            emit operateMesSucess(mechines);
            // 根据需求处理获取到的主件条码
            // 可以将数据存储到数据结构中，或进行其他操作

            // 其他操作...
        }
        else
        {
            emit operateMesError(mechines, "请求失败：" + resultMsg);
            // 其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}
void xwdmes::getBindTable(const int mechines, const QString &productSN)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/getBindTable";

    // 构建请求参数
    QJsonObject requestData;
    requestData["productSN"] = productSN;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            emit operateMesSucess(mechines);
            QJsonArray dataArray = jsonObj.value("data").toArray();
            foreach(const QJsonValue &dataValue, dataArray)
            {
                QJsonObject dataObj = dataValue.toObject();
                QString productSN = dataObj.value("productSN").toString();
                QString itemCode = dataObj.value("itemCode").toString();
                QString testValue = dataObj.value("testValue").toString();

                qDebug() << "productSN:" << productSN;
                qDebug() << "itemCode:" << itemCode;
                qDebug() << "testValue:" << testValue;
                // 根据需求处理获取到的子件明细
                // 可以将数据存储到数据结构中，或进行其他操作
            }

            // 其他操作...
        }
        else
        {
            emit operateMesError(mechines, "请求失败：" + resultMsg);
            // 其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}

void xwdmes::replaceSN(const int mechines, const QString &mo, const QString &itemSN)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/replaceSN";

    // 构建请求参数
    QJsonObject requestData;
    requestData["mo"] = mo;
    requestData["itemSN"] = itemSN;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            emit operateMesSucess(mechines);
            QString data = jsonObj.value("data").toString();

            qDebug() << "data: " << data;
            // 根据需求处理获取到的产品条码
            // 可以将数据存储到数据结构中，或进行其他操作

            // 其他操作...
        }
        else
        {
            emit operateMesError(mechines, "请求失败：" + resultMsg);
            // 其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}
void xwdmes::getMaterialAndMoBySN(const int mechines, const QString &SN)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/getMaterialAndMoBySN";

    // 构建请求参数
    QJsonObject requestData;
    requestData["SN"] = SN;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            emit operateMesSucess(mechines);
            QJsonObject dataObj = jsonObj.value("data").toObject();
            QString itemCode = dataObj.value("itemCode").toString();
            QString mo = dataObj.value("mo").toString();

            qDebug() << "itemCode: " << itemCode;
            qDebug() << "mo: " << mo;
            // 根据需求处理获取到的料号和制令单
            // 可以将数据存储到数据结构中，或进行其他操作

            // 其他操作...
        }
        else
        {
            emit operateMesError(mechines, "请求失败：" + resultMsg);
            // 其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}
void xwdmes::getGroupItem(const int mechines, const QString &mo, const QString &machineNo)
{
    // 接口地址
    QString url = "https://hzznyjmes.sunwoda.com/ims-pms/api/device/getGroupItem";

    // 构建请求参数
    QJsonObject requestData;
    requestData["mo"] = mo;
    requestData["machineNo"] = machineNo;

    QJsonDocument jsonDoc(requestData);
    QByteArray jsonData = jsonDoc.toJson();

    QNetworkAccessManager manager;
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 发送POST请求
    QNetworkReply *reply = manager.post(request, jsonData);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray responseData = reply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
        QJsonObject jsonObj = jsonDoc.object();

        QString resultCode = jsonObj.value("resultCode").toString();
        QString resultMsg = jsonObj.value("resultMsg").toString();

        if (resultCode == "0000")
        {
            emit operateMesSucess(mechines);
            QJsonObject dataObj = jsonObj.value("data").toObject();
            QString itemCode = dataObj.value("itemCode").toString();
            QString itemName = dataObj.value("itemName").toString();
            int controlDepth = dataObj.value("controlDepth").toInt();
            double dosage = dataObj.value("dosage").toDouble();

            qDebug() << "itemCode: " << itemCode;
            qDebug() << "itemName: " << itemName;
            qDebug() << "controlDepth: " << controlDepth;
            qDebug() << "dosage: " << dosage;
            // 根据需求处理获取到的子件料号
            // 可以将数据存储到数据结构中，或进行其他操作

            // 其他操作...
        }
        else
        {
            emit operateMesError(mechines, "请求失败：" + resultMsg);
            // 其他操作...
        }
    }
    else
    {
        // 处理网络请求错误
        emit operateMesError(mechines, "网络请求错误：" + reply->errorString());
    }

    reply->deleteLater();
}
