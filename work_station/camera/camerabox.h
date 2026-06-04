#ifndef CAMERABOX_H
#define CAMERABOX_H

#include <vector>

#include "box_base.h"
#include "cameratest.h"
#include "fixture_uart.h"
#include "ui_cameratest.h"
#include "ui_fixture_uart.h"

namespace Ui {
    class camerabox;
}

class camerabox : public box_base {
    Q_OBJECT
public:
    explicit camerabox(QWidget* parent = nullptr);
    ~camerabox();

private:
    Ui::camerabox* ui;
    void checkAllTest(int fixtureNumber) override;
    Fixture_uart* Fixture_uart_ui = NULL;  // 设备控制窗口

signals:
    void go_camera_next(int);
};

#endif  // CAMERABOX_H
