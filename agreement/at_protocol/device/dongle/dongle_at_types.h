#ifndef DONGLE_AT_TYPES_H
#define DONGLE_AT_TYPES_H

#include "../../access/at_types.h"

enum class DongleCmd {
    BleScanConnect,      // AT+MAC= 扫描/连接，data: MAC；全 0 表示断开
    BleScanConnectByName, // 根据广播名称自动连接
    BleDirectConnect,    // AT+DCON=
    BleOtaConnect,       // AT+OTA=
    BleAppConnect,       // AT+BLE=
    BleMainConnect,      // AT+MAIN=
    OtaDataPassthrough,  // AT+OTADATA= 0/1
    OtaPktSize,          // AT+OTAPKTSIZE= 切包字节数
    BleMtu,              // AT+BLEMTU= MTU 字节数
    MainDataPassthrough, // AT+MAINDATA= 0/1
    BleLog,              // AT+BLELOG= 0/1
    GetSuction,          // AT+SUCTION= 0/1
    AdcSwitch,           // AT+ADC= 0/1
    BleDeviceLog,        // AT+BLEDEVICELOG= 0/1
    Bomb,                // AT+BOMB= QVariantMap{deviceName,rssi,connectionInterval,command}
    GetGmac,             // get: AT+GMAC
};

#endif // DONGLE_AT_TYPES_H
