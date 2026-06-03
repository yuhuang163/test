#include "test_case.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
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
        {QStringLiteral("ASSEMBLY_CURRENT_TEST"), QStringLiteral("半成品工站")},
        {QStringLiteral("AGING_TEST"), QStringLiteral("老化测试工站")},
        {QStringLiteral("BUTTON_TEST"), QStringLiteral("按键测试工站")},
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

/** FlowStations / Station 组用的 ini 键须为 ASCII，否则 QSettings 会写成 %U5389%U5BB3 等形式。 */
bool isAsciiFlowStationKey(const QString& key) {
    const QString k = key.trimmed();
    if (k.isEmpty())
        return false;
    for (const QChar c : k) {
        if (c.unicode() > 127)
            return false;
        if (c == QLatin1Char('/') || c == QLatin1Char('\\'))
            return false;
    }
    return true;
}

QString allocateCustomFlowStationKey(const QVector<TestFlowStationEntry>& catalog) {
    for (int n = 1; n < 10000; ++n) {
        const QString key = QStringLiteral("FLOW_ST_%1").arg(n, 4, 10, QChar(QLatin1Char('0')));
        bool taken = false;
        for (const TestFlowStationEntry& entry : catalog) {
            if (entry.key.compare(key, Qt::CaseInsensitive) == 0) {
                taken = true;
                break;
            }
        }
        if (!taken)
            return key;
    }
    return QStringLiteral("FLOW_ST_%1").arg(QDateTime::currentMSecsSinceEpoch());
}

void migrateFlowStationIniData(const QString& oldKey, const QString& newKey, const QString& displayName) {
    if (oldKey.isEmpty() || newKey.isEmpty() || oldKey.compare(newKey, Qt::CaseInsensitive) == 0)
        return;

    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);

    const QString oldGroup = stationGroup(oldKey);
    const QString newGroup = stationGroup(newKey);

    ini.beginGroup(oldGroup);
    const bool hadData = ini.contains(QStringLiteral("Items")) || ini.contains(QStringLiteral("StopFlowOnTestFail"))
                         || ini.contains(QStringLiteral("StopOnGateFail"));
    const QVariant items = ini.value(QStringLiteral("Items"));
    const QVariant stopFlow = ini.value(QStringLiteral("StopFlowOnTestFail"));
    const QVariant stopGate = ini.value(QStringLiteral("StopOnGateFail"));
    ini.endGroup();

    if (hadData) {
        ini.beginGroup(newGroup);
        if (items.isValid())
            ini.setValue(QStringLiteral("Items"), items);
        if (stopFlow.isValid())
            ini.setValue(QStringLiteral("StopFlowOnTestFail"), stopFlow);
        if (stopGate.isValid())
            ini.setValue(QStringLiteral("StopOnGateFail"), stopGate);
        ini.endGroup();
        ini.beginGroup(oldGroup);
        ini.remove(QString());
        ini.endGroup();
    }

    TestFlowMeta meta;
    TestCaseStore::loadFlowMeta(meta);
    bool metaChanged = false;
    if (meta.selectedStation.compare(oldKey, Qt::CaseInsensitive) == 0) {
        meta.selectedStation = newKey;
        metaChanged = true;
    }
    if (meta.selectedStationName.compare(oldKey, Qt::CaseInsensitive) == 0) {
        meta.selectedStationName = displayName.isEmpty() ? newKey : displayName;
        metaChanged = true;
    }
    if (metaChanged)
        TestCaseStore::saveFlowMeta(meta);

    syncTestCaseIni(ini, flowPath);
}

bool normalizeFlowStationCatalogKeys(QVector<TestFlowStationEntry>& catalog) {
    bool changed = false;
    for (int i = 0; i < catalog.size(); ++i) {
        if (isAsciiFlowStationKey(catalog[i].key))
            continue;
        const QString oldKey = catalog[i].key;
        const QString newKey = allocateCustomFlowStationKey(catalog);
        migrateFlowStationIniData(oldKey, newKey, catalog[i].displayName);
        catalog[i].key = newKey;
        changed = true;
    }
    return changed;
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
    if (normalizeFlowStationCatalogKeys(result))
        saveFlowStationCatalog(result);
    return result;
}

bool TestCaseStore::saveFlowStationCatalog(const QVector<TestFlowStationEntry>& entries) {
    QVector<TestFlowStationEntry> normalized = entries;
    normalizeFlowStationCatalogKeys(normalized);

    TestCasePaths::ensureRootDir();
    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(flowStationsCatalogGroup());
    const QStringList oldKeys = ini.childKeys();
    for (const QString& oldKey : oldKeys)
        ini.remove(oldKey);
    for (const TestFlowStationEntry& entry : normalized) {
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
    QVector<TestFlowStationEntry> catalog = loadFlowStationCatalog();
    const QString key = presetKey.isEmpty() ? allocateCustomFlowStationKey(catalog) : presetKey;
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

bool TestCaseStore::renameFlowStation(const QString& stationKey, const QString& newDisplayName,
                                      QString* errorOut) {
    const QString k = stationKey.trimmed();
    const QString name = newDisplayName.trimmed();
    if (k.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("工站键无效");
        return false;
    }
    if (!TestCasePaths::isValidCaseFileName(name, errorOut))
        return false;

    QVector<TestFlowStationEntry> catalog = loadFlowStationCatalog();
    int targetIdx = -1;
    for (int i = 0; i < catalog.size(); ++i) {
        if (catalog[i].key.compare(k, Qt::CaseInsensitive) == 0) {
            targetIdx = i;
            break;
        }
    }
    if (targetIdx < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("工站不在目录中：%1").arg(k);
        return false;
    }
    if (catalog[targetIdx].displayName == name)
        return true;

    const QString presetKeyForName = lookupPresetKeyFromDisplayName(name);
    if (!presetKeyForName.isEmpty() && presetKeyForName.compare(k, Qt::CaseInsensitive) != 0) {
        if (errorOut) {
            *errorOut = QStringLiteral("名称与工站「%1」冲突")
                              .arg(lookupPresetDisplayName(presetKeyForName));
        }
        return false;
    }
    for (int i = 0; i < catalog.size(); ++i) {
        if (i == targetIdx)
            continue;
        if (catalog[i].displayName == name) {
            if (errorOut)
                *errorOut = QStringLiteral("工站名称已存在：%1").arg(name);
            return false;
        }
    }

    catalog[targetIdx].displayName = name;
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
    out.send.productProtocol =
        DeviceCmdCatalog::productProtocolFromIni(ini.value(QStringLiteral("Send/Protocol")).toString());
    if (out.send.productProtocol == TestCaseProductProtocol::Qfctp
        && out.send.deviceCmd.compare(QStringLiteral("BaseInfo"), Qt::CaseInsensitive) == 0) {
        out.send.deviceCmd = QStringLiteral("SoftVersionRead");
    }
    const QString channelIni = ini.value(QStringLiteral("Send/Channel")).toString().trimmed();
    if (channelIni.compare(QStringLiteral("Dongle"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Dongle;
    } else if (channelIni.compare(QStringLiteral("Cloud"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Cloud;
    } else if (channelIni.compare(QStringLiteral("ProductSerial"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::ProductSerial;
    } else if (channelIni.compare(QStringLiteral("Product"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Product;
    } else {
        ProductSerialCmd inferSerial;
        TupleCmd inferTuple;
        DongleCmd inferDongle;
        if (ProductSerialCmdCatalog::productSerialCmdFromName(out.send.deviceCmd, inferSerial)) {
            out.send.channel = TestCaseSendChannel::ProductSerial;
        } else if (TupleCmdCatalog::tupleCmdFromName(out.send.deviceCmd, inferTuple)) {
            out.send.channel = TestCaseSendChannel::Cloud;
        } else if (DongleCmdCatalog::dongleCmdFromName(out.send.deviceCmd, inferDongle)) {
            out.send.channel = TestCaseSendChannel::Dongle;
        } else {
            out.send.channel = TestCaseSendChannel::Product;
        }
    }
    if (out.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(out.send.deviceCmd, dongleCmd)) {
            if (!DongleCmdCatalog::isCmdForAction(dongleCmd, out.send.action))
                out.send.action = DongleCmdCatalog::actionFor(dongleCmd);
            DongleCmdCatalog::paramFromIniGroup(ini, dongleCmd, out.send.param);
        }
    } else if (out.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (TupleCmdCatalog::tupleCmdFromName(out.send.deviceCmd, tupleCmd)) {
            if (!TupleCmdCatalog::isCmdForAction(tupleCmd, out.send.action))
                out.send.action = TupleCmdCatalog::actionFor(tupleCmd);
            TupleCmdCatalog::paramFromIniGroup(ini, tupleCmd, out.send.param);
        }
    } else if (out.send.channel == TestCaseSendChannel::ProductSerial) {
        ProductSerialCmd serialCmd;
        if (ProductSerialCmdCatalog::productSerialCmdFromName(out.send.deviceCmd, serialCmd)) {
            out.send.action = ProductSerialCmdCatalog::actionFor(serialCmd);
            out.send.param = QVariant();
        }
    } else {
        DeviceCmd cmd;
        if (DeviceCmdCatalog::deviceCmdFromName(out.send.deviceCmd, cmd)) {
            if (!DeviceCmdCatalog::isCmdForAction(cmd, out.send.action))
                out.send.action = DeviceCmdCatalog::actionFor(cmd);
            DeviceCmdCatalog::paramFromIniGroup(ini, cmd, out.send.param);
        }
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
    out.gate.expectedSettingsKey = ini.value(QStringLiteral("Gate/ExpectedSettingsKey")).toString();
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
        } else if (out.hook.hookId == QStringLiteral("CLOUD_TUPLE_FETCH")) {
            out.hook.enabled = false;
            out.send.channel = TestCaseSendChannel::Cloud;
            out.send.action = TestCaseSendAction::Get;
            out.send.deviceCmd = QStringLiteral("ApplyTupleByMac");
            if (!out.send.param.isValid() || out.send.param.toString().trimmed().isEmpty())
                out.send.param = QStringLiteral("$MAC");
        } else if (out.hook.hookId == QStringLiteral("TUPLE_WRITE_REPORT")) {
            out.hook.enabled = false;
            out.send.channel = TestCaseSendChannel::Cloud;
            out.send.action = TestCaseSendAction::Set;
            out.send.deviceCmd = QStringLiteral("ReportWriteRecord");
            out.send.param = QVariant();
        } else {
            QString serialCmd;
            const QString hid = out.hook.hookId;
            if (hid == QStringLiteral("PROD_INST_RESET_ACK")
                || hid.startsWith(QStringLiteral("PROD_INST_RESET_ACK_"))) {
                serialCmd = QStringLiteral("InstrumentReset");
            } else if (hid == QStringLiteral("PROD_INST_START_RX_2402_1M")) {
                serialCmd = QStringLiteral("StartRx2402Ble1M");
            } else if (hid == QStringLiteral("PROD_INST_START_RX_2440_1M")) {
                serialCmd = QStringLiteral("StartRx2440Ble1M");
            } else if (hid == QStringLiteral("PROD_INST_START_RX_2480_1M")) {
                serialCmd = QStringLiteral("StartRx2480Ble1M");
            } else if (hid == QStringLiteral("PROD_INST_START_RX_2402_2M")) {
                serialCmd = QStringLiteral("StartRx2402Ble2M");
            } else if (hid == QStringLiteral("PROD_INST_START_RX_2440_2M")) {
                serialCmd = QStringLiteral("StartRx2440Ble2M");
            } else if (hid == QStringLiteral("PROD_INST_START_RX_2480_2M")) {
                serialCmd = QStringLiteral("StartRx2480Ble2M");
            } else if (hid == QStringLiteral("PROD_INST_STOP_RX_PER")
                       || hid.startsWith(QStringLiteral("PROD_INST_STOP_RX_PER_"))) {
                serialCmd = QStringLiteral("StopRxAndPer");
            }
            if (!serialCmd.isEmpty()) {
                out.hook.enabled = false;
                out.hook.hookId.clear();
                out.send.channel = TestCaseSendChannel::ProductSerial;
                out.send.action = TestCaseSendAction::Set;
                out.send.deviceCmd = serialCmd;
                out.send.param = QVariant();
                if (out.timing.commandTimeoutMs <= 0)
                    out.timing.commandTimeoutMs = 30000;
            }
        }
    }

    // 基本信息软件版本：旧 ini 误用 range + soft_version，改为版本比对（期望写在 case ini 的 Gate/Expected）
    if (out.gate.enabled && out.gate.reportType == QStringLiteral("ProtocolBaseInfoData")
        && out.gate.field == QStringLiteral("soft_version") && out.gate.op == TestCaseGateOp::Range) {
        out.gate.op = TestCaseGateOp::CompareVersions;
    }
    // 旧配置从全局设置读版本：清空，改由 case ini Gate/Expected 配置
    if (out.gate.enabled && out.gate.reportType == QStringLiteral("ProtocolBaseInfoData")
        && out.gate.field == QStringLiteral("soft_version")
        && out.gate.expectedSettingsKey == QStringLiteral("ProductInfo/Software_Version")
        && out.gate.expected.trimmed().isEmpty()) {
        out.gate.expectedSettingsKey.clear();
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
    QString channelStr = QStringLiteral("Product");
    if (def.send.channel == TestCaseSendChannel::Dongle)
        channelStr = QStringLiteral("Dongle");
    else if (def.send.channel == TestCaseSendChannel::Cloud)
        channelStr = QStringLiteral("Cloud");
    else if (def.send.channel == TestCaseSendChannel::ProductSerial)
        channelStr = QStringLiteral("ProductSerial");
    ini.setValue(QStringLiteral("Send/Channel"), channelStr);
    if (def.send.channel == TestCaseSendChannel::Product)
        ini.setValue(QStringLiteral("Send/Protocol"),
                     DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol));
    ini.setValue(QStringLiteral("Send/DeviceCmd"), def.send.deviceCmd);
    if (def.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd))
            DongleCmdCatalog::paramToIniGroup(ini, dongleCmd, def.send.param);
    } else if (def.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd))
            TupleCmdCatalog::paramToIniGroup(ini, tupleCmd, def.send.param);
    } else if (def.send.channel != TestCaseSendChannel::ProductSerial) {
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
    ini.setValue(QStringLiteral("Gate/ExpectedSettingsKey"), def.gate.expectedSettingsKey);
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
    } else if (def.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (!TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd)) {
            errors.append(QStringLiteral("云端测试指令无效"));
        } else if (!TupleCmdCatalog::isCmdForAction(tupleCmd, def.send.action)) {
            errors.append(QStringLiteral("云端指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!TupleCmdCatalog::paramSchemaFor(tupleCmd, schema))
                errors.append(QStringLiteral("该云端指令尚未配置参数模板，请联系工程师"));
        }
    } else if (def.send.channel == TestCaseSendChannel::ProductSerial) {
        ProductSerialCmd serialCmd;
        if (!ProductSerialCmdCatalog::productSerialCmdFromName(def.send.deviceCmd, serialCmd)) {
            errors.append(QStringLiteral("产品串口测试指令无效"));
        } else if (!ProductSerialCmdCatalog::isCmdForAction(serialCmd, def.send.action)) {
            errors.append(QStringLiteral("产品串口指令仅支持「设置」"));
        }
    } else {
        DeviceCmd cmd;
        if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
            errors.append(QStringLiteral("产品测试指令无效"));
        } else if (!DeviceCmdCatalog::isCmdForAction(cmd, def.send.action)) {
            errors.append(QStringLiteral("产品指令与操作方式不匹配"));
        } else if (!DeviceCmdCatalog::isCmdSupportedByProtocol(cmd, def.send.productProtocol, def.send.action)) {
            errors.append(QStringLiteral("该指令不属于所选产品协议（FCTP/QPB）"));
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
    {DeviceCmd::ForbidSleep, DeviceCmdParamKind::JsonMap,
     "禁止休眠：value=1 开启，0 关闭\n示例：{\"value\":1} 或 value=1"},
    {DeviceCmd::Sn, DeviceCmdParamKind::JsonMap,
     "读写 SN：which_sn 类型\n"
     "  1=整机SN(TAIL)  6=deviceName(SUB_PID)  7=productKey(SKUID)\n"
     "写 productKey：which_sn=7，sn=$TUPLE_PRODUCT_KEY\n"
     "写 deviceName：which_sn=6，sn=$TUPLE_DEVICE_NAME\n"
     "写整机 SN：which_sn=1，sn=具体值或 $TAIL_SN"},
    {DeviceCmd::WriteKey, DeviceCmdParamKind::JsonMap,
     "写 deviceSecret：value=$TUPLE_DEVICE_SECRET（默认取云端已获取三元组）"},
    {DeviceCmd::SoftVersionRead, DeviceCmdParamKind::None,
     "FCTP：Get SoftVersionRead 回包 soft_version（软件版本），无需参数\n"
     "卡控：ReportType=ProtocolBaseInfoData，Field=soft_version，Op=compareVersions，Expected=目标版本"},
    {DeviceCmd::BaseInfo, DeviceCmdParamKind::None,
     "QPB：读写基础信息（含软件/资源版本等）\nFCTP 请改用 SoftVersionRead（读取版本号）"},
    {DeviceCmd::GetBattery, DeviceCmdParamKind::None, "无需参数"},
    {DeviceCmd::FacResult, DeviceCmdParamKind::JsonMap,
     "产测结果：done=1 通过(留空等同1)，done=0 失败\n示例：done=1 或 {\"done\":1}"},
    {DeviceCmd::BurningMode, DeviceCmdParamKind::JsonMap,
     "老化：mode、seconds，可选 switch\n示例：{\"mode\":1,\"seconds\":3600}"},
    {DeviceCmd::Sleep, DeviceCmdParamKind::JsonMap, "休眠：switch=1 进入，0 退出\n示例：{\"switch\":1}"},
    {DeviceCmd::ShipMode, DeviceCmdParamKind::None, "无需参数"},
    {DeviceCmd::FacMode, DeviceCmdParamKind::JsonMap,
     "工厂模式：value=1 进入，0 退出\n示例：{\"value\":1}"},
    {DeviceCmd::DevReset, DeviceCmdParamKind::None, "无需参数"},
    {DeviceCmd::WifiDisconnect, DeviceCmdParamKind::None, "无需参数"},
    {DeviceCmd::WifiConnect, DeviceCmdParamKind::JsonMap,
     "WiFi：name=SSID，password=密码\n示例：name=TestAP\npassword=12345678"},
    {DeviceCmd::RssiRead, DeviceCmdParamKind::JsonMap,
     "RSSI：mode=0 读 BLE，mode=1 读 BT\n示例：{\"mode\":0}"},
    {DeviceCmd::ChargeCurrentRead, DeviceCmdParamKind::None, "无需参数"},
    {DeviceCmd::TupleRead, DeviceCmdParamKind::None, "无需参数；比对在步骤逻辑中与云端三元组比较"},
    {DeviceCmd::PeriphState, DeviceCmdParamKind::None, "无需参数"},
    {DeviceCmd::FactoryReset, DeviceCmdParamKind::None, "无需参数"},
};

#define X(name) \
    { QStringLiteral(#name), DeviceCmd::name },
const QHash<QString, DeviceCmd> kNameMap = {
    X(ForbidSleep) X(Sn) X(SoftVersionRead) X(BaseInfo) X(GetBattery) X(FacResult) X(BurningMode) X(Sleep) X(ShipMode) X(FacMode)
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

// 与 qfctp.cpp / qpb.cpp 中 set()/get() 的 case 保持一致（未实现的枚举不出现在设置页）。
// 使用 QVector 而非 QSet：enum class DeviceCmd 未提供 qHash，QSet 无法编译。
const QVector<DeviceCmd>& qfctpSetCmds() {
    static const QVector<DeviceCmd> cmds = {
        DeviceCmd::FacMode,         DeviceCmd::BurningMode,     DeviceCmd::SuctionMode,
        DeviceCmd::BtSignalMode,    DeviceCmd::BtNoSignalMode,  DeviceCmd::BtFreqMode,
        DeviceCmd::Sleep,           DeviceCmd::WriteKey,        DeviceCmd::Sn,
        DeviceCmd::FacResult,       DeviceCmd::TrimSet,         DeviceCmd::MacWrite,
        DeviceCmd::NightLightSet,   DeviceCmd::LedTest,         DeviceCmd::FactoryReset,
        DeviceCmd::ShipMode,        DeviceCmd::LcdBacklight,    DeviceCmd::LightReportControl,
        DeviceCmd::LightCalibWrite, DeviceCmd::CompensationSet,
    };
    return cmds;
}

const QVector<DeviceCmd>& qfctpGetCmds() {
    static const QVector<DeviceCmd> cmds = {
        DeviceCmd::Sn,               DeviceCmd::TrimRead,         DeviceCmd::MacRead,
        DeviceCmd::SoftVersionRead,  DeviceCmd::TupleRead,        DeviceCmd::LightSensorInfo,
        DeviceCmd::RssiRead,         DeviceCmd::KeySignalRead,    DeviceCmd::LightCalibRead,
        DeviceCmd::ChargeCurrentRead, DeviceCmd::AgingStatusRead, DeviceCmd::FactoryDoneRead,
        DeviceCmd::DeviceExceptionRead, DeviceCmd::DeviceInfo,   DeviceCmd::PeriphState,
        DeviceCmd::GetBattery,
    };
    return cmds;
}

const QVector<DeviceCmd>& qpbSetCmds() {
    static const QVector<DeviceCmd> cmds = {
        DeviceCmd::MotorCali,        DeviceCmd::WifiConnect,      DeviceCmd::CameraState,
        DeviceCmd::PressCollect,     DeviceCmd::ImuCollect,       DeviceCmd::PressCaliResult,
        DeviceCmd::ImuCaliResult,    DeviceCmd::NewImuCaliResult, DeviceCmd::LocalOta,
        DeviceCmd::StartOtaApp,    DeviceCmd::StartMultiBleOtaApp, DeviceCmd::ConfigNetworkApp,
        DeviceCmd::MotorDampingState, DeviceCmd::RgbColor,        DeviceCmd::LedColor,
        DeviceCmd::MotorTestState,   DeviceCmd::MotorCaliState,   DeviceCmd::FacResult,
        DeviceCmd::ScreenColor,      DeviceCmd::ShipMode,         DeviceCmd::MotorAdcSwitch,
        DeviceCmd::MotorState,       DeviceCmd::MotorParam,       DeviceCmd::MotorCaliResultParam,
        DeviceCmd::Music,            DeviceCmd::BurningMode,      DeviceCmd::BrushRecord,
        DeviceCmd::BrushTime,        DeviceCmd::Sleep,             DeviceCmd::ForbidSleep,
        DeviceCmd::ScreenCameraState, DeviceCmd::CameraLightState, DeviceCmd::CameraSupportState,
        DeviceCmd::CameraExposureTime, DeviceCmd::DevReset,       DeviceCmd::BrushReset,
        DeviceCmd::DeviceMode,       DeviceCmd::BrushControl,     DeviceCmd::FacMode,
        DeviceCmd::CameraPictureState, DeviceCmd::Sn,             DeviceCmd::IAmApp,
        DeviceCmd::WifiDisconnect,   DeviceCmd::ServoMotorInfo, DeviceCmd::MicControl,
        DeviceCmd::UploadRecordData, DeviceCmd::SevorMotorParam, DeviceCmd::NewWifiConnect,
    };
    return cmds;
}

const QVector<DeviceCmd>& qpbGetCmds() {
    static const QVector<DeviceCmd> cmds = {
        DeviceCmd::GetBattery,       DeviceCmd::BaseInfo,         DeviceCmd::GetImuCaliResult,
        DeviceCmd::GetPressCaliResult, DeviceCmd::DeviceInfo,     DeviceCmd::PeriphState,
        DeviceCmd::ConnectInfo,      DeviceCmd::WifiInfo,         DeviceCmd::GetServoMotorInfo,
        DeviceCmd::NowMusicInfo,     DeviceCmd::SdCardInfo,       DeviceCmd::LightSensorInfo,
        DeviceCmd::ButtonState,      DeviceCmd::Sn,               DeviceCmd::BurshBacklog,
    };
    return cmds;
}

bool deviceCmdInProtocolSet(DeviceCmd cmd, TestCaseProductProtocol protocol, TestCaseSendAction action) {
    const QVector<DeviceCmd>& setPool =
        protocol == TestCaseProductProtocol::Qpb ? qpbSetCmds() : qfctpSetCmds();
    const QVector<DeviceCmd>& getPool =
        protocol == TestCaseProductProtocol::Qpb ? qpbGetCmds() : qfctpGetCmds();
    if (cmd == DeviceCmd::Sn)
        return action == TestCaseSendAction::Set ? setPool.contains(cmd) : getPool.contains(cmd);
    if (action == TestCaseSendAction::Set)
        return setPool.contains(cmd);
    return getPool.contains(cmd);
}

/** 设置页「指令内容」下拉：去掉与「操作方式」重复的动作前缀。 */
QString cmdPickerDisplayLabel(QString label) {
    label = label.trimmed();
    if (label.startsWith(QStringLiteral("Dongle "), Qt::CaseInsensitive))
        label = label.mid(7).trimmed();
    static const QStringList prefixes = {
        QStringLiteral("设置"), QStringLiteral("写入"), QStringLiteral("读取"),
        QStringLiteral("获取"), QStringLiteral("上报"),
    };
    for (const QString& prefix : prefixes) {
        if (label.startsWith(prefix)) {
            label = label.mid(prefix.size()).trimmed();
            break;
        }
    }
    return label;
}

const QHash<QString, QString>& cmdUiLabelMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("ForbidSleep"), QStringLiteral("禁止休眠")},
        {QStringLiteral("Sn"), QStringLiteral("序列号")},
        {QStringLiteral("SoftVersionRead"), QStringLiteral("版本号")},
        {QStringLiteral("BaseInfo"), QStringLiteral("基本信息")},
        {QStringLiteral("GetBattery"), QStringLiteral("电量")},
        {QStringLiteral("FacResult"), QStringLiteral("产测结果")},
        {QStringLiteral("BurningMode"), QStringLiteral("老化模式")},
        {QStringLiteral("Sleep"), QStringLiteral("休眠")},
        {QStringLiteral("ShipMode"), QStringLiteral("关机")},
        {QStringLiteral("FacMode"), QStringLiteral("工厂模式")},
        {QStringLiteral("DevReset"), QStringLiteral("设备复位")},
        {QStringLiteral("WifiDisconnect"), QStringLiteral("断开无线网络")},
        {QStringLiteral("WifiConnect"), QStringLiteral("连接无线网络")},
        {QStringLiteral("RssiRead"), QStringLiteral("信号强度")},
        {QStringLiteral("ChargeCurrentRead"), QStringLiteral("充电电流")},
        {QStringLiteral("TupleRead"), QStringLiteral("三元组")},
        {QStringLiteral("PeriphState"), QStringLiteral("外设状态")},
        {QStringLiteral("FactoryReset"), QStringLiteral("恢复出厂")},
        {QStringLiteral("PressSensorTemp"), QStringLiteral("压力传感器温度")},
        {QStringLiteral("UartReceive"), QStringLiteral("串口接收开关")},
        {QStringLiteral("RgbColor"), QStringLiteral("RGB颜色")},
        {QStringLiteral("MotorCali"), QStringLiteral("电机校准")},
        {QStringLiteral("MotorDampingState"), QStringLiteral("电机阻尼状态")},
        {QStringLiteral("MotorTestState"), QStringLiteral("电机测试状态")},
        {QStringLiteral("MotorCaliState"), QStringLiteral("电机校准状态")},
        {QStringLiteral("ScreenColor"), QStringLiteral("屏幕颜色")},
        {QStringLiteral("LedColor"), QStringLiteral("指示灯颜色")},
        {QStringLiteral("MotorAdcSwitch"), QStringLiteral("电机ADC开关")},
        {QStringLiteral("MotorParam"), QStringLiteral("电机参数")},
        {QStringLiteral("MotorState"), QStringLiteral("电机运行状态")},
        {QStringLiteral("MotorCaliResultParam"), QStringLiteral("电机校准结果参数")},
        {QStringLiteral("Music"), QStringLiteral("音乐")},
        {QStringLiteral("BrushRecord"), QStringLiteral("刷牙记录")},
        {QStringLiteral("BrushTime"), QStringLiteral("刷牙时间")},
        {QStringLiteral("CameraState"), QStringLiteral("摄像头状态")},
        {QStringLiteral("ScreenCameraState"), QStringLiteral("屏幕摄像头状态")},
        {QStringLiteral("CameraLightState"), QStringLiteral("摄像头补光状态")},
        {QStringLiteral("CameraSupportState"), QStringLiteral("摄像头支持状态")},
        {QStringLiteral("CameraExposureTime"), QStringLiteral("摄像头曝光时间")},
        {QStringLiteral("BrushReset"), QStringLiteral("刷牙复位")},
        {QStringLiteral("PressCaliResult"), QStringLiteral("压力校准结果")},
        {QStringLiteral("ImuCaliResult"), QStringLiteral("惯性校准结果")},
        {QStringLiteral("NewImuCaliResult"), QStringLiteral("新惯性校准结果")},
        {QStringLiteral("DeviceMode"), QStringLiteral("设备模式")},
        {QStringLiteral("BrushControl"), QStringLiteral("刷牙控制")},
        {QStringLiteral("CameraPictureState"), QStringLiteral("摄像头拍照状态")},
        {QStringLiteral("LocalOta"), QStringLiteral("本地固件升级")},
        {QStringLiteral("StartOtaApp"), QStringLiteral("升级应用")},
        {QStringLiteral("IAmApp"), QStringLiteral("应用身份声明")},
        {QStringLiteral("ConfigNetworkApp"), QStringLiteral("配网应用")},
        {QStringLiteral("StartMultiBleOtaApp"), QStringLiteral("多设备蓝牙升级")},
        {QStringLiteral("PressCollect"), QStringLiteral("压力采集")},
        {QStringLiteral("ImuCollect"), QStringLiteral("惯性传感器采集")},
        {QStringLiteral("CameraFaultDataPacket"), QStringLiteral("摄像头故障数据")},
        {QStringLiteral("ServoMotorInfo"), QStringLiteral("舵机信息")},
        {QStringLiteral("MicControl"), QStringLiteral("麦克风控制")},
        {QStringLiteral("UploadRecordData"), QStringLiteral("记录数据上传")},
        {QStringLiteral("NewWifiConnect"), QStringLiteral("无线网络(新协议)")},
        {QStringLiteral("SevorMotorParam"), QStringLiteral("舵机参数")},
        {QStringLiteral("SuctionMode"), QStringLiteral("吸力模式")},
        {QStringLiteral("BtSignalMode"), QStringLiteral("蓝牙信号模式")},
        {QStringLiteral("BtNoSignalMode"), QStringLiteral("蓝牙无信号模式")},
        {QStringLiteral("BtFreqMode"), QStringLiteral("蓝牙定频模式")},
        {QStringLiteral("WriteKey"), QStringLiteral("密钥")},
        {QStringLiteral("TrimSet"), QStringLiteral("微调值")},
        {QStringLiteral("MacWrite"), QStringLiteral("网卡地址")},
        {QStringLiteral("NightLightSet"), QStringLiteral("夜灯")},
        {QStringLiteral("LedTest"), QStringLiteral("指示灯测试")},
        {QStringLiteral("LcdBacklight"), QStringLiteral("屏幕背光")},
        {QStringLiteral("LightReportControl"), QStringLiteral("灯光上报控制")},
        {QStringLiteral("LightCalibWrite"), QStringLiteral("灯光校准")},
        {QStringLiteral("CompensationSet"), QStringLiteral("补偿参数")},
        {QStringLiteral("NowMusicInfo"), QStringLiteral("当前音乐信息")},
        {QStringLiteral("SdCardInfo"), QStringLiteral("存储卡信息")},
        {QStringLiteral("LightSensorInfo"), QStringLiteral("环境光传感器信息")},
        {QStringLiteral("ButtonState"), QStringLiteral("按键状态")},
        {QStringLiteral("GetPressCaliResult"), QStringLiteral("压力校准结果")},
        {QStringLiteral("GetImuCaliResult"), QStringLiteral("惯性校准结果")},
        {QStringLiteral("DeviceInfo"), QStringLiteral("设备信息")},
        {QStringLiteral("ConnectInfo"), QStringLiteral("连接信息")},
        {QStringLiteral("WifiInfo"), QStringLiteral("无线网络信息")},
        {QStringLiteral("GetServoMotorInfo"), QStringLiteral("舵机信息")},
        {QStringLiteral("BurshBacklog"), QStringLiteral("刷牙积压数据")},
        {QStringLiteral("TrimRead"), QStringLiteral("微调值")},
        {QStringLiteral("MacRead"), QStringLiteral("网卡地址")},
        {QStringLiteral("KeySignalRead"), QStringLiteral("按键信号")},
        {QStringLiteral("LightCalibRead"), QStringLiteral("灯光校准")},
        {QStringLiteral("AgingStatusRead"), QStringLiteral("老化状态")},
        {QStringLiteral("FactoryDoneRead"), QStringLiteral("产测完成标志")},
        {QStringLiteral("DeviceExceptionRead"), QStringLiteral("设备异常")},
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

int jsonMapIntValue(const QVariantMap& map, int defaultValue = 0) {
    if (map.contains(QStringLiteral("int")))
        return map.value(QStringLiteral("int")).toInt();
    if (map.contains(QStringLiteral("value")))
        return map.value(QStringLiteral("value")).toInt();
    if (map.size() == 1)
        return map.constBegin().value().toInt();
    return defaultValue;
}

QVariantMap jsonMapWithLegacyInt(const QSettings& settings) {
    QVariantMap map = readSendParamMap(settings);
    if (!map.isEmpty())
        return map;
    const QVariant legacyInt = readSendScopedParam(settings, QStringLiteral("int"), QVariant());
    if (legacyInt.isValid())
        map.insert(QStringLiteral("value"), legacyInt);
    const QString legacyStr = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
    if (!legacyStr.isEmpty())
        map.insert(QStringLiteral("value"), legacyStr);
    return map;
}

}  // namespace

QStringList DeviceCmdCatalog::allDeviceCmdNames() {
    QStringList names = kNameMap.keys();
    names.sort();
    return names;
}

QStringList DeviceCmdCatalog::allDeviceCmdNames(TestCaseSendAction action) {
    return allDeviceCmdNames(action, TestCaseProductProtocol::Qfctp);
}

QStringList DeviceCmdCatalog::allDeviceCmdNames(TestCaseSendAction action, TestCaseProductProtocol protocol) {
    QStringList names;
    for (auto it = kNameMap.cbegin(); it != kNameMap.cend(); ++it) {
        if (!isCmdSupportedByProtocol(it.value(), protocol, action))
            continue;
        if (!isCmdForAction(it.value(), action))
            continue;
        names.append(it.key());
    }
    names.sort();
    return names;
}

TestCaseProductProtocol DeviceCmdCatalog::productProtocolFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("Qpb"), Qt::CaseInsensitive) == 0
        || t.compare(QStringLiteral("PB"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qpb;
    return TestCaseProductProtocol::Qfctp;
}

QString DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol protocol) {
    return protocol == TestCaseProductProtocol::Qpb ? QStringLiteral("Qpb") : QStringLiteral("Qfctp");
}

QString DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol protocol) {
    return protocol == TestCaseProductProtocol::Qpb ? QStringLiteral("QPB") : QStringLiteral("FCTP");
}

bool DeviceCmdCatalog::isCmdSupportedByProtocol(DeviceCmd cmd, TestCaseProductProtocol protocol,
                                                  TestCaseSendAction action) {
    if (!deviceCmdInProtocolSet(cmd, protocol, action))
        return false;
    return isCmdForAction(cmd, action);
}

TestCaseSendAction DeviceCmdCatalog::actionFor(DeviceCmd cmd) {
    switch (cmd) {
    case DeviceCmd::Sn:
        return TestCaseSendAction::Set;
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
        return TestCaseSendAction::Get;
    default:
        break;
    }
    if (static_cast<int>(cmd) >= static_cast<int>(DeviceCmd::NowMusicInfo))
        return TestCaseSendAction::Get;
    return TestCaseSendAction::Set;
}

bool DeviceCmdCatalog::isCmdForAction(DeviceCmd cmd, TestCaseSendAction action) {
    if (cmd == DeviceCmd::Sn)
        return action == TestCaseSendAction::Set || action == TestCaseSendAction::Get;
    return actionFor(cmd) == action;
}

QString DeviceCmdCatalog::deviceCmdUiLabel(const QString& enumName) {
    return deviceCmdUiLabel(enumName, TestCaseProductProtocol::Qfctp);
}

QString DeviceCmdCatalog::deviceCmdUiLabel(const QString& enumName, TestCaseProductProtocol protocol) {
    const QString key = enumName.trimmed();
    const QString label = cmdUiLabelMap().value(key);
    if (!label.isEmpty())
        return cmdPickerDisplayLabel(label);
    return QStringLiteral("未登记指令");
}

bool DeviceCmdCatalog::deviceCmdFromName(const QString& name, DeviceCmd& out) {
    const QString trimmed = name.trimmed();
    if (trimmed.compare(QStringLiteral("BaseInfo"), Qt::CaseInsensitive) == 0) {
        out = DeviceCmd::BaseInfo;
        return true;
    }
    const auto it = kNameMap.constFind(trimmed);
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
    // 协议枚举已登记、未在 kCatalog 单独维护：默认无参（设置页不显示参数区；有参指令请加入 kCatalog）
    if (kNameMap.contains(deviceCmdToName(cmd))) {
        out.kind = DeviceCmdParamKind::None;
        out.hint = QStringLiteral("未在产测模板登记：一般无需参数；若协议需要请联系工程师加入 kCatalog");
        return true;
    }
    return false;
}

QString DeviceCmdCatalog::paramUiHint(const QString& deviceCmdName) {
    DeviceCmd cmd;
    if (!deviceCmdFromName(deviceCmdName, cmd))
        return QStringLiteral("未知产品指令");
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return QStringLiteral("该指令未登记");
    return schema.hint.trimmed().isEmpty() ? QStringLiteral("按协议填写 JSON 或 name=value 行")
                                           : schema.hint;
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
        out = jsonMapWithLegacyInt(settings);
        return true;
    }
    return false;
}

QVariant DeviceCmdCatalog::normalizeSendParam(DeviceCmd cmd, const QVariant& param) {
    if (!param.canConvert<QVariantMap>())
        return param;

    const QVariantMap map = param.toMap();
    if (map.isEmpty()) {
        switch (cmd) {
        case DeviceCmd::SoftVersionRead:
        case DeviceCmd::BaseInfo:
        case DeviceCmd::GetBattery:
        case DeviceCmd::DevReset:
        case DeviceCmd::WifiDisconnect:
        case DeviceCmd::ChargeCurrentRead:
        case DeviceCmd::TupleRead:
        case DeviceCmd::PeriphState:
        case DeviceCmd::FactoryReset:
        case DeviceCmd::ShipMode:
            return QVariant();
        default:
            return QVariant();
        }
    }

    switch (cmd) {
    case DeviceCmd::ForbidSleep:
    case DeviceCmd::FacMode:
        return jsonMapIntValue(map, 1);
    case DeviceCmd::FacResult: {
        if (map.contains(QStringLiteral("done")))
            return map;
        QVariantMap out = map;
        if (!out.contains(QStringLiteral("done")))
            out.insert(QStringLiteral("done"), jsonMapIntValue(map, 1));
        return out;
    }
    case DeviceCmd::Sn: {
        const auto whichFromMap = [&map]() -> int {
            if (map.contains(QStringLiteral("which_sn")))
                return map.value(QStringLiteral("which_sn")).toInt();
            if (map.contains(QStringLiteral("which")))
                return map.value(QStringLiteral("which")).toInt();
            if (map.contains(QStringLiteral("type")))
                return map.value(QStringLiteral("type")).toInt();
            return jsonMapIntValue(map, 0);
        };
        QByteArray snBytes;
        if (map.contains(QStringLiteral("sn")))
            snBytes = map.value(QStringLiteral("sn")).toString().toUtf8();
        else if (map.contains(QStringLiteral("value")))
            snBytes = map.value(QStringLiteral("value")).toString().toUtf8();
        else if (map.contains(QStringLiteral("string")))
            snBytes = map.value(QStringLiteral("string")).toString().toUtf8();
        if (!snBytes.isEmpty()) {
            DeviceSnPayload payload;
            payload.which_sn = static_cast<FacDevInfoType>(whichFromMap());
            payload.sn = snBytes;
            return QVariant::fromValue(payload);
        }
        if (map.contains(QStringLiteral("which_sn")) || map.contains(QStringLiteral("which"))
            || map.contains(QStringLiteral("type")))
            return whichFromMap();
        return jsonMapIntValue(map, 0);
    }
    case DeviceCmd::Sleep:
    case DeviceCmd::BurningMode:
    case DeviceCmd::WifiConnect:
    case DeviceCmd::RssiRead:
        return map;
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
    case DeviceCmd::GetBattery:
    case DeviceCmd::DevReset:
    case DeviceCmd::WifiDisconnect:
    case DeviceCmd::ChargeCurrentRead:
    case DeviceCmd::TupleRead:
    case DeviceCmd::PeriphState:
    case DeviceCmd::FactoryReset:
    case DeviceCmd::ShipMode:
        return map.isEmpty() ? QVariant() : QVariant(map);
    default:
        return map;
    }
}

void DeviceCmdCatalog::paramToIniGroup(QSettings& settings, DeviceCmd cmd, const QVariant& value) {
    removeKeysWithPrefix(settings, QStringLiteral("Param"));
    removeKeysWithPrefix(settings, sendParamIniPrefix());
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    if (schema.kind == DeviceCmdParamKind::JsonMap) {
        if (value.canConvert<QVariantMap>()) {
            writeJsonMap(settings, prefix, value);
        } else if (value.type() == QVariant::String) {
            settings.setValue(prefix + QStringLiteral("/value"), value.toString());
        } else {
            settings.setValue(prefix + QStringLiteral("/value"), value.toInt());
        }
        return;
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
    {DongleCmd::BleScanConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String,
     "蓝牙 MAC：留空或 $MAC = 当前工位 MAC\n示例：$MAC"},
    {DongleCmd::BleDirectConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String,
     "直连 MAC：留空或 $MAC = 当前工位 MAC\n示例：$MAC"},
    {DongleCmd::BleOtaConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String,
     "OTA 连接 MAC：留空或 $MAC\n示例：$MAC"},
    {DongleCmd::BleAppConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String,
     "App 通道 MAC：留空或 $MAC\n示例：$MAC"},
    {DongleCmd::BleMainConnect, TestCaseSendAction::Set, DeviceCmdParamKind::String,
     "主通道 MAC：留空或 $MAC\n示例：$MAC"},
    {DongleCmd::OtaDataPassthrough, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0=关 1=开"},
    {DongleCmd::MainDataPassthrough, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0=关 1=开"},
    {DongleCmd::BleLog, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0=关 1=开"},
    {DongleCmd::BleDeviceLog, TestCaseSendAction::Set, DeviceCmdParamKind::Int, "0=关 1=开"},
    {DongleCmd::Bomb, TestCaseSendAction::Set, DeviceCmdParamKind::JsonMap,
     "广播注入：deviceName,rssi,connectionInterval,command\n按 Dongle 协议填 JSON 或 name=value"},
    {DongleCmd::GetGmac, TestCaseSendAction::Get, DeviceCmdParamKind::None, "无需参数"},
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
        {QStringLiteral("BleScanConnect"), QStringLiteral("扫描连接蓝牙")},
        {QStringLiteral("BleDirectConnect"), QStringLiteral("直连蓝牙")},
        {QStringLiteral("BleOtaConnect"), QStringLiteral("OTA 蓝牙连接")},
        {QStringLiteral("BleAppConnect"), QStringLiteral("App 蓝牙连接")},
        {QStringLiteral("BleMainConnect"), QStringLiteral("主通道蓝牙连接")},
        {QStringLiteral("OtaDataPassthrough"), QStringLiteral("OTA 数据透传")},
        {QStringLiteral("MainDataPassthrough"), QStringLiteral("主通道数据透传")},
        {QStringLiteral("BleLog"), QStringLiteral("BLE 日志开关")},
        {QStringLiteral("BleDeviceLog"), QStringLiteral("BLE 设备日志开关")},
        {QStringLiteral("Bomb"), QStringLiteral("广播注入")},
        {QStringLiteral("GetGmac"), QStringLiteral("GMAC")},
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
        return cmdPickerDisplayLabel(label);
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

QString DongleCmdCatalog::paramUiHint(const QString& dongleCmdName) {
    DongleCmd cmd;
    if (!dongleCmdFromName(dongleCmdName, cmd))
        return QStringLiteral("未知 Dongle 指令");
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return QStringLiteral("该 Dongle 指令未登记");
    return schema.hint;
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

// ===================== ProductSerialCmdCatalog =====================

namespace {

struct ProductSerialCmdEntry {
    ProductSerialCmd cmd;
    TestCaseSendAction action;
    const char* hint;
};

const ProductSerialCmdEntry kProductSerialCatalog[] = {
    {ProductSerialCmd::InstrumentReset, TestCaseSendAction::Set,
     "经产品串口发复位帧，等待仪器应答 040E0405030C00"},
    {ProductSerialCmd::StartRx2402Ble1M, TestCaseSendAction::Set,
     "开始接收 2402MHz BLE 1M，等待应答 040E0405332000"},
    {ProductSerialCmd::StartRx2440Ble1M, TestCaseSendAction::Set,
     "开始接收 2440MHz BLE 1M，等待应答 040E0405332000"},
    {ProductSerialCmd::StartRx2480Ble1M, TestCaseSendAction::Set,
     "开始接收 2480MHz BLE 1M，等待应答 040E0405332000"},
    {ProductSerialCmd::StartRx2402Ble2M, TestCaseSendAction::Set,
     "开始接收 2402MHz BLE 2M，等待应答 040E0405332000"},
    {ProductSerialCmd::StartRx2440Ble2M, TestCaseSendAction::Set,
     "开始接收 2440MHz BLE 2M，等待应答 040E0405332000"},
    {ProductSerialCmd::StartRx2480Ble2M, TestCaseSendAction::Set,
     "开始接收 2480MHz BLE 2M，等待应答 040E0405332000"},
    {ProductSerialCmd::StopRxAndPer, TestCaseSendAction::Set,
     "停止接收并统计 PER；可与前序「开始接收」及并联 CMW 配置配合"},
};

const QHash<QString, ProductSerialCmd> kProductSerialNameMap = {
    {QStringLiteral("InstrumentReset"), ProductSerialCmd::InstrumentReset},
    {QStringLiteral("StartRx2402Ble1M"), ProductSerialCmd::StartRx2402Ble1M},
    {QStringLiteral("StartRx2440Ble1M"), ProductSerialCmd::StartRx2440Ble1M},
    {QStringLiteral("StartRx2480Ble1M"), ProductSerialCmd::StartRx2480Ble1M},
    {QStringLiteral("StartRx2402Ble2M"), ProductSerialCmd::StartRx2402Ble2M},
    {QStringLiteral("StartRx2440Ble2M"), ProductSerialCmd::StartRx2440Ble2M},
    {QStringLiteral("StartRx2480Ble2M"), ProductSerialCmd::StartRx2480Ble2M},
    {QStringLiteral("StopRxAndPer"), ProductSerialCmd::StopRxAndPer},
};

const QHash<QString, QString>& productSerialCmdUiLabelMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("InstrumentReset"), QStringLiteral("仪器复位应答")},
        {QStringLiteral("StartRx2402Ble1M"), QStringLiteral("开始接收 2402 BLE1M")},
        {QStringLiteral("StartRx2440Ble1M"), QStringLiteral("开始接收 2440 BLE1M")},
        {QStringLiteral("StartRx2480Ble1M"), QStringLiteral("开始接收 2480 BLE1M")},
        {QStringLiteral("StartRx2402Ble2M"), QStringLiteral("开始接收 2402 BLE2M")},
        {QStringLiteral("StartRx2440Ble2M"), QStringLiteral("开始接收 2440 BLE2M")},
        {QStringLiteral("StartRx2480Ble2M"), QStringLiteral("开始接收 2480 BLE2M")},
        {QStringLiteral("StopRxAndPer"), QStringLiteral("停止接收与 PER")},
    };
    return map;
}

}  // namespace

QStringList ProductSerialCmdCatalog::allProductSerialCmdNames() {
    QStringList names = kProductSerialNameMap.keys();
    names.sort();
    return names;
}

TestCaseSendAction ProductSerialCmdCatalog::actionFor(ProductSerialCmd cmd) {
    for (const ProductSerialCmdEntry& entry : kProductSerialCatalog) {
        if (entry.cmd == cmd)
            return entry.action;
    }
    return TestCaseSendAction::Set;
}

bool ProductSerialCmdCatalog::isCmdForAction(ProductSerialCmd cmd, TestCaseSendAction action) {
    return actionFor(cmd) == action;
}

QString ProductSerialCmdCatalog::productSerialCmdUiLabel(const QString& enumName) {
    const QString key = enumName.trimmed();
    const QString label = productSerialCmdUiLabelMap().value(key);
    if (!label.isEmpty())
        return cmdPickerDisplayLabel(label);
    return QStringLiteral("未登记串口指令");
}

bool ProductSerialCmdCatalog::productSerialCmdFromName(const QString& name, ProductSerialCmd& out) {
    const auto it = kProductSerialNameMap.constFind(name.trimmed());
    if (it == kProductSerialNameMap.cend())
        return false;
    out = it.value();
    return true;
}

QString ProductSerialCmdCatalog::productSerialCmdToName(ProductSerialCmd cmd) {
    for (auto it = kProductSerialNameMap.cbegin(); it != kProductSerialNameMap.cend(); ++it) {
        if (it.value() == cmd)
            return it.key();
    }
    return QString();
}

bool ProductSerialCmdCatalog::paramSchemaFor(ProductSerialCmd cmd, DeviceCmdParamSchema& out) {
    for (const ProductSerialCmdEntry& entry : kProductSerialCatalog) {
        if (entry.cmd == cmd) {
            out.kind = DeviceCmdParamKind::None;
            out.hint = QString::fromUtf8(entry.hint);
            return true;
        }
    }
    return false;
}

QString ProductSerialCmdCatalog::paramUiHint(const QString& enumName) {
    ProductSerialCmd cmd;
    if (!productSerialCmdFromName(enumName, cmd))
        return QString();
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return QString();
    return schema.hint;
}

int ProductSerialCmdCatalog::brushProfileForCmd(ProductSerialCmd cmd) {
    switch (cmd) {
        case ProductSerialCmd::StartRx2402Ble1M:
            return 0;
        case ProductSerialCmd::StartRx2440Ble1M:
            return 1;
        case ProductSerialCmd::StartRx2480Ble1M:
            return 2;
        case ProductSerialCmd::StartRx2402Ble2M:
            return 3;
        case ProductSerialCmd::StartRx2440Ble2M:
            return 4;
        case ProductSerialCmd::StartRx2480Ble2M:
            return 5;
        default:
            return -1;
    }
}

// ===================== TupleCmdCatalog =====================

namespace {

struct TupleCmdEntry {
    TupleCmd cmd;
    TestCaseSendAction action;
    DeviceCmdParamKind kind;
    const char* hint;
};

const TupleCmdEntry kTupleCatalog[] = {
    {TupleCmd::Login, TestCaseSendAction::Set, DeviceCmdParamKind::JsonMap,
     "三元组云端登录（环境）：每环境单独 ini，如 三元组云端登录（prod）\n"
     "Param/baseUrl、Param/userName、Param/password；须在获取三元组之前执行"},
    {TupleCmd::ApplyTupleByMac, TestCaseSendAction::Get, DeviceCmdParamKind::JsonMap,
     "按 MAC 拉三元组：Param/mac 留空或 $MAC；Param/sku、Param/position 在用例 ini 配置\n"
     "示例：mac=$MAC，sku=PH9，position=L"},
    {TupleCmd::DebugUpdateMacStatus, TestCaseSendAction::Set, DeviceCmdParamKind::JsonMap,
     "调试：mac、status\n示例：mac=AA1122334455\nstatus=2"},
    {TupleCmd::ReportWriteRecord, TestCaseSendAction::Set, DeviceCmdParamKind::None,
     "无需参数；使用内存中已获取的三元组与测试结果"},
};

const QHash<QString, TupleCmd> kTupleNameMap = {
    {QStringLiteral("Login"), TupleCmd::Login},
    {QStringLiteral("ApplyTupleByMac"), TupleCmd::ApplyTupleByMac},
    {QStringLiteral("DebugUpdateMacStatus"), TupleCmd::DebugUpdateMacStatus},
    {QStringLiteral("ReportWriteRecord"), TupleCmd::ReportWriteRecord},
};

const QHash<QString, QString>& tupleCmdUiLabelMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("Login"), QStringLiteral("三元组云端登录")},
        {QStringLiteral("ApplyTupleByMac"), QStringLiteral("云端三元组")},
        {QStringLiteral("DebugUpdateMacStatus"), QStringLiteral("调试 MAC 状态")},
        {QStringLiteral("ReportWriteRecord"), QStringLiteral("三元组写入记录")},
    };
    return map;
}

}  // namespace

QStringList TupleCmdCatalog::allTupleCmdNames() {
    QStringList names = kTupleNameMap.keys();
    names.sort();
    return names;
}

QStringList TupleCmdCatalog::allTupleCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (const TupleCmdEntry& entry : kTupleCatalog) {
        if (entry.action == action)
            names.append(tupleCmdToName(entry.cmd));
    }
    names.sort();
    return names;
}

TestCaseSendAction TupleCmdCatalog::actionFor(TupleCmd cmd) {
    for (const TupleCmdEntry& entry : kTupleCatalog) {
        if (entry.cmd == cmd)
            return entry.action;
    }
    return TestCaseSendAction::Set;
}

bool TupleCmdCatalog::isCmdForAction(TupleCmd cmd, TestCaseSendAction action) {
    return actionFor(cmd) == action;
}

QString TupleCmdCatalog::tupleCmdUiLabel(const QString& enumName) {
    const QString key = enumName.trimmed();
    const QString label = tupleCmdUiLabelMap().value(key);
    if (!label.isEmpty())
        return cmdPickerDisplayLabel(label);
    return QStringLiteral("未登记云端指令");
}

bool TupleCmdCatalog::tupleCmdFromName(const QString& name, TupleCmd& out) {
    const auto it = kTupleNameMap.constFind(name.trimmed());
    if (it == kTupleNameMap.cend())
        return false;
    out = it.value();
    return true;
}

QString TupleCmdCatalog::tupleCmdToName(TupleCmd cmd) {
    return QTupleService::tupleCmdToName(cmd);
}

bool TupleCmdCatalog::paramSchemaFor(TupleCmd cmd, DeviceCmdParamSchema& out) {
    for (const auto& e : kTupleCatalog) {
        if (e.cmd == cmd) {
            out.kind = e.kind;
            out.hint = e.hint ? QString::fromUtf8(e.hint) : QString();
            return true;
        }
    }
    return false;
}

QString TupleCmdCatalog::paramUiHint(const QString& tupleCmdName) {
    TupleCmd cmd;
    if (!tupleCmdFromName(tupleCmdName, cmd))
        return QStringLiteral("未知云端指令");
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return QStringLiteral("该云端指令未登记");
    return schema.hint;
}

bool TupleCmdCatalog::paramFromIniGroup(const QSettings& settings, TupleCmd cmd, QVariant& out) {
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

void TupleCmdCatalog::paramToIniGroup(QSettings& settings, TupleCmd cmd, const QVariant& value) {
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
        QString expected = gate.expected.trimmed();
        if (expected.isEmpty() && !gate.expectedSettingsKey.isEmpty())
            expected = SETTINGS.value(gate.expectedSettingsKey).toString().trimmed();
        // 未在 case ini 配置期望时跳过比对（不再回退 上位机设置.ini 的 ProductInfo/Software_Version）
        if (expected.isEmpty()) {
            passOut = true;
            detailOut = QStringLiteral("当前=%1, case 未配置 Gate/Expected").arg(actual);
            return true;
        }
        if (reportType == QLatin1String("ProtocolBaseInfoData") && gate.field == QLatin1String("soft_version")
            && gate.expected.isEmpty() && !gate.expectedSettingsKey.isEmpty()
            && !SETTINGS.value(QStringLiteral("ProductInfo/SoftwareVersion_checkBox"), true).toBool()) {
            passOut = true;
            detailOut = QStringLiteral("当前=%1, 未启用软件版本校验").arg(actual);
            return true;
        }
        passOut = CommonUtils::compareVersions(expected, actual);
        detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
        return true;
    }

    if (gate.op == TestCaseGateOp::Eq) {
        bool strOk = false;
        const QString actual = fieldStringFromVariant(reportType, gate.field, payload, strOk);
        if (strOk) {
            QString expected = gate.expected.trimmed();
            if (expected.isEmpty() && !gate.expectedSettingsKey.isEmpty())
                expected = SETTINGS.value(gate.expectedSettingsKey).toString().trimmed();
            if (expected.isEmpty()) {
                passOut = false;
                detailOut = QStringLiteral("当前=%1, 未配置期望( Gate/Expected 或 MES/UI SN)").arg(
                    actual.isEmpty() ? QStringLiteral("-") : actual);
            } else {
                passOut = (actual == expected);
                detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
            }
            return true;
        }
    }

    if (!ok) {
        passOut = false;
        detailOut = QStringLiteral("无法从上报数据读取字段");
        return true;
    }

    double low = gate.low;
    double high = gate.high;
    resolveRangeBounds(gate, low, high);

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

void GateRegistry::resolveRangeBounds(const TestCaseGate& gate, double& lowOut, double& highOut) {
    lowOut = gate.low;
    highOut = gate.high;
    if (!gate.lowSettingsKey.isEmpty())
        lowOut = SETTINGS.value(gate.lowSettingsKey, lowOut).toDouble();
    if (!gate.highSettingsKey.isEmpty())
        highOut = SETTINGS.value(gate.highSettingsKey, highOut).toDouble();
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
    if (def.send.channel == TestCaseSendChannel::ProductSerial)
        return true;
    if (isDongleBleConnectStep(def))
        return true;
    if (def.gate.enabled)
        return true;
    if (def.send.action == TestCaseSendAction::Get)
        return true;
    return false;
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
    if (def.send.channel == TestCaseSendChannel::ProductSerial)
        return 30000;
    if (isDongleBleConnectStep(def))
        return 6000;
    return def.gate.enabled ? 8000 : 3000;
}
