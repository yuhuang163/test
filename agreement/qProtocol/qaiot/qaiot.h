#ifndef QAIOT_H
#define QAIOT_H

#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <cstdint>

#include "qprotocol.h"

class Qaiot : public qProtocol {
    Q_OBJECT
public:
    explicit Qaiot(QSerialPort* parent = nullptr);

    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;
    bool sendCustomMessage(const QVariantMap& map) override;

private:
    struct TlvNode {
        quint8 rawType = 0;
        quint8 type = 0;
        bool hasChildren = false;
        QByteArray value;
        QList<TlvNode> children;
    };

    struct Message {
        quint8 serviceId = 0;
        quint8 commandId = 0;
        QList<TlvNode> tlvs;
    };

    bool parseMessage(const QByteArray& frame, Message* message, QString* errorMessage) const;
    bool parseTlvs(const QByteArray& data, int start, int end, QList<TlvNode>* out, QString* errorMessage) const;
    bool readVarLength(const QByteArray& data, int* pos, int end, int* length, QString* errorMessage) const;
    QByteArray buildMessage(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs) const;
    QByteArray buildTlvs(const QList<TlvNode>& tlvs) const;
    QByteArray encodeVarLength(int length) const;
    bool tlvFromVariant(const QVariant& value, TlvNode* out, QString* errorMessage) const;
    bool valueFromVariant(const QVariant& value, QByteArray* out, QString* errorMessage) const;
    QString describeMessage(const Message& message) const;
    QString describeTlv(const TlvNode& tlv, int depth = 0) const;

    QSerialPort* serialPort = nullptr;
};

#endif  // QAIOT_H
