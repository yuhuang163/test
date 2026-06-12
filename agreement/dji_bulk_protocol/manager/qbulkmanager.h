#ifndef QBULKMANAGER_H
#define QBULKMANAGER_H

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#include <QObject>
#include <QThread>
#include <QSet>
#include "lusb0_usb.h"
#include "../device/dji_bulk_device.h"
#include "../codec/dji_bulk_codec.h"

// For QSet to work with struct or class
struct UsbVidPid {
    uint16_t vid;
    uint16_t pid;
    UsbVidPid() : vid(0), pid(0) {}
    UsbVidPid(uint16_t v, uint16_t p) : vid(v), pid(p) {}
    bool operator==(const UsbVidPid& other) const {
        return vid == other.vid && pid == other.pid;
    }
};

inline uint qHash(const UsbVidPid& key, uint seed = 0) {
    return qHash(key.vid ^ key.pid, seed);
}

class QBulkManager : public QObject {
    Q_OBJECT
public:
    explicit QBulkManager(QObject* parent = nullptr);
    ~QBulkManager();

    bool openDevice(uint16_t vid, uint16_t pid, int interfaceNumber = 0);
    bool searchDevice();
    void closeDevice();

    void stopRead();
    void startRead();
    bool bulkRead(unsigned char ep, QByteArray& data, unsigned int timeout = 500);
    bool bulkWriteRaw(const QByteArray& data, unsigned int timeout = 1000);

    DjiBulkDevice* device() const { return device_; }

signals:
    void readyRead(QByteArray& data);
    void bulk_device_error(int code, const QString& msg);
    void reconect(); // 通知上层重新连接
    void usbDeviceAdded(quint16 vid, quint16 pid);
    void usbDeviceListReady(const QSet<UsbVidPid>& devices);

private:
    struct usb_dev_handle* handle;
    QSet<UsbVidPid> usbDeviceSet;
    bool is_open;
    bool running;
    int ep_numer;

    DjiBulkCodec codec_;
    DjiBulkDevice* device_;
};

#endif // QBULKMANAGER_H
