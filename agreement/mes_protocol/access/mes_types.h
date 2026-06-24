#ifndef MES_TYPES_H
#define MES_TYPES_H

#include <QString>
#include <QVariant>

enum class MesRoute {
    BYD,
    HQ,
    HZ,
    JJ,
    LX,
    WKS,
    XWD,
    YDM
};

enum class MesCmd {
    Login,
    GetTestData,
    ProcessInspection,
    TestPass
};

struct ProtocolMesData {
    bool success;
    int state;
    QString resultMsg;
    QVariant payload;
};

#endif // MES_TYPES_H
