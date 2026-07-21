#include "gc_series_tcp_device.h"

#include "Abini.h"
#include "inovance_h5u_tcp.h"

#include <QVariantMap>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

GcPlcCoilRequest parseGcCoilRequest(const QVariant& param) {
    GcPlcCoilRequest req;
    if (param.canConvert<GcPlcCoilRequest>())
        return param.value<GcPlcCoilRequest>();
    if (param.canConvert<int>()) {
        req.m = param.toInt();
        req.value = true;
        return req;
    }
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        req.m = map.value(QStringLiteral("m")).toInt();
        req.value = map.value(QStringLiteral("value"), true).toBool();
    }
    return req;
}

} // namespace

void registerGcPlcCmdMetaTypes() {
    qRegisterMetaType<GcPlcCoilRequest>();
}

GcSeriesTcpDevice::GcSeriesTcpDevice(QObject* parent) : QObject(parent) {
    registerGcPlcCmdMetaTypes();
}

void GcSeriesTcpDevice::setTcp(InovanceH5uModbusTcp* tcp) {
    tcp_ = tcp;
}

void GcSeriesTcpDevice::setStationIndex(int stationIndex) {
    stationIndex_ = qMax(1, stationIndex);
}

void GcSeriesTcpDevice::setLogFn(LogFn fn) {
    log_ = std::move(fn);
}

void GcSeriesTcpDevice::logLine(const QString& line) const {
    if (log_)
        log_(line);
}

bool GcSeriesTcpDevice::isQueryCmd(GcPlcCmd cmd) {
    return cmd == GcPlcCmd::IsConnected;
}

GcSeriesTcpDevice::Config GcSeriesTcpDevice::configFromSettings() const {
    Config cfg;
    const int st = stationIndex_;
    cfg.ipAddress =
        SETTINGS.value(QStringLiteral("GC_PLC/IpAddress_Station%1").arg(st),
                       SETTINGS.value(QStringLiteral("GC_PLC/IpAddress"), QStringLiteral("192.168.2.90")))
            .toString();
    cfg.port = SETTINGS
                   .value(QStringLiteral("GC_PLC/Port_Station%1").arg(st),
                          SETTINGS.value(QStringLiteral("GC_PLC/Port"), 502))
                   .toInt();
    cfg.unitId = quint8(SETTINGS
                            .value(QStringLiteral("GC_PLC/UnitId_Station%1").arg(st),
                                   SETTINGS.value(QStringLiteral("GC_PLC/UnitId"), 1))
                            .toUInt());
    // GC 协议：M0 起始地址 4096；与汇川 PLC/MCoilAddressOffset 完全独立
    cfg.mCoilAddressOffset =
        SETTINGS
            .value(QStringLiteral("GC_PLC/MCoilAddressOffset_Station%1").arg(st),
                   SETTINGS.value(QStringLiteral("GC_PLC/MCoilAddressOffset"), 4096))
            .toInt();
    cfg.connectTimeoutMs = SETTINGS.value(QStringLiteral("GC_PLC/ConnectTimeoutMs"), 3000).toInt();
    cfg.requestTimeoutMs = SETTINGS.value(QStringLiteral("GC_PLC/RequestTimeoutMs"), 2000).toInt();
    return cfg;
}

GcSeriesTcpDevice::Config GcSeriesTcpDevice::configFromParam(const QVariant& param) const {
    Config cfg = configFromSettings();
    if (!param.canConvert<QVariantMap>())
        return cfg;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("host")))
        cfg.ipAddress = map.value(QStringLiteral("host")).toString();
    if (map.contains(QStringLiteral("port")))
        cfg.port = map.value(QStringLiteral("port")).toInt();
    if (map.contains(QStringLiteral("unitId")))
        cfg.unitId = quint8(map.value(QStringLiteral("unitId")).toUInt());
    if (map.contains(QStringLiteral("mCoilAddressOffset")))
        cfg.mCoilAddressOffset = map.value(QStringLiteral("mCoilAddressOffset")).toInt();
    return cfg;
}

bool GcSeriesTcpDevice::set(GcPlcCmd cmd, const QVariant& param, QString* errorMessage) {
    if (!tcp_) {
        if (errorMessage)
            *errorMessage = QStringLiteral("GC PLC TCP 未初始化");
        return false;
    }

    switch (cmd) {
    case GcPlcCmd::Connect: {
        const Config cfg = configFromParam(param);
        logLine(QStringLiteral("GC PLC 连接: IP=%1 Port=%2 UnitId=%3 Offset=%4")
                    .arg(cfg.ipAddress)
                    .arg(cfg.port)
                    .arg(cfg.unitId)
                    .arg(cfg.mCoilAddressOffset));
        return tcp_->connectPlc(cfg.ipAddress, quint16(cfg.port), cfg.unitId, cfg.connectTimeoutMs, errorMessage);
    }
    case GcPlcCmd::Disconnect:
        tcp_->disconnect();
        return true;
    case GcPlcCmd::WriteCoil: {
        const Config cfg = configFromSettings();
        const GcPlcCoilRequest req = parseGcCoilRequest(param);
        logLine(QStringLiteral("GC WriteCoil M%1 value=%2 addr=%3")
                    .arg(req.m)
                    .arg(req.value ? 1 : 0)
                    .arg(req.m + cfg.mCoilAddressOffset));
        return tcp_->writeMCoil(req.m, req.value, cfg.mCoilAddressOffset, cfg.unitId, cfg.requestTimeoutMs,
                                errorMessage);
    }
    default:
        if (errorMessage)
            *errorMessage = QStringLiteral("GcPlcCmd 非 set 类指令: %1").arg(static_cast<int>(cmd));
        return false;
    }
}

bool GcSeriesTcpDevice::get(GcPlcCmd cmd, const QVariant& param, QVariant* result, QString* errorMessage) {
    Q_UNUSED(param);
    if (!tcp_) {
        if (errorMessage)
            *errorMessage = QStringLiteral("GC PLC TCP 未初始化");
        return false;
    }
    if (cmd == GcPlcCmd::IsConnected) {
        if (result)
            *result = tcp_->isConnected();
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("GcPlcCmd 非 get 类指令: %1").arg(static_cast<int>(cmd));
    return false;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
