#ifndef MULTI_TEMP_LOGGER_RTU_TYPES_H
#define MULTI_TEMP_LOGGER_RTU_TYPES_H

/** 多路温度记录仪 Modbus RTU（协议 A 版）指令。 */
enum class MultiTempLoggerRtuCmd {
    /** 读指定通道温度（保持寄存器 通道温度字对）。 */
    ReadChannelTemp,
    /** 原文/十六进制整帧收发（开放报文，含 CRC）。 */
    SendRaw,
};

#endif // MULTI_TEMP_LOGGER_RTU_TYPES_H
