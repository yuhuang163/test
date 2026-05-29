#ifndef QCUSTOMWORKBOX_H
#define QCUSTOMWORKBOX_H

#include "box_base.h"
#include "fixture_uart.h"
#include "ui_fixture_uart.h"

namespace Ui {
    class QCustomWorkBox;
}

class QCustomWorkBox : public box_base {
    Q_OBJECT

public:
    explicit QCustomWorkBox(QWidget* parent = nullptr);
    ~QCustomWorkBox();
    Ui::QCustomWorkBox* ui;

private:
    Fixture_uart* Fixture_uart_ui = nullptr;

private slots:
    void startTest();
};

#endif  // QCUSTOMWORKBOX_H
