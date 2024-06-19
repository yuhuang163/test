#ifndef LXMES_H
#define LXMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"
class lxmes : public Qmes
{
    Q_OBJECT
public:
    lxmes();
    QString url = "";
    QString field = "";   // 字段
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

// signals:
//     void send_lxmes_state(int state);                                   // 信号声明
//     void operateMesSucess(const int mechines);                      // 信号声明
//     void operateMesError(const int mechines, QString resultMsg);    // 信号声明
//     void send_lxmes_testvalue(const int mechines, QString resultMsg);   // 信号声明
};

#endif   // LXMES_H
