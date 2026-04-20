#ifndef Qfctp_H
#define Qfctp_H
#include <QByteArray>
#include <QList>
#include <QSerialPort>
#include <QVariant>
#include <cstdint>

#include "qprotocol.h"

class Qfctp : public QSerialPort, public qProtocol {
    friend void qfctp_on_full_frame(const uint8_t *frame_data, uint16_t frame_len, void *user_data);

public:
    explicit Qfctp(QSerialPort* parent = nullptr);
    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;

private:
    enum PhyParseState {
        PHY_STATE_IDLE = 0,
        PHY_STATE_HEADER,
        PHY_STATE_LEN,
        PHY_STATE_CHANNEL,
        PHY_STATE_PAYLOAD,
    };

    void        handleFullFrame(const uint8_t *frameData, uint16_t frameLen);
    bool        tryUnwrapPhyPacket(const QByteArray &packet, QList<QByteArray> &outPackets);
    QByteArray  wrapPhyPacket(const QByteArray &innerPacket) const;
    bool        sendPacket(const QByteArray &innerPacket, QByteArray *outPhyPacket = nullptr) const;
    void        sendFactoryTestMode(bool enter);

    QSerialPort *serialPort = nullptr;
    PhyParseState m_phyState = PHY_STATE_IDLE;
    int           m_phyHeaderHits = 0;
    int           m_phyExpectedLen = 0;
    uint8_t       m_phyChannel = 0;
    QByteArray    m_phyPayload;
    uint8_t m_fctpSeq = 0;
};

#endif // Qfctp_H
