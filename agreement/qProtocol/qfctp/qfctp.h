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

class Qfctp : public qProtocol {
    Q_OBJECT
    friend void qfctp_on_full_frame(const uint8_t *frame_data, uint16_t frame_len, void *user_data);

public:
    explicit Qfctp(QSerialPort* parent = nullptr);
    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;
    bool sendCustomMessage(const QVariantMap &map) override;


private:
    using ResponseHandler = void (Qfctp::*)(const uint8_t *mainValue, uint16_t mainLen);

    struct PendingRequest {
        uint16_t    serviceId = 0;
        uint16_t    tlvType = 0;
        QString     actionName;
        QByteArray  requestValue;  // 请求 TLV Value（如按键电容的 KK）
    };

    enum PhyParseState {
        PHY_STATE_IDLE = 0,
        PHY_STATE_HEADER,
        PHY_STATE_LEN,
        PHY_STATE_CHANNEL,
        PHY_STATE_PAYLOAD,
    };

    void        handleFullFrame(const uint8_t *frameData, uint16_t frameLen);
    void        handleResponseService(uint8_t seq, uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen, const QByteArray &frameRaw);
    void        handleNotifyService(uint16_t serviceId, const uint8_t *tlvs, uint16_t serviceLen);
    bool        handleRequestResult(uint8_t seq, const PendingRequest &req, int mainValue, bool hasMainValue, uint16_t errCode, bool hasErrCode);
    void        registerResponseHandlers();
    void        registerResponseHandler(uint16_t serviceId, uint16_t tlvType, const QString &responseName, ResponseHandler handler);
    void        handleResponseByType(const PendingRequest &req, const uint8_t *mainValue, uint16_t mainLen);
    bool        isDataResponse(uint16_t serviceId, uint16_t tlvType) const;
    QString     responseNameFor(uint16_t serviceId, uint16_t tlvType) const;


    void        handleRspSnRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspTupleRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspAgingStatus(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspDeviceException(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspTrimGet(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspFactoryDoneRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspRssiRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspSensorState(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspFwVersionRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspMacRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspBatteryRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspKeySignalRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspLightCalibRead(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspLcdBacklight(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspLightReportCtrl(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspLightCalibWrite(const uint8_t *mainValue, uint16_t mainLen);
    void        handleRspChargeCurrentRead(const uint8_t *mainValue, uint16_t mainLen);



    bool        setSn(const QVariant &data);
    bool        setCaseAgingMode(const QVariantMap &map);
    bool        setCaseAgingExit();
    bool        setCaseSuctionMode(const QVariantMap &map);
    bool        setCaseBtSignalMode(const QVariantMap &map);
    bool        setCaseBtNoSignalMode(const QVariantMap &map);
    bool        setCaseBtFreqMode(const QVariantMap &map);
    bool        setCaseStandbyMode(const QVariantMap &map);
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
    bool        sendRequest(uint16_t serviceId, uint16_t tlvType, const QByteArray &value, const char *actionName);
    bool        tryUnwrapPhyPacket(const QByteArray &packet, QList<QByteArray> &outPackets);
    
    QByteArray  wrapPhyPacket(const QByteArray &innerPacket) const;
    bool        sendPacket(const QByteArray &innerPacket, QByteArray *outPhyPacket = nullptr) const;
    bool        sendServiceTlv(uint16_t serviceId, uint16_t tlvType, QByteArray value, const char *actionName, uint8_t *outSeq = nullptr);
    bool        sendTestsServiceTlv(uint16_t tlvType, QByteArray value, const char *actionName);
    void        sendFactoryTestMode(bool enter);

    QSerialPort *serialPort = nullptr;
    PhyParseState m_phyState = PHY_STATE_IDLE;
    int           m_phyHeaderHits = 0;
    int           m_phyExpectedLen = 0;
    uint8_t       m_phyChannel = 0;
    QByteArray    m_phyPayload;
    uint8_t       m_fctpSeq = 0;
    QHash<uint8_t, PendingRequest> m_pendingRequests;
    QHash<uint32_t, ResponseHandler> m_responseHandlers;
    QHash<uint32_t, QString> m_responseNames;
    /** handleResponseByType 内当前应答对应的请求上下文（供按键电容等需关联请求参数的处理）。 */
    PendingRequest m_currentResponseRequest;

signals:
    void send_tuple_parsed(ProtocolTupleData data);
    void send_aging_status(ProtocolAgingStatusData data);
    void send_device_exception(ProtocolDeviceExceptionData data);
    void send_trim_read(ProtocolUInt32ValueData data);
    void send_factory_done_read(ProtocolFactoryDoneData data);
    void send_rssi_read(ProtocolRssiData data);
    void send_mac_read(ProtocolMacData data);
    void send_key_signal_read(ProtocolUInt32ValueData data);
    void send_light_calib_read(ProtocolUInt32ValueData data);
    void send_lcd_backlight_ack(ProtocolAckData data);
    void send_light_report_ctrl_ack(ProtocolAckData data);
    void send_light_calib_write_ack(ProtocolUInt32ValueData data);
    void send_charge_current_read(ProtocolUInt32ValueData data);
};

#endif // Qfctp_H
