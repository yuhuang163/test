#ifndef QMODBUSMANAGER_H
#define QMODBUSMANAGER_H

#include "inovance_h5u_tcp.h"
#include "modbus_types.h"
#include "qmodbus_rtu_rx_buffer.h"

#include <QObject>
#include <QString>
#include <utility>

class SerialChannel;

/**
 * Modbus 域唯一工站入口：路由 TCP PLC（汇川 H5U）与串口 RTU 电流表（HQ/LX device）。
 * 组帧/CRC/粘包在 modbus/codec；设备语义在 modbus/device。
 */
class QModbusManager : public QObject {
    Q_OBJECT

  public:
    using LogFn = InovanceH5uPlcExec::LogFn;
    using IsContinueFn = InovanceH5uPlcExec::IsContinueFn;

    explicit QModbusManager(QObject* parent = nullptr);

    void setStationIndex(int stationIndex);
    int stationIndex() const;

    void setLogFn(LogFn fn);
    void setIsContinueFn(IsContinueFn fn);

    // --- TCP PLC（inovance_h5u_tcp）---
    InovanceH5uModbusTcp* h5uTcp();
    const InovanceH5uModbusTcp* h5uTcp() const;

    bool exec(PlcCmd cmd, const QVariant& param = {}, QVariant* result = nullptr, QString* errorMessage = nullptr);

    bool connectPlc(QString* errorMessage = nullptr);
    void disconnectPlc();
    bool isPlcConnected() const;

    template <typename Fn>
    auto withSession(Fn&& fn) -> decltype(fn(std::declval<PlcModbusSession&>())) {
        PlcModbusSession session = makeSession();
        return fn(session);
    }

    // --- 串口 RTU 电流表（hq/lx_ammeter_rtu）---
    void attachSerialChannel(SerialChannel* channel);
    SerialChannel* serialChannel() const;

    void setRtuRoute(ModbusRtuRoute route);
    ModbusRtuRoute rtuRoute() const;

    void setLuxshareMachineId(int machineId1Based);
    int luxshareMachineId() const;

    void loadRtuRouteFromSettings();

    bool isRtuAmmeterRoute() const;

    bool exec(ModbusRtuAmmeterCmd cmd, QString* errorMessage = nullptr);

    bool feedRtuRx(const QByteArray& chunk);

    void resetRtuRxState();

  signals:
    void rtuAmmeterReadingReceived(const QString& valueText);

  private:
    PlcModbusSession makeSession() const;
    bool tryEmitRtuReading(const QByteArray& frame);
    bool tryEmitLxRtuReading();

    int stationIndex_ = 1;
    InovanceH5uModbusTcp h5uTcp_;
    LogFn log_;
    IsContinueFn isContinue_;

    ModbusRtuRoute rtuRoute_ = ModbusRtuRoute::None;
    int luxshareMachineId_ = 1;
    SerialChannel* serialChannel_ = nullptr;
    ModbusRtuRxBuffer rtuRxBuffer_;
};

#endif // QMODBUSMANAGER_H
