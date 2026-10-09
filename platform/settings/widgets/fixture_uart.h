#ifndef FIXTURE_UART_H
#define FIXTURE_UART_H

#include <QWidget>

#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Ui {
class Fixture_uart;
}

class QFixtureManager;
class QTimer;

enum class FixtureUartRole {
    FixturePort,
    PlcSharedPort,
    PreStartMonitor
};

class Fixture_uart : public QWidget {
    Q_OBJECT
  public:
    explicit Fixture_uart(QWidget* parent = nullptr, FixtureUartRole role = FixtureUartRole::FixturePort);
    ~Fixture_uart();

    Ui::Fixture_uart* ui;

    // Delegate to QFixtureManager
    void sendimuData(imuFixtureState fixstate);
    void sendFixtureData(FixtureState fixstate);
    void send_command_to_machine(int command_id, int numb);
    bool isFixtureSerialOpen() const;
    bool tryOpenSerialPort(const QString& portName, bool autoConnect = false);
    void closeSerialPort();
    /** 当前连接/选中的治具 COM 口名称（日志用）。 */
    QString currentFixtureComName() const;
    void sendPcbaFrame(const QByteArray& frame);
    QFixtureManager* fixtureManager() const {
        return fixtureManager_;
    }
    void setFixBaudRate(int baudRate);
    bool sendRawFrame(const QByteArray& frame, QString* errorMessage = nullptr);
    bool exchangeRawFrame(const QByteArray& request, QByteArray* response, int timeoutMs,
                          QString* errorMessage = nullptr);

    /** 压感工站多机同步发令时的延时与上次命令记录（委托 QFixtureManager）。 */
    void delay_msec(unsigned int msec);
    qint64 lastCommidTimestamp() const;
    void setLastCommidTimestamp(qint64 timestamp);
    machine_command_id_e lastCommid() const;
    void setLastCommid(machine_command_id_e commandId);

    void reloadStationConfig();
    QString currentStationKey() const;

    int fixBaudRate = 9600;

  protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

  private:
    QFixtureManager* fixtureManager_ = nullptr;
    QTimer* scanSerialPortsTimer_ = nullptr;
    bool isConfigLoading_ = false;
    bool plcSharedPort_ = false;
    FixtureUartRole role_ = FixtureUartRole::FixturePort;

    void loadStationPageConfig();
    void saveFixturePortConfig();
    void savePreStartMonitorConfig();
    void updateWindowTitleWithStation();
    void applyRolePageVisibility();
    void applyPreStartMonitorFormEnabled(bool enabled);
    void applyXwdDownScanFormEnabled(bool enabled);

    void updateDeviceFieldsVisibility();

  signals:
    // Re-emitted from QFixtureManager
    void send_data_to_mechine(const FixturePacketData datapack);
    void send_data_to_mechine_imu(int state);
    void send_data_to_mechine_sleep(const FixturePacketData datapack);
    void send_data_to_mechine_start();
    void start_fix_action(int state);
    void send_data_to_mechine_press(int state);

  private slots:
    void set_camera_action(camreaFixtureState fixstate);

    void on_FixtureconnectButton_clicked();
    void on_FixturedisconnectButton_clicked();
    void scanSerialPorts();

    void send_start_command(int i);
    void send_start_sleep_command(int i);
    void send_start_white_modle_command(int i);

    void onManagerConnected();
    void onManagerDisconnected();
    void onManagerError(int error, const QString& message);

    void on_FixturecomNameCombo_currentTextChanged(const QString& text);
    void on_plcDeviceCombo_currentIndexChanged(int index);
    void on_plcComPortCombo_currentTextChanged(const QString& text);
    void on_plcBaudRateCombo_currentTextChanged(const QString& text);
    void on_plcIpLineEdit_textChanged(const QString& text);
    void on_plcIpLineEdit_editingFinished();
    void on_plcPortSpinBox_valueChanged(int value);
    void on_plcWaitAddressLineEdit_textChanged(const QString& text);
    void on_plcWaitAddressLineEdit_editingFinished();
    void on_scannerIpLineEdit_textChanged(const QString& text);
    void on_scannerIpLineEdit_editingFinished();
    void on_scannerPortSpinBox_valueChanged(int value);
    void on_preStartMonitorEnabledCheckBox_toggled(bool checked);
    void on_xwdDownScanEnabledCheckBox_toggled(bool checked);
    void on_xwdPlcFixtureComCombo_currentTextChanged(const QString& text);
    void on_xwdPlcFixtureBaudCombo_currentTextChanged(const QString& text);
    void on_serialScannerComCombo_currentTextChanged(const QString& text);
    void on_serialScannerBaudCombo_currentTextChanged(const QString& text);
    void on_serialScannerHexEdit_textChanged(const QString& text);
    void on_serialScannerHexEdit_editingFinished();
    void on_serialScannerTimeoutSpin_valueChanged(int value);
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_UART_H
