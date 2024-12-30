#ifndef PRESS_CALIB_BOX_H
#define PRESS_CALIB_BOX_H

#include <QMainWindow>

#include "box_base.h"
#include "fixture_uart.h"
#include "hqmes.h"
#include "pressuresensorform.h"
#include "qat.h"
#include "ui_fixture_uart.h"
#include "ui_pressuresensorform.h"
#include "xwdmes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
    class PressCalibBox;
}
QT_END_NAMESPACE

class PressCalibBox : public box_base {
    Q_OBJECT
public:
    explicit PressCalibBox(QWidget* parent = nullptr);
    ~PressCalibBox();

private:
    Ui::PressCalibBox* ui;
    Fixture_uart* Fixture_uart_ui = NULL;  // 牙刷控制窗口
    QList<PressureSensorForm*> widgets;
    QList<test_base*> testList;
    bool isTestContinue = true;
    std::vector<int> display_state;
private slots:

    void resetall();
    void send_uart_data(FixturePacketData PacketData);
    void display_on_screen(int instruct);
};
#endif  // MAINWINDOW_H
