#include "quicklz_dec.h"

#include <stdio.h>

#include <QFile>
#include <QtDebug>

#include "quicklz.h"

#define DEC_BUF_SIZE (1024)
#define FLASH_PAGE_SIZE (400)

#define DECOMPRESS_4K (4 * 1024)
#define TX_AND_DECOMPRESS_BUFFER_SIZE (0x2800)
uint8_t tx_buffer[TX_AND_DECOMPRESS_BUFFER_SIZE];

uint32_t quicklz_decompress(uint8_t* pack_addr, uint32_t pack_size, fdecompress_cb cb, read_flash_cb read_cb) {
    uint32_t counter4k = 0;
    uint32_t decompress4k = 0;
    int32_t result = 0;
    uint32_t compress_len = 0;
    uint32_t decompress_len = 0;
    uint32_t rd_pos = 0;
    uint32_t decompress_size = 0;
    uint32_t compress_size = 0;
    uint8_t* in_buffer = NULL;
    uint8_t* out_buffer = NULL;
    uint8_t* lv_share_buf = NULL;
    qlz_state_decompress* state_decompress = NULL;

    in_buffer = (uint8_t*)malloc(DEC_BUF_SIZE + FLASH_PAGE_SIZE);
    if (in_buffer == NULL) {
        qFatal("in_buffer malloc fail\n");
        goto exit;
    }

    result = 0;
    state_decompress = (qlz_state_decompress*)tx_buffer;
    lv_share_buf = tx_buffer + sizeof(qlz_state_decompress);
    out_buffer = lv_share_buf;

    memset(state_decompress, 0, sizeof(qlz_state_decompress));

    qFatal("decompress pack_addr(%p) pack_size(%08x) buf_size: %d, start...\n", pack_addr, pack_size,
           (sizeof(qlz_state_decompress) + DECOMPRESS_4K));

    while (compress_size < pack_size) {
        if ((compress_size + 9) > pack_size) {
            qFatal("head last data %d %d.\r\n", decompress4k, pack_size - compress_size);
            //          memcpy(out_buffer + decompress4k, (const void*)(pack_addr + rd_pos), pack_size - compress_size);
            read_cb((pack_addr + rd_pos), out_buffer + decompress4k, pack_size - compress_size);
            if (cb) {
                qFatal("head wr 4k %d\n", decompress4k + pack_size - compress_size);
                cb(out_buffer + decompress4k, decompress4k + pack_size - compress_size, counter4k++);
            }

            compress_size += pack_size - compress_size;
            decompress_size += pack_size - compress_size;
        } else {
            //            memcpy(in_buffer,(const void*)(pack_addr + rd_pos), 9);
            read_cb((pack_addr + rd_pos), in_buffer, 9);
            compress_len = *((uint32_t*)(in_buffer + 1));
            decompress_len = *((uint32_t*)(in_buffer + 5));
            if (decompress_len == 1024) {
                //              memcpy(in_buffer,(const void*)(pack_addr + rd_pos), compress_len);

                read_cb((pack_addr + rd_pos), in_buffer, compress_len);
                qlz_decompress((char*)in_buffer, out_buffer + decompress4k, state_decompress);
                decompress4k += decompress_len;
                if (cb && decompress4k == DECOMPRESS_4K) {
                    qFatal("wr 4k %d\n", decompress4k);
                    cb(out_buffer, decompress4k, counter4k++);
                    decompress4k = 0;
                }
                rd_pos += compress_len;
                compress_size += compress_len;
                decompress_size += decompress_len;
                qFatal("d size is %d, c size is %d.\r\n", decompress_size, compress_size);
            } else {
                qFatal("tail last data %d %d.\r\n", decompress4k, pack_size - compress_size);
                //   memcpy(out_buffer + decompress4k,(const void*)(pack_addr + rd_pos), pack_size - compress_size);
                read_cb((pack_addr + rd_pos), out_buffer + decompress4k, pack_size - compress_size);
                if (cb) {
                    qFatal("tail wr 4k %d\n", decompress4k + pack_size - compress_size);
                    cb(out_buffer, decompress4k + pack_size - compress_size, counter4k++);
                }
                decompress_size += pack_size - compress_size;
                compress_size += pack_size - compress_size;
            }
        }
    }
    result = 1;

exit:

    if (in_buffer)
        free(in_buffer);

    if (result == 1)
        qFatal("decompress pass c size is %d, d size is %d.\r\n", compress_size, decompress_size);
    else
        qFatal("decompress fail.\r\n");
    return decompress_size;
}
// 示例写回调函数
int32_t my_write_cb(uint8_t* decompress_data, uint32_t len, uint16_t packCnt) {
    // 将解压后的数据写入文件或缓冲区
    QFile file("output.dat");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(reinterpret_cast<char*>(decompress_data), len);
        file.close();
        return 0;  // 返回0表示成功
    }
    return -1;  // 返回负值表示失败
}

// 示例读回调函数
int32_t my_read_cb(uint8_t* addr, uint8_t* buf, uint32_t len) {
    // 从文件或内存中读取压缩数据
    QFile file("compressed.dat");
    if (file.open(QIODevice::ReadOnly)) {
        int32_t read_size = file.read(reinterpret_cast<char*>(buf), len);
        file.close();
        return read_size;
    }
    return -1;  // 返回负值表示失败
}
