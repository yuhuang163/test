#include "qserialportreader.h"

#include <QSerialPort>
#include <QTimer>

QSerialPortReader::QSerialPortReader(QObject* parent) : QObject(parent) {
    debounceTimer_ = new QTimer(this);
    debounceTimer_->setSingleShot(true);
    connect(debounceTimer_, &QTimer::timeout, this, &QSerialPortReader::onDebounceTimeout);
}

void QSerialPortReader::bind(QSerialPort* port, int debounceMs) {
    if (port_ == port) {
        if (debounceTimer_) {
            debounceTimer_->setInterval(debounceMs);
        }
        return;
    }

    if (port_) {
        disconnect(port_, nullptr, this, nullptr);
    }

    port_ = port;
    buffer_.clear();
    if (debounceTimer_) {
        debounceTimer_->setInterval(debounceMs);
    }

    if (!port_) {
        return;
    }

    connect(port_, &QSerialPort::readyRead, this, &QSerialPortReader::onReadyRead);
}

void QSerialPortReader::setHandler(std::function<void(const QByteArray&)> handler) {
    handler_ = std::move(handler);
}

void QSerialPortReader::clear() {
    buffer_.clear();
    if (debounceTimer_) {
        debounceTimer_->stop();
    }
}

void QSerialPortReader::onReadyRead() {
    if (!port_) {
        return;
    }
    buffer_.append(port_->readAll());
    if (debounceTimer_) {
        debounceTimer_->start();
    }
}

void QSerialPortReader::onDebounceTimeout() {
    if (buffer_.isEmpty() || !handler_) {
        buffer_.clear();
        return;
    }
    const QByteArray chunk = buffer_;
    buffer_.clear();
    handler_(chunk);
}
