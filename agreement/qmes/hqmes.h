#ifndef HQMES_H
#define HQMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"

class hqmes : public Qmes
{
    Q_OBJECT
public:
    hqmes();
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

    void MesGetGlobalInfo_hq();
private:
    QString encryption(QString passsword);
    typedef QString (*EncryptFunction)(const QString &);
    QString G_TOKEN;
    void getLocalInfo();
    QString IP_Adress;
    QString MAC_Adress;
    QString hostName;

private slots:
    void onNetworkReplyFinished();

// signals:
//     void sendMesState(int state);                                   // 信号声明
//     void operateMesSucess(const int mechines);                      // 信号声明
//     void operateMesError(const int mechines, QString resultMsg);    // 信号声明
//     void sendMesTestvalue(const int mechines, QString resultMsg);   // 信号声明
};

#endif   // HQMES_H
