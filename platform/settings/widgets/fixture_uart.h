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

class Fixture_uart : public QWidget {
    Q_OBJECT
  public:
    explicit Fixture_uart(QWidget* parent = nullptr);
    ~Fixture_uart();

    Ui::Fixture_uart* ui;

    // Delegate to QFixtureManager
    void sendimuData(imuFixtureState fixstate);
    void sendFixtureData(FixtureState fixstate);
    void send_command_to_machine(int command_id, int numb);
    bool isFixtureSerialOpen() const;
    void sendPcbaFrame(const QByteArray& frame);

    int fixBaudRate = 9600;

  private:
    QFixtureManager* fixtureManager_ = nullptr;

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
    void on_FixturerefreshCom_clicked();
    void on_FixturedisconnectButton_clicked();

    void send_start_command(int i);
    void send_start_sleep_command(int i);
    void send_start_white_modle_command(int i);

    void onManagerConnected();
    void onManagerDisconnected();
    void onManagerError(int error, const QString& message);
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // FIXTURE_UART_H
