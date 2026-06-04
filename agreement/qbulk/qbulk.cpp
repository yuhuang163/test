#include "qbulk.h"
#include <QDebug>
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QBulk::QBulk() : handle(nullptr) {
    usb_init(); // 初始化 libusb 0.1

    registerCommand();
    qDebug() << "[QBulk] libusb 0.1 initialized.";
}
void QBulk::registerCommand() {
    djifactoryCommandList[djiFactroyCmd_get_version] = std::bind(
        &QBulk::process_djiFactroyCmd_get_version, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_factory_mode_handle] = std::bind(
        &QBulk::process_dji_factory_mode_handle, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_amt_task_start] = std::bind(
        &QBulk::process_dji_amt_task_start, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_amt_task_get_result] = std::bind(
        &QBulk::process_dji_amt_task_get_result, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_amt_task_get_log] = std::bind(
        &QBulk::process_dji_amt_task_get_log, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_2a_send_file] =
        std::bind(&QBulk::process_dji_2a_send_file, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_set_date] =
        std::bind(&QBulk::process_dji_set_date, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_get_date] =
        std::bind(&QBulk::process_dji_get_date, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sec_act_command_general] =
        std::bind(&QBulk::process_dji_get_active, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_get_product_status] = std::bind(
        &QBulk::process_dji_get_product_status, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_get_product_dbg_misc_subcmd_count] =
        std::bind(&QBulk::process_get_product_dbg_misc_subcmd_count, this,
                                                                                       std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sec_dbg_command_req_handler] = std::bind(
        &QBulk::process_sec_dbg_command_req_handler, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sec_dbg_command_auth_handler] =
        std::bind(&QBulk::process_sec_dbg_command_auth_handler, this,
                                                                                  std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_anti_rollback_comm] = std::bind(
        &QBulk::process_get_anti_rollback_comm, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_set_sn_operate] =
        std::bind(&QBulk::process_set_sn_operate, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_get_sn_operate] =
        std::bind(&QBulk::process_get_sn_operate, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sys_event_reboot] =
        std::bind(&QBulk::process_sys_event_reboot, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_sys_event_report_status] = std::bind(
        &QBulk::process_sys_event_report_status, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_set_product_status] = std::bind(
        &QBulk::process_set_product_status, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_read_root_key_status] = std::bind(
        &QBulk::process_read_root_key_status, this, std::placeholders::_1);
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

            qDebug() << "[QBulk] Device VID:" << hex << dev->descriptor.idVendor
                     << "PID:" << dev->descriptor.idProduct;

            uint16_t vid = dev->descriptor.idVendor;
            uint16_t pid = dev->descriptor.idProduct;

            UsbVidPid key(vid, pid);

            if (!usbDeviceSet.contains(key)) {
                usbDeviceSet.insert(key);

                emit usbDeviceAdded(vid, pid);

                qDebug().nospace() << "[QBulk] Found new device VID=0x" << hex << vid
                                   << " PID=0x" << pid;
            }
            // 遍历 configuration
            for (int cfg = 0; cfg < dev->descriptor.bNumConfigurations; ++cfg) {
                const struct usb_config_descriptor *config = &dev->config[cfg];

                // 遍历 interface
                for (int ifc = 0; ifc < config->bNumInterfaces; ++ifc) {
                    const struct usb_interface *interface = &config->interface[ifc];

                    // 遍历 altsetting
                    for (int alt = 0; alt < interface->num_altsetting; ++alt) {
                        ifd = &interface->altsetting[alt];

                        qDebug() << "    MI:"
                                 << QString("MI_%1").arg(ifd->bInterfaceNumber, 2, 10,
                                                         QChar('0'))
                                 << "Class:"
                                 << QString("0x%1").arg(ifd->bInterfaceClass, 2, 16,
                                                        QChar('0'))
                                 << "EPs:" << ifd->bNumEndpoints;
                    }
                }
            }

            if (dev->descriptor.idVendor == vid && dev->descriptor.idProduct == pid &&
                ifd->bInterfaceNumber == interfaceNumber) {
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
bool QBulk::searchDevice() {
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

            qDebug() << "[QBulk] Device VID:" << hex << vid << "PID:" << pid;

            UsbVidPid key(vid, pid);
            currentDevices.insert(key);

            // ===== 遍历 interface（保留你原逻辑）=====
            for (int cfg = 0; cfg < dev->descriptor.bNumConfigurations; ++cfg) {
                const struct usb_config_descriptor *config = &dev->config[cfg];

                for (int ifc = 0; ifc < config->bNumInterfaces; ++ifc) {
                    const struct usb_interface *interface = &config->interface[ifc];

                    for (int alt = 0; alt < interface->num_altsetting; ++alt) {
                        ifd = &interface->altsetting[alt];

                        qDebug() << "    MI:"
                                 << QString("MI_%1").arg(ifd->bInterfaceNumber, 2, 10,
                                                         QChar('0'))
                                 << "Class:"
                                 << QString("0x%1").arg(ifd->bInterfaceClass, 2, 16,
                                                        QChar('0'))
                                 << "EPs:" << ifd->bNumEndpoints;
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
        emit bulk_device_error(2, "USB device disconnected");
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
        qDebug().noquote() << "BULK RX:" << QString::fromLatin1(data.toHex(' ').toUpper());
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
        transferred == USB_ERROR_NO_DEVICE || transferred == USB_ERROR_NOT_DATA) {

        qDebug() << "[QBulk] USB device disconnected, releasing resources";

        // 立刻回收资源
        closeDevice();

        // 通知上层（UI / 管理器）

        emit bulk_device_error(transferred, "USB device disconnected");
        is_open = false;
    }

    return false;
}
QByteArray QBulk::buildPacket(uint8_t receiver, uint8_t cmdType, uint8_t cmdSet,
                              uint8_t cmdID, const QByteArray &data,
                              uint8_t encryptionType, uint8_t isResponse) {
    QByteArray packet;

    // 1️⃣ SOF
    packet.append(char(0x55));

    // 2️⃣ length 占 2 字节 (整个包长度，包括 CRC16)
    uint16_t len =
        1 + 2 + 1 + 1 + 1 + 2 + 1 + 1 + 1 + data.size() +
                   2; // sync+ver/len+headcrc+sender/recv+seq+type+cmdset+cmdid+data+CRC16
    // packet.append(char(len & 0xFF));       // length低字节
    // packet.append(char((len >> 8) & 0xFF)); // length高字节

    uint16_t version = 1; // 1 = 使用 V1 格式协议

    uint16_t vl = ((version & 0x3F) << 10) | (len & 0x03FF);

    packet.append(char(vl & 0xFF)); // little-endian
    packet.append(char((vl >> 8) & 0xFF));

    // 3️⃣ Header CRC (CRC-8 over sync + length bytes)
    uint8_t headerCrc =
        crc8_calc(reinterpret_cast<const uint8_t *>(packet.data()), 3,
                                  DUSS_MB_PACKAGE_V1_CRCH_INIT);
    packet.append(char(headerCrc));

    // 4️⃣ sender/receiver 字节
    packet.append(0x0a);
    packet.append(char(receiver));

    uint16_t seq = m_seqNum.fetch_add(1, std::memory_order_relaxed);
    // 5️⃣ sequence number
    // little-endian
    packet.append(char(seq & 0xFF));
    packet.append(char((seq >> 8) & 0xFF));

    uint8_t reserve = 0;
    uint8_t typeByte = 0;
    if (isResponse) {
        // 1000 0000
        cmdType = 0;                            // 回复不需要应答
        typeByte = ((isResponse & 0x01) << 7) | // 数据包的类型
                   ((cmdType & 0x03) << 5) |    // 是否需要应答
                   ((reserve & 0x01) << 4) |
                   (encryptionType & 0x0F); // encription type 加密类型
    } else {

        typeByte = ((isResponse & 0x01) << 7) | // 数据包的类型
                   ((cmdType & 0x03) << 5) |    // 是否需要应答
                   ((reserve & 0x01) << 4) |
                   (encryptionType & 0x0F); // encription type 加密类型
    }

    packet.append(char(typeByte));

    // 7️⃣ cmdSet + cmdID
    packet.append(char(cmdSet));
    packet.append(char(cmdID));

    // 8️⃣ 数据域
    packet.append(data);

    // 9️⃣ CRC16
    uint16_t crc16 =
        duss_util_crc16_calc(reinterpret_cast<const uint8_t *>(packet.data()),
                                          packet.size(), DUSS_MB_PACKAGE_V1_CRC_INIT);
    packet.append(char(crc16 & 0xFF));        // CRC16低字节
    packet.append(char((crc16 >> 8) & 0xFF)); // CRC16高字节

    return packet;
}
bool QBulk::bulkWrite(const QByteArray &data, unsigned int timeout) {
    if (!handle) {
        qDebug() << "[QBulk] bulkWrite failed: handle is null.";
        return false;
    }
    QString hexdata;
    for (uchar b : data) {
        hexdata += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
    }
    hexdata.chop(1); // 去掉最后一个空格

    qDebug().noquote() << "BULK TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    emit send_bulk_data("USB TX:"+hexdata);

    int transferred =
        usb_bulk_write(handle, ep_numer,
                                     reinterpret_cast<char *>(const_cast<unsigned char *>(
                                         reinterpret_cast<const unsigned char *>(data.data()))),
                                     data.size(), timeout);
    if (transferred > 0) {
        // qDebug() << "[QBulk] bulkWrite success, transferred bytes:" <<
        // transferred
        //          << "Endpoint:" << hex << int(ep_numer);
        return true;
    } else {
        qDebug() << "[QBulk] bulkWrite failed, error:" << transferred
                 << "Endpoint:" << hex << int(ep_numer);
        emit reconect();
        is_open = false;
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

void QBulk::get_dev_ver_status() {
    QByteArray v1data;

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x01, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::get_device_date() {
    QByteArray v1data;
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x4b, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::get_product_status() {
    QByteArray v1data;
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xc5, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::get_product_dbg_misc_subcmd_count() {
    QByteArray v1data;
    v1data.append(char(0x02));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xe0, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::get_Esdd_Check_Antirollback() {

    typedef struct {
        uint8_t sec_comm_subcmd;
        uint8_t reserved;
        uint16_t sec_comm_len;
        uint8_t data[0];
    } dji_sec_comm_req_t;

    QByteArray v1data;
    v1data.append(char(0x03));
    v1data.append(char(0x00));
    v1data.append(char(0x06));
    v1data.append(char(0x00));

    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x36, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::get_current_slot() {

    typedef struct {
        uint8_t sec_comm_subcmd;
        uint8_t reserved;
        uint16_t sec_comm_len;
        uint8_t data[0];
    } dji_sec_comm_req_t;

    QByteArray v1data;
    v1data.append(char(0x02));
    v1data.append(char(0x00));
    v1data.append(char(0x03));
    v1data.append(char(0x00));
    // 下面是data
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x03));
    v1data.append(char(0x00));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x36, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::get_product_md5_result() {

    typedef struct {
        uint8_t sec_comm_subcmd;
        uint8_t reserved;
        uint16_t sec_comm_len;
        uint8_t data[0];
    } dji_sec_comm_req_t;

    QByteArray v1data;
    v1data.append(char(0x02));
    v1data.append(char(0x00));
    v1data.append(char(0x03));
    v1data.append(char(0x00));
    // 下面是data
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x04));
    v1data.append(char(0x00));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x36, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::get_active_times() {
    QByteArray v1data;
    v1data.append(char(0x3f));
    v1data.append(char(0x3f));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x01));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0x32, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_wake_wifi() {
    QByteArray v1data;

    v1data.append(char(0x00));

    QByteArray pkt =
    buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0700), 2, 0x07, 0x41, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::get_product_active() {
    QByteArray v1data;
    v1data.append(char(0x31));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0x32, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::set_device_restory_setting() {
    QByteArray v1data;
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x1704), 2, 0x03, 0xf3, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_product_dbg_count() {
    QByteArray v1data;
    // magic_num
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    // auth_id
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xe1, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::set_sec_dbg_auth_req_one_func(const QString &snStr,
                                          const QString &nameStr, uint32_t perm,
                                          uint32_t count, uint32_t time,
                                          uint32_t nonce) {
#pragma pack(push, 1)
    typedef struct {
        uint8_t version;    // 01
        uint8_t resv;       // 00
        uint16_t sn_length; // little-endian
        uint8_t sn[32];     // SN padded
        uint8_t name[32];   // name padded
        uint32_t perm;      // 授权的权限
        uint32_t
            count; // 授权次数。不过使用次数，对QA测试会有一些问题。0xFFFFFFFF可以不限制
        uint32_t time;    // 授权的到期时间
        uint32_t nonce;   // random
        uint8_t cmac[32]; // only first 16 bytes used
        uint8_t sig[64];  // ECDSA signature
    } sec_cmd_dbg_auth_req_t;
#pragma pack(pop)

    static_assert(sizeof(sec_cmd_dbg_auth_req_t) == 180,
                  "sec_cmd_dbg_auth_req_t size mismatch");

    // 1️⃣ init
    sec_cmd_dbg_auth_req_t req;
    memset(&req, 0, sizeof(req));

    // 2️⃣ fixed fields
    req.version = 0x01;
    req.resv = 0x00;

    // 3️⃣ SN
    QByteArray sn = snStr.toLatin1();
    req.sn_length = qMin<int>(sn.size(), sizeof(req.sn));
    memcpy(req.sn, sn.constData(), req.sn_length);

    // 4️⃣ name
    QByteArray name = nameStr.toLatin1();
    memcpy(req.name, name.constData(), qMin<int>(name.size(), sizeof(req.name)));

    // 5️⃣ numeric fields
    req.perm = perm;
    req.count = count;
    req.time = time;
    req.nonce = nonce;

    // 6️⃣ CMAC (placeholder, first 16 bytes valid)
    // TODO: calculate real CMAC
    // 先清零全部 32 字节
    memset(req.cmac, 0x00, sizeof(req.cmac));

    // 抓包里的 CMAC（16 字节）
    static const uint8_t cmac_data[16] = {0x72, 0x7f, 0xd7, 0xf3, 0x0b, 0xd4,
                                          0xb3, 0x34, 0xbd, 0x2d, 0xc4, 0x6a,
                                          0x5c, 0xc2, 0x92, 0x30};

    // 拷贝到 cmac[0..15]
    memcpy(req.cmac, cmac_data, sizeof(cmac_data));

    // 7️⃣ Signature (placeholder)
    // TODO: calculate real ECDSA signature
    // 不需要再 memset，一次 memcpy 就够
    static const uint8_t sig_data[64] = {
                                         0x25, 0x53, 0x70, 0x57, 0x57, 0x00, 0x49, 0x33, 0x52, 0x3f, 0xe5,
        0xfa, 0xd2, 0xf1, 0x39, 0x4d, 0xa1, 0xd1, 0x96, 0xa6, 0x6c, 0x45,
        0x0d, 0xf7, 0x38, 0x95, 0x37, 0x21, 0xa5, 0x84, 0x7c, 0x80, 0xed,
        0xd2, 0x13, 0xba, 0xd6, 0xe2, 0x1f, 0x87, 0x92, 0x4c, 0x61, 0xa2,
        0x17, 0xf1, 0x11, 0xcf, 0x49, 0x37, 0x4b, 0x28, 0x5a, 0x05, 0x8e,
        0xd6, 0x45, 0x3a, 0xeb, 0x97, 0x3a, 0xe4, 0x63, 0xe5};

    memcpy(req.sig, sig_data, sizeof(sig_data));

    // 8️⃣ build QByteArray
    QByteArray v1data;

    // magic / header（如果你协议需要）
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));

    // payload
    v1data.append(reinterpret_cast<const char *>(&req), sizeof(req));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xe2, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

// void QBulk::set_check_one_time_auth (){
//     typedef struct {
//         uint8_t version;//01
//         uint8_t resv;//00
//         uint16_t sn_length;//0a 00
//         uint8_t sn[32];//sn凑满32个
//         uint8_t name[32];//o-joy凑满32个
//         uint32_t perm;//  70 00 00 00
//         uint32_t count;//  32 00 00 00
//         uint32_t time;//ea  07 01 1c
//         uint32_t nonce;//49  1e e8 52
//         uint8_t cmac[32]; /* only the first 16 bytes are used *///72 7f d7 f3
//         0b  d4 b3 34 bd  2d c4 6a 5c  c2 92 30 00 00 00 00 00  00 00 00 00 00
//         00 00 00  00 00 00 uint8_t sig[64];  /* esdsa signature *///25  53 70
//         57 57  00 49 33 52  3f e5 fa d2  f1 39 4d a1 d1 96 a6 6c  45 0d f7 38
//         95 37 21 a5  84 7c 80 ed d2 13 ba d6  e2 1f 87 92  4c 61 a2 17   f1
//         11 cf 49  37 4b 28 5a  05 8e d6 45  3a eb 97 3a  e4 63 e5
//     } __attribute__((packed)) sec_cmd_dbg_auth_req_t;

//     QByteArray v1data;
//     //magic_num
//     v1data.append(char(0x00));
//     v1data.append(char(0x00));
//     v1data.append(char(0x00));
//     v1data.append(char(0x00));
//     //auth_id
//     v1data.append(char(0x00));
//     v1data.append(char(0x00));
//     v1data.append(char(0x00));
//     v1data.append(char(0x00));

//     QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0,
//     0xe2, v1data); bulkWrite( pkt, 1000); // 发到 OUT endpoint
//     0x01，100ms超时
// }

void QBulk::set_device_date() {
    QByteArray v1data;

    QDateTime now = QDateTime::currentDateTime();
    QDate date = now.date();
    QTime time = now.time();

    // year：uint16_t（小端）
    uint16_t year = date.year();
    v1data.append(static_cast<char>(year & 0xFF));
    v1data.append(static_cast<char>((year >> 8) & 0xFF));

    // month / day / hour / minute / second
    v1data.append(static_cast<char>(date.month()));
    v1data.append(static_cast<char>(date.day()));
    v1data.append(static_cast<char>(time.hour()));
    v1data.append(static_cast<char>(time.minute()));
    v1data.append(static_cast<char>(time.second()));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x4a, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::get_Rpmb_Board() {

    QByteArray v1data;
    uint8_t sn_type = 0x01;
    v1data.append(char(sn_type));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x51, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::get_Rpmb_Device() {

    QByteArray v1data;
    uint8_t sn_type = 0x04;
    v1data.append(char(sn_type));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x51, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::get_root_key_status() {

    QByteArray v1data;

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xc6, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_sys_event_reboot() {
    QByteArray v1data;

    v1data.append(char(0x00));

    // DJI_RESET_TYPE_WARM_RESET	0x00	热重启
    // DJI_RESET_TYPE_COLD_RESET	0x01	     冷重启
    // DJI_RESET_TYPE_POWER_OFF	0x02	    关机
    // DJI_RESET_TYPE_OVER_TMEP_PROTECTION	0x03	   过温保护
    // DJI_RESET_TYPE_POWER_ON	0x04	 开机
    // DJI_RESET_TYPE_EMC_RESET	0x05	由于静电或其他合规原因导致的重启
    v1data.append(char(0x01));

    // 重启延迟时间
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));

    //   0x0005	dji_sys异步上报重启结果。
    v1data.append(char(0x00));
    v1data.append(char(0x05));

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x0b, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_Rpmb_Board(const QString &sn) {
    QByteArray snBytes = sn.toUtf8();
    uint16_t snLen = snBytes.size();

    QByteArray v1data;

    uint8_t sn_type = 0x01;
    v1data.append(char(sn_type));

    // ③ sn_length（小端）
    v1data.append(char(snLen & 0xFF));
    v1data.append(char((snLen >> 8) & 0xFF));

    // ④ sn 内容
    v1data.append(snBytes);

    // ❌ 一般 SN 不需要 \0，除非协议要求
    // v1data.append(char(0x00));
    emit send_bulk_data("写入的核心板sn是:" + sn);
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x50, v1data);

    bulkWrite(pkt, 1000);
}

void QBulk::set_Rpmb_Device(const QString &sn) {
    QByteArray snBytes = sn.toUtf8();
    uint16_t snLen = snBytes.size();

    QByteArray v1data;
    uint8_t sn_type = 0x04;
    v1data.append(char(sn_type));

    // ③ sn_length（小端）
    v1data.append(char(snLen & 0xFF));
    v1data.append(char((snLen >> 8) & 0xFF));
    //sn的地址
    v1data.append(char(0x00));
     v1data.append(char(0x10));
       v1data.append(char(0x00));
       v1data.append(char(0xf5));


    // ④ sn 内容
    v1data.append(snBytes);

    // ❌ 一般 SN 不需要 \0，除非协议要求
    // v1data.append(char(0x00));
    emit send_bulk_data("写入的设备sn是:" + sn);
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x50, v1data);

    bulkWrite(pkt, 1000);
}

void QBulk::set_amt_clean_flag() {
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("clear_log");
    v1data.append(char(0x00));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_amt_check_clean_flag() {
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("check_clear_log");

    v1data.append(char(0x00));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_sys_poweroff() { // 0x0a00 set system power_level: 5
    QByteArray v1data;

    v1data.append(char(0x82)); // 01 00 0001 设置为2，表明sysmode消息
    v1data.append(char(0x05)); // s5关机的意思

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x44, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_2a_send_file_data() {
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
        v1data.append(reinterpret_cast<const char *>(&seq), sizeof(uint32_t));

        // ③ payload
        v1data.append(payload);

        // ④ 组包
        QByteArray pkt =
            buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);

        // ⑤ 发送
        bulkWrite(pkt, 1000);

        qDebug() << "[2A] send seq =" << seq << "payload =" << payload.size();

        seq++;
        two_a_can_send = 0;

        while (!two_a_can_send)
            QCoreApplication::processEvents();

        sentBytes += payload.size();
        int progress = int(sentBytes * 100 / fileSize);
        emit send2aprogress(progress);
    }

    file.close();
}

void QBulk::set_2a_send_file_info_check() {
    if (tow_a_filepath.isEmpty()) {
        qWarning() << "[2A] file path is empty";
        return;
    }

    QByteArray v1data;

    // ① 请求类型
    v1data.append(char(0x03)); // 请求接收文件 + 校验

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
            md5_append(&md5, reinterpret_cast<const md5_byte_t *>(chunk.constData()),
                       chunk.size());
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
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);

    bulkWrite(pkt, 1000);
}

void QBulk::set_2a_send_file_info(const QString &filepath) {
    QByteArray v1data;
    tow_a_filepath = filepath;
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
    v1data.append(reinterpret_cast<const char *>(&fileSize), sizeof(fileSize));

    // ④ 文件名（只取名字，不要路径）
    QString fileName = QFileInfo(filepath).fileName();
    QByteArray fileNameBytes = fileName.toUtf8();

    // ⑤ 文件名长度 uint8_t
    quint8 fileNameLen = static_cast<quint8>(fileNameBytes.size());
    v1data.append(char(fileNameLen));

    // ⑥ 文件名本体
    v1data.append(fileNameBytes);

    qDebug() << "[2A] file size  =" << fileSize;

    v1data.append(char(0x00)); // 拓展的
    v1data.append(char(0x01)); // 拓展的
    v1data.append(char(0x01)); // 拓展的

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_amt_task_test(const QString &cmdStr, uint32_t timeout) {

    emit send_bulk_data("执行脚本：" + cmdStr);

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

    set_amt_task_start(cmd, timeout, param);

    waitWork(1000);
    is_running_amt = 1;
    while (is_running_amt) {
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

void QBulk::set_2a_download_path_info(const QString &filepath) {
    QByteArray v1data;
    tow_a_download_filepath = filepath;
    // ① 请求类型
    v1data.append(char(0x06)); // 请求接收文件
    QByteArray fileNameBytes = tow_a_download_filepath.toUtf8();

    quint8 fileNameLen = static_cast<quint8>(tow_a_download_filepath.size());
    v1data.append(char(fileNameLen));

    // ⑥ 文件名本体
    v1data.append(fileNameBytes);

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::set_2a_download_file_info(const QString &filepath) {
    m_totalFileSize = 0; // 文件总大小（字节）
    m_receivedSize = 0;  // 已接收字节数
    lost_list_rsp_seq = 0;
    QByteArray v1data;
    tow_a_download_filepath = filepath;
    QString fileName = QFileInfo(tow_a_download_filepath).fileName();

    // exe 同级目录
    QString basePath = QCoreApplication::applicationDirPath() + "/two_a_data";

    // 当前日期
    QString dateDirName = QDate::currentDate().toString("yyyyMMdd");
    QString timeTag = QDateTime::currentDateTime().toString("_HHmmss");

    // two_a_data/YYYYMMDD
    QDir dir(basePath + "/" + dateDirName + timeTag);

    // 递归创建目录
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "create dir failed:" << dir.absolutePath();
            return;
        }
    }

    tow_a_download_filepath_local = dir.filePath(fileName);

    // ① 请求类型
    v1data.append(char(0x08)); // 请求接收文件
    QByteArray fileNameBytes = tow_a_download_filepath.toUtf8();

    quint8 fileNameLen = static_cast<quint8>(tow_a_download_filepath.size());
    v1data.append(char(fileNameLen));

    // ⑥ 文件名本体
    v1data.append(fileNameBytes);

    v1data.append(char(0x00)); // 拓展的
    v1data.append(char(0x01)); // 拓展的
    v1data.append(char(0x01)); // 拓展的

    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::set_2a_download_file_devide_rsp() {
    QByteArray v1data;

    // ① 请求类型
    v1data.append(char(0x00)); // 请求接收文件

    // // ② 拓展字段
    // v1data.append(char(0x00));
    // v1data.append(char(0x01));
    // v1data.append(char(0x01));

    // ③ pack_size = 980 (0x03D4)
    uint16_t pack_size = 980;
    v1data.append(char(pack_size & 0xFF));        // L
    v1data.append(char((pack_size >> 8) & 0xFF)); // H

    // ④ window_size = 5000 (0x1388)
    uint16_t window_size = 5000;
    v1data.append(char(window_size & 0xFF));        // L
    v1data.append(char((window_size >> 8) & 0xFF)); // H

    // ⑤ checksum_type
    v1data.append(char(0x01)); // 举例：md5

    // ⑥ protocol_type
    v1data.append(char(0x01)); // 举例：2A 协议

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00,
                                 0x2a, v1data, 0, 1);

    bulkWrite(pkt, 1000);
}
void QBulk::set_2a_download_file_devide_lost_list_rsp() {
    QByteArray v1data;

    // ① 请求类型
    v1data.append(char(0x00)); // 请求接收文件

    v1data.append(reinterpret_cast<const char *>(&lost_list_rsp_seq),
                  sizeof(uint32_t));

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00,
                                 0x2a, v1data, 0, 1);

    bulkWrite(pkt, 1000);
}
void QBulk::set_2a_download_file_ok_rsp(QByteArray &f) {
    qDebug() << "==== set_2a_download_file_ok_rsp ====";
    qDebug() << "f.size() =" << f.size();
    qDebug() << "f md5 HEX =" << f.toHex(' ');

    // ① 校验 MD5 长度
    if (f.size() != 16) {
        qWarning() << "[2A] invalid md5 length:" << f.size();
        return;
    }

    if (tow_a_download_filepath.isEmpty()) {
        qWarning() << "[2A] download file path is empty";
        return;
    }

    // ② 打开文件

    QFile file(tow_a_download_filepath_local);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[2A] open file failed:" << tow_a_download_filepath_local;
        return;
    }

    // ③ 计算本地文件 MD5
    md5_state_t md5;
    md5_byte_t digest[16];

    md5_init(&md5);

    while (!file.atEnd()) {
        QByteArray chunk = file.read(4096);
        if (!chunk.isEmpty()) {
            md5_append(&md5, reinterpret_cast<const md5_byte_t *>(chunk.constData()),
                       chunk.size());
        }
    }

    md5_finish(&md5, digest);

    QByteArray localMd5(reinterpret_cast<const char *>(digest), 16);

    qDebug() << "[2A] local file md5 =" << localMd5.toHex();

    // ④ 比对 MD5
    bool md5_ok = (localMd5 == f);

    qDebug() << "[2A] md5 compare result =" << md5_ok;
    if (md5_ok)
        emit send_bulk_data("2a下载md5数据比对成功");
    else
        emit send_bulk_data("2a下载md5数据比对失败");

    // ⑤ 组 rsp 包
    QByteArray v1data;

    /*
   * 建议协议语义：
   * 0x00 = MD5 OK
   * 0x01 = MD5 FAIL
   */
    v1data.append(char(md5_ok ? 0x00 : 0x01));

    QByteArray pkt = buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00,
                                 0x2a, v1data, 0, 1);

    bulkWrite(pkt, 1000);
}

void QBulk::prase_2a_download_file_data(QByteArray &f) {
    // qDebug() << "==== prase_2a_download_file_data ====";
    // qDebug() << "f.size() =" << f.size();
    // qDebug() << "f HEX =" << f.toHex(' ');

    if (f.size() < 4) {
        qWarning() << "data too short, size =" << f.size();
        return;
    }

    // ① 解析前 4 个字节（uint32，小端）
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    lost_list_rsp_seq = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    qDebug() << "lost_list_rsp_seq =" << lost_list_rsp_seq;

    emit send_bulk_data("当前PC端收到的窗口内的最大值" +
                        QString::number(lost_list_rsp_seq));

    // ② 真正的文件内容（从第 4 字节开始）
    QByteArray payload = f.mid(4);

    // qDebug() << "payload size =" << payload.size();
    // qDebug() << "payload HEX =" << payload.toHex(' ');
    // qDebug() << "payload as UTF-8 =" << QString::fromUtf8(payload);

    QFile file(tow_a_download_filepath_local);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        qWarning() << "open file failed:" << tow_a_download_filepath_local
                   << file.errorString();
        return;
    }

    // ⭐ 关键：原始字节写入
    qint64 written = file.write(payload);
    file.close();

    if (written != payload.size()) {
        qWarning() << "[2A] write size mismatch:" << written << "/"
                   << payload.size();
    }

    // ================== ⭐ 关键进度逻辑 ==================
    m_receivedSize += written;

    int progress = 0;
    if (m_totalFileSize > 0) {
        progress = static_cast<int>((m_receivedSize * 100) / m_totalFileSize);
    }

    emit download2aprogress(progress);
}

void QBulk::set_2a_download_file_info_check() { ; }

void QBulk::set_amt_task_start(const QString &cmdStr, uint32_t timeout,
                               const QByteArray &param) {
    QByteArray payload;

    // ---------- cmd：固定 SYS_AMT_TEST_CMD_STR_LEN ----------
    QByteArray cmdBuf(SYS_AMT_TEST_CMD_STR_LEN, 0);
    QByteArray cmdUtf8 = cmdStr.toUtf8();
    uint32_t cmdlen = cmdUtf8.size();

    memcpy(cmdBuf.data(), cmdUtf8.constData(),
           qMin(cmdUtf8.size(), cmdBuf.size()));

    payload.append(cmdBuf); // ✅ 固定长度

    // ---------- cmdid ----------
    uint32_t cmdid = ++m_cmdId;

    // ---------- paralen ----------
    uint16_t paramLen = param.size();

    // ---------- append fields ----------
    payload.append(reinterpret_cast<const char *>(&cmdlen), sizeof(cmdlen));
    payload.append(reinterpret_cast<const char *>(&cmdid), sizeof(cmdid));
    payload.append(reinterpret_cast<const char *>(&timeout), sizeof(timeout));
    payload.append(reinterpret_cast<const char *>(&paramLen), sizeof(paramLen));

    // ---------- param ----------
    if (paramLen > 0) {
        payload.append(param);
    }

    // ---------- build & send ----------
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf4, payload);

    bulkWrite(pkt, 1000);
}

void QBulk::set_amt_task_get_result() {
    QByteArray v1data;
    uint32_t cmdid = m_cmdId; // ✅ 内部自增
    v1data.append(reinterpret_cast<const char *>(&cmdid), sizeof(cmdid));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf6, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::set_amt_task_get_log(uint32_t offset) {
    QByteArray v1data;
    uint32_t cmdid = m_cmdId;    // ✅ 内部自增
    uint16_t metaId = 0xffff;    // 没有用
    uint8_t metaDataType = 0x01; // 没有用
    uint32_t fetchLen = 4096;

    v1data.append(reinterpret_cast<const char *>(&cmdid), sizeof(cmdid));
    // metaid
    v1data.append(reinterpret_cast<const char *>(&metaId), sizeof(metaId));

    // meta_data_type
    v1data.append(reinterpret_cast<const char *>(&metaDataType),
                  sizeof(metaDataType));

    // meta_data_offset
    v1data.append(reinterpret_cast<const char *>(&offset), sizeof(offset));

    // fetchlength
    v1data.append(reinterpret_cast<const char *>(&fetchLen), sizeof(fetchLen));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf8, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_amt_task_rst() {
    QByteArray v1data;
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf7, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}
void QBulk::set_write_product_status() {
    QByteArray v1data;
    // v1data.append(char(0x00));
    // v1data.append(char(0x00));
    QByteArray pkt =
        buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xc4, v1data);
    bulkWrite(pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
}

void QBulk::parseCmd(QByteArray &buffer) {
    const int minHeaderSize = 12; // sync + ver/len + headCRC + sender/receiver +
    // seq + type + cmdSet + cmdID
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
            qDebug() << "[QBulk] Sync byte not found, discard:" << hex
                     << quint8(buf[0]);
            buf.remove(0, 1);
            continue;
        }

        // 2️⃣ 获取 length（低10位）和 version（高6位）
        quint16 verLen = quint8(buf[1]) | (quint8(buf[2]) << 8);
        quint16 length = verLen & 0x03FF;
        quint8 version = (verLen >> 10) & 0x3F;

        if (buf.size() < length) {
            qDebug()
                << "[QBulk] Incomplete packet, waiting for more data, length field:"
                << length << "buffer size:" << buf.size();
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

        if (cmdID == 0X81)
            break;
        if (cmdSet == 0X02)
            break;
        if (cmdID == 0X42)
            break;
        if (cmdID == 0Xf1)
            break;
        Q_UNUSED(reserve);
        Q_UNUSED(headCRC);
        Q_UNUSED(sequenceNum);
        Q_UNUSED(sender);
        Q_UNUSED(receiver);

        // qDebug() << "USB RX:" << hexdata;

        emit send_bulk_data("USB RX:"+hexdata);

        qDebug() << "[QBulk] handlePacket: version=" << version
                 << "isResponse=" << isResponse << "cmdType=" << cmdType
                 << "encryptionType=" << encryptionType << "cmdSet=0x"
                 << QString::number(cmdSet, 16) << "cmdID=0x"
                 << QString::number(cmdID, 16) << "Data=" << QString::fromUtf8(data)
                 << "DataLen=" << data.size() << "\r\n";

        if (cmdSet == 0) {
            auto it = djifactoryCommandList.find(static_cast<djiFactroyCmd>(cmdID));
            if (it != djifactoryCommandList.end()) {
                it->second(data);
            } else {
                qDebug() << QString("factory event not found , cmd id: 0x%1")
                                .arg(cmdID, 2, 16, QChar('0'))
                                .toUpper();
            }
        } else {
            qDebug() << QString(" cmd set: 0x%1 no for us")
                            .arg(cmdSet, 2, 16, QChar('0'))
                            .toUpper();
        }

        // 6️⃣ 发射信号
        // emit cmdReceived(isResponse, cmdType, encryptionType, cmdSet, cmdID,
        // data);
    }

    buffer = buf; // 保留半包
    // qDebug() << "[QBulk] parseCmd exit, remaining buffer size:" <<
    // buffer.size();
}

void QBulk::process_dji_amt_task_start(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];

    emit sendGetDjiResponse(1, info.retCode);
}

static QString amtAckCodeToString(int code) {
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
        return QString("未知返回值 (0x%1)").arg(code, 2, 16, QLatin1Char('0'));
    }
}
QString QBulk::amtTaskResultToString(uint8_t code) {
    switch (code) {
    case AMT_TASK_RESULT_PASS:
        is_running_amt = 0;
        return "执行成功";
    case AMT_TASK_RESULT_ONGOING:
        is_running_amt = 1;
        return "任务进行中";
    case AMT_TASK_RESULT_TIMER_ERROR:
        is_running_amt = 0;
        return "定时器错误";
    case AMT_TASK_RESULT_EXECUTE_SHELL_ERROR:
        is_running_amt = 0;
        return "脚本执行失败";
    case AMT_TASK_RESULT_NOT_FIND_TASK_INDEX:
        is_running_amt = 0;
        return "未找到任务索引";
    case AMT_TASK_RESULT_TIMEOUT:
        is_running_amt = 0;
        return "任务超时";
    case AMT_TASK_RESULT_BEING_STOPPED:
        is_running_amt = 0;
        return "任务被中止";
    case AMT_TASK_RESULT_GENERIC_ERROR:
        is_running_amt = 0;
        return "通用错误";
    default:
        return QString("未知错误 (0x%1)").arg(code, 2, 16, QChar('0')).toUpper();
    }
}

void QBulk::process_dji_amt_task_get_result(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    if (info.retCode == 0) {

        emit send_bulk_data("脚本执行状态：" + amtTaskResultToString(p[1]));
        if (p[1] == AMT_TASK_RESULT_ONGOING)
            ;
        else if (p[1] == AMT_TASK_RESULT_PASS)
            emit sendGetDjiResponse(1, info.retCode);
        else
            emit sendGetDjiResponse(1, info.retCode);
    } else {
        emit sendGetDjiResponse(1, info.retCode);
        emit send_bulk_data("amt执行错误码翻译为：" +
                            amtAckCodeToString(info.retCode));
        emit send_bulk_data("脚本执行状态：" + amtTaskResultToString(p[1]));
    }
}

void QBulk::process_dji_2a_send_file(QByteArray &f) {

    struct DeviceInfo {
        uint8_t retCode;
    };

    DeviceInfo info = {0};

    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    uint8_t retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
    if (f.size() == 1) {
        emit send_bulk_data("收到回应成功状态");
        return; // 正常回应不希望往下跑
    }
    if (f.size() == 5) // 窗口的文件第几包
    {
        uint32_t window_size = static_cast<uint32_t>(p[1]) |
                               (static_cast<uint32_t>(p[2]) << 8) |
                               (static_cast<uint32_t>(p[3]) << 16) |
                               (static_cast<uint32_t>(p[4]) << 24);

        emit send_bulk_data("当前设备端收到的窗口内的最大值" +
                            QString::number(window_size));
        two_a_can_send = 1;
        return;
    }

    if (retCode == 0x01) {
        m_totalFileSize = static_cast<uint32_t>(p[1]) |
                          (static_cast<uint32_t>(p[2]) << 8) |
                          (static_cast<uint32_t>(p[3]) << 16) |
                          (static_cast<uint32_t>(p[4]) << 24);

        set_2a_download_file_devide_rsp();
        return; // 不希望往下跑
    }
    if (retCode == 0x04) {
        QByteArray payload = QByteArray::fromRawData(
            reinterpret_cast<const char *>(p + 1), f.size() - 1);

        prase_2a_download_file_data(payload);
        set_2a_download_file_devide_lost_list_rsp();

        return; // 不希望往下跑
    }
    if (retCode == 0x03) {
        QByteArray payload = QByteArray::fromRawData(
            reinterpret_cast<const char *>(p + 1), f.size() - 1);
        set_2a_download_file_ok_rsp(payload);
        emit send_bulk_data("文件下载完成");
        return; // 不希望往下跑
    }

    // 小端解析
    two_a_pack_size =
        static_cast<uint16_t>(p[1]) | (static_cast<uint16_t>(p[2]) << 8);

    two_a_window_size =
        static_cast<uint16_t>(p[3]) | (static_cast<uint16_t>(p[4]) << 8);

    uint8_t checksum_type = p[5];
    uint8_t protocol_type = p[6];

    // ---- 日志 ----
    qDebug() << "[2A]"
             << "retCode ="
             << QString("0x%1").arg(retCode, 2, 16, QLatin1Char('0'))
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

// test_wakealarm_interrupts.sh  研究看看日志少捞的问题
void QBulk::process_dji_amt_task_get_log(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info = {0};


    if (f.size() < 9) {
        qWarning() << "[AMT] get_log ack too short:" << f.size();
        return;
    }

    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    uint8_t retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    // 2️⃣ actual_log_length (LSB)
    uint32_t actualLen = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);

    // 3️⃣ remainder_log_length
    uint32_t remainLen = p[5] | (p[6] << 8) | (p[7] << 16) | (p[8] << 24);

    // 4️⃣ meta_data
    QByteArray metaData = f.mid(9, actualLen);

    // ---- debug ----
    qDebug().noquote() << "[AMT LOG]"
                       << "retCode=" << retCode << "actualLen=" << actualLen
                       << "remainLen=" << remainLen
                       << "metaData HEX=" << metaData.toHex(' ').toUpper();

    // 如果 metaData 是文本
    if (!metaData.isEmpty()) {
        qDebug().noquote() << "metaData TXT=" << QString::fromUtf8(metaData);
    }
    emit send_bulk_data(QString("执行日志：%1").arg(QString::fromUtf8(metaData)));
    // 5️⃣ 如果还有剩余日志，继续 fetch
    if (remainLen > 0) {
        set_amt_task_get_log(actualLen);
    }
}

void QBulk::process_sys_event_reboot(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t product_status;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}
void QBulk::process_sys_event_report_status(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t product_status;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
    info.product_status = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                          ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("报告内容=%1").arg(info.product_status));
}
void QBulk::process_set_product_status(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}

void QBulk::process_read_root_key_status(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint8_t key_status;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    if (info.retCode == 0) {
    info.key_status = (uint8_t)p[1];

        emit send_bulk_data(QString("key注入情况=%1").arg(info.key_status));
    }
}

void QBulk::process_dji_get_product_status(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t product_status;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    if (info.retCode == 0) {
    info.product_status = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                              ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("切量产状态=%1").arg(info.product_status));
    }
}

void QBulk::process_get_sn_operate(QByteArray &f) {
#pragma pack(push, 1)
    struct DeviceInfo {
        uint8_t retCode;
        uint16_t length; // little-endian
        uint8_t serial_number[0];
        // 飞控地址
        // uint32_t fc_addr;他的boradsn只有10个原因是这个
        // 真实product sn
        // uint8_t product_sn[0];
    };
#pragma pack(pop)

    // 1️⃣ 最小包校验
    if (f.size() < int(sizeof(DeviceInfo))) {
        qWarning() << "SN packet too short";
        return;
    }

    const DeviceInfo *info = reinterpret_cast<const DeviceInfo *>(f.constData());

    // 2️⃣ 端序修正
    uint16_t sn_len = qFromLittleEndian(info->length);

    // 3️⃣ 长度一致性校验
    if (f.size() != int(sizeof(DeviceInfo) + sn_len)) {
        qWarning() << "SN length mismatch:" << sn_len << f.size();
        return;
    }

    // 4️⃣ 取 SN
    const char *sn_ptr =
        reinterpret_cast<const char *>(info) + sizeof(DeviceInfo);

    if (sn_ptr[0] == 0) {
        sn_len = sn_len - 4;
        sn_ptr = sn_ptr + 4;
    }

    QByteArray snData(sn_ptr, sn_len);
    QString sn = QString::fromLatin1(snData);
    // 5️⃣ 正确的返回码逻辑

    emit sendGetDjiResponse(1, info->retCode);

    // 6️⃣ 调试输出
    emit send_bulk_data(QString("长度=%1").arg(sn_len));
    emit send_bulk_data(QString("sn=%1").arg(sn));
}

void QBulk::process_set_sn_operate(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t value;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}
#define CHECK_ERR_NO_SLOT 0x01
#define CHECK_ERR_UNRD_OPEN 0xf0
#define CHECK_ERR_UNRD_ALLOC 0xf1
#define CHECK_ERR_UNRD_GET 0xf2
#define CHECK_ERR_POPEN 0xf3
#define CHECK_ERR_EXEC 0xf4
#define CHECK_ERR_LENGTH 0xf5
#define CHECK_ERR_MISMATCH 0xf6
#define CHECK_ERR_MEM_CALLOC 0xf7
static QString checkErrToString(uint8_t err) {
    switch (err) {
    case CHECK_ERR_NO_SLOT:
        return "无可用 slot";
    case CHECK_ERR_UNRD_OPEN:
        return "UNRD 打开失败";
    case CHECK_ERR_UNRD_ALLOC:
        return "UNRD 内存分配失败";
    case CHECK_ERR_UNRD_GET:
        return "UNRD 获取数据失败，做一下大包升级";
    case CHECK_ERR_POPEN:
        return "popen 执行失败";
    case CHECK_ERR_EXEC:
        return "命令执行失败";
    case CHECK_ERR_LENGTH:
        return "数据长度错误";
    case CHECK_ERR_MISMATCH:
        return "数据校验不匹配";
    case CHECK_ERR_MEM_CALLOC:
        return "内存 calloc 失败";
    default:
        return QString("未知错误(0x%1)").arg(err, 2, 16, QLatin1Char('0'));
    }
}

void QBulk::process_get_anti_rollback_comm(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t value;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    if (f.size() == 2) {
        emit send_bulk_data(QString("当前所在槽=%1").arg(p[1]));
        return;
    }
    if (f.size() == 3) {

        emit send_bulk_data(
            QString("ab分区校验失败值分别为=%1 %2").arg(p[1]).arg(p[2]));
        if (info.retCode == 0x00)
            emit send_bulk_data("ab分区校验成功");
        else
            emit send_bulk_data(QString("AB 分区校验失败："
                                        "A[%1: %2], B[%3: %4]")
                                    .arg(p[1])
                                    .arg(checkErrToString(p[1]))
                                    .arg(p[2])
                                    .arg(checkErrToString(p[2])));

        return;
    }
    info.value = (uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) |
                 ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("返回滚的value=%1").arg(info.value));
}

void QBulk::process_sec_dbg_command_auth_handler(QByteArray &f) {
#pragma pack(push, 1)
    struct DeviceInfo {
        uint8_t retCode; // 0
    };
#pragma pack(pop)

    if (f.size() < sizeof(DeviceInfo)) {
        qWarning() << "sec dbg response too short:" << f.size()
                   << "need:" << sizeof(DeviceInfo);
        return;
    }

    DeviceInfo info;
    memcpy(&info, f.constData(), sizeof(DeviceInfo));

    emit sendGetDjiResponse(1, info.retCode);
}

void QBulk::process_sec_dbg_command_req_handler(QByteArray &f) {
#pragma pack(push, 1)
    struct DeviceInfo {
        uint8_t retCode;    // 0
        uint16_t sn_length; // 1~2  小端
        uint8_t sn[32];     // 3~34
        uint32_t nonce;     // 35~38
    };
#pragma pack(pop)

    if (f.size() < sizeof(DeviceInfo)) {
        qWarning() << "sec dbg response too short:" << f.size()
                   << "need:" << sizeof(DeviceInfo);
        return;
    }

    DeviceInfo info;
    memcpy(&info, f.constData(), sizeof(DeviceInfo));

    // 1️⃣ retCode
    emit sendGetDjiResponse(1, info.retCode);

    // 2️⃣ sn_length（USB 协议默认小端，Qt / x86 直接可用）
    uint16_t snLen = info.sn_length;
    if (snLen > sizeof(info.sn))
        snLen = sizeof(info.sn);

    // 3️⃣ sn 转 QString（假设是 ASCII）
    QString snStr =
        QString::fromLatin1(reinterpret_cast<const char *>(info.sn), snLen);

    emit send_bulk_data(QStringLiteral("SN=%1").arg(snStr));

    // // 4️⃣ nonce（如果你后面要用）
    // qDebug() << "nonce =" << Qt::hex << info.nonce;

    set_sec_dbg_auth_req_one_func(snStr, "o-joy", 0x70, 0x32, 0x1c0107ea,
                                  info.nonce);
}

void QBulk::process_get_product_dbg_misc_subcmd_count(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t left_count;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
    info.left_count = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
                      ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("安全调试模式次数=%1").arg(info.left_count));
}

void QBulk::process_dji_get_active(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t left_count;
    };
    DeviceInfo info;

    struct DjiActivationTime {
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    };
#pragma pack(push, 1)
    typedef struct {
        uint8_t retCode;
        uint8_t total_trial_times;
        uint8_t used_trial_times;
        uint8_t trial_state; /* 0: not in trial 1: is in trial  */
    } sec_cmd_act_trial_times_ack_t;
#pragma pack(pop)

    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    if (p[1] == 5) {
        sec_cmd_act_trial_times_ack_t times_info;
        memcpy(&times_info, f.constData(), sizeof(sec_cmd_act_trial_times_ack_t));

        emit send_bulk_data(
            QString("试用总次数=%1").arg(times_info.total_trial_times));
        emit send_bulk_data(
            QString("已试用次数=%1").arg(times_info.used_trial_times));
        return;
    }

    int len = f.size();
    int offset = 0;
    info.retCode = p[0];
    // 1️⃣ retCode
    if (len < 1)
        return;
    uint8_t retCode = p[offset++];
    if (retCode == 0)
        emit sendGetDjiResponse(1, info.retCode);
    else {
        emit sendGetDjiResponse(1, info.retCode);
        return;
    }

    // 2️⃣ reserved(6) + activate_state(2)
    if (offset + 1 > len)
        return;
    uint8_t state_byte = p[offset++];

    uint8_t activate_state = state_byte & 0x03; // bit[1:0]
    uint8_t reserved = state_byte >> 2;         // bit[7:2]

    qDebug() << "activate_state =" << activate_state;
    emit send_bulk_data(QString("激活状态值=%1").arg(activate_state));

    qDebug() << "reserved =" << reserved;

    // 3️⃣ activation_time（7 字节）
    if (offset + 7 > len)
        return;

    DjiActivationTime time;
    time.year = (uint16_t)p[offset] | ((uint16_t)p[offset + 1] << 8);
    offset += 2;

    time.month = p[offset++];
    time.day = p[offset++];
    time.hour = p[offset++];
    time.minute = p[offset++];
    time.second = p[offset++];

    qDebug() << "activation_time ="
             << QString("%1-%2-%3 %4:%5:%6")
                    .arg(time.year)
                    .arg(time.month, 2, 10, QChar('0'))
                    .arg(time.day, 2, 10, QChar('0'))
                    .arg(time.hour, 2, 10, QChar('0'))
                    .arg(time.minute, 2, 10, QChar('0'))
                    .arg(time.second, 2, 10, QChar('0'));

    // 4️⃣ board_num_length
    if (offset + 1 > len)
        return;
    uint8_t board_num_length = p[offset++];

    // 5️⃣ board_num（变长）
    if (offset + board_num_length > len)
        return;
    QByteArray board_num(reinterpret_cast<const char *>(p + offset),
                         board_num_length);
    offset += board_num_length;

    qDebug() << "board_num HEX =" << board_num.toHex(' ');
    qDebug() << "board_num STR =" << QString::fromLatin1(board_num);
}

void QBulk::process_dji_set_date(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}
void QBulk::process_dji_get_date(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint16_t year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;
    };
    if (f.size() < 8) {
        qWarning() << "get_date frame too short:" << f.size();
        return;
    }

    DeviceInfo info{};

    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    int idx = 0;

    // 1️⃣ retCode
    info.retCode = p[idx++];

    // 2️⃣ year（uint16_t，小端）
    info.year = p[idx] | (p[idx + 1] << 8);
    idx += 2;

    // 3️⃣ time fields
    info.month = p[idx++];
    info.day = p[idx++];
    info.hour = p[idx++];
    info.minute = p[idx++];
    info.second = p[idx++];

    // ===== 业务处理 =====
    emit sendGetDjiResponse(1, info.retCode);

    // ===== 格式化日期时间 =====
    QString dateStr = QString("%1-%2-%3 %4:%5:%6")
                          .arg(info.year, 4, 10, QLatin1Char('0'))
                          .arg(info.month, 2, 10, QLatin1Char('0'))
                          .arg(info.day, 2, 10, QLatin1Char('0'))
                          .arg(info.hour, 2, 10, QLatin1Char('0'))
                          .arg(info.minute, 2, 10, QLatin1Char('0'))
                          .arg(info.second, 2, 10, QLatin1Char('0'));

    emit send_bulk_data(QString("日期=%1").arg(dateStr));
}

void QBulk::process_dji_factory_mode_handle(QByteArray &f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];
  emit sendGetDjiResponse(1, info.retCode);
}

void QBulk::process_djiFactroyCmd_get_version(QByteArray &f) {
    struct DeviceVersionInfo {
        uint8_t retCode;

        uint8_t majorVersion;
        uint8_t minorVersion;

        QString hardwareVersion;

        QString loaderVersion;   // "AA.BB.CC.DD"
        QString firmwareVersion; // "AA.BB.CC.DD"

        bool is_mass_product;
        bool is_support_safe_update;
        bool is_not_existed;
    };
    if (f.size() < 26) {
        qWarning() << "[GET_VERSION] data too short:" << f.size();
        return;
    }

    DeviceVersionInfo info;

    const uint8_t *p = reinterpret_cast<const uint8_t *>(f.constData());

    info.retCode = p[0];

    emit sendGetDjiResponse(1, info.retCode);
    // 2️⃣ major / minor version (4bit + 4bit)
    info.majorVersion = (p[1] >> 4) & 0x0F;
    info.minorVersion = p[1] & 0x0F;

    // 3️⃣ hardware_version (CHAR8, 0 结尾)
    info.hardwareVersion =
        QString::fromLatin1(reinterpret_cast<const char *>(p + 2),
                                               strnlen(reinterpret_cast<const char *>(p + 2), 16));

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

    uint32_t flags = (uint32_t)p[26] | ((uint32_t)p[27] << 8) |
                     ((uint32_t)p[28] << 16) | ((uint32_t)p[29] << 24);

    info.is_mass_product = flags & (1u << 31);

    info.is_support_safe_update = flags & (1u << 30);

    info.is_not_existed = flags & (1u << 29);

    // 6️⃣ 打印（干净、不丑）
    qDebug().noquote()
        << "[GET_VERSION]" << QString("ret=%1").arg(info.retCode)
        << QString("proto=%1.%2").arg(info.majorVersion).arg(info.minorVersion)
        << QString("hw=%1").arg(info.hardwareVersion)
        << QString("loader=%1").arg(info.loaderVersion)
        << QString("fw=%1").arg(info.firmwareVersion);

    emit send_bulk_data(QString("版本号=%1").arg(info.firmwareVersion));
    emit send_bulk_data(QString("加密状态=%1").arg(info.is_mass_product));
}
