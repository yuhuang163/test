#ifndef QROOT_H
#define QROOT_H

#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "qprotocol.h"

/** Root 吸奶器 PCBA 测试协议：帧格式为串口协议，经 Dongle 蓝牙透传收发（同 Qpb）。 */
class Qroot : public qProtocol {
    Q_OBJECT

  public:
    explicit Qroot(QSerialPort* port = nullptr);

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
        BatteryTemp = 0x80,
        BatteryInfo = 0xE0,
        AgingEnter = 0xAF,
        TestMode = 0x90,
        SoftVersion = 0x91,
        MacRead = 0x92,
        LedTest = 0x93,
        Vibration = 0x94,
        MacWrite = 0x95,
        FlangeStatus = 0x96,
        NtcStatus = 0x97,
        HeatTemp = 0x98,
        VibStatus = 0x99,
        KeyNotify = 0x9A,
        ToggleKeyNotify = 0x9B,
        PumpTestEnter = 0x9D,
        PumpTestExit = 0x9E,
        FactoryReset = 0xFC,
    };

    static constexpr quint8 kFactoryResetParam = 0x04;

    static quint8 checksum8(const QByteArray& data);
    static QByteArray buildPacket(quint8 ct, quint8 cid, const QByteArray& body);
    static QString formatMacFromWire(const QByteArray& mac6);
    static QString formatSoftVersion(const QByteArray& body);
    static QByteArray parseMacToWire(const QVariant& data);
    static quint8 parseOnOffParam(const QVariant& data, quint8 defaultValue = 1);

    enum PhyParseState : quint8 {
        PhyIdle = 0,
        PhyHeader,
        PhyChannel,
        PhyLen,
        PhyPayload,
    };

    static QByteArray wrapPhyPacket(const QByteArray& innerPacket);
    void feedPhyRx(const QByteArray& data, QList<QByteArray>& outInnerPackets);

    bool sendPacket(quint8 ct, quint8 cid, const QByteArray& body);
    void drainRxBuffer();
    void handleFrame(quint8 ct, quint8 cid, const QByteArray& body);

    void sendTestMode(quint8 mode);
    void sendLedTest(quint8 mode);
    void sendVibration(quint8 mode);
    void sendMacWrite(const QByteArray& mac6);
    void sendQuery(CommandId cid);
    /** Notify 0x9A：body 0x01 开启按键上报，0x00 关闭。 */
    void sendKeyNotifySwitch(quint8 enable);

    static QString formatKeyNotifyLabel(quint8 keyId);
    void emitKeyNotifyReport(quint8 keyId);
    void emitToggleKeyNotifyReport(quint8 state);
    void emitBatteryInfoReport(const QByteArray& body);

    QSerialPort* serialPort_ = nullptr;
    QByteArray rxBuffer_;
    quint8 pendingCid_ = 0;
    bool hasPending_ = false;

    PhyParseState phyState_ = PhyIdle;
    int phyHeaderHits_ = 0;
    int phyExpectedLen_ = 0;
    quint8 phyChannel_ = 0;
    QByteArray phyPayload_;
};

#endif // QROOT_H
