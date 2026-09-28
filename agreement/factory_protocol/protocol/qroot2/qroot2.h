#ifndef QROOT2_H
#define QROOT2_H

#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "dongle_phy_codec.h"

/**
 * Qroot2 协议：AA55 帧经 Dongle 蓝牙透传。
 * - Air1：CID 以 0x90~0xAF / 0xDA~0xDD 等为主（0xA5 版本、0xA0 SN、0xA2~A4 泵、0xAB 关机…）
 * - 【仅 M5 Smart】另用 App 风格 CID：0xF0 设备信息、0xF1 SN、0xB0 完整状态、0xC0 泵控制、0xFC 系统控制
 * - 【仅 M5 Smart】三元组烧录：0xF8 productID / 0xF9 deviceID / 0xFA EncryptionKey；读 0xF7
 */
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
        PumpStallCurrent = 0x82, // 读泵电流检测电压 mV
        LedControl = 0x90,
        MacRead = 0x91,
        MacWrite = 0x92,
        Vibration = 0x93,
        SoftVersion = 0xA5, // Air1 读版本
        DeviceSnRead = 0xA0, // Air1 读 SN
        DeviceSnWrite = 0xA1,
        ModeSet = 0xA2,   // Air1 设模式
        LevelSet = 0xA3,  // Air1 设档位
        PumpState = 0xA4, // Air1 泵启停
        KeyTest = 0xA6,
        /** Air1：Ack 约 2B 电量+状态；【仅 M5 Smart】Ack CAL=0x2D 完整吸奶器状态 */
        GetState = 0xB0,
        BowlCalibRead = 0xAD,
        BowlCalib = 0xAE,
        TestMode = 0xAF,
        PoseAngle = 0xDA,
        PoseAbnormal = 0xD3,
        PoseSwitch = 0xDB,
        PoseSwitchRead = 0xDC, // 读角度检测开关；勿用 0xF0
        PoseCalib = 0xDD,
        PowerOff = 0xAB, // Air1 关机
        /** 【仅 M5 Smart】吸奶器控制 REQ CAL=0x0A */
        PumpControl = 0xC0,
        /** 【仅 M5 Smart】获取设备信息（含软硬件版本） */
        DeviceInfoGet = 0xF0,
        /** 【仅 M5 Smart】获取设备 SN；空 body；Ack SN 33/35B + 4B 预留 */
        DeviceSnGetM5 = 0xF1,
        /** 【仅 M5 Smart】读三元组；空 body；Ack 30B=productID(6)+deviceID(16)+KeyTail(8 RC4) */
        TupleRead = 0xF7,
        /** 【仅 M5 Smart】写 productID ≤6 */
        ProductIdWrite = 0xF8,
        /** 【仅 M5 Smart】写 deviceID ≤16 */
        DeviceIdWrite = 0xF9,
        /** 【仅 M5 Smart】写 EncryptionKey ≤16 */
        EncryptionKeyWrite = 0xFA,
        /** 【仅 M5 Smart】系统控制：01关机 02重启 03OTA 04复位 */
        SystemControl = 0xFC,
    };

    enum class BowlCalibCapView : quint8 { None = 0, Empty = 1, Full = 2 };

    static quint8 checksum8(const QByteArray& data);
    static QByteArray buildPacket(quint8 ct, quint8 cid, const QByteArray& body);
    static QString formatMacFromWire(const QByteArray& body);
    static QString formatSoftVersion(const QByteArray& body);
    /** 【仅 M5 Smart】0xF0 打包版本：111 → "11.1" */
    static QString formatPackedVersion(quint8 packed);
    static QString productModelName(quint8 code);
    static QByteArray parseMacToWire(const QVariant& data);
    static quint8 parseOnOffParam(const QVariant& data, quint8 defaultValue = 1);
    static quint8 parseUInt8Param(const QVariant& data, quint8 defaultValue, const QString& key = QStringLiteral("value"));
    static QByteArray truncateUtf8Bytes(const QVariant& data, int maxLen, const QString& valueKey = QStringLiteral("value"));
    /** 【仅 M5 Smart】0xC0 10B body */
    static QByteArray buildPumpControlBody(const QVariant& data);

    static QByteArray wrapPhyPacket(const QByteArray& innerPacket);
    void feedPhyRx(const QByteArray& data, QList<QByteArray>& outInnerPackets);

    bool sendPacket(quint8 ct, quint8 cid, const QByteArray& body);
    void drainRxBuffer();
    void handleFrame(quint8 ct, quint8 cid, const QByteArray& body);

    void sendDeviceSnWrite(const QByteArray& sn, quint8 snType, int snLength = 35);
    void sendProductIdWrite(const QByteArray& productId);
    void sendDeviceIdWrite(const QByteArray& deviceId);
    void sendEncryptionKeyWrite(const QByteArray& key);
    bool setSn(const QVariant& data);
    static QByteArray buildLedControlBody(const QVariant& data);
    static QByteArray buildSuctionModeLevel(const QVariant& data, quint8* modeOut, quint8* levelOut);

    QSerialPort* serialPort_ = nullptr;
    QByteArray rxBuffer_;
    quint8 pendingCid_ = 0;
    bool hasPending_ = false;
    ProtocolSnType pendingSnType_ = ProtocolSnType::TailSn;
    QByteArray pendingWriteSn_;
    /** 最近一次写入的 EncryptionKey，用于 0xF7 KeyTail RC4 解密校验 */
    QByteArray lastEncryptionKey_;
    BowlCalibCapView pendingBowlCalibCapView_ = BowlCalibCapView::None;

    DonglePhyRxCodec phyRx_{kDonglePhyRxAcceptFacOnly, "[Qroot2]"};
};

#endif // QROOT2_H
