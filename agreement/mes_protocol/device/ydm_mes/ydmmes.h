#ifndef YDMMES_H
#define YDMMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"
class ydmmes : public Qmes {
    Q_OBJECT
  public:
    QJsonArray transformString(const QString& input);
    ydmmes();
    QString url = "";
    QString field = ""; // 字段
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;
};

#endif // YDMMES_H
