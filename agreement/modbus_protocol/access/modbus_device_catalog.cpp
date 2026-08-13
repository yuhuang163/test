#include "modbus_device_catalog.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr ModbusDeviceBinding kBindings[] = {
    {ModbusDeviceRoute::InovanceH5uTcp, ModbusLinkKind::Tcp, "inovance_h5u_tcp", "PlcCmd", u8"汇川 H5U PLC"},
    {ModbusDeviceRoute::GcSeriesTcp, ModbusLinkKind::Tcp, "gc_series_tcp", "GcPlcCmd", u8"GC系列 PLC"},
    {ModbusDeviceRoute::HqAmmeterRtu, ModbusLinkKind::Rtu, "hq_ammeter_rtu", "HqAmmeterRtuCmd", u8"华勤 RTU 电流表"},
    {ModbusDeviceRoute::LxAmmeterRtu, ModbusLinkKind::Rtu, "lx_ammeter_rtu", "LxAmmeterRtuCmd", u8"立讯 RTU 电流表"},
    {ModbusDeviceRoute::MultiTempLoggerRtu, ModbusLinkKind::Rtu, "multi_temp_logger_rtu", "MultiTempLoggerRtuCmd",
     u8"多路温度记录仪 RTU"},
    {ModbusDeviceRoute::XinjiePlcRtu, ModbusLinkKind::Rtu, "xinjie_plc_rtu", "XinjePlcCmd", u8"信捷 PLC RTU"},
};

} // namespace

namespace ModbusDeviceCatalog {

const ModbusDeviceBinding* bindingForRoute(ModbusDeviceRoute route) {
    for (const ModbusDeviceBinding& b : kBindings) {
        if (b.route == route) {
            return &b;
        }
    }
    return nullptr;
}

ModbusLinkKind linkKindForRoute(ModbusDeviceRoute route) {
    const ModbusDeviceBinding* b = bindingForRoute(route);
    return b ? b->linkKind : ModbusLinkKind::Tcp;
}

ModbusDeviceRoute deviceRouteFromProtocolType(const QString& protocolType) {
    const QString value = protocolType.trimmed().toLower();
    if (value == QStringLiteral("scpi")) {
        return ModbusDeviceRoute::None;
    }
    if (value == QStringLiteral("lx") || value == QStringLiteral("lxmodbus")) {
        return ModbusDeviceRoute::LxAmmeterRtu;
    }
    if (value == QStringLiteral("hq") || value == QStringLiteral("hqmodbus")) {
        return ModbusDeviceRoute::HqAmmeterRtu;
    }
    return ModbusDeviceRoute::None;
}

QString deviceRouteToIni(ModbusDeviceRoute route) {
    switch (route) {
    case ModbusDeviceRoute::InovanceH5uTcp:
        return QStringLiteral("InovanceH5uTcp");
    case ModbusDeviceRoute::GcSeriesTcp:
        return QStringLiteral("GcSeriesTcp");
    case ModbusDeviceRoute::HqAmmeterRtu:
        return QStringLiteral("HqAmmeterRtu");
    case ModbusDeviceRoute::LxAmmeterRtu:
        return QStringLiteral("LxAmmeterRtu");
    case ModbusDeviceRoute::MultiTempLoggerRtu:
        return QStringLiteral("MultiTempLoggerRtu");
    case ModbusDeviceRoute::XinjiePlcRtu:
        return QStringLiteral("XinjiePlcRtu");
    default:
        return QStringLiteral("None");
    }
}

ModbusDeviceRoute deviceRouteFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("InovanceH5uTcp"), Qt::CaseInsensitive) == 0) {
        return ModbusDeviceRoute::InovanceH5uTcp;
    }
    if (t.compare(QStringLiteral("GcSeriesTcp"), Qt::CaseInsensitive) == 0) {
        return ModbusDeviceRoute::GcSeriesTcp;
    }
    if (t.compare(QStringLiteral("HqAmmeterRtu"), Qt::CaseInsensitive) == 0) {
        return ModbusDeviceRoute::HqAmmeterRtu;
    }
    if (t.compare(QStringLiteral("LxAmmeterRtu"), Qt::CaseInsensitive) == 0) {
        return ModbusDeviceRoute::LxAmmeterRtu;
    }
    if (t.compare(QStringLiteral("MultiTempLoggerRtu"), Qt::CaseInsensitive) == 0) {
        return ModbusDeviceRoute::MultiTempLoggerRtu;
    }
    if (t.compare(QStringLiteral("XinjiePlcRtu"), Qt::CaseInsensitive) == 0) {
        return ModbusDeviceRoute::XinjiePlcRtu;
    }
    return ModbusDeviceRoute::None;
}

QString deviceRouteUiLabel(ModbusDeviceRoute route) {
    const ModbusDeviceBinding* b = bindingForRoute(route);
    return b && b->uiLabel ? QString::fromUtf8(b->uiLabel) : QStringLiteral("未选择");
}

} // namespace ModbusDeviceCatalog
