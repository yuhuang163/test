#ifndef QBULK_H
#define QBULK_H

#include <windows.h>
#include <winusb.h>
#include <setupapi.h>
#include <QString>
#include <QByteArray>

#pragma comment(lib, "winusb.lib")
#pragma comment(lib, "setupapi.lib")

class QBulk
{
public:
    QBulk();
    ~QBulk();

    bool openDevice(USHORT vid, USHORT pid, int mi = 4);
    void closeDevice();

    bool bulkRead(UCHAR ep, QByteArray &data, ULONG timeout = 1000);
    bool bulkWrite(UCHAR ep, const QByteArray &data, ULONG timeout = 1000);

private:
    HANDLE deviceHandle;
    WINUSB_INTERFACE_HANDLE usbHandle;
    QString findDevicePath(USHORT vid, USHORT pid, int mi);
};

#endif // QBULK_H
