#ifndef QFIXTUREMANAGER_H
#define QFIXTUREMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QFuture>
#include <atomic>
#include <functional>

#include "fixture_uart_types.h"

class HzPcbaFixtureDevice;
class PressFixtureDevice;
class CameraFixtureDevice;
class ImuFixtureDevice;
class Qlog;

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 治具串口统一管理：持有各具体治具 device，对外保持原有接口。 */
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

    std::atomic<bool> running_{false};
    QFuture<void> future_;

    HzPcbaFixtureDevice* hzPcbaDevice_ = nullptr;
    PressFixtureDevice* pressDevice_ = nullptr;
    CameraFixtureDevice* cameraDevice_ = nullptr;
    ImuFixtureDevice* imuDevice_ = nullptr;
    Qlog* log_ = nullptr;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // QFIXTUREMANAGER_H
