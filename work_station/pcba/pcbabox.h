#ifndef PCBABOX_H
#define PCBABOX_H
#include "box_base.h"
#include "fixture_uart.h"
#include "pcbaform.h"
#include "ui_fixture_uart.h"
#include "ui_pcbaform.h"

namespace Ui
{
    class pcbabox;
}

class pcbabox : public box_base
{
    Q_OBJECT
public:
    pcbabox(QWidget *parent = nullptr);
    ~pcbabox();
private:
    Ui::pcbabox *ui;
    Fixture_uart *Fixture_uart_ui = NULL;
private slots:
    void startTest();
};
#endif   // PCBABOX_H
