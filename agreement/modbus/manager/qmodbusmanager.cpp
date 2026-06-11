#include "qmodbusmanager.h"

#include "Abini.h"
#include "modbus_device_catalog.h"
#include "serial_channel.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QModbusManager::QModbusManager(QObject* parent) : QObject(parent) {
    InovanceH5uPlcExec::registerMetaTypes();
}

void QModbusManager::setStationIndex(int stationIndex) {
    stationIndex_ = qMax(1, stationIndex);
}

int QModbusManager::stationIndex() const {
    return stationIndex_;
}

void QModbusManager::setLogFn(LogFn fn) {
    log_ = std::move(fn);
}

void QModbusManager::setIsContinueFn(IsContinueFn fn) {
    isContinue_ = std::move(fn);
}

InovanceH5uModbusTcp* QModbusManager::h5uTcp() {
    return &h5uTcp_;
}

const InovanceH5uModbusTcp* QModbusManager::h5uTcp() const {
    return &h5uTcp_;
}

PlcModbusSession QModbusManager::makeSession() const {
    return PlcModbusSession(const_cast<InovanceH5uModbusTcp*>(&h5uTcp_), PlcStationConfig::fromSettings(stationIndex_),
                            log_, isContinue_);
}

bool QModbusManager::exec(PlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage) {
    return InovanceH5uPlcExec::exec(h5uTcp_, stationIndex_, log_, isContinue_, cmd, param, result, errorMessage);
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

void QModbusManager::resetRtuRxState() {
    rtuRxBuffer_.reset();
}

bool QModbusManager::exec(HqAmmeterRtuCmd cmd, QString* errorMessage) {
    if (deviceRoute_ != ModbusDeviceRoute::HqAmmeterRtu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前路由非华勤 RTU 电流表");
        }
        return false;
    }
    if (!serialChannel_ || !serialChannel_->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus RTU 串口未打开");
        }
        return false;
    }

    QByteArray payload;
    switch (cmd) {
    case HqAmmeterRtuCmd::ReadMeasurement:
        payload = HqAmmeterModbusRtu::buildReadMeasurementRequest();
        break;
    case HqAmmeterRtuCmd::SetBaud115200:
        payload = HqAmmeterModbusRtu::buildSetBaud115200Request();
        break;
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral("华勤 RTU 表不支持该命令");
        }
        return false;
    }

    if (payload.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus RTU 组帧失败");
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

bool QModbusManager::exec(LxAmmeterRtuCmd cmd, QString* errorMessage) {
    if (deviceRoute_ != ModbusDeviceRoute::LxAmmeterRtu) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("当前路由非立讯 RTU 电流表");
        }
        return false;
    }
    if (!serialChannel_ || !serialChannel_->isOpen()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus RTU 串口未打开");
        }
        return false;
    }

    QByteArray payload;
    switch (cmd) {
    case LxAmmeterRtuCmd::ReadMeasurement:
        payload = LxAmmeterModbusRtu::buildReadMeasurementRequest(luxshareMachineId_);
        break;
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral("立讯 RTU 表不支持该命令");
        }
        return false;
    }

    if (payload.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Modbus RTU 组帧失败");
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

bool QModbusManager::tryEmitRtuReading(const QByteArray& frame) {
    const ModbusRtuCodec::AmmeterReading reading =
        deviceRoute_ == ModbusDeviceRoute::LxAmmeterRtu ? LxAmmeterModbusRtu::parseResponse(frame)
                                        : HqAmmeterModbusRtu::parseResponse(frame);
    if (!reading.ok) {
        return false;
    }
    emit rtuAmmeterReadingReceived(reading.valueText);
    return true;
}

bool QModbusManager::tryEmitLxRtuReading() {
    const QByteArray& frame = rtuRxBuffer_.buffer();
    if (frame.isEmpty()) {
        return false;
    }
    if (!tryEmitRtuReading(frame)) {
        return false;
    }
    rtuRxBuffer_.reset();
    return true;
}

bool QModbusManager::feedRtuRx(const QByteArray& chunk) {
    if (!isRtuAmmeterRoute() || chunk.isEmpty()) {
        return false;
    }

    qDebug().noquote() << "Modbus RTU RX:" << QString::fromLatin1(chunk.toHex(' ').toUpper());

    if (deviceRoute_ == ModbusDeviceRoute::LxAmmeterRtu) {
        rtuRxBuffer_.feed(chunk);
        return tryEmitLxRtuReading() || !rtuRxBuffer_.buffer().isEmpty();
    }

    if (tryEmitRtuReading(chunk)) {
        return true;
    }
    qDebug() << "Invalid Modbus RTU frame";
    return true;
}
