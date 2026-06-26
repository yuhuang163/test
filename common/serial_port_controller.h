#ifndef SERIAL_PORT_CONTROLLER_H
#define SERIAL_PORT_CONTROLLER_H

#include <QObject>
#include <QComboBox>
#include <QString>
#include <QSerialPort>

#include "serial_channel.h"

class SerialPortController : public QObject {
    Q_OBJECT

  public:
    struct UIControls {
        QComboBox* portCombo = nullptr;
    };

    explicit SerialPortController(SerialChannel* channel, QObject* parent = nullptr);
    ~SerialPortController() override = default;

    void setUIControls(const UIControls& ui);

    // Default open parameters
    void setBaudRate(int baudRate);
    void setReadDebounceMs(int debounceMs);
    void setRtsDtrMode(SerialChannel::RtsDtrMode mode);

    bool open();
    void close();
    bool isOpen() const;

  signals:
    void stateChanged(bool isOpen);
    void errorOccurred(const QString& errorMessage);

  private slots:
    void onChannelErrorOccurred(QSerialPort::SerialPortError error, const QString& message);

  private:
    SerialChannel* channel_ = nullptr;
    UIControls ui_;
    int baudRate_ = 115200;
    int debounceMs_ = 10;
    SerialChannel::RtsDtrMode rtsDtrMode_ = SerialChannel::RtsDtrMode::Enable;
};

#endif // SERIAL_PORT_CONTROLLER_H
