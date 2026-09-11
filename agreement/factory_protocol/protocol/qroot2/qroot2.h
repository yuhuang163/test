#ifndef QROOT2_H
#define QROOT2_H

#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "dongle_phy_codec.h"

/** Air1 等产品的 Qroot2 协议：AA55 帧经 Dongle 蓝牙透传（帧结构与 Qroot 相同，CID 表不同）。 */
class Qroot2 : public qProtocol {
    Q_OBJECT

  public:
    explicit Qroot2(QSerialPort* port = nullptr);

    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;
    bool sendCustomMessage(const QVariantMap& map) override;

  private:
    enum CommandType : quint8 {
        Req = 0x00,
        Ack = 0x01,
        Nack = 0x02,
        Notify = 0x03,
    };

    enum CommandId : quint8 {
        LedControl = 0x90,
        MacRead = 0x91,
        MacWrite = 0x92,
        Vibration = 0x93,
        SoftVersion = 0xA5,
        DeviceSnRead = 0xA0,
        DeviceSnWrite = 0xA1,
        ModeSet = 0xA2,
        LevelSet = 0xA3,
        KeyTest = 0xA6,
        BowlCalib = 0xAE,
        PoseSwitch = 0xDB,
        PoseCalib = 0xDD,
        PowerOff = 0xFC,
    };

    static constexpr quint8 kAir1PowerOffParam = 0x04;
    static constexpr quint8 kLedControlSubCmd = 0x02;

    static quint8 checksum8(const QByteArray& data);
    static QByteArray buildPacket(quint8 ct, quint8 cid, const QByteArray& body);
    static QString formatMacFromWire(const QByteArray& mac6);
    static QString formatSoftVersion(const QByteArray& body);
    static QByteArray parseMacToWire(const QVariant& data);
    static quint8 parseOnOffParam(const QVariant& data, quint8 defaultValue = 1);
    static quint8 parseUInt8Param(const QVariant& data, quint8 defaultValue, const QString& key = QStringLiteral("value"));
    static QByteArray truncateUtf8Bytes(const QVariant& data, int maxLen, const QString& valueKey = QStringLiteral("value"));

    static QByteArray wrapPhyPacket(const QByteArray& innerPacket);
    void feedPhyRx(const QByteArray& data, QList<QByteArray>& outInnerPackets);

    bool sendPacket(quint8 ct, quint8 cid, const QByteArray& body);
    void drainRxBuffer();
    void handleFrame(quint8 ct, quint8 cid, const QByteArray& body);

    void sendDeviceSnWrite(const QByteArray& sn, quint8 snType);
    bool setSn(const QVariant& data);
    static QByteArray buildLedControlBody(const QVariant& data);
    static QByteArray buildSuctionModeLevel(const QVariant& data, quint8* modeOut, quint8* levelOut);

    QSerialPort* serialPort_ = nullptr;
    QByteArray rxBuffer_;
    quint8 pendingCid_ = 0;
    bool hasPending_ = false;
    ProtocolSnType pendingSnType_ = ProtocolSnType::TailSn;
    QByteArray pendingWriteSn_;

    DonglePhyRxCodec phyRx_{kDonglePhyRxAcceptFacOnly, "[Qroot2]"};
};

#endif // QROOT2_H
