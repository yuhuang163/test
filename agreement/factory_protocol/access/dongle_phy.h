#ifndef DONGLE_PHY_H
#define DONGLE_PHY_H

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

#endif // DONGLE_PHY_H
