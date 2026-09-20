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
        PumpStallCurrent = 0x82, // 读泵电流检测电压 mV
        LedControl = 0x90,
        MacRead = 0x91,
        MacWrite = 0x92,
        Vibration = 0x93,
        SoftVersion = 0xA5,
        DeviceSnRead = 0xA0,
        DeviceSnWrite = 0xA1,
        ModeSet = 0xA2,
        LevelSet = 0xA3,
        PumpState = 0xA4,
        KeyTest = 0xA6,
        GetState = 0xB0, // BLE_CMD_GET_STATE；空 body；Ack：电量% + 工作/充电状态
        BowlCalibRead = 0xAD, // 读已存空/满校准电容，空 body；Ack 13B
        BowlCalib = 0xAE, // body 0空碗 1满碗，固件当场采样，不下发电容
        TestMode = 0xAF,  // body 0自检 1老化
        PoseAngle = 0xDA,      // 获取角度信息（产测）；Ack 4B：pitch/roll 0.1° 有符号大端
        PoseAbnormal = 0xD3,   // 角度异常停机 Notify；body 4B 同 0xDA（|pitch|>50° 或 |roll|>40°）
        PoseSwitch = 0xDB,     // 写角度检测开关（会停泵）；勿与 App 0xCB 混淆
        PoseSwitchRead = 0xDC, // 读角度检测开关；勿用 0xF0 看停泵相关
        PoseCalib = 0xDD,
        PowerOff = 0xAB, // BLE_CMD_SET_PWROFF；Req body 1B 固件不判，原样回 ACK
    };

    /** 0xAD 读校准电容：空碗/满碗步骤共用回包，按请求命令分发不同 ReportType */
    enum class BowlCalibCapView : quint8 { None = 0, Empty = 1, Full = 2 };

    static quint8 checksum8(const QByteArray& data);
    static QByteArray buildPacket(quint8 ct, quint8 cid, const QByteArray& body);
    static QString formatMacFromWire(const QByteArray& body);
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
    BowlCalibCapView pendingBowlCalibCapView_ = BowlCalibCapView::None;

    DonglePhyRxCodec phyRx_{kDonglePhyRxAcceptFacOnly, "[Qroot2]"};
};

#endif // QROOT2_H
