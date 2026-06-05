#ifndef QFREEWORKBOX_H
#define QFREEWORKBOX_H

#include "box_base.h"
#include "fixture_uart.h"
#include "ui_fixture_uart.h"

namespace Ui {
    class QFreeWorkBox;
}

class QFreeWorkBox : public box_base {
    Q_OBJECT

public:
    explicit QFreeWorkBox(QWidget* parent = nullptr);
    ~QFreeWorkBox();

    Ui::QFreeWorkBox* ui;
    /** 治具串口调试窗口（test_case 治具通道复用，可能为空）。 */
    Fixture_uart* fixtureUartWidget() const { return Fixture_uart_ui; }

private:
    Fixture_uart* Fixture_uart_ui = nullptr;

private slots:
    void startTest();
};

#endif  // QFREEWORKBOX_H
