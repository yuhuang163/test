#include "qmodbusmanager.h"

#include "Abini.h"
#include "modbus_device_catalog.h"
#include "serial_channel.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QModbusManager::QModbusManager(QObject* parent) : QObject(parent), h5uDevice_(this) {
    syncH5uDeviceBindings();
    syncRtuDeviceBindings();
}

void QModbusManager::syncH5uDeviceBindings() {
    h5uDevice_.setTcp(&h5uTcp_);
    h5uDevice_.setStationIndex(stationIndex_);
    h5uDevice_.setLogFn(log_);
    h5uDevice_.setIsContinueFn(isContinue_);
}

void QModbusManager::syncRtuDeviceBindings() {
    lxAmmeterRtu_.setMachineId(luxshareMachineId_);
}

void QModbusManager::setStationIndex(int stationIndex) {
    stationIndex_ = qMax(1, stationIndex);
    syncH5uDeviceBindings();
}

int QModbusManager::stationIndex() const {
    return stationIndex_;
}

void QModbusManager::setLogFn(LogFn fn) {
    log_ = std::move(fn);
    syncH5uDeviceBindings();
}

void QModbusManager::setIsContinueFn(IsContinueFn fn) {
    isContinue_ = std::move(fn);
    syncH5uDeviceBindings();
}

InovanceH5uModbusTcp* QModbusManager::h5uTcp() {
    return &h5uTcp_;
}

const InovanceH5uModbusTcp* QModbusManager::h5uTcp() const {
    return &h5uTcp_;
}

InovanceH5uTcpDevice* QModbusManager::h5uDevice() {
    return &h5uDevice_;
}

const InovanceH5uTcpDevice* QModbusManager::h5uDevice() const {
    return &h5uDevice_;
}

PlcModbusSession QModbusManager::makeSession() const {
    return PlcModbusSession(const_cast<InovanceH5uModbusTcp*>(&h5uTcp_), PlcStationConfig::fromSettings(stationIndex_),
                            log_, isContinue_);
}

bool QModbusManager::exec(PlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage) {
    if (InovanceH5uTcpDevice::isQueryCmd(cmd)) {
        return h5uDevice_.get(cmd, param, result, errorMessage);
    }
    return h5uDevice_.set(cmd, param, errorMessage);
}

bool QModbusManager::connectPlc(QString* errorMessage) {
    return exec(PlcCmd::Connect, {}, nullptr, errorMessage);
}

void QModbusManager::disconnectPlc() {
    exec(PlcCmd::Disconnect);
}

bool QModbusManager::isPlcConnected() const {
    QVariant connected;
    const_cast<QModbusManager*>(this)->exec(PlcCmd::IsConnected, {}, &connected, nullptr);
    return connected.toBool();
}

void QModbusManager::attachSerialChannel(SerialChannel* channel) {
    serialChannel_ = channel;
}

SerialChannel* QModbusManager::serialChannel() const {
    return serialChannel_;
}

void QModbusManager::setDeviceRoute(ModbusDeviceRoute route) {
    deviceRoute_ = route;
    resetRtuRxState();
}

ModbusDeviceRoute QModbusManager::deviceRoute() const {
    return deviceRoute_;
}

void QModbusManager::setLuxshareMachineId(int machineId1Based) {
    luxshareMachineId_ = machineId1Based;
    syncRtuDeviceBindings();
}

int QModbusManager::luxshareMachineId() const {
    return luxshareMachineId_;
}

void QModbusManager::loadDeviceRouteFromSettings() {
    setDeviceRoute(ModbusDeviceCatalog::deviceRouteFromProtocolType(
        SETTINGS.value(QStringLiteral("Current/ProtocolType")).toString()));
    setLuxshareMachineId(SETTINGS.value(QStringLiteral("Current/LuxshareMachineId"), 1).toInt());
}

bool QModbusManager::isRtuAmmeterRoute() const {
    return deviceRoute_ == ModbusDeviceRoute::HqAmmeterRtu || deviceRoute_ == ModbusDeviceRoute::LxAmmeterRtu;
}

IModbusRtuDevice* QModbusManager::activeRtuDevice() {
    switch (deviceRoute_) {
    case ModbusDeviceRoute::HqAmmeterRtu:
        return &hqAmmeterRtu_;
    case ModbusDeviceRoute::LxAmmeterRtu:
        return &lxAmmeterRtu_;
    default:
        return nullptr;
    }
}

void QModbusManager::resetRtuRxState() {
    if (IModbusRtuDevice* dev = activeRtuDevice()) {
        dev->resetRxState();
    }
}

bool QModbusManager::exec(HqAmmeterRtuCmd cmd, QString* errorMessage) {
    if (deviceRoute_ != ModbusDeviceRoute::HqAmmeterRtu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前路由非华勤 RTU 电流表");
        }
        return false;
    }
    return exec<HqAmmeterRtuCmd>(cmd, {}, errorMessage);
}

bool QModbusManager::exec(LxAmmeterRtuCmd cmd, QString* errorMessage) {
    if (deviceRoute_ != ModbusDeviceRoute::LxAmmeterRtu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前路由非立讯 RTU 电流表");
        }
        return false;
    }
    return exec<LxAmmeterRtuCmd>(cmd, {}, errorMessage);
}

bool QModbusManager::feedRtuRx(const QByteArray& chunk) {
    if (!isRtuAmmeterRoute() || chunk.isEmpty()) {
        return false;
    }

    qDebug().noquote() << "Modbus RTU RX:" << QString::fromLatin1(chunk.toHex(' ').toUpper());

    IModbusRtuDevice* dev = activeRtuDevice();
    if (!dev) {
        return false;
    }

    QString valueText;
    if (dev->feedRx(chunk, &valueText)) {
        emit rtuAmmeterReadingReceived(valueText);
        return true;
    }

    if (deviceRoute_ == ModbusDeviceRoute::HqAmmeterRtu) {
        qDebug() << "Invalid Modbus RTU frame";
    }
    return true;
}
