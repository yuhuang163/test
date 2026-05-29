#ifndef QSERIALPORTREADER_H
#define QSERIALPORTREADER_H

#include <QByteArray>
#include <QObject>
#include <functional>

class QSerialPort;
class QTimer;

/** 串口收包防抖：readyRead 累积缓冲，定时器到期后一次性回调（与 test_base dongle 逻辑一致） */
class QSerialPortReader : public QObject {
    Q_OBJECT
public:
    explicit QSerialPortReader(QObject* parent = nullptr);

    void bind(QSerialPort* port, int debounceMs = 10);
    void setHandler(std::function<void(const QByteArray&)> handler);
    void clear();

private slots:
    void onReadyRead();
    void onDebounceTimeout();

private:
    QSerialPort* port_ = nullptr;
    QTimer* debounceTimer_ = nullptr;
    QByteArray buffer_;
    std::function<void(const QByteArray&)> handler_;
};

#endif  // QSERIALPORTREADER_H
