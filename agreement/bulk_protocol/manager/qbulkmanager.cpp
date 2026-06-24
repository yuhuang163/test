#include "qbulkmanager.h"
#include <QDebug>
#include <QThread>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QBulkManager::QBulkManager(QObject* parent)
    : QObject(parent), handle(nullptr), is_open(false), running(false), ep_numer(0x01) {
    usb_init();
    device_ = new DjiBulkDevice(this);

    // 日志文本由上层直连 device_->send_bulk_data；readyRead 保留给原始 bulk 字节流（若上层需要）
    connect(device_, &DjiBulkDevice::send_bulk_data, this, [this](const QString& str) { qDebug() << str; });

    // Connect WriteCallback
    device_->setWriteCallback([this](const QByteArray& data) {
        bulkWriteRaw(data);
    });

    qDebug() << "[QBulkManager] libusb 0.1 initialized.";
}

QBulkManager::~QBulkManager() {
    closeDevice();
    stopRead();
    qDebug() << "[QBulkManager] libusb exited.";
}

bool QBulkManager::openDevice(uint16_t vid, uint16_t pid, int interfaceNumber) {
    qDebug() << "[QBulkManager] Scanning USB devices...";
    usb_find_busses();
    usb_find_devices();
    struct usb_bus* bus;
    struct usb_device* dev = nullptr;
    const struct usb_interface_descriptor* ifd = nullptr;

    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {
            uint16_t cvid = dev->descriptor.idVendor;
            uint16_t cpid = dev->descriptor.idProduct;
            UsbVidPid key(cvid, cpid);
            if (!usbDeviceSet.contains(key)) {
                usbDeviceSet.insert(key);
                emit usbDeviceAdded(cvid, cpid);
            }
            if (cvid == vid && cpid == pid) {
                // Simplified checks, normally would check interfaceNumber
                goto found_device;
            }
        }
    }
    return false;

found_device:
    handle = usb_open(dev);
    if (!handle)
        return false;
    if (usb_claim_interface(handle, interfaceNumber) < 0) {
        usb_close(handle);
        handle = nullptr;
        return false;
    }
    is_open = true;
    return true;
}

bool QBulkManager::searchDevice() {
    usb_find_busses();
    usb_find_devices();
    QSet<UsbVidPid> currentDevices;
    struct usb_bus* bus;
    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (struct usb_device* dev = bus->devices; dev; dev = dev->next) {
            currentDevices.insert(UsbVidPid(dev->descriptor.idVendor, dev->descriptor.idProduct));
        }
    }
    usbDeviceSet = currentDevices;
    emit usbDeviceListReady(usbDeviceSet);
    return !usbDeviceSet.isEmpty();
}

void QBulkManager::closeDevice() {
    if (handle) {
        usb_release_interface(handle, 0);
        usb_close(handle);
        handle = nullptr;
        is_open = false;
        emit bulk_device_error(-4, "USB device disconnected");
    }
}

void QBulkManager::stopRead() {
    running = false;
}

void QBulkManager::startRead() {
    running = true;
    QByteArray readBuffer;

    while (running) {
        if (!handle) {
            QThread::msleep(100);
            continue;
        }

        QByteArray data;
        if (bulkRead(0x85, data) && !data.isEmpty()) {
            readBuffer.append(data);
            codec_.feed(readBuffer, [this](const DjiBulkFrame& frame) {
                device_->onFrameReceived(frame);
            });
        }
        QThread::msleep(5);
    }
}

bool QBulkManager::bulkRead(unsigned char ep, QByteArray& data, unsigned int timeout) {
    if (!handle)
        return false;
    unsigned char buffer[4096];
    int transferred = usb_bulk_read(handle, ep, reinterpret_cast<char*>(buffer), sizeof(buffer), timeout);

    if (transferred > 0) {
        data = QByteArray(reinterpret_cast<char*>(buffer), transferred);
        return true;
    }
    if (transferred == 0)
        return false;

    if (transferred == -4 || transferred == -5 || transferred == -116) {
        closeDevice();
    }
    return false;
}

bool QBulkManager::bulkWriteRaw(const QByteArray& data, unsigned int timeout) {
    if (!handle)
        return false;
    int transferred = usb_bulk_write(handle, ep_numer,
                                     reinterpret_cast<char*>(const_cast<unsigned char*>(
                                         reinterpret_cast<const unsigned char*>(data.data()))),
                                     data.size(), timeout);
    if (transferred > 0) {
        return true;
    } else {
        emit reconect();
        is_open = false;
        return false;
    }
}
