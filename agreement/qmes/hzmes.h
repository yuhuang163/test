#ifndef HZMES_H
#define HZMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"

/** 华庄 MES：HTTP GET /mrs/checkRoute|createRoute；testItem 为逗号分隔测试日志，可空串 */
class hzmes : public Qmes {
    Q_OBJECT
public:
    hzmes();
    void reloadMesConfig();
    QString url = "";
    QString field = "";  // getField 字段名，如 wifimac、prodNo
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;
};

#endif  // HZMES_H
