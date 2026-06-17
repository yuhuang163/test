#ifndef DONGLE_AT_TYPES_H
#define DONGLE_AT_TYPES_H

#include <QString>
#include <QVariant>
#include <QVariantMap>

/** Dongle AT 指令枚举 */
enum class DongleCmd {
    BleScanConnect,      // AT+MAC= 扫描/连接，data: MAC；全 0 表示断开
    BleDirectConnect,    // AT+DCON=
    BleOtaConnect,       // AT+OTA=
    BleAppConnect,       // AT+BLE=
    BleMainConnect,      // AT+MAIN=
    OtaDataPassthrough,  // AT+OTADATA= 0/1
    OtaPktSize,          // AT+OTAPKTSIZE= 切包字节数
    BleMtu,              // AT+BLEMTU= MTU 字节数
    MainDataPassthrough, // AT+MAINDATA= 0/1
    BleLog,              // AT+BLELOG= 0/1
    BleDeviceLog,        // AT+BLEDEVICELOG= 0/1
    Bomb,                // AT+BOMB= QVariantMap{deviceName,rssi,connectionInterval,command}
    GetGmac,             // get: AT+GMAC
};

#endif // DONGLE_AT_TYPES_H
