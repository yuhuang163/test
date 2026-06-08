#ifndef PLC_STATION_CONFIG_H
#define PLC_STATION_CONFIG_H

#include <QtGlobal>
#include <QString>

/** 按工位从 SETTINGS 解析汇川 PLC Modbus 线圈地址与连接参数。 */
struct PlcStationConfig {
    int stationIndex = 1;
    QString ipAddress;
    int port = 502;
    quint8 unitId = 1;
    int mCoilAddressOffset = 0;
    int mBase = 200;
    int switchForwardM = 0;
    int switchPressM = 0;
    int connectVerifyM = 0;
    int positionReadyBase = 0;
    int stepDoneBase = 0;
    int keyDoneM = 0;
    int switchTestDoneResetM = 211;

    static PlcStationConfig fromSettings(int stationIndex);
};

QString plcBoolText(bool on);

#endif  // PLC_STATION_CONFIG_H
