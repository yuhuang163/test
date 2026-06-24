#ifndef INOVANCE_H5U_TCP_H
#define INOVANCE_H5U_TCP_H

#include <QString>
#include <QVector>
#include <QTcpSocket>
#include <functional>

#include "inovance_h5u_tcp_types.h"

/**
 * 汇川 H5U Modbus TCP 传输与会话（单目录）。
 * PlcCmd 分发见 inovance_h5u_tcp_device（set/get，对齐 scpi/device）。
 */

class InovanceH5uModbusTcp {
  public:
    InovanceH5uModbusTcp() = default;

    void disconnect();
    bool isConnected() const {
        return socket_.state() == QAbstractSocket::ConnectedState;
    }

    bool connectPlc(const QString& host, quint16 port, quint8 unitId, int timeoutMs, QString* errorMessage);
    bool writeMCoil(int mNumber, bool value, int mCoilAddressOffset, quint8 unitId, int requestTimeoutMs,
                    QString* errorMessage);
    bool readMCoils(int mStartNumber, int quantity, int mCoilAddressOffset, quint8 unitId, int requestTimeoutMs,
                    QVector<bool>* out, QString* errorMessage);

  private:
    bool transact(const QByteArray& pdu, QByteArray* responsePdu, QString* errorMessage, int requestTimeoutMs,
                  quint8 unitId);
    quint16 nextTransactionId();
    static QByteArray buildAdu(quint16 transactionId, quint8 unitId, const QByteArray& pdu);
    static bool parseResponse(const QByteArray& fullAdu, quint16 expectTransactionId, quint8 expectUnitId,
                              QByteArray* responsePdu, QString* errorMessage);

    QTcpSocket socket_;
    quint16 transactionId_ = 0;
    bool traceEnabled_ = false;
};

struct PlcStationConfig {
    int stationIndex = 1;
    QString ipAddress;
    int port = 502;
    quint8 unitId = 1;
    int mCoilAddressOffset = 0;
    int mBase = 200;
    int switchForwardM = 0;
    int switchPressM = 0;
    int connectVerifyM = 0;
    int positionReadyBase = 0;
    int stepDoneBase = 0;
    int keyDoneM = 0;
    int switchTestDoneResetM = 211;

    static PlcStationConfig fromSettings(int stationIndex);
};

QString plcBoolText(bool on);

class PlcModbusSession {
  public:
    using LogFn = std::function<void(const QString& line)>;
    using IsContinueFn = std::function<bool()>;

    PlcModbusSession(InovanceH5uModbusTcp* tcp, const PlcStationConfig& cfg, LogFn log = {},
                     IsContinueFn isContinue = {});

    InovanceH5uModbusTcp* tcp() const {
        return tcp_;
    }
    const PlcStationConfig& config() const {
        return cfg_;
    }

    bool connectPlc(QString* errorMessage);
    void disconnect();
    bool isConnected() const;
    bool readCoil(int absoluteM, bool* value, QString* errorMessage);
    bool writeCoil(int absoluteM, bool value, QString* errorMessage);
    bool waitCoilTrue(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage);
    bool waitCoilFalse(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage);
    bool sendStepDone(QString* errorMessage);
    void logSessionSummary(const QString& stepTag) const;
    void logLine(const QString& line) const;

  private:
    int requestTimeoutMs() const;

    InovanceH5uModbusTcp* tcp_;
    PlcStationConfig cfg_;
    LogFn log_;
    IsContinueFn isContinue_;
};

#endif // INOVANCE_H5U_TCP_H
