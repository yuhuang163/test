#include "bulk_device.h"
#include <QDebug>
#include <QDateTime>
#include <QThread>

DjiBulkDevice::DjiBulkDevice(QObject* parent) : QObject(parent) {
    registerCommand();
}
DjiBulkDevice::~DjiBulkDevice() {
}
void DjiBulkDevice::onFrameReceived(const DjiBulkFrame& frame) {
    auto it = djifactoryCommandList.find(static_cast<djiFactroyCmd>(frame.cmdID));
    if (it != djifactoryCommandList.end()) {
        QByteArray dataCopy = frame.data;
        it->second(dataCopy);
    }
}
void DjiBulkDevice::registerCommand() {
    djifactoryCommandList[djiFactroyCmd_get_version] = std::bind(
        &DjiBulkDevice::process_djiFactroyCmd_get_version, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_factory_mode_handle] = std::bind(
        &DjiBulkDevice::process_dji_factory_mode_handle, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_amt_task_start] = std::bind(
        &DjiBulkDevice::process_dji_amt_task_start, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_amt_task_get_result] = std::bind(
        &DjiBulkDevice::process_dji_amt_task_get_result, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_amt_task_get_log] = std::bind(
        &DjiBulkDevice::process_dji_amt_task_get_log, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_2a_send_file] =
        std::bind(&DjiBulkDevice::process_dji_2a_send_file, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_set_date] =
        std::bind(&DjiBulkDevice::process_dji_set_date, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_get_date] =
        std::bind(&DjiBulkDevice::process_dji_get_date, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sec_act_command_general] =
        std::bind(&DjiBulkDevice::process_dji_get_active, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_get_product_status] = std::bind(
        &DjiBulkDevice::process_dji_get_product_status, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_get_product_dbg_misc_subcmd_count] =
        std::bind(&DjiBulkDevice::process_get_product_dbg_misc_subcmd_count, this,
                  std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sec_dbg_command_req_handler] = std::bind(
        &DjiBulkDevice::process_sec_dbg_command_req_handler, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sec_dbg_command_auth_handler] =
        std::bind(&DjiBulkDevice::process_sec_dbg_command_auth_handler, this,
                  std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_anti_rollback_comm] = std::bind(
        &DjiBulkDevice::process_get_anti_rollback_comm, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_set_sn_operate] =
        std::bind(&DjiBulkDevice::process_set_sn_operate, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_get_sn_operate] =
        std::bind(&DjiBulkDevice::process_get_sn_operate, this, std::placeholders::_1);

    djifactoryCommandList[djiFactroyCmd_sys_event_reboot] =
        std::bind(&DjiBulkDevice::process_sys_event_reboot, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_sys_event_report_status] = std::bind(
        &DjiBulkDevice::process_sys_event_report_status, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_set_product_status] = std::bind(
        &DjiBulkDevice::process_set_product_status, this, std::placeholders::_1);
    djifactoryCommandList[djiFactroyCmd_read_root_key_status] = std::bind(
        &DjiBulkDevice::process_read_root_key_status, this, std::placeholders::_1);
}

void DjiBulkDevice::get_dev_ver_status() {
    QByteArray v1data;

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x01, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::get_device_date() {
    QByteArray v1data;
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x4b, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::get_product_status() {
    QByteArray v1data;
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xc5, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::get_product_dbg_misc_subcmd_count() {
    QByteArray v1data;
    v1data.append(char(0x02));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xe0, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::get_Esdd_Check_Antirollback() {

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
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x36, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::get_current_slot() {

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
    // 涓嬮潰鏄痙ata
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x03));
    v1data.append(char(0x00));

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x36, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::get_product_md5_result() {

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
    // 涓嬮潰鏄痙ata
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x04));
    v1data.append(char(0x00));

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x36, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::get_active_times() {
    QByteArray v1data;
    v1data.append(char(0x3f));
    v1data.append(char(0x3f));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x01));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0x32, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_wake_wifi() {
    QByteArray v1data;

    v1data.append(char(0x00));

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0700), 2, 0x07, 0x41, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::get_product_active() {
    QByteArray v1data;
    v1data.append(char(0x31));

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0x32, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::set_device_restory_setting() {
    QByteArray v1data;
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x1704), 2, 0x03, 0xf3, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_product_dbg_count() {
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
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xe1, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::set_sec_dbg_auth_req_one_func(const QString& snStr,
                                                  const QString& nameStr, uint32_t perm,
                                                  uint32_t count, uint32_t time,
                                                  uint32_t nonce) {
#pragma pack(push, 1)
    typedef struct {
        uint8_t version;    // 01
        uint8_t resv;       // 00
        uint16_t sn_length; // little-endian
        uint8_t sn[32];     // SN padded
        uint8_t name[32];   // name padded
        uint32_t perm;      // 鎺堟潈鐨勬潈闄?
        uint32_t
            count;        // 鎺堟潈娆℃暟銆備笉杩囦娇鐢ㄦ鏁帮紝瀵筈A娴嬭瘯浼氭湁涓€浜涢棶棰樸€?xFFFFFFFF鍙互涓嶉檺鍒?
        uint32_t time;    // 鎺堟潈鐨勫埌鏈熸椂闂?
        uint32_t nonce;   // random
        uint8_t cmac[32]; // only first 16 bytes used
        uint8_t sig[64];  // ECDSA signature
    } sec_cmd_dbg_auth_req_t;
#pragma pack(pop)

    static_assert(sizeof(sec_cmd_dbg_auth_req_t) == 180,
                  "sec_cmd_dbg_auth_req_t size mismatch");

    // 1锔忊儯 init
    sec_cmd_dbg_auth_req_t req;
    memset(&req, 0, sizeof(req));

    // 2锔忊儯 fixed fields
    req.version = 0x01;
    req.resv = 0x00;

    // 3锔忊儯 SN
    QByteArray sn = snStr.toLatin1();
    req.sn_length = qMin<int>(sn.size(), sizeof(req.sn));
    memcpy(req.sn, sn.constData(), req.sn_length);

    // 4锔忊儯 name
    QByteArray name = nameStr.toLatin1();
    memcpy(req.name, name.constData(), qMin<int>(name.size(), sizeof(req.name)));

    // 5锔忊儯 numeric fields
    req.perm = perm;
    req.count = count;
    req.time = time;
    req.nonce = nonce;

    // 6锔忊儯 CMAC (placeholder, first 16 bytes valid)
    // TODO: calculate real CMAC
    // 鍏堟竻闆跺叏閮?32 瀛楄妭
    memset(req.cmac, 0x00, sizeof(req.cmac));

    // 鎶撳寘閲岀殑 CMAC锛?6 瀛楄妭锛?
    static const uint8_t cmac_data[16] = {0x72, 0x7f, 0xd7, 0xf3, 0x0b, 0xd4,
                                          0xb3, 0x34, 0xbd, 0x2d, 0xc4, 0x6a,
                                          0x5c, 0xc2, 0x92, 0x30};

    // 鎷疯礉鍒?cmac[0..15]
    memcpy(req.cmac, cmac_data, sizeof(cmac_data));

    // 7锔忊儯 Signature (placeholder)
    // TODO: calculate real ECDSA signature
    // 涓嶉渶瑕佸啀 memset锛屼竴娆?memcpy 灏卞
    static const uint8_t sig_data[64] = {
        0x25, 0x53, 0x70, 0x57, 0x57, 0x00, 0x49, 0x33, 0x52, 0x3f, 0xe5,
        0xfa, 0xd2, 0xf1, 0x39, 0x4d, 0xa1, 0xd1, 0x96, 0xa6, 0x6c, 0x45,
        0x0d, 0xf7, 0x38, 0x95, 0x37, 0x21, 0xa5, 0x84, 0x7c, 0x80, 0xed,
        0xd2, 0x13, 0xba, 0xd6, 0xe2, 0x1f, 0x87, 0x92, 0x4c, 0x61, 0xa2,
        0x17, 0xf1, 0x11, 0xcf, 0x49, 0x37, 0x4b, 0x28, 0x5a, 0x05, 0x8e,
        0xd6, 0x45, 0x3a, 0xeb, 0x97, 0x3a, 0xe4, 0x63, 0xe5};

    memcpy(req.sig, sig_data, sizeof(sig_data));

    // 8锔忊儯 build QByteArray
    QByteArray v1data;

    // magic / header锛堝鏋滀綘鍗忚闇€瑕侊級
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));

    // payload
    v1data.append(reinterpret_cast<const char*>(&req), sizeof(req));

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xe2, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

// void DjiBulkDevice::set_check_one_time_auth (){
//     typedef struct {
//         uint8_t version;//01
//         uint8_t resv;//00
//         uint16_t sn_length;//0a 00
//         uint8_t sn[32];//sn鍑戞弧32涓?
//         uint8_t name[32];//o-joy鍑戞弧32涓?
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

//     QByteArray pkt = DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0,
//     0xe2, v1data); bulkWrite( pkt, 1000); // 鍙戝埌 OUT endpoint
//     0x01锛?00ms瓒呮椂
// }

void DjiBulkDevice::set_device_date() {
    QByteArray v1data;

    QDateTime now = QDateTime::currentDateTime();
    QDate date = now.date();
    QTime time = now.time();

    // year锛歶int16_t锛堝皬绔級
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
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x4a, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::get_Rpmb_Board() {

    QByteArray v1data;
    uint8_t sn_type = 0x01;
    v1data.append(char(sn_type));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x51, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::get_Rpmb_Device() {

    QByteArray v1data;
    uint8_t sn_type = 0x04;
    v1data.append(char(sn_type));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x51, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::get_root_key_status() {

    QByteArray v1data;

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xc6, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_sys_event_reboot() {
    QByteArray v1data;

    v1data.append(char(0x00));

    // DJI_RESET_TYPE_WARM_RESET	0x00	鐑噸鍚?
    // DJI_RESET_TYPE_COLD_RESET	0x01	     鍐烽噸鍚?
    // DJI_RESET_TYPE_POWER_OFF	0x02	    鍏虫満
    // DJI_RESET_TYPE_OVER_TMEP_PROTECTION	0x03	   杩囨俯淇濇姢
    // DJI_RESET_TYPE_POWER_ON	0x04	 寮€鏈?
    // DJI_RESET_TYPE_EMC_RESET	0x05	鐢变簬闈欑數鎴栧叾浠栧悎瑙勫師鍥犲鑷寸殑閲嶅惎
    v1data.append(char(0x01));

    // 閲嶅惎寤惰繜鏃堕棿
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));
    v1data.append(char(0x00));

    //   0x0005	dji_sys寮傛涓婃姤閲嶅惎缁撴灉銆?
    v1data.append(char(0x00));
    v1data.append(char(0x05));

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x0b, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_Rpmb_Board(const QString& sn) {
    QByteArray snBytes = sn.toUtf8();
    uint16_t snLen = snBytes.size();

    QByteArray v1data;

    uint8_t sn_type = 0x01;
    v1data.append(char(sn_type));

    // 鈶?sn_length锛堝皬绔級
    v1data.append(char(snLen & 0xFF));
    v1data.append(char((snLen >> 8) & 0xFF));

    // 鈶?sn 鍐呭
    v1data.append(snBytes);

    // 鉂?涓€鑸?SN 涓嶉渶瑕?\0锛岄櫎闈炲崗璁姹?
    // v1data.append(char(0x00));
    emit send_bulk_data("鍐欏叆鐨勬牳蹇冩澘sn鏄?" + sn);
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x50, v1data);

    if (writeCb_)
        writeCb_(pkt);
}

void DjiBulkDevice::set_Rpmb_Device(const QString& sn) {
    QByteArray snBytes = sn.toUtf8();
    uint16_t snLen = snBytes.size();

    QByteArray v1data;
    uint8_t sn_type = 0x04;
    v1data.append(char(sn_type));

    // 鈶?sn_length锛堝皬绔級
    v1data.append(char(snLen & 0xFF));
    v1data.append(char((snLen >> 8) & 0xFF));
    //sn鐨勫湴鍧€
    v1data.append(char(0x00));
    v1data.append(char(0x10));
    v1data.append(char(0x00));
    v1data.append(char(0xf5));

    // 鈶?sn 鍐呭
    v1data.append(snBytes);

    // 鉂?涓€鑸?SN 涓嶉渶瑕?\0锛岄櫎闈炲崗璁姹?
    // v1data.append(char(0x00));
    emit send_bulk_data("鍐欏叆鐨勮澶噑n鏄?" + sn);
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x50, v1data);

    if (writeCb_)
        writeCb_(pkt);
}

void DjiBulkDevice::set_amt_clean_flag() {
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("clear_log");
    v1data.append(char(0x00));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_amt_check_clean_flag() {
    QByteArray v1data;
    v1data.append(char(0x80));
    v1data.append(char(0x0a));
    v1data.append("check_clear_log");

    v1data.append(char(0x00));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0x44, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_sys_poweroff() { // 0x0a00 set system power_level: 5
    QByteArray v1data;

    v1data.append(char(0x82)); // 01 00 0001 璁剧疆涓?锛岃〃鏄巗ysmode娑堟伅
    v1data.append(char(0x05)); // s5鍏虫満鐨勬剰鎬?

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0, 0x44, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_2a_send_file_data() {
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

        // 鈶?璇锋眰绫诲瀷
        v1data.append(char(0x04));

        // 鈶?seq_num锛堝皬绔級
        v1data.append(reinterpret_cast<const char*>(&seq), sizeof(uint32_t));

        // 鈶?payload
        v1data.append(payload);

        // 鈶?缁勫寘
        QByteArray pkt =
            DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);

        // 鈶?鍙戦€?
        if (writeCb_)
            writeCb_(pkt);

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

void DjiBulkDevice::set_2a_send_file_info_check() {
    if (tow_a_filepath.isEmpty()) {
        qWarning() << "[2A] file path is empty";
        return;
    }

    QByteArray v1data;

    // 鈶?璇锋眰绫诲瀷
    v1data.append(char(0x03)); // 璇锋眰鎺ユ敹鏂囦欢 + 鏍￠獙

    // 鈶?鎵撳紑鏂囦欢
    QFile file(tow_a_filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[2A] open file failed:" << tow_a_filepath;
        return;
    }

    // 鈶?MD5 璁＄畻锛堜娇鐢?md5_append锛?
    md5_state_t md5;
    md5_byte_t digest[16];

    DjiBulkCodec::md5_init(&md5);

    while (!file.atEnd()) {
        QByteArray chunk = file.read(4096);
        if (!chunk.isEmpty()) {
            DjiBulkCodec::md5_append(&md5, reinterpret_cast<const md5_byte_t*>(chunk.constData()),
                                     chunk.size());
        }
    }

    DjiBulkCodec::md5_finish(&md5, digest);

    // 鈶?娣诲姞 md5锛坲int8_t[16]锛?
    v1data.append(reinterpret_cast<const char*>(digest), 16);

    // 璋冭瘯杈撳嚭
    QByteArray md5Hex(reinterpret_cast<const char*>(digest), 16);
    qDebug() << "[2A] file path =" << tow_a_filepath;
    qDebug() << "[2A] file md5  =" << md5Hex.toHex();

    // 鈶?缁勫寘骞跺彂閫?
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);

    if (writeCb_)
        writeCb_(pkt);
}

void DjiBulkDevice::set_2a_send_file_info(const QString& filepath) {
    QByteArray v1data;
    tow_a_filepath = filepath;
    // 鈶?璇锋眰绫诲瀷
    v1data.append(char(0x01)); // 璇锋眰鎺ユ敹鏂囦欢

    // 鈶?鎵撳紑鏂囦欢
    QFile file(filepath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "open file failed:" << filepath;
        return;
    }

    // 鈶?鏂囦欢澶у皬 uint32_t
    quint32 fileSize = file.size();
    v1data.append(reinterpret_cast<const char*>(&fileSize), sizeof(fileSize));

    // 鈶?鏂囦欢鍚嶏紙鍙彇鍚嶅瓧锛屼笉瑕佽矾寰勶級
    QString fileName = QFileInfo(filepath).fileName();
    QByteArray fileNameBytes = fileName.toUtf8();

    // 鈶?鏂囦欢鍚嶉暱搴?uint8_t
    quint8 fileNameLen = static_cast<quint8>(fileNameBytes.size());
    v1data.append(char(fileNameLen));

    // 鈶?鏂囦欢鍚嶆湰浣?
    v1data.append(fileNameBytes);

    qDebug() << "[2A] file size  =" << fileSize;

    v1data.append(char(0x00)); // 鎷撳睍鐨?
    v1data.append(char(0x01)); // 鎷撳睍鐨?
    v1data.append(char(0x01)); // 鎷撳睍鐨?

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_amt_task_test(const QString& cmdStr, uint32_t timeout) {

    emit send_bulk_data(QStringLiteral("执行脚本：") + cmdStr);

    QString input = cmdStr.trimmed();
    qDebug() << "[ComboBox] raw text =" << input;

    if (input.isEmpty()) {
        qWarning() << "[ComboBox] empty text";
        return;
    }

    // 1锔忊儯 鎸夌┖鏍煎垎鍓诧紙鏀寔澶氫釜绌烘牸锛?
    QStringList parts =
        input.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    qDebug() << "[ComboBox] split parts =" << parts;

    if (parts.isEmpty()) {
        qWarning() << "[ComboBox] no parts after split";
        return;
    }

    // 2锔忊儯 绗竴涓槸鑴氭湰鍚?
    QString cmd = parts.takeFirst();
    qDebug() << "[ComboBox] cmd =" << cmd;

    // 3锔忊儯 鍓╀綑鐨勬槸鍙傛暟
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
void DjiBulkDevice::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

void DjiBulkDevice::set_2a_download_path_info(const QString& filepath) {
    QByteArray v1data;
    tow_a_download_filepath = filepath;
    // 鈶?璇锋眰绫诲瀷
    v1data.append(char(0x06)); // 璇锋眰鎺ユ敹鏂囦欢
    QByteArray fileNameBytes = tow_a_download_filepath.toUtf8();

    quint8 fileNameLen = static_cast<quint8>(tow_a_download_filepath.size());
    v1data.append(char(fileNameLen));

    // 鈶?鏂囦欢鍚嶆湰浣?
    v1data.append(fileNameBytes);

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::set_2a_download_file_info(const QString& filepath) {
    m_totalFileSize = 0; // 鏂囦欢鎬诲ぇ灏忥紙瀛楄妭锛?
    m_receivedSize = 0;  // 宸叉帴鏀跺瓧鑺傛暟
    lost_list_rsp_seq = 0;
    QByteArray v1data;
    tow_a_download_filepath = filepath;
    QString fileName = QFileInfo(tow_a_download_filepath).fileName();

    // exe 鍚岀骇鐩綍
    QString basePath = QCoreApplication::applicationDirPath() + "/two_a_data";

    // 褰撳墠鏃ユ湡
    QString dateDirName = QDate::currentDate().toString("yyyyMMdd");
    QString timeTag = QDateTime::currentDateTime().toString("_HHmmss");

    // two_a_data/YYYYMMDD
    QDir dir(basePath + "/" + dateDirName + timeTag);

    // 閫掑綊鍒涘缓鐩綍
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "create dir failed:" << dir.absolutePath();
            return;
        }
    }

    tow_a_download_filepath_local = dir.filePath(fileName);

    // 鈶?璇锋眰绫诲瀷
    v1data.append(char(0x08)); // 璇锋眰鎺ユ敹鏂囦欢
    QByteArray fileNameBytes = tow_a_download_filepath.toUtf8();

    quint8 fileNameLen = static_cast<quint8>(tow_a_download_filepath.size());
    v1data.append(char(fileNameLen));

    // 鈶?鏂囦欢鍚嶆湰浣?
    v1data.append(fileNameBytes);

    v1data.append(char(0x00)); // 鎷撳睍鐨?
    v1data.append(char(0x01)); // 鎷撳睍鐨?
    v1data.append(char(0x01)); // 鎷撳睍鐨?

    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00, 0x2a, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::set_2a_download_file_devide_rsp() {
    QByteArray v1data;

    // 鈶?璇锋眰绫诲瀷
    v1data.append(char(0x00)); // 璇锋眰鎺ユ敹鏂囦欢

    // // 鈶?鎷撳睍瀛楁
    // v1data.append(char(0x00));
    // v1data.append(char(0x01));
    // v1data.append(char(0x01));

    // 鈶?pack_size = 980 (0x03D4)
    uint16_t pack_size = 980;
    v1data.append(char(pack_size & 0xFF));        // L
    v1data.append(char((pack_size >> 8) & 0xFF)); // H

    // 鈶?window_size = 5000 (0x1388)
    uint16_t window_size = 5000;
    v1data.append(char(window_size & 0xFF));        // L
    v1data.append(char((window_size >> 8) & 0xFF)); // H

    // 鈶?checksum_type
    v1data.append(char(0x01)); // 涓句緥锛歮d5

    // 鈶?protocol_type
    v1data.append(char(0x01)); // 涓句緥锛?A 鍗忚

    QByteArray pkt = DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00,
                                               0x2a, v1data, 0, 1);

    if (writeCb_)
        writeCb_(pkt);
}
void DjiBulkDevice::set_2a_download_file_devide_lost_list_rsp() {
    QByteArray v1data;

    // 鈶?璇锋眰绫诲瀷
    v1data.append(char(0x00)); // 璇锋眰鎺ユ敹鏂囦欢

    v1data.append(reinterpret_cast<const char*>(&lost_list_rsp_seq),
                  sizeof(uint32_t));

    QByteArray pkt = DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00,
                                               0x2a, v1data, 0, 1);

    if (writeCb_)
        writeCb_(pkt);
}
void DjiBulkDevice::set_2a_download_file_ok_rsp(QByteArray& f) {
    qDebug() << "==== set_2a_download_file_ok_rsp ====";
    qDebug() << "f.size() =" << f.size();
    qDebug() << "f md5 HEX =" << f.toHex(' ');

    // 鈶?鏍￠獙 MD5 闀垮害
    if (f.size() != 16) {
        qWarning() << "[2A] invalid md5 length:" << f.size();
        return;
    }

    if (tow_a_download_filepath.isEmpty()) {
        qWarning() << "[2A] download file path is empty";
        return;
    }

    // 鈶?鎵撳紑鏂囦欢

    QFile file(tow_a_download_filepath_local);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[2A] open file failed:" << tow_a_download_filepath_local;
        return;
    }

    // 鈶?璁＄畻鏈湴鏂囦欢 MD5
    md5_state_t md5;
    md5_byte_t digest[16];

    DjiBulkCodec::md5_init(&md5);

    while (!file.atEnd()) {
        QByteArray chunk = file.read(4096);
        if (!chunk.isEmpty()) {
            DjiBulkCodec::md5_append(&md5, reinterpret_cast<const md5_byte_t*>(chunk.constData()),
                                     chunk.size());
        }
    }

    DjiBulkCodec::md5_finish(&md5, digest);

    QByteArray localMd5(reinterpret_cast<const char*>(digest), 16);

    qDebug() << "[2A] local file md5 =" << localMd5.toHex();

    // 鈶?姣斿 MD5
    bool md5_ok = (localMd5 == f);

    qDebug() << "[2A] md5 compare result =" << md5_ok;
    if (md5_ok)
        emit send_bulk_data("2a涓嬭浇md5鏁版嵁姣斿鎴愬姛");
    else
        emit send_bulk_data("2a涓嬭浇md5鏁版嵁姣斿澶辫触");

    // 鈶?缁?rsp 鍖?
    QByteArray v1data;

    /*
   * 寤鸿鍗忚璇箟锛?
   * 0x00 = MD5 OK
   * 0x01 = MD5 FAIL
   */
    v1data.append(char(md5_ok ? 0x00 : 0x01));

    QByteArray pkt = DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0801), 2, 0x00,
                                               0x2a, v1data, 0, 1);

    if (writeCb_)
        writeCb_(pkt);
}

void DjiBulkDevice::prase_2a_download_file_data(QByteArray& f) {
    // qDebug() << "==== prase_2a_download_file_data ====";
    // qDebug() << "f.size() =" << f.size();
    // qDebug() << "f HEX =" << f.toHex(' ');

    if (f.size() < 4) {
        qWarning() << "data too short, size =" << f.size();
        return;
    }

    // 鈶?瑙ｆ瀽鍓?4 涓瓧鑺傦紙uint32锛屽皬绔級
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    lost_list_rsp_seq = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

    qDebug() << "lost_list_rsp_seq =" << lost_list_rsp_seq;

    emit send_bulk_data(QStringLiteral("当前PC端收到的窗口内的最大值：") +
                        QString::number(lost_list_rsp_seq));

    // 鈶?鐪熸鐨勬枃浠跺唴瀹癸紙浠庣 4 瀛楄妭寮€濮嬶級
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

    // 猸?鍏抽敭锛氬師濮嬪瓧鑺傚啓鍏?
    qint64 written = file.write(payload);
    file.close();

    if (written != payload.size()) {
        qWarning() << "[2A] write size mismatch:" << written << "/"
                   << payload.size();
    }

    // ================== 猸?鍏抽敭杩涘害閫昏緫 ==================
    m_receivedSize += written;

    int progress = 0;
    if (m_totalFileSize > 0) {
        progress = static_cast<int>((m_receivedSize * 100) / m_totalFileSize);
    }

    emit download2aprogress(progress);
}

void DjiBulkDevice::set_2a_download_file_info_check() {
    ;
}

void DjiBulkDevice::set_amt_task_start(const QString& cmdStr, uint32_t timeout,
                                       const QByteArray& param) {
    QByteArray payload;

    // ---------- cmd锛氬浐瀹?SYS_AMT_TEST_CMD_STR_LEN ----------
    QByteArray cmdBuf(SYS_AMT_TEST_CMD_STR_LEN, 0);
    QByteArray cmdUtf8 = cmdStr.toUtf8();
    uint32_t cmdlen = cmdUtf8.size();

    memcpy(cmdBuf.data(), cmdUtf8.constData(),
           qMin(cmdUtf8.size(), cmdBuf.size()));

    payload.append(cmdBuf); // 鉁?鍥哄畾闀垮害

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
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf4, payload);

    if (writeCb_)
        writeCb_(pkt);
}

void DjiBulkDevice::set_amt_task_get_result() {
    QByteArray v1data;
    uint32_t cmdid = m_cmdId; // 鉁?鍐呴儴鑷
    v1data.append(reinterpret_cast<const char*>(&cmdid), sizeof(cmdid));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf6, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::set_amt_task_get_log(uint32_t offset) {
    QByteArray v1data;
    uint32_t cmdid = m_cmdId;    // 鉁?鍐呴儴鑷
    uint16_t metaId = 0xffff;    // 娌℃湁鐢?
    uint8_t metaDataType = 0x01; // 娌℃湁鐢?
    uint32_t fetchLen = 4096;

    v1data.append(reinterpret_cast<const char*>(&cmdid), sizeof(cmdid));
    // metaid
    v1data.append(reinterpret_cast<const char*>(&metaId), sizeof(metaId));

    // meta_data_type
    v1data.append(reinterpret_cast<const char*>(&metaDataType),
                  sizeof(metaDataType));

    // meta_data_offset
    v1data.append(reinterpret_cast<const char*>(&offset), sizeof(offset));

    // fetchlength
    v1data.append(reinterpret_cast<const char*>(&fetchLen), sizeof(fetchLen));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf8, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_amt_task_rst() {
    QByteArray v1data;
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0803), 2, 0, 0xf7, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}
void DjiBulkDevice::set_write_product_status() {
    QByteArray v1data;
    // v1data.append(char(0x00));
    // v1data.append(char(0x00));
    QByteArray pkt =
        DjiBulkCodec::buildPacket(HOST_ID_FROM_16BIT_TO_8BIT(0x0804), 2, 0, 0xc4, v1data);
    if (writeCb_)
        writeCb_(pkt); // 鍙戝埌 OUT endpoint 0x01锛?00ms瓒呮椂
}

void DjiBulkDevice::process_dji_amt_task_start(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];

    emit sendGetDjiResponse(1, info.retCode);
}

static QString amtAckCodeToString(int code) {
    switch (static_cast<dji_amt_task_ack_code_e>(code)) {
    case AMT_TASK_COMMON_ACK_SUCCESS:
        return QString("Success");

    case AMT_TASK_COMMON_ACK_NOTSUPPORT:
        return QStringLiteral("不支持命令");

    case AMT_TASK_COMMON_ACK_FAILURE:
        return QString("鎵ц澶辫触");

    case AMT_TASK_COMMON_ACK_INVALID_STATE:
        return QStringLiteral("无效的设备状态");

    case AMT_TASK_COMMON_ACK_OUTOFTASK:
        return QString("浠诲姟瓒呮暟閲忎簡");

    case AMT_TASK_COMMON_ACK_ILLEGAL:
        return QString("闈炴硶鍙傛暟");

    default:
        return QString("鏈煡杩斿洖鍊?(0x%1)").arg(code, 2, 16, QLatin1Char('0'));
    }
}
QString DjiBulkDevice::amtTaskResultToString(uint8_t code) {
    switch (code) {
    case AMT_TASK_RESULT_PASS:
        is_running_amt = 0;
        return QStringLiteral("执行成功");
    case AMT_TASK_RESULT_ONGOING:
        is_running_amt = 1;
        return QStringLiteral("任务进行中");
    case AMT_TASK_RESULT_TIMER_ERROR:
        is_running_amt = 0;
        return QStringLiteral("定时器错误");
    case AMT_TASK_RESULT_EXECUTE_SHELL_ERROR:
        is_running_amt = 0;
        return QStringLiteral("脚本执行失败");
    case AMT_TASK_RESULT_NOT_FIND_TASK_INDEX:
        is_running_amt = 0;
        return QStringLiteral("未找到任务索引");
    case AMT_TASK_RESULT_TIMEOUT:
        is_running_amt = 0;
        return QStringLiteral("任务超时");
    case AMT_TASK_RESULT_BEING_STOPPED:
        is_running_amt = 0;
        return QStringLiteral("任务被中止");
    case AMT_TASK_RESULT_GENERIC_ERROR:
        is_running_amt = 0;
        return QStringLiteral("通用错误");
    default:
        return QStringLiteral("未知错误 (0x%1)").arg(code, 2, 16, QChar('0')).toUpper();
    }
}

void DjiBulkDevice::process_dji_amt_task_get_result(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    if (info.retCode == 0) {

        emit send_bulk_data("鑴氭湰鎵ц鐘舵€侊細" + amtTaskResultToString(p[1]));
        if (p[1] == AMT_TASK_RESULT_ONGOING)
            ;
        else if (p[1] == AMT_TASK_RESULT_PASS)
            emit sendGetDjiResponse(1, info.retCode);
        else
            emit sendGetDjiResponse(1, info.retCode);
    } else {
        emit sendGetDjiResponse(1, info.retCode);
        emit send_bulk_data(QStringLiteral("amt执行错误码翻译为：") +
                            amtAckCodeToString(info.retCode));
        emit send_bulk_data("鑴氭湰鎵ц鐘舵€侊細" + amtTaskResultToString(p[1]));
    }
}

void DjiBulkDevice::process_dji_2a_send_file(QByteArray& f) {

    struct DeviceInfo {
        uint8_t retCode;
    };

    DeviceInfo info = {0};

    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    uint8_t retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
    if (f.size() == 1) {
        emit send_bulk_data(QStringLiteral("收到回应成功状态"));
        return; // 姝ｅ父鍥炲簲涓嶅笇鏈涘線涓嬭窇
    }
    if (f.size() == 5) // 绐楀彛鐨勬枃浠剁鍑犲寘
    {
        uint32_t window_size = static_cast<uint32_t>(p[1]) |
            (static_cast<uint32_t>(p[2]) << 8) |
            (static_cast<uint32_t>(p[3]) << 16) |
            (static_cast<uint32_t>(p[4]) << 24);

        emit send_bulk_data(QStringLiteral("当前设备端收到的窗口内的最大值：") +
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
        return; // 涓嶅笇鏈涘線涓嬭窇
    }
    if (retCode == 0x04) {
        QByteArray payload = QByteArray::fromRawData(
            reinterpret_cast<const char*>(p + 1), f.size() - 1);

        prase_2a_download_file_data(payload);
        set_2a_download_file_devide_lost_list_rsp();

        return; // 涓嶅笇鏈涘線涓嬭窇
    }
    if (retCode == 0x03) {
        QByteArray payload = QByteArray::fromRawData(
            reinterpret_cast<const char*>(p + 1), f.size() - 1);
        set_2a_download_file_ok_rsp(payload);
        emit send_bulk_data("鏂囦欢涓嬭浇瀹屾垚");
        return; // 涓嶅笇鏈涘線涓嬭窇
    }

    // 灏忕瑙ｆ瀽
    two_a_pack_size =
        static_cast<uint16_t>(p[1]) | (static_cast<uint16_t>(p[2]) << 8);

    two_a_window_size =
        static_cast<uint16_t>(p[3]) | (static_cast<uint16_t>(p[4]) << 8);

    uint8_t checksum_type = p[5];
    uint8_t protocol_type = p[6];

    // ---- 鏃ュ織 ----
    qDebug() << "[2A]"
             << "retCode ="
             << QString("0x%1").arg(retCode, 2, 16, QLatin1Char('0'))
             << "pack_size =" << two_a_pack_size
             << "window_size =" << two_a_window_size
             << "checksum_type =" << checksum_type
             << "protocol_type =" << protocol_type;

    // checksum_type
    // DJI_FILE_TRANS_CHECKSUM_MD5	0X01	  MD5鏍￠獙
    // DJI_FILE_TRANS_CHECKSUM_CRC16	0X02	  CRC16鏍￠獙
    // DJI_FILE_TRANS_CHECKSUM_CRC32	0X03	 CRC32鏍￠獙

    // protocol_type
    // DJI_TRANS_EVERY_PACK_ACK	0X00	姣忎釜鍖卆ck
    // DJI_TRANS_LOST_LIST_ACK	0X01	涓㈠寘鍒楄〃ack

    // DJI_DUSS_MB_RET_OK = 0, // 鍥炲OK
    // DJI_DUSS_MB_RET_ACK = 1, // 鍥炲ACK
    // DJI_DUSS_MB_RET_INVALID_CMD = 224, // 鍛戒护鏃犳晥
    // DJI_DUSS_MB_RET_TIMEOUT = 225, // 瓒呮椂
    // DJI_DUSS_MB_RET_OUT_OF_MEMORY = 226, // 瓒呭嚭鍐呭瓨
    // DJI_DUSS_MB_RET_INVALID_PARAM = 227, // 鍙傛暟鏃犳晥
    // DJI_DUSS_MB_RET_INVALID_STATE = 228, // 鐘舵€佹棤鏁?
    // DJI_DUSS_MB_RET_TIME_NOT_SYNC = 229, // 鏃堕棿涓嶅悓姝?
    // DJI_DUSS_MB_RET_SET_PARAM_FAILED = 230, // 璁剧疆鍙傛暟澶辫触
    // DJI_DUSS_MB_RET_GET_PARAM_FAILED = 231, // 鑾峰彇鍙傛暟澶辫触
    // DJI_DUSS_MB_RET_SDCARD_NOT_INSERTED = 232, // 鏈彃SD鍗?
    // DJI_DUSS_MB_RET_SDCARD_FULL = 233, // SD鍗℃弧
    // DJI_DUSS_MB_RET_SDCARD_ERR = 234, // SD鍗￠敊璇?
    // DJI_DUSS_MB_RET_SENSOR_ERR = 235, // 浼犳劅鍣ㄩ敊璇?
    // DJI_DUSS_MB_RET_CRITICAL_ERR = 236, // 涓ラ噸閿欒
    // DJI_DUSS_MB_RET_PRARM_LEN_TOO_LONG = 237, // 鍙傛暟闀垮害瓒呴暱
    // DJI_DUSS_MB_RET_FW_SEQNUM_NOT_IN_ORDER = 240, // 鍥轰欢搴忓垪涓嶅湪鍛戒护涓?
    // DJI_DUSS_MB_RET_FW_EXCEED_FLASH = 241, // 鍥轰欢瓒呭嚭flash鍐呭瓨鑼冨洿
    // DJI_DUSS_MB_RET_FW_CHECK_ERR = 242, // 鍥轰欢妫€鏌ュけ璐?
    // DJI_DUSS_MB_RET_FW_FLASH_ERASE_ERR = 243, // 鍥轰欢flash鎿﹂櫎澶辫触
    // DJI_DUSS_MB_RET_FW_FLASH_PROGRAM_ERR = 244, // 鍥轰欢flash缂栫▼澶辫触
    // DJI_DUSS_MB_RET_FW_UPDATE_STATE_ERR = 245, // 鍥轰欢鍗囩骇鐘舵€侀敊璇?
    // DJI_DUSS_MB_RET_FW_INVALID_TYPE = 246, // 鍥轰欢绫诲瀷鏃犳晥
    // DJI_DUSS_MB_RET_FW_UPDATE_WAIT_FINISH = 247, // 鍥轰欢鍗囩骇绛夊緟瀹屾垚
    // DJI_DUSS_MB_RET_FW_UPDATE_RC_DISCONNECT = 248, // 鍥轰欢鍗囩骇閬ユ帶鍣ㄦ柇杩?
    // DJI_DUSS_MB_RET_FW_UPGRADE_MOTOR_RUNNING = 249, // 鍥轰欢鍗囩骇鐢垫満杩愯浆
    // DJI_DUSS_MB_RET_HARDWARE_ERR = 250, // 纭欢閿欒
    // DJI_DUSS_MB_RET_DEV_BAT_NOT_ENOUGH = 251, // 璁惧鐢垫睜鐢甸噺涓嶈冻
    // DJI_DUSS_MB_RET_DEV_UAV_DISCONNECT = 252, // 璁惧椋炴満澶辫繛
    // DJI_DUSS_MB_RET_FW_FLASH_ERASING = 253, // 鍥轰欢flash鎿﹂櫎涓?
    // DJI_DUSS_MB_RET_CHECK_CONNECTION_ERR = 254, // 妫€鏌ヨ繛鎺ラ敊璇?
    // DJI_DUSS_MB_RET_UNSPECIFIED = 255, // 淇濈暀
    // ---- 涓氬姟澶勭悊 ----
}

// test_wakealarm_interrupts.sh  鐮旂┒鐪嬬湅鏃ュ織灏戞崬鐨勯棶棰?
void DjiBulkDevice::process_dji_amt_task_get_log(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info = {0};

    if (f.size() < 9) {
        qWarning() << "[AMT] get_log ack too short:" << f.size();
        return;
    }

    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    uint8_t retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    // 2锔忊儯 actual_log_length (LSB)
    uint32_t actualLen = p[1] | (p[2] << 8) | (p[3] << 16) | (p[4] << 24);

    // 3锔忊儯 remainder_log_length
    uint32_t remainLen = p[5] | (p[6] << 8) | (p[7] << 16) | (p[8] << 24);

    // 4锔忊儯 meta_data
    QByteArray metaData = f.mid(9, actualLen);

    // ---- debug ----
    qDebug().noquote() << "[AMT LOG]"
                       << "retCode=" << retCode << "actualLen=" << actualLen
                       << "remainLen=" << remainLen
                       << "metaData HEX=" << metaData.toHex(' ').toUpper();

    // 濡傛灉 metaData 鏄枃鏈?
    if (!metaData.isEmpty()) {
        qDebug().noquote() << "metaData TXT=" << QString::fromUtf8(metaData);
    }
    emit send_bulk_data(QString("鎵ц鏃ュ織锛?1").arg(QString::fromUtf8(metaData)));
    // 5锔忊儯 濡傛灉杩樻湁鍓╀綑鏃ュ織锛岀户缁?fetch
    if (remainLen > 0) {
        set_amt_task_get_log(actualLen);
    }
}

void DjiBulkDevice::process_sys_event_reboot(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t product_status;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}
void DjiBulkDevice::process_sys_event_report_status(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t product_status;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
    info.product_status = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
        ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("鎶ュ憡鍐呭=%1").arg(info.product_status));
}
void DjiBulkDevice::process_set_product_status(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}

void DjiBulkDevice::process_read_root_key_status(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint8_t key_status;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    if (info.retCode == 0) {
        info.key_status = (uint8_t)p[1];

        emit send_bulk_data(QString("key娉ㄥ叆鎯呭喌=%1").arg(info.key_status));
    }
}

void DjiBulkDevice::process_dji_get_product_status(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t product_status;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    if (info.retCode == 0) {
        info.product_status = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
            ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);

        emit send_bulk_data(QString("鍒囬噺浜х姸鎬?%1").arg(info.product_status));
    }
}

void DjiBulkDevice::process_get_sn_operate(QByteArray& f) {
#pragma pack(push, 1)
    struct DeviceInfo {
        uint8_t retCode;
        uint16_t length; // little-endian
        uint8_t serial_number[0];
        // 椋炴帶鍦板潃
        // uint32_t fc_addr;浠栫殑boradsn鍙湁10涓師鍥犳槸杩欎釜
        // 鐪熷疄product sn
        // uint8_t product_sn[0];
    };
#pragma pack(pop)

    // 1锔忊儯 鏈€灏忓寘鏍￠獙
    if (f.size() < int(sizeof(DeviceInfo))) {
        qWarning() << "SN packet too short";
        return;
    }

    const DeviceInfo* info = reinterpret_cast<const DeviceInfo*>(f.constData());

    // 2锔忊儯 绔簭淇
    uint16_t sn_len = qFromLittleEndian(info->length);

    // 3锔忊儯 闀垮害涓€鑷存€ф牎楠?
    if (f.size() != int(sizeof(DeviceInfo) + sn_len)) {
        qWarning() << "SN length mismatch:" << sn_len << f.size();
        return;
    }

    // 4锔忊儯 鍙?SN
    const char* sn_ptr =
        reinterpret_cast<const char*>(info) + sizeof(DeviceInfo);

    if (sn_ptr[0] == 0) {
        sn_len = sn_len - 4;
        sn_ptr = sn_ptr + 4;
    }

    QByteArray snData(sn_ptr, sn_len);
    QString sn = QString::fromLatin1(snData);
    // 5锔忊儯 姝ｇ‘鐨勮繑鍥炵爜閫昏緫

    emit sendGetDjiResponse(1, info->retCode);

    // 6锔忊儯 璋冭瘯杈撳嚭
    emit send_bulk_data(QString("闀垮害=%1").arg(sn_len));
    emit send_bulk_data(QString("sn=%1").arg(sn));
}

void DjiBulkDevice::process_set_sn_operate(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t value;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

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
        return QStringLiteral("无可用 slot");
    case CHECK_ERR_UNRD_OPEN:
        return QStringLiteral("UNRD 打开失败");
    case CHECK_ERR_UNRD_ALLOC:
        return QStringLiteral("UNRD 内存分配失败");
    case CHECK_ERR_UNRD_GET:
        return QStringLiteral("UNRD 获取数据失败，做一下大包升级");
    case CHECK_ERR_POPEN:
        return QStringLiteral("popen 执行失败");
    case CHECK_ERR_EXEC:
        return QStringLiteral("命令执行失败");
    case CHECK_ERR_LENGTH:
        return QStringLiteral("数据长度错误");
    case CHECK_ERR_MISMATCH:
        return QStringLiteral("数据校验不匹配");
    case CHECK_ERR_MEM_CALLOC:
        return QStringLiteral("内存 calloc 失败");
    default:
        return QStringLiteral("未知错误(0x%1)").arg(err, 2, 16, QLatin1Char('0'));
    }
}

void DjiBulkDevice::process_get_anti_rollback_comm(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t value;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);

    if (f.size() == 2) {
        emit send_bulk_data(QString("褰撳墠鎵€鍦ㄦЫ=%1").arg(p[1]));
        return;
    }
    if (f.size() == 3) {

        emit send_bulk_data(
            QString("ab鍒嗗尯鏍￠獙澶辫触鍊煎垎鍒负=%1 %2").arg(p[1]).arg(p[2]));
        if (info.retCode == 0x00)
            emit send_bulk_data("ab鍒嗗尯鏍￠獙鎴愬姛");
        else
            emit send_bulk_data(QStringLiteral("AB 分区校验失败：") + QStringLiteral("A[%1: %2], B[%3: %4]").arg(p[1]).arg(checkErrToString(p[1])).arg(p[2]).arg(checkErrToString(p[2])));

        return;
    }
    info.value = (uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) |
        ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("杩斿洖婊氱殑value=%1").arg(info.value));
}

void DjiBulkDevice::process_sec_dbg_command_auth_handler(QByteArray& f) {
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

void DjiBulkDevice::process_sec_dbg_command_req_handler(QByteArray& f) {
#pragma pack(push, 1)
    struct DeviceInfo {
        uint8_t retCode;    // 0
        uint16_t sn_length; // 1~2  灏忕
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

    // 1锔忊儯 retCode
    emit sendGetDjiResponse(1, info.retCode);

    // 2锔忊儯 sn_length锛圲SB 鍗忚榛樿灏忕锛孮t / x86 鐩存帴鍙敤锛?
    uint16_t snLen = info.sn_length;
    if (snLen > sizeof(info.sn))
        snLen = sizeof(info.sn);

    // 3锔忊儯 sn 杞?QString锛堝亣璁炬槸 ASCII锛?
    QString snStr =
        QString::fromLatin1(reinterpret_cast<const char*>(info.sn), snLen);

    emit send_bulk_data(QStringLiteral("SN=%1").arg(snStr));

    // // 4锔忊儯 nonce锛堝鏋滀綘鍚庨潰瑕佺敤锛?
    // qDebug() << "nonce =" << Qt::hex << info.nonce;

    set_sec_dbg_auth_req_one_func(snStr, "o-joy", 0x70, 0x32, 0x1c0107ea,
                                  info.nonce);
}

void DjiBulkDevice::process_get_product_dbg_misc_subcmd_count(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
        uint32_t left_count;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
    info.left_count = (uint32_t)p[1] | ((uint32_t)p[2] << 8) |
        ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);

    emit send_bulk_data(QString("瀹夊叏璋冭瘯妯″紡娆℃暟=%1").arg(info.left_count));
}

void DjiBulkDevice::process_dji_get_active(QByteArray& f) {
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

    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    if (p[1] == 5) {
        sec_cmd_act_trial_times_ack_t times_info;
        memcpy(&times_info, f.constData(), sizeof(sec_cmd_act_trial_times_ack_t));

        emit send_bulk_data(
            QString("璇曠敤鎬绘鏁?%1").arg(times_info.total_trial_times));
        emit send_bulk_data(
            QString("宸茶瘯鐢ㄦ鏁?%1").arg(times_info.used_trial_times));
        return;
    }

    int len = f.size();
    int offset = 0;
    info.retCode = p[0];
    // 1锔忊儯 retCode
    if (len < 1)
        return;
    uint8_t retCode = p[offset++];
    if (retCode == 0)
        emit sendGetDjiResponse(1, info.retCode);
    else {
        emit sendGetDjiResponse(1, info.retCode);
        return;
    }

    // 2锔忊儯 reserved(6) + activate_state(2)
    if (offset + 1 > len)
        return;
    uint8_t state_byte = p[offset++];

    uint8_t activate_state = state_byte & 0x03; // bit[1:0]
    uint8_t reserved = state_byte >> 2;         // bit[7:2]

    qDebug() << "activate_state =" << activate_state;
    emit send_bulk_data(QString("婵€娲荤姸鎬佸€?%1").arg(activate_state));

    qDebug() << "reserved =" << reserved;

    // 3锔忊儯 activation_time锛? 瀛楄妭锛?
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

    // 4锔忊儯 board_num_length
    if (offset + 1 > len)
        return;
    uint8_t board_num_length = p[offset++];

    // 5锔忊儯 board_num锛堝彉闀匡級
    if (offset + board_num_length > len)
        return;
    QByteArray board_num(reinterpret_cast<const char*>(p + offset),
                         board_num_length);
    offset += board_num_length;

    qDebug() << "board_num HEX =" << board_num.toHex(' ');
    qDebug() << "board_num STR =" << QString::fromLatin1(board_num);
}

void DjiBulkDevice::process_dji_set_date(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}
void DjiBulkDevice::process_dji_get_date(QByteArray& f) {
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

    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    int idx = 0;

    // 1锔忊儯 retCode
    info.retCode = p[idx++];

    // 2锔忊儯 year锛坲int16_t锛屽皬绔級
    info.year = p[idx] | (p[idx + 1] << 8);
    idx += 2;

    // 3锔忊儯 time fields
    info.month = p[idx++];
    info.day = p[idx++];
    info.hour = p[idx++];
    info.minute = p[idx++];
    info.second = p[idx++];

    // ===== 涓氬姟澶勭悊 =====
    emit sendGetDjiResponse(1, info.retCode);

    // ===== 鏍煎紡鍖栨棩鏈熸椂闂?=====
    QString dateStr = QString("%1-%2-%3 %4:%5:%6")
                          .arg(info.year, 4, 10, QLatin1Char('0'))
                          .arg(info.month, 2, 10, QLatin1Char('0'))
                          .arg(info.day, 2, 10, QLatin1Char('0'))
                          .arg(info.hour, 2, 10, QLatin1Char('0'))
                          .arg(info.minute, 2, 10, QLatin1Char('0'))
                          .arg(info.second, 2, 10, QLatin1Char('0'));

    emit send_bulk_data(QString("鏃ユ湡=%1").arg(dateStr));
}

void DjiBulkDevice::process_dji_factory_mode_handle(QByteArray& f) {
    struct DeviceInfo {
        uint8_t retCode;
    };
    DeviceInfo info;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];
    emit sendGetDjiResponse(1, info.retCode);
}

void DjiBulkDevice::process_djiFactroyCmd_get_version(QByteArray& f) {
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

    const uint8_t* p = reinterpret_cast<const uint8_t*>(f.constData());

    info.retCode = p[0];

    emit sendGetDjiResponse(1, info.retCode);
    // 2锔忊儯 major / minor version (4bit + 4bit)
    info.majorVersion = (p[1] >> 4) & 0x0F;
    info.minorVersion = p[1] & 0x0F;

    // 3锔忊儯 hardware_version (CHAR8, 0 缁撳熬)
    info.hardwareVersion =
        QString::fromLatin1(reinterpret_cast<const char*>(p + 2),
                            strnlen(reinterpret_cast<const char*>(p + 2), 16));

    // 4锔忊儯 loader_version (LSB 鈫?MSB)
    info.loaderVersion = QString("%1.%2.%3.%4")
                             .arg(p[21], 2, 10, QChar('0'))  // AA
                             .arg(p[20], 2, 10, QChar('0'))  // BB
                             .arg(p[19], 2, 10, QChar('0'))  // CC
                             .arg(p[18], 2, 10, QChar('0')); // DD

    // 5锔忊儯 firmware_version (LSB 鈫?MSB)
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

    // 6锔忊儯 鎵撳嵃锛堝共鍑€銆佷笉涓戯級
    qDebug().noquote()
        << "[GET_VERSION]" << QString("ret=%1").arg(info.retCode)
        << QString("proto=%1.%2").arg(info.majorVersion).arg(info.minorVersion)
        << QString("hw=%1").arg(info.hardwareVersion)
        << QString("loader=%1").arg(info.loaderVersion)
        << QString("fw=%1").arg(info.firmwareVersion);

    emit send_bulk_data(QString("鐗堟湰鍙?%1").arg(info.firmwareVersion));
    emit send_bulk_data(QString("鍔犲瘑鐘舵€?%1").arg(info.is_mass_product));
}
