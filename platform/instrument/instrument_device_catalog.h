#ifndef INSTRUMENT_DEVICE_CATALOG_H
#define INSTRUMENT_DEVICE_CATALOG_H

#include "modbus_types.h"
#include "scpi_types.h"

#include <QString>
#include <QStringList>

/**
 * 工站侧「工厂 + 用途」与协议层设备的映射（platform/instrument）。
 * 同一物理仪表在不同工厂可能承担不同用途名；工站/设置页只选中文用途，再解析到 modbus/scpi device。
 */

enum class InstrumentProtocolKind {
    None,
    Modbus,
    Scpi,
};

/** 稳定用途键（Ini / test_case 存此值，界面显示 labelZh）。 */
namespace InstrumentPurposeId {
inline constexpr const char* ProgrammablePower = "programmable_power";
inline constexpr const char* AmmeterCom = "ammeter_com";
inline constexpr const char* AmmeterComScpi = "ammeter_scpi";
inline constexpr const char* PlcV3 = "plc_v3";
inline constexpr const char* CmwGprf = "cmw_gprf";
} // namespace InstrumentPurposeId

struct InstrumentDeviceRef {
    InstrumentProtocolKind protocol = InstrumentProtocolKind::None;
    ModbusDeviceRoute modbusRoute = ModbusDeviceRoute::None;
    ScpiDeviceRoute scpiRoute = ScpiDeviceRoute::None;

    QString factoryId;
    QString purposeId;
    QString labelZh;

    /** agreement 内 device 目录名，如 huiling_wfp60h_scpi */
    QString deviceDir;
    /** 协议 Cmd 枚举类型名，如 HuilingScpiCmd、PlcCmd */
    QString cmdEnumTypeName;
    /** 与 ModbusDeviceCatalog / Scpi 路由 Ini 一致的英文键 */
    QString protocolRouteKey;
    /** 常用 SETTINGS 前缀提示（可选） */
    QString settingsHint;
};

namespace InstrumentDeviceCatalog {

QString normalizeFactoryId(const QString& factoryId);
QString factoryFromSettings();

/** 某工厂下可选的用途中文名（已去重，含通配 * 与工厂专属项）。 */
QStringList purposeLabelsForFactory(const QString& factoryId);

/** 某工厂下用途 stable id 列表。 */
QStringList purposeIdsForFactory(const QString& factoryId);

QString purposeLabelZh(const QString& factoryId, const QString& purposeId);

/**
 * 工厂 + 用途中文名 → 协议设备。
 * factoryId 空时读 Mes/FACTORY；先匹配工厂专属行，再回退 factoryId="*"。
 */
bool resolveByLabel(const QString& factoryId, const QString& purposeLabelZh, InstrumentDeviceRef* out);

bool resolveByPurposeId(const QString& factoryId, const QString& purposeId, InstrumentDeviceRef* out);

/** 填充 deviceDir、cmdEnumTypeName、protocolRouteKey（基于 modbus/scpi 已有 catalog）。 */
void enrichProtocolMeta(InstrumentDeviceRef* ref);

} // namespace InstrumentDeviceCatalog

#endif // INSTRUMENT_DEVICE_CATALOG_H
