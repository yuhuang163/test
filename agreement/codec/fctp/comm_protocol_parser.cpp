/*
* INCLUDE FILES
*****************************************************************************************
*/
// #include "root_common.h"
// #include "root_common_api.h" // IWYU pragma: keep
#include "comm_protocol_defs.h"
#include "comm_protocol_parser.h"

#include <QDateTime>
#include <QDebug>
#include <QString>

/*
* DEFINES
*****************************************************************************************
*/
#define PACKET_ASSEMBLY_BUFFER_SIZE 1024 ///< 环形缓冲区大小
#define PACKET_ASSEMBLY_TIMEOUT_MS 1000  ///< 组包超时时间(毫秒)

/*
* LOCAL VARIABLE DEFINITIONS
*****************************************************************************************
*/
/// 环形缓冲区结构
typedef struct {
    uint8_t buffer[PACKET_ASSEMBLY_BUFFER_SIZE]; ///< 环形缓冲区
    uint16_t write_pos;                          ///< 写位置
    uint16_t read_pos;                           ///< 读位置
    uint16_t data_size;                          ///< 已有数据大小
    qint64 last_recv_ms;                         ///< 最后接收时间（毫秒，QDateTime::currentMSecsSinceEpoch）
    bool assembling;                             ///< 是否正在组包
} packet_assembly_buffer_t;

/// 组包器全局实例
static packet_assembly_buffer_t g_packet_assembly = {0};

/*
* GLOBAL VARIABLE DEFINITIONS
*****************************************************************************************
*/

/*
* LOCAL FUNCTION DEFINITIONS
*****************************************************************************************
*/
static void packet_assembly_reset(void) {
    g_packet_assembly.write_pos = 0;
    g_packet_assembly.read_pos = 0;
    g_packet_assembly.data_size = 0;
    g_packet_assembly.last_recv_ms = 0;
    g_packet_assembly.assembling = false;
}

static int packet_assembly_write(const uint8_t* data, uint16_t len) {
    if (len > PACKET_ASSEMBLY_BUFFER_SIZE - g_packet_assembly.data_size) {
        qDebug() << "[comm_protocol] packet assembly buffer overflow, need:" << len
                 << "available:" << (PACKET_ASSEMBLY_BUFFER_SIZE - g_packet_assembly.data_size);
        return -1;
    }

    for (uint16_t i = 0; i < len; i++) {
        g_packet_assembly.buffer[g_packet_assembly.write_pos] = data[i];
        g_packet_assembly.write_pos = (g_packet_assembly.write_pos + 1) % PACKET_ASSEMBLY_BUFFER_SIZE;
    }

    g_packet_assembly.data_size += len;
    g_packet_assembly.last_recv_ms = QDateTime::currentMSecsSinceEpoch();
    return 0;
}

static int packet_assembly_read(uint8_t* buffer, uint16_t len) {
    if (len > g_packet_assembly.data_size) {
        qDebug() << "[comm_protocol] packet assembly read overflow";
        return -1;
    }

    for (uint16_t i = 0; i < len; i++) {
        buffer[i] = g_packet_assembly.buffer[g_packet_assembly.read_pos];
        g_packet_assembly.read_pos = (g_packet_assembly.read_pos + 1) % PACKET_ASSEMBLY_BUFFER_SIZE;
    }

    g_packet_assembly.data_size -= len;

    return 0;
}

static bool packet_assembly_is_timeout(void) {
    if (g_packet_assembly.data_size == 0) {
        return false;
    }
    return (QDateTime::currentMSecsSinceEpoch() - g_packet_assembly.last_recv_ms) > PACKET_ASSEMBLY_TIMEOUT_MS;
}

/*
* GLOBAL FUNCTION DEFINITIONS
*****************************************************************************************
*/
int comm_protocol_assemble_packet(const uint8_t* data, uint16_t len, comm_protocol_frame_callback_t callback,
                                  void* user_data) {
    if (!data || !callback) {
        return -1;
    }

    if (len == 0) {
        return -1;
    }

    /// 检查超时，如果超时则重置缓冲区
    if (packet_assembly_is_timeout()) {
        qDebug() << "[comm_protocol] packet assembly timeout, reset buffer";
        packet_assembly_reset();
    }

    /// 写入数据到环形缓冲区
    if (packet_assembly_write(data, len) < 0) {
        qDebug() << "[comm_protocol] failed to write data to assembly buffer";
        packet_assembly_reset();
        return -2;
    }

    /// 尝试解析完整的帧
    while (g_packet_assembly.data_size > 0) {
        /// 至少需要有Header才能解析
        if (g_packet_assembly.data_size < COMM_PROTOCOL_HEADER_SIZE) {
            g_packet_assembly.assembling = true;
            return 1; /// 需要更多数据
        }

        /// 临时缓冲区用于读取Header
        uint8_t header[COMM_PROTOCOL_HEADER_SIZE];
        uint16_t read_pos_backup = g_packet_assembly.read_pos;

        /// 读取Header（不移动读指针）
        for (uint16_t i = 0; i < COMM_PROTOCOL_HEADER_SIZE; i++) {
            header[i] = g_packet_assembly.buffer[(read_pos_backup + i) % PACKET_ASSEMBLY_BUFFER_SIZE];
        }

        /// 验证SOF
        uint16_t sof = ((uint16_t)header[1] << 8) | header[0];
        if (sof != COMM_PROTOCOL_SOF) {
            /// SOF错误，尝试在缓冲区中查找下一个有效的SOF
            qDebug() << "[comm_protocol] invalid sof:"
                     << QString::asprintf("0x%04X", sof) << "searching for next valid frame";
            /// 丢弃一个字节，继续查找
            uint8_t dummy;
            packet_assembly_read(&dummy, 1);
            continue;
        }

        /// 解析 Payload 长度（与帧头一致：byte2=seq, byte3=frame_type, byte4-5=payload 小端）
        uint16_t payload_length = (uint16_t)header[4] | ((uint16_t)header[5] << 8);

        /// 验证Payload长度
        if (payload_length < COMM_PROTOCOL_PAYLOAD_SIZE_MIN ||
            payload_length > COMM_PROTOCOL_MAX_PAYLOAD_SIZE) {
            qDebug() << "[comm_protocol] invalid payload length:" << payload_length;
            /// 丢弃一个字节，继续查找
            uint8_t dummy;
            packet_assembly_read(&dummy, 1);
            continue;
        }
        /// 计算完整帧的长度
        uint16_t frame_size = COMM_PROTOCOL_HEADER_SIZE + payload_length + COMM_PROTOCOL_FOOTER_SIZE;

        /// 检查是否接收到完整的帧
        if (g_packet_assembly.data_size < frame_size) {
            g_packet_assembly.assembling = true;
            return 1; /// 需要更多数据
        }
        /// 分配临时缓冲区存储完整帧
        uint8_t* frame_buffer = (uint8_t*)malloc(frame_size);
        if (frame_buffer == NULL) {
            qDebug() << "[comm_protocol] failed to allocate memory for frame";
            packet_assembly_reset();
            return -3;
        }

        /// 从环形缓冲区读取完整帧
        if (packet_assembly_read(frame_buffer, frame_size) < 0) {
            qDebug() << "[comm_protocol] failed to read complete frame";
            free(frame_buffer);
            packet_assembly_reset();
            return -4;
        }
        /// 验证Checksum（小端序：低字节在前，高字节在后）
        uint16_t checksum_expected = CRC16(frame_buffer, frame_size - COMM_PROTOCOL_FOOTER_SIZE);
        uint16_t checksum_received = frame_buffer[frame_size - 2] | ((uint16_t)frame_buffer[frame_size - 1] << 8);
        if (checksum_expected != checksum_received) {
            qDebug() << "[comm_protocol] checksum mismatch: expected"
                     << QString::asprintf("0x%04X", checksum_expected) << "received"
                     << QString::asprintf("0x%04X", checksum_received);
            free(frame_buffer);
            continue; /// 继续处理缓冲区中的其他数据
        }
        /// 调用回调函数传递完整帧
        callback(frame_buffer, frame_size, user_data);
        /// 释放临时缓冲区
        free(frame_buffer);
        /// 如果缓冲区已空，重置状态
        if (g_packet_assembly.data_size == 0) {
            g_packet_assembly.assembling = false;
        }
    }
    return 0; /// 1: 成功处理, 0: 需要更多数据, <0: 错误
}

int comm_protocol_frame_deserialize(const uint8_t* buffer, uint16_t buffer_size, comm_protocol_frame_node_t* frame) {
    if (!buffer || !frame) {
        return COMM_PROTOCOL_ERROR_INVALID_DATA;
    }
    /// 验证buffer长度是否合理
    if (buffer_size < COMM_PROTOCOL_FRAME_SIZE_MIN || buffer_size > COMM_PROTOCOL_FRAME_SIZE_MAX) {
        qDebug() << "[comm_protocol] buffer size error:" << buffer_size;
        return COMM_PROTOCOL_ERROR_INVALID_FRAME_SIZE;
    }
    /// 解析Header（小端序）
    frame->sof = buffer[0] | ((uint16_t)buffer[1] << 8);
    frame->seq = buffer[2];
    frame->frame_type = buffer[3];
    frame->payload_length = buffer[4] | ((uint16_t)buffer[5] << 8);
    frame->checksum = buffer[buffer_size - 2] | ((uint16_t)buffer[buffer_size - 1] << 8);
    /// 验证SOF是否正确
    if (frame->sof != COMM_PROTOCOL_SOF) {
        qDebug() << "[comm_protocol] invalid sof:" << QString::asprintf("%04X", frame->sof);
        return COMM_PROTOCOL_ERROR_INVALID_FRAME_SOF;
    }
    /// 验证Payload长度是否合理
    if (frame->payload_length < COMM_PROTOCOL_PAYLOAD_SIZE_MIN) {
        qDebug() << "[comm_protocol] payload length too small:" << frame->payload_length;
        return COMM_PROTOCOL_ERROR_INVALID_FRAME_PAYLOAD_LENGTH;
    }
    /// 验证总长度是否匹配
    if (buffer_size != COMM_PROTOCOL_HEADER_SIZE + frame->payload_length + COMM_PROTOCOL_FOOTER_SIZE) {
        qDebug() << "[comm_protocol] buffer size error:" << buffer_size
                 << "payload_length:" << frame->payload_length;
        return COMM_PROTOCOL_ERROR_INVALID_FRAME_SIZE;
    }
    /// 验证Checksum
    uint16_t checksum = CRC16(buffer, buffer_size - COMM_PROTOCOL_FOOTER_SIZE);
    if (frame->checksum != checksum) {
        qDebug() << "[comm_protocol] invalid checksum: got"
                 << QString::asprintf("%x", frame->checksum) << "expected"
                 << QString::asprintf("%x", checksum);
        return COMM_PROTOCOL_ERROR_INVALID_CRC;
    }
    /// 获取Payload指针
    const uint8_t* payload = buffer + COMM_PROTOCOL_HEADER_SIZE;
    comm_protocol_service_t* service = NULL;
    uint32_t payload_offset = 0, next_offset = 0;
    frame->payload.service_count = 0;
    for (uint8_t i = 0; i < COMM_PROTOCOL_MAX_SERVICE_NUM; i++) {
        if (payload_offset >= frame->payload_length) {
            // qDebug() << "[comm_protocol] complete deserialize, payload_length:" << frame->payload_length
            //          << "payload_offset:" << payload_offset;
            break;
        }
        service = (comm_protocol_service_t*)(payload + payload_offset);
        frame->payload.services[i].svr_id = service->svr_id;
        frame->payload.services[i].srv_length = service->srv_length;
        frame->payload.services[i].tlvs = service->tlvs; /// 零拷贝：tlvs 指向 buffer，必须保证 buffer 生命周期足够长
        frame->payload.service_count++;
        payload_offset += service->srv_length + COMM_PROTOCOL_SERVICE_HEADER_SIZE;
        // qDebug() << "[comm_protocol] payload_length:" << frame->payload_length
        //          << "payload_offset:" << payload_offset << "service_count:" << frame->payload.service_count;
    }
    return COMM_PROTOCOL_ERROR_NONE;
}