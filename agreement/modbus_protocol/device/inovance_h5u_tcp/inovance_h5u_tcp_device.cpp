#include "inovance_h5u_tcp_device.h"

#include "Abini.h"
#include "inovance_h5u_tcp.h"

#include <QVariantMap>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

PlcCoilRequest parseCoilRequest(const QVariant& param, bool requireValue) {
    PlcCoilRequest req;
    if (param.canConvert<PlcCoilRequest>()) {
        return param.value<PlcCoilRequest>();
    }
    if (param.canConvert<int>()) {
        req.m = param.toInt();
        return req;
    }
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        req.m = map.value(QStringLiteral("m")).toInt();
        if (requireValue) {
            req.value = map.value(QStringLiteral("value")).toBool();
        }
    }
    return req;
}

PlcWaitCoilRequest parseWaitCoilRequest(const QVariant& param) {
    if (param.canConvert<PlcWaitCoilRequest>()) {
        return param.value<PlcWaitCoilRequest>();
    }
    PlcWaitCoilRequest req;
    if (param.canConvert<int>()) {
        req.m = param.toInt();
        return req;
    }
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        req.m = map.value(QStringLiteral("m")).toInt();
        if (map.contains(QStringLiteral("timeoutMs"))) {
            req.timeoutMs = map.value(QStringLiteral("timeoutMs")).toInt();
        }
        if (map.contains(QStringLiteral("pollMs"))) {
            req.pollMs = map.value(QStringLiteral("pollMs")).toInt();
        }
    }
    return req;
}

PlcReadCoilsRequest parseReadCoilsRequest(const QVariant& param) {
    if (param.canConvert<PlcReadCoilsRequest>()) {
        return param.value<PlcReadCoilsRequest>();
    }
    PlcReadCoilsRequest req;
    if (param.canConvert<int>()) {
        req.m = param.toInt();
        req.quantity = 1;
        return req;
    }
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        req.m = map.value(QStringLiteral("m")).toInt();
        req.quantity = qMax(1, map.value(QStringLiteral("quantity"), 1).toInt());
    }
    return req;
}

} // namespace

void registerPlcCmdMetaTypes() {
    qRegisterMetaType<PlcCoilRequest>();
    qRegisterMetaType<PlcWaitCoilRequest>();
    qRegisterMetaType<PlcReadCoilsRequest>();
}

InovanceH5uTcpDevice::InovanceH5uTcpDevice(QObject* parent) : QObject(parent) {
    registerPlcCmdMetaTypes();
}

void InovanceH5uTcpDevice::setTcp(InovanceH5uModbusTcp* tcp) {
    tcp_ = tcp;
}

void InovanceH5uTcpDevice::setStationIndex(int stationIndex) {
    stationIndex_ = qMax(1, stationIndex);
}

void InovanceH5uTcpDevice::setLogFn(LogFn fn) {
    log_ = std::move(fn);
}

void InovanceH5uTcpDevice::setIsContinueFn(IsContinueFn fn) {
    isContinue_ = std::move(fn);
}

bool InovanceH5uTcpDevice::isQueryCmd(PlcCmd cmd) {
    switch (cmd) {
    case PlcCmd::IsConnected:
    case PlcCmd::ReadCoil:
    case PlcCmd::ReadCoils:
        return true;
    default:
        return false;
    }
}

PlcModbusSession InovanceH5uTcpDevice::makeSession() const {
    return PlcModbusSession(tcp_, PlcStationConfig::fromSettings(stationIndex_), log_, isContinue_);
}

bool InovanceH5uTcpDevice::set(PlcCmd cmd, const QVariant& param, QString* errorMessage) {
    if (!tcp_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PLC TCP 未初始化");
        }
        return false;
    }

    PlcModbusSession session = makeSession();

    switch (cmd) {
    case PlcCmd::Connect: {
        if (param.canConvert<QVariantMap>()) {
            const QVariantMap map = param.toMap();
            PlcStationConfig cfg = PlcStationConfig::fromSettings(stationIndex_);
            if (map.contains(QStringLiteral("host"))) {
                cfg.ipAddress = map.value(QStringLiteral("host")).toString();
            }
            if (map.contains(QStringLiteral("port"))) {
                cfg.port = map.value(QStringLiteral("port")).toInt();
            }
            if (map.contains(QStringLiteral("unitId"))) {
                cfg.unitId = quint8(map.value(QStringLiteral("unitId")).toUInt());
            }
            PlcModbusSession customSession(tcp_, cfg, log_, isContinue_);
            return customSession.connectPlc(errorMessage);
        }
        return session.connectPlc(errorMessage);
    }
    case PlcCmd::Disconnect:
        session.disconnect();
        return true;
    case PlcCmd::WriteCoil: {
        const PlcCoilRequest req = parseCoilRequest(param, true);
        return session.writeCoil(req.m, req.value, errorMessage);
    }
    case PlcCmd::WaitCoilTrue: {
        const PlcWaitCoilRequest req = parseWaitCoilRequest(param);
        return session.waitCoilTrue(req.m, req.timeoutMs, req.pollMs, errorMessage);
    }
    case PlcCmd::WaitCoilFalse: {
        const PlcWaitCoilRequest req = parseWaitCoilRequest(param);
        return session.waitCoilFalse(req.m, req.timeoutMs, req.pollMs, errorMessage);
    }
    case PlcCmd::SendStepDone:
        return session.sendStepDone(errorMessage);
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral("PlcCmd 非 set 类指令: %1").arg(static_cast<int>(cmd));
        }
        return false;
    }
}

bool InovanceH5uTcpDevice::get(PlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage) {
    if (!tcp_) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("PLC TCP 未初始化");
        }
        return false;
    }

    PlcModbusSession session = makeSession();

    switch (cmd) {
    case PlcCmd::IsConnected:
        if (result) {
            *result = session.isConnected();
        }
        return true;
    case PlcCmd::ReadCoil: {
        const PlcCoilRequest req = parseCoilRequest(param, false);
        bool value = false;
        if (!session.readCoil(req.m, &value, errorMessage)) {
            return false;
        }
        if (result) {
            *result = value;
        }
        return true;
    }
    case PlcCmd::ReadCoils: {
        const PlcReadCoilsRequest req = parseReadCoilsRequest(param);
        QVector<bool> bits;
        const int requestMs = SETTINGS.value(QStringLiteral("PLC/RequestTimeoutMs"), 2000).toInt();
        const PlcStationConfig cfg = PlcStationConfig::fromSettings(stationIndex_);
        if (!tcp_->readMCoils(req.m, req.quantity, cfg.mCoilAddressOffset, cfg.unitId, requestMs, &bits, errorMessage)) {
            return false;
        }
        if (result) {
            *result = QVariant::fromValue(bits);
        }
        return true;
    }
    default:
        if (errorMessage) {
            *errorMessage = QStringLiteral("PlcCmd 非 get 类指令: %1").arg(static_cast<int>(cmd));
        }
        return false;
    }
}
