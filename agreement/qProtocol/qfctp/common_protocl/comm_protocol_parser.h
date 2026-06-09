#ifndef __COMM_PROTOCOL_PARSER_H__
#define __COMM_PROTOCOL_PARSER_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Include files
******************************************************************************/
#include "stdint.h" // IWYU pragma: keep
#include "comm_protocol_defs.h"

/*
* INTERNAL FUNCTION DECLARATION
*****************************************************************************************
*/

/**
 * @brief 组包接口 - 处理分片数据并组装成完整帧
 * @param data 接收到的数据片段
 * @param len 数据片段长度
 * @param callback 当组装完成时的回调函数
 * @param user_data 用户自定义数据，会传递给回调函数
 * @return 0-成功处理, 1-需要更多数据, <0-错误
 */
int comm_protocol_assemble_packet(const uint8_t* data, uint16_t len, comm_protocol_frame_callback_t callback,
                                  void* user_data);

/**
 * @brief 反序列化协议帧
 * @param buffer 帧数据缓冲区
 * @param buffer_size 缓冲区大小
 * @param frame 输出的帧节点结构
 * @return 0-成功, <0-错误码
 */
int comm_protocol_frame_deserialize(const uint8_t* buffer, uint16_t buffer_size, comm_protocol_frame_node_t* frame);

#ifdef __cplusplus
}
#endif

#endif // __COMM_PROTOCOL_PARSER_H__
