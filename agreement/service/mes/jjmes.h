#ifndef JJMES_H
#define JJMES_H
#include "my_set/my_typedef.h"
#include "qmes.h"
class jjmes : public Qmes
{
    Q_OBJECT
public:
    jjmes();
    QString url = "";
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

// signals:
//     void send_jjmes_mes_state(int state);                               // 信号声明
//     void sendMesTestvalue(const int mechines, QString resultMsg);   // 信号声明
//     void operateMesSucess(const int mechines);                      // 信号声明
//     void operateMesError(const int mechines, QString resultMsg);    // 信号声明
};

#endif   // JJMES_H
