#ifndef BYDMES_H
#define BYDMES_H

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include "my_set/my_typedef.h"
#include "qmes.h"

/// BYD MES2 实现；.cpp 内按 ①HTTP 组包 ②JSON 行表 ③外部 ini ④构造/读键 ⑤报文组包 ⑥响应解析 ⑦HTTP 发送 ⑧槽函数 分区维护。
class bydmes : public Qmes {
    Q_OBJECT
  public:
    bydmes();
    /// 从上位机设置.ini 读取 ConfigFilePath，解析出外部 mes_config.ini 路径并登记；取参时每次 QSettings 按键读盘，不整表缓存。
    /// 外部 ini 编码见 bydmes.cpp 的 bydMesPickIniFileCodec（拒绝非 UTF 的 codecForUtfText 误判；无 BOM 时 UTF-8 严格合法则 UTF-8，否则 GB2312/GBK）。
    /// 返回：成功返回 true，失败返回 false 并设置 errorMessage
    static bool loadExternalMesConfig(QString* errorMessage = nullptr);
    /// 清除已登记的外部 ini 路径
    static void clearExternalMesConfig();

    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

  private:
    QString baseUrl() const;
    QString settingsValue(const QString& key, const QString& fallback = QString()) const;
    bool ensureExternalMesConfig(const MesPacketData& pack);
    QJsonArray buildBydTestDataList(const MesPacketData& pack, const QString& testTime) const;
    QJsonObject buildBydTestDataCollectParam(const MesPacketData& pack) const;
    /// 「按过程码返回值中解析 SN」。
    QString parseSnFromGetSnByProcessCodeResponse(const QByteArray& responseData) const;
    bool isSuccessResponse(const QByteArray& responseData, QString* responseText, QString* errorMessage) const;
    QByteArray sendRequest(const QString& method, const QJsonObject& param, QString* errorMessage) const;
    /// NET 与 LoginID/CLIENT_ID 缺失时 qWarning + operateMesError，返回 true 表示应中止请求（非 const：需 emit 信号）
    bool emitIfMissingLoginClientOrNet(const MesPacketData& pack, const QJsonObject& param, const QString& sceneLabel);
};

#endif // BYDMES_H
