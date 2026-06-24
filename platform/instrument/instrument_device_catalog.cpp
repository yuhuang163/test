#include "instrument_device_catalog.h"

#include "Abini.h"
#include "modbus_device_catalog.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

enum class RowProtocol {
    Modbus,
    Scpi,
};

struct PurposeRow {
    const char* factoryId;
    const char* purposeId;
    const char* labelZh;
    RowProtocol protocol;
    int routeValue;
    const char* settingsHint;
};

// factoryId: 小写 Mes/FACTORY；"*" 表示各工厂默认/通用项（工厂专属行优先）。
constexpr PurposeRow kPurposeRows[] = {
    {"*", InstrumentPurposeId::ProgrammablePower, u8"程控电源", RowProtocol::Scpi,
     static_cast<int>(ScpiDeviceRoute::HuilingWfp60h), "VisaPower/*"},
    {"byd", InstrumentPurposeId::ProgrammablePower, u8"程控电源", RowProtocol::Scpi,
     static_cast<int>(ScpiDeviceRoute::HuilingWfp60h), "VisaPower/*"},

    {"*", InstrumentPurposeId::AmmeterComScpi, u8"COM电流表(SCP)", RowProtocol::Scpi,
     static_cast<int>(ScpiDeviceRoute::HuilingWfp60h), "Current/ProtocolType=scpi"},
    {"hq", InstrumentPurposeId::AmmeterCom, u8"COM电流表", RowProtocol::Modbus,
     static_cast<int>(ModbusDeviceRoute::HqAmmeterRtu), "Current/ProtocolType=hq"},
    {"lx", InstrumentPurposeId::AmmeterCom, u8"COM电流表", RowProtocol::Modbus,
     static_cast<int>(ModbusDeviceRoute::LxAmmeterRtu), "Current/ProtocolType=lx"},
    {"*", InstrumentPurposeId::AmmeterCom, u8"COM电流表", RowProtocol::Scpi,
     static_cast<int>(ScpiDeviceRoute::HuilingWfp60h), "Current/ProtocolType=scpi|auto"},

    {"*", InstrumentPurposeId::PlcV3, u8"V3治具PLC", RowProtocol::Modbus,
     static_cast<int>(ModbusDeviceRoute::InovanceH5uTcp), "PLC/*_StationN"},

    {"*", InstrumentPurposeId::CmwGprf, u8"CMW100射频源", RowProtocol::Scpi,
     static_cast<int>(ScpiDeviceRoute::RsCmw100), "BlePer/CmwVisaAddress"},
};

QString scpiDeviceDir(ScpiDeviceRoute route) {
    switch (route) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("huiling_wfp60h_scpi");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("rs_cmw100_scpi");
    default:
        return QString();
    }
}

QString scpiCmdEnumTypeName(ScpiDeviceRoute route) {
    switch (route) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("HuilingScpiCmd");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("CmwScpiCmd");
    default:
        return QString();
    }
}

QString scpiRouteKey(ScpiDeviceRoute route) {
    switch (route) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("HuilingWfp60h");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("RsCmw100");
    default:
        return QStringLiteral("None");
    }
}

bool rowMatchesFactory(const PurposeRow& row, const QString& factoryNorm) {
    const QString rowFactory = QString::fromLatin1(row.factoryId);
    return rowFactory == QStringLiteral("*") || rowFactory == factoryNorm;
}

bool fillRefFromRow(const PurposeRow& row, const QString& factoryNorm, InstrumentDeviceRef* out) {
    if (!out) {
        return false;
    }
    out->factoryId = factoryNorm;
    out->purposeId = QString::fromLatin1(row.purposeId);
    out->labelZh = QString::fromUtf8(row.labelZh);
    out->settingsHint = row.settingsHint ? QString::fromLatin1(row.settingsHint) : QString();
    out->modbusRoute = ModbusDeviceRoute::None;
    out->scpiRoute = ScpiDeviceRoute::None;

    if (row.protocol == RowProtocol::Modbus) {
        out->protocol = InstrumentProtocolKind::Modbus;
        out->modbusRoute = static_cast<ModbusDeviceRoute>(row.routeValue);
    } else if (row.protocol == RowProtocol::Scpi) {
        out->protocol = InstrumentProtocolKind::Scpi;
        out->scpiRoute = static_cast<ScpiDeviceRoute>(row.routeValue);
    } else {
        out->protocol = InstrumentProtocolKind::None;
        return false;
    }

    InstrumentDeviceCatalog::enrichProtocolMeta(out);
    return out->protocol != InstrumentProtocolKind::None;
}

template <typename Pred>
const PurposeRow* findRow(const QString& factoryNorm, Pred predicate) {
    const PurposeRow* wildcard = nullptr;
    for (const PurposeRow& row : kPurposeRows) {
        if (!rowMatchesFactory(row, factoryNorm)) {
            continue;
        }
        if (predicate(row)) {
            if (QString::fromLatin1(row.factoryId) != QStringLiteral("*")) {
                return &row;
            }
            wildcard = &row;
        }
    }
    return wildcard;
}

} // namespace

namespace InstrumentDeviceCatalog {

QString normalizeFactoryId(const QString& factoryId) {
    return factoryId.trimmed().toLower();
}

QString factoryFromSettings() {
    return normalizeFactoryId(SETTINGS.value(QStringLiteral("Mes/FACTORY")).toString());
}

QStringList purposeLabelsForFactory(const QString& factoryId) {
    const QString factoryNorm = factoryId.isEmpty() ? factoryFromSettings() : normalizeFactoryId(factoryId);
    QStringList labels;
    for (const PurposeRow& row : kPurposeRows) {
        if (!rowMatchesFactory(row, factoryNorm)) {
            continue;
        }
        const QString label = QString::fromUtf8(row.labelZh);
        if (!labels.contains(label)) {
            labels.append(label);
        }
    }
    labels.sort(Qt::CaseInsensitive);
    return labels;
}

QStringList purposeIdsForFactory(const QString& factoryId) {
    const QString factoryNorm = factoryId.isEmpty() ? factoryFromSettings() : normalizeFactoryId(factoryId);
    QStringList ids;
    for (const PurposeRow& row : kPurposeRows) {
        if (!rowMatchesFactory(row, factoryNorm)) {
            continue;
        }
        const QString id = QString::fromLatin1(row.purposeId);
        if (!ids.contains(id)) {
            ids.append(id);
        }
    }
    return ids;
}

QString purposeLabelZh(const QString& factoryId, const QString& purposeId) {
    const QString factoryNorm = factoryId.isEmpty() ? factoryFromSettings() : normalizeFactoryId(factoryId);
    const QString idNorm = purposeId.trimmed().toLower();
    const PurposeRow* row = findRow(factoryNorm, [&](const PurposeRow& r) {
        return QString::fromLatin1(r.purposeId).toLower() == idNorm;
    });
    return row ? QString::fromUtf8(row->labelZh) : QString();
}

bool resolveByLabel(const QString& factoryId, const QString& purposeLabelZh, InstrumentDeviceRef* out) {
    const QString factoryNorm = factoryId.isEmpty() ? factoryFromSettings() : normalizeFactoryId(factoryId);
    const QString label = purposeLabelZh.trimmed();
    if (label.isEmpty()) {
        return false;
    }
    const PurposeRow* row = findRow(factoryNorm, [&](const PurposeRow& r) {
        return QString::fromUtf8(r.labelZh) == label;
    });
    return row && fillRefFromRow(*row, factoryNorm, out);
}

bool resolveByPurposeId(const QString& factoryId, const QString& purposeId, InstrumentDeviceRef* out) {
    const QString factoryNorm = factoryId.isEmpty() ? factoryFromSettings() : normalizeFactoryId(factoryId);
    const QString idNorm = purposeId.trimmed().toLower();
    if (idNorm.isEmpty()) {
        return false;
    }
    const PurposeRow* row = findRow(factoryNorm, [&](const PurposeRow& r) {
        return QString::fromLatin1(r.purposeId).toLower() == idNorm;
    });
    return row && fillRefFromRow(*row, factoryNorm, out);
}

void enrichProtocolMeta(InstrumentDeviceRef* ref) {
    if (!ref) {
        return;
    }
    if (ref->protocol == InstrumentProtocolKind::Modbus) {
        const ModbusDeviceBinding* binding = ModbusDeviceCatalog::bindingForRoute(ref->modbusRoute);
        if (binding) {
            ref->deviceDir = binding->deviceDir ? QString::fromLatin1(binding->deviceDir) : QString();
            ref->cmdEnumTypeName = binding->cmdEnumTypeName ? QString::fromLatin1(binding->cmdEnumTypeName) : QString();
        }
        ref->protocolRouteKey = ModbusDeviceCatalog::deviceRouteToIni(ref->modbusRoute);
        return;
    }
    if (ref->protocol == InstrumentProtocolKind::Scpi) {
        ref->deviceDir = scpiDeviceDir(ref->scpiRoute);
        ref->cmdEnumTypeName = scpiCmdEnumTypeName(ref->scpiRoute);
        ref->protocolRouteKey = scpiRouteKey(ref->scpiRoute);
    }
}

} // namespace InstrumentDeviceCatalog
