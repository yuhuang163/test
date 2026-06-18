#ifndef INOVANCE_H5U_TCP_DEVICE_H
#define INOVANCE_H5U_TCP_DEVICE_H

#include <QObject>
#include <QVariant>
#include <functional>

#include "inovance_h5u_tcp_types.h"

class InovanceH5uModbusTcp;
class PlcModbusSession;

/** 汇川 H5U：PlcCmd → Modbus TCP 线圈读写（对齐 scpi/device 的 set/get 分发）。 */
class InovanceH5uTcpDevice : public QObject {
    Q_OBJECT

  public:
    using LogFn = std::function<void(const QString& line)>;
    using IsContinueFn = std::function<bool()>;

    explicit InovanceH5uTcpDevice(QObject* parent = nullptr);

    void setTcp(InovanceH5uModbusTcp* tcp);
    void setStationIndex(int stationIndex);
    void setLogFn(LogFn fn);
    void setIsContinueFn(IsContinueFn fn);

    static bool isQueryCmd(PlcCmd cmd);

    bool set(PlcCmd cmd, const QVariant& param = {}, QString* errorMessage = nullptr);
    bool get(PlcCmd cmd, const QVariant& param = {}, QVariant* result = nullptr, QString* errorMessage = nullptr);

  private:
    PlcModbusSession makeSession() const;

    InovanceH5uModbusTcp* tcp_ = nullptr;
    int stationIndex_ = 1;
    LogFn log_;
    IsContinueFn isContinue_;
};

#endif // INOVANCE_H5U_TCP_DEVICE_H
