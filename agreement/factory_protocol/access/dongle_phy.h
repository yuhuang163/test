#ifndef DONGLE_PHY_H
#define DONGLE_PHY_H

#include <QByteArray>
#include <QtGlobal>

/** Dongle 串口 PHY 外层帧常量（见 docs/协议文档/dongle协议.md §1.1、§1.2） */
constexpr quint8 kDonglePhyRxHeaderByte = 0xAA; // Dongle → 上位机
constexpr quint8 kDonglePhyTxHeaderByte = 0xCC; // 上位机 → Dongle
constexpr int kDonglePhyHeaderSize = 8;

constexpr quint8 kDonglePhyChannelFac = 1;     // 产测 / qroot / qfctp 工厂
constexpr quint8 kDonglePhyChannelApp = 2;     // qfctp App 透传
constexpr quint8 kDonglePhyChannelMain = 3;    // qfctp Main 透传
constexpr quint8 kDonglePhyChannelSuction = 4; // 三路吸力二进制上行（DonglePhyRxCodec）

/** DonglePhyRxCodec 收包 channel 位掩码（bit N 表示允许 channel N） */
constexpr quint8 kDonglePhyRxAcceptFacOnly = quint8(1u << kDonglePhyChannelFac);
constexpr quint8 kDonglePhyRxAcceptFacAppMain =
    quint8((1u << kDonglePhyChannelFac) | (1u << kDonglePhyChannelApp) | (1u << kDonglePhyChannelMain));

/** 吸力/温度高频流式数据：跳过串口 UI 与逐帧落盘（含 AT 文本与 PHY channel=4 二进制） */
inline bool shouldSkipDongleStreamUartLog(const QByteArray& data) {
    if (data.isEmpty())
        return false;
    if (data.contains("AT+TEMP_DATA") || data.contains("AT+SUCTION_DATA"))
        return true;

    const int minFrame = kDonglePhyHeaderSize + 2; // channel + len
    if (data.size() >= minFrame) {
        for (int i = 0; i <= data.size() - minFrame; ++i) {
            bool headerOk = true;
            for (int j = 0; j < kDonglePhyHeaderSize; ++j) {
                if (static_cast<quint8>(data.at(i + j)) != kDonglePhyRxHeaderByte) {
                    headerOk = false;
                    break;
                }
            }
            if (headerOk && static_cast<quint8>(data.at(i + kDonglePhyHeaderSize)) == kDonglePhyChannelSuction)
                return true;
        }
    }

    // 分片到达时可能尚无完整帧头+channel，但已是纯二进制吸力流
    if (!data.contains(QByteArrayLiteral("AT"))
        && static_cast<quint8>(data.at(0)) == kDonglePhyRxHeaderByte)
        return true;
    return false;
}

#endif // DONGLE_PHY_H
