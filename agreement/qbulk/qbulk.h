#ifndef QBULK_H
#define QBULK_H

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#include <QByteArray>
#include <cstdint>
#include <lusb0_usb.h>
// extern "C" {
// #include <lusb0_usb.h>   // libusb 0.1 API
// }
#include <QMessageBox>
#include <QObject>
#include <QQueue>
#define USB_ERROR_IO -1
#define USB_ERROR_INVALID_PARAM -2
#define USB_ERROR_ACCESS -3
#define USB_ERROR_NO_DEVICE -4 // 有些版本
#define USB_ERROR_NOT_FOUND -5 // ★ 常见
#define USB_ERROR_NOT_DATA -116
#define DUSS_MB_PACKAGE_V1_CRCH_INIT 0x77  /**< CRC 8 init value */
#define DUSS_MB_PACKAGE_V1_CRC_INIT 0x3692 /**< CRC 16 init value */
#define HOST_ID_FROM_16BIT_TO_8BIT(host_id)                                    \
(((((host_id) & 0x07) << 5) | (((host_id) >> 8) & 0x1F)) & 0xFF)
#define HOST_ID_FROM_8BIT_TO_16BIT(host_id)                                    \
(((((host_id) & 0x1F) << 8) | (((host_id) >> 5) & 0x07)) & 0xFFFF)
#define SYS_AMT_TEST_CMD_STR_LEN (512)
typedef enum __sys_amt_task_result_e {
    AMT_TASK_RESULT_PASS = 0x00,
    AMT_TASK_RESULT_ONGOING = 0xf0,
    AMT_TASK_RESULT_TIMER_ERROR = 0xf1,
    AMT_TASK_RESULT_EXECUTE_SHELL_ERROR = 0xf2,
    AMT_TASK_RESULT_NOT_FIND_TASK_INDEX = 0xf3,
    AMT_TASK_RESULT_TIMEOUT = 0xf4,
    AMT_TASK_RESULT_BEING_STOPPED = 0xf5,
    AMT_TASK_RESULT_GENERIC_ERROR = 0xf6,
    AMT_TASK_RESULT_OTHER
} sys_amt_test_result_e;


typedef enum _djiFactroyCmd {
    djiFactroyCmd_FIRST_COMMAND_NO_USE = 0,
    djiFactroyCmd_get_version = 0x01,
    djiFactroyCmd_factory_mode_handle = 0x44, // 清楚工厂数据    关机


    djiFactroyCmd_2a_send_file = 0x2a,




    //security 0804
    djiFactroyCmd_get_product_status = 0xc5,
    djiFactroyCmd_set_product_status = 0xc4,
    djiFactroyCmd_read_root_key_status = 0xc6,
    djiFactroyCmd_sec_act_command_general = 0x32,


    //amt 0803
    djiFactroyCmd_amt_task_start = 0xf4,
    djiFactroyCmd_amt_task_get_result = 0xf6,
    djiFactroyCmd_amt_task_get_log = 0xf8,
    djiFactroyCmd_get_product_dbg_misc_subcmd_count = 0xe0,
    djiFactroyCmd_sec_dbg_command_req_handler=0xe1,
    djiFactroyCmd_sec_dbg_command_auth_handler=0xe2,
    djiFactroyCmd_set_sn_operate = 0x50,
    djiFactroyCmd_get_sn_operate = 0x51,


    //djisys 0801
    djiFactroyCmd_sys_event_reboot=0x0b,
    djiFactroyCmd_sys_event_report_status=0x0c,

    djiFactroyCmd_set_date = 0x4a,
    djiFactroyCmd_get_date = 0x4b,
    djiFactroyCmd_anti_rollback_comm = 0x36,




} djiFactroyCmd;
typedef enum __dji_amt_task_ack_code_e {
    AMT_TASK_COMMON_ACK_SUCCESS = 0,
    AMT_TASK_COMMON_ACK_NOTSUPPORT = 0xE0, // 0xE0
    AMT_TASK_COMMON_ACK_FAILURE = 0xE1,
    AMT_TASK_COMMON_ACK_INVALID_STATE = 0xE4, // 0xE4
    AMT_TASK_COMMON_ACK_OUTOFTASK = 0xE5,
    AMT_TASK_COMMON_ACK_ILLEGAL = 0xE6,
} dji_amt_task_ack_code_e;

typedef unsigned char md5_byte_t; /* 8-bit byte */
typedef unsigned int md5_word_t;  /* 32-bit word */

/* Define the state of the MD5 Algorithm. */
typedef struct md5_state_s {
    md5_word_t count[2]; /* message length in bits, lsw first */
    md5_word_t abcd[4];  /* digest buffer */
    md5_byte_t buf[64];  /* accumulate block */
} md5_state_t;

class QBulk : public QObject {
    Q_OBJECT
public:
    QBulk();
    ~QBulk();
    uint8_t ep_numer = 0x05;
    using UsbVidPid = QPair<uint16_t, uint16_t>;
    QStringList usbDeviceList; // 给 comboBox 用
    QSet<UsbVidPid> usbDeviceSet;

    // 打开 USB 设备，vid/pid，指定接口号
    bool openDevice(uint16_t vid, uint16_t pid, int interfaceNumber);

    // 关闭设备
    void closeDevice();
    bool searchDevice();
    // Bulk 读取
    bool bulkRead(unsigned char ep, QByteArray &data,
                  unsigned int timeout = 1000);

    // Bulk 写入
    bool bulkWrite(const QByteArray &data, unsigned int timeout = 1000);
    // 数据解包/命令解析
    void parseCmd(QByteArray &buffer);
    void handlePacket(const QByteArray &packet);
    QByteArray buildPacket(uint8_t receiver, uint8_t cmdType, uint8_t cmdSet,
                           uint8_t cmdID, const QByteArray &data,
                           uint8_t encryptionType = 0, uint8_t isResponse = 0);
    uint16_t duss_util_crc16_calc(const uint8_t *data, uint32_t len,
                                  uint16_t init_crc);
    uint8_t crc8_calc(const uint8_t *data, uint32_t len, uint8_t init_crc);
    void md5_init(md5_state_t *pms);
    void md5_append(md5_state_t *pms, const md5_byte_t *data, int nbytes);
    void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);
    bool is_open = false;
public slots:
    void startRead(); // 线程里跑
    void stopRead();
    int isOpen() { return is_open; }

public slots:
    void get_dev_ver_status();

    void get_product_dbg_misc_subcmd_count();
    void get_device_date();

    void get_product_status();
    void get_product_active();
 void get_current_slot();
     void get_active_times();
         void set_wake_wifi();

     void get_product_md5_result();

    void get_Esdd_Check_Antirollback();

    // void set_Esdd_Check_Antirollback();

    void set_Rpmb_Board(const QString &sn);
    void set_Rpmb_Device(const QString &sn);

    void get_Rpmb_Board();
    void get_Rpmb_Device();
    void get_root_key_status();


    // void set_check_one_time_auth();

    void set_product_dbg_count();
    void set_amt_clean_flag();
    void set_device_restory_setting();
    void  set_sec_dbg_auth_req_one_func(
        const QString& snStr,
        const QString& nameStr,
        uint32_t perm,
        uint32_t count,
        uint32_t time,
        uint32_t nonce);
    void set_device_date();
    void set_amt_check_clean_flag();
    void set_amt_task_start(const QString &cmdStr, uint32_t timeout,
                            const QByteArray &param);
    void set_amt_task_get_result();
    void set_amt_task_get_log(uint32_t offset);

    void set_amt_task_test(const QString &cmdStr, uint32_t timeout);

    void set_amt_task_rst();
    void set_write_product_status();

    void set_sys_poweroff();
    void set_2a_send_file_info(const QString &filepath);
    void set_2a_send_file_data();
    void set_2a_send_file_info_check();

    void set_2a_download_path_info(const QString &filepath);
    void set_2a_download_file_info(const QString &filepath);
    void set_2a_download_file_devide_rsp();
    void set_2a_download_file_devide_lost_list_rsp();
    void set_2a_download_file_ok_rsp(QByteArray &f);

    void prase_2a_download_file_data(QByteArray &f);
    void set_2a_download_file_info_check();

     void set_sys_event_reboot();



signals:
    void readyRead(QByteArray &data);
    void bulk_device_error(int code, const QString &msg);
    void send_bulk_data(QString data);
    void sendGetDjiResponse(int data,int errocode=0);
    void send2aprogress(int data);
    void download2aprogress(int data);
    void reconect(); // 通知上层重新连接

    void usbDeviceAdded(quint16 vid, quint16 pid);
    void usbDeviceListReady(const QSet<UsbVidPid> &devices);

private slots:
    void process_djiFactroyCmd_get_version(QByteArray &f);
    void process_dji_factory_mode_handle(QByteArray &f);
    void process_dji_amt_task_start(QByteArray &f);
    void process_dji_amt_task_get_result(QByteArray &f);
    void process_dji_amt_task_get_log(QByteArray &f);
    void process_dji_2a_send_file(QByteArray &f);
    void process_dji_set_date(QByteArray &f);
    void process_dji_get_date(QByteArray &f);
    void process_dji_get_active(QByteArray &f);
    void process_dji_get_product_status(QByteArray &f);
    void process_get_product_dbg_misc_subcmd_count(QByteArray &f);
    void process_get_anti_rollback_comm(QByteArray &f);
    void process_set_sn_operate(QByteArray &f);
    void process_get_sn_operate(QByteArray &f);
    void process_sec_dbg_command_req_handler(QByteArray &f);
    void process_sec_dbg_command_auth_handler(QByteArray &f);
    void process_sys_event_reboot(QByteArray &f);
    void process_sys_event_report_status(QByteArray &f);//没有用
    void process_set_product_status(QByteArray &f);
    void process_read_root_key_status(QByteArray &f);






private:
    QString amtTaskResultToString(uint8_t code);
    void waitWork(int ms);
    bool is_running_amt = false;
    QString tow_a_filepath;
    uint32_t lost_list_rsp_seq = 0;
    QString tow_a_download_filepath;
    QString tow_a_download_filepath_local;
    bool two_a_can_send = 1;
    uint16_t two_a_pack_size = 0;
    uint16_t two_a_window_size = 0;
    uint32_t m_cmdId = 0;

    quint64 m_totalFileSize = 0; // 文件总大小（字节）
    quint64 m_receivedSize = 0;  // 已接收字节数

    void registerCommand();
    typedef std::function<void(QByteArray &)> callback;
    std::map<djiFactroyCmd, callback> djifactoryCommandList;
    struct usb_dev_handle *handle = nullptr;
    std::atomic_bool running{false};

    // CRC 校验
    bool checkHeaderCRC(const QByteArray &packet);
    bool checkDataCRC(const QByteArray &packet);
    std::atomic<uint16_t> m_seqNum{0}; // 线程安全
};

#endif // QBULK_H
