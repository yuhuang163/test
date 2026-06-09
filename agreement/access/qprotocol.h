#ifndef QPROTOCOL_H
#define QPROTOCOL_H

#include <QByteArray>
#include <QObject>
#include <QVariant>
#include <QVariantMap>

#include "qprotocol_types.h"

class qProtocol : public QObject {
    Q_OBJECT
  public:
    explicit qProtocol(QObject* parent = nullptr);
    virtual ~qProtocol() = default;

    // 协议收包解析入口（由具体协议实现）。
    virtual void parseCmd(const QByteArray& byte) = 0;
    // 统一命令下发入口（由具体协议实现）。
    virtual void set(DeviceCmd cmd, const QVariant& data = {}) = 0;
    // 统一命令读取入口（由具体协议实现）。
    virtual void get(DeviceCmd cmd, const QVariant& param = {}) = 0;
    // 统一通用消息发送入口（由具体协议实现；不支持时返回 false）。
    virtual bool sendCustomMessage(const QVariantMap& map) = 0;

  protected:
    void emitReport(const QString& reportType, const QVariant& payload = QVariant()) {
        emit reportReceived(ProtocolReport(reportType, payload));
    }

  signals:
    /** 统一上行数据信封 */
    void reportReceived(const ProtocolReport& report);
    /** 传输层 ACK，与结构化 report 分离 */
    void sendGetProductResponse(int data);
};

#endif // QPROTOCOL_H
