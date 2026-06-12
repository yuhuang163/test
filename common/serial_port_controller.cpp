#include "serial_port_controller.h"

#include <QDebug>

SerialPortController::SerialPortController(SerialChannel* channel, QObject* parent)
    : QObject(parent), channel_(channel) {
    if (channel_) {
        connect(channel_, &SerialChannel::errorOccurred, this, &SerialPortController::onChannelErrorOccurred);
    }
}

void SerialPortController::setUIControls(const UIControls& ui) {
    ui_ = ui;
}

void SerialPortController::setBaudRate(int baudRate) {
    baudRate_ = baudRate;
}

void SerialPortController::setReadDebounceMs(int debounceMs) {
    debounceMs_ = debounceMs;
}

void SerialPortController::setRtsDtrMode(SerialChannel::RtsDtrMode mode) {
    rtsDtrMode_ = mode;
}

bool SerialPortController::open() {
    if (!channel_)
        return false;

    QString portName;
    if (ui_.portCombo) {
        portName = ui_.portCombo->currentText();
    }

    if (portName.isEmpty()) {
        qWarning() << "SerialPortController: Port name is empty";
        return false;
    }

    SerialChannel::OpenParams params;
    params.portName = portName;
    params.baudRate = baudRate_;
    params.readDebounceMs = debounceMs_;
    params.rtsDtrMode = rtsDtrMode_;

    if (channel_->open(params)) {
        emit stateChanged(true);
        return true;
    } else {
        qWarning() << "SerialPortController: Failed to open port" << portName;
        return false;
    }
}

void SerialPortController::close() {
    if (!channel_)
        return;

    channel_->close();
    emit stateChanged(false);
}

bool SerialPortController::isOpen() const {
    return channel_ && channel_->port() && channel_->port()->isOpen();
}

void SerialPortController::onChannelErrorOccurred(QSerialPort::SerialPortError error, const QString& message) {
    if (error == QSerialPort::NoError)
        return;

    qWarning() << "SerialPortController: Port error on" << (channel_ ? channel_->portName() : "unknown")
               << "code=" << error << "detail=" << message;

    if (error == QSerialPort::PermissionError) {
        close();
        emit errorOccurred(QStringLiteral("串口权限问题: ") + message);
    } else {
        emit errorOccurred(message);
    }
}
