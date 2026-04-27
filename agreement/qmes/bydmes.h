#ifndef BYDMES_H
#define BYDMES_H

#include <QJsonObject>

#include "my_set/my_typedef.h"
#include "qmes.h"

class bydmes : public Qmes {
    Q_OBJECT
public:
    bydmes();

    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

private:
    QString baseUrl() const;
    QString settingsValue(const QString& key, const QString& fallback = QString()) const;
    QString buildRemark(const MesPacketData& pack) const;
    QJsonObject buildBydStartParam(const MesPacketData& pack) const;
    QJsonObject buildBydGetCustomDataParam() const;
    QJsonObject buildBydTestDataCollectParam(const MesPacketData& pack) const;
    QJsonObject buildBydCompleteParam(const MesPacketData& pack) const;
    QJsonObject buildBydNcCompleteParam(const MesPacketData& pack) const;
    QJsonArray buildBydTestDataList(const MesPacketData& pack, const QString& testTime) const;
    QString normalizeTestResult(const QString& result) const;
    bool isSuccessResponse(const QByteArray& responseData, QString* responseText, QString* errorMessage) const;
    QByteArray sendRequest(const QString& method, const QJsonObject& param, QString* errorMessage) const;
    QString normalizeMacString(QString value) const;
    QString extractMacForUi(const QByteArray& responseData) const;
    void collectMacFromJsonObject(const QJsonObject& obj, QString* out) const;
};

#endif  // BYDMES_H
