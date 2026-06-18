#ifndef QFIXTUREMANAGER_H
#define QFIXTUREMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QFuture>
#include <atomic>
#include <functional>

#include "fixture_uart_types.h"
#include "usmile_ring_buffer.h"

class FixturePcbaUartProtocol;
class FixturePressUartProtocol;
class Qlog;

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

class QFixtureManager : public QObject {
    Q_OBJECT
  public:
    explicit QFixtureManager(QObject* parent = nullptr);
    ~QFixtureManager() override;

    bool open(const QString& portName, int baudRate = 9600);
    void close();
    bool isOpen() const;

    QString portName() const;
    QString errorString() const;

    // --- 命令下发接口 ---
    void sendPcbaFrame(const QByteArray& frame);
    void sendimuData(imuFixtureState fixstate);
    void set_camera_action(camreaFixtureState fixstate);
    void sendFixtureData(FixtureState fixstate);
    void send_start_command(int i);
    void send_start_sleep_command(int i);
    void send_start_white_modle_command(int i);
    void send_command_to_machine(int command_id, int numb);

    void delayMsec(unsigned int msec);
    qint64 lastCommidTimestamp() const;
    void setLastCommidTimestamp(qint64 timestamp);
    machine_command_id_e lastCommid() const;
    void setLastCommid(machine_command_id_e commandId);

  signals:
    void connected();
    void disconnected();
    void errorOccurred(QSerialPort::SerialPortError error, const QString& message);

    // --- 解包与状态信号（与原 Fixture_uart 信号完全一致） ---
    void send_data_to_mechine(const FixturePacketData datapack);
    void send_data_to_mechine_imu(int state);
    void send_data_to_mechine_sleep(const FixturePacketData datapack);
    void send_data_to_mechine_start();
    void start_fix_action(int state);
    void send_data_to_mechine_press(int state);

  private slots:
    void readFixtureSerialPortData();
    void handleError(QSerialPort::SerialPortError error);

  private:
    void solve_frame();
    void dispatchTextProtocols(const QByteArray& data);
    void writeFixturePort(const QByteArray& data, bool logTx = true, bool startAction = false);

    QSerialPort* fixtureSerialPort_ = nullptr;
    QTimer* fixtureSerialPortTimer_ = nullptr;
    QByteArray fixtureSerialPortBuf_;

    RingBuf* fixRingBuf_ = nullptr;
    usmile_ring_buffer_t p_fixRingBuffer_;
    uint8_t fix_ring_buffer_[100 * 1024];
    uint8_t frame_buf_[2 * 1024];

    std::atomic<bool> running_{false};
    QFuture<void> future_;

    FixturePcbaUartProtocol* pcbaProtocol_ = nullptr;
    FixturePressUartProtocol* pressProtocol_ = nullptr;
    Qlog* log_ = nullptr;

    qint64 last_sent_timestamp_ = 0;
    qint64 last_commid_timestamp_ = 0;
    machine_command_id_e last_commid_ = COMMAND_ID_MAX;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // QFIXTUREMANAGER_H
