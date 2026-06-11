
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

static size_t bufferReadInternal(byte *data, size_t length, TickType_t waitTicks, bool quiet)
{
    size_t readLength = length;
    if (readLength > UART_RING_BUFFER_SIZE)
    {
        if (!quiet)
        {
            Serial.print("读取失败，需要读取的太多");
            Serial.println(readLength);
        }
        readLength = UART_RING_BUFFER_SIZE;
    }

    size_t bytesRead = 0;
    void *temp = xRingbufferReceive(ringBuffer, &bytesRead, waitTicks);
    if (temp == NULL)
    {
        return 0;
    }

    size_t copyLength = bytesRead < readLength ? bytesRead : readLength;
    if (!quiet && bytesRead > readLength)
    {
        Serial.print("失败，实际读取的大于需要的");
        Serial.print(bytesRead);
        Serial.print(" ");
        Serial.println(readLength);
    }

    memcpy(data, temp, copyLength);
    vRingbufferReturnItem(ringBuffer, temp);
    return copyLength;
}

size_t bufferRead(byte *data, size_t length)
{
    return bufferReadInternal(data, length, pdMS_TO_TICKS(25), false);
}

size_t bufferReadOta(byte *data, size_t length)
{
    return bufferReadInternal(data, length, pdMS_TO_TICKS(OTA_UART_READ_WAIT_MS), true);
}

void cleanupRingBuffer()
{
    vRingbufferDelete(ringBuffer);
}