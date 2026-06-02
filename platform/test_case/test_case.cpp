#include "test_case.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <QSettings>
#include <QTextCodec>

#include "Abini.h"
#include "common_utils.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

// ===================== TestCasePaths =====================

namespace TestCasePaths {

QString rootDir() {
    return QCoreApplication::applicationDirPath() + QStringLiteral("/test_case");
}

QString flowIniFileName() {
    return QStringLiteral("总的测试流程.ini");
}

QString flowIniPath() {
    return rootDir() + QLatin1Char('/') + flowIniFileName();
}

QString caseIniPath(const QString& caseName) {
    return rootDir() + QLatin1Char('/') + caseName.trimmed() + QStringLiteral(".ini");
}

bool ensureRootDir() {
    QDir dir(rootDir());
    if (dir.exists())
        return true;
    return dir.mkpath(QStringLiteral("."));
}

bool isReservedCaseName(const QString& name) {
    const QString n = name.trimmed();
    if (n.isEmpty())
        return true;
    if (n.compare(flowIniFileName(), Qt::CaseInsensitive) == 0)
        return true;
    if (n.compare(QStringLiteral("总的测试流程"), Qt::CaseInsensitive) == 0)
        return true;
    return false;
}

bool isValidCaseFileName(const QString& name, QString* errorOut) {
    const QString n = name.trimmed();
    if (n.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("名称不能为空");
        return false;
    }
    if (isReservedCaseName(n)) {
        if (errorOut)
            *errorOut = QStringLiteral("名称与保留名冲突");
        return false;
    }
    static const QString forbidden = QStringLiteral("\\/:*?\"<>|");
    for (const QChar c : n) {
        if (forbidden.contains(c)) {
            if (errorOut)
                *errorOut = QStringLiteral("名称不能包含 \\ / : * ? \" < > |");
            return false;
        }
        if (c.category() == QChar::Other_Control) {
            if (errorOut)
                *errorOut = QStringLiteral("名称含有非法控制字符");
            return false;
        }
    }
    return true;
}

}  // namespace TestCasePaths

// ===================== TestCaseStore（内部） =====================

namespace {

QString stationGroup(const QString& stationKey) {
    return QStringLiteral("Station/") + stationKey.trimmed();
}

QString flowStationsCatalogGroup() {
    return QStringLiteral("FlowStations");
}

QVector<TestFlowStationEntry> builtinFlowStationPresets() {
    return {
        {QStringLiteral("default"), QStringLiteral("默认工站")},
        {QStringLiteral("FREE_WORK"), QStringLiteral("自由工站")},
        {QStringLiteral("ASSEMBLY_CURRENT_TEST"), QStringLiteral("组装电流测试工站")},
        {QStringLiteral("AGING_TEST"), QStringLiteral("老化测试工站")},
        {QStringLiteral("BUTTON_TEST"), QStringLiteral("按键测试工站")},
        {QStringLiteral("SCREEN_TEST"), QStringLiteral("屏幕测试工站")},
        {QStringLiteral("BLUETOOTH_TEST"), QStringLiteral("蓝牙测试工站")},
        {QStringLiteral("SUCTION_TEST"), QStringLiteral("吸力测试工站")},
    };
}

QString lookupPresetDisplayName(const QString& key) {
    for (const TestFlowStationEntry& entry : builtinFlowStationPresets()) {
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0)
            return entry.displayName;
    }
    return QString();
}

QString lookupPresetKeyFromDisplayName(const QString& displayName) {
    const QString n = displayName.trimmed();
    for (const TestFlowStationEntry& entry : builtinFlowStationPresets()) {
        if (entry.displayName == n)
            return entry.key;
    }
    return QString();
}

/** 与 SETTINGS（上位机设置.ini）一致：IniFormat + UTF-8，避免 Windows 下中文写成 \\x 转义。 */
void applyTestCaseIniCodec(QSettings& ini) {
    ini.setIniCodec(QTextCodec::codecForName("UTF-8"));
}

void ensureUtf8BomOnIniFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite))
        return;
    QByteArray data = file.readAll();
    static const QByteArray kUtf8Bom = QByteArray::fromHex("EFBBBF");
    if (data.startsWith(kUtf8Bom))
        return;
    file.seek(0);
    file.resize(0);
    file.write(kUtf8Bom);
    file.write(data);
}

void syncTestCaseIni(QSettings& ini, const QString& filePath) {
    ini.sync();
    if (ini.status() == QSettings::NoError)
        ensureUtf8BomOnIniFile(filePath);
}

}  // namespace

QVector<TestFlowStationEntry> TestCaseStore::defaultFlowStationPresets() {
    return builtinFlowStationPresets();
}

QVector<TestFlowStationEntry> TestCaseStore::loadFlowStationCatalog() {
    TestCasePaths::ensureRootDir();
    QHash<QString, QString> nameByKey;
    for (const TestFlowStationEntry& preset : builtinFlowStationPresets())
        nameByKey.insert(preset.key, preset.displayName);

    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(flowStationsCatalogGroup());
    const QStringList catalogKeys = ini.childKeys();
    for (const QString& key : catalogKeys) {
        const QString k = key.trimmed();
        if (k.isEmpty())
            continue;
        const QString name = ini.value(key).toString().trimmed();
        nameByKey.insert(k, name.isEmpty() ? lookupPresetDisplayName(k) : name);
    }
    ini.endGroup();

    for (const QString& flowKey : TestCaseStore::listStationKeysFromFlow()) {
        const QString k = flowKey.trimmed();
        if (k.isEmpty() || nameByKey.contains(k))
            continue;
        const QString presetName = lookupPresetDisplayName(k);
        nameByKey.insert(k, presetName.isEmpty() ? k : presetName);
    }

    if (catalogKeys.isEmpty()) {
        QVector<TestFlowStationEntry> seed;
        seed.reserve(nameByKey.size());
        for (auto it = nameByKey.constBegin(); it != nameByKey.constEnd(); ++it)
            seed.append({it.key(), it.value()});
        saveFlowStationCatalog(seed);
    }

    QVector<TestFlowStationEntry> result;
    result.reserve(nameByKey.size());
    for (auto it = nameByKey.constBegin(); it != nameByKey.constEnd(); ++it)
        result.append({it.key(), it.value()});
    std::sort(result.begin(), result.end(), [](const TestFlowStationEntry& a, const TestFlowStationEntry& b) {
        if (a.key == QStringLiteral("default"))
            return true;
        if (b.key == QStringLiteral("default"))
            return false;
        if (a.key == QStringLiteral("FREE_WORK"))
            return true;
        if (b.key == QStringLiteral("FREE_WORK"))
            return false;
        return a.displayName.localeAwareCompare(b.displayName) < 0;
    });
    return result;
}

bool TestCaseStore::saveFlowStationCatalog(const QVector<TestFlowStationEntry>& entries) {
    TestCasePaths::ensureRootDir();
    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(flowStationsCatalogGroup());
    const QStringList oldKeys = ini.childKeys();
    for (const QString& oldKey : oldKeys)
        ini.remove(oldKey);
    for (const TestFlowStationEntry& entry : entries) {
        const QString k = entry.key.trimmed();
        if (k.isEmpty())
            continue;
        const QString name = entry.displayName.trimmed();
        ini.setValue(k, name.isEmpty() ? lookupPresetDisplayName(k) : name);
    }
    ini.endGroup();
    syncTestCaseIni(ini, flowPath);
    return true;
}

QString TestCaseStore::flowStationDisplayName(const QString& stationKey) {
    const QString k = stationKey.trimmed();
    if (k.isEmpty())
        return QString();
    for (const TestFlowStationEntry& entry : loadFlowStationCatalog()) {
        if (entry.key.compare(k, Qt::CaseInsensitive) == 0)
            return entry.displayName;
    }
    const QString preset = lookupPresetDisplayName(k);
    return preset.isEmpty() ? k : preset;
}

QString TestCaseStore::resolveFlowStationKey(const QString& displayNameOrKey) {
    const QString t = displayNameOrKey.trimmed();
    if (t.isEmpty())
        return QString();
    for (const TestFlowStationEntry& entry : builtinFlowStationPresets()) {
        if (entry.key.compare(t, Qt::CaseInsensitive) == 0)
            return entry.key;
        if (entry.displayName == t)
            return entry.key;
    }
    const QVector<TestFlowStationEntry> catalog = loadFlowStationCatalog();
    for (const TestFlowStationEntry& entry : catalog) {
        if (entry.key.compare(t, Qt::CaseInsensitive) == 0)
            return entry.key;
        if (entry.displayName == t)
            return entry.key;
    }
    return t;
}

bool TestCaseStore::addFlowStation(const QString& displayName, QString* errorOut) {
    const QString name = displayName.trimmed();
    if (name.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("工站名称不能为空");
        return false;
    }
    if (!TestCasePaths::isValidCaseFileName(name, errorOut))
        return false;

    const QString presetKey = lookupPresetKeyFromDisplayName(name);
    const QString key = presetKey.isEmpty() ? name : presetKey;
    QVector<TestFlowStationEntry> catalog = loadFlowStationCatalog();
    for (const TestFlowStationEntry& entry : catalog) {
        if (entry.displayName == name) {
            if (errorOut)
                *errorOut = QStringLiteral("工站名称已存在：%1").arg(name);
            return false;
        }
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0) {
            if (errorOut)
                *errorOut = QStringLiteral("工站已存在：%1").arg(name);
            return false;
        }
    }
    catalog.append({key, name});
    return saveFlowStationCatalog(catalog);
}

bool TestCaseStore::removeFlowStation(const QString& key, QString* errorOut) {
    const QString k = key.trimmed();
    if (k.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("工站键无效");
        return false;
    }
    QVector<TestFlowStationEntry> catalog = loadFlowStationCatalog();
    bool found = false;
    for (int i = catalog.size() - 1; i >= 0; --i) {
        if (catalog[i].key.compare(k, Qt::CaseInsensitive) == 0) {
            catalog.removeAt(i);
            found = true;
            break;
        }
    }
    if (!found) {
        if (errorOut)
            *errorOut = QStringLiteral("工站不在目录中：%1").arg(k);
        return false;
    }
    if (!saveFlowStationCatalog(catalog))
        return false;

    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(k));
    ini.remove(QStringLiteral("Items"));
    ini.remove(QStringLiteral("StopFlowOnTestFail"));
    ini.remove(QStringLiteral("StopOnGateFail"));
    ini.endGroup();
    syncTestCaseIni(ini, flowPath);
    return true;
}

TestCaseGateOp gateOpFromString(const QString& s) {
    if (s == QLatin1String("gt"))
        return TestCaseGateOp::Gt;
    if (s == QLatin1String("lt"))
        return TestCaseGateOp::Lt;
    if (s == QLatin1String("eq"))
        return TestCaseGateOp::Eq;
    if (s == QLatin1String("compareVersions"))
        return TestCaseGateOp::CompareVersions;
    return TestCaseGateOp::Range;
}

QString gateOpToString(TestCaseGateOp op) {
    switch (op) {
    case TestCaseGateOp::Gt:
        return QStringLiteral("gt");
    case TestCaseGateOp::Lt:
        return QStringLiteral("lt");
    case TestCaseGateOp::Eq:
        return QStringLiteral("eq");
    case TestCaseGateOp::CompareVersions:
        return QStringLiteral("compareVersions");
    default:
        return QStringLiteral("range");
    }
}

bool TestCaseStore::loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut) {
    Q_UNUSED(errorOut);
    TestCasePaths::ensureRootDir();
    const QString casePath = TestCasePaths::caseIniPath(caseName);
    QSettings ini(casePath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);

    const QString nameInIni = ini.value(QStringLiteral("Meta/Name"), caseName).toString().trimmed();
    const QString displayInIni = ini.value(QStringLiteral("Meta/DisplayName")).toString().trimmed();
    if (!displayInIni.isEmpty())
        out.meta.name = displayInIni;
    else if (!nameInIni.isEmpty())
        out.meta.name = nameInIni;
    else
        out.meta.name = caseName.trimmed();
    out.meta.displayName = out.meta.name;
    out.meta.mesTag = ini.value(QStringLiteral("Meta/MesTag")).toString().trimmed();
    out.meta.promptEnabled = ini.value(QStringLiteral("Meta/PromptEnabled"), false).toBool();
    out.meta.promptText = ini.value(QStringLiteral("Meta/PromptText")).toString();

    const QString action = ini.value(QStringLiteral("Send/Action"), QStringLiteral("Set")).toString();
    out.send.action = action.compare(QLatin1String("Get"), Qt::CaseInsensitive) == 0 ? TestCaseSendAction::Get
                                                                                       : TestCaseSendAction::Set;
    out.send.deviceCmd = ini.value(QStringLiteral("Send/DeviceCmd")).toString().trimmed();
    const QString channelIni = ini.value(QStringLiteral("Send/Channel")).toString().trimmed();
    if (channelIni.compare(QStringLiteral("Dongle"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Dongle;
    } else if (channelIni.compare(QStringLiteral("Product"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Product;
    } else {
        DongleCmd inferDongle;
        out.send.channel = DongleCmdCatalog::dongleCmdFromName(out.send.deviceCmd, inferDongle)
                               ? TestCaseSendChannel::Dongle
                               : TestCaseSendChannel::Product;
    }
    if (out.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(out.send.deviceCmd, dongleCmd)) {
            if (!DongleCmdCatalog::isCmdForAction(dongleCmd, out.send.action))
                out.send.action = DongleCmdCatalog::actionFor(dongleCmd);
            DongleCmdCatalog::paramFromIniGroup(ini, dongleCmd, out.send.param);
        }
    } else {
        DeviceCmd cmd;
        if (DeviceCmdCatalog::deviceCmdFromName(out.send.deviceCmd, cmd))
            DeviceCmdCatalog::paramFromIniGroup(ini, cmd, out.send.param);
    }

    out.timing.delayBeforeMs = ini.value(QStringLiteral("Timing/DelayBeforeMs"), 0).toInt();
    out.timing.delayAfterMs = ini.value(QStringLiteral("Timing/DelayAfterMs"), 0).toInt();
    out.timing.commandTimeoutMs = ini.value(QStringLiteral("Timing/CommandTimeoutMs"), 0).toInt();

    out.gate.enabled = ini.value(QStringLiteral("Gate/Enabled"), false).toBool();
    out.gate.reportType = ini.value(QStringLiteral("Gate/ReportType")).toString().trimmed();
    out.gate.field = ini.value(QStringLiteral("Gate/Field")).toString().trimmed();
    out.gate.op = gateOpFromString(ini.value(QStringLiteral("Gate/Op"), QStringLiteral("range")).toString());
    out.gate.low = ini.value(QStringLiteral("Gate/Low"), 0).toDouble();
    out.gate.high = ini.value(QStringLiteral("Gate/High"), 0).toDouble();
    out.gate.expected = ini.value(QStringLiteral("Gate/Expected")).toString();
    out.gate.lowSettingsKey = ini.value(QStringLiteral("Gate/LowSettingsKey")).toString();
    out.gate.highSettingsKey = ini.value(QStringLiteral("Gate/HighSettingsKey")).toString();

    out.hook.enabled = ini.value(QStringLiteral("Hook/Enabled"), false).toBool();
    out.hook.hookId = ini.value(QStringLiteral("Hook/HookId")).toString().trimmed();

    // 旧 AT 钩子迁移为 Dongle ini（BT_SCAN_MAC / BT_DIRECT_DCON）
    if (out.hook.enabled) {
        if (out.hook.hookId == QStringLiteral("BT_SCAN_MAC")) {
            out.hook.enabled = false;
            out.send.channel = TestCaseSendChannel::Dongle;
            out.send.action = TestCaseSendAction::Set;
            out.send.deviceCmd = QStringLiteral("BleScanConnect");
            if (!out.send.param.isValid() || out.send.param.toString().trimmed().isEmpty())
                out.send.param = QStringLiteral("$MAC");
        } else if (out.hook.hookId == QStringLiteral("BT_DIRECT_DCON")) {
            out.hook.enabled = false;
            out.send.channel = TestCaseSendChannel::Dongle;
            out.send.action = TestCaseSendAction::Set;
            out.send.deviceCmd = QStringLiteral("BleDirectConnect");
            if (!out.send.param.isValid() || out.send.param.toString().trimmed().isEmpty())
                out.send.param = QStringLiteral("$MAC");
        }
    }

    return true;
}

bool TestCaseStore::saveCase(const TestCaseDefinition& def, QString* errorOut) {
    Q_UNUSED(errorOut);
    TestCasePaths::ensureRootDir();
    const QString casePath = TestCasePaths::caseIniPath(def.meta.name);
    QSettings ini(casePath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.clear();

    ini.setValue(QStringLiteral("Meta/Name"), def.meta.name);
    ini.setValue(QStringLiteral("Meta/DisplayName"), def.meta.name);
    ini.setValue(QStringLiteral("Meta/MesTag"), def.meta.mesTag);
    ini.setValue(QStringLiteral("Meta/PromptEnabled"), def.meta.promptEnabled);
    ini.setValue(QStringLiteral("Meta/PromptText"), def.meta.promptText);

    ini.setValue(QStringLiteral("Send/Action"), def.send.action == TestCaseSendAction::Get ? QStringLiteral("Get")
                                                                                          : QStringLiteral("Set"));
    ini.setValue(QStringLiteral("Send/Channel"),
                 def.send.channel == TestCaseSendChannel::Dongle ? QStringLiteral("Dongle") : QStringLiteral("Product"));
    ini.setValue(QStringLiteral("Send/DeviceCmd"), def.send.deviceCmd);
    if (def.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd))
            DongleCmdCatalog::paramToIniGroup(ini, dongleCmd, def.send.param);
    } else {
        DeviceCmd cmd;
        if (DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd))
            DeviceCmdCatalog::paramToIniGroup(ini, cmd, def.send.param);
    }

    ini.setValue(QStringLiteral("Timing/DelayBeforeMs"), def.timing.delayBeforeMs);
    ini.setValue(QStringLiteral("Timing/DelayAfterMs"), def.timing.delayAfterMs);
    ini.setValue(QStringLiteral("Timing/CommandTimeoutMs"), def.timing.commandTimeoutMs);

    ini.setValue(QStringLiteral("Gate/Enabled"), def.gate.enabled);
    ini.setValue(QStringLiteral("Gate/ReportType"), def.gate.reportType);
    ini.setValue(QStringLiteral("Gate/Field"), def.gate.field);
    ini.setValue(QStringLiteral("Gate/Op"), gateOpToString(def.gate.op));
    ini.setValue(QStringLiteral("Gate/Low"), def.gate.low);
    ini.setValue(QStringLiteral("Gate/High"), def.gate.high);
    ini.setValue(QStringLiteral("Gate/Expected"), def.gate.expected);
    ini.setValue(QStringLiteral("Gate/LowSettingsKey"), def.gate.lowSettingsKey);
    ini.setValue(QStringLiteral("Gate/HighSettingsKey"), def.gate.highSettingsKey);

    ini.setValue(QStringLiteral("Hook/Enabled"), def.hook.enabled);
    ini.setValue(QStringLiteral("Hook/HookId"), def.hook.hookId);
    syncTestCaseIni(ini, casePath);
    return true;
}

QStringList TestCaseStore::listCaseIniNames() {
    TestCasePaths::ensureRootDir();
    QDir dir(TestCasePaths::rootDir());
    const QString flowName = TestCasePaths::flowIniFileName();
    QStringList names;
    for (const QFileInfo& fi : dir.entryInfoList({QStringLiteral("*.ini")}, QDir::Files)) {
        if (fi.fileName().compare(flowName, Qt::CaseInsensitive) == 0)
            continue;
        names.append(fi.completeBaseName());
    }
    names.sort();
    return names;
}

bool TestCaseStore::loadFlowMeta(TestFlowMeta& out) {
    TestCasePaths::ensureRootDir();
    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    out.version = ini.value(QStringLiteral("Meta/Version"), 1).toInt();
    out.selectedStation = ini.value(QStringLiteral("Meta/SelectedStation")).toString().trimmed();
    out.selectedStationName = ini.value(QStringLiteral("Meta/SelectedStationName")).toString().trimmed();
    return true;
}

bool TestCaseStore::saveFlowMeta(const TestFlowMeta& meta) {
    TestCasePaths::ensureRootDir();
    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.setValue(QStringLiteral("Meta/Version"), meta.version);
    ini.setValue(QStringLiteral("Meta/SelectedStation"), meta.selectedStation);
    ini.setValue(QStringLiteral("Meta/SelectedStationName"), meta.selectedStationName);
    syncTestCaseIni(ini, flowPath);
    return true;
}

QStringList TestCaseStore::loadStationItems(const QString& stationKey) {
    QStringList items;
    for (const TestFlowItemEntry& entry : loadStationFlowItems(stationKey))
        items.append(entry.caseName);
    return items;
}

bool TestCaseStore::saveStationItems(const QString& stationKey, const QStringList& items) {
    QVector<TestFlowItemEntry> entries;
    entries.reserve(items.size());
    for (const QString& name : items) {
        TestFlowItemEntry entry;
        entry.caseName = name;
        entries.append(entry);
    }
    return saveStationFlowItems(stationKey, entries);
}

bool TestCaseStore::loadStationStopFlowOnTestFail(const QString& stationKey, bool defaultValue) {
    TestCasePaths::ensureRootDir();
    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(stationKey));
    bool result = defaultValue;
    if (ini.contains(QStringLiteral("StopFlowOnTestFail")))
        result = ini.value(QStringLiteral("StopFlowOnTestFail"), defaultValue).toBool();
    else if (ini.contains(QStringLiteral("StopOnGateFail")))
        result = true;  // 兼容旧 per-case 列表
    ini.endGroup();
    return result;
}

QVector<TestFlowItemEntry> TestCaseStore::loadStationFlowItems(const QString& stationKey) {
    TestCasePaths::ensureRootDir();
    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(stationKey));
    const QString rawItems = ini.value(QStringLiteral("Items")).toString();
    ini.endGroup();

    QVector<TestFlowItemEntry> entries;
    for (const QString& part : rawItems.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (name.isEmpty())
            continue;
        TestFlowItemEntry entry;
        entry.caseName = name;
        entries.append(entry);
    }
    return entries;
}

bool TestCaseStore::saveStationFlowItems(const QString& stationKey, const QVector<TestFlowItemEntry>& items,
                                         bool stopFlowOnTestFail) {
    TestCasePaths::ensureRootDir();
    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(stationKey));
    QStringList names;
    for (const TestFlowItemEntry& entry : items) {
        const QString name = entry.caseName.trimmed();
        if (name.isEmpty())
            continue;
        names.append(name);
    }
    ini.setValue(QStringLiteral("Items"), names.join(QLatin1Char(',')));
    ini.setValue(QStringLiteral("StopFlowOnTestFail"), stopFlowOnTestFail);
    ini.remove(QStringLiteral("StopOnGateFail"));
    ini.endGroup();
    syncTestCaseIni(ini, flowPath);
    return true;
}

QStringList TestCaseStore::listStationKeysFromFlow() {
    TestCasePaths::ensureRootDir();
    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    QStringList keys;
    const QStringList groups = ini.childGroups();
    const QString prefix = QStringLiteral("Station/");
    for (const QString& g : groups) {
        if (g.startsWith(prefix))
            keys.append(g.mid(prefix.size()));
    }
    return keys;
}

// ===================== TestCaseValidator =====================

bool TestCaseValidator::validateCase(const TestCaseDefinition& def, QStringList& errors) {
    errors.clear();
    QString nameErr;
    if (!TestCasePaths::isValidCaseFileName(def.meta.name, &nameErr))
        errors.append(nameErr);

    if (def.send.deviceCmd.isEmpty()) {
        errors.append(QStringLiteral("请选择测试指令"));
    } else if (def.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (!DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd)) {
            errors.append(QStringLiteral("Dongle 测试指令无效"));
        } else if (!DongleCmdCatalog::isCmdForAction(dongleCmd, def.send.action)) {
            errors.append(QStringLiteral("Dongle 指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!DongleCmdCatalog::paramSchemaFor(dongleCmd, schema))
                errors.append(QStringLiteral("该 Dongle 指令尚未配置参数模板，请联系工程师"));
        }
    } else {
        DeviceCmd cmd;
        if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
            errors.append(QStringLiteral("产品测试指令无效"));
        } else {
            DeviceCmdParamSchema schema;
            if (!DeviceCmdCatalog::paramSchemaFor(cmd, schema))
                errors.append(QStringLiteral("该指令尚未配置参数模板，请联系工程师"));
        }
    }

    if (def.timing.delayBeforeMs < 0 || def.timing.delayAfterMs < 0)
        errors.append(QStringLiteral("延时不能为负数"));
    if (def.timing.commandTimeoutMs < 0)
        errors.append(QStringLiteral("指令超时不能为负数"));
    if (def.timing.commandTimeoutMs > 0 && def.timing.commandTimeoutMs < 100)
        errors.append(QStringLiteral("指令超时须为 0（自动）或不少于 100 毫秒"));

    if (def.meta.promptEnabled && def.meta.promptText.trimmed().isEmpty())
        errors.append(QStringLiteral("已勾选操作提示时须填写提示文字"));

    if (def.gate.enabled) {
        GateTypeDescriptor desc;
        if (!GateRegistry::descriptorFor(def.gate.reportType, desc))
            errors.append(QStringLiteral("回传数据类型未登记，请联系工程师"));
        else if (!GateRegistry::fieldsFor(def.gate.reportType).contains(def.gate.field))
            errors.append(QStringLiteral("判定项目未登记，请联系工程师"));
    }

    if (def.hook.enabled && !TestCaseHookRegistry::contains(def.hook.hookId))
        errors.append(QStringLiteral("预置流程类型无效，请联系工程师"));

    const QString path = TestCasePaths::caseIniPath(def.meta.name);
    if (QFile::exists(path)) {
        TestCaseDefinition existing;
        TestCaseStore::loadCase(def.meta.name, existing);
        Q_UNUSED(existing);
    }

    return errors.isEmpty();
}

// ===================== DeviceCmdCatalog =====================

namespace {

struct CmdEntry {
    DeviceCmd cmd;
    DeviceCmdParamKind kind;
    const char* hint;
};

const CmdEntry kCatalog[] = {
    {DeviceCmd::ForbidSleep, DeviceCmdParamKind::Int, "FacSwitch_OPEN=1"},
    {DeviceCmd::Sn, DeviceCmdParamKind::Int, "FacDevInfoType"},
    {DeviceCmd::BaseInfo, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::GetBattery, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::FacResult, DeviceCmdParamKind::Int, "1"},
    {DeviceCmd::BurningMode, DeviceCmdParamKind::JsonMap, "mode,seconds,switch"},
    {DeviceCmd::Sleep, DeviceCmdParamKind::Int, "FacSwitch_START"},
    {DeviceCmd::ShipMode, DeviceCmdParamKind::Int, "1"},
    {DeviceCmd::FacMode, DeviceCmdParamKind::Int, "1"},
    {DeviceCmd::DevReset, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::WifiDisconnect, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::WifiConnect, DeviceCmdParamKind::JsonMap, "name,password"},
    {DeviceCmd::RssiRead, DeviceCmdParamKind::JsonMap, "mode"},
    {DeviceCmd::ChargeCurrentRead, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::TupleRead, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::PeriphState, DeviceCmdParamKind::None, nullptr},
    {DeviceCmd::FactoryReset, DeviceCmdParamKind::None, nullptr},
};

#define X(name) \
    { QStringLiteral(#name), DeviceCmd::name },
const QHash<QString, DeviceCmd> kNameMap = {
    X(ForbidSleep) X(Sn) X(BaseInfo) X(GetBattery) X(FacResult) X(BurningMode) X(Sleep) X(ShipMode) X(FacMode)
        X(DevReset) X(WifiDisconnect) X(WifiConnect) X(RssiRead) X(ChargeCurrentRead) X(TupleRead) X(PeriphState)
        X(FactoryReset) X(PressSensorTemp) X(UartReceive) X(RgbColor) X(MotorCali) X(MotorDampingState)
        X(MotorTestState) X(MotorCaliState) X(ScreenColor) X(LedColor) X(MotorAdcSwitch) X(MotorParam) X(MotorState)
        X(MotorCaliResultParam) X(Music) X(BrushRecord) X(BrushTime) X(CameraState) X(ScreenCameraState)
        X(CameraLightState) X(CameraSupportState) X(CameraExposureTime) X(BrushReset) X(PressCaliResult)
        X(ImuCaliResult) X(NewImuCaliResult) X(DeviceMode) X(BrushControl) X(CameraPictureState) X(LocalOta)
        X(StartOtaApp) X(IAmApp) X(ConfigNetworkApp) X(StartMultiBleOtaApp) X(PressCollect) X(ImuCollect)
        X(CameraFaultDataPacket) X(ServoMotorInfo) X(MicControl) X(UploadRecordData) X(NewWifiConnect)
        X(SevorMotorParam) X(SuctionMode) X(BtSignalMode) X(BtNoSignalMode) X(BtFreqMode) X(WriteKey) X(TrimSet)
        X(MacWrite) X(NightLightSet) X(LedTest) X(LcdBacklight) X(LightReportControl) X(LightCalibWrite)
        X(CompensationSet) X(NowMusicInfo) X(SdCardInfo) X(LightSensorInfo) X(ButtonState) X(GetPressCaliResult)
        X(GetImuCaliResult) X(DeviceInfo) X(ConnectInfo) X(WifiInfo) X(GetServoMotorInfo) X(BurshBacklog)
        X(TrimRead) X(MacRead) X(KeySignalRead) X(LightCalibRead) X(AgingStatusRead) X(FactoryDoneRead)
        X(DeviceExceptionRead)};
#undef X

const QHash<QString, QString>& cmdUiLabelMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("ForbidSleep"), QStringLiteral("禁止休眠")},
        {QStringLiteral("Sn"), QStringLiteral("读写序列号")},
        {QStringLiteral("BaseInfo"), QStringLiteral("读取基本信息")},
        {QStringLiteral("GetBattery"), QStringLiteral("读取电量")},
        {QStringLiteral("FacResult"), QStringLiteral("写入产测结果")},
        {QStringLiteral("BurningMode"), QStringLiteral("老化/烧机模式")},
        {QStringLiteral("Sleep"), QStringLiteral("休眠")},
        {QStringLiteral("ShipMode"), QStringLiteral("关机")},
        {QStringLiteral("FacMode"), QStringLiteral("进入工厂模式")},
        {QStringLiteral("DevReset"), QStringLiteral("设备复位")},
        {QStringLiteral("WifiDisconnect"), QStringLiteral("断开无线网络")},
        {QStringLiteral("WifiConnect"), QStringLiteral("连接无线网络")},
        {QStringLiteral("RssiRead"), QStringLiteral("读取信号强度")},
        {QStringLiteral("ChargeCurrentRead"), QStringLiteral("读取充电电流")},
        {QStringLiteral("TupleRead"), QStringLiteral("读取三元组")},
        {QStringLiteral("PeriphState"), QStringLiteral("读取外设状态")},
        {QStringLiteral("FactoryReset"), QStringLiteral("恢复出厂设置")},
        {QStringLiteral("PressSensorTemp"), QStringLiteral("压力传感器温度")},
        {QStringLiteral("UartReceive"), QStringLiteral("串口接收开关")},
        {QStringLiteral("RgbColor"), QStringLiteral("设置RGB颜色")},
        {QStringLiteral("MotorCali"), QStringLiteral("电机校准")},
        {QStringLiteral("MotorDampingState"), QStringLiteral("电机阻尼状态")},
        {QStringLiteral("MotorTestState"), QStringLiteral("电机测试状态")},
        {QStringLiteral("MotorCaliState"), QStringLiteral("电机校准状态")},
        {QStringLiteral("ScreenColor"), QStringLiteral("设置屏幕颜色")},
        {QStringLiteral("LedColor"), QStringLiteral("设置指示灯颜色")},
        {QStringLiteral("MotorAdcSwitch"), QStringLiteral("电机ADC开关")},
        {QStringLiteral("MotorParam"), QStringLiteral("设置电机参数")},
        {QStringLiteral("MotorState"), QStringLiteral("电机运行状态")},
        {QStringLiteral("MotorCaliResultParam"), QStringLiteral("电机校准结果参数")},
        {QStringLiteral("Music"), QStringLiteral("设置音乐")},
        {QStringLiteral("BrushRecord"), QStringLiteral("设置刷牙记录")},
        {QStringLiteral("BrushTime"), QStringLiteral("设置刷牙时间")},
        {QStringLiteral("CameraState"), QStringLiteral("设置摄像头状态")},
        {QStringLiteral("ScreenCameraState"), QStringLiteral("屏幕摄像头状态")},
        {QStringLiteral("CameraLightState"), QStringLiteral("摄像头补光状态")},
        {QStringLiteral("CameraSupportState"), QStringLiteral("摄像头支持状态")},
        {QStringLiteral("CameraExposureTime"), QStringLiteral("摄像头曝光时间")},
        {QStringLiteral("BrushReset"), QStringLiteral("刷牙复位")},
        {QStringLiteral("PressCaliResult"), QStringLiteral("写入压力校准结果")},
        {QStringLiteral("ImuCaliResult"), QStringLiteral("写入惯性校准结果")},
        {QStringLiteral("NewImuCaliResult"), QStringLiteral("写入新惯性校准结果")},
        {QStringLiteral("DeviceMode"), QStringLiteral("设置设备模式")},
        {QStringLiteral("BrushControl"), QStringLiteral("刷牙控制")},
        {QStringLiteral("CameraPictureState"), QStringLiteral("摄像头拍照状态")},
        {QStringLiteral("LocalOta"), QStringLiteral("本地固件升级")},
        {QStringLiteral("StartOtaApp"), QStringLiteral("启动升级应用")},
        {QStringLiteral("IAmApp"), QStringLiteral("应用身份声明")},
        {QStringLiteral("ConfigNetworkApp"), QStringLiteral("配置网络应用")},
        {QStringLiteral("StartMultiBleOtaApp"), QStringLiteral("启动多设备蓝牙升级")},
        {QStringLiteral("PressCollect"), QStringLiteral("压力采集")},
        {QStringLiteral("ImuCollect"), QStringLiteral("惯性传感器采集")},
        {QStringLiteral("CameraFaultDataPacket"), QStringLiteral("摄像头故障数据")},
        {QStringLiteral("ServoMotorInfo"), QStringLiteral("舵机信息")},
        {QStringLiteral("MicControl"), QStringLiteral("麦克风控制")},
        {QStringLiteral("UploadRecordData"), QStringLiteral("上传记录数据")},
        {QStringLiteral("NewWifiConnect"), QStringLiteral("新方式连接无线网络")},
        {QStringLiteral("SevorMotorParam"), QStringLiteral("舵机参数")},
        {QStringLiteral("SuctionMode"), QStringLiteral("吸力模式")},
        {QStringLiteral("BtSignalMode"), QStringLiteral("蓝牙信号模式")},
        {QStringLiteral("BtNoSignalMode"), QStringLiteral("蓝牙无信号模式")},
        {QStringLiteral("BtFreqMode"), QStringLiteral("蓝牙定频模式")},
        {QStringLiteral("WriteKey"), QStringLiteral("写入密钥")},
        {QStringLiteral("TrimSet"), QStringLiteral("写入微调值")},
        {QStringLiteral("MacWrite"), QStringLiteral("写入网卡地址")},
        {QStringLiteral("NightLightSet"), QStringLiteral("夜灯设置")},
        {QStringLiteral("LedTest"), QStringLiteral("指示灯测试")},
        {QStringLiteral("LcdBacklight"), QStringLiteral("屏幕背光")},
        {QStringLiteral("LightReportControl"), QStringLiteral("灯光上报控制")},
        {QStringLiteral("LightCalibWrite"), QStringLiteral("写入灯光校准")},
        {QStringLiteral("CompensationSet"), QStringLiteral("补偿参数设置")},
        {QStringLiteral("NowMusicInfo"), QStringLiteral("当前音乐信息")},
        {QStringLiteral("SdCardInfo"), QStringLiteral("存储卡信息")},
        {QStringLiteral("LightSensorInfo"), QStringLiteral("环境光传感器信息")},
        {QStringLiteral("ButtonState"), QStringLiteral("按键状态")},
        {QStringLiteral("GetPressCaliResult"), QStringLiteral("读取压力校准结果")},
        {QStringLiteral("GetImuCaliResult"), QStringLiteral("读取惯性校准结果")},
        {QStringLiteral("DeviceInfo"), QStringLiteral("读取设备信息")},
        {QStringLiteral("ConnectInfo"), QStringLiteral("读取连接信息")},
        {QStringLiteral("WifiInfo"), QStringLiteral("读取无线网络信息")},
        {QStringLiteral("GetServoMotorInfo"), QStringLiteral("读取舵机信息")},
        {QStringLiteral("BurshBacklog"), QStringLiteral("刷牙积压数据")},
        {QStringLiteral("TrimRead"), QStringLiteral("读取微调值")},
        {QStringLiteral("MacRead"), QStringLiteral("读取网卡地址")},
        {QStringLiteral("KeySignalRead"), QStringLiteral("读取按键信号")},
        {QStringLiteral("LightCalibRead"), QStringLiteral("读取灯光校准")},
        {QStringLiteral("AgingStatusRead"), QStringLiteral("读取老化状态")},
        {QStringLiteral("FactoryDoneRead"), QStringLiteral("读取产测完成标志")},
        {QStringLiteral("DeviceExceptionRead"), QStringLiteral("读取设备异常")},
    };
    return map;
}

QVariant readJsonMap(const QSettings& s, const QString& prefix) {
    QVariantMap map;
    const QStringList keys = s.allKeys();
    const QString p = prefix + QLatin1Char('/');
    for (const QString& key : keys) {
        if (!key.startsWith(p))
            continue;
        const QString sub = key.mid(p.size());
        if (sub.contains(QLatin1Char('/')))
            continue;
        map.insert(sub, s.value(key));
    }
    if (map.isEmpty()) {
        const QString json = s.value(prefix + QStringLiteral("/json")).toString();
        if (!json.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
            if (doc.isObject())
                return doc.object().toVariantMap();
        }
        return {};
    }
    return map;
}

void removeKeysWithPrefix(QSettings& s, const QString& prefix) {
    const QString p = prefix + QLatin1Char('/');
    for (const QString& key : s.allKeys()) {
        if (key == prefix || key.startsWith(p))
            s.remove(key);
    }
}

void writeJsonMap(QSettings& s, const QString& prefix, const QVariant& value) {
    removeKeysWithPrefix(s, prefix);
    const QVariantMap map = value.toMap();
    for (auto it = map.cbegin(); it != map.cend(); ++it)
        s.setValue(prefix + QLatin1Char('/') + it.key(), it.value());
}

QString sendParamIniPrefix() {
    return QStringLiteral("Send/Param");
}

QVariant readSendScopedParam(const QSettings& settings, const QString& leafKey, const QVariant& defaultValue) {
    const QString sendKey = sendParamIniPrefix() + QLatin1Char('/') + leafKey;
    if (settings.contains(sendKey))
        return settings.value(sendKey);
    const QString legacyKey = QStringLiteral("Param/") + leafKey;
    if (settings.contains(legacyKey))
        return settings.value(legacyKey);
    return defaultValue;
}

QVariantMap readSendParamMap(const QSettings& settings) {
    QVariantMap map = readJsonMap(settings, sendParamIniPrefix()).toMap();
    if (!map.isEmpty())
        return map;
    return readJsonMap(settings, QStringLiteral("Param")).toMap();
}

}  // namespace

QStringList DeviceCmdCatalog::allDeviceCmdNames() {
    QStringList names = kNameMap.keys();
    names.sort();
    return names;
}

QString DeviceCmdCatalog::deviceCmdUiLabel(const QString& enumName) {
    const QString key = enumName.trimmed();
    const QString label = cmdUiLabelMap().value(key);
    if (!label.isEmpty())
        return label;
    return QStringLiteral("未登记指令");
}

bool DeviceCmdCatalog::deviceCmdFromName(const QString& name, DeviceCmd& out) {
    const auto it = kNameMap.constFind(name.trimmed());
    if (it == kNameMap.cend())
        return false;
    out = it.value();
    return true;
}

QString DeviceCmdCatalog::deviceCmdToName(DeviceCmd cmd) {
    for (auto it = kNameMap.cbegin(); it != kNameMap.cend(); ++it) {
        if (it.value() == cmd)
            return it.key();
    }
    return QString::number(static_cast<int>(cmd));
}

bool DeviceCmdCatalog::paramSchemaFor(DeviceCmd cmd, DeviceCmdParamSchema& out) {
    for (const auto& e : kCatalog) {
        if (e.cmd == cmd) {
            out.kind = e.kind;
            out.hint = e.hint ? QString::fromUtf8(e.hint) : QString();
            return true;
        }
    }
    // 协议枚举中已登记、但未单独维护模板的指令：默认无参数（多数 set/get 不传参即可）
    if (kNameMap.contains(deviceCmdToName(cmd))) {
        out.kind = DeviceCmdParamKind::None;
        out.hint.clear();
        return true;
    }
    return false;
}

bool DeviceCmdCatalog::paramFromIniGroup(const QSettings& settings, DeviceCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::Int:
        out = readSendScopedParam(settings, QStringLiteral("int"), 0).toInt();
        return true;
    case DeviceCmdParamKind::UInt:
        out = readSendScopedParam(settings, QStringLiteral("uint"), 0).toUInt();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = readSendParamMap(settings);
        return true;
    }
    return false;
}

void DeviceCmdCatalog::paramToIniGroup(QSettings& settings, DeviceCmd cmd, const QVariant& value) {
    removeKeysWithPrefix(settings, QStringLiteral("Param"));
    removeKeysWithPrefix(settings, sendParamIniPrefix());
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        settings.setValue(prefix + QStringLiteral("/int"), value.toInt());
        break;
    case DeviceCmdParamKind::UInt:
        settings.setValue(prefix + QStringLiteral("/uint"), value.toUInt());
        break;
    case DeviceCmdParamKind::String:
        settings.setValue(prefix + QStringLiteral("/string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        writeJsonMap(settings, prefix, value);
        break;
    }
}

QVariant DeviceCmdCatalog::paramFromSettings(const QSettings&, const QString&) {
    return {};
}

void DeviceCmdCatalog::paramToSettings(QSettings&, const QString&, const QVariant&) {}

// ===================== DongleCmdCatalog =====================

namespace {

struct DongleCmdEntry {
    DongleCmd cmd;
    TestCaseSendAction action;
    DeviceCmdParamKind kind;
    const char* hint;
};

const DongleCmdEntry kDongleCatalog[] = {
    {DongleCmd::BleScanConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String, "MAC"},
    {DongleCmd::BleDirectConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String, "MAC"},
    {DongleCmd::BleOtaConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String, "MAC"},
    {DongleCmd::BleAppConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String, "MAC"},
    {DongleCmd::BleMainConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String, "MAC"},
    {DongleCmd::OtaDataPassthrough, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0/1"},
    {DongleCmd::MainDataPassthrough, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0/1"},
    {DongleCmd::BleLog, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0/1"},
    {DongleCmd::BleDeviceLog, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0/1"},
    {DongleCmd::Bomb, TestCaseSendAction::Set, DeviceCmdParamKind::JsonMap, "deviceName,rssi,connectionInterval,command"},
    {DongleCmd::GetGmac, TestCaseSendAction::Get, DeviceCmdParamKind::None, nullptr},
};

#define Y(name) \
    { QStringLiteral(#name), DongleCmd::name },
const QHash<QString, DongleCmd> kDongleNameMap = {
    Y(BleScanConnect) Y(BleDirectConnect) Y(BleOtaConnect) Y(BleAppConnect) Y(BleMainConnect)
        Y(OtaDataPassthrough) Y(MainDataPassthrough) Y(BleLog) Y(BleDeviceLog) Y(Bomb) Y(GetGmac)};
#undef Y

const QHash<QString, DongleCmd> kDongleLegacyNameMap = {
    {QStringLiteral("DongleBleScanConnect"), DongleCmd::BleScanConnect},
    {QStringLiteral("DongleBleDirectConnect"), DongleCmd::BleDirectConnect},
    {QStringLiteral("DongleBleOtaConnect"), DongleCmd::BleOtaConnect},
    {QStringLiteral("DongleBleAppConnect"), DongleCmd::BleAppConnect},
    {QStringLiteral("DongleBleMainConnect"), DongleCmd::BleMainConnect},
    {QStringLiteral("DongleOtaDataPassthrough"), DongleCmd::OtaDataPassthrough},
    {QStringLiteral("DongleMainDataPassthrough"), DongleCmd::MainDataPassthrough},
    {QStringLiteral("DongleBleLog"), DongleCmd::BleLog},
    {QStringLiteral("DongleBleDeviceLog"), DongleCmd::BleDeviceLog},
    {QStringLiteral("DongleBomb"), DongleCmd::Bomb},
    {QStringLiteral("DongleGetGmac"), DongleCmd::GetGmac},
};

const QHash<QString, QString>& dongleCmdUiLabelMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("BleScanConnect"), QStringLiteral("Dongle 扫描连接蓝牙")},
        {QStringLiteral("BleDirectConnect"), QStringLiteral("Dongle 直连蓝牙")},
        {QStringLiteral("BleOtaConnect"), QStringLiteral("Dongle OTA 蓝牙连接")},
        {QStringLiteral("BleAppConnect"), QStringLiteral("Dongle App 蓝牙连接")},
        {QStringLiteral("BleMainConnect"), QStringLiteral("Dongle 主通道蓝牙连接")},
        {QStringLiteral("OtaDataPassthrough"), QStringLiteral("Dongle OTA 数据透传")},
        {QStringLiteral("MainDataPassthrough"), QStringLiteral("Dongle 主通道数据透传")},
        {QStringLiteral("BleLog"), QStringLiteral("Dongle BLE 日志开关")},
        {QStringLiteral("BleDeviceLog"), QStringLiteral("Dongle BLE 设备日志开关")},
        {QStringLiteral("Bomb"), QStringLiteral("Dongle 广播注入")},
        {QStringLiteral("GetGmac"), QStringLiteral("Dongle 读取 GMAC")},
    };
    return map;
}

}  // namespace

QStringList DongleCmdCatalog::allDongleCmdNames() {
    QStringList names = kDongleNameMap.keys();
    names.sort();
    return names;
}

QStringList DongleCmdCatalog::allDongleCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (const DongleCmdEntry& entry : kDongleCatalog) {
        if (entry.action == action)
            names.append(dongleCmdToName(entry.cmd));
    }
    names.sort();
    return names;
}

TestCaseSendAction DongleCmdCatalog::actionFor(DongleCmd cmd) {
    for (const DongleCmdEntry& entry : kDongleCatalog) {
        if (entry.cmd == cmd)
            return entry.action;
    }
    return TestCaseSendAction::Set;
}

bool DongleCmdCatalog::isCmdForAction(DongleCmd cmd, TestCaseSendAction action) {
    return actionFor(cmd) == action;
}

QString DongleCmdCatalog::dongleCmdUiLabel(const QString& enumName) {
    const QString key = enumName.trimmed();
    const QString label = dongleCmdUiLabelMap().value(key);
    if (!label.isEmpty())
        return label;
    return QStringLiteral("未登记 Dongle 指令");
}

bool DongleCmdCatalog::dongleCmdFromName(const QString& name, DongleCmd& out) {
    const QString trimmed = name.trimmed();
    const auto it = kDongleNameMap.constFind(trimmed);
    if (it != kDongleNameMap.cend()) {
        out = it.value();
        return true;
    }
    const auto legacy = kDongleLegacyNameMap.constFind(trimmed);
    if (legacy != kDongleLegacyNameMap.cend()) {
        out = legacy.value();
        return true;
    }
    return false;
}

QString DongleCmdCatalog::dongleCmdToName(DongleCmd cmd) {
    for (auto it = kDongleNameMap.cbegin(); it != kDongleNameMap.cend(); ++it) {
        if (it.value() == cmd)
            return it.key();
    }
    return QString::number(static_cast<int>(cmd));
}

bool DongleCmdCatalog::paramSchemaFor(DongleCmd cmd, DeviceCmdParamSchema& out) {
    for (const auto& e : kDongleCatalog) {
        if (e.cmd == cmd) {
            out.kind = e.kind;
            out.hint = e.hint ? QString::fromUtf8(e.hint) : QString();
            return true;
        }
    }
    return false;
}

bool DongleCmdCatalog::paramFromIniGroup(const QSettings& settings, DongleCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::Int:
        out = readSendScopedParam(settings, QStringLiteral("int"), 0).toInt();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = readSendParamMap(settings);
        return true;
    default:
        return false;
    }
}

void DongleCmdCatalog::paramToIniGroup(QSettings& settings, DongleCmd cmd, const QVariant& value) {
    removeKeysWithPrefix(settings, QStringLiteral("Param"));
    removeKeysWithPrefix(settings, sendParamIniPrefix());
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        settings.setValue(prefix + QStringLiteral("/int"), value.toInt());
        break;
    case DeviceCmdParamKind::String:
        settings.setValue(prefix + QStringLiteral("/string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        writeJsonMap(settings, prefix, value);
        break;
    default:
        break;
    }
}

// ===================== GateRegistry =====================

namespace {

const QVector<GateTypeDescriptor> kTypes = {
    {QStringLiteral("ProtocolRssiData"), QStringLiteral("蓝牙信号强度"),
     {{QStringLiteral("dbm"), QStringLiteral("信号强度(分贝)")}}},
    {QStringLiteral("ProtocolBatteryData"), QStringLiteral("电量"),
     {{QStringLiteral("percent"), QStringLiteral("电量(%)")},
      {QStringLiteral("chargeState"), QStringLiteral("充电状态")},
      {QStringLiteral("voltageMv"), QStringLiteral("电压(mV)")}}},
    {QStringLiteral("ProtocolUInt32ValueData"), QStringLiteral("数值(电流/按键电容等)"),
     {{QStringLiteral("value"), QStringLiteral("数值")},
      {QStringLiteral("auxId"), QStringLiteral("辅助编号")}}},
    {QStringLiteral("ProtocolSnData"), QStringLiteral("序列号"),
     {{QStringLiteral("value"), QStringLiteral("序列号文本")}}},
    {QStringLiteral("ProtocolBaseInfoData"), QStringLiteral("基本信息"),
     {{QStringLiteral("soft_version"), QStringLiteral("软件版本")},
      {QStringLiteral("res_version"), QStringLiteral("资源版本")},
      {QStringLiteral("product_name"), QStringLiteral("产品名称")},
      {QStringLiteral("hw_version"), QStringLiteral("硬件版本")},
      {QStringLiteral("algo_version"), QStringLiteral("算法版本")},
      {QStringLiteral("ageing_state"), QStringLiteral("老化状态")}}},
    {QStringLiteral("ProtocolPeriphStateData"), QStringLiteral("外设状态"),
     {{QStringLiteral("press0_state"), QStringLiteral("压感0状态")},
      {QStringLiteral("press1_state"), QStringLiteral("压感1状态")},
      {QStringLiteral("battery_ic_state"), QStringLiteral("电池IC状态")},
      {QStringLiteral("touch_ic_state"), QStringLiteral("触摸IC状态")},
      {QStringLiteral("led_ic_state"), QStringLiteral("LED IC状态")},
      {QStringLiteral("pd_ic_state"), QStringLiteral("PD IC状态")}}},
    {QStringLiteral("ProtocolTupleData"), QStringLiteral("设备三元组"),
     {{QStringLiteral("productId"), QStringLiteral("产品密钥")},
      {QStringLiteral("deviceId"), QStringLiteral("设备名")},
      {QStringLiteral("key"), QStringLiteral("设备密钥")}}},
    {QStringLiteral("ProtocolButtonStateData"), QStringLiteral("按键状态"),
     {{QStringLiteral("modeButtonState"), QStringLiteral("模式键状态")},
      {QStringLiteral("powerButtonState"), QStringLiteral("电源键状态")},
      {QStringLiteral("keyButtonId"), QStringLiteral("按键编号")}}},
    {QStringLiteral("ProtocolAgingStatusData"), QStringLiteral("老化状态上报"),
     {{QStringLiteral("status"), QStringLiteral("状态码")},
      {QStringLiteral("loops"), QStringLiteral("循环次数")},
      {QStringLiteral("seconds"), QStringLiteral("秒数")}}},
    {QStringLiteral("ProtocolMusicStateData"), QStringLiteral("音乐状态"),
     {{QStringLiteral("musicState"), QStringLiteral("音乐状态码")}}},
    {QStringLiteral("ProtocolResultData"), QStringLiteral("通用结果码"),
     {{QStringLiteral("result"), QStringLiteral("结果码")}}},
};

double fieldValueFromVariant(const QString& reportType, const QString& field, const QVariant& payload, bool& ok) {
    ok = false;
    if (reportType == QLatin1String("ProtocolRssiData")) {
        const auto d = payload.value<ProtocolRssiData>();
        if (field == QLatin1String("dbm")) {
            ok = true;
            return d.dbm;
        }
    } else if (reportType == QLatin1String("ProtocolBatteryData")) {
        const auto d = payload.value<ProtocolBatteryData>();
        if (field == QLatin1String("percent")) {
            ok = true;
            return d.percent;
        }
        if (field == QLatin1String("chargeState")) {
            ok = true;
            return d.chargeState;
        }
        if (field == QLatin1String("voltageMv")) {
            ok = true;
            return d.voltageMv;
        }
    } else if (reportType == QLatin1String("ProtocolUInt32ValueData")) {
        const auto d = payload.value<ProtocolUInt32ValueData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return static_cast<double>(d.value);
        }
        if (field == QLatin1String("auxId")) {
            ok = true;
            return d.auxId;
        }
    } else if (reportType == QLatin1String("ProtocolBaseInfoData")) {
        const auto d = payload.value<ProtocolBaseInfoData>();
        if (field == QLatin1String("ageing_state")) {
            ok = true;
            return d.ageing_state;
        }
    } else if (reportType == QLatin1String("ProtocolPeriphStateData")) {
        const auto d = payload.value<ProtocolPeriphStateData>();
        if (field == QLatin1String("press0_state")) {
            ok = true;
            return d.press0_state;
        }
        if (field == QLatin1String("press1_state")) {
            ok = true;
            return d.press1_state;
        }
        if (field == QLatin1String("battery_ic_state")) {
            ok = true;
            return d.battery_ic_state;
        }
        if (field == QLatin1String("touch_ic_state")) {
            ok = true;
            return d.touch_ic_state;
        }
        if (field == QLatin1String("led_ic_state")) {
            ok = true;
            return d.led_ic_state;
        }
        if (field == QLatin1String("pd_ic_state")) {
            ok = true;
            return d.pd_ic_state;
        }
    } else if (reportType == QLatin1String("ProtocolButtonStateData")) {
        const auto d = payload.value<ProtocolButtonStateData>();
        if (field == QLatin1String("modeButtonState")) {
            ok = true;
            return d.modeButtonState;
        }
        if (field == QLatin1String("powerButtonState")) {
            ok = true;
            return d.powerButtonState;
        }
        if (field == QLatin1String("keyButtonId")) {
            ok = true;
            return d.keyButtonId;
        }
    } else if (reportType == QLatin1String("ProtocolAgingStatusData")) {
        const auto d = payload.value<ProtocolAgingStatusData>();
        if (field == QLatin1String("status")) {
            ok = true;
            return d.status;
        }
        if (field == QLatin1String("loops")) {
            ok = true;
            return d.loops;
        }
        if (field == QLatin1String("seconds")) {
            ok = true;
            return static_cast<double>(d.seconds);
        }
    } else if (reportType == QLatin1String("ProtocolMusicStateData")) {
        const auto d = payload.value<ProtocolMusicStateData>();
        if (field == QLatin1String("musicState")) {
            ok = true;
            return d.musicState;
        }
    } else if (reportType == QLatin1String("ProtocolResultData")) {
        const auto d = payload.value<ProtocolResultData>();
        if (field == QLatin1String("result")) {
            ok = true;
            return d.result;
        }
    }
    return 0.0;
}

QString fieldStringFromVariant(const QString& reportType, const QString& field, const QVariant& payload, bool& ok) {
    ok = false;
    if (reportType == QLatin1String("ProtocolSnData")) {
        const auto d = payload.value<ProtocolSnData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return d.value.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolBaseInfoData")) {
        const auto d = payload.value<ProtocolBaseInfoData>();
        if (field == QLatin1String("soft_version")) {
            ok = true;
            return d.soft_version.trimmed();
        }
        if (field == QLatin1String("res_version")) {
            ok = true;
            return d.res_version.trimmed();
        }
        if (field == QLatin1String("product_name")) {
            ok = true;
            return d.product_name.trimmed();
        }
        if (field == QLatin1String("hw_version")) {
            ok = true;
            return d.hw_version.trimmed();
        }
        if (field == QLatin1String("algo_version")) {
            ok = true;
            return d.algo_version.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolTupleData")) {
        const auto d = payload.value<ProtocolTupleData>();
        if (field == QLatin1String("productId")) {
            ok = true;
            return d.productId.trimmed();
        }
        if (field == QLatin1String("deviceId")) {
            ok = true;
            return d.deviceId.trimmed();
        }
        if (field == QLatin1String("key")) {
            ok = true;
            return d.key.trimmed();
        }
    }
    return {};
}

}  // namespace

QStringList GateRegistry::reportTypes() {
    QStringList list;
    for (const auto& t : kTypes)
        list.append(t.reportType);
    return list;
}

QVector<GateTypeDescriptor> GateRegistry::allTypeDescriptors() {
    return kTypes;
}

bool GateRegistry::descriptorFor(const QString& reportType, GateTypeDescriptor& out) {
    for (const auto& t : kTypes) {
        if (t.reportType == reportType) {
            out = t;
            return true;
        }
    }
    return false;
}

QStringList GateRegistry::fieldsFor(const QString& reportType) {
    GateTypeDescriptor d;
    if (!descriptorFor(reportType, d))
        return {};
    QStringList fields;
    for (const auto& f : d.fields)
        fields.append(f.field);
    return fields;
}

bool GateRegistry::evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                            QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (!gate.enabled)
        return true;

    bool ok = false;
    double value = fieldValueFromVariant(reportType, gate.field, payload, ok);
    if (gate.op == TestCaseGateOp::CompareVersions) {
        bool strOk = false;
        QString actual = fieldStringFromVariant(reportType, gate.field, payload, strOk);
        if (!strOk) {
            passOut = false;
            detailOut = QStringLiteral("无法从上报数据读取文本字段");
            return true;
        }
        const QString expected = gate.expected.trimmed();
        passOut = !expected.isEmpty() && CommonUtils::compareVersions(expected, actual);
        detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
        return true;
    }

    if (!ok) {
        passOut = false;
        detailOut = QStringLiteral("无法从上报数据读取字段");
        return true;
    }

    double low = gate.low;
    double high = gate.high;
    if (!gate.lowSettingsKey.isEmpty())
        low = SETTINGS.value(gate.lowSettingsKey, low).toDouble();
    if (!gate.highSettingsKey.isEmpty())
        high = SETTINGS.value(gate.highSettingsKey, high).toDouble();

    switch (gate.op) {
    case TestCaseGateOp::Gt:
        passOut = value > low;
        break;
    case TestCaseGateOp::Lt:
        passOut = value < low;
        break;
    case TestCaseGateOp::Eq:
        passOut = qAbs(value - low) < 0.0001;
        break;
    default:
        passOut = value >= low && value <= high;
        break;
    }
    detailOut = QStringLiteral("当前值=%1, 允许[%2,%3]").arg(value).arg(low).arg(high);
    return true;
}

// ===================== TestCaseHookRegistry =====================

namespace {
QHash<QString, TestCaseHookFn>& hooks() {
    static QHash<QString, TestCaseHookFn> table;
    return table;
}
}  // namespace

void TestCaseHookRegistry::registerHook(const QString& hookId, TestCaseHookFn fn) {
    if (hookId.trimmed().isEmpty() || !fn)
        return;
    hooks().insert(hookId.trimmed(), std::move(fn));
}

bool TestCaseHookRegistry::contains(const QString& hookId) {
    return hooks().contains(hookId.trimmed());
}

QStringList TestCaseHookRegistry::hookIds() {
    return hooks().keys();
}

bool TestCaseHookRegistry::invoke(const QString& hookId, QFreeWork* ctx) {
    const auto it = hooks().constFind(hookId.trimmed());
    if (it == hooks().cend() || !ctx)
        return false;
    it.value()(ctx);
    return true;
}

// ===================== TestCaseRunner =====================

QStringList TestCaseRunner::loadFlowForStation(const QString& stationKey) {
    return TestCaseStore::loadStationItems(stationKey.trimmed());
}

bool TestCaseRunner::loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut) {
    return TestCaseStore::loadCase(caseName, out, errorOut);
}

QString TestCaseRunner::stepLabel(const TestCaseDefinition& def) {
    return def.meta.name.trimmed();
}

bool TestCaseRunner::needAsyncDone(const TestCaseDefinition& def) {
    if (def.hook.enabled)
        return true;
    if (isDongleBleConnectStep(def))
        return true;
    return def.gate.enabled;
}

bool TestCaseRunner::isDongleBleConnectStep(const TestCaseDefinition& def) {
    if (def.send.channel != TestCaseSendChannel::Dongle || def.send.action != TestCaseSendAction::Set)
        return false;
    return def.send.deviceCmd == QStringLiteral("BleScanConnect")
           || def.send.deviceCmd == QStringLiteral("BleDirectConnect");
}

int TestCaseRunner::commandTimeoutMs(const TestCaseDefinition& def) {
    if (def.timing.commandTimeoutMs > 0)
        return def.timing.commandTimeoutMs;
    if (isDongleBleConnectStep(def))
        return 6000;
    return def.gate.enabled ? 8000 : 3000;
}
