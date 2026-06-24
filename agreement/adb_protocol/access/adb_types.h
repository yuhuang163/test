#ifndef ADB_TYPES_H
#define ADB_TYPES_H

#include <QString>
#include <QVariant>

enum class AdbCmd {
    StartServer,
    StopServer,
    PushFile,
    PullFile,
    ShellExecute,
    StartKeyMonitor,
    StopKeyMonitor
};

struct AdbDeviceRoute {
    QString serialNumber;
};

struct ProtocolAdbData {
    bool success;
    QString output;
    QVariant payload;
};

#endif // ADB_TYPES_H
