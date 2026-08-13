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
        /** RS485 半双工：exchange 发前 RTS 高、发完 RTS 低再收。 */
        Rs485HalfDuplex,
    };

    struct OpenParams {
        QString portName;
        qint32 baudRate = 921600;
        qint32 readBufferSize = 4096;
        int readDebounceMs = 10;
        QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
        RtsDtrMode rtsDtrMode = RtsDtrMode::ToggleReset;
        /** 多数设备为 8-N-1；信捷 PLC 等出厂 8-E-1 的设备需显式指定 */
        QSerialPort::Parity parity = QSerialPort::NoParity;
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

    /** 清空未吐出的收包缓冲（发前调用，避免旧数据误作本次应答）。 */
    void clearReceiveBuffer();
    /** 等待下一帧（防抖结束后的 frameReceived）；超时返回 false。 */
    bool waitForFrame(QByteArray* outFrame, int timeoutMs);
    /** 先清缓冲、写请求，再等回包（避免发送后、监听前应答已到导致丢包）。 */
    bool exchange(const QByteArray& request, QByteArray* response, int timeoutMs);
    /** 发一次请求并收集多段防抖收包（Modbus RTU 粘包/回显时用）。 */
    bool exchangeCollect(const QByteArray& request, QByteArray* response, int timeoutMs);

    static QStringList availablePortNames();
    static void updateComboBoxPorts(QComboBox* comboBox);
    /** 主板自带 COM1 等不用于产测的口，下拉与枚举时屏蔽。 */
    static bool isHiddenSystemPort(const QString& portName);
    /** 设备已卸载但仍残留在注册表的幽灵口返回 false（占用中的口仍视为存在）。 */
    static bool isPortPresent(const QString& portName);

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
