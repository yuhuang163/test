#ifndef WKSMES_H
#define WKSMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"
class wksmes : public Qmes {
    Q_OBJECT
public:
    QString transformString(const QString& input);
    wksmes();
    QString url = "";
    QString field = "";  // 字段
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;
};

#endif  // WKSMES_H
