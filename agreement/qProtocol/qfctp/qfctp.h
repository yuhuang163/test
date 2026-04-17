#ifndef Qfctp_H
#define Qfctp_H
#include <QByteArray>
#include <QSerialPort>

#include "qprotocol.h"

class Qfctp : public QSerialPort, public qProtocol {
public:
    Qfctp();
    void parseCmd(const QByteArray& byte) override;
};

#endif // Qfctp_H
