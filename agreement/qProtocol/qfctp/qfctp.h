#ifndef Qfctp_H
#define Qfctp_H
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <cstdint>

#include "qprotocol.h"

class Qfctp : public QSerialPort, public qProtocol {
    Q_OBJECT
    friend void qfctp_on_full_frame(const uint8_t *frame_data, uint16_t frame_len, void *user_data);

public:
    explicit Qfctp(QSerialPort* parent = nullptr);
    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;

signals:
    void send_pb_date(QString data);

private:
    enum class RequestKind {
        Unknown = 0,
        FactoryMode,
        BurningMode,
        AgingModeExit,
        AgingStatusGet,
        SuctionMode,
        BtSignalMode,
        BtNoSignalMode,
        BtFreqMode,
        TrimSet,
        TrimGet,
        StandbyMode,
        SnWrite,
        SnRead,
        TupleWriteProductId,
        TupleWriteDeviceId,
        TupleWriteKey,
        TupleRead,
        FactoryDoneWrite,
        FactoryDoneRead,
        DeviceExceptionGet,
        SensorStateGet,
        RssiGet,
        BatteryGet,
        KeySignalGet,
        ChargeCurrentGet,
        CompensationSet,
        FwVersionGet,
        MacRead,
        MacWrite,
        PowerOff,
        LedTest,
        NightLightSet,
        FactoryReset,
        LcdBacklightSet,
        LightReportControl,
        LightCalibWrite,
        LightCalibRead
    };

    struct PendingRequest {
        RequestKind kind = RequestKind::Unknown;
        uint16_t    serviceId = 0;
        uint16_t    tlvType = 0;
        QString     actionName;
    };

    enum PhyParseState {
        PHY_STATE_IDLE = 0,
        PHY_STATE_HEADER,
        PHY_STATE_LEN,
        PHY_STATE_CHANNEL,
        PHY_STATE_PAYLOAD,
    };

    void        handleFullFrame(const uint8_t *frameData, uint16_t frameLen);
    void        handleResponseService(uint8_t seq, uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen);
    void        handleNotifyService(uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen);
    void        handleRequestResult(uint8_t seq, const PendingRequest &req, int mainValue, bool hasMainValue, uint16_t errCode, bool hasErrCode);
    bool        handleSetSn(const QVariant &data);
    bool        setCaseAgingMode(const QVariantMap &map);
    bool        setCaseAgingExit();
    bool        setCaseSuctionMode(const QVariantMap &map);
    bool        setCaseBtSignalMode(const QVariantMap &map);
    bool        setCaseBtNoSignalMode(const QVariantMap &map);
    bool        setCaseBtFreqMode(const QVariantMap &map);
    bool        setCaseStandbyMode(const QVariantMap &map);
    bool        setCaseWriteProductId(const QVariantMap &map);
    bool        setCaseWriteDeviceId(const QVariantMap &map);
    bool        setCaseWriteKey(const QVariantMap &map);
    bool        setCaseFactoryDoneWrite(const QVariantMap &map);
    bool        setCaseTrimSet(const QVariantMap &map);
    bool        setCaseMacWrite(const QVariantMap &map);
    bool        setCaseNightLightSet(const QVariantMap &map);
    bool        setCaseLedTest(const QVariantMap &map);
    bool        setCaseFactoryReset();
    bool        setCasePowerOff();
    bool        setCaseLcdBacklight(const QVariantMap &map);
    bool        setCaseLightReportControl(const QVariantMap &map);
    bool        setCaseLightCalibWrite(const QVariantMap &map);
    bool        setCaseCompensationSet(const QVariantMap &map);

    
    bool        getCaseTupleRead();
    bool        getCaseTrimRead();
    bool        getCaseFwVersionRead();
    bool        getCaseMacRead();
    bool        getCaseRssiRead(const QVariantMap &map);
    bool        getCaseKeySignalRead(const QVariantMap &map);
    bool        getCaseLightCalibRead(const QVariantMap &map);
    bool        getCaseChargeCurrentRead();
    bool        getCaseAgingStatusRead(const QVariantMap &map);
    bool        getCaseFactoryDoneRead();
    bool        getCaseDeviceExceptionRead();
    bool        getCasePeriphStateRead();
    bool        getCaseBatteryRead();
    bool        sendRequest(RequestKind kind, uint16_t serviceId, uint16_t tlvType, const QByteArray &value, const char *actionName);
    bool        tryUnwrapPhyPacket(const QByteArray &packet, QList<QByteArray> &outPackets);
    QByteArray  wrapPhyPacket(const QByteArray &innerPacket) const;
    bool        sendPacket(const QByteArray &innerPacket, QByteArray *outPhyPacket = nullptr) const;
    bool        sendServiceTlv(uint16_t serviceId, uint16_t tlvType, QByteArray value, const char *actionName, uint8_t *outSeq = nullptr);
    bool        sendTestsServiceTlv(uint16_t tlvType, QByteArray value, const char *actionName, RequestKind kind = RequestKind::Unknown);
    void        sendFactoryTestMode(bool enter);

    QSerialPort *serialPort = nullptr;
    PhyParseState m_phyState = PHY_STATE_IDLE;
    int           m_phyHeaderHits = 0;
    int           m_phyExpectedLen = 0;
    uint8_t       m_phyChannel = 0;
    QByteArray    m_phyPayload;
    uint8_t       m_fctpSeq = 0;
    QHash<uint8_t, PendingRequest> m_pendingRequests;
};

#endif // Qfctp_H
