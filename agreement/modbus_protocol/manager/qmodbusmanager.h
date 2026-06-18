#ifndef QMODBUSMANAGER_H
#define QMODBUSMANAGER_H

#include <QByteArray>
#include <QDebug>
#include <QObject>
#include <QString>
#include <QVariant>
#include <utility>

#include "hq_ammeter_rtu.h"
#include "hq_ammeter_rtu_types.h"
#include "imodbus_rtu_device.h"
#include "inovance_h5u_tcp.h"
#include "inovance_h5u_tcp_device.h"
#include "lx_ammeter_rtu.h"
#include "lx_ammeter_rtu_types.h"
#include "modbus_types.h"
#include "qmodbus_rtu_rx_buffer.h"

#include <QObject>
#include <QString>
#include <utility>
#include "serial_channel.h"
#include <QVariant>
/**
 * Modbus domain entry: H5U TCP PLC + HQ/LX RTU ammeters.
 * RTU: IModbusRtuDevice + template exec; PLC: InovanceH5uTcpDevice set/get.
 */
class QModbusManager : public QObject {
    Q_OBJECT

  public:
    using LogFn = InovanceH5uTcpDevice::LogFn;
    using IsContinueFn = InovanceH5uTcpDevice::IsContinueFn;

    explicit QModbusManager(QObject* parent = nullptr);

    void setStationIndex(int stationIndex);
    int stationIndex() const;

    void setLogFn(LogFn fn);
    void setIsContinueFn(IsContinueFn fn);

    InovanceH5uModbusTcp* h5uTcp();
    const InovanceH5uModbusTcp* h5uTcp() const;
    InovanceH5uTcpDevice* h5uDevice();
    const InovanceH5uTcpDevice* h5uDevice() const;

    bool exec(PlcCmd cmd, const QVariant& param = {}, QVariant* result = nullptr, QString* errorMessage = nullptr);

    bool connectPlc(QString* errorMessage = nullptr);
    void disconnectPlc();
    bool isPlcConnected() const;

    template <typename Fn>
    auto withSession(Fn&& fn) -> decltype(fn(std::declval<PlcModbusSession&>())) {
        PlcModbusSession session = makeSession();
        return fn(session);
    }

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
            if (errorMessage) {
                *errorMessage = QStringLiteral("当前未配置正确的 RTU 设备路由");
            }
            return false;
        }
        if (!serialChannel_ || !serialChannel_->isOpen()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus RTU 串口未打开");
            }
            return false;
        }

        const QByteArray payload = dev->buildRequest(static_cast<int>(cmd), param);
        if (payload.isEmpty()) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus RTU 组帧失败或设备不支持该命令");
            }
            return false;
        }

        resetRtuRxState();
        qDebug().noquote() << "Modbus RTU TX:" << QString::fromLatin1(payload.toHex(' ').toUpper());
        if (serialChannel_->write(payload) < 0) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Modbus RTU 发送失败: %1").arg(serialChannel_->errorString());
            }
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
    void syncH5uDeviceBindings();
    void syncRtuDeviceBindings();

    int stationIndex_ = 1;
    InovanceH5uModbusTcp h5uTcp_;
    InovanceH5uTcpDevice h5uDevice_;
    HqAmmeterModbusRtu hqAmmeterRtu_;
    LxAmmeterModbusRtu lxAmmeterRtu_;
    LogFn log_;
    IsContinueFn isContinue_;

    ModbusDeviceRoute deviceRoute_ = ModbusDeviceRoute::None;
    int luxshareMachineId_ = 1;
    SerialChannel* serialChannel_ = nullptr;
};

#endif // QMODBUSMANAGER_H
