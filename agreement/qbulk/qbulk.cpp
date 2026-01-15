#include "qbulk.h"
#include <QDebug>
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

QBulk::QBulk() : handle(nullptr) {
    usb_init(); // 初始化 libusb 0.1

 registerCommand();
    qDebug() << "[QBulk] libusb 0.1 initialized.";
}
void QBulk::registerCommand() {
    djifactoryCommandList[djiFactroyCmd_get_version] =
        std::bind(&QBulk::process_djiFactroyCmd_get_version, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_factory_mode_handle] =
        std::bind(&QBulk::process_dji_factory_mode_handle, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_amt_task_start] =
        std::bind(&QBulk::process_dji_amt_task_start, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_amt_task_get_result] =
        std::bind(&QBulk::process_dji_amt_task_get_result, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_amt_task_get_log] =
        std::bind(&QBulk::process_dji_amt_task_get_log, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_2a_send_file] =
        std::bind(&QBulk::process_dji_2a_send_file, this, std::placeholders::_1);


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
    const struct usb_interface_descriptor *ifd;
    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {

            qDebug() << "[QBulk] Device VID:"
                     << hex << dev->descriptor.idVendor
                     << "PID:" << dev->descriptor.idProduct;

            uint16_t vid = dev->descriptor.idVendor;
            uint16_t pid = dev->descriptor.idProduct;

            UsbVidPid key(vid, pid);

            if (!usbDeviceSet.contains(key)) {
                usbDeviceSet.insert(key);

                emit usbDeviceAdded(vid, pid);

                qDebug().nospace()
                    << "[QBulk] Found new device VID=0x"
                    << hex << vid
                    << " PID=0x"
                    << pid;
            }
            // 遍历 configuration
            for (int cfg = 0; cfg < dev->descriptor.bNumConfigurations; ++cfg) {
                const struct usb_config_descriptor *config =
                    &dev->config[cfg];

                // 遍历 interface
                for (int ifc = 0; ifc < config->bNumInterfaces; ++ifc) {
                    const struct usb_interface *interface =
                        &config->interface[ifc];

                    // 遍历 altsetting
                    for (int alt = 0; alt < interface->num_altsetting; ++alt) {
                        ifd =&interface->altsetting[alt];

                        qDebug() << "    MI:"
                                 << QString("MI_%1")
                                        .arg(ifd->bInterfaceNumber, 2, 10, QChar('0'))
                                 << "Class:"
                                 << QString("0x%1")
                                        .arg(ifd->bInterfaceClass, 2, 16, QChar('0'))
                                 << "EPs:"
                                 << ifd->bNumEndpoints;
                    }
                }
            }

            if (dev->descriptor.idVendor == vid &&
                dev->descriptor.idProduct == pid&&
                ifd->bInterfaceNumber==interfaceNumber)
                 {
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
bool QBulk::searchDevice()
{
    qDebug() << "[QBulk] searchDevice USB devices...";

    usb_find_busses();
    usb_find_devices();

    QSet<UsbVidPid> currentDevices;

    struct usb_bus *bus;
    struct usb_device *dev = nullptr;
    const struct usb_interface_descriptor *ifd;

    for (bus = usb_get_busses(); bus; bus = bus->next) {
        for (dev = bus->devices; dev; dev = dev->next) {

            uint16_t vid = dev->descriptor.idVendor;
            uint16_t pid = dev->descriptor.idProduct;

            qDebug() << "[QBulk] Device VID:"
                     << hex << vid
                     << "PID:" << pid;

            UsbVidPid key(vid, pid);
            currentDevices.insert(key);

            // ===== 遍历 interface（保留你原逻辑）=====
            for (int cfg = 0; cfg < dev->descriptor.bNumConfigurations; ++cfg) {
                const struct usb_config_descriptor *config = &dev->config[cfg];

                for (int ifc = 0; ifc < config->bNumInterfaces; ++ifc) {
                    const struct usb_interface *interface =
                        &config->interface[ifc];

                    for (int alt = 0; alt < interface->num_altsetting; ++alt) {
                        ifd = &interface->altsetting[alt];

                        qDebug() << "    MI:"
                                 << QString("MI_%1")
                                        .arg(ifd->bInterfaceNumber, 2, 10, QChar('0'))
                                 << "Class:"
                                 << QString("0x%1")
                                        .arg(ifd->bInterfaceClass, 2, 16, QChar('0'))
                                 << "EPs:"
                                 << ifd->bNumEndpoints;
                    }
                }
            }
        }
    }

    // ✅ 更新内部缓存
    usbDeviceSet = currentDevices;

    // ✅ 一次性通知：当前完整设备列表
    emit usbDeviceListReady(usbDeviceSet);

    return !usbDeviceSet.isEmpty();
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
        transferred == USB_ERROR_NO_DEVICE||
        transferred ==  USB_ERROR_NOT_DATA) {

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
bool QBulk::bulkWrite( const QByteArray &data,
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
        usb_bulk_write(handle, ep_numer,
                                     reinterpret_cast<char *>(const_cast<unsigned char *>(
                                         reinterpret_cast<const unsigned char *>(data.data()))),
                                     data.size(), timeout);
    if (transferred > 0) {
        // qDebug() << "[QBulk] bulkWrite success, transferred bytes:" << transferred
        //          << "Endpoint:" << hex << int(ep_numer);
        return true;
    } else {
        qDebug() << "[QBulk] bulkWrite failed, error:" << transferred
                 << "Endpoint:" << hex << int(ep_numer);
        return false;
    }


}
void QBulk::stopRead() { running = false; }
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
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}


void QBulk::set_amt_clean_flag(){
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("clear_log");
    v1data.append(char(0x00));
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_amt_check_clean_flag(){
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("check_clear_log");

    v1data.append(char(0x00));
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::set_sys_poweroff(){//0x0a00 set system power_level: 5
    QByteArray v1data;

    v1data.append(char(0x82));//01 00 0001 设置为2，表明sysmode消息
    v1data.append(char(0x05));//s5关机的意思

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x44, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::set_2a_send_file_data()
{
    if (tow_a_filepath.isEmpty()) {
        qWarning() << "[2A] file path is empty";
        return;
    }

    if (two_a_pack_size == 0) {
        qWarning() << "[2A] pack_size is 0";
        return;
    }

    QFile file(tow_a_filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[2A] open file failed:" << tow_a_filepath;
        return;
    }

    uint32_t seq = 0;
    qint64 fileSize = file.size();
    qint64 sentBytes = 0;
    while (!file.atEnd()) {

        QByteArray payload = file.read(two_a_pack_size);
        if (payload.isEmpty())
            break;

        QByteArray v1data;

        // ① 请求类型
        v1data.append(char(0x04));

        // ② seq_num（小端）
        v1data.append(reinterpret_cast<const char *>(&seq),
                      sizeof(uint32_t));

        // ③ payload
        v1data.append(payload);

        // ④ 组包
        QByteArray pkt = buildPacket(
            HOST_ID_FROM_16BIT_TO_8BIT(0x0801),
            2,
            0x00,
            0x2a,
            v1data
            );

        // ⑤ 发送
        bulkWrite(pkt, 1000);

        qDebug() << "[2A] send seq =" << seq
                 << "payload =" << payload.size();

        seq++;
        two_a_can_send=0;

        while (!two_a_can_send)
                QCoreApplication::processEvents();

        sentBytes += payload.size();
            int progress = int(sentBytes * 100 / fileSize);
        emit send2aprogress(progress);

}

    file.close();
}


void QBulk::set_2a_send_file_info_check()
{
    if (tow_a_filepath.isEmpty()) {
        qWarning() << "[2A] file path is empty";
        return;
    }

    QByteArray v1data;

    // ① 请求类型
    v1data.append(char(0x03));   // 请求接收文件 + 校验

    // ② 打开文件
    QFile file(tow_a_filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[2A] open file failed:" << tow_a_filepath;
        return;
    }

    // ③ MD5 计算（使用 md5_append）
    md5_state_t md5;
    md5_byte_t digest[16];

    md5_init(&md5);

    while (!file.atEnd()) {
        QByteArray chunk = file.read(4096);
        if (!chunk.isEmpty()) {
            md5_append(
                &md5,
                reinterpret_cast<const md5_byte_t *>(chunk.constData()),
                chunk.size()
                );
        }
    }

    md5_finish(&md5, digest);

    // ④ 添加 md5（uint8_t[16]）
    v1data.append(reinterpret_cast<const char *>(digest), 16);

    // 调试输出
    QByteArray md5Hex(reinterpret_cast<const char *>(digest), 16);
    qDebug() << "[2A] file path =" << tow_a_filepath;
    qDebug() << "[2A] file md5  =" << md5Hex.toHex();

    // ⑤ 组包并发送
    QByteArray pkt = buildPacket(
        HOST_ID_FROM_16BIT_TO_8BIT(0x0801),
        2,
        0x00,
        0x2a,
        v1data
        );

    bulkWrite(pkt, 1000);
}


void QBulk::set_2a_send_file_info(const QString &filepath){
    QByteArray v1data;
     tow_a_filepath=filepath;
    // ① 请求类型
    v1data.append(char(0x01)); // 请求接收文件

    // ② 打开文件
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "open file failed:" << filepath;
        return;
    }

    // ③ 文件大小 uint32_t
    quint32 fileSize = file.size();
    v1data.append(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));

    // ④ 文件名（只取名字，不要路径）
    QString fileName = QFileInfo(filepath).fileName();
    QByteArray fileNameBytes = fileName.toUtf8();

    // ⑤ 文件名长度 uint8_t
    quint8 fileNameLen = static_cast<quint8>(fileNameBytes.size());
    v1data.append(char(fileNameLen));

    // ⑥ 文件名本体
    v1data.append(fileNameBytes);


    qDebug() << "[2A] file size  =" << fileSize;

    v1data.append(char(0x00)); //拓展的
    v1data.append(char(0x01)); //拓展的
    v1data.append(char(0x01)); //拓展的

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::set_amt_task_test(const QString &cmdStr,uint32_t timeout){

    emit send_bulk_data("执行脚本："+cmdStr);

    QString input = cmdStr.trimmed();
    qDebug() << "[ComboBox] raw text =" << input;

    if (input.isEmpty()) {
        qWarning() << "[ComboBox] empty text";
        return;
    }

    // 1️⃣ 按空格分割（支持多个空格）
    QStringList parts =
        input.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    qDebug() << "[ComboBox] split parts =" << parts;

    if (parts.isEmpty()) {
        qWarning() << "[ComboBox] no parts after split";
        return;
    }

    // 2️⃣ 第一个是脚本名
    QString cmd = parts.takeFirst();
    qDebug() << "[ComboBox] cmd =" << cmd;

    // 3️⃣ 剩余的是参数
    QByteArray param;
    if (!parts.isEmpty()) {
        param = parts.join(' ').toUtf8();
    }

    qDebug() << "[ComboBox] param(str) =" << QString::fromUtf8(param);
    qDebug() << "[ComboBox] param(hex) =" << param.toHex(' ');
    qDebug() << "[ComboBox] param len =" << param.size();



    set_amt_task_start(cmd,timeout,param);

    waitWork(1000);
    is_running_amt=1;
    while(is_running_amt)
    {
        set_amt_task_get_result();
        waitWork(1000);

    }
    set_amt_task_get_log(0);
}
void QBulk::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}
void QBulk::set_amt_task_start(const QString &cmdStr,
                               uint32_t timeout,
                               const QByteArray &param)
{
    QByteArray payload;

    // ---------- cmd：固定 SYS_AMT_TEST_CMD_STR_LEN ----------
    QByteArray cmdBuf(SYS_AMT_TEST_CMD_STR_LEN, 0);
    QByteArray cmdUtf8 = cmdStr.toUtf8();
    uint32_t cmdlen = cmdUtf8.size();

    memcpy(cmdBuf.data(),
           cmdUtf8.constData(),
           qMin(cmdUtf8.size(), cmdBuf.size()));

    payload.append(cmdBuf);   // ✅ 固定长度

    // ---------- cmdid ----------
    uint32_t cmdid = ++m_cmdId;

    // ---------- paralen ----------
    uint16_t paramLen = param.size();

    // ---------- append fields ----------
    payload.append(reinterpret_cast<const char*>(&cmdlen), sizeof(cmdlen));
    payload.append(reinterpret_cast<const char*>(&cmdid), sizeof(cmdid));
    payload.append(reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    payload.append(reinterpret_cast<const char*>(&paramLen), sizeof(paramLen));

    // ---------- param ----------
    if (paramLen > 0) {
        payload.append(param);
    }

    // ---------- build & send ----------
    QByteArray pkt = buildPacket(
        HOST_ID_FROM_16BIT_TO_8BIT(0x0803),
        2,
        0,
        0xf4,
        payload
        );

    bulkWrite( pkt, 1000);
}


void QBulk::set_amt_task_get_result(){
    QByteArray v1data;
    uint32_t cmdid    = m_cmdId;   // ✅ 内部自增
    v1data.append(reinterpret_cast<const char*>(&cmdid), sizeof(cmdid));
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf6, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}


void QBulk::set_amt_task_get_log(  uint32_t offset){
    QByteArray v1data;
    uint32_t cmdid    = m_cmdId;   // ✅ 内部自增
    uint16_t metaId=0xffff;// 没有用
    uint8_t  metaDataType=0x01;// 没有用
    uint32_t fetchLen=4096;

    v1data.append(reinterpret_cast<const char*>(&cmdid), sizeof(cmdid));
    // metaid
    v1data.append(reinterpret_cast<const char*>(&metaId), sizeof(metaId));

    // meta_data_type
    v1data.append(reinterpret_cast<const char*>(&metaDataType), sizeof(metaDataType));

    // meta_data_offset
    v1data.append(reinterpret_cast<const char*>(&offset), sizeof(offset));

    // fetchlength
    v1data.append(reinterpret_cast<const char*>(&fetchLen), sizeof(fetchLen));
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf8, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
void QBulk::set_amt_task_rst(){
    QByteArray v1data;
    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf7, v1data);
    bulkWrite( pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时

}
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
        if(cmdSet==0X02)
            break;
         qDebug() << "USB RX:" << hexdata;

        qDebug() << "[QBulk] handlePacket: version=" << version
                 << "isResponse=" << isResponse
                 << "cmdType=" << cmdType
                 << "encryptionType=" << encryptionType
                  << "cmdSet=0x" << QString::number(cmdSet, 16)
                  << "cmdID=0x"  << QString::number(cmdID, 16)
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
            qDebug() << QString(" cmd set: 0x%1 no for us")
            .arg(cmdSet, 2, 16, QChar('0'))
                .toUpper();
        }

        // 6️⃣ 发射信号
        // emit cmdReceived(isResponse, cmdType, encryptionType, cmdSet, cmdID, data);
    }

    buffer = buf; // 保留半包
    // qDebug() << "[QBulk] parseCmd exit, remaining buffer size:" << buffer.size();
}


void QBulk::process_dji_amt_task_start(QByteArray& f){
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(f.constData());
    // 1️⃣ ret_code
    info.retCode = p[0];
    if(info.retCode==0)
        ; // emit sendGetDjiResponse(1);
    else
        emit sendGetDjiResponse(info.retCode);
}


static QString amtAckCodeToString(int code)
{
    switch (static_cast<dji_amt_task_ack_code_e>(code)) {
    case AMT_TASK_COMMON_ACK_SUCCESS:
        return QString("Success");

    case AMT_TASK_COMMON_ACK_NOTSUPPORT:
        return QString("不支持命令");

    case AMT_TASK_COMMON_ACK_FAILURE:
        return QString("执行失败");

    case AMT_TASK_COMMON_ACK_INVALID_STATE:
        return QString("无效的设备状态");

    case AMT_TASK_COMMON_ACK_OUTOFTASK:
        return QString("任务超数量了");

    case AMT_TASK_COMMON_ACK_ILLEGAL:
        return QString("非法参数");

    default:
        return QString("未知返回值 (0x%1)")
            .arg(code, 2, 16, QLatin1Char('0'));
    }
}
 QString QBulk::amtTaskResultToString(uint8_t code)
{
    switch (code) {
    case AMT_TASK_RESULT_PASS:
        is_running_amt=0;
        return "执行成功";
    case AMT_TASK_RESULT_ONGOING:
        is_running_amt=1;
        return "任务进行中";
    case AMT_TASK_RESULT_TIMER_ERROR:
          is_running_amt=0;
        return "定时器错误";
    case AMT_TASK_RESULT_EXECUTE_SHELL_ERROR:
          is_running_amt=0;
        return "脚本执行失败";
    case AMT_TASK_RESULT_NOT_FIND_TASK_INDEX:
          is_running_amt=0;
        return "未找到任务索引";
    case AMT_TASK_RESULT_TIMEOUT:
         is_running_amt=0;
        return "任务超时";
    case AMT_TASK_RESULT_BEING_STOPPED:
          is_running_amt=0;
        return "任务被中止";
    case AMT_TASK_RESULT_GENERIC_ERROR:
          is_running_amt=0;
        return "通用错误";
    default:
        return QString("未知错误 (0x%1)")
            .arg(code, 2, 16, QChar('0')).toUpper();
    }
}

void QBulk::process_dji_amt_task_get_result(QByteArray& f){
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(f.constData());
    // 1️⃣ ret_code
    info.retCode = p[0];
    if(info.retCode==0)
    {

        emit send_bulk_data("脚本执行状态："+amtTaskResultToString(p[1]));
        if(p[1]==AMT_TASK_RESULT_ONGOING)
            ;
        else if(p[1]==AMT_TASK_RESULT_PASS)
            emit sendGetDjiResponse(1);
        else
            emit sendGetDjiResponse(info.retCode);
    }
    else
    {
        emit sendGetDjiResponse(info.retCode);
        emit send_bulk_data("amt执行错误码翻译为："+amtAckCodeToString(info.retCode));
        emit send_bulk_data("脚本执行状态："+amtTaskResultToString(p[1]));
    }


}


void QBulk::process_dji_2a_send_file(QByteArray &f)
{


    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(f.constData());

    uint8_t retCode = p[0];
    if (retCode == 0x00) {
        emit sendGetDjiResponse(1);   // 成功
    } else {
        emit sendGetDjiResponse(retCode);
    }

    if(f.size()==5)
    {
        uint32_t window_size =
            static_cast<uint32_t>(p[1]) |
            (static_cast<uint32_t>(p[2]) << 8) |
            (static_cast<uint32_t>(p[3]) << 16) |
            (static_cast<uint32_t>(p[4]) << 24);
        emit send_bulk_data("当前收到的窗口内的最大值"+QString::number(window_size));
        qDebug() << "当前收到的窗口内的最大值"<<window_size;
        two_a_can_send=1;
        return;
    }
    if(f.size()==1)
    {
        emit send_bulk_data("结束发包");
        qDebug() << "结束发包";
        return;
    }

    // 小端解析
     two_a_pack_size =
        static_cast<uint16_t>(p[1]) |
        (static_cast<uint16_t>(p[2]) << 8);

     two_a_window_size =
        static_cast<uint16_t>(p[3]) |
        (static_cast<uint16_t>(p[4]) << 8);

    uint8_t checksum_type = p[5];
    uint8_t protocol_type = p[6];

    // ---- 日志 ----
    qDebug() << "[2A]"
             << "retCode =" << QString("0x%1").arg(retCode, 2, 16, QLatin1Char('0'))
             << "pack_size =" << two_a_pack_size
             << "window_size =" << two_a_window_size
             << "checksum_type =" << checksum_type
             << "protocol_type =" << protocol_type;



    // checksum_type
    // DJI_FILE_TRANS_CHECKSUM_MD5	0X01	  MD5校验
    // DJI_FILE_TRANS_CHECKSUM_CRC16	0X02	  CRC16校验
    // DJI_FILE_TRANS_CHECKSUM_CRC32	0X03	 CRC32校验

    // protocol_type
    // DJI_TRANS_EVERY_PACK_ACK	0X00	每个包ack
    // DJI_TRANS_LOST_LIST_ACK	0X01	丢包列表ack

    // DJI_DUSS_MB_RET_OK = 0, // 回复OK
    // DJI_DUSS_MB_RET_ACK = 1, // 回复ACK
    // DJI_DUSS_MB_RET_INVALID_CMD = 224, // 命令无效
    // DJI_DUSS_MB_RET_TIMEOUT = 225, // 超时
    // DJI_DUSS_MB_RET_OUT_OF_MEMORY = 226, // 超出内存
    // DJI_DUSS_MB_RET_INVALID_PARAM = 227, // 参数无效
    // DJI_DUSS_MB_RET_INVALID_STATE = 228, // 状态无效
    // DJI_DUSS_MB_RET_TIME_NOT_SYNC = 229, // 时间不同步
    // DJI_DUSS_MB_RET_SET_PARAM_FAILED = 230, // 设置参数失败
    // DJI_DUSS_MB_RET_GET_PARAM_FAILED = 231, // 获取参数失败
    // DJI_DUSS_MB_RET_SDCARD_NOT_INSERTED = 232, // 未插SD卡
    // DJI_DUSS_MB_RET_SDCARD_FULL = 233, // SD卡满
    // DJI_DUSS_MB_RET_SDCARD_ERR = 234, // SD卡错误
    // DJI_DUSS_MB_RET_SENSOR_ERR = 235, // 传感器错误
    // DJI_DUSS_MB_RET_CRITICAL_ERR = 236, // 严重错误
    // DJI_DUSS_MB_RET_PRARM_LEN_TOO_LONG = 237, // 参数长度超长
    // DJI_DUSS_MB_RET_FW_SEQNUM_NOT_IN_ORDER = 240, // 固件序列不在命令中
    // DJI_DUSS_MB_RET_FW_EXCEED_FLASH = 241, // 固件超出flash内存范围
    // DJI_DUSS_MB_RET_FW_CHECK_ERR = 242, // 固件检查失败
    // DJI_DUSS_MB_RET_FW_FLASH_ERASE_ERR = 243, // 固件flash擦除失败
    // DJI_DUSS_MB_RET_FW_FLASH_PROGRAM_ERR = 244, // 固件flash编程失败
    // DJI_DUSS_MB_RET_FW_UPDATE_STATE_ERR = 245, // 固件升级状态错误
    // DJI_DUSS_MB_RET_FW_INVALID_TYPE = 246, // 固件类型无效
    // DJI_DUSS_MB_RET_FW_UPDATE_WAIT_FINISH = 247, // 固件升级等待完成
    // DJI_DUSS_MB_RET_FW_UPDATE_RC_DISCONNECT = 248, // 固件升级遥控器断连
    // DJI_DUSS_MB_RET_FW_UPGRADE_MOTOR_RUNNING = 249, // 固件升级电机运转
    // DJI_DUSS_MB_RET_HARDWARE_ERR = 250, // 硬件错误
    // DJI_DUSS_MB_RET_DEV_BAT_NOT_ENOUGH = 251, // 设备电池电量不足
    // DJI_DUSS_MB_RET_DEV_UAV_DISCONNECT = 252, // 设备飞机失连
    // DJI_DUSS_MB_RET_FW_FLASH_ERASING = 253, // 固件flash擦除中
    // DJI_DUSS_MB_RET_CHECK_CONNECTION_ERR = 254, // 检查连接错误
    // DJI_DUSS_MB_RET_UNSPECIFIED = 255, // 保留
    // ---- 业务处理 ----

}

//test_wakealarm_interrupts.sh  研究看看日志少捞的问题
void QBulk::process_dji_amt_task_get_log(QByteArray &f)
{
    if (f.size() < 9) {
        qWarning() << "[AMT] get_log ack too short:" << f.size();
        return;
    }

    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(f.constData());

    // 1️⃣ ret_code
    uint8_t retCode = p[0];

    if (retCode == 0)
      ;//  emit sendGetDjiResponse(1);
    else
        emit sendGetDjiResponse(retCode);

    // 2️⃣ actual_log_length (LSB)
    uint32_t actualLen =
        p[1] |
        (p[2] << 8) |
        (p[3] << 16) |
        (p[4] << 24);

    // 3️⃣ remainder_log_length
    uint32_t remainLen =
        p[5] |
        (p[6] << 8) |
        (p[7] << 16) |
        (p[8] << 24);

    // 4️⃣ meta_data
    QByteArray metaData = f.mid(9, actualLen);

    // ---- debug ----
    qDebug().noquote()
        << "[AMT LOG]"
        << "retCode=" << retCode
        << "actualLen=" << actualLen
        << "remainLen=" << remainLen
        << "metaData HEX="
        << metaData.toHex(' ').toUpper();

    // 如果 metaData 是文本
    if (!metaData.isEmpty()) {
        qDebug().noquote()
        << "metaData TXT="
        << QString::fromUtf8(metaData);
    }
    emit send_bulk_data(QString("执行日志：%1").arg(QString::fromUtf8(metaData)));
    // 5️⃣ 如果还有剩余日志，继续 fetch
    if (remainLen > 0) {
        set_amt_task_get_log(actualLen);
    }
}

void QBulk::process_dji_factory_mode_handle(QByteArray& f){
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p =
        reinterpret_cast<const uint8_t *>(f.constData());
    // 1️⃣ ret_code
    info.retCode = p[0];
    if(info.retCode==0)
        emit sendGetDjiResponse(1);
    else
        emit sendGetDjiResponse(info.retCode);
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
