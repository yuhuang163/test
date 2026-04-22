#ifndef QPROTOCOL_H
#define QPROTOCOL_H

#include <QByteArray>
#include <QVariant>
#include <QVariantMap>

#include "qprotocol_types.h"

class qProtocol {
public:
    qProtocol();
    virtual ~qProtocol() = default;

    // 协议收包解析入口（由具体协议实现）。
    virtual void parseCmd(const QByteArray& byte) = 0;
    // 统一命令下发入口（由具体协议实现）。
    virtual void set(DeviceCmd cmd, const QVariant& data = {}) = 0;
    // 统一命令读取入口（由具体协议实现）。
    virtual void get(DeviceCmd cmd, const QVariant& param = {}) = 0;
    // 统一通用消息发送入口（由具体协议实现；不支持时返回 false）。
    virtual bool sendCustomMessage(const QVariantMap& map) = 0;
};

#endif  // QPROTOCOL_H
