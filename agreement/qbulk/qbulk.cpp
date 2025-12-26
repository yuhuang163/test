#include "qbulk.h"
#include <QDebug>

QBulk::QBulk() : handle(nullptr)
{
    usb_init();              // 初始化 libusb 0.1
    usb_find_busses();
    usb_find_devices();
    qDebug() << "[QBulk] libusb 0.1 initialized.";
}

QBulk::~QBulk()
{
    closeDevice();
    qDebug() << "[QBulk] libusb exited.";
}

bool QBulk::openDevice(uint16_t vid, uint16_t pid, int interfaceNumber)
{
    qDebug() << "[QBulk] Scanning USB devices...";

    struct usb_bus *bus;
    struct usb_device *dev = nullptr;

    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            qDebug() << "[QBulk] Device found VID:" << hex << dev->descriptor.idVendor
                     << "PID:" << dev->descriptor.idProduct;
            if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid) {
                goto found_device;
            }
        }
    }

    qDebug() << "[QBulk] Target device VID:" << hex << vid
             << "PID:" << hex << pid << "not found!";
    return false;

found_device:
    handle = usb_open(dev);
    if (!handle) {
        qDebug() << "[QBulk] Failed to open device!";
        return false;
    }

    if (usb_claim_interface(handle, interfaceNumber) < 0) {
        qDebug() << "[QBulk] Failed to claim interface" << interfaceNumber;
        usb_close(handle);
        handle = nullptr;
        return false;
    }

    qDebug() << "[QBulk] Device opened and interface claimed successfully.";
    return true;
}

void QBulk::closeDevice()
{
    if (handle) {
        usb_release_interface(handle, 0);
        usb_close(handle);
        handle = nullptr;
        qDebug() << "[QBulk] Device closed.";
    }
}

bool QBulk::bulkRead(unsigned char ep, QByteArray &data, unsigned int timeout)
{
    if (!handle) {
        qDebug() << "[QBulk] bulkRead failed: handle is null.";
        return false;
    }

    unsigned char buffer[4096];
    int transferred = usb_bulk_read(handle, ep, reinterpret_cast<char*>(buffer), sizeof(buffer), timeout);
    if (transferred >= 0) {
        data = QByteArray((char*)buffer, transferred);
        qDebug() << "[QBulk] bulkRead success, transferred bytes:" << transferred
                 << "Endpoint:" << hex << int(ep);
        return true;
    } else {
        qDebug() << "[QBulk] bulkRead failed, error:" << transferred
                 << "Endpoint:" << hex << int(ep);
        return false;
    }
}

bool QBulk::bulkWrite(unsigned char ep, const QByteArray &data, unsigned int timeout)
{
    if (!handle) {
        qDebug() << "[QBulk] bulkWrite failed: handle is null.";
        return false;
    }

    int transferred = usb_bulk_write(handle, ep, reinterpret_cast<char*>(const_cast<unsigned char*>(reinterpret_cast<const unsigned char*>(data.data()))),
                                     data.size(), timeout);
    if (transferred >= 0) {
        qDebug() << "[QBulk] bulkWrite success, transferred bytes:" << transferred
                 << "Endpoint:" << hex << int(ep);
        return true;
    } else {
        qDebug() << "[QBulk] bulkWrite failed, error:" << transferred
                 << "Endpoint:" << hex << int(ep);
        return false;
    }
}
