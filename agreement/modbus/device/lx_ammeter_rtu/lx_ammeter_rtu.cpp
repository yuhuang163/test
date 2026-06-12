#include "lx_ammeter_rtu.h"

#include <QDebug>
#include <QStringList>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

void LxAmmeterModbusRtu::setMachineId(int machineId1Based) {
    machineId1Based_ = machineId1Based;
}

int LxAmmeterModbusRtu::machineId() const {
    return machineId1Based_;
}

void LxAmmeterModbusRtu::resetRxState() {
    rtuRxBuffer_.reset();
}

QByteArray LxAmmeterModbusRtu::buildReadMeasurementRequest(int machineId1Based) {
    static const QStringList machineCmdList = {
        QString(), QStringLiteral("010300000002c40b"), QStringLiteral("020300000002C438"),
        QStringLiteral("030300000002C5E9"), QStringLiteral("040300000002C45E"),
        QStringLiteral("050300000002C58F"), QStringLiteral("060300000002C5BC")};
    if (machineId1Based <= 0 || machineId1Based >= machineCmdList.size()) {
        qDebug() << "不支持的立讯设备号:" << machineId1Based;
        return {};
    }
    return QByteArray::fromHex(machineCmdList.at(machineId1Based).toLatin1());
}

ModbusRtuCodec::AmmeterReading LxAmmeterModbusRtu::parseResponseFrame(const QByteArray& frame) {
    return ModbusRtuCodec::parseLxHoldRegisterFrame(frame);
}

QByteArray LxAmmeterModbusRtu::buildRequest(int cmd, const QVariant& param) {
    Q_UNUSED(param);
    switch (static_cast<LxAmmeterRtuCmd>(cmd)) {
    case LxAmmeterRtuCmd::ReadMeasurement:
        return buildReadMeasurementRequest(machineId1Based_);
    default:
        return {};
    }
}

bool LxAmmeterModbusRtu::parseResponse(const QByteArray& frame, QString* valueText) {
    const ModbusRtuCodec::AmmeterReading reading = parseResponseFrame(frame);
    if (!reading.ok) {
        return false;
    }
    if (valueText) {
        *valueText = reading.valueText;
    }
    return true;
}

bool LxAmmeterModbusRtu::feedRx(const QByteArray& chunk, QString* valueText) {
    if (chunk.isEmpty()) {
        return false;
    }
    rtuRxBuffer_.feed(chunk);
    const QByteArray& frame = rtuRxBuffer_.buffer();
    if (frame.isEmpty()) {
        return false;
    }
    if (!parseResponse(frame, valueText)) {
        return false;
    }
    rtuRxBuffer_.reset();
    return true;
}
