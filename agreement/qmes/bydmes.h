#ifndef BYDMES_H
#define BYDMES_H

#include <QJsonObject>
#include <QMap>
#include <QString>

#include "my_set/my_typedef.h"
#include "qmes.h"

class bydmes : public Qmes {
    Q_OBJECT
public:
    bydmes();
    /// 产线 BYD MES：设置页「选产线文件」解析结果进内存（不写 ini），在 bydmes 中取键时优先于本机 Mes/MES，其余键仍按原逻辑读 Mes/→MES/→fallback。
    static void setRuntimeMesParamMap(const QMap<QString, QString>& params);
    static void clearRuntimeMesParamMap();

    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

    /// 根据过程码请求 SN：成功时 emit sendMesTestvalue（与 GetCustomData 同形 JSON 数组，NAME=SN）
    void QuerySnByProcessCode(MesPacketData pack);

private:
    QString baseUrl() const;
    QString settingsValue(const QString& key, const QString& fallback = QString()) const;
    QString buildRemark(const MesPacketData& pack) const;
    QJsonObject buildBydStartParam(const MesPacketData& pack) const;
    QJsonObject buildBydGetCustomDataParam() const;
    /// 过程码换 SN 的 param；method 默认 GetSnByProcessCode，可由 Mes/GetSnByProcessCodeMethod 覆盖
    QJsonObject buildBydGetSnByProcessCodeParam(const QString& processCode) const;
    QJsonObject buildBydTestDataCollectParam(const MesPacketData& pack) const;
    QJsonObject buildBydCompleteParam(const MesPacketData& pack) const;
    QJsonObject buildBydNcCompleteParam(const MesPacketData& pack) const;
    QJsonArray buildBydTestDataList(const MesPacketData& pack, const QString& testTime) const;
    QString normalizeTestResult(const QString& result) const;
    bool isSuccessResponse(const QByteArray& responseData, QString* responseText, QString* errorMessage) const;
    QByteArray sendRequest(const QString& method, const QJsonObject& param, QString* errorMessage) const;
    /// 解析 GetCustomData 返回 JSON 中 DATA[] 的 NAME / VALUE / REMARK，供 sendMesTestvalue 下发。
    QString formatGetCustomDataItemsJson(const QByteArray& responseData) const;
    /// 从「按过程码查 SN」接口返回中解析主键（兼容 DATA 为对象/字符串/行表；行表见 Mes/GetSnByProcessCodeItemName）
    QString parseSnFromGetSnByProcessCodeResponse(const QByteArray& responseData) const;
};

#endif  // BYDMES_H
