#ifndef PLC_MODBUS_SESSION_H
#define PLC_MODBUS_SESSION_H

#include "inovance_h5u_modbus_tcp.h"
#include "plc_station_config.h"

#include <functional>

/** 基于 InovanceH5uModbusTcp 的 PLC 线圈读写与会话辅助。 */
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

#endif // PLC_MODBUS_SESSION_H
