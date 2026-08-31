#include "cmd_catalog_base.h"
#include "test_case_ini_param.h"
#include <utility>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QString CmdCatalogUi::pickerDisplayLabel(QString label) {
    label = label.trimmed();
    if (label.startsWith(QStringLiteral("Dongle "), Qt::CaseInsensitive))
        label = label.mid(7).trimmed();
    static const QStringList prefixes = {
        QStringLiteral("设置"),
        QStringLiteral("写入"),
        QStringLiteral("读取"),
        QStringLiteral("获取"),
        QStringLiteral("上报"),
    };
    for (const QString& prefix : prefixes) {
        if (label.startsWith(prefix)) {
            label = label.mid(prefix.size()).trimmed();
            break;
        }
    }
    return label;
}

bool CmdCatalogParamIni::readFromIni(CmdCatalogParamIniProfile profile, const QSettings& settings,
                                     DeviceCmdParamKind kind, QVariant& out) {
    if (profile == CmdCatalogParamIniProfile::None)
        return false;
    switch (kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::Int:
        out = readSendScopedParam(settings, QStringLiteral("int"), 0).toInt();
        return true;
    case DeviceCmdParamKind::UInt:
        if (profile == CmdCatalogParamIniProfile::WithUIntAndLegacyJson) {
            out = readSendScopedParam(settings, QStringLiteral("uint"), 0).toUInt();
            return true;
        }
        return false;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        if (profile == CmdCatalogParamIniProfile::WithUIntAndLegacyJson)
            out = jsonMapWithLegacyInt(settings);
        else
            out = readSendParamMap(settings);
        return true;
    }
    return false;
}

void CmdCatalogParamIni::writeToIni(CmdCatalogParamIniProfile profile, QSettings& settings, DeviceCmdParamKind kind,
                                    const QVariant& value) {
    if (profile == CmdCatalogParamIniProfile::None)
        return;
    removeSendParamKeys(settings);
    const QString prefix = sendParamIniPrefix();
    switch (kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        writeSendParamLeaf(settings, QStringLiteral("int"), value.toInt());
        break;
    case DeviceCmdParamKind::UInt:
        if (profile == CmdCatalogParamIniProfile::WithUIntAndLegacyJson)
            writeSendParamLeaf(settings, QStringLiteral("uint"), value.toUInt());
        break;
    case DeviceCmdParamKind::String:
        writeSendParamLeaf(settings, QStringLiteral("string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        writeJsonMap(settings, prefix, value);
        break;
    }
}

void CmdManifestRegistry::finalize() {
    cmdIndex_.clear();
    nameIndex_.clear();
    for (int i = 0; i < rows.size(); ++i) {
        const Row& row = rows.at(i);
        cmdIndex_.insert(row.cmd, i);
        if (row.enumName && row.enumName[0] != '\0')
            nameIndex_.insert(QString::fromLatin1(row.enumName), i);
    }
}

const CmdManifestRegistry::Row* CmdManifestRegistry::findByCmd(int cmd) const {
    const auto it = cmdIndex_.constFind(cmd);
    if (it == cmdIndex_.cend())
        return nullptr;
    return &rows.at(it.value());
}

const CmdManifestRegistry::Row* CmdManifestRegistry::findByEnumName(const QString& name) const {
    const auto it = nameIndex_.constFind(name);
    if (it == nameIndex_.cend())
        return nullptr;
    return &rows.at(it.value());
}

CmdManifestCatalog::CmdManifestCatalog(CmdManifestRegistry registry) : registry_(std::move(registry)) {
    registry_.finalize();
}

QStringList CmdManifestCatalog::allCmdNames(TestCaseSendAction action) const {
    QStringList names;
    for (const CmdManifestRegistry::Row& row : registry_.rows) {
        if (registry_.policy.filterBySendAction
            && !TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction CmdManifestCatalog::actionFor(int cmd) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return registry_.policy.missingCmdDefaultAction;
}

bool CmdManifestCatalog::isCmdForAction(int cmd, TestCaseSendAction action) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString CmdManifestCatalog::cmdUiLabel(const QString& enumName) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0') {
            const QString label = QString::fromUtf8(row->uiLabel);
            if (registry_.policy.uiLabelMode == CmdCatalogUiLabelMode::Picker)
                return CmdCatalogUi::pickerDisplayLabel(label);
            return label;
        }
    }
    const QString unknown = registry_.policy.unknownCmdLabel;
    if (!unknown.isEmpty())
        return unknown;
    return enumName;
}

bool CmdManifestCatalog::cmdFromName(const QString& name, int& out) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString CmdManifestCatalog::cmdToName(int cmd) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    if (registry_.policy.cmdToNameFallback == CmdCatalogCmdToNameFallback::Numeric)
        return QString::number(cmd);
    return QString();
}

bool CmdManifestCatalog::paramSchemaFor(int cmd, DeviceCmdParamSchema& out) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString CmdManifestCatalog::paramUiHint(const QString& enumName) const {
    if (const CmdManifestRegistry::Row* row = registry_.findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    if (registry_.policy.paramHintStyle == CmdCatalogParamHintStyle::HintOnly)
        return registry_.policy.unregisteredParamHint;
    int cmd = 0;
    if (!cmdFromName(enumName, cmd))
        return registry_.policy.unknownNameParamHint;
    return registry_.policy.unregisteredParamHint;
}

bool CmdManifestCatalog::paramFromIniGroup(const QSettings& settings, int cmd, QVariant& out) const {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    return CmdCatalogParamIni::readFromIni(registry_.policy.paramIniProfile, settings, schema.kind, out);
}

void CmdManifestCatalog::paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    CmdCatalogParamIni::writeToIni(registry_.policy.paramIniProfile, settings, schema.kind, value);
}
