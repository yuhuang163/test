#ifndef QMODBUSMANAGER_H
#define QMODBUSMANAGER_H

#include "hq_ammeter_rtu.h"
#include "imodbus_rtu_device.h"
#include "inovance_h5u_tcp.h"
#include "lx_ammeter_rtu.h"
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

    void setDeviceRoute(ModbusDeviceRoute route);
    ModbusDeviceRoute deviceRoute() const;

    void setLuxshareMachineId(int machineId1Based);
    int luxshareMachineId() const;

    void loadDeviceRouteFromSettings();

    bool isRtuAmmeterRoute() const;

    IModbusRtuDevice* activeRtuDevice();

    template <typename CmdType>
    bool exec(CmdType cmd, const QVariant& param = {}, QString* errorMessage = nullptr) {
        IModbusRtuDevice* dev = activeRtuDevice();
        if (!dev) {
            if (errorMessage) *errorMessage = QStringLiteral("当前未配置正确的 RTU 设备路由");
            return false;
        }
        if (!serialChannel_ || !serialChannel_->isOpen()) {
            if (errorMessage) *errorMessage = QStringLiteral("Modbus RTU 串口未打开");
            return false;
        }
        
        QByteArray payload = dev->buildRequest(static_cast<int>(cmd), param);
        if (payload.isEmpty()) {
            if (errorMessage) *errorMessage = QStringLiteral("Modbus RTU 组帧失败或设备不支持该命令");
            return false;
        }

        resetRtuRxState();
        if (serialChannel_->write(payload) < 0) {
            if (errorMessage) *errorMessage = QStringLiteral("Modbus RTU 发送失败: %1").arg(serialChannel_->errorString());
            return false;
        }
        return true;
    }

    bool exec(HqAmmeterRtuCmd cmd, QString* errorMessage = nullptr);
    bool exec(LxAmmeterRtuCmd cmd, QString* errorMessage = nullptr);

    bool feedRtuRx(const QByteArray& chunk);

    void resetRtuRxState();

  signals:
    void rtuAmmeterReadingReceived(const QString& valueText);

  private:
    PlcModbusSession makeSession() const;
    bool tryEmitRtuReading(const QByteArray& frame);
    bool tryEmitLxRtuReading();

    HqAmmeterModbusRtu hqAmmeterDevice_;
    LxAmmeterModbusRtu lxAmmeterDevice_;

    int stationIndex_ = 1;
    InovanceH5uModbusTcp h5uTcp_;
    LogFn log_;
    IsContinueFn isContinue_;

    ModbusDeviceRoute deviceRoute_ = ModbusDeviceRoute::None;
    int luxshareMachineId_ = 1;
    SerialChannel* serialChannel_ = nullptr;
    ModbusRtuRxBuffer rtuRxBuffer_;
};

#endif // QMODBUSMANAGER_H
