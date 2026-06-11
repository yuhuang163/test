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
    Fixture_uart* fixtureUartWidget() const {
        return Fixture_uart_ui;
    }
    /** 治具 test_case 防呆：按配置自动创建窗口并连接串口；autoConnectedOut 表示本次是否新连上。 */
    Fixture_uart* ensureFixtureUartConnected(int stationIndex, QString* detailOut = nullptr,
                                             bool* autoConnectedOut = nullptr);

  private:
    static QString resolvedFixtureComName(int stationIndex);
    Fixture_uart* Fixture_uart_ui = nullptr;

  private slots:
    void startTest();
};

#endif // QFREEWORKBOX_H
