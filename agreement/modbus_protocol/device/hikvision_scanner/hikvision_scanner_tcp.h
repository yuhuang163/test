#ifndef HIKVISION_SCANNER_TCP_H
#define HIKVISION_SCANNER_TCP_H

#include <QString>
#include <QTcpSocket>

class HikvisionScannerTcp {
public:
    HikvisionScannerTcp();
    ~HikvisionScannerTcp();

    bool connectDevice(const QString& ip, int port, int timeoutMs, QString* errorMessage = nullptr);
    void disconnectDevice();
    bool isConnected() const;

    bool sendStartAndRead(QString* outResult, int timeoutMs, QString* errorMessage = nullptr);

private:
    QTcpSocket socket_;
};

#endif // HIKVISION_SCANNER_TCP_H
