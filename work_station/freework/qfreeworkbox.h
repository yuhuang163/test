#ifndef QFREEWORKBOX_H
#define QFREEWORKBOX_H

#include "box_base.h"
#include "fixture_uart.h"
#include "ui_fixture_uart.h"

#include <QHash>
#include <QMutex>

class SerialChannel;

namespace Ui {
class QFreeWorkBox;
}

class Asd9026aDevice;

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
    /** 顶部“连接治具串口”窗口当前选择的端口；窗口未打开时读取已保存配置。 */
    QString selectedFixtureComName(int stationIndex) const;
    /** 一拖多工位共用同一个 ASD9026A 串口对象，避免重复打开顶部治具串口。 */
    Asd9026aDevice* sharedAsd9026aDevice() const {
        return asd9026aDevice_;
    }
    /** 所有子工位均已停止时释放共享 ASD9026A 串口。 */
    void releaseSharedAsd9026aIfIdle();
    /** 两工位共用一台多路温度记录仪时的共享串口（按步骤 Param 设备序号）。 */
    SerialChannel* ensureSharedTempLoggerChannel(int deviceIndex0Based, const QString& portName, QString* errorOut,
                                                 int baudRate = 115200);
    QMutex* sharedTempLoggerMutex(int deviceIndex0Based);
    void releaseSharedTempLoggerIfIdle();

  private:
    static QString resolvedFixtureComName(int stationIndex);
    Fixture_uart* Fixture_uart_ui = nullptr;
    Asd9026aDevice* asd9026aDevice_ = nullptr;
    QHash<int, SerialChannel*> sharedTempLoggerChannels_;
    QHash<int, QMutex*> sharedTempLoggerMutexes_;

  private slots:
    void startTest();
};

#endif // QFREEWORKBOX_H
