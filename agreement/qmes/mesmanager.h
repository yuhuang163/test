#ifndef MESMANAGER_H
#define MESMANAGER_H
#include <bydmes.h>  // 比亚迪的mes头文件
#include <hqmes.h>   // 华勤的mes头文件
#include <jjmes.h>   // 金进的mes头文件
#include <lxmes.h>   // 立讯的mes头文件
#include <wksmes.h>  // 伟克森的mes头文件
#include <xwdmes.h>  // 欣旺达的mes头文件

#include <vector>

#include "my_set/my_typedef.h"
#include "ydmmes.h"
class QMesManager : public QObject {
    Q_OBJECT
public:
    QMesManager();

public slots:
    void loginAll(MesPacketData pack);
    void GetTestDataAll(MesPacketData pack);
    void ProcessInspectionAll(MesPacketData pack);
    void TestPassAll(MesPacketData pack);
    void handleMesState(int state);                                  // 信号声明
    void handleMesSucess(const int mechines);                        // 信号声明
    void handleMesError(const int mechines, QString resultMsg);      // 信号声明
    void handleMesTestvalue(const int mechines, QString resultMsg);  // 信号声明

signals:
    void MesState(int state);                                  // 信号声明
    void MesSucess(const int mechines);                        // 信号声明
    void MesError(const int mechines, QString resultMsg);      // 信号声明
    void MesTestvalue(const int mechines, QString resultMsg);  // 信号声明
private:
    bydmes BydMes;
    jjmes JjMes;
    xwdmes XwdMes;
    lxmes LxMes;
    hqmes HqMes;
    wksmes WksMes;
    ydmmes YdmMes;
    std::vector<Qmes*> MesSystems;
};

#endif  // MESMANAGER_H
