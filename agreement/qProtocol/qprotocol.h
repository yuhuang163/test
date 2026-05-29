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

signals:
    void send_pb_date(QString data);
    void sendGetProductResponse(int data);
    void send_base_data(ProtocolBaseInfoData data);
    void send_sn_data(ProtocolSnData data);
    void send_battary(ProtocolBatteryData data);
    void send_button_state(ProtocolButtonStateData data);
    void send_periph_data(ProtocolPeriphStateData data);
    void send_photosensitive_info(ProtocolPhotosensitiveData data);
};

#endif  // QPROTOCOL_H
