#ifndef QAIOT_H
#define QAIOT_H

#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <cstdint>

#include "aiot_link_codec.h"
#include "qprotocol.h"

/**
 * Momcozy AIOT / FCT&ATE 协议（规范 v1.0.0）。
 * 串口路径：Dongle PHY(8×CC) → 链路层(SOF 0x5A) → 应用层(Service+Command+TLV)。
 * FCT&ATE Service ID 固定为 0x04。
 */
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

    /** Dongle 收包：8×AA → Channel → Len → Payload（与发：8×CC → Len → Channel 不同） */
    enum PhyState { PhyIdle, PhyHeader, PhyChannel, PhyLen, PhyPayload };

    bool parseMessage(const QByteArray& frame, Message* message, QString* errorMessage) const;
    bool parseTlvs(const QByteArray& data, int start, int end, QList<TlvNode>* out, QString* errorMessage) const;
    bool readVarLength(const QByteArray& data, int* pos, int end, int* length, QString* errorMessage) const;
    QByteArray buildMessage(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs) const;
    QByteArray buildTlvs(const QList<TlvNode>& tlvs) const;
    QByteArray encodeVarLength(int length) const;
    bool tlvFromVariant(const QVariant& value, TlvNode* out, QString* errorMessage) const;
    bool valueFromVariant(const QVariant& value, QByteArray* out, QString* errorMessage) const;
    QString describeMessage(const Message& message) const;
    QString describeTlv(const TlvNode& tlv, int depth = 0, quint8 commandId = 0xFF) const;

    TlvNode makeLeaf(quint8 type, const QByteArray& value) const;
    TlvNode makeParent(quint8 type, const QList<TlvNode>& children) const;
    QByteArray u8(quint8 v) const;
    QByteArray u16be(quint16 v) const;
    QByteArray u32be(quint32 v) const;
    bool findTlv(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const;
    bool findTlvDeep(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const;

    bool sendAppPdu(const QByteArray& appPdu);
    bool sendServiceCommand(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs,
                            const QString& actionName);
    QByteArray wrapPhyPacket(const QByteArray& innerPacket) const;
    bool tryUnwrapPhyPacket(const QByteArray& packet, QList<QByteArray>& outPackets);

    void handleLinkFrame(const AiotLinkCodec::Frame& frame);
    void handleAppMessage(const Message& message);
    void handleFctResponse(const Message& message);

    QSerialPort* serialPort = nullptr;
    AiotLinkCodec linkCodec_;

    // 多分片组包
    QByteArray reassembly_;
    bool reassembling_ = false;
    uint8_t expectFsn_ = 0;

    // Dongle PHY 状态机（与 Qfctp 一致：TX=[CC*][len][ch][payload]）
    PhyState phyState_ = PhyIdle;
    int phyHeaderHits_ = 0;
    int phyExpectedLen_ = 0;
    uint8_t phyChannel_ = 0;
    QByteArray phyPayload_;

    quint8 pendingService_ = 0;
    quint8 pendingCommand_ = 0;
    QString pendingAction_;
};

#endif // QAIOT_H
