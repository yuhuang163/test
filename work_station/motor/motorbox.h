#ifndef MOTORmotorbox_H
#define MOTORmotorbox_H

#include "Abini.h"
#include "box_base.h"
#include "fixture_uart.h"
#include "motor.h"
#include "ui_motor.h"

namespace Ui {
    class motorbox;
}

class motorbox : public box_base {
    Q_OBJECT
public:
    explicit motorbox(QWidget* parent = nullptr);
    ~motorbox();
    Ui::motorbox* ui;
    int motor_cali_stage = 1;
private slots:
    void resetValue();
    void checkAllTest(int fixtureNumber) override;
};

#endif  // MOTORmotorbox_H
