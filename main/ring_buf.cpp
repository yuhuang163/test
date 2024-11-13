
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

    size_t readLength = length; // 确保长度不超过缓冲区大小
    if (readLength > UART_RING_BUFFER_SIZE)
    {
        Serial.print("读取失败，需要读取的太多");
        Serial.println(readLength);
        readLength = UART_RING_BUFFER_SIZE;
    }

    size_t bytesRead = 0;
    void *temp = xRingbufferReceive(ringBuffer, &bytesRead, pdMS_TO_TICKS(25));
    if (temp != NULL)
    {
        size_t copyLength = 0;
        if (bytesRead < readLength)
        {
            copyLength = bytesRead;
        }
        else
        {
            Serial.print("失败，实际读取的大于需要的");
            Serial.print(bytesRead); Serial.print(" ");
            Serial.println(readLength);

            copyLength = readLength;
        }

        memcpy(data, temp, copyLength);
        vRingbufferReturnItem(ringBuffer, temp); // 清空已经使用的缓存区
        return copyLength;
    }
    else
    {

        return 0;
    }
}

void cleanupRingBuffer()
{
    vRingbufferDelete(ringBuffer);
}