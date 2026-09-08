#ifndef PRE_START_MONITOR_CONFIG_H
#define PRE_START_MONITOR_CONFIG_H

#include <QString>

struct PreStartMonitorConfig {
    bool enabled = false;
    QString plcDevice;
    QString plcIp = "127.0.0.1";
    int plcPort = 502;
    QString plcComPort;
    int plcBaudRate = 19200;
    int plcSlaveId = 1;
    QString plcWaitAddress = "M5";
    int plcWaitAddressM = 5;
    int plcPollIntervalMs = 500;
    QString scannerIp = "192.168.1.64";
    int scannerPort = 2001;
    int scannerTimeoutMs = 1000;
    bool autoIncrementIpByStation = true;
};

#endif // PRE_START_MONITOR_CONFIG_H
