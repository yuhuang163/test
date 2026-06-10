#ifndef QPRODUCT_H
#define QPRODUCT_H

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class QSerialPort;

/**
 * 产品串口侧仪器协议（帧格式见 docs/测试.md）：复位 / 开始接收 / 停止接收及收包数解析。
 * 组帧、写串口、解析；接收二进制由 test_base::readProductSerialPortData 等调用 parseCmd（与治具 jig->parseCmd 同形）写入累积缓冲，
 * 再用 productSerialRxAccum() 配合 responseContainsPrefix / parseStopReceivePacketCountLe 等做判定；
 * 或在工站 connect 下方信号，由 parseCmd 在累积缓冲内自动识别「首次出现的」应答边沿（每轮流程前 clear 一次即可）。
 */
class Qproduct : public QObject {
    Q_OBJECT
  public:
    enum class ProductCmd {
        WriteRaw,           // set: QByteArray
        WriteHex,           // set: QString
        ParseRx,            // set: QByteArray
        ClearRxAccum,       // set/get
        SendReset,          // set
        SendStopReceive,    // set
        SendStart2402Ble1M, // set
        SendStart2440Ble1M, // set
        SendStart2480Ble1M, // set
        SendStart2402Ble2M, // set
        SendStart2440Ble2M, // set
        SendStart2480Ble2M, // set
        GetStopReceiveCount // get: QVariant(QByteArray)
    };

    explicit Qproduct(QSerialPort* port, QObject* parent = nullptr);

    QSerialPort* serialPort() const {
        return port_;
    }

    /** 无空格十六进制字符串转二进制，如 "01030C00"。非法字符返回空。 */
    static QByteArray hexToBytes(const QString& hexNoSpaces);
    static QString bytesToHex(const QByteArray& data);

    /** 向产品串口写入一帧原始字节（需已 open）。 */
    bool writeRaw(const QByteArray& frame, QString* errorOut = nullptr);
    bool writeHex(const QString& hexNoSpaces, QString* errorOut = nullptr);

    /** 与 jig->parseCmd 一致：由 readProductSerialPortData 在每批聚合数据上调用，追加缓冲并扫描 docs/测试.md 固定应答，必要时发 instrument* 信号。 */
    void parseCmd(const QByteArray& data);
    void set(ProductCmd cmd, const QVariant& data = {});
    void get(ProductCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);
    const QByteArray& productSerialRxAccum() const {
        return productSerialRxAccum_;
    }
    void clearProductSerialRxAccum();

    // ---- docs/测试.md 固定帧 ----
    static QByteArray cmdReset() {
        return QByteArray::fromHex("01030C00");
    }
    static QByteArray cmdStopReceive() {
        return QByteArray::fromHex("011F2000");
    }
    static QByteArray ackReset() {
        return QByteArray::fromHex("040E0405030C00");
    }
    static QByteArray ackStartReceive() {
        return QByteArray::fromHex("040E0405332000");
    }
    /** 停止接收响应前缀（后接 2 字节小端收包数）。 */
    static QByteArray prefixStopReceive() {
        return QByteArray::fromHex("040E06051F2000");
    }

    /** 按文档「开始接收」固定帧（与 cmdReset 等同为头内 fromHex）。 */
    static QByteArray buildStartReceiveCmd2402Ble1M() {
        return QByteArray::fromHex("01332003000100");
    }
    static QByteArray buildStartReceiveCmd2440Ble1M() {
        return QByteArray::fromHex("01332003130100");
    }
    static QByteArray buildStartReceiveCmd2480Ble1M() {
        return QByteArray::fromHex("01332003270100");
    }
    static QByteArray buildStartReceiveCmd2402Ble2M() {
        return QByteArray::fromHex("01332003000200");
    }
    static QByteArray buildStartReceiveCmd2440Ble2M() {
        return QByteArray::fromHex("01332003130200");
    }
    static QByteArray buildStartReceiveCmd2480Ble2M() {
        return QByteArray::fromHex("01332003270200");
    }

    static bool responseContainsPrefix(const QByteArray& rx, const QByteArray& prefix);
    /** 从停止接收应答中解析小端 16 位收包数；未找到前缀返回 -1。 */
    static int parseStopReceivePacketCountLe(const QByteArray& rx);
    /** PER = (仪器发包数 - 实际收包数) / 仪器发包数 */
    static double computePer(int instrumentSendCount, int receivedCount);

  signals:
    /** 累积数据中首次出现复位应答（040E0405030C00）时发一次，下次需先 clearProductSerialRxAccum */
    void instrumentAckResetSeen();
    /** 首次出现「开始接收」应答（040E0405332000） */
    void instrumentAckStartReceiveSeen();
    /** 首次形成完整「停止接收」应答（含 2 字节小端收包数） */
    void instrumentStopReceiveSeen(int receivedPacketCountLe);

  private:
    void scanRxForInstrumentEvents();

    QSerialPort* port_ = nullptr;
    QByteArray productSerialRxAccum_;
    bool emittedAckReset_ = false;
    bool emittedAckStart_ = false;
    bool emittedStopReceive_ = false;
};

#endif // QPRODUCT_H
