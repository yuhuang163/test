#ifndef QMES_H
#define QMES_H
#include <QString>
#include <QVector>

#include "my_set/my_typedef.h"

class Qmes : public QObject
{
    Q_OBJECT
public:
    Qmes();
    virtual void LogIn(MesPacketData pack) = 0;
    virtual void ProcessInspection(MesPacketData pack) = 0;
    virtual void TestPass(MesPacketData pack) = 0;
    virtual void GetTestData(MesPacketData pack) = 0;
protected:
    struct TestDataItem {
        QString name;
        QString value;
        QString maxValue;
        QString minValue;
        QString standardValue;
        QString unit;
        QString result;
    };

    QVector<TestDataItem> parseTestDataItems(const MesPacketData& pack) const;
signals:
    void sendMesState(int state);                                   // 信号声明
    void operateMesSucess(const int mechines);                      // 信号声明
    void operateMesError(const int mechines, QString resultMsg);    // 信号声明
    void sendMesTestvalue(const int mechines, QString resultMsg);   // 信号声明
};

#endif   // QMES_H
