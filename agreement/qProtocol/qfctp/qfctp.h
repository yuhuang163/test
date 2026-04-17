#ifndef Qfctp_H
#define Qfctp_H
#include <QByteArray>
#include <QVariant>
#include <QSerialPort>

#include "qprotocol.h"

class Qfctp : public QSerialPort, public qProtocol {
public:
    explicit Qfctp(QSerialPort* parent = nullptr);
    void parseCmd(const QByteArray& byte) override;
    void set(DeviceCmd cmd, const QVariant& data = {}) override;
    void get(DeviceCmd cmd, const QVariant& param = {}) override;
};

#endif // Qfctp_H
