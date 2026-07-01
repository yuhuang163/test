#ifndef XZWMES_H
#define XZWMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"

/** xzw 工厂 Sunwinon MES：无单独登录，站前 groupTest + 过站 wipTest/mItemvalue。 */
class xzwmes : public Qmes {
    Q_OBJECT
  public:
    xzwmes();
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

  private:
    QString apiUrl(const QString& path) const;
};

#endif // XZWMES_H
