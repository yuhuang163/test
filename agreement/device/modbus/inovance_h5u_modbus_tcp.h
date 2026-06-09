#ifndef INOVANCE_H5U_MODBUS_TCP_H
#define INOVANCE_H5U_MODBUS_TCP_H

#include <QString>
#include <QVector>
#include <QTcpSocket>

/**
 * 汇川 H5U 等从站：Modbus TCP 访问 M 区线圈（与《INOVANCE H5U PLC MODBUS QUICK GUIDE》中
 * M0~M7999 对应线圈地址 0~7999 的常见映射一致；可通过 mCoilAddressOffset 平移）。
 */
class InovanceH5uModbusTcp {
  public:
    InovanceH5uModbusTcp() = default;

    void disconnect();
    bool isConnected() const {
        return socket_.state() == QAbstractSocket::ConnectedState;
    }

    /** 建立 Modbus TCP 连接；unitId 为从站号，常见为 1。 */
    bool connectPlc(const QString& host, quint16 port, quint8 unitId, int timeoutMs, QString* errorMessage);

    /** 写单个线圈；mNumber 为逻辑 M 编号（如 200 表示 M200）。 */
    bool writeMCoil(int mNumber, bool value, int mCoilAddressOffset, quint8 unitId, int requestTimeoutMs,
                    QString* errorMessage);

    /** 读连续线圈；起始为逻辑 M 编号。 */
    bool readMCoils(int mStartNumber, int quantity, int mCoilAddressOffset, quint8 unitId, int requestTimeoutMs,
                    QVector<bool>* out, QString* errorMessage);

    /** 为 true 时 qDebug 输出 ADU/PDU 摘要（由 PLC/ModbusTrace 或环境变量 PLC_MODBUS_TRACE=1 打开）。 */
    // void setTraceEnabled(bool on) { traceEnabled_ = on; }
    // bool traceEnabled() const { return traceEnabled_; }

  private:
    bool transact(const QByteArray& pdu, QByteArray* responsePdu, QString* errorMessage, int requestTimeoutMs,
                  quint8 unitId);

    quint16 nextTransactionId();
    static QByteArray buildAdu(quint16 transactionId, quint8 unitId, const QByteArray& pdu);
    static bool parseResponse(const QByteArray& fullAdu, quint16 expectTransactionId, quint8 expectUnitId,
                              QByteArray* responsePdu, QString* errorMessage);

    QTcpSocket socket_;
    quint16 transactionId_ = 0;
    bool traceEnabled_ = 0;
};

#endif
