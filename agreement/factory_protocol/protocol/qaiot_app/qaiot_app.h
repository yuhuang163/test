#ifndef QAIOT_APP_H
#define QAIOT_APP_H

#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <cstdint>

#include "aiot_link_codec.h"
#include "dongle_phy_codec.h"
#include "qprotocol.h"

/**
 * Momcozy AIoT 应用层（规范 v1.0.1）。
 * 发给 Dongle 的 PHY 通道按服务小节区分，不固定走 App：
 * 5.1 Device Management → 5，5.7 Data → 6，5.10 Pump → 7；OTA 才是通道 2。
 * 链路层与产测 AIOT 相同（SOF 0x5A），应用层为 Service + Command + TLV。
 */
class QaiotApp : public qProtocol {
    Q_OBJECT
  public:
    explicit QaiotApp(QSerialPort* parent = nullptr);

    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;
    bool sendCustomMessage(const QVariantMap& map) override;

  private:
    struct TlvNode {
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
    QString describeMessage(const Message& message) const;
    QString describeTlv(const TlvNode& tlv, int depth) const;

    TlvNode makeLeaf(quint8 type, const QByteArray& value) const;
    TlvNode makeParent(quint8 type, const QList<TlvNode>& children) const;
    bool findTlv(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const;
    bool findTlvDeep(const QList<TlvNode>& tlvs, quint8 type, TlvNode* out) const;

    bool sendServiceCommand(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs,
                            const QString& actionName, bool versionHeader = false);
    bool sendAppPdu(const QByteArray& appPdu, bool versionHeader, quint8 channel);
    QByteArray wrapPhyPacket(const QByteArray& innerPacket, quint8 channel) const;
    bool tryUnwrapPhyPacket(const QByteArray& packet, QList<QByteArray>& outPackets);

    void handleLinkFrame(const AiotLinkCodec::Frame& frame);
    void handleAppMessage(const Message& message);

    QSerialPort* serialPort = nullptr;
    AiotLinkCodec linkCodec_;
    uint8_t psnCounter_ = 0;
    // 协商带回 link_layer_version 后，后续帧按 Version=0 头发送
    bool useLinkV2_ = false;
    int maxPayload_ = 512;

    QByteArray reassembly_;
    bool reassembling_ = false;
    uint8_t expectFsn_ = 0;

    // 按服务收：FAC / OTA(APP) / DEV_MGMT / DATA / PUMP
    DonglePhyRxCodec phyRx_{kDonglePhyRxAcceptAiotApp, "[QAIOT-APP]"};

    quint8 pendingService_ = 0xFF;
    quint8 pendingCommand_ = 0xFF;
    QString pendingAction_;
};

#endif // QAIOT_APP_H
