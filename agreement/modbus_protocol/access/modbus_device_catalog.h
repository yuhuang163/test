#ifndef MODBUS_DEVICE_CATALOG_H
#define MODBUS_DEVICE_CATALOG_H

#include "modbus_types.h"

#include <QString>

enum class ModbusLinkKind {
    Tcp,
    Rtu,
};

/** 设备绑定（协议链路仅在内部使用，工站不选）。 */
struct ModbusDeviceBinding {
    ModbusDeviceRoute route = ModbusDeviceRoute::None;
    ModbusLinkKind linkKind = ModbusLinkKind::Tcp;
    const char* deviceDir = nullptr;
    const char* cmdEnumTypeName = nullptr;
    const char* uiLabel = nullptr;
};

namespace ModbusDeviceCatalog {

const ModbusDeviceBinding* bindingForRoute(ModbusDeviceRoute route);
ModbusLinkKind linkKindForRoute(ModbusDeviceRoute route);

/** 产线 Current/ProtocolType → 电流表设备（scpi 等非 modbus 返回 None）。 */
ModbusDeviceRoute deviceRouteFromProtocolType(const QString& protocolType);

QString deviceRouteToIni(ModbusDeviceRoute route);
ModbusDeviceRoute deviceRouteFromIni(const QString& text);
QString deviceRouteUiLabel(ModbusDeviceRoute route);

} // namespace ModbusDeviceCatalog

#endif // MODBUS_DEVICE_CATALOG_H
