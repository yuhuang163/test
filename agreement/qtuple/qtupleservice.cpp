#include "qtupleservice.h"

#include <QDateTime>
#include <QEventLoop>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QDebug>
#include <QSslError>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <QSysInfo>

#include "my_set/my_typedef.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

/** 上报检验项中的 deviceSecret 脱敏：保留前面内容，末尾 3 个字符用 '*' 代替（过短则全部打码）。 */
QString maskDeviceSecretTail3(const QString& secret) {
    const int n = secret.size();
    if (n <= 0) {
        return QString();
    }
    if (n <= 3) {
        return QString(n, QLatin1Char('*'));
    }
    return secret.left(n - 3) + QLatin1String("***");
}

QStringList tupleCurlHeaderArgs(const QByteArray& authHeader) {
    QStringList args;
    auto addHeader = [&](const QString& name, const QString& value) {
        args << "-H" << QString("%1: %2").arg(name, value);
    };

    addHeader("Content-Type", "application/json");
    addHeader("Device-Id", QSysInfo::machineHostName());
    addHeader("APP-Version", "factory-tool");
    if (!authHeader.isEmpty()) {
        addHeader("Authorization", QString::fromUtf8(authHeader));
    }
    return args;
}

bool runTupleCurl(const QStringList& args, QByteArray* response, QString* error, int* httpStatus) {
    QProcess curl;
    curl.start(QStringLiteral("curl.exe"), args);
    if (!curl.waitForStarted(3000)) {
        if (error) {
            *error = QStringLiteral("curl.exe 启动失败：") + curl.errorString();
        }
        return false;
    }
    if (!curl.waitForFinished(30000)) {
        curl.kill();
        curl.waitForFinished(1000);
        if (error) {
            *error = QStringLiteral("curl.exe 请求超时");
        }
        return false;
    }

    QByteArray output = curl.readAllStandardOutput();
    const QByteArray stderrText = curl.readAllStandardError();
    const QByteArray marker = "\n__HTTP_STATUS__:";
    int status = 0;
    const int markerIndex = output.lastIndexOf(marker);
    if (markerIndex >= 0) {
        status = output.mid(markerIndex + marker.size()).trimmed().toInt();
        output = output.left(markerIndex);
    }
    if (httpStatus) {
        *httpStatus = status;
    }
    if (response) {
        *response = output;
    }

    const bool ok = curl.exitStatus() == QProcess::NormalExit &&
                    curl.exitCode() == 0 &&
                    status >= 200 &&
                    status < 300;
    if (!ok && error) {
        *error = QString("curl exit=%1 httpStatus=%2 %3 %4")
                     .arg(curl.exitCode())
                     .arg(status)
                     .arg(QString::fromUtf8(stderrText).trimmed())
                     .arg(QString::fromUtf8(output).trimmed())
                     .trimmed();
    }
    return ok;
}

bool requestGetByCurl(const QUrl& url, const QByteArray& authHeader, QByteArray* response, QString* error, int* httpStatus) {
    QStringList curlArgs;
    curlArgs << "-sS" << "--connect-timeout" << "10" << "--max-time" << "30"
             << "-X" << "GET";
    curlArgs << tupleCurlHeaderArgs(authHeader);
    curlArgs << url.toString(QUrl::FullyEncoded)
             << "-w" << "\n__HTTP_STATUS__:%{http_code}";

    return runTupleCurl(curlArgs, response, error, httpStatus);
}

bool requestPostByCurl(const QUrl& url, const QByteArray& authHeader, const QByteArray& body, QByteArray* response, QString* error, int* httpStatus) {
    QStringList curlArgs;
    curlArgs << "-sS" << "--connect-timeout" << "10" << "--max-time" << "30"
             << "-X" << "POST";
    curlArgs << tupleCurlHeaderArgs(authHeader);
    curlArgs << "--data-binary" << QString::fromUtf8(body)
             << url.toString(QUrl::FullyEncoded)
             << "-w" << "\n__HTTP_STATUS__:%{http_code}";

    return runTupleCurl(curlArgs, response, error, httpStatus);
}

}  // namespace

static const QHash<QString, QString>& tupleInspectionOpLabels() {
    // 源文件为 UTF-8 时，用 QString::fromUtf8 显式按 UTF-8 解码字面量（与工程内其它中文用法一致）
    static const QHash<QString, QString> kTable = {
        {QString::fromUtf8("Z_BLE_WAITE_BOX"), QString::fromUtf8("连接蓝牙测试盒")},
        {QString::fromUtf8("Z_WIFI_WAITE_BOX"), QString::fromUtf8("连接WiFi测试盒")},
        {QString::fromUtf8("AT_MODE"), QString::fromUtf8("修改串口模式")},
        {QString::fromUtf8("AT_MULTI"), QString::fromUtf8("修改串口连接模式")},
        {QString::fromUtf8("AT_REBOOT"), QString::fromUtf8("重启串口")},
        {QString::fromUtf8("AT_UUID"), QString::fromUtf8("订阅UUID")},
        {QString::fromUtf8("AT_DISC"), QString::fromUtf8("断开连接")},
        {QString::fromUtf8("AT_SCAN"), QString::fromUtf8("发现设备中...")},
        {QString::fromUtf8("AT_CONN"), QString::fromUtf8("建立连接")},
        {QString::fromUtf8("AT_GAT_STATE"), QString::fromUtf8("查询连接状态")},
        {QString::fromUtf8("AT_GATT_SEND"), QString::fromUtf8("发送数据")},
        {QString::fromUtf8("Q_UPDATE_MAC_STATUS"), QString::fromUtf8("更新烧录状态")},
        {QString::fromUtf8("C_POWER_OFF"), QString::fromUtf8("控制关机")},
        {QString::fromUtf8("R_SN"), QString::fromUtf8("读SN")},
        {QString::fromUtf8("R_BATTERY_LEVEL"), QString::fromUtf8("读电量")},
        {QString::fromUtf8("R_MODE_SIDE"), QString::fromUtf8("读左右标记")},
        {QString::fromUtf8("R_MAC"), QString::fromUtf8("读MAC")},
        {QString::fromUtf8("R_CODE"), QString::fromUtf8("读CODE")},
        {QString::fromUtf8("R_VAR"), QString::fromUtf8("读版本信息")},
        {QString::fromUtf8("R_MC11_F_MILK_CAPACITY"), QString::fromUtf8("读奶量检测数据")},
        {QString::fromUtf8("W_MC11_N_MILK_CAPACITY_STABLE"), QString::fromUtf8("写空杯检测数据")},
        {QString::fromUtf8("W_MC11_F_MILK_CAPACITY_STABLE"), QString::fromUtf8("写满杯检测数据")},
        {QString::fromUtf8("R_MC11_F_MILK_CAPACITY_STABLE"), QString::fromUtf8("读溢奶检测标定值")},
        {QString::fromUtf8("R_D_TUPLE"), QString::fromUtf8("读三元组")},
        {QString::fromUtf8("R_JL_F_MAC"), QString::fromUtf8("检测蓝牙地址")},
        {QString::fromUtf8("R_JL_F_BLE_FREQ"), QString::fromUtf8("检测频偏")},
        {QString::fromUtf8("R_JL_F_BLE_RSSI"), QString::fromUtf8("检测信号")},
        {QString::fromUtf8("R_LX_FB_RX_RSSI"), QString::fromUtf8("信号板信号强度")},
        {QString::fromUtf8("R_LX_DUT_RX_RSSI"), QString::fromUtf8("待测品信号强度")},
        {QString::fromUtf8("R_LX_TXP_RESULT"), QString::fromUtf8("收发包总数")},
        {QString::fromUtf8("R_LX_FREQ_OFFSET"), QString::fromUtf8("频偏")},
        {QString::fromUtf8("R_LX_RSSI_DIFF"), QString::fromUtf8("信号强度差")},
        {QString::fromUtf8("R_ESP_GPIO_TEST_L"), QString::fromUtf8("GPIO连通性0")},
        {QString::fromUtf8("R_ESP_GPIO_TEST_H"), QString::fromUtf8("GPIO连通性1")},
        {QString::fromUtf8("W_SN"), QString::fromUtf8("写入SN")},
        {QString::fromUtf8("W_MODE_SIDE"), QString::fromUtf8("写入左右标记")},
        {QString::fromUtf8("W_MAC"), QString::fromUtf8("写入MAC")},
        {QString::fromUtf8("W_CODE"), QString::fromUtf8("写入CODE")},
        {QString::fromUtf8("W_P_KEY"), QString::fromUtf8("写入ProductKey")},
        {QString::fromUtf8("W_D_NAME"), QString::fromUtf8("写入DeviceName")},
        {QString::fromUtf8("W_D_SECRET"), QString::fromUtf8("写入DeviceSecret")},
        {QString::fromUtf8("W_SE_OPEN"), QString::fromUtf8("打开屏蔽箱")},
        {QString::fromUtf8("W_SE_LIGHT_ON"), QString::fromUtf8("打开信号灯")},
        {QString::fromUtf8("R_MO_MAC"), QString::fromUtf8("读MAC")},
        {QString::fromUtf8("R_MO_SN"), QString::fromUtf8("读SN")},
        {QString::fromUtf8("R_MO_TUPLE"), QString::fromUtf8("读三元组")},
        {QString::fromUtf8("R_MO_PROD_TYPE"), QString::fromUtf8("读产品类型")},
        {QString::fromUtf8("R_MO_BROADCAST_NAME"), QString::fromUtf8("读广播名称")},
        {QString::fromUtf8("R_MO_VAR"), QString::fromUtf8("读版本信息")},
        {QString::fromUtf8("W_MO_TUPLE"), QString::fromUtf8("写入三元组")},
        {QString::fromUtf8("W_MO_SN"), QString::fromUtf8("写入SN")},
        {QString::fromUtf8("W_MO_PROD_TYPE"), QString::fromUtf8("写入产品类型")},
        {QString::fromUtf8("W_MO_VALID_CODE"), QString::fromUtf8("写入数据校验")},
        {QString::fromUtf8("W_MO_BROADCAST_NAME"), QString::fromUtf8("写入广播名称")},
        {QString::fromUtf8("C_MO_WIFI_SSID_PASS"), QString::fromUtf8("连接路由器")},
        {QString::fromUtf8("X_LOAD_RAM"), QString::fromUtf8("加载测试RAM")},
        {QString::fromUtf8("X_PING_LAN"), QString::fromUtf8("域网Ping丢包率")},
        // 上位机 reportWriteRecord 使用，与前端 OP_KEY_NAME 并列维护
        {QString::fromUtf8("R_BT_RSSI"), QString::fromUtf8("蓝牙链路RSSI")},
    };
    return kTable;
}

/** 三元组写入记录上报：业务侧只写与 tupleInspectionOpLabels 一致的中文名，再映射为线上 R_* 操作码。 */
static QString tupleReportWireKeyFromDisplayZh(const QString& displayZh) {
    static const QHash<QString, QString> kZhToWire = {
        {QString::fromUtf8("读三元组"), QString::fromUtf8("R_D_TUPLE")},
        {QString::fromUtf8("蓝牙链路RSSI"), QString::fromUtf8("R_BT_RSSI")},
        {QString::fromUtf8("检测信号"), QString::fromUtf8("R_JL_F_BLE_RSSI")},
        {QString::fromUtf8("读版本信息"), QString::fromUtf8("R_MO_VAR")},
    };
    return kZhToWire.value(displayZh.trimmed());
}

static QString formatTupleReportInspectionItem(const QString& displayZh, const QString& payload, bool itemPass, qint64 ts) {
    const QString wire = tupleReportWireKeyFromDisplayZh(displayZh);
    if (wire.isEmpty()) {
        qWarning() << "[Tuple] reportWriteRecord unknown inspection label:" << displayZh;
        return QString();
    }
    return QString("%1:%2:%3:%4")
        .arg(wire)
        .arg(payload)
        .arg(itemPass ? QString::fromUtf8("true") : QString::fromUtf8("false"))
        .arg(ts);
}

QString QTupleService::inspectionOpDisplayName(const QString& opKey) {
    const QString k = opKey.trimmed();
    const auto& t = tupleInspectionOpLabels();
    const auto it = t.constFind(k);
    return it == t.cend() ? k : it.value();
}

QString QTupleService::inspectionOpDisplayNameFromItem(const QString& itemOrKey) {
    const int cut = itemOrKey.indexOf(QLatin1Char(':'));
    const QString key = cut < 0 ? itemOrKey.trimmed() : itemOrKey.left(cut).trimmed();
    return inspectionOpDisplayName(key);
}

QTupleService::QTupleService(const QString& baseUrl)
    : baseUrl_(baseUrl.isEmpty() ? SETTINGS.value("Tuple/BaseUrl", "http://192.168.200.140:8080").toString() : baseUrl) {}

bool QTupleService::login(const QString& userName, const QString& password, QString* error) {
    const QByteArray token = QString("%1:%2").arg(userName, password).toUtf8().toBase64();
    authHeader_ = "Basic " + token;

    const QString path = QStringLiteral("/api/mac-addresses/auth");
    QUrl url(normalizedBaseUrl() + path);
    const QString authLog = authHeader_.startsWith("Basic ")
        ? QStringLiteral("Basic <redacted %1 bytes>").arg(authHeader_.size() - 6)
        : QStringLiteral("<none>");
    qDebug().noquote() << "[Tuple] login request:"
                       << "method=GET"
                       << "url=" + url.toString(QUrl::FullyEncoded)
                       << "userName=" + userName
                       << "Content-Type=application/json"
                       << "Device-Id=" + QSysInfo::machineHostName()
                       << "APP-Version=factory-tool"
                       << "Authorization=" + authLog;

    QByteArray response;
    int httpStatus = 0;
    if (!requestGet(path, QString(), &response, error, &httpStatus)) {
        qDebug().noquote() << "[Tuple] login failed httpStatus=" << httpStatus
                           << "responseBody=" << QString::fromUtf8(response)
                           << "error=" << (error ? *error : QString());
        authHeader_.clear();
        return false;
    }
    qDebug().noquote() << "[Tuple] login ok httpStatus=" << httpStatus
                       << "responseBody=" << QString::fromUtf8(response);
    return true;
}

TupleApplyResult QTupleService::applyTupleByMac(const QString& mac, const QString& sku, const QString& position) {
    TupleApplyResult result;
    qDebug().noquote() << "[Tuple] applyTupleByMac params:"
                       << "mac=" + mac
                       << "sku=" + sku
                       << "position=" + position;
    QUrlQuery query;
    query.addQueryItem("mac", mac);
    query.addQueryItem("sku", sku);
    query.addQueryItem("position", position);

    QByteArray response;
    if (!requestGet("/api/mac-addresses/applyTupleByMac", query.toString(QUrl::FullyEncoded), &response, &result.error)) {
        return result;
    }

    qDebug().noquote() << "[Tuple] applyTupleByMac response:" << QString::fromUtf8(response);
    const QJsonDocument doc = QJsonDocument::fromJson(response);
    if (!doc.isObject()) {
        result.error = "三元组响应不是 JSON 对象";
        return result;
    }

    const QJsonObject obj = doc.object();
    const QJsonObject dataObj = obj.value("data").isObject() ? obj.value("data").toObject() : obj;
    const int successCode = dataObj.contains("success") ? dataObj.value("success").toInt(-1) : obj.value("success").toInt(0);
    const int code = obj.value("code").toInt(200);
    if (code != 200 || successCode != 0) {
        result.error = obj.value("message").toString(obj.value("msg").toString("三元组接口返回失败"));
        return result;
    }

    result.mac = dataObj.value("mac").toString(mac);
    result.productKey = dataObj.value("productKey").toString();
    result.deviceName = dataObj.value("deviceName").toString();
    result.deviceSecret = dataObj.value("deviceSecret").toString();
    result.sn = dataObj.value("sn").toString();
    result.status = dataObj.value("status").toInt();
    result.availableCount = dataObj.value("availableCount").toInt();
    result.success = !result.productKey.isEmpty() && !result.deviceName.isEmpty() && !result.deviceSecret.isEmpty();
    if (!result.success) {
        result.error = "三元组字段缺失";
    }
    return result;
}

bool QTupleService::debugUpdateMacStatus(const QString& mac, int status, QString* error) {
    QJsonObject bodyObj;
    bodyObj.insert("mac", mac);
    bodyObj.insert("status", status);
    const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    QByteArray response;
    if (!requestPost("/api/mac-addresses", body, &response, error)) {
        return false;
    }

    qDebug().noquote() << "[Tuple] debugUpdateMacStatus response:" << QString::fromUtf8(response);
    return true;
}

bool QTupleService::reportWriteRecord(const TupleApplyResult& tuple, const QString& productSn, const QString& result,
                                      const QString& btRssi, bool btRssiPass,
                                      const QString& bleRssi, bool bleRssiPass,
                                      const QString& softwareVersion, bool softwareVersionPass, QString* error) {
    const bool pass = result == "OK" || result == "通过" || result == "true";
    const qint64 timestamp = QDateTime::currentMSecsSinceEpoch();
    const QString reportTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    const QString sn =  productSn.trimmed();
    // 载荷：productKey + deviceName + 脱敏后的 deviceSecret（与 R_MO_TUPLE 拼接习惯一致，避免明文密钥）
    const QString tupleValue = tuple.productKey + tuple.deviceName + maskDeviceSecretTail3(tuple.deviceSecret);
    const QString zhTuple = QString::fromUtf8("读三元组");
    const QString inspectionItem = formatTupleReportInspectionItem(zhTuple, tupleValue, pass, timestamp);

    QJsonArray inspectionItems;
    if (!inspectionItem.isEmpty()) {
        inspectionItems.append(inspectionItem);
    }
    if (!btRssi.trimmed().isEmpty()) {
        const QString row = formatTupleReportInspectionItem(QString::fromUtf8("蓝牙链路RSSI"), btRssi.trimmed(), btRssiPass, timestamp);
        if (!row.isEmpty()) {
            inspectionItems.append(row);
        }
    }
    if (!bleRssi.trimmed().isEmpty()) {
        const QString row = formatTupleReportInspectionItem(QString::fromUtf8("检测信号"), bleRssi.trimmed(), bleRssiPass, timestamp);
        if (!row.isEmpty()) {
            inspectionItems.append(row);
        }
    }
    if (!softwareVersion.trimmed().isEmpty()) {
        const QString row = formatTupleReportInspectionItem(QString::fromUtf8("读版本信息"), softwareVersion.trimmed(), softwareVersionPass, timestamp);
        if (!row.isEmpty()) {
            inspectionItems.append(row);
        }
    }

    QJsonObject bodyObj;
    bodyObj.insert("sn", sn);
    bodyObj.insert("reportTime", reportTime);
    bodyObj.insert("inspectionItems", inspectionItems);
    const QByteArray body = QJsonDocument(bodyObj).toJson(QJsonDocument::Compact);

    // qDebug().noquote() << "[Tuple] reportWriteRecord body:" << QString::fromUtf8(body);
    QByteArray response;
    if (!requestPost("/api/inspection/report", body, &response, error)) {
        return false;
    }

    qDebug().noquote() << "[Tuple] reportWriteRecord response:" << QString::fromUtf8(response);
    return true;
}

QString QTupleService::normalizedBaseUrl() const {
    QString url = baseUrl_.trimmed();
    while (url.endsWith('/')) {
        url.chop(1);
    }
    return url;
}

bool QTupleService::requestGet(const QString& path, const QString& query, QByteArray* response, QString* error, int* httpStatus) {
    QNetworkAccessManager manager;
    QUrl url(normalizedBaseUrl() + path);
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    if (SETTINGS.value("Tuple/UseCurl", true).toBool()) {
        const bool curlOk = requestGetByCurl(url, authHeader_, response, error, httpStatus);
        qDebug().noquote() << (curlOk ? "[Tuple] GET curl ok httpStatus=" : "[Tuple] GET curl failed httpStatus=")
                           << (httpStatus ? *httpStatus : 0)
                           << "responseBody=" << (response ? QString::fromUtf8(*response) : QString())
                           << "error=" << (error ? *error : QString());
        return curlOk;
    }
    QNetworkRequest request(url);
    setCommonHeaders(request);
    qDebug().noquote() << "[Tuple] GET api:" << url.toString(QUrl::FullyEncoded);

    QNetworkReply* reply = manager.get(request);
    QList<QSslError> sslErrors;
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, QOverload<const QList<QSslError>&>::of(&QNetworkReply::sslErrors),
                     [&](const QList<QSslError>& errors) {
                         sslErrors = errors;
                     });
    loop.exec();

    const QByteArray body = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (httpStatus) {
        *httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    }
    if (!ok) {
        QStringList sslErrorTexts;
        for (const QSslError& sslError : sslErrors) {
            sslErrorTexts.append(sslError.errorString());
        }
        qDebug().noquote() << "[Tuple] GET failed detail:"
                           << "qtError=" << static_cast<int>(reply->error())
                           << "errorString=" << reply->errorString()
                           << "httpStatus=" << reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt()
                           << "sslErrors=" << sslErrorTexts.join("; ");
        if (reply->error() == QNetworkReply::UnknownNetworkError &&
            reply->errorString().contains(QStringLiteral("Permission denied"), Qt::CaseInsensitive)) {
            QByteArray curlBody;
            QString curlError;
            int curlStatus = 0;
            if (requestGetByCurl(url, authHeader_, &curlBody, &curlError, &curlStatus)) {
                if (httpStatus) {
                    *httpStatus = curlStatus;
                }
                if (response) {
                    *response = curlBody;
                }
                qDebug().noquote() << "[Tuple] GET curl fallback ok httpStatus=" << curlStatus
                                   << "responseBody=" << QString::fromUtf8(curlBody);
                reply->deleteLater();
                return true;
            }
            qDebug().noquote() << "[Tuple] GET curl fallback failed httpStatus=" << curlStatus
                               << "error=" << curlError
                               << "responseBody=" << QString::fromUtf8(curlBody);
        }
    }
    if (response) {
        *response = body;
    }
    if (!ok && error) {
        *error = reply->errorString() + " " + QString::fromUtf8(body);
    }
    reply->deleteLater();
    return ok;
}

bool QTupleService::requestPost(const QString& path, const QByteArray& body, QByteArray* response, QString* error) {
    QNetworkAccessManager manager;
    const QUrl url(normalizedBaseUrl() + path);
    if (SETTINGS.value("Tuple/UseCurl", true).toBool()) {
        int curlStatus = 0;
        const bool curlOk = requestPostByCurl(url, authHeader_, body, response, error, &curlStatus);
        qDebug().noquote() << (curlOk ? "[Tuple] POST curl ok httpStatus=" : "[Tuple] POST curl failed httpStatus=")
                           << curlStatus
                           << "responseBody=" << (response ? QString::fromUtf8(*response) : QString())
                           << "error=" << (error ? *error : QString());
        return curlOk;
    }
    QNetworkRequest request(url);
    setCommonHeaders(request);
    qDebug().noquote() << "[Tuple] POST api:" << url.toString(QUrl::FullyEncoded)
                       << "body:" << QString::fromUtf8(body);

    QNetworkReply* reply = manager.post(request, body);
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray responseBody = reply->readAll();
    const bool ok = reply->error() == QNetworkReply::NoError;
    if (!ok &&
        reply->error() == QNetworkReply::UnknownNetworkError &&
        reply->errorString().contains(QStringLiteral("Permission denied"), Qt::CaseInsensitive)) {
        QByteArray curlBody;
        QString curlError;
        int curlStatus = 0;
        if (requestPostByCurl(url, authHeader_, body, &curlBody, &curlError, &curlStatus)) {
            if (response) {
                *response = curlBody;
            }
            qDebug().noquote() << "[Tuple] POST curl fallback ok httpStatus=" << curlStatus
                               << "responseBody=" << QString::fromUtf8(curlBody);
            reply->deleteLater();
            return true;
        }
        qDebug().noquote() << "[Tuple] POST curl fallback failed httpStatus=" << curlStatus
                           << "error=" << curlError
                           << "responseBody=" << QString::fromUtf8(curlBody);
    }
    if (response) {
        *response = responseBody;
    }
    if (!ok && error) {
        *error = reply->errorString() + " " + QString::fromUtf8(responseBody);
    }
    reply->deleteLater();
    return ok;
}

void QTupleService::setCommonHeaders(QNetworkRequest& request) const {
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Device-Id", QSysInfo::machineHostName().toUtf8());
    request.setRawHeader("APP-Version", "factory-tool");
    if (!authHeader_.isEmpty()) {
        request.setRawHeader("Authorization", authHeader_);
    }
}
