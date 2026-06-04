#ifndef __USMILIE_RING_BUFFER_H__
#define __USMILIE_RING_BUFFER_H__

/*
 * INCLUDE FILES
 *****************************************************************************************
 */
#include <stdint.h>
#include <stdbool.h>
#include "qdebug.h"
#include <QMutex>
#include <QMutexLocker>
#include <QMessageBox>
#include <QObject>
#include <QQueue>
typedef struct
{
    uint32_t             member_size;           /**< 成员的大小。 */
    uint32_t             element_size;          //!< 一个元素的大小
    uint8_t              *p_buffer;              /**< 指向保存数据的缓冲区的指针，大小为 member_size * element_size */
    uint32_t             write_index;           /**< 写入索引。 */
    uint32_t             read_index;            /**< 读取索引。 */
} usmile_ring_buffer_t;
class RingBuf: public QObject
{
   Q_OBJECT

public:
     RingBuf(usmile_ring_buffer_t *p_ring_buff,  uint8_t *p_buff, uint32_t element_size, uint32_t member_size);
      ~RingBuf();
    void LOGE(const char *data);
 QMutex mutex;
    /**
 *****************************************************************************************
 * @brief 初始化环形缓冲区。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区结构体的指针。
 * @param[in] p_buff:      指向保存数据的缓冲区的指针。
 * @param[in] buff_size:   缓冲区的大小。
 *
 * @return 初始化环形缓冲区的结果。
 *****************************************************************************************
 */
    bool usmile_ring_buffer_init(usmile_ring_buffer_t *p_ring_buff, QMutex *lock, uint8_t *p_buff, uint32_t element_size, uint32_t member_size);

    /**
 *****************************************************************************************
 * @brief 反初始化环形缓冲区。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区结构体的指针。
 *
 * @return 反初始化环形缓冲区的结果。
 *****************************************************************************************
 */

    bool usmile_ring_buffer_deinit(usmile_ring_buffer_t *p_ring_buff);

    /**
 *****************************************************************************************
 * @brief 写入数据到环形缓冲区。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区的指针。
 * @param[in] p_wr_data:   指向需要写入的数据的指针。
 * @param[in] length:      需要写入的数据长度。
 *
 * @return 写入的数据长度。
 *****************************************************************************************
 */
    uint32_t usmile_ring_buffer_write(usmile_ring_buffer_t *p_ring_buff, uint8_t const *p_wr_data, uint32_t length);


    /**
 *****************************************************************************************
 * @brief 从环形缓冲区删除数据。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区的指针。
 * @param[in] length:      需要删除的数据长度。
 *
 * @return 删除的数据长度。
 *****************************************************************************************
 */
    uint32_t usmile_ring_buffer_delete(usmile_ring_buffer_t *p_ring_buff, uint32_t length);


    /**
 *****************************************************************************************
 * @brief 从环形缓冲区读取数据。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区的指针。
 * @param[in] p_rd_data:   指向保存读取数据的缓冲区的指针。
 * @param[in] length:      想要读取的数据长度。
 *
 * @return 可用的读取数据长度。
 *****************************************************************************************
 */
    uint32_t usmile_ring_buffer_read(usmile_ring_buffer_t *p_ring_buff, uint8_t *p_rd_data, uint32_t length);

    /**
 *****************************************************************************************
 * @brief 从环形缓冲区挑选数据。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区的指针。
 * @param[in] p_rd_data:   指向保存挑选数据的缓冲区的指针。
 * @param[in] length:      想要读取的数据长度。
 *
 * @return 可用的挑选数据长度。
 *****************************************************************************************
 */
    uint32_t usmile_ring_buffer_pick(usmile_ring_buffer_t *p_ring_buff, uint8_t *p_rd_data, uint32_t length);
    /**
 *****************************************************************************************
 * @brief 获取环形缓冲区的剩余空间。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区的指针。
 *
 * @retval  剩余空间的大小。
 *****************************************************************************************
 */
    uint32_t usmile_ring_buffer_surplus_space_get(usmile_ring_buffer_t *p_ring_buff);

    /**
 *****************************************************************************************
 * @brief 获取环形缓冲区中可用数据的长度。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区的指针。
 *
 * @retval  可用数据的长度。
 *****************************************************************************************
 */
    uint32_t usmile_ring_buffer_items_count_get(usmile_ring_buffer_t *p_ring_buff);

    /**
 *****************************************************************************************
 * @brief 清除环形缓冲区。
 *
 * @param[in] p_ring_buff: 指向环形缓冲区结构体的指针。
 *****************************************************************************************
 */
    void usmile_ring_buffer_clean(usmile_ring_buffer_t *p_ring_buff);

    /**
 *****************************************************************************************
 * @brief  检查环形缓冲区是否接近满。
 *
 * @param[in] p_ring_buff:    指向环形缓冲区的指针。
 * @param[in] left_threshold: 剩余空间的阈值。
 *
 * @return 缓冲区接近满的状态。
 *****************************************************************************************
 */
    bool usmile_ring_buffer_is_reach_left_threshold(usmile_ring_buffer_t *p_ring_buff, uint32_t letf_threshold);
    /** @} */


    int usmile_ring_buffer_get_last_wr_index(usmile_ring_buffer_t *p_ring_buff);

    int usmile_ring_buffer_get_rd_index(usmile_ring_buffer_t *p_ring_buff);
};




#endif
