#ifndef BULK_TYPES_H
#define BULK_TYPES_H

#include <cstdint>

#define USB_ERROR_IO -1
#define USB_ERROR_INVALID_PARAM -2
#define USB_ERROR_ACCESS -3
#define USB_ERROR_NO_DEVICE -4
#define USB_ERROR_NOT_FOUND -5
#define USB_ERROR_NOT_DATA -116

#define DUSS_MB_PACKAGE_V1_CRCH_INIT 0x77
#define DUSS_MB_PACKAGE_V1_CRC_INIT 0x3692
#define HOST_ID_FROM_16BIT_TO_8BIT(host_id) \
    (((((host_id) & 0x07) << 5) | (((host_id) >> 8) & 0x1F)) & 0xFF)
#define HOST_ID_FROM_8BIT_TO_16BIT(host_id) \
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
    djiFactroyCmd_sec_dbg_command_req_handler = 0xe1,
    djiFactroyCmd_sec_dbg_command_auth_handler = 0xe2,
    djiFactroyCmd_set_sn_operate = 0x50,
    djiFactroyCmd_get_sn_operate = 0x51,

    //djisys 0801
    djiFactroyCmd_sys_event_reboot = 0x0b,
    djiFactroyCmd_sys_event_report_status = 0x0c,

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

struct DjiBulkFrame {
    uint8_t receiver;
    uint8_t cmdType;
    uint8_t cmdSet;
    uint8_t cmdID;
    uint8_t encryptionType;
    uint8_t isResponse;
    QByteArray data;
};

#endif // BULK_TYPES_H
