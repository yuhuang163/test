#ifndef GC_SERIES_TCP_DEVICE_H
#define GC_SERIES_TCP_DEVICE_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>

#include "gc_series_tcp_types.h"

class InovanceH5uModbusTcp;

/**
 * GC 系列 PLC 设备层：独立配置键 GC_PLC/*，M 区默认偏移 4096。
 * 仅复用底层 Modbus TCP 收发（InovanceH5uModbusTcp），不走汇川 PLC/* 会话逻辑。
 */
class GcSeriesTcpDevice : public QObject {
    Q_OBJECT
  public:
    using LogFn = std::function<void(const QString& line)>;
    using IsContinueFn = std::function<bool()>;

    explicit GcSeriesTcpDevice(QObject* parent = nullptr);

    void setTcp(InovanceH5uModbusTcp* tcp);
    void setStationIndex(int stationIndex);
    void setLogFn(LogFn fn);

    static bool isQueryCmd(GcPlcCmd cmd);
    bool set(GcPlcCmd cmd, const QVariant& param, QString* errorMessage);
    bool get(GcPlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage);

  private:
    struct Config {
        QString ipAddress;
        int port = 502;
        quint8 unitId = 1;
        int mCoilAddressOffset = 4096;
        int connectTimeoutMs = 3000;
        int requestTimeoutMs = 2000;
    };

    Config configFromSettings() const;
    Config configFromParam(const QVariant& param) const;
    void logLine(const QString& line) const;

    InovanceH5uModbusTcp* tcp_ = nullptr;
    int stationIndex_ = 1;
    LogFn log_;
};

#endif // GC_SERIES_TCP_DEVICE_H
