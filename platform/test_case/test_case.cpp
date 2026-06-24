#include "test_case.h"

#include "cmd_manifest_common.h"
#include "device_cmd_manifest.h"
#include "dongle_cmd_manifest.h"
#include "fixture_pcba_cmd_manifest.h"
#include "product_serial_cmd_manifest.h"
#include "modbus_cmd_manifest.h"
#include "scpi_cmd_manifest.h"
#include "tuple_cmd_manifest.h"

#include <QCoreApplication>
#include <QDateTime>
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
#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
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

} // namespace TestCasePaths

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
    const bool hadData = ini.contains(QStringLiteral("Items")) || ini.contains(QStringLiteral("StopFlowOnTestFail")) || ini.contains(QStringLiteral("StopOnGateFail"));
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

} // namespace

QVector<TestFlowStationEntry> TestCaseStore::defaultFlowStationPresets() {
    return builtinFlowStationPresets();
}

QVector<TestFlowStationEntry> TestCaseStore::loadFlowStationCatalog() {
    TestCasePaths::ensureRootDir();
    QHash<QString, QString> nameByKey;

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

    // 仅有 [Station/xxx] 流程段、未写入 FlowStations 时补登记（不自动补回已删除的内置工站）
    for (const QString& flowKey : TestCaseStore::listStationKeysFromFlow()) {
        const QString k = flowKey.trimmed();
        if (k.isEmpty() || nameByKey.contains(k))
            continue;
        const QString presetName = lookupPresetDisplayName(k);
        nameByKey.insert(k, presetName.isEmpty() ? k : presetName);
    }

    if (catalogKeys.isEmpty() && nameByKey.isEmpty()) {
        for (const TestFlowStationEntry& preset : builtinFlowStationPresets())
            nameByKey.insert(preset.key, preset.displayName);
    }

    if (catalogKeys.isEmpty() && !nameByKey.isEmpty()) {
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

bool TestCaseStore::copyFlowStation(const QString& sourceStationKey, const QString& newDisplayName,
                                    const QVector<TestFlowItemEntry>& items, bool stopFlowOnTestFail,
                                    QString* outNewKey, QString* errorOut) {
    const QString src = resolveFlowStationKey(sourceStationKey.trimmed());
    if (src.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("源工站无效");
        return false;
    }

    bool sourceListed = false;
    for (const TestFlowStationEntry& entry : loadFlowStationCatalog()) {
        if (entry.key.compare(src, Qt::CaseInsensitive) == 0) {
            sourceListed = true;
            break;
        }
    }
    if (!sourceListed) {
        if (errorOut)
            *errorOut = QStringLiteral("源工站不在目录中：%1").arg(src);
        return false;
    }

    if (!addFlowStation(newDisplayName, errorOut))
        return false;

    const QString newKey = resolveFlowStationKey(newDisplayName.trimmed());
    if (newKey.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("新工站创建后无法解析键");
        return false;
    }
    if (!saveStationFlowItems(newKey, items, stopFlowOnTestFail)) {
        if (errorOut)
            *errorOut = QStringLiteral("无法写入新工站流程");
        removeFlowStation(newKey, nullptr);
        return false;
    }
    if (outNewKey)
        *outNewKey = newKey;
    return true;
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
    ini.remove(QString());
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

namespace {

void loadMultiGatesFromIni(QSettings& ini, TestCaseDefinition& out) {
    out.gates.clear();
    const int count = ini.value(QStringLiteral("Gate/Count"), 0).toInt();
    const QString reportType = out.gate.reportType;
    if (count > 0) {
        for (int i = 1; i <= count; ++i) {
            ini.beginGroup(QStringLiteral("Gate/%1").arg(i));
            TestCaseGate g;
            g.enabled = ini.value(QStringLiteral("Enabled"), true).toBool();
            g.reportType = reportType;
            g.field = ini.value(QStringLiteral("Field")).toString().trimmed();
            g.op = gateOpFromString(ini.value(QStringLiteral("Op"), QStringLiteral("eq")).toString());
            g.expected = ini.value(QStringLiteral("Expected")).toString().trimmed();
            g.low = ini.value(QStringLiteral("Low"), g.expected.toDouble()).toDouble();
            g.high = ini.value(QStringLiteral("High"), g.low).toDouble();
            ini.endGroup();
            if (!g.field.isEmpty())
                out.gates.append(g);
        }
        return;
    }
    if (count <= 0 && out.gate.field == QLatin1String("multi")) {
        for (int i = 1; i <= 32; ++i) {
            ini.beginGroup(QStringLiteral("Gate/%1").arg(i));
            const QString field = ini.value(QStringLiteral("Field")).toString().trimmed();
            if (field.isEmpty()) {
                ini.endGroup();
                break;
            }
            TestCaseGate g;
            g.enabled = ini.value(QStringLiteral("Enabled"), true).toBool();
            g.reportType = reportType;
            g.field = field;
            g.op = gateOpFromString(ini.value(QStringLiteral("Op"), QStringLiteral("eq")).toString());
            g.expected = ini.value(QStringLiteral("Expected")).toString().trimmed();
            g.low = ini.value(QStringLiteral("Low"), g.expected.toDouble()).toDouble();
            g.high = ini.value(QStringLiteral("High"), g.low).toDouble();
            ini.endGroup();
            out.gates.append(g);
        }
        if (!out.gates.isEmpty())
            return;
    }
    if (out.gate.enabled && GateRegistry::isAllFieldsGateField(out.gate.field) && reportType == QStringLiteral("ProtocolPeriphStateData")) {
        for (const QString& f : GateRegistry::fieldsFor(reportType)) {
            TestCaseGate g = out.gate;
            g.field = f;
            g.op = TestCaseGateOp::Eq;
            g.expected = QString::number(static_cast<int>(out.gate.low));
            out.gates.append(g);
        }
        return;
    }
    if (out.gate.enabled)
        out.gates.append(out.gate);
}

void saveMultiGatesToIni(QSettings& ini, const TestCaseDefinition& def) {
    for (int i = 1; i <= 32; ++i)
        ini.remove(QStringLiteral("Gate/%1").arg(i));
    ini.remove(QStringLiteral("Gate/Count"));
    if (def.gates.size() <= 1)
        return;
    ini.setValue(QStringLiteral("Gate/Count"), def.gates.size());
    ini.setValue(QStringLiteral("Gate/Field"), QStringLiteral("multi"));
    for (int i = 0; i < def.gates.size(); ++i) {
        ini.beginGroup(QStringLiteral("Gate/%1").arg(i + 1));
        const TestCaseGate& g = def.gates.at(i);
        ini.setValue(QStringLiteral("Field"), g.field);
        ini.setValue(QStringLiteral("Enabled"), g.enabled);
        ini.setValue(QStringLiteral("Op"), gateOpToString(g.op));
        ini.setValue(QStringLiteral("Expected"), g.expected);
        ini.setValue(QStringLiteral("Low"), g.low);
        ini.setValue(QStringLiteral("High"), g.high);
        ini.endGroup();
    }
}

} // namespace

namespace {
QVariant readSendScopedParam(const QSettings& settings, const QString& leafKey, const QVariant& defaultValue);
} // namespace

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
    out.send.device = ini.value(QStringLiteral("Send/Device")).toString().trimmed();
    const QString protocolIni = ini.value(QStringLiteral("Send/Protocol")).toString();
    out.send.productProtocol = DeviceCmdCatalog::productProtocolFromIni(protocolIni);
    out.send.fixtureProtocol = FixturePcbaCmdCatalog::fixtureProtocolFromIni(protocolIni);
    if (out.send.productProtocol == TestCaseProductProtocol::Qfctp && out.send.deviceCmd.compare(QStringLiteral("BaseInfo"), Qt::CaseInsensitive) == 0) {
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
    } else if (channelIni.compare(QStringLiteral("Fixture"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Fixture;
    } else if (channelIni.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Modbus;
    } else if (channelIni.compare(QStringLiteral("Scpi"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Scpi;
    } else {
        FixturePcbaCmd inferFixturePcba;
        ProductSerialCmd inferSerial;
        TupleCmd inferTuple;
        DongleCmd inferDongle;
        if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(out.send.deviceCmd, inferFixturePcba)) {
            out.send.channel = TestCaseSendChannel::Fixture;
        } else if (ProductSerialCmdCatalog::productSerialCmdFromName(out.send.deviceCmd, inferSerial)) {
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
    } else if (out.send.channel == TestCaseSendChannel::Fixture) {
        FixturePcbaCmd fixtureCmd;
        if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(out.send.deviceCmd, fixtureCmd)) {
            if (!FixturePcbaCmdCatalog::isCmdForAction(fixtureCmd, out.send.action))
                out.send.action = FixturePcbaCmdCatalog::actionFor(fixtureCmd);
            FixturePcbaCmdCatalog::paramFromIniGroup(ini, fixtureCmd, out.send.param);
        }
    } else if (out.send.channel == TestCaseSendChannel::Modbus || out.send.channel == TestCaseSendChannel::Scpi) {
        QVariant val = ini.value(QStringLiteral("Send/Param"));
        if (!val.isValid()) {
            val = readSendScopedParam(ini, QStringLiteral("value"), QVariant());
        }
        if (!val.isValid()) {
            val = readSendScopedParam(ini, QStringLiteral("int"), QVariant());
        }
        if (!val.isValid()) {
            val = readSendScopedParam(ini, QStringLiteral("string"), QVariant());
        }
        out.send.param = val;
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

    // ProtocolUInt32ValueData 已按业务拆分，兼容旧 ini
    if (out.gate.reportType == QStringLiteral("ProtocolUInt32ValueData")) {
        DeviceCmd legacyCmd;
        if (DeviceCmdCatalog::deviceCmdFromName(out.send.deviceCmd, legacyCmd)) {
            if (legacyCmd == DeviceCmd::ChargeCurrentRead) {
                out.gate.reportType = QStringLiteral("ProtocolChargeCurrentData");
                if (out.gate.field == QStringLiteral("value"))
                    out.gate.field = QStringLiteral("currentMa");
            } else if (legacyCmd == DeviceCmd::KeySignalRead) {
                out.gate.reportType = QStringLiteral("ProtocolKeyCapData");
                if (out.gate.field == QStringLiteral("value"))
                    out.gate.field = QStringLiteral("capacitance");
                else if (out.gate.field == QStringLiteral("auxId"))
                    out.gate.field = QStringLiteral("keyId");
            }
        }
    }

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
            if (hid == QStringLiteral("PROD_INST_RESET_ACK") || hid.startsWith(QStringLiteral("PROD_INST_RESET_ACK_"))) {
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
            } else if (hid == QStringLiteral("PROD_INST_STOP_RX_PER") || hid.startsWith(QStringLiteral("PROD_INST_STOP_RX_PER_"))) {
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
    if (out.gate.enabled && out.gate.reportType == QStringLiteral("ProtocolBaseInfoData") && out.gate.field == QStringLiteral("soft_version") && out.gate.op == TestCaseGateOp::Range) {
        out.gate.op = TestCaseGateOp::CompareVersions;
    }
    // 旧配置从全局设置读版本：清空，改由 case ini Gate/Expected 配置
    if (out.gate.enabled && out.gate.reportType == QStringLiteral("ProtocolBaseInfoData") && out.gate.field == QStringLiteral("soft_version") && out.gate.expectedSettingsKey == QStringLiteral("ProductInfo/Software_Version") && out.gate.expected.trimmed().isEmpty()) {
        out.gate.expectedSettingsKey.clear();
    }

    loadMultiGatesFromIni(ini, out);

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

    ini.setValue(QStringLiteral("Send/Action"), def.send.action == TestCaseSendAction::Get ? QStringLiteral("Get") : QStringLiteral("Set"));
    QString channelStr = QStringLiteral("Product");
    if (def.send.channel == TestCaseSendChannel::Dongle)
        channelStr = QStringLiteral("Dongle");
    else if (def.send.channel == TestCaseSendChannel::Cloud)
        channelStr = QStringLiteral("Cloud");
    else if (def.send.channel == TestCaseSendChannel::ProductSerial)
        channelStr = QStringLiteral("ProductSerial");
    else if (def.send.channel == TestCaseSendChannel::Fixture)
        channelStr = QStringLiteral("Fixture");
    else if (def.send.channel == TestCaseSendChannel::Modbus)
        channelStr = QStringLiteral("Modbus");
    else if (def.send.channel == TestCaseSendChannel::Scpi)
        channelStr = QStringLiteral("Scpi");
    ini.setValue(QStringLiteral("Send/Channel"), channelStr);
    if (def.send.channel == TestCaseSendChannel::Product)
        ini.setValue(QStringLiteral("Send/Protocol"),
                     DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol));
    else if (def.send.channel == TestCaseSendChannel::Fixture)
        ini.setValue(QStringLiteral("Send/Protocol"),
                     FixturePcbaCmdCatalog::fixtureProtocolToIni(def.send.fixtureProtocol));
    ini.setValue(QStringLiteral("Send/DeviceCmd"), def.send.deviceCmd);
    if (def.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd))
            DongleCmdCatalog::paramToIniGroup(ini, dongleCmd, def.send.param);
    } else if (def.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd))
            TupleCmdCatalog::paramToIniGroup(ini, tupleCmd, def.send.param);
    } else if (def.send.channel == TestCaseSendChannel::Fixture) {
        FixturePcbaCmd fixtureCmd;
        if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, fixtureCmd))
            FixturePcbaCmdCatalog::paramToIniGroup(ini, fixtureCmd, def.send.param);
    } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        if (!def.send.device.isEmpty())
            ini.setValue(QStringLiteral("Send/Device"), def.send.device);
        if (def.send.param.isValid())
            ini.setValue(QStringLiteral("Send/Param"), def.send.param);
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
    saveMultiGatesToIni(ini, def);

    ini.setValue(QStringLiteral("Hook/Enabled"), def.hook.enabled);
    ini.setValue(QStringLiteral("Hook/HookId"), def.hook.hookId);
    syncTestCaseIni(ini, casePath);
    return true;
}

QVector<TestCaseGate> TestCaseStore::effectiveGates(const TestCaseDefinition& def) {
    if (!def.gates.isEmpty())
        return def.gates;
    if (def.gate.enabled)
        return {def.gate};
    return {};
}

QVector<TestCaseGate> TestCaseStore::activeGatesForEvaluation(const TestCaseDefinition& def) {
    if (!def.gate.enabled)
        return {};
    const QVector<TestCaseGate> gates = effectiveGates(def);
    if (gates.isEmpty())
        return {};

    QVector<TestCaseGate> active;
    active.reserve(gates.size());
    for (const TestCaseGate& g : gates) {
        if (g.enabled)
            active.append(g);
    }
    return active;
}

bool TestCaseStore::usesMultiFieldGates(const TestCaseDefinition& def) {
    return def.gates.size() > 1;
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
    for (const TestFlowItemEntry& entry : loadStationFlowItems(stationKey)) {
        if (entry.enabled)
            items.append(entry.caseName);
    }
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
        result = true; // 兼容旧 per-case 列表
    ini.endGroup();
    return result;
}

QVector<TestFlowItemEntry> TestCaseStore::loadStationFlowItems(const QString& stationKey) {
    TestCasePaths::ensureRootDir();
    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(stationKey));
    const QString rawItems = ini.value(QStringLiteral("Items")).toString();
    const QString rawDisabled = ini.value(QStringLiteral("DisabledItems")).toString();
    const QString rawEnabled = ini.value(QStringLiteral("ItemEnabled")).toString();
    ini.endGroup();

    QSet<QString> disabledNames;
    for (const QString& part : rawDisabled.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (!name.isEmpty())
            disabledNames.insert(name);
    }

    const QStringList enabledParts = rawEnabled.split(QLatin1Char(','), Qt::SkipEmptyParts);
    const bool useLegacyItemEnabled = disabledNames.isEmpty() && !rawEnabled.trimmed().isEmpty();

    QVector<TestFlowItemEntry> entries;
    int index = 0;
    for (const QString& part : rawItems.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (name.isEmpty())
            continue;
        TestFlowItemEntry entry;
        entry.caseName = name;
        entry.enabled = true;
        if (disabledNames.contains(name)) {
            entry.enabled = false;
        } else if (useLegacyItemEnabled && index < enabledParts.size()) {
            const QString flag = enabledParts.at(index).trimmed();
            entry.enabled = !(flag == QLatin1String("0") || flag.compare(QLatin1String("false"), Qt::CaseInsensitive) == 0);
        }
        entries.append(entry);
        ++index;
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
    QStringList disabledNames;
    for (const TestFlowItemEntry& entry : items) {
        const QString name = entry.caseName.trimmed();
        if (name.isEmpty())
            continue;
        names.append(name);
        if (!entry.enabled)
            disabledNames.append(name);
    }
    ini.setValue(QStringLiteral("Items"), names.join(QLatin1Char(',')));
    if (disabledNames.isEmpty()) {
        ini.remove(QStringLiteral("DisabledItems"));
    } else {
        ini.setValue(QStringLiteral("DisabledItems"), disabledNames.join(QLatin1Char(',')));
    }
    ini.remove(QStringLiteral("ItemEnabled"));
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
    } else if (def.send.channel == TestCaseSendChannel::Fixture) {
        if (def.send.fixtureProtocol != TestCaseFixtureProtocol::Pcba) {
            errors.append(QStringLiteral("治具协议类型无效"));
        }
        FixturePcbaCmd fixtureCmd;
        if (!FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, fixtureCmd)) {
            errors.append(QStringLiteral("治具 PCBA 测试指令无效"));
        } else if (!FixturePcbaCmdCatalog::isCmdForAction(fixtureCmd, def.send.action)) {
            errors.append(QStringLiteral("治具指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!FixturePcbaCmdCatalog::paramSchemaFor(fixtureCmd, schema))
                errors.append(QStringLiteral("该治具指令尚未配置参数模板，请联系工程师"));
        }
    } else {
        DeviceCmd cmd;
        if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
            errors.append(QStringLiteral("产品测试指令无效"));
        } else if (!DeviceCmdCatalog::isCmdForAction(cmd, def.send.action)) {
            errors.append(QStringLiteral("产品指令与操作方式不匹配"));
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
        if (!GateRegistry::descriptorFor(def.gate.reportType, desc)) {
            errors.append(QStringLiteral("回传数据类型未登记，请联系工程师"));
        } else if (TestCaseStore::usesMultiFieldGates(def)) {
            const QStringList fields = GateRegistry::fieldsFor(def.gate.reportType);
            for (const TestCaseGate& g : def.gates) {
                if (!fields.contains(g.field))
                    errors.append(QStringLiteral("分项判定字段未登记：%1").arg(g.field));
                if (g.op == TestCaseGateOp::Eq && g.expected.trimmed().isEmpty())
                    errors.append(QStringLiteral("分项「%1」须填写期望值")
                                      .arg(GateRegistry::fieldDisplayName(def.gate.reportType, g.field)));
            }
        } else if (!GateRegistry::isAllFieldsGateField(def.gate.field) && !GateRegistry::fieldsFor(def.gate.reportType).contains(def.gate.field)) {
            errors.append(QStringLiteral("判定项目未登记，请联系工程师"));
        }
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

/** 设置页「指令内容」下拉：去掉与「操作方式」重复的动作前缀。 */
QString cmdPickerDisplayLabel(QString label) {
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

} // namespace

QStringList DeviceCmdCatalog::allDeviceCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < DeviceCmdManifest::rowCount(); ++i) {
        const DeviceCmdManifest::Row& row = DeviceCmdManifest::rows()[i];
        if (!isCmdForAction(row.cmd, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseProductProtocol DeviceCmdCatalog::productProtocolFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("Qpb"), Qt::CaseInsensitive) == 0 || t.compare(QStringLiteral("PB"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qpb;
    if (t.compare(QStringLiteral("Qroot"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qroot;
    return TestCaseProductProtocol::Qfctp;
}

QString DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol protocol) {
    switch (protocol) {
    case TestCaseProductProtocol::Qpb:
        return QStringLiteral("Qpb");
    case TestCaseProductProtocol::Qroot:
        return QStringLiteral("Qroot");
    default:
        return QStringLiteral("Qfctp");
    }
}

QString DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol protocol) {
    switch (protocol) {
    case TestCaseProductProtocol::Qpb:
        return QStringLiteral("QPB");
    case TestCaseProductProtocol::Qroot:
        return QStringLiteral("Qroot");
    default:
        return QStringLiteral("FCTP");
    }
}

TestCaseSendAction DeviceCmdCatalog::actionFor(DeviceCmd cmd) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool DeviceCmdCatalog::isCmdForAction(DeviceCmd cmd, TestCaseSendAction action) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString DeviceCmdCatalog::deviceCmdUiLabel(const QString& enumName) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记指令");
}

bool DeviceCmdCatalog::deviceCmdFromName(const QString& name, DeviceCmd& out) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString DeviceCmdCatalog::deviceCmdToName(DeviceCmd cmd) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool DeviceCmdCatalog::paramSchemaFor(DeviceCmd cmd, DeviceCmdParamSchema& out) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString DeviceCmdCatalog::paramUiHint(const QString& deviceCmdName) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(deviceCmdName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    DeviceCmd cmd;
    if (!deviceCmdFromName(deviceCmdName, cmd))
        return QStringLiteral("未知产品指令");
    return QStringLiteral("按协议填写 JSON 或 name=value 行");
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
        if (map.contains(QStringLiteral("which_sn")) || map.contains(QStringLiteral("which")) || map.contains(QStringLiteral("type")))
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

// ===================== DongleCmdCatalog =====================

QStringList DongleCmdCatalog::allDongleCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < DongleCmdManifest::rowCount(); ++i) {
        const DongleCmdManifest::Row& row = DongleCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction DongleCmdCatalog::actionFor(DongleCmd cmd) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool DongleCmdCatalog::isCmdForAction(DongleCmd cmd, TestCaseSendAction action) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString DongleCmdCatalog::dongleCmdUiLabel(const QString& enumName) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记 Dongle 指令");
}

bool DongleCmdCatalog::dongleCmdFromName(const QString& name, DongleCmd& out) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString DongleCmdCatalog::dongleCmdToName(DongleCmd cmd) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool DongleCmdCatalog::paramSchemaFor(DongleCmd cmd, DeviceCmdParamSchema& out) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString DongleCmdCatalog::paramUiHint(const QString& dongleCmdName) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(dongleCmdName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    DongleCmd cmd;
    if (!dongleCmdFromName(dongleCmdName, cmd))
        return QStringLiteral("未知 Dongle 指令");
    return QStringLiteral("该 Dongle 指令未登记");
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

// ===================== FixturePcbaCmdCatalog =====================

QStringList FixturePcbaCmdCatalog::allFixturePcbaCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < FixturePcbaCmdManifest::rowCount(); ++i) {
        const FixturePcbaCmdManifest::Row& row = FixturePcbaCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseFixtureProtocol FixturePcbaCmdCatalog::fixtureProtocolFromIni(const QString& text) {
    if (text.compare(QStringLiteral("Pcba"), Qt::CaseInsensitive) == 0 || text.compare(QStringLiteral("PCBA"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Pcba;
    return TestCaseFixtureProtocol::Pcba;
}

QString FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol protocol) {
    Q_UNUSED(protocol);
    return QStringLiteral("Pcba");
}

QString FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol protocol) {
    Q_UNUSED(protocol);
    return QStringLiteral("PCBA测试协议");
}

TestCaseSendAction FixturePcbaCmdCatalog::actionFor(FixturePcbaCmd cmd) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool FixturePcbaCmdCatalog::isCmdForAction(FixturePcbaCmd cmd, TestCaseSendAction action) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(const QString& enumName) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记治具指令");
}

bool FixturePcbaCmdCatalog::fixturePcbaCmdFromName(const QString& name, FixturePcbaCmd& out) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString FixturePcbaCmdCatalog::fixturePcbaCmdToName(FixturePcbaCmd cmd) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool FixturePcbaCmdCatalog::paramSchemaFor(FixturePcbaCmd cmd, DeviceCmdParamSchema& out) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString FixturePcbaCmdCatalog::paramUiHint(const QString& enumName) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
}

bool FixturePcbaCmdCatalog::paramFromIniGroup(const QSettings& settings, FixturePcbaCmd cmd, QVariant& out) {
    switch (cmd) {
    case FixturePcbaCmd::StartTest:
    case FixturePcbaCmd::StartSleep:
    case FixturePcbaCmd::StartWhiteMode: {
        QVariant machine = readSendScopedParam(settings, QStringLiteral("MachineIndex"), QVariant());
        if (!machine.isValid())
            machine = settings.value(QStringLiteral("Send/Param/MachineIndex"));
        if (!machine.isValid()) {
            const QString legacyKey = QStringLiteral("SendParam/MachineIndex");
            if (settings.contains(legacyKey))
                machine = settings.value(legacyKey);
        }
        if (!machine.isValid()) {
            out = QStringLiteral("$INDEX");
            break;
        }
        if (machine.userType() == QMetaType::QString) {
            const QString s = machine.toString().trimmed();
            if (s.compare(QStringLiteral("$INDEX"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$SLOT"), Qt::CaseInsensitive) == 0 || s.isEmpty()) {
                out = QStringLiteral("$INDEX");
                break;
            }
        }
        bool ok = false;
        int idx = machine.toInt(&ok);
        if (!ok || idx == 0)
            out = QStringLiteral("$INDEX");
        else
            out = qBound(1, idx, 15);
        break;
    }
    default:
        out = QVariant();
        break;
    }
    return true;
}

void FixturePcbaCmdCatalog::paramToIniGroup(QSettings& settings, FixturePcbaCmd cmd, const QVariant& value) {
    removeKeysWithPrefix(settings, QStringLiteral("SendParam"));
    removeKeysWithPrefix(settings, sendParamIniPrefix());
    const QString prefix = sendParamIniPrefix();
    switch (cmd) {
    case FixturePcbaCmd::StartTest:
    case FixturePcbaCmd::StartSleep:
    case FixturePcbaCmd::StartWhiteMode:
        if (value.userType() == QMetaType::QString)
            settings.setValue(prefix + QStringLiteral("/MachineIndex"), value.toString().trimmed());
        else
            settings.setValue(prefix + QStringLiteral("/MachineIndex"), value.toInt());
        break;
    default:
        break;
    }
}

// ===================== ProductSerialCmdCatalog =====================

QStringList ProductSerialCmdCatalog::allProductSerialCmdNames() {
    QStringList names;
    for (int i = 0; i < ProductSerialCmdManifest::rowCount(); ++i)
        names.append(QString::fromLatin1(ProductSerialCmdManifest::rows()[i].enumName));
    names.sort();
    return names;
}

TestCaseSendAction ProductSerialCmdCatalog::actionFor(ProductSerialCmd cmd) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool ProductSerialCmdCatalog::isCmdForAction(ProductSerialCmd cmd, TestCaseSendAction action) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString ProductSerialCmdCatalog::productSerialCmdUiLabel(const QString& enumName) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记串口指令");
}

bool ProductSerialCmdCatalog::productSerialCmdFromName(const QString& name, ProductSerialCmd& out) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString ProductSerialCmdCatalog::productSerialCmdToName(ProductSerialCmd cmd) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool ProductSerialCmdCatalog::paramSchemaFor(ProductSerialCmd cmd, DeviceCmdParamSchema& out) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd)) {
        out.kind = DeviceCmdParamKind::None;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString ProductSerialCmdCatalog::paramUiHint(const QString& enumName) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
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

// ===================== ModbusPeriphCmdCatalog =====================

QStringList ModbusPeriphCmdCatalog::allDeviceKeys() {
    return {ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::InovanceH5uTcp),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::HqAmmeterRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::LxAmmeterRtu)};
}

QString ModbusPeriphCmdCatalog::deviceUiLabel(ModbusDeviceRoute device) {
    return ModbusDeviceCatalog::deviceRouteUiLabel(device);
}

ModbusDeviceRoute ModbusPeriphCmdCatalog::deviceFromIni(const QString& text) {
    return ModbusDeviceCatalog::deviceRouteFromIni(text);
}

QString ModbusPeriphCmdCatalog::deviceToIni(ModbusDeviceRoute device) {
    return ModbusDeviceCatalog::deviceRouteToIni(device);
}

QStringList ModbusPeriphCmdCatalog::allCmdNames(ModbusDeviceRoute device, TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < ModbusCmdManifest::rowCount(); ++i) {
        const ModbusCmdManifest::Row& row = ModbusCmdManifest::rows()[i];
        if (row.device != device) {
            continue;
        }
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action)) {
            continue;
        }
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

bool ModbusPeriphCmdCatalog::isCmdForDevice(ModbusDeviceRoute device, const QString& enumName,
                                            TestCaseSendAction action) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    if (!row) {
        return false;
    }
    return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
}

QString ModbusPeriphCmdCatalog::cmdUiLabel(ModbusDeviceRoute device, const QString& enumName) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->uiLabel ? QString::fromUtf8(row->uiLabel) : enumName;
}

QString ModbusPeriphCmdCatalog::paramUiHint(ModbusDeviceRoute device, const QString& enumName) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
}

// ===================== ScpiPeriphCmdCatalog =====================

QStringList ScpiPeriphCmdCatalog::allDeviceKeys() {
    return {QStringLiteral("HuilingWfp60h"), QStringLiteral("RsCmw100")};
}

QString ScpiPeriphCmdCatalog::deviceUiLabel(ScpiDeviceRoute device) {
    switch (device) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("会凌电源/万方模拟器");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("罗德与施瓦茨 CMW100");
    default:
        return QStringLiteral("未知设备");
    }
}

ScpiDeviceRoute ScpiPeriphCmdCatalog::deviceFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("HuilingWfp60h"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::HuilingWfp60h;
    if (t.compare(QStringLiteral("RsCmw100"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::RsCmw100;
    return ScpiDeviceRoute::None;
}

QString ScpiPeriphCmdCatalog::deviceToIni(ScpiDeviceRoute device) {
    switch (device) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("HuilingWfp60h");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("RsCmw100");
    default:
        return QStringLiteral("None");
    }
}

QStringList ScpiPeriphCmdCatalog::allCmdNames(ScpiDeviceRoute device, TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < ScpiCmdManifest::rowCount(); ++i) {
        const ScpiCmdManifest::Row& row = ScpiCmdManifest::rows()[i];
        if (row.device != device) {
            continue;
        }
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action)) {
            continue;
        }
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

bool ScpiPeriphCmdCatalog::isCmdForDevice(ScpiDeviceRoute device, const QString& enumName,
                                          TestCaseSendAction action) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    if (!row) {
        return false;
    }
    return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
}

QString ScpiPeriphCmdCatalog::cmdUiLabel(ScpiDeviceRoute device, const QString& enumName) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->uiLabel ? QString::fromUtf8(row->uiLabel) : enumName;
}

QString ScpiPeriphCmdCatalog::paramUiHint(ScpiDeviceRoute device, const QString& enumName) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
}

// ===================== TupleCmdCatalog =====================

QStringList TupleCmdCatalog::allTupleCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < TupleCmdManifest::rowCount(); ++i) {
        const TupleCmdManifest::Row& row = TupleCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction TupleCmdCatalog::actionFor(TupleCmd cmd) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool TupleCmdCatalog::isCmdForAction(TupleCmd cmd, TestCaseSendAction action) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString TupleCmdCatalog::tupleCmdUiLabel(const QString& enumName) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记云端指令");
}

bool TupleCmdCatalog::tupleCmdFromName(const QString& name, TupleCmd& out) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString TupleCmdCatalog::tupleCmdToName(TupleCmd cmd) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool TupleCmdCatalog::paramSchemaFor(TupleCmd cmd, DeviceCmdParamSchema& out) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString TupleCmdCatalog::paramUiHint(const QString& tupleCmdName) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByEnumName(tupleCmdName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    TupleCmd cmd;
    if (!tupleCmdFromName(tupleCmdName, cmd))
        return QStringLiteral("未知云端指令");
    return QStringLiteral("该云端指令未登记");
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
    {QStringLiteral("ProtocolRssiData"), QStringLiteral("蓝牙信号强度"), {{QStringLiteral("dbm"), QStringLiteral("信号强度(分贝)")}}},
    {QStringLiteral("ProtocolBatteryData"), QStringLiteral("电量"), {{QStringLiteral("percent"), QStringLiteral("电量(%)")}, {QStringLiteral("chargeState"), QStringLiteral("充电状态")}, {QStringLiteral("voltageMv"), QStringLiteral("电压(mV)")}}},
    {QStringLiteral("ProtocolKeyCapData"), QStringLiteral("按键电容"), {{QStringLiteral("capacitance"), QStringLiteral("电容值")}, {QStringLiteral("keyId"), QStringLiteral("按键编号")}}},
    {QStringLiteral("ProtocolChargeCurrentData"), QStringLiteral("充电电流"), {{QStringLiteral("currentMa"), QStringLiteral("电流(mA)")}}},
    {QStringLiteral("ProtocolTrimData"), QStringLiteral("Trim微调值"), {{QStringLiteral("trim"), QStringLiteral("微调值")}}},
    {QStringLiteral("ProtocolLightCalibData"), QStringLiteral("光感校准值"), {{QStringLiteral("calibValue"), QStringLiteral("校准值")}}},
    {QStringLiteral("ProtocolSnData"), QStringLiteral("序列号"), {{QStringLiteral("value"), QStringLiteral("序列号文本")}}},
    {QStringLiteral("ProtocolBaseInfoData"), QStringLiteral("基本信息"), {{QStringLiteral("soft_version"), QStringLiteral("软件版本")}, {QStringLiteral("res_version"), QStringLiteral("资源版本")}, {QStringLiteral("product_name"), QStringLiteral("产品名称")}, {QStringLiteral("hw_version"), QStringLiteral("硬件版本")}, {QStringLiteral("algo_version"), QStringLiteral("算法版本")}, {QStringLiteral("ageing_state"), QStringLiteral("老化状态")}}},
    {QStringLiteral("ProtocolPeriphStateData"), QStringLiteral("外设状态"), {{QStringLiteral("press0_state"), QStringLiteral("压感0状态")}, {QStringLiteral("press1_state"), QStringLiteral("压感1状态")}, {QStringLiteral("battery_ic_state"), QStringLiteral("电池IC状态")}, {QStringLiteral("touch_ic_state"), QStringLiteral("触摸IC状态")}, {QStringLiteral("led_ic_state"), QStringLiteral("LED IC状态")}, {QStringLiteral("pd_ic_state"), QStringLiteral("PD IC状态")}}},
    {QStringLiteral("ProtocolTupleData"), QStringLiteral("设备三元组"), {{QStringLiteral("productId"), QStringLiteral("产品密钥")}, {QStringLiteral("deviceId"), QStringLiteral("设备名")}, {QStringLiteral("key"), QStringLiteral("设备密钥")}}},
    {QStringLiteral("ProtocolButtonStateData"), QStringLiteral("按键状态"), {{QStringLiteral("modeButtonState"), QStringLiteral("模式键状态")}, {QStringLiteral("powerButtonState"), QStringLiteral("电源键状态")}, {QStringLiteral("keyButtonId"), QStringLiteral("按键编号")}}},
    {QStringLiteral("ProtocolAgingStatusData"), QStringLiteral("老化状态上报"), {{QStringLiteral("status"), QStringLiteral("状态码")}, {QStringLiteral("loops"), QStringLiteral("循环次数")}, {QStringLiteral("seconds"), QStringLiteral("秒数")}}},
    {QStringLiteral("ProtocolMusicStateData"), QStringLiteral("音乐状态"), {{QStringLiteral("musicState"), QStringLiteral("音乐状态码")}}},
    {QStringLiteral("ProtocolResultData"), QStringLiteral("通用结果码"), {{QStringLiteral("result"), QStringLiteral("结果码")}}},
    {QStringLiteral("ProtocolFixturePcbaData"), QStringLiteral("PCBA治具数据包"), {{QStringLiteral("machineNumber"), QStringLiteral("机号")}, {QStringLiteral("staticCurrent"), QStringLiteral("静态电流(uA)")}, {QStringLiteral("workingCurrent"), QStringLiteral("工作电流(mA)")}, {QStringLiteral("chargingCurrent"), QStringLiteral("充电电流(mA)")}, {QStringLiteral("musicCurrent"), QStringLiteral("音频IC电流(mA)")}, {QStringLiteral("standbyCurrentUa"), QStringLiteral("待机电流(uA)")}, {QStringLiteral("pumpVoltageMv"), QStringLiteral("泵电压(mV)")}, {QStringLiteral("mcuVoltageMv"), QStringLiteral("MCU电压(mV)")}, {QStringLiteral("valveVoltageMv"), QStringLiteral("阀电压(mV)")}, {QStringLiteral("button1"), QStringLiteral("按键1")}, {QStringLiteral("button2"), QStringLiteral("按键2")}, {QStringLiteral("overVoltageLight"), QStringLiteral("过压灯")}, {QStringLiteral("fixerro"), QStringLiteral("治具错误码")}}},
    {QStringLiteral("ProtocolMacData"), QStringLiteral("MAC地址"), {{QStringLiteral("mac"), QStringLiteral("MAC文本")}}},
    {QStringLiteral("ProtocolTypeData"), QStringLiteral("状态码"), {{QStringLiteral("type"), QStringLiteral("状态值")}}},
    {QStringLiteral("ProtocolMeasureData"), QStringLiteral("外设测量值"), {{QStringLiteral("value"), QStringLiteral("测量数值")}, {QStringLiteral("valueText"), QStringLiteral("测量文本值")}, {QStringLiteral("deviceName"), QStringLiteral("外设名称")}, {QStringLiteral("channel"), QStringLiteral("通道号")}, {QStringLiteral("type"), QStringLiteral("测量类型")}, {QStringLiteral("unit"), QStringLiteral("单位")}}},
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
    } else if (reportType == QLatin1String("ProtocolKeyCapData")) {
        const auto d = payload.value<ProtocolKeyCapData>();
        if (field == QLatin1String("capacitance")) {
            ok = true;
            return static_cast<double>(d.capacitance);
        }
        if (field == QLatin1String("keyId")) {
            ok = true;
            return d.keyId;
        }
    } else if (reportType == QLatin1String("ProtocolChargeCurrentData")) {
        const auto d = payload.value<ProtocolChargeCurrentData>();
        if (field == QLatin1String("currentMa")) {
            ok = true;
            return static_cast<double>(d.currentMa);
        }
    } else if (reportType == QLatin1String("ProtocolTrimData")) {
        const auto d = payload.value<ProtocolTrimData>();
        if (field == QLatin1String("trim")) {
            ok = true;
            return static_cast<double>(d.trim);
        }
    } else if (reportType == QLatin1String("ProtocolLightCalibData")) {
        const auto d = payload.value<ProtocolLightCalibData>();
        if (field == QLatin1String("calibValue")) {
            ok = true;
            return static_cast<double>(d.calibValue);
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
    } else if (reportType == QLatin1String("ProtocolTypeData") || reportType == QLatin1String("ProtocolBatteryTempData")) {
        const auto d = payload.value<ProtocolTypeData>();
        if (field == QLatin1String("type")) {
            ok = true;
            return d.type;
        }
    } else if (reportType == QLatin1String("ProtocolFixturePcbaData")) {
        QVariantMap m = payload.toMap();
        if (m.isEmpty()) {
            const FixturePacketData pack = payload.value<FixturePacketData>();
            m.insert(QStringLiteral("machineNumber"), pack.machineNumber);
            m.insert(QStringLiteral("staticCurrent"), pack.staticCurrent);
            m.insert(QStringLiteral("workingCurrent"), pack.workingCurrent);
            m.insert(QStringLiteral("chargingCurrent"), pack.chargingCurrent);
            m.insert(QStringLiteral("musicCurrent"), pack.musicCurrent);
            m.insert(QStringLiteral("standbyCurrentUa"), pack.standbyCurrentUa);
            m.insert(QStringLiteral("pumpVoltageMv"), pack.pumpVoltageMv);
            m.insert(QStringLiteral("mcuVoltageMv"), pack.mcuVoltageMv);
            m.insert(QStringLiteral("valveVoltageMv"), pack.valveVoltageMv);
            m.insert(QStringLiteral("button1"), pack.button1);
            m.insert(QStringLiteral("button2"), pack.button2);
            m.insert(QStringLiteral("overVoltageLight"), pack.overVoltageLight);
            m.insert(QStringLiteral("fixerro"), pack.fixerro);
        }
        if (m.contains(field)) {
            ok = true;
            return m.value(field).toDouble();
        }
    } else if (reportType == QLatin1String("ProtocolMeasureData")) {
        const auto d = payload.value<ProtocolMeasureData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return d.value;
        }
    }
    return 0.0;
}

QString fieldStringFromVariant(const QString& reportType, const QString& field, const QVariant& payload, bool& ok) {
    ok = false;
    if (reportType == QLatin1String("ProtocolRssiData")) {
        const auto d = payload.value<ProtocolRssiData>();
        if (field == QLatin1String("dbm")) {
            ok = true;
            return QString::number(d.dbm);
        }
    } else if (reportType == QLatin1String("ProtocolBatteryData")) {
        const auto d = payload.value<ProtocolBatteryData>();
        if (field == QLatin1String("percent")) {
            ok = true;
            return QString::number(d.percent);
        }
    } else if (reportType == QLatin1String("ProtocolSnData")) {
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
    } else if (reportType == QLatin1String("ProtocolMacData")) {
        const auto d = payload.value<ProtocolMacData>();
        if (field == QLatin1String("mac")) {
            ok = true;
            return d.mac.trimmed();
        }
    } else if (reportType == QLatin1String("ProtocolTypeData") || reportType == QLatin1String("ProtocolBatteryTempData")) {
        const auto d = payload.value<ProtocolTypeData>();
        if (field == QLatin1String("type")) {
            ok = true;
            return QString::number(d.type);
        }
    } else if (reportType == QLatin1String("ProtocolPeriphStateData")) {
        const auto d = payload.value<ProtocolPeriphStateData>();
        if (field == QLatin1String("press0_state")) {
            ok = true;
            return QString::number(d.press0_state);
        }
        if (field == QLatin1String("press1_state")) {
            ok = true;
            return QString::number(d.press1_state);
        }
        if (field == QLatin1String("battery_ic_state")) {
            ok = true;
            return QString::number(d.battery_ic_state);
        }
        if (field == QLatin1String("touch_ic_state")) {
            ok = true;
            return QString::number(d.touch_ic_state);
        }
        if (field == QLatin1String("led_ic_state")) {
            ok = true;
            return QString::number(d.led_ic_state);
        }
        if (field == QLatin1String("pd_ic_state")) {
            ok = true;
            return QString::number(d.pd_ic_state);
        }
    } else if (reportType == QLatin1String("ProtocolMeasureData")) {
        const auto d = payload.value<ProtocolMeasureData>();
        if (field == QLatin1String("value")) {
            ok = true;
            return QString::number(d.value);
        }
        if (field == QLatin1String("valueText")) {
            ok = true;
            return d.valueText.trimmed();
        }
        if (field == QLatin1String("deviceName")) {
            ok = true;
            return d.deviceName.trimmed();
        }
        if (field == QLatin1String("channel")) {
            ok = true;
            return d.channel.trimmed();
        }
        if (field == QLatin1String("type")) {
            ok = true;
            return d.type.trimmed();
        }
        if (field == QLatin1String("unit")) {
            ok = true;
            return d.unit.trimmed();
        }
    }
    return {};
}

} // namespace

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

bool GateRegistry::isAllFieldsGateField(const QString& field) {
    const QString f = field.trimmed();
    return f.isEmpty() || f == QLatin1String("*") || f.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0;
}

QString GateRegistry::fieldDisplayName(const QString& reportType, const QString& field) {
    GateTypeDescriptor desc;
    if (!descriptorFor(reportType, desc))
        return field;
    for (const GateFieldDescriptor& fd : desc.fields) {
        if (fd.field == field)
            return fd.displayName;
    }
    return field;
}

bool GateRegistry::evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                            QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (!gate.enabled)
        return true;

    if (isAllFieldsGateField(gate.field)) {
        const QStringList fields = fieldsFor(reportType);
        if (fields.isEmpty()) {
            passOut = false;
            detailOut = QStringLiteral("回传类型无可用判定字段");
            return true;
        }
        bool allPass = true;
        QStringList parts;
        for (const QString& subField : fields) {
            TestCaseGate subGate = gate;
            subGate.field = subField;
            bool subPass = true;
            QString subDetail;
            evaluate(subGate, reportType, payload, subPass, subDetail);
            if (!subPass)
                allPass = false;
            parts.append(QStringLiteral("%1(%2)").arg(fieldDisplayName(reportType, subField), subDetail));
        }
        passOut = allPass;
        detailOut = parts.join(QStringLiteral("; "));
        return true;
    }

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
        if (reportType == QLatin1String("ProtocolBaseInfoData") && gate.field == QLatin1String("soft_version") && gate.expected.isEmpty() && !gate.expectedSettingsKey.isEmpty() && !SETTINGS.value(QStringLiteral("ProductInfo/SoftwareVersion_checkBox"), true).toBool()) {
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
                detailOut = QStringLiteral("当前=%1, 未配置期望( Gate/Expected 或 MES/UI SN)").arg(actual.isEmpty() ? QStringLiteral("-") : actual);
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
    case TestCaseGateOp::Eq: {
        double expectVal = low;
        if (!gate.expected.trimmed().isEmpty()) {
            bool convOk = false;
            const double parsed = gate.expected.trimmed().toDouble(&convOk);
            if (convOk)
                expectVal = parsed;
        }
        passOut = qAbs(value - expectVal) < 0.0001;
        detailOut = QStringLiteral("当前值=%1, 期望=%2").arg(value).arg(expectVal);
        return true;
    }
    default:
        passOut = value >= low && value <= high;
        break;
    }
    detailOut = QStringLiteral("当前值=%1, 允许[%2,%3]").arg(value).arg(low).arg(high);
    return true;
}

bool GateRegistry::evaluateAll(const QVector<TestCaseGate>& gates, const QString& reportType,
                               const QVariant& payload, bool& passOut, QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (gates.isEmpty())
        return true;
    bool allPass = true;
    QStringList parts;
    for (const TestCaseGate& g : gates) {
        TestCaseGate ge = g;
        ge.enabled = true;
        ge.reportType = reportType;
        bool subPass = true;
        QString subDetail;
        evaluate(ge, reportType, payload, subPass, subDetail);
        if (!subPass)
            allPass = false;
        parts.append(QStringLiteral("%1(%2)")
                         .arg(fieldDisplayName(reportType, ge.field), subDetail));
    }
    passOut = allPass;
    detailOut = parts.join(QStringLiteral("; "));
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

QString GateRegistry::formatGateAsk(const TestCaseGate& gate, const QString& reportType) {
    Q_UNUSED(reportType);
    if (gate.op == TestCaseGateOp::Range) {
        double low = gate.low;
        double high = gate.high;
        resolveRangeBounds(gate, low, high);
        return QStringLiteral("[%1,%2]").arg(low).arg(high);
    }
    if (gate.op == TestCaseGateOp::Gt)
        return QStringLiteral(">%1").arg(gate.low);
    if (gate.op == TestCaseGateOp::Lt)
        return QStringLiteral("<%1").arg(gate.high);
    if (gate.op == TestCaseGateOp::Eq || gate.op == TestCaseGateOp::CompareVersions)
        return gate.expected.trimmed();
    return gate.expected.trimmed();
}

QString GateRegistry::formatMultiFieldAsk(const QVector<TestCaseGate>& gates, const QString& reportType) {
    QStringList expectedParts;
    expectedParts.reserve(gates.size());
    for (const TestCaseGate& g : gates) {
        const QString name = fieldDisplayName(reportType, g.field);
        if (g.op == TestCaseGateOp::Range) {
            double low = g.low;
            double high = g.high;
            resolveRangeBounds(g, low, high);
            expectedParts.append(QStringLiteral("%1=[%2,%3]").arg(name).arg(low).arg(high));
        } else if (g.op == TestCaseGateOp::Eq) {
            expectedParts.append(QStringLiteral("%1=%2").arg(name, g.expected));
        } else if (g.op == TestCaseGateOp::Gt) {
            expectedParts.append(QStringLiteral("%1>%2").arg(name).arg(g.low));
        } else if (g.op == TestCaseGateOp::Lt) {
            expectedParts.append(QStringLiteral("%1<%2").arg(name).arg(g.high));
        } else {
            expectedParts.append(QStringLiteral("%1:%2").arg(name, g.expected));
        }
    }
    return expectedParts.join(QLatin1Char(';'));
}

namespace {

QString fixturePacketSummary(const FixturePacketData& pack) {
    return QStringLiteral("机号=%1 静态=%2 工作=%3 充电=%4 泵=%5 MCU=%6 阀=%7")
        .arg(pack.machineNumber)
        .arg(pack.staticCurrent)
        .arg(pack.workingCurrent)
        .arg(pack.chargingCurrent)
        .arg(pack.pumpVoltageMv)
        .arg(pack.mcuVoltageMv)
        .arg(pack.valveVoltageMv);
}

QString periphStateSummary(const ProtocolPeriphStateData& periph) {
    return QStringLiteral("press0=%1;press1=%2;battery=%3;touch=%4;led=%5;pd=%6")
        .arg(periph.press0_state)
        .arg(periph.press1_state)
        .arg(periph.battery_ic_state)
        .arg(periph.touch_ic_state)
        .arg(periph.led_ic_state)
        .arg(periph.pd_ic_state);
}

QString primaryFieldTestData(const TestCaseGate& primaryGate, const QString& reportType, const QVariant& payload) {
    bool strOk = false;
    const QString fromField = fieldStringFromVariant(reportType, primaryGate.field, payload, strOk);
    if (strOk && !fromField.isEmpty())
        return fromField;
    bool numOk = false;
    const double fromNum = fieldValueFromVariant(reportType, primaryGate.field, payload, numOk);
    if (numOk)
        return QString::number(fromNum);
    return {};
}

} // namespace

GateStepDisplay GateRegistry::formatStepDisplay(const TestCaseGate& primaryGate, const QVector<TestCaseGate>& allGates,
                                                const QString& reportType, const QVariant& payload,
                                                bool multiFieldMode) {
    GateStepDisplay out;
    if (reportType == QLatin1String("ProtocolFixturePcbaData") && payload.canConvert<FixturePacketData>()) {
        out.testData = fixturePacketSummary(payload.value<FixturePacketData>());
    } else if (reportType == QLatin1String("ProtocolPeriphStateData") && payload.canConvert<ProtocolPeriphStateData>()) {
        out.testData = periphStateSummary(payload.value<ProtocolPeriphStateData>());
    } else {
        out.testData = primaryFieldTestData(primaryGate, reportType, payload);
    }

    if (multiFieldMode && allGates.size() > 1)
        out.ask = formatMultiFieldAsk(allGates, reportType);
    else
        out.ask = formatGateAsk(primaryGate, reportType);
    return out;
}

// ===================== TestCaseHookRegistry =====================

namespace {
QHash<QString, TestCaseHookFn>& hooks() {
    static QHash<QString, TestCaseHookFn> table;
    return table;
}
} // namespace

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

bool TestCaseRunner::stepWaitsForPromptAck(const TestCaseDefinition& def) {
    if (!def.meta.promptEnabled || def.hook.enabled || def.gate.enabled)
        return false;
    if (isDongleBleConnectStep(def))
        return false;
    if (def.send.action == TestCaseSendAction::Get)
        return false;
    return true;
}

bool TestCaseRunner::needAsyncDone(const TestCaseDefinition& def) {
    if (def.hook.enabled)
        return true;
    if (def.send.channel == TestCaseSendChannel::ProductSerial)
        return true;
    if (def.send.channel == TestCaseSendChannel::Fixture)
        return true;
    if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        if (def.send.action == TestCaseSendAction::Get || def.gate.enabled)
            return true;
    }
    if (isDongleBleConnectStep(def))
        return true;
    if (def.gate.enabled)
        return true;
    if (def.send.action == TestCaseSendAction::Get)
        return true;
    if (stepWaitsForPromptAck(def))
        return true;
    return false;
}

bool TestCaseRunner::isDongleBleConnectStep(const TestCaseDefinition& def) {
    if (def.send.channel != TestCaseSendChannel::Dongle || def.send.action != TestCaseSendAction::Set)
        return false;
    return def.send.deviceCmd == QStringLiteral("BleScanConnect") || def.send.deviceCmd == QStringLiteral("BleDirectConnect");
}

bool TestCaseRunner::stepRequiresProductBle(const TestCaseDefinition& def) {
    if (def.hook.enabled)
        return false;
    if (def.send.channel != TestCaseSendChannel::Product)
        return false;
    // 仅弹窗提示、无卡控：不依赖 BLE，流程可继续（如「弹窗提示开机」）
    if (def.meta.promptEnabled && !def.gate.enabled)
        return false;
    if (def.send.action == TestCaseSendAction::Get)
        return true;
    if (def.gate.enabled)
        return true;
    return true;
}

int TestCaseRunner::commandTimeoutMs(const TestCaseDefinition& def) {
    if (def.timing.commandTimeoutMs > 0)
        return def.timing.commandTimeoutMs;
    if (def.send.channel == TestCaseSendChannel::Fixture)
        return def.gate.enabled ? 8000 : 5000;
    if (def.send.channel == TestCaseSendChannel::ProductSerial)
        return 30000;
    if (isDongleBleConnectStep(def))
        return 6000;
    return def.gate.enabled ? 8000 : 3000;
}
