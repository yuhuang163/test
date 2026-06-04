#ifndef FIXTURE_UART_H
#define FIXTURE_UART_H

#include <atomic>

#include <QFuture>
#include <QSerialPort>
#include <QTimer>
#include <QWidget>

#include <QtConcurrent>

#include "protocol/fixture_pcba_uart_protocol.h"
#include "protocol/fixture_press_uart_protocol.h"
#include "protocol/fixture_uart_types.h"
#include "qlog.h"
#include "usmile_ring_buffer.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace Ui {
    class Fixture_uart;
}

class Fixture_uart : public QWidget {
    Q_OBJECT
public:
    qint64 last_sent_timestamp = 0;
    std::atomic<bool> running;
    QFuture<void> future;

    explicit Fixture_uart(QWidget* parent = nullptr);
    ~Fixture_uart();

    Ui::Fixture_uart* ui;
    void sendimuData(imuFixtureState fixstate);
    void sendFixtureData(FixtureState fixstate);

    machine_command_id_e last_commid = COMMAND_ID_MAX;
    qint64 last_commid_timestamp = 0;

    void FixtureCommandInit(void);
    void send_command_to_machine(int command_id, int numb);
    void delay_msec(unsigned int msec);

    int fixBaudRate = 9600;

private:
    void solve_frame(void);
    void dispatchTextProtocols(const QByteArray& data);
    void writeFixturePort(const QByteArray& data, bool logTx = true, bool startAction = false);

    RingBuf* fixRingBuf = nullptr;
    usmile_ring_buffer_t p_fixRingBuffer;
    uint8_t fix_ring_buffer[100 * 1024];
    uint8_t frame_buf[2 * 1024];

    FixturePcbaUartProtocol* pcbaProtocol_ = nullptr;
    FixturePressUartProtocol pressProtocol_;
    Qlog log_;

    QSerialPort* fixtureSerialPort;
    QTimer* fixtureSerialPortTimer = new QTimer(this);
    QByteArray fixtureSerialPortBuf = 0;

signals:
    void send_data_to_mechine(const FixturePacketData datapack);
    void send_data_to_mechine_imu(int state);
    void send_data_to_mechine_sleep(const FixturePacketData datapack);
    void send_data_to_mechine_start();
    void start_fix_action(int state);
    void send_data_to_mechine_press(int state);

private slots:
    void readFixtureSerialPortData();
    void set_camera_action(camreaFixtureState fixstate);

    void openFixtureSerialPort();
    void closeFixtureSerialPort();
    void refresh_Fixtureuart_state(int state);
    void on_FixtureconnectButton_clicked();
    void on_FixturerefreshCom_clicked();
    void on_FixturedisconnectButton_clicked();
    void FixturehandleError(QSerialPort::SerialPortError error);
    void send_start_command(int i);
    void send_start_sleep_command(int i);
    void send_start_white_modle_command(int i);
};

#if _MSC_VER >= 1600
#    pragma execution_character_set(pop)
#endif

#endif  // FIXTURE_UART_H
