#include "qbulk.h"
#include <QDebug>
#include <Usbiodef.h>
// WinUSB 官方 GUID
DEFINE_GUID(GUID_DEVINTERFACE_WINUSB,
            0xdee824ef, 0x729b, 0x4a0e,
            0x9c, 0x14, 0xb7, 0x11, 0x7d, 0x33, 0xa8, 0x17);

QBulk::QBulk()
{
    deviceHandle = INVALID_HANDLE_VALUE;
    usbHandle = nullptr;

}

QBulk::~QBulk()
{
    closeDevice();
}



QString QBulk::findDevicePath(USHORT vid, USHORT pid, int mi)
{
    HDEVINFO devInfo = SetupDiGetClassDevs(
        &GUID_DEVINTERFACE_WINUSB,
        NULL,
        NULL,
        DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

    if (devInfo == INVALID_HANDLE_VALUE)
        return "";

    SP_DEVICE_INTERFACE_DATA iface;
    iface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);

    DWORD index = 0;
    while (SetupDiEnumDeviceInterfaces(devInfo, NULL, &GUID_DEVINTERFACE_WINUSB, index, &iface))
    {
        DWORD requiredSize = 0;
        SetupDiGetDeviceInterfaceDetail(devInfo, &iface, NULL, 0, &requiredSize, NULL);

        PSP_DEVICE_INTERFACE_DETAIL_DATA detail =
            (PSP_DEVICE_INTERFACE_DETAIL_DATA)malloc(requiredSize);

        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);

        if (!SetupDiGetDeviceInterfaceDetail(devInfo, &iface, detail, requiredSize, NULL, NULL))
        {
            free(detail);
            index++;
            continue;
        }

        QString path = QString::fromWCharArray(detail->DevicePath);

        if (path.contains(QString("vid_%1").arg(vid,4,16,QChar('0')), Qt::CaseInsensitive) &&
            path.contains(QString("pid_%1").arg(pid,4,16,QChar('0')), Qt::CaseInsensitive) &&
            path.contains(QString("mi_%1").arg(mi,2,16,QChar('0')), Qt::CaseInsensitive))
        {
            free(detail);
            SetupDiDestroyDeviceInfoList(devInfo);
            return path;
        }

        free(detail);
        index++;
    }

    SetupDiDestroyDeviceInfoList(devInfo);
    return "";
}


bool QBulk::openDevice(USHORT vid, USHORT pid, int mi)
{
    QString devPath = findDevicePath(vid, pid, mi);

    if (devPath.isEmpty())
    {
        qDebug() << "Device path not found!";
        return false;
    }

    qDebug() << "Found device path:" << devPath;

    deviceHandle = CreateFile(
        devPath.toStdWString().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
        NULL);

    if (deviceHandle == INVALID_HANDLE_VALUE)
    {
        qDebug() << "CreateFile failed:" << GetLastError();
        return false;
    }

    if (!WinUsb_Initialize(deviceHandle, &usbHandle))
    {
        qDebug() << "WinUsb_Initialize failed:" << GetLastError();
        CloseHandle(deviceHandle);
        deviceHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    qDebug() << "WinUSB device opened successfully!";
    return true;
}

void QBulk::closeDevice()
{
    if (usbHandle)
    {
        WinUsb_Free(usbHandle);
        usbHandle = nullptr;
    }
    if (deviceHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(deviceHandle);
        deviceHandle = INVALID_HANDLE_VALUE;
    }
}

bool QBulk::bulkRead(UCHAR ep, QByteArray &data, ULONG timeout)
{
    if (!usbHandle) return false;

    UCHAR buffer[4096];
    ULONG transferred = 0;

    if (!WinUsb_ReadPipe(usbHandle, ep, buffer, sizeof(buffer), &transferred, NULL))
        return false;

    data = QByteArray((char*)buffer, transferred);
    return true;
}

bool QBulk::bulkWrite(UCHAR ep, const QByteArray &data, ULONG /*timeout*/)
{
    if (!usbHandle) return false;

    ULONG transferred = 0;
    return WinUsb_WritePipe(usbHandle, ep,
                            (PUCHAR)data.data(), data.size(),
                            &transferred, NULL);
}
