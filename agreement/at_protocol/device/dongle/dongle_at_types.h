#ifndef DONGLE_AT_TYPES_H

#define DONGLE_AT_TYPES_H



#include "../../access/at_types.h"



enum class DongleCmd {

    BleScanConnect,       // AT+MAC= 扫描/连接，data: MAC；全 0 表示断开

    BleScanConnectByName, // 根据广播名称自动连接

    BleDisconnect,        // AT+MAC=00:00:00:00:00:00 主动断开当前 BLE

    BleDirectConnect,     // AT+DCON=

    BleOtaConnect,        // AT+OTA=

    BleAppConnect,        // AT+BLE=

    BleMainConnect,       // AT+MAIN=

    OtaDataPassthrough,   // AT+OTADATA= 0/1

    OtaPktSize,           // AT+OTAPKTSIZE= 切包字节数

    BleMtu,               // AT+BLEMTU= MTU 字节数

    MainDataPassthrough,  // AT+MAINDATA= 0/1

    BleLog,               // AT+BLERSSILOG= 0/1（连接后 RSSI 上报开关，旧指令 AT+BLELOG）

    GetSuction,           // AT+SUCTION= 0/1

    SetSuctionOsr,        // AT+SUCTIONOSR= 1..4（默认 4=8192X/16ms）

    AdcSwitch,            // AT+HSADC= 0/1（高量程采样）

    LowRangeAdcSwitch,    // AT+LSADC= 0/1

    BleDeviceLog,         // AT+BLEDEVICELOG= 0/1

    Bomb,                 // AT+BOMB= QVariantMap{deviceName,rssi,connectionInterval,command}

    GetGmac,              // get: AT+GMAC

    /** 双通道吸力采样（CH1/CH2，同 BYD），结果 ProtocolDongleSuctionPeakData + Gate */

    SampleSuctionDual,

    /** 单通道吸力采样，Param 可选 channel=1|2|3 */

    SampleSuctionSingle,

};



#endif // DONGLE_AT_TYPES_H


