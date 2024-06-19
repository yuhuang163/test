#ifndef IMUBOX_H
#define IMUBOX_H

#include "Abini.h"
#include "box_base.h"
#include "fixture_uart.h"
#include "imucali.h"
#include "ui_fixture_uart.h"
#include "ui_imucali.h"
#include <vector>

namespace Ui
{
    class imubox;
}

class imubox : public box_base
{
    Q_OBJECT
public:
    explicit imubox(QWidget *parent = nullptr);
    ~imubox();
private:
    Ui::imubox *ui;

    std::vector<int> testStates;
    std::vector<int> stage1States;
    std::vector<int> stage2States;
    std::vector<int> stage3States;
    std::vector<int> fixtureLeftStates;
    std::vector<int> fixtureRightStates;
    std::vector<int> fixtureDownStates;
    std::vector<int> fixtureUpStates;

    Fixture_uart *Fixture_uart_ui = NULL;   // 牙刷控制窗口

signals:
    void send_fixture_log(QString data);

private slots:
    void set_cylinder_state(imuFixtureState state);
    void check_all_over_test(int testNumber);
    bool checkstateReady(std::vector<int> States);
    void check_all_over_stage1(int testNumber);
    void check_all_over_stage2(int testNumber);
    void check_all_over_stage3(int testNumber);
    void check_all_over_fixture_left(int testNumber);
    void check_all_over_fixture_down(int testNumber);
    void check_all_over_fixture_right(int testNumber);
    void check_all_over_fixture_up(int testNumber);
    bool checkAlltestReady();
    void set_relay_state(int state);
    void resetall();
};

#endif   // IMUBOX_H
