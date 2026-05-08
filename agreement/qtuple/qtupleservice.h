#ifndef QTUPLESERVICE_H
#define QTUPLESERVICE_H

#include <QByteArray>
#include <QString>

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

class QTupleService {
public:
    explicit QTupleService(const QString& baseUrl = QString());

    bool login(const QString& userName, const QString& password, QString* error = nullptr);
    TupleApplyResult applyTupleByMac(const QString& mac, const QString& sku, const QString& position);
    bool debugUpdateMacStatus(const QString& mac, int status, QString* error = nullptr);
    bool reportWriteRecord(const TupleApplyResult& tuple, const QString& productSn, const QString& result,
                           const QString& btRssi = QString(), bool btRssiPass = true,
                           const QString& bleRssi = QString(), bool bleRssiPass = true,
                           const QString& softwareVersion = QString(), bool softwareVersionPass = true,
                           QString* error = nullptr);

private:
    QString normalizedBaseUrl() const;
    bool requestGet(const QString& path, const QString& query, QByteArray* response, QString* error);
    bool requestPost(const QString& path, const QByteArray& body, QByteArray* response, QString* error);
    void setCommonHeaders(class QNetworkRequest& request) const;

    QString baseUrl_;
    QByteArray authHeader_;
};

#endif  // QTUPLESERVICE_H
