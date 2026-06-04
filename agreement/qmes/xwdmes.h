#ifndef XWDMES_H
#define XWDMES_H

#include "my_set/my_typedef.h"
#include "qmes.h"

class xwdmes : public Qmes
{
    Q_OBJECT
public:
    xwdmes();
    void LogIn(MesPacketData pack) override;
    void ProcessInspection(MesPacketData pack) override;
    void TestPass(MesPacketData pack) override;
    void GetTestData(MesPacketData pack) override;

private slots:

    void onNetworkReplyFinished();
    // 工序检验

    // 采集过站
    void collectPass(const int mechines, const QString &sn, const QString &mo,
                     const QString &userno, const QString &machineno, const QString &result,
                     const QString &itemvalue);

    // 组装过站
    void assemblePass(const int mechines, const QString &machineNo, const QString &productSN,
                      const QString &mo, const QString &emp, const QString &kpItemSnAll);
    // 测试数据离线上传
    void uploadOfflineData(const int mechines, const QString &mSn, const QString &mResult,
                           const QString &mUserno, const QString &mMachineno, const QString &mError,
                           const QString &mItemvalue);

    // 获取产品测试项
    void getSoftwareVersionBySn(const int mechines, const QString &sn);
    // 查询主件条码
    void getKeyToProduct(const int mechines, const QString &productKey, const QString &itemCode);
    // 获取配件绑定明细
    void getBindTable(const int mechines, const QString &productSN);
    // 获取绑定的产品条码
    void replaceSN(const int mechines, const QString &mo, const QString &itemSN);
    // 获取料号和制令单
    void getMaterialAndMoBySN(const int mechines, const QString &SN);
    // 获取子件料号
    void getGroupItem(const int mechines, const QString &mo, const QString &machineNo);

// signals:
//     void sendMesState(int state);                              // 信号声明
//     void operateMesSucess(const int mechines);                     // 信号声明
//     void operateMesError(const int mechines, QString resultMsg);   // 信号声明
//     void sendMesTestvalue(const int mechines, QString resultMsg);   // 信号声明
};

#endif   // XWDMES_H
