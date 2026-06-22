#ifndef QTUPLESERVICE_H
#define QTUPLESERVICE_H

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantMap>

struct TupleApplyResult {
    bool success = false;
    QString error;
    QString mac;
    QString productKey;
    QString deviceName;
    QString deviceSecret;
    QString sn;
    int status = 0;
    int availableCount = 0;
};

/** 云端三元组 REST 操作（经 service->set/get 下发） */
enum class TupleCmd {
    Login,                // set: userName, password
    ApplyTupleByMac,      // get: mac, sku, position → lastApplyResult()
    DebugUpdateMacStatus, // set: mac, status
    ReportWriteRecord,    // set: tuple/productSn/result 及可选 RSSI、版本字段
};

class QTupleService {
  public:
    explicit QTupleService(const QString& baseUrl = QString());

    void parseCmd(const QByteArray& byte);
    void set(TupleCmd cmd, const QVariant& data = {});
    void get(TupleCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);

    TupleApplyResult lastApplyResult() const {
        return lastApplyResult_;
    }
    QString lastError() const {
        return lastError_;
    }

    static QString tupleCmdToName(TupleCmd cmd);
    static bool tupleCmdFromName(const QString& name, TupleCmd& out);

    /** 登录成功后写入，供同一次测试流程内后续 QTupleService 实例复用。 */
    static void clearSharedSession();
    static bool hasSharedSession();

    bool hasAuth() const {
        return !authHeader_.isEmpty();
    }

    /** 检验项操作码（如 R_D_TUPLE、R_VAR）→ 人可读中文名；未知则返回 \a opKey。 */
    static QString inspectionOpDisplayName(const QString& opKey);
    /** \a itemOrKey 为完整检验项字符串时取首段（':' 前）为操作码再查表；否则等同 \ref inspectionOpDisplayName。 */
    static QString inspectionOpDisplayNameFromItem(const QString& itemOrKey);

  private:
    bool loginImpl(const QString& userName, const QString& password, QString* error);
    TupleApplyResult applyTupleByMacImpl(const QString& mac, const QString& sku, const QString& position);
    bool debugUpdateMacStatusImpl(const QString& mac, int status, const QString& sn, QString* error);
    bool reportWriteRecordImpl(const TupleApplyResult& tuple, const QString& productSn, const QString& result,
                               const QString& btRssi, bool btRssiPass, const QString& bleRssi, bool bleRssiPass,
                               const QString& softwareVersion, bool softwareVersionPass, QString* error);
    TupleApplyResult parseApplyTupleResponse(const QByteArray& response) const;

    QString normalizedBaseUrl() const;
    bool requestGet(const QString& path, const QString& query, QByteArray* response, QString* error, int* httpStatus = nullptr);
    bool requestPost(const QString& path, const QByteArray& body, QByteArray* response, QString* error);
    void setCommonHeaders(class QNetworkRequest& request) const;

    QString baseUrl_;
    QByteArray authHeader_;
    TupleApplyResult lastApplyResult_;
    QString lastError_;
};

#endif // QTUPLESERVICE_H
