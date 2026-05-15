#ifndef QPRODUCT_H
#define QPRODUCT_H

#include <QByteArray>
#include <QObject>
#include <QString>

class QSerialPort;

/**
 * 产品串口侧仪器协议（帧格式见 docs/测试.md）：复位 / 开始接收 / 停止接收及收包数解析。
 * 仅负责组帧、写串口与解析；读缓冲仍由上层（如 test_base 的 productSerialPortBuf）汇总后传入解析接口。
 */
class Qproduct : public QObject {
    Q_OBJECT
public:
    explicit Qproduct(QSerialPort* port, QObject* parent = nullptr);

    QSerialPort* serialPort() const { return port_; }

    /** 无空格十六进制字符串转二进制，如 "01030C00"。非法字符返回空。 */
    static QByteArray hexToBytes(const QString& hexNoSpaces);
    static QString bytesToHex(const QByteArray& data);

    /** 向产品串口写入一帧原始字节（需已 open）。 */
    bool writeRaw(const QByteArray& frame, QString* errorOut = nullptr);
    bool writeHex(const QString& hexNoSpaces, QString* errorOut = nullptr);

    // ---- docs/测试.md 固定帧 ----
    static QByteArray cmdReset() { return QByteArray::fromHex("01030C00"); }
    static QByteArray cmdStopReceive() { return QByteArray::fromHex("011F2000"); }
    static QByteArray ackReset() { return QByteArray::fromHex("040E0405030C00"); }
    static QByteArray ackStartReceive() { return QByteArray::fromHex("040E0405332000"); }
    /** 停止接收响应前缀（后接 2 字节小端收包数）。 */
    static QByteArray prefixStopReceive() { return QByteArray::fromHex("040E06051F2000"); }

    /** 按文档「开始接收」固定帧（与 cmdReset 等同为头内 fromHex）。 */
    static QByteArray buildStartReceiveCmd2402Ble1M() { return QByteArray::fromHex("01332003000100"); }
    static QByteArray buildStartReceiveCmd2440Ble1M() { return QByteArray::fromHex("01332003130100"); }
    static QByteArray buildStartReceiveCmd2480Ble1M() { return QByteArray::fromHex("01332003270100"); }
    static QByteArray buildStartReceiveCmd2402Ble2M() { return QByteArray::fromHex("01332003000200"); }
    static QByteArray buildStartReceiveCmd2440Ble2M() { return QByteArray::fromHex("01332003130200"); }
    static QByteArray buildStartReceiveCmd2480Ble2M() { return QByteArray::fromHex("01332003270200"); }

    static bool responseContainsPrefix(const QByteArray& rx, const QByteArray& prefix) {
        return rx.indexOf(prefix) >= 0;
    }
    /** 从停止接收应答中解析小端 16 位收包数；未找到前缀返回 -1。 */
    static int parseStopReceivePacketCountLe(const QByteArray& rx) {
        const QByteArray pre = prefixStopReceive();
        const int idx = rx.indexOf(pre);
        if (idx < 0)
            return -1;
        const int valPos = idx + pre.size();
        if (rx.size() < valPos + 2)
            return -1;
        const auto b0 = static_cast<quint8>(rx.at(valPos));
        const auto b1 = static_cast<quint8>(rx.at(valPos + 1));
        return static_cast<int>(b0 | (b1 << 8));
    }
    /** PER = (仪器发包数 - 实际收包数) / 仪器发包数 */
    static double computePer(int instrumentSendCount, int receivedCount) {
        if (instrumentSendCount <= 0)
            return 0.0;
        return static_cast<double>(instrumentSendCount - receivedCount) /
               static_cast<double>(instrumentSendCount);
    }

private:
    QSerialPort* port_ = nullptr;
};

#endif  // QPRODUCT_H
