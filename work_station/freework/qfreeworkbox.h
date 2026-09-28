#ifndef QFREEWORKBOX_H
#define QFREEWORKBOX_H

#include "box_base.h"
#include "fixture_uart.h"
#include "serial_channel.h"
#include "ui_fixture_uart.h"

#include "pre_start_monitor_config.h"
#include "qmodbusmanager.h"

#include <QHash>
#include <QMutex>

class QAction;
class SerialChannel;
class QTimer;

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
    /** 懒创建顶部治具串口窗口（不自动 open）。 */
    void ensureFixtureUartWidgetCreated();
    /** 治具 test_case 防呆：按配置自动创建窗口并连接串口；autoConnectedOut 表示本次是否新连上。 */
    Fixture_uart* ensureFixtureUartConnected(int stationIndex, QString* detailOut = nullptr,
                                             bool* autoConnectedOut = nullptr);
    /** M8 PLC Hook 专用的一拖多治具串口，与程控电源/普通治具串口物理隔离。 */
    Fixture_uart* ensurePlcFixtureUartConnected(QString* detailOut = nullptr);
    /** 顶部“连接治具串口”窗口当前选择的端口；窗口未打开时读取已保存配置。 */
    QString selectedFixtureComName(int stationIndex) const;
    /** 一拖多工位共用同一个 ASD9026A 串口对象，避免重复打开顶部治具串口。 */
    Asd9026aDevice* sharedAsd9026aDevice() const {
        return asd9026aDevice_;
    }
    /** ASD9026A 共享串口互斥（一拖多同时读电流/配置时串行化收发）。 */
    QMutex* sharedAsd9026aMutex() {
        return &sharedAsd9026aMutex_;
    }
    /** 所有子工位均已停止时释放共享 ASD9026A 串口。 */
    void releaseSharedAsd9026aIfIdle();
    /** 两工位共用一台多路温度记录仪时的共享串口（按步骤 Param 设备序号）。 */
    SerialChannel* ensureSharedTempLoggerChannel(int deviceIndex0Based, const QString& portName, QString* errorOut,
                                                 int baudRate = 115200,
                                                 SerialChannel::RtsDtrMode rtsMode = SerialChannel::RtsDtrMode::Enable);
    QMutex* sharedTempLoggerMutex(int deviceIndex0Based);
    void releaseSharedTempLoggerIfIdle();

    /** 共享 PLC 按键 / XWD 到位监控与一拖多扫码枪联动 */
    void updatePlcMonitorState();
    void triggerAllStationScanners();
    void triggerAllStationSerialScanners();
    bool isAnyStationTesting() const;
    void releaseSharedPlcIfIdle();
    /** PLC 治具串口一拖多互斥（流程 UP/DOWN）。 */
    QMutex* sharedPlcFixtureMutex();
    /** 自动扫码监控口互斥（XWD DOWN?，与流程治具口配置独立）。 */
    QMutex* sharedXwdMonitorMutex();
    /** 按当前选中工站 flow.ini [SerialUi] 刷新顶部治具/PLC 治具串口菜单文案与波特率。 */
    void applyFixtureMenuLabel();

  public slots:
    void checkAllover(int fixtureNumber) override;

  protected:
    void closeEvent(QCloseEvent* event) override;
    void abortBlockingWaits() override;

  private:
    static QString resolvedFixtureComName(int stationIndex);
    static QString resolvedPlcFixtureComName();
    /** 普通治具串口窗口；show=false 时仅创建不弹窗。 */
    void ensureFixtureUartWindow(bool show = true);
    /** PLC 治具串口独立窗口；show=false 时仅创建不弹窗。 */
    void ensurePlcFixtureUartWindow(bool show = true);
    /** 自动扫码监控口：与 PLC 治具同设备时复用共享通道；不同 COM 才另开。 */
    Fixture_uart* ensureXwdMonitorUartConnected(QString* detailOut = nullptr);
    void releaseXwdMonitorUart();

    Fixture_uart* Fixture_uart_ui = nullptr;
    Fixture_uart* plcFixtureUartUi_ = nullptr;
    Fixture_uart* xwdMonitorUartUi_ = nullptr;
    /** 到位监控当前是否走 PLC 治具共享串口（同一台 XWD）。 */
    bool xwdMonitorUsesSharedPlcUart_ = false;
    QAction* fixtureConnectAct_ = nullptr;
    QAction* plcFixtureConnectAct_ = nullptr;
    Fixture_uart* preStartMonitorUi_ = nullptr;
    Asd9026aDevice* asd9026aDevice_ = nullptr;
    QMutex sharedAsd9026aMutex_;
    QHash<int, SerialChannel*> sharedTempLoggerChannels_;
    QHash<int, QMutex*> sharedTempLoggerMutexes_;
    QHash<int, SerialChannel::OpenParams> sharedTempLoggerOpenParams_;

    // 全局共享 PLC 按键 / XWD 到位监控
    QModbusManager sharedPlcModbusManager_;
    QTimer* plcMonitorTimer_ = nullptr;
    bool plcMonitorRunning_ = false;
    PreStartMonitorConfig sharedPlcConfig_;
    QMutex sharedPlcFixtureMutex_;
    QMutex sharedXwdMonitorMutex_;

  private slots:
    void startTest();
    void onPlcMonitorTimeout();
    /** 轮询 XWD DOWN?，OK 则触发串口扫码；返回 true 表示已触发。 */
    bool pollXwdDownAndTriggerScan();
};

#endif // QFREEWORKBOX_H
