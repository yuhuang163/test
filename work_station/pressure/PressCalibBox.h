#ifndef PRESS_CALIB_BOX_H
#define PRESS_CALIB_BOX_H

#include "fixture_uart.h"
#include "hqmes.h"
#include "pressuresensorform.h"
#include "qat.h"
#include "ui_fixture_uart.h"
#include "ui_pressuresensorform.h"
#include "xwdmes.h"
#include <QMainWindow>
#include "box_base.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class PressCalibBox;
}
QT_END_NAMESPACE

class PressCalibBox : public box_base
{
    Q_OBJECT
public:
    PressCalibBox(QWidget *parent = nullptr);
    ~PressCalibBox();
    void update_main_style(QString style);
    void totally_task();

    // mes
    xwdmes *xwd_mes;
    hqmes *hq_mes;
    lxmes *lx_mes;
private:
    // mes
    QString M_USERNO;
    QString M_PASSWORD;
    QString M_MACHINENO;
    MesPacketData pack;
    QList<PressureSensorForm *> widgets;
	PressureSensorForm *PressureSensor_ui = NULL;
    Ui::PressCalibBox *ui;
    std::vector<int> get_mac_state;
    std::vector<int> get_result_numb;
    std::vector<int> get_result_state;
    std::vector<int> display_state;
    Fixture_uart *Fixture_uart_ui = NULL;   // 牙刷控制窗口

    int formRow = 1;      // 记录总共是几行几列的窗口
    int formColumn = 1;   // 记录总共是几行几列的窗口
    void saveCustom();
    void recoverCustom();

    bool isTestContinue = true;
protected:
    virtual void closeEvent(QCloseEvent *);

private slots:

    void resetall();
    bool check_all_machine_ready(FixturePacketData packet_data);
    void send_uart_data(FixturePacketData PacketData);
    void display_on_screen(int instruct, int test_state);
};
#endif   // MAINWINDOW_H
