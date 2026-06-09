#ifndef __COMM_PROTOCOL_DEFS_H__
#define __COMM_PROTOCOL_DEFS_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Include files
******************************************************************************/
#include <stdint.h>
#include <stdio.h>

/* 头文件内实现：用 static inline，避免多个 .cpp 包含本头时出现 LNK2005 重复定义 */
static inline unsigned short crc16_compute(unsigned char const *p_data, unsigned int size,
                                           unsigned short const *p_crc)
{
    unsigned short crc = (p_crc == NULL) ? 0xFFFFu : *p_crc;
    uint32_t       i;
    for (i = 0; i < size; i++) {
        crc = (unsigned char)(crc >> 8) | (crc << 8);
        crc ^= p_data[i];
        crc ^= (unsigned char)(crc & 0xFF) >> 4;
        crc ^= (crc << 8) << 4;
        crc ^= ((crc & 0xFF) << 4) << 1;
    }
    return crc;
}

static inline void hex_to_ascii(const unsigned char *hex, int len, char *out)
{
    int i;
    for (i = 0; i < len; i++) {
        sprintf(out + i * 2, "%02X", hex[i]);
    }
}
/// 计算CRC16.
#define CRC16(data, len)            crc16_compute((const uint8_t *)(data), len, NULL)

/// CRC16 增量更新（用于分块计算 / streaming）。
#define CRC16_UPDATE(crc, data, len) \
do { \
        unsigned short __crc_prev = (unsigned short)(crc); \
        (crc) = (unsigned short)crc16_compute((const uint8_t *)(data), (len), &__crc_prev); \
} while (0)


/// 协议常量定义
#define COMM_PROTOCOL_SOF                 0x5CC5             ///< Start Of Frame 标识
#define COMM_PROTOCOL_HEADER_SIZE         6                  ///< Header大小: SOF(2) + Seq(1) + FrameType(1) + Length(2)
#define COMM_PROTOCOL_FOOTER_SIZE         2                  ///< Footer大小: Checksum(2)
#define COMM_PROTOCOL_SERVICE_HEADER_SIZE 4                  ///< Service Header: SvrID(2) + Length(2)
#define COMM_PROTOCOL_TLV_HEADER_SIZE     4                  ///< TLV Header: Type(2) + Length(2)
#define COMM_PROTOCOL_SERVICE_SIZE_MIN    (COMM_PROTOCOL_SERVICE_HEADER_SIZE + \
                        COMM_PROTOCOL_TLV_HEADER_SIZE)       ///< 单个Service最小长度
#define COMM_PROTOCOL_PAYLOAD_SIZE_MIN    (COMM_PROTOCOL_HEADER_SIZE + \
                        COMM_PROTOCOL_FOOTER_SIZE)           ///< Payload最小长度
#define COMM_PROTOCOL_MAX_PAYLOAD_SIZE    1024               ///< 最大Payload长度
#define COMM_PROTOCOL_FRAME_SIZE_MAX      (COMM_PROTOCOL_MAX_PAYLOAD_SIZE + \
                        COMM_PROTOCOL_HEADER_SIZE +\
                        COMM_PROTOCOL_FOOTER_SIZE)           ///< 帧最大长度
#define COMM_PROTOCOL_FRAME_SIZE_MIN    (COMM_PROTOCOL_HEADER_SIZE + \
                        COMM_PROTOCOL_SERVICE_SIZE_MIN + \
                        COMM_PROTOCOL_FOOTER_SIZE)           ///< 帧最小长度

/// 协议最大长度限制
#define COMM_PROTOCOL_MAX_SERVICE_NUM     16                 ///< 单帧最大Service数量
#define COMM_PROTOCOL_MAX_TLV_NUM         32                 ///< 单Service最大TLV数量
#define COMM_PROTOCOL_MAX_TLV_VALUE_SIZE  256                ///< 单个TLV最大Value长度

/// TLV结构定义
typedef struct {
    uint16_t type;          ///< Type ID
    uint16_t length;        ///< Value数据长度
    uint8_t  value[0];       ///< 柔性数组成员，实际数据（动态长度）
} comm_protocol_tlv_t;

/// TLV节点（用于链表或数组管理）
typedef struct {
    uint16_t type;          ///< Type ID
    uint16_t length;        ///< Value数据长度
    uint8_t  *value;        ///< 指向Value数据的指针
} comm_protocol_tlv_node_t;

/// Service结构定义（带柔性数组）
typedef struct {
    uint16_t svr_id;        ///< Service ID
    uint16_t srv_length;    ///< Service数据长度（包含所有TLV的总长度）
    uint8_t  tlvs[0];        ///< 柔性数组成员，存储多个TLV包的数据
} comm_protocol_service_t;

/// Service节点（用于动态管理多个TLV）
typedef struct {
    uint16_t svr_id;                                           ///< Service ID
    uint16_t srv_length;                                       ///< Service数据长度
    uint8_t *tlvs;                                             ///< TLV数组
} comm_protocol_service_node_t;

/// Payload节点（用于动态管理多个Service）
typedef struct {
    uint16_t service_count;                                         ///< Service数量
    comm_protocol_service_node_t services[COMM_PROTOCOL_MAX_SERVICE_NUM]; ///< Service数组
} comm_protocol_payload_node_t;

/// 完整的通讯帧结构（用于数据流解析）
typedef struct {
    uint16_t sof;             ///< Start Of Frame: 0x5CC5
    uint8_t seq;             ///< 帧序列号
    uint8_t frame_type;      ///< 帧类型 ack/nack/request/response
    uint16_t payload_length; ///< Payload字段的长度（不包含Header和Footer）
    uint8_t  payload[0];      ///< 柔性数组成员，Payload数据
    // uint16_t checksum;      ///< CRC16校验和，覆盖Header和Payload域
} comm_protocol_frame_t;

/// 完整的通讯帧节点（用于构建和解析）
typedef struct {
    uint16_t sof;           ///< Start Of Frame: 0x5CC5
    uint8_t seq;             ///< 帧序列号
    uint8_t frame_type;      ///< 帧类型 request/response/notify
    uint16_t payload_length;        ///< Payload字段的长度（不包含Header和Footer）
    comm_protocol_payload_node_t  payload;    ///< Payload节点
    uint16_t checksum;      ///< CRC16校验和，覆盖Header和Payload域
} comm_protocol_frame_node_t;

/// 协议帧构建器
typedef struct {
    uint16_t  payload_size;      /// 帧总大小
    comm_protocol_frame_t *frame;
} comm_protocol_builder_t;

/// 服务节点包装器
typedef struct {
    comm_protocol_builder_t *builder;      /// 协议帧构建器
    comm_protocol_service_t *service;        /// 服务帧
} comm_protocol_service_node_wrapper_t;

/// 帧类型
typedef enum {
    COMM_PROTOCOL_FRAME_TYPE_REQUEST = 0,       ///< 请求帧
    COMM_PROTOCOL_FRAME_TYPE_RESPONSE,      ///< 响应帧
    COMM_PROTOCOL_FRAME_TYPE_NOTIFY,        ///< 通知帧
    COMM_PROTOCOL_FRAME_TYPE_MAX,
} comm_protocol_frame_type_e;

/// 服务ID
typedef enum {
    COMM_PROTOCOL_SERVICE_ID_NONE = 0,
    COMM_PROTOCOL_ALGO_SERVICE = 1,
    COMM_PROTOCOL_SENSORS_SERVICE = 2,
    COMM_PROTOCOL_BREAST_PUMP_SERVICE = 3,
    COMM_PROTOCOL_IOT_SERVICE = 4,
    COMM_PROTOCOL_SYSTEM_CONFIG_SERVICE = 5,
    COMM_PROTOCOL_SYSTEM_STATE_SERVICE = 6,
    COMM_PROTOCOL_TESTS_SERVICE = 7,
    COMM_PROTOCOL_TRACKING_DATA_SERVICE = 8,
} comm_protocol_service_id_e;

/// 错误码
typedef enum {
    COMM_PROTOCOL_ERROR_NONE = 0,
    COMM_PROTOCOL_ERROR_INVALID_FRAME_SIZE,
    COMM_PROTOCOL_ERROR_INVALID_FRAME_SOF,
    COMM_PROTOCOL_ERROR_INVALID_FRAME_PAYLOAD_LENGTH,
    COMM_PROTOCOL_ERROR_INVALID_CRC,
    COMM_PROTOCOL_ERROR_INVALID_SERVICE_ID,
    COMM_PROTOCOL_ERROR_INVALID_SERVICE_LENGTH,
    COMM_PROTOCOL_ERROR_INVALID_TLV_ID,
    COMM_PROTOCOL_ERROR_INVALID_TLV_LENGTH,
    COMM_PROTOCOL_ERROR_INVALID_SERVICE_COUNT, /// 服务数量不合法
    COMM_PROTOCOL_ERROR_INVALID_BUILDER,
    COMM_PROTOCOL_ERROR_INVALID_TLV_FUNC,
    COMM_PROTOCOL_ERROR_INVALID_SIZE,
    COMM_PROTOCOL_ERROR_INVALID_DATA,
    COMM_PROTOCOL_ERROR_INVALID_RSP_SERVICE,
} comm_protocol_error_code_t;

/// 描述协议帧服务error code tlv id
#define PROTOCOL_SERVICE_ERROR_CODE_TLV_ID         (0xF000)

/// 8.2.1 产测模式 TLV Type
#define COMM_PROTOCOL_TLV_TYPE_FACTORY_TEST_MODE   (0x0001u)


/// 协议帧回调函数类型（user_data 由 comm_protocol_assemble_packet 传入）
typedef void (*comm_protocol_frame_callback_t)(const uint8_t *frame_data, uint16_t frame_len, void *user_data);

#ifdef __cplusplus
}
#endif
#endif // __COMM_PROTOCOL_DEFS_H__ 