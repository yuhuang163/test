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

private:
    Fixture_uart* Fixture_uart_ui = nullptr;

private slots:
    void startTest();
};

#endif  // QFREEWORKBOX_H
