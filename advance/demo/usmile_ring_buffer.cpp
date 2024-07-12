
#include "usmile_ring_buffer.h"
#include <string.h>

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif
/*
 * GLOBAL FUNCTION DEFINITIONS
 *****************************************************************************************
 */
 RingBuf::RingBuf(usmile_ring_buffer_t *p_ring_buff,  uint8_t *p_buff, uint32_t element_size, uint32_t member_size)
{

    if (NULL == p_buff || NULL == p_ring_buff)
    {
         qDebug() << "初始化环形队列失败" ;
    }
    else
    {
        p_ring_buff->member_size   = member_size;
        p_ring_buff->element_size  = element_size;
        p_ring_buff->p_buffer      = p_buff;
        p_ring_buff->write_index   = 0;
        p_ring_buff->read_index    = 0;

       // qDebug() << "初始化环形队列成功" ;
    }
}
RingBuf::~RingBuf()
{
    mutex.unlock();
    qDebug() << "Destructor called, lock released.";
}


bool RingBuf::usmile_ring_buffer_deinit(usmile_ring_buffer_t *p_ring_buff)
{
    if (NULL == p_ring_buff)
    {
        return false;
    }
    else
    {
        p_ring_buff->member_size   = 0;
        p_ring_buff->element_size  = 0;
        p_ring_buff->p_buffer      = 0;
        p_ring_buff->write_index   = 0;
        p_ring_buff->read_index    = 0;

        return true;
    }
}

uint32_t  RingBuf::usmile_ring_buffer_write(usmile_ring_buffer_t *p_ring_buff, uint8_t const *p_wr_data, uint32_t length)
{
    uint32_t surplus_space  = 0;
    uint32_t over_flow      = 0;
    uint32_t wr_idx         = 0;
    uint32_t rd_idx         = 0;



      QMutexLocker locker(&mutex);




    wr_idx = p_ring_buff->write_index;
    rd_idx = p_ring_buff->read_index;

    if (rd_idx > wr_idx)
    {
        surplus_space = rd_idx - wr_idx - 1;
        length        = (length > surplus_space ? surplus_space : length);
    }
    else
    {
        surplus_space = p_ring_buff->member_size - wr_idx + rd_idx - 1;
        length        = (length > surplus_space ? surplus_space : length);

        if (wr_idx + length >= p_ring_buff->member_size)
        {
            over_flow = wr_idx + length - p_ring_buff->member_size;
        }
    }

    memcpy(p_ring_buff->p_buffer + (wr_idx * p_ring_buff->element_size), p_wr_data, (length - over_flow) * p_ring_buff->element_size);
    memcpy(p_ring_buff->p_buffer, p_wr_data + ((length - over_flow) * p_ring_buff->element_size), over_flow * p_ring_buff->element_size);
    wr_idx += length;

    if (wr_idx >= p_ring_buff->member_size)
    {
        wr_idx -= p_ring_buff->member_size;
    }

    p_ring_buff->write_index = wr_idx;




    return length;
}

uint32_t  RingBuf::usmile_ring_buffer_read(usmile_ring_buffer_t *p_ring_buff, uint8_t *p_rd_data, uint32_t length)
{
    uint32_t items_avail = 0;
    uint32_t over_flow   = 0;
    uint32_t wr_idx      = p_ring_buff->write_index;
    uint32_t rd_idx      = p_ring_buff->read_index;



       QMutexLocker locker(&mutex);


    if (wr_idx >= rd_idx)
    {
        items_avail = wr_idx - rd_idx;
        length = (length > items_avail ? items_avail : length);
    }
    else
    {
        items_avail = p_ring_buff->member_size - rd_idx + wr_idx;
        length = (length > items_avail ? items_avail : length);

        if (rd_idx + length >= p_ring_buff->member_size)
        {
            over_flow = length + rd_idx - p_ring_buff->member_size;
        }
    }

    memcpy(p_rd_data, p_ring_buff->p_buffer + (rd_idx * p_ring_buff->element_size), (length - over_flow)*p_ring_buff->element_size);
    memcpy(p_rd_data + ((length - over_flow) * p_ring_buff->element_size), p_ring_buff->p_buffer, over_flow * p_ring_buff->element_size);
    rd_idx += length;

    if (rd_idx >= p_ring_buff->member_size && rd_idx > wr_idx)
    {
        rd_idx -= p_ring_buff->member_size;
    }

    p_ring_buff->read_index = rd_idx;



    return length;
}


uint32_t  RingBuf::usmile_ring_buffer_delete(usmile_ring_buffer_t *p_ring_buff, uint32_t length)
{
    uint32_t items_avail = 0;
    uint32_t over_flow   = 0;
    uint32_t wr_idx      = p_ring_buff->write_index;
    uint32_t rd_idx      = p_ring_buff->read_index;



       QMutexLocker locker(&mutex);

    // qDebug() << "usmile_ring_buffer_delete的rd_idx: " << rd_idx;
    // qDebug() << "usmile_ring_buffer_delete的wr_idx: " << wr_idx;
    if (wr_idx >= rd_idx)
    {
        items_avail = wr_idx - rd_idx;
        length = (length > items_avail ? items_avail : length);
    }
    else
    {
        items_avail = p_ring_buff->member_size - rd_idx + wr_idx;
        length = (length > items_avail ? items_avail : length);

        if (rd_idx + length >= p_ring_buff->member_size)
        {
            over_flow = length + rd_idx - p_ring_buff->member_size;
        }
    }

    rd_idx += length;

    if (rd_idx >= p_ring_buff->member_size && rd_idx > wr_idx)
    {
        rd_idx -= p_ring_buff->member_size;
    }

    p_ring_buff->read_index = rd_idx;



    return length;
}



uint32_t  RingBuf::usmile_ring_buffer_pick(usmile_ring_buffer_t *p_ring_buff, uint8_t *p_rd_data, uint32_t length)
{
    uint32_t items_avail = 0;
    uint32_t over_flow   = 0;
    uint32_t wr_idx      = p_ring_buff->write_index;
    uint32_t rd_idx      = p_ring_buff->read_index;



    QMutexLocker locker(&mutex);


    if (wr_idx >= rd_idx)
    {
        items_avail = wr_idx - rd_idx;
        length = (length > items_avail ? items_avail : length);
    }
    else
    {
        items_avail = p_ring_buff->member_size - rd_idx + wr_idx;
        length = (length > items_avail ? items_avail : length);

        if (rd_idx + length >= p_ring_buff->member_size)
        {
            over_flow = length + rd_idx - p_ring_buff->member_size;
        }
    }

    // qDebug() << "rd_idx: " << rd_idx;
    // qDebug() << "wr_idx: " << wr_idx;
    // qDebug() << "p_ring_buff->element_size: " << p_ring_buff->element_size;
    // qDebug() << "length: " << length;
    // qDebug() << "over_flow: " << over_flow;
    // qDebug() << "Source address range: " << reinterpret_cast<void*>(p_ring_buff->p_buffer) << " - " << reinterpret_cast<void*>(p_ring_buff->p_buffer + (rd_idx * p_ring_buff->element_size));
    // qDebug() << "Destination address range: " << reinterpret_cast<void*>(p_rd_data) << " - " << reinterpret_cast<void*>(p_rd_data + (length - over_flow)*p_ring_buff->element_size);

    memcpy(p_rd_data, p_ring_buff->p_buffer + (rd_idx * p_ring_buff->element_size), (length - over_flow)*p_ring_buff->element_size);
    memcpy(p_rd_data + ((length - over_flow) * p_ring_buff->element_size), p_ring_buff->p_buffer, over_flow * p_ring_buff->element_size);



    return length;
}
uint32_t  RingBuf::usmile_ring_buffer_items_count_get(usmile_ring_buffer_t *p_ring_buff)

{
    uint32_t size = 0;
    uint32_t wr_idx = 0;
    uint32_t rd_idx = 0;



       QMutexLocker locker(&mutex);


    wr_idx = p_ring_buff->write_index;
    rd_idx = p_ring_buff->read_index;

    // qDebug() << "rd_idx: " << rd_idx;
    // qDebug() << "wr_idx: " << wr_idx;


    if (rd_idx <= wr_idx)
    {
        size = wr_idx - rd_idx;
    }
    else
    {
        size = p_ring_buff->member_size - rd_idx + wr_idx;
    }



    return size;
}

uint32_t  RingBuf::usmile_ring_buffer_surplus_space_get(usmile_ring_buffer_t *p_ring_buff)
{
    uint32_t surplus_space = 0;
    uint32_t wr_idx = 0;
    uint32_t rd_idx = 0;



       QMutexLocker locker(&mutex);
;
    wr_idx = p_ring_buff->write_index;
    rd_idx = p_ring_buff->read_index;

    if (rd_idx > wr_idx)
    {
        surplus_space = rd_idx - wr_idx - 1;
    }
    else
    {
        surplus_space = p_ring_buff->member_size - wr_idx + rd_idx - 1;
    }

    ;

    return surplus_space;
}

bool  RingBuf::usmile_ring_buffer_is_reach_left_threshold(usmile_ring_buffer_t *p_ring_buff, uint32_t letf_threshold)
{
    uint32_t surplus_space;

    surplus_space = usmile_ring_buffer_surplus_space_get(p_ring_buff);

    if (letf_threshold >= surplus_space)
    {
        return true;
    }
    else
    {
        return false;
    }
}

void  RingBuf::usmile_ring_buffer_clean(usmile_ring_buffer_t *p_ring_buff)
{


       QMutexLocker locker(&mutex);


    p_ring_buff->write_index = 0;
    p_ring_buff->read_index  = 0;


}


int  RingBuf::usmile_ring_buffer_get_last_wr_index(usmile_ring_buffer_t *p_ring_buff)
{
    int size = 0;



       QMutexLocker locker(&mutex);


    if (p_ring_buff->write_index == p_ring_buff->read_index) {

        return -1;
    }

    if(p_ring_buff->write_index == 0) {
        size = p_ring_buff->member_size - 1;
    } else {
        size = p_ring_buff->write_index - 1;
    }


    return size;
}


int  RingBuf::usmile_ring_buffer_get_rd_index(usmile_ring_buffer_t *p_ring_buff)
{
    return p_ring_buff->read_index;
}

void RingBuf::LOGE(const char *data) {
    qDebug()<<data;
}
