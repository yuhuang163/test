#ifndef DJI_BULK_DEVICE_H
#define DJI_BULK_DEVICE_H

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#include <QByteArray>
#include <QObject>
#include <QString>
#include <functional>
#include <map>
#include "dji_bulk_types.h"
#include "dji_bulk_codec.h"

class DjiBulkDevice : public QObject {
    Q_OBJECT
public:
    explicit DjiBulkDevice(QObject* parent = nullptr);
    ~DjiBulkDevice();

    using WriteCallback = std::function<void(const QByteArray& data)>;
    void setWriteCallback(const WriteCallback& cb) { writeCb_ = cb; }

    void onFrameReceived(const DjiBulkFrame& frame);

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
    void set_Rpmb_Board(const QString& sn);
    void set_Rpmb_Device(const QString& sn);
    void get_Rpmb_Board();
    void get_Rpmb_Device();
    void get_root_key_status();
    void set_product_dbg_count();
    void set_amt_clean_flag();
    void set_device_restory_setting();
    void set_sec_dbg_auth_req_one_func(
        const QString& snStr,
        const QString& nameStr,
        uint32_t perm,
        uint32_t count,
        uint32_t time,
        uint32_t nonce);
    void set_device_date();
    void set_amt_check_clean_flag();
    void set_amt_task_start(const QString& cmdStr, uint32_t timeout,
                            const QByteArray& param);
    void set_amt_task_get_result();
    void set_amt_task_get_log(uint32_t offset);
    void set_amt_task_test(const QString& cmdStr, uint32_t timeout);
    void set_amt_task_rst();
    void set_write_product_status();
    void set_sys_poweroff();
    void set_2a_send_file_info(const QString& filepath);
    void set_2a_send_file_data();
    void set_2a_send_file_info_check();
    void set_2a_download_path_info(const QString& filepath);
    void set_2a_download_file_info(const QString& filepath);
    void set_2a_download_file_devide_rsp();
    void set_2a_download_file_devide_lost_list_rsp();
    void set_2a_download_file_ok_rsp(QByteArray& f);
    void prase_2a_download_file_data(QByteArray& f);
    void set_2a_download_file_info_check();
    void set_sys_event_reboot();

signals:
    void send_bulk_data(QString data);
    void sendGetDjiResponse(int data, int errocode = 0);
    void send2aprogress(int data);
    void download2aprogress(int data);

private slots:
    void process_djiFactroyCmd_get_version(QByteArray& f);
    void process_dji_factory_mode_handle(QByteArray& f);
    void process_dji_amt_task_start(QByteArray& f);
    void process_dji_amt_task_get_result(QByteArray& f);
    void process_dji_amt_task_get_log(QByteArray& f);
    void process_dji_2a_send_file(QByteArray& f);
    void process_dji_set_date(QByteArray& f);
    void process_dji_get_date(QByteArray& f);
    void process_dji_get_active(QByteArray& f);
    void process_dji_get_product_status(QByteArray& f);
    void process_get_product_dbg_misc_subcmd_count(QByteArray& f);
    void process_get_anti_rollback_comm(QByteArray& f);
    void process_set_sn_operate(QByteArray& f);
    void process_get_sn_operate(QByteArray& f);
    void process_sec_dbg_command_req_handler(QByteArray& f);
    void process_sec_dbg_command_auth_handler(QByteArray& f);
    void process_sys_event_reboot(QByteArray& f);
    void process_sys_event_report_status(QByteArray& f);
    void process_set_product_status(QByteArray& f);
    void process_read_root_key_status(QByteArray& f);

private:
    QString amtTaskResultToString(uint8_t code);
    void waitWork(int ms);
    void registerCommand();

    WriteCallback writeCb_;

    bool is_running_amt = false;
    QString tow_a_filepath;
    uint32_t lost_list_rsp_seq = 0;
    QString tow_a_download_filepath;
    QString tow_a_download_filepath_local;
    bool two_a_can_send = 1;
    uint16_t two_a_pack_size = 0;
    uint16_t two_a_window_size = 0;
    uint32_t m_cmdId = 0;
    quint64 m_totalFileSize = 0;
    quint64 m_receivedSize = 0;

    typedef std::function<void(QByteArray&)> callback;
    std::map<djiFactroyCmd, callback> djifactoryCommandList;
};

#endif // DJI_BULK_DEVICE_H