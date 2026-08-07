#ifndef XINJIE_PLC_RTU_DEVICE_H
#define XINJIE_PLC_RTU_DEVICE_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>

#include "xinjie_plc_rtu_types.h"

#include "serial_channel.h"

/**
 * 信捷 PLC Modbus RTU 设备层（串口）。
 * 配置键 XINJE_PLC/*；步骤 Param 可覆盖 comPort/baudRate/slaveId。
 * 地址串与开发参考资料 XJPLC 演示一致：M/D/X/Y/S/T/C。
 */
class XinjePlcRtuDevice : public QObject {
    Q_OBJECT
  public:
    using LogFn = std::function<void(const QString& line)>;

    explicit XinjePlcRtuDevice(QObject* parent = nullptr);

    void setSerialChannel(SerialChannel* channel);
    void setStationIndex(int stationIndex);
    void setLogFn(LogFn fn);

    static bool isQueryCmd(XinjePlcCmd cmd);
    bool set(XinjePlcCmd cmd, const QVariant& param, QString* errorMessage);
    bool get(XinjePlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage);

  private:
    struct Config {
        QString comPort;
        int baudRate = 19200;
        quint8 slaveId = 1;
        int requestTimeoutMs = 2000;
        SerialChannel::RtsDtrMode rtsMode = SerialChannel::RtsDtrMode::None;
        // 信捷 PLC 出厂通信格式为 19200 8-E-1
        QSerialPort::Parity parity = QSerialPort::EvenParity;
    };

    Config configFromSettings() const;
    Config configFromParam(const QVariant& param) const;
    void logLine(const QString& line) const;
    bool ensureOpen(const Config& cfg, QString* errorMessage);
    bool transact(const QByteArray& request, QByteArray* response, int timeoutMs, QString* errorMessage) const;
    bool transactPdu(quint8 slaveId, const QByteArray& pdu, QByteArray* responsePdu, int timeoutMs,
                     QString* errorMessage) const;
    /** 只发不收：PLC 写线圈/写寄存器不回帧时使用（Param expectReply=true 可恢复校验）。 */
    bool sendPduNoReply(quint8 slaveId, const QByteArray& pdu, QString* errorMessage) const;

    SerialChannel* serial_ = nullptr;
    int stationIndex_ = 1;
    LogFn log_;
    Config activeConfig_;
    bool opened_ = false;
};

#endif // XINJIE_PLC_RTU_DEVICE_H
