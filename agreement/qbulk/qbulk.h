#ifndef QBULK_H
#define QBULK_H

#include <QByteArray>
#include <cstdint>
#include <lusb0_usb.h>
// extern "C" {
// #include <lusb0_usb.h>   // libusb 0.1 API
// }

class QBulk
{
public:
    QBulk();
    ~QBulk();

    // 打开 USB 设备，vid/pid，指定接口号
    bool openDevice(uint16_t vid, uint16_t pid, int interfaceNumber);

    // 关闭设备
    void closeDevice();

    // Bulk 读取
    bool bulkRead(unsigned char ep, QByteArray &data, unsigned int timeout = 1000);

    // Bulk 写入
    bool bulkWrite(unsigned char ep, const QByteArray &data, unsigned int timeout = 1000);

private:
    usb_dev_handle *handle;
};

#endif // QBULK_H
