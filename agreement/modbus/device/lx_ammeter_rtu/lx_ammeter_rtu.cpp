#include "lx_ammeter_rtu.h"

#include <QDebug>
#include <QStringList>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

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

// --- IModbusRtuDevice interface bridge ---

QByteArray LxAmmeterModbusRtu::buildRequest(int cmd, const QVariant& param) {
    switch (static_cast<LxAmmeterRtuCmd>(cmd)) {
    case LxAmmeterRtuCmd::ReadMeasurement:
        return buildReadMeasurementRequest(param.toInt());
    default:
        return {};
    }
}

bool LxAmmeterModbusRtu::parseResponse(const QByteArray& frame, QString* valueText) {
    const auto reading = parseResponseFrame(frame);
    if (!reading.ok) return false;
    if (valueText) *valueText = reading.valueText;
    return true;
}
