#include "qbulk.h"
#include <QDebug>

QBulk::QBulk() : handle(nullptr) {
    usb_init(); // 初始化 libusb 0.1

 registerCommand();
    qDebug() << "[QBulk] libusb 0.1 initialized.";
}
void QBulk::registerCommand() {
    djifactoryCommandList[djiFactroyCmd_get_version] =
        std::bind(&QBulk::process_djiFactroyCmd_get_version, this, std::placeholders::_1);

}
QBulk::~QBulk() {
    closeDevice();
    stopRead();
    qDebug() << "[QBulk] libusb exited.";
}

bool QBulk::openDevice(uint16_t vid, uint16_t pid, int interfaceNumber) {
    qDebug() << "[QBulk] Scanning USB devices...";
    usb_find_busses();
    usb_find_devices();
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

    qDebug() << "[QBulk] Target device VID:" << hex << vid << "PID:" << hex << pid
             << "not found!";
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
    is_open = true;
    qDebug() << "[QBulk] Device opened and interface claimed successfully.";
    return true;
}


void QBulk::closeDevice() {
    if (handle) {
        usb_release_interface(handle, 0);
        usb_close(handle);
        handle = nullptr;
        qDebug() << "[QBulk] Device closed.";
    }

}

static const char *usbErrorToString(int err) {
    switch (err) {
    case USB_ERROR_IO:
        return "I/O error";
    case USB_ERROR_INVALID_PARAM:
        return "Invalid parameter";
    case USB_ERROR_ACCESS:
        return "Access denied (permission)";
    case USB_ERROR_NO_DEVICE:
        return "No such device (disconnected)";
    case USB_ERROR_NOT_FOUND:
        return "Device not found / endpoint invalid";
    case USB_ERROR_NOT_DATA:
        return "NO DATA";
    default:
        return "Unknown USB error";
    }
}

bool QBulk::bulkRead(unsigned char ep, QByteArray &data, unsigned int timeout) {
    if (!handle) {
        qDebug() << "[QBulk] bulkRead: handle is null";
        return false;
    }

    unsigned char buffer[4096];
    int transferred = usb_bulk_read(handle, ep, reinterpret_cast<char *>(buffer),
                                    sizeof(buffer), timeout);

    // ✅ 正常收到数据
    if (transferred > 0) {
        data = QByteArray(reinterpret_cast<char *>(buffer), transferred);
        return true;
    }

    // 🟡 正常无数据（NAK / timeout）
    if (transferred == 0) {
        return false;
    }

    // 🔴 真实错误（<0）
    qDebug().noquote() << "[QBulk] bulkRead failed"
                       << "error:" << transferred << "("
                       << usbErrorToString(transferred) << ")"
                       << "Endpoint: 0x" << hex << int(ep);

    // ⭐ 设备已断开 or 句柄失效
    if (transferred == USB_ERROR_NOT_FOUND ||
        transferred == USB_ERROR_NO_DEVICE) {

        qDebug() << "[QBulk] USB device disconnected, releasing resources";

        // 立刻回收资源
        closeDevice();

        // 通知上层（UI / 管理器）

        emit error(transferred, "USB device disconnected");
        is_open = false;
    }

    return false;
}
QByteArray QBulk::buildPacket( uint8_t receiver,
                             uint8_t cmdType,
                              uint8_t cmdSet, uint8_t cmdID,
                              const QByteArray &data, uint8_t encryptionType)
{
    QByteArray packet;

    // 1️⃣ SOF
    packet.append(char(0x55));

    // 2️⃣ length 占 2 字节 (整个包长度，包括 CRC16)
    uint16_t len = 1 + 2 + 1 + 1 + 1 + 2 + 1 + 1 + 1 + data.size() + 2; // sync+ver/len+headcrc+sender/recv+seq+type+cmdset+cmdid+data+CRC16
    // packet.append(char(len & 0xFF));       // length低字节
    // packet.append(char((len >> 8) & 0xFF)); // length高字节

    uint16_t version = 1;        // 1 = 使用 V1 格式协议

    uint16_t vl =
        ((version & 0x3F) << 10) |
        (len & 0x03FF);

    packet.append(char(vl & 0xFF));        // little-endian
    packet.append(char((vl >> 8) & 0xFF));



    // 3️⃣ Header CRC (CRC-8 over sync + length bytes)
    uint8_t headerCrc = crc8_calc(reinterpret_cast<const uint8_t*>(packet.data()), 3, DUSS_MB_PACKAGE_V1_CRCH_INIT);
    packet.append(char(headerCrc));

    // 4️⃣ sender/receiver 字节
    packet.append(0x0a);
    packet.append(char(receiver));

    uint16_t seq = m_seqNum.fetch_add(1, std::memory_order_relaxed);
        // 5️⃣ sequence number
    // little-endian
    packet.append(char(seq & 0xFF));
    packet.append(char((seq >> 8) & 0xFF));

    uint8_t isResponse=0;
    uint8_t reserve=0;

    uint8_t typeByte =
        ((isResponse     & 0x01) << 7) |//数据包的类型
        ((cmdType        & 0x03) << 5) |//是否需要应答
        ((reserve        & 0x01) << 4) |
        ( encryptionType & 0x0F);//encription type 加密类型

    packet.append(char(typeByte));

    // 7️⃣ cmdSet + cmdID
    packet.append(char(cmdSet));
    packet.append(char(cmdID));

    // 8️⃣ 数据域
    packet.append(data);

    // 9️⃣ CRC16
    uint16_t crc16 = duss_util_crc16_calc(reinterpret_cast<const uint8_t*>(packet.data()), packet.size(), DUSS_MB_PACKAGE_V1_CRC_INIT);
    packet.append(char(crc16 & 0xFF));       // CRC16低字节
    packet.append(char((crc16 >> 8) & 0xFF)); // CRC16高字节

    return packet;
}
bool QBulk::bulkWrite(unsigned char ep, const QByteArray &data,
                      unsigned int timeout) {
    if (!handle) {
        qDebug() << "[QBulk] bulkWrite failed: handle is null.";
        return false;
    }
    QString hexdata;
    for (uchar b : data) {
        hexdata += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
    }
    hexdata.chop(1); // 去掉最后一个空格

    qDebug() << "USB TX:" << hexdata;


    int transferred =
        usb_bulk_write(handle, ep,
                                     reinterpret_cast<char *>(const_cast<unsigned char *>(
                                         reinterpret_cast<const unsigned char *>(data.data()))),
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
void QBulk::startRead() {
    running = true;

    while (running) {
        if (!handle) {
            // 设备断开 → 等待一段时间再尝试重连
            QThread::msleep(100);
            continue;
        }

        QByteArray data;
        if (bulkRead(0x85, data) && !data.isEmpty()) {
            parseCmd(data);
        }

        QThread::msleep(5);
    }
}

void QBulk::get_dev_ver() {
    QByteArray v1data;

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x01, v1data);
    bulkWrite(0x05, pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::set_amt_test(QString data){
    QByteArray v1data;

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 1, v1data);
    bulkWrite(0x05, pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::set_amt_clean_flag(){
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("clear_log", 9);
    v1data.append(char(0x00));
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    bulkWrite(0x05, pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_amt_check_clean_flag(){
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("check_clear_log", 9);
    v1data.append(char(0x00));
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    bulkWrite(0x05, pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::stopRead() { running = false; }
// qbulk.cpp
void QBulk::parseCmd(QByteArray &buffer) {
    const int minHeaderSize = 12; // sync + ver/len + headCRC + sender/receiver + seq + type + cmdSet + cmdID
    const int crcSize = 2;        // CRC16
    QByteArray buf = buffer;

    // qDebug() << "[QBulk] parseCmd start, buffer size:" << buf.size();

    QString hexdata;
    for (uchar b : buf) {
        hexdata += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
    }
    hexdata.chop(1); // 去掉最后一个空格




    while (buf.size() >= minHeaderSize + crcSize) {
        // 1️⃣ 查找 sync byte
        if (quint8(buf[0]) != 0x55) {
            qDebug() << "[QBulk] Sync byte not found, discard:" << hex << quint8(buf[0]);
            buf.remove(0, 1);
            continue;
        }

        // 2️⃣ 获取 length（低10位）和 version（高6位）
        quint16 verLen = quint8(buf[1]) | (quint8(buf[2]) << 8);
        quint16 length = verLen & 0x03FF;
        quint8 version = (verLen >> 10) & 0x3F;

        if (buf.size() < length) {
            qDebug() << "[QBulk] Incomplete packet, waiting for more data, length field:" << length
                     << "buffer size:" << buf.size();
            break;
        }

        QByteArray packet = buf.left(length);
        buf.remove(0, length);

        // 3️⃣ 校验 header CRC
        if (!checkHeaderCRC(packet)) {
            qDebug() << "[QBulk] Header CRC failed, discard packet";
            continue;
        }

        // 4️⃣ 校验数据 CRC
        if (!checkDataCRC(packet)) {
            qDebug() << "[QBulk] Data CRC failed, discard packet";
            continue;
        }

        // 5️⃣ 解析字段
        quint8 headCRC = quint8(packet[3]);
        quint8 sender = quint8(packet[4]);
        quint8 receiver = quint8(packet[5]);
        quint16 sequenceNum = quint8(packet[6]) | (quint8(packet[7]) << 8);
        quint8 typeByte = quint8(packet[8]);

        quint8 isResponse = (typeByte >> 7) & 0x01;
        quint8 cmdType = (typeByte >> 5) & 0x03;
        quint8 reserve = (typeByte >> 4) & 0x01;
        quint8 encryptionType = typeByte & 0x0F;

        quint8 cmdSet = quint8(packet[9]);
        quint8 cmdID = quint8(packet[10]);
        QByteArray data = packet.mid(11, packet.size() - 11 - crcSize);

        if(cmdID==0X81)
            break;
         qDebug() << "USB RX:" << hexdata;

        qDebug() << "[QBulk] handlePacket: version=" << version
                 << "isResponse=" << isResponse
                 << "cmdType=" << cmdType
                 << "encryptionType=" << encryptionType
                 << "cmdSet=" << cmdSet
                 << "cmdID=" << cmdID
                 << "Data=" << QString::fromUtf8(data)
                 << "DataLen=" << data.size();

        if(cmdSet==0){
        auto it = djifactoryCommandList.find(static_cast<djiFactroyCmd>(cmdID));
        if (it != djifactoryCommandList.end()) {
            it->second(data);
        } else {
            qDebug() << QString("factory event not found , cmd id: 0x%1")
            .arg(cmdID, 2, 16, QChar('0'))
                .toUpper();

        }}else{
            qDebug() << QString(" cmd id: 0x%1 no for us")
            .arg(cmdSet, 2, 16, QChar('0'))
                .toUpper();
        }

        // 6️⃣ 发射信号
        // emit cmdReceived(isResponse, cmdType, encryptionType, cmdSet, cmdID, data);
    }

    buffer = buf; // 保留半包
    // qDebug() << "[QBulk] parseCmd exit, remaining buffer size:" << buffer.size();
}
static const uint8_t crc8_table[256] = {
    0x00, 0x5e, 0xbc, 0xe2, 0x61, 0x3f, 0xdd, 0x83, 0xc2, 0x9c, 0x7e, 0x20, 0xa3, 0xfd, 0x1f, 0x41,
    0x9d, 0xc3, 0x21, 0x7f, 0xfc, 0xa2, 0x40, 0x1e, 0x5f, 0x01, 0xe3, 0xbd, 0x3e, 0x60, 0x82, 0xdc,
    0x23, 0x7d, 0x9f, 0xc1, 0x42, 0x1c, 0xfe, 0xa0, 0xe1, 0xbf, 0x5d, 0x03, 0x80, 0xde, 0x3c, 0x62,
    0xbe, 0xe0, 0x02, 0x5c, 0xdf, 0x81, 0x63, 0x3d, 0x7c, 0x22, 0xc0, 0x9e, 0x1d, 0x43, 0xa1, 0xff,
    0x46, 0x18, 0xfa, 0xa4, 0x27, 0x79, 0x9b, 0xc5, 0x84, 0xda, 0x38, 0x66, 0xe5, 0xbb, 0x59, 0x07,
    0xdb, 0x85, 0x67, 0x39, 0xba, 0xe4, 0x06, 0x58, 0x19, 0x47, 0xa5, 0xfb, 0x78, 0x26, 0xc4, 0x9a,
    0x65, 0x3b, 0xd9, 0x87, 0x04, 0x5a, 0xb8, 0xe6, 0xa7, 0xf9, 0x1b, 0x45, 0xc6, 0x98, 0x7a, 0x24,
    0xf8, 0xa6, 0x44, 0x1a, 0x99, 0xc7, 0x25, 0x7b, 0x3a, 0x64, 0x86, 0xd8, 0x5b, 0x05, 0xe7, 0xb9,
    0x8c, 0xd2, 0x30, 0x6e, 0xed, 0xb3, 0x51, 0x0f, 0x4e, 0x10, 0xf2, 0xac, 0x2f, 0x71, 0x93, 0xcd,
    0x11, 0x4f, 0xad, 0xf3, 0x70, 0x2e, 0xcc, 0x92, 0xd3, 0x8d, 0x6f, 0x31, 0xb2, 0xec, 0x0e, 0x50,
    0xaf, 0xf1, 0x13, 0x4d, 0xce, 0x90, 0x72, 0x2c, 0x6d, 0x33, 0xd1, 0x8f, 0x0c, 0x52, 0xb0, 0xee,
    0x32, 0x6c, 0x8e, 0xd0, 0x53, 0x0d, 0xef, 0xb1, 0xf0, 0xae, 0x4c, 0x12, 0x91, 0xcf, 0x2d, 0x73,
    0xca, 0x94, 0x76, 0x28, 0xab, 0xf5, 0x17, 0x49, 0x08, 0x56, 0xb4, 0xea, 0x69, 0x37, 0xd5, 0x8b,
    0x57, 0x09, 0xeb, 0xb5, 0x36, 0x68, 0x8a, 0xd4, 0x95, 0xcb, 0x29, 0x77, 0xf4, 0xaa, 0x48, 0x16,
    0xe9, 0xb7, 0x55, 0x0b, 0x88, 0xd6, 0x34, 0x6a, 0x2b, 0x75, 0x97, 0xc9, 0x4a, 0x14, 0xf6, 0xa8,
    0x74, 0x2a, 0xc8, 0x96, 0x15, 0x4b, 0xa9, 0xf7, 0xb6, 0xe8, 0x0a, 0x54, 0xd7, 0x89, 0x6b, 0x35,
};

uint8_t QBulk::crc8_calc(const uint8_t *data, uint32_t len, uint8_t init_crc) {
    uint8_t crc = init_crc;
    while (len--) {
        uint8_t byte = *data++ ^ crc;
        crc = crc8_table[byte];
    }
    return crc;
}
bool QBulk::checkHeaderCRC(const QByteArray &packet) {
    const int headerCrcIndex = 3; // CRC-8 在 packet[3]
    if (packet.size() < 4) return false; // 包太短，无法校验

    quint8 crcInPacket = quint8(packet[headerCrcIndex]);

    // 计算 CRC-8，范围是 sync + Ver/Len 字节，也就是 packet[0..2]
    const uint8_t *data = reinterpret_cast<const uint8_t *>(packet.constData());
    uint32_t dataLen = 3; // 包头前三字节
    uint8_t crcCalc = crc8_calc(data, dataLen, DUSS_MB_PACKAGE_V1_CRCH_INIT); // 初始值按协议设定

    if (crcCalc != crcInPacket) {
        qDebug() << "[QBulk] Header CRC mismatch, calculated:" << hex << int(crcCalc)
        << "expected:" << hex << int(crcInPacket);
        return false;
    }

    return true;
}
static const uint16_t crc16_table[256] = {
    0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
    0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
    0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
    0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
    0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
    0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
    0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
    0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
    0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
    0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
    0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
    0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
    0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
    0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
    0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
    0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
    0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
    0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
    0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
    0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
    0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
    0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
    0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
    0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
    0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
    0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
    0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
    0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
    0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
    0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
    0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
    0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78,
};

uint16_t QBulk::duss_util_crc16_calc(const uint8_t *data, uint32_t len, uint16_t init_crc) {
    const uint8_t BITS_OF_BYTE = 8;
    const uint16_t MASK_BIT_BYTE = 0xFF;

    while (len--) {
        uint8_t byte = *data++;
        init_crc = ((init_crc >> BITS_OF_BYTE) & MASK_BIT_BYTE) ^ crc16_table[(uint8_t)init_crc ^ byte];
    }

    return init_crc;
}

bool QBulk::checkDataCRC(const QByteArray &packet) {
    const int crcSize = 2;
    if (packet.size() < crcSize) return false;

    // 包尾的 CRC-16
    quint16 crcInPacket = quint8(packet[packet.size() - 2]) | (quint8(packet[packet.size() - 1]) << 8);

    // 数据区：从 sync (0) 到 数据末尾（不包含包尾 CRC16）
    const uint8_t *data = reinterpret_cast<const uint8_t *>(packet.constData());
    uint32_t dataLen = packet.size() - crcSize;

    // 计算 CRC16
    uint16_t crcCalc = duss_util_crc16_calc(data, dataLen, DUSS_MB_PACKAGE_V1_CRC_INIT); // 初始值可根据协议调整

    if (crcCalc != crcInPacket) {
        qDebug() << "[QBulk] CRC16 mismatch, calculated:" << hex << crcCalc
                 << "expected:" << hex << crcInPacket;
        return false;
    }

    return true;
}
void QBulk::process_djiFactroyCmd_get_version(QByteArray& f){
    struct DeviceVersionInfo {
        uint8_t retCode;

        uint8_t majorVersion;
        uint8_t minorVersion;

        QString hardwareVersion;

        QString loaderVersion;    // "AA.BB.CC.DD"
        QString firmwareVersion;  // "AA.BB.CC.DD"
    };
    if (f.size() < 26) {
        qWarning() << "[GET_VERSION] data too short:" << f.size();
        return;
    }

    DeviceVersionInfo info;

    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(f.constData());

    // 1️⃣ ret_code
    info.retCode = p[0];

    if(info.retCode==0)
            emit sendGetDjiResponse(1);
    else
        emit sendGetDjiResponse(-1);
    // 2️⃣ major / minor version (4bit + 4bit)
    info.majorVersion = (p[1] >> 4) & 0x0F;
    info.minorVersion =  p[1]       & 0x0F;

    // 3️⃣ hardware_version (CHAR8, 0 结尾)
    info.hardwareVersion = QString::fromLatin1(
        reinterpret_cast<const char *>(p + 2),
        strnlen(reinterpret_cast<const char *>(p + 2), 16)
        );

    // 4️⃣ loader_version (LSB → MSB)
    info.loaderVersion = QString("%1.%2.%3.%4")
                             .arg(p[21], 2, 10, QChar('0'))  // AA
                             .arg(p[20], 2, 10, QChar('0'))  // BB
                             .arg(p[19], 2, 10, QChar('0'))  // CC
                             .arg(p[18], 2, 10, QChar('0')); // DD

    // 5️⃣ firmware_version (LSB → MSB)
    info.firmwareVersion = QString("%1.%2.%3.%4")
                               .arg(p[25], 2, 10, QChar('0'))
                               .arg(p[24], 2, 10, QChar('0'))
                               .arg(p[23], 2, 10, QChar('0'))
                               .arg(p[22], 2, 10, QChar('0'));



    // 6️⃣ 打印（干净、不丑）
    qDebug().noquote()
        << "[GET_VERSION]"
        << QString("ret=%1").arg(info.retCode)
        << QString("proto=%1.%2")
               .arg(info.majorVersion)
               .arg(info.minorVersion)
        << QString("hw=%1").arg(info.hardwareVersion)
        << QString("loader=%1").arg(info.loaderVersion)
        << QString("fw=%1").arg(info.firmwareVersion);

     emit send_bulk_data(QString("版本号=%1").arg(info.firmwareVersion));



}
