#ifndef PLATFORM_SERIAL_CHANNEL_H
#define PLATFORM_SERIAL_CHANNEL_H

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QTimer>

class QComboBox;

/** 平台层串口通道：打开/关闭、防抖读、RTS/DTR、写数据；底层 QSerialPort 可供协议层使用 */
class SerialChannel : public QObject {
    Q_OBJECT

  public:
    enum class RtsDtrMode {
        None,
        Enable,
        DtrOnly,
        ToggleReset,
        FullReset,
    };

    struct OpenParams {
        QString portName;
        qint32 baudRate = 921600;
        qint32 readBufferSize = 4096;
        int readDebounceMs = 10;
        QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
        RtsDtrMode rtsDtrMode = RtsDtrMode::ToggleReset;
    };

    explicit SerialChannel(QObject* parent = nullptr);
    ~SerialChannel() override;

    QSerialPort* port();
    const QSerialPort* port() const;

    void setDefaultParams(const OpenParams& params);
    bool open(const QString& portName);
    bool open(const OpenParams& params);
    void close();
    bool isOpen() const;

    qint64 write(const QByteArray& data);
    QString portName() const;
    QString errorString() const;

    static QStringList availablePortNames();
    static void updateComboBoxPorts(QComboBox* comboBox);

  signals:
    void opened();
    void closed();
    void frameReceived(const QByteArray& data);
    void errorOccurred(QSerialPort::SerialPortError error, const QString& message);

  private slots:
    void onReadyRead();
    void onReadTimer();
    void onPortError(QSerialPort::SerialPortError error);

  private:
    void applyLineSettings();
    void applyRtsDtr();

    QSerialPort* port_ = nullptr;
    QTimer* readTimer_ = nullptr;
    QByteArray rxBuffer_;
    OpenParams params_;
};

#endif // PLATFORM_SERIAL_CHANNEL_H
