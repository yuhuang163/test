
#include "config.h"

RingbufHandle_t ringBuffer;

void initRingBuffer()
{
    ringBuffer = xRingbufferCreate(UART_RING_BUFFER_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (ringBuffer == NULL)
    {
        Serial.println("创建环形缓冲区失败");
    }
}

void bufferWrite(const byte *data, size_t length)
{
    // 确保长度不超过缓冲区大小
    size_t writeLength = length;
    if (writeLength > UART_RING_BUFFER_SIZE)
    {
        writeLength = UART_RING_BUFFER_SIZE;
    }

    size_t bytesWritten = xRingbufferSend(ringBuffer, data, writeLength, pdMS_TO_TICKS(500));

    if (!bytesWritten)
    {
        Serial.print("写入数据失败,大小为");
        Serial.println(writeLength);
    }
}

size_t bufferRead(byte *data, size_t length)
{
    // 确保长度不超过缓冲区大小
    size_t readLength = length;
    if (readLength > UART_RING_BUFFER_SIZE)
    {
        readLength = UART_RING_BUFFER_SIZE;
    }

    size_t bytesRead = 0;
    void *temp = xRingbufferReceive(ringBuffer, &bytesRead, pdMS_TO_TICKS(500));
    if (temp != NULL)
    {
        size_t copyLength = (bytesRead < readLength) ? bytesRead : readLength;
        memcpy(data, temp, copyLength);
        vRingbufferReturnItem(ringBuffer, temp);
        return copyLength;
    }
    // else
    // {
    //     Serial.println("读取环形队列数据失败");
    // }

    return 0;
}

void cleanupRingBuffer()
{
    vRingbufferDelete(ringBuffer);
}