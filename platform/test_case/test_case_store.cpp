#include "test_case.h"
#include "test_case_ini_param.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <algorithm>
#include <QSettings>
#include <QTextCodec>

#include "Abini.h"
#include "common_utils.h"

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

QString stepsDir() {
    return rootDir() + QStringLiteral("/steps");
}

QString profilesDir() {
    return rootDir() + QStringLiteral("/profiles");
}

QString profileDir(const QString& stationKey);

QString profileMetaPath(const QString& stationKey) {
    return profileDir(stationKey) + QStringLiteral("/profile.ini");
}

QString profileFlowPath(const QString& stationKey) {
    return profileDir(stationKey) + QStringLiteral("/flow.ini");
}

QString profileStepOverridePath(const QString& stationKey, const QString& stepId) {
    return profileDir(stationKey) + QStringLiteral("/steps/") + stepId.trimmed() + QStringLiteral(".ini");
}

QString stepLibraryPath(const QString& stepId) {
    return stepsDir() + QLatin1Char('/') + stepId.trimmed() + QStringLiteral(".ini");
}

bool stepIniExistsForStation(const QString& stationKey, const QString& stepId) {
    const QString id = stepId.trimmed();
    if (id.isEmpty()) {
        return false;
    }
    if (QFile::exists(stepLibraryPath(id))) {
        return true;
    }
    const QString key = stationKey.trimmed();
    if (key.isEmpty()) {
        return false;
    }
    return QFile::exists(profileStepOverridePath(key, id));
}

bool ensureRootDir() {
    QDir dir(rootDir());
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;
    QDir(stepsDir()).mkpath(QStringLiteral("."));
    QDir(profilesDir()).mkpath(QStringLiteral("."));
        return true;
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

void syncTestCaseIni(QSettings& ini, const QString& filePath) {
    Q_UNUSED(filePath);
    ini.sync();
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

/** 工站对外以 profiles 目录名（中文）区分；StationKey 仅 ini 内 ASCII 键，已被其它目录占用则分配 FLOW_ST_xxxx */
QString resolveStationKeyForProfileFolder(const QString& folderName, const QString& profileIniKey,
                                          const QVector<TestFlowStationEntry>& catalog,
                                          const QHash<QString, int>& keyToIndex,
                                          const QHash<QString, int>& displayNameToIndex) {
    const QString displayName = folderName.trimmed();
    const int byNameIdx = displayNameToIndex.value(displayName, -1);
    if (byNameIdx >= 0)
        return catalog[byNameIdx].key.trimmed();

    auto keyTakenByOtherFolder = [&](const QString& key) {
        const QString k = key.trimmed();
        if (k.isEmpty())
            return false;
        const int idx = keyToIndex.value(k, -1);
        return idx >= 0 && catalog[idx].displayName != displayName;
    };

    QString stationKey = profileIniKey.trimmed();
    if (stationKey.isEmpty()) {
        stationKey = lookupPresetKeyFromDisplayName(displayName);
        if (stationKey.isEmpty() || keyTakenByOtherFolder(stationKey))
            stationKey = allocateCustomFlowStationKey(catalog);
    } else if (keyTakenByOtherFolder(stationKey)) {
        stationKey = allocateCustomFlowStationKey(catalog);
    }
    return stationKey;
}

void stripLegacyFlowMetaFromFlowIniFile() {
    const QString flowPath = TestCasePaths::flowIniPath();
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(QStringLiteral("Meta"));
    ini.remove(QString());
    ini.endGroup();
    syncTestCaseIni(ini, flowPath);
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
        TestCaseStore::saveSelectedFlowStation(meta.selectedStation, meta.selectedStationName);

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

bool copyDirectoryRecursively(const QString& srcDir, const QString& dstDir) {
    const QDir src(srcDir);
    if (!src.exists())
        return false;
    QDir().mkpath(dstDir);
    const QDir dst(dstDir);
    for (const QFileInfo& fi : src.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) {
        const QString dstPath = dst.filePath(fi.fileName());
        if (QFile::exists(dstPath))
            QFile::remove(dstPath);
        if (!QFile::copy(fi.absoluteFilePath(), dstPath))
            return false;
    }
    for (const QFileInfo& fi : src.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (!copyDirectoryRecursively(fi.absoluteFilePath(), dst.filePath(fi.fileName())))
            return false;
    }
    return true;
}

void ensureProfileDirectory(const QString& stationKey, const QString& displayName, const QString& createdFrom) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return;
    TestCasePaths::ensureRootDir();
    QDir().mkpath(TestCasePaths::profileDir(key) + QStringLiteral("/steps"));
    const QString metaPath = TestCasePaths::profileMetaPath(key);
    if (!QFile::exists(metaPath)) {
        QSettings meta(metaPath, QSettings::IniFormat);
        applyTestCaseIniCodec(meta);
        meta.setValue(QStringLiteral("Profile/StationKey"), key);
        meta.setValue(QStringLiteral("Profile/DisplayName"), displayName.isEmpty() ? key : displayName.trimmed());
        if (!createdFrom.isEmpty())
            meta.setValue(QStringLiteral("Profile/CreatedFrom"), createdFrom.trimmed());
        meta.setValue(QStringLiteral("Profile/CreatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
        meta.setValue(QStringLiteral("Profile/ProfileVersion"), 1);
        meta.setValue(QStringLiteral("Profile/StepsLibraryVersion"), 1);
        syncTestCaseIni(meta, metaPath);
        return;
    }
    if (!displayName.isEmpty()) {
        QSettings meta(metaPath, QSettings::IniFormat);
        applyTestCaseIniCodec(meta);
        const QString cur = meta.value(QStringLiteral("Profile/DisplayName")).toString();
        if (cur != displayName.trimmed()) {
            meta.setValue(QStringLiteral("Profile/DisplayName"), displayName.trimmed());
            syncTestCaseIni(meta, metaPath);
        }
    }
}

} // namespace

QVector<TestFlowStationEntry> TestCaseStore::defaultFlowStationPresets() {
    return builtinFlowStationPresets();
}

bool TestCaseStore::stationBelongsToProduct(const QString& stationDisplayName, const QString& productName) {
    return CommonUtils::stationBelongsToProduct(stationDisplayName, productName);
}

QVector<TestFlowStationEntry> TestCaseStore::loadFlowStationCatalogForProduct(const QString& productName) {
    const QVector<TestFlowStationEntry> all = loadFlowStationCatalog();
    if (productName.trimmed().isEmpty())
        return all;
    QVector<TestFlowStationEntry> filtered;
    filtered.reserve(all.size());
    for (const TestFlowStationEntry& entry : all) {
        if (stationBelongsToProduct(entry.displayName, productName))
            filtered.append(entry);
    }
    return filtered;
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

QString TestCasePaths::profileFolderName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return QString();
    QString folder = TestCaseStore::flowStationDisplayName(key);
    if (!isValidCaseFileName(folder, nullptr))
        folder = key;
    return folder;
}

QString TestCasePaths::profileDir(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    const QString folder = profileFolderName(key);
    const QString displayPath = profilesDir() + QLatin1Char('/') + folder;
    if (key.isEmpty())
        return displayPath;
    const QString legacyPath = profilesDir() + QLatin1Char('/') + key;
    if (QDir(displayPath).exists())
        return displayPath;
    if (legacyPath.compare(displayPath, Qt::CaseInsensitive) != 0 && QDir(legacyPath).exists())
        return legacyPath;
    return displayPath;
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
    QString key = presetKey;
    for (const TestFlowStationEntry& entry : catalog) {
        if (entry.displayName == name) {
            if (errorOut)
                *errorOut = QStringLiteral("工站名称已存在：%1").arg(name);
            return false;
        }
    }
    if (!key.isEmpty()) {
        for (const TestFlowStationEntry& entry : catalog) {
        if (entry.key.compare(key, Qt::CaseInsensitive) == 0) {
                key = QString();
                break;
        }
    }
    }
    if (key.isEmpty())
        key = allocateCustomFlowStationKey(catalog);
    catalog.append({key, name});
    if (!saveFlowStationCatalog(catalog))
        return false;
    ensureProfileDirectory(key, name, QString());
    return true;
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
    const QString srcProfile = TestCasePaths::profileDir(src);
    if (QDir(srcProfile).exists()) {
        copyDirectoryRecursively(srcProfile, TestCasePaths::profileDir(newKey));
        ensureProfileDirectory(newKey, newDisplayName.trimmed(), src);
    } else {
        ensureProfileDirectory(newKey, newDisplayName.trimmed(), QString());
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

    const QString oldDisplayName = catalog[targetIdx].displayName;
    catalog[targetIdx].displayName = name;
    if (!saveFlowStationCatalog(catalog))
        return false;

    const QString profilesRoot = TestCasePaths::profilesDir();
    const QString newDir = profilesRoot + QLatin1Char('/') + name;
    const QString oldDir = profilesRoot + QLatin1Char('/') + oldDisplayName;
    if (QDir(oldDir).exists() && oldDir.compare(newDir, Qt::CaseInsensitive) != 0) {
        if (QDir(newDir).exists())
            QDir(newDir).removeRecursively();
        QDir().rename(oldDir, newDir);
    }
    const QString legacyKeyDir = profilesRoot + QLatin1Char('/') + k;
    if (QDir(legacyKeyDir).exists() && legacyKeyDir.compare(newDir, Qt::CaseInsensitive) != 0) {
        if (QDir(newDir).exists())
            QDir(newDir).removeRecursively();
        QDir().rename(legacyKeyDir, newDir);
    }
    return true;
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

    const QString profilePath = TestCasePaths::profileDir(k);
    if (QDir(profilePath).exists())
        QDir(profilePath).removeRecursively();
    const QString legacyKeyPath = TestCasePaths::profilesDir() + QLatin1Char('/') + k;
    if (legacyKeyPath.compare(profilePath, Qt::CaseInsensitive) != 0 && QDir(legacyKeyPath).exists())
        QDir(legacyKeyPath).removeRecursively();
    return true;
}

namespace {

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

bool iniHasMultiGateItems(const QSettings& ini) {
    return ini.value(QStringLiteral("Gate/Count"), 0).toInt() > 0;
}

/** 多项卡控只认 [Gate] 下 Count + ItemN_Field / ItemN_Low …，与 saveMultiGatesToIni 同一套键。 */
void loadMultiGatesFromIni(QSettings& ini, TestCaseDefinition& out) {
    out.gates.clear();
    const QString reportType = out.gate.reportType;
    const int count = ini.value(QStringLiteral("Gate/Count"), 0).toInt();
    for (int i = 1; i <= count; ++i) {
        const QString p = QStringLiteral("Gate/Item%1_").arg(i);
        TestCaseGate g;
        g.reportType = reportType;
        g.field = ini.value(p + QStringLiteral("Field")).toString().trimmed();
        g.enabled = ini.value(p + QStringLiteral("Enabled"), true).toBool();
        g.op = gateOpFromString(ini.value(p + QStringLiteral("Op"), QStringLiteral("range")).toString());
        g.expected = ini.value(p + QStringLiteral("Expected")).toString().trimmed();
        g.low = ini.value(p + QStringLiteral("Low"), 0).toDouble();
        g.high = ini.value(p + QStringLiteral("High"), 0).toDouble();
        g.lowSettingsKey = ini.value(p + QStringLiteral("LowSettingsKey")).toString();
        g.highSettingsKey = ini.value(p + QStringLiteral("HighSettingsKey")).toString();
        g.expectedSettingsKey = ini.value(p + QStringLiteral("ExpectedSettingsKey")).toString();
        if (!g.field.isEmpty() && g.field != QLatin1String("multi"))
            out.gates.append(g);
    }
    if (!out.gates.isEmpty())
        return;

    if (out.gate.enabled && GateRegistry::isAllFieldsGateField(out.gate.field)
        && reportType == QStringLiteral("ProtocolPeriphStateData")) {
        for (const QString& f : GateRegistry::fieldsFor(reportType)) {
            TestCaseGate g = out.gate;
            g.field = f;
            g.op = TestCaseGateOp::Eq;
            g.expected = QString::number(static_cast<int>(out.gate.low));
            out.gates.append(g);
        }
        return;
    }
    // field=multi 只是占位，不能当成表格里的判定项，否则界面全是 0/未勾选
    if (out.gate.enabled && out.gate.field != QLatin1String("multi") && !out.gate.field.isEmpty())
        out.gates.append(out.gate);
}

bool isMultiFieldGateReportType(const QString& reportType) {
    return reportType == QLatin1String("ProtocolDongleSuctionPeakData")
        || reportType == QLatin1String("ProtocolFixturePcbaData")
        || reportType == QLatin1String("ProtocolJieliBtBoxData");
}

bool gateRangeLooksLikePlaceholder(const TestCaseGate& g) {
    return g.field.trimmed().isEmpty() || (qFuzzyIsNull(g.low) && qFuzzyIsNull(g.high));
}

bool profileHasLoadedMultiGateDetails(const TestCaseDefinition& def) {
    if (def.gates.size() > 1)
        return true;
    if (def.gates.size() == 1) {
        const TestCaseGate& g = def.gates.first();
        return !g.field.isEmpty() && g.field != QLatin1String("multi") && !gateRangeLooksLikePlaceholder(g);
    }
    return false;
}

void saveMultiGatesToIni(QSettings& ini, const TestCaseDefinition& def) {
    const QStringList keys = ini.allKeys();
    for (const QString& key : keys) {
        if (key == QLatin1String("Gate/Count") || key.startsWith(QLatin1String("Gate/Item")))
            ini.remove(key);
        else if (key.startsWith(QLatin1String("Gate/")) && key.size() > 5 && key.at(5).isDigit())
            ini.remove(key);
    }

    const bool saveAsMulti = def.gates.size() > 1
        || def.gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0
        || isMultiFieldGateReportType(def.gate.reportType);
    if (!saveAsMulti || def.gates.isEmpty())
        return;

    ini.setValue(QStringLiteral("Gate/Count"), def.gates.size());
    ini.setValue(QStringLiteral("Gate/Field"), QStringLiteral("multi"));
    for (int i = 0; i < def.gates.size(); ++i) {
        const TestCaseGate& g = def.gates.at(i);
        const QString p = QStringLiteral("Gate/Item%1_").arg(i + 1);
        ini.setValue(p + QStringLiteral("Field"), g.field);
        ini.setValue(p + QStringLiteral("Enabled"), g.enabled);
        ini.setValue(p + QStringLiteral("Op"), gateOpToString(g.op));
        ini.setValue(p + QStringLiteral("Expected"), g.expected);
        ini.setValue(p + QStringLiteral("Low"), g.low);
        ini.setValue(p + QStringLiteral("High"), g.high);
        ini.setValue(p + QStringLiteral("LowSettingsKey"), g.lowSettingsKey);
        ini.setValue(p + QStringLiteral("HighSettingsKey"), g.highSettingsKey);
        ini.setValue(p + QStringLiteral("ExpectedSettingsKey"), g.expectedSettingsKey);
    }
}

} // namespace

namespace {
bool loadCaseDefinitionFromIniFile(const QString& iniPath, const QString& stepId, TestCaseDefinition& out);
void applyCaseIniOverlay(QSettings& overlay, TestCaseDefinition& def);
bool writeCaseIniFile(const QString& path, const TestCaseDefinition& def, bool profileOverlayOnly);
void syncProfileFlowFromLegacyIni(const QString& stationKey);
void supplementMissingHookSendParamsFromLibrary(const QString& stepId, TestCaseDefinition& def);

void applySendChannelIniText(const QString& channelIni, TestCaseDefinition& def) {
    if (channelIni.compare(QStringLiteral("Dongle"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Dongle;
    } else if (channelIni.compare(QStringLiteral("Cloud"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Cloud;
    } else if (channelIni.compare(QStringLiteral("ProductSerial"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::ProductSerial;
    } else if (channelIni.compare(QStringLiteral("Product"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Product;
    } else if (channelIni.compare(QStringLiteral("Fixture"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Fixture;
    } else if (channelIni.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Modbus;
    } else if (channelIni.compare(QStringLiteral("Scpi"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Scpi;
    } else if (channelIni.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0) {
        // 兼容旧 Channel=UsbCamera：并入治具通信
        def.send.channel = TestCaseSendChannel::Fixture;
        def.send.fixtureProtocol = TestCaseFixtureProtocol::UsbCamera;
    }
}

void applySendProtocolIniText(const QString& protocolIni, TestCaseDefinition& def) {
    if (def.send.channel == TestCaseSendChannel::Fixture)
        def.send.fixtureProtocol = FixturePcbaCmdCatalog::fixtureProtocolFromIni(protocolIni);
    else
        def.send.productProtocol = DeviceCmdCatalog::productProtocolFromIni(protocolIni);
}

bool stepIniHasMeaningfulContent(const QString& path) {
    if (!QFile::exists(path))
        return false;
    if (QFileInfo(path).size() < 24)
        return false;
    QSettings ini(path, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    return ini.contains(QStringLiteral("Send/Channel")) || ini.contains(QStringLiteral("Send/DeviceCmd"))
           || ini.contains(QStringLiteral("Meta/MesTag")) || ini.contains(QStringLiteral("Hook/HookId"));
}

bool isLegacyHookDeviceCmdPlaceholder(const QString& deviceCmd) {
    return deviceCmd.trimmed().compare(QLatin1String("Hook"), Qt::CaseInsensitive) == 0;
}

/** 工站 steps 缺 [Hook] 时从步骤库补全（如 M8_写入mac：Send/DeviceCmd=Hook + MAC_WRITE_ROOT）。 */
void supplementMissingHookFromLibrary(const QString& stepId, TestCaseDefinition& def);

/** 工站 steps 缺 [Gate] 多字段卡控时从步骤库补全（如杰理 RSSI/频偏 WaitRfInfo）。 */
void supplementMissingGateFromLibrary(const QString& stationKey, const QString& stepId, TestCaseDefinition& def);

/** 将 test_case 根目录平铺 ini 迁入 steps/（库文件缺失或为空时覆盖） */
void migrateLegacyFlatInisToStepLibrary() {
    TestCasePaths::ensureRootDir();
    const QString flowName = TestCasePaths::flowIniFileName();
    QDir root(TestCasePaths::rootDir());
    for (const QFileInfo& fi : root.entryInfoList({QStringLiteral("*.ini")}, QDir::Files)) {
        if (fi.fileName().compare(flowName, Qt::CaseInsensitive) == 0)
            continue;
        const QString stepId = fi.completeBaseName();
        if (TestCasePaths::isReservedCaseName(stepId))
            continue;

        const QString legacyPath = fi.absoluteFilePath();
        if (!stepIniHasMeaningfulContent(legacyPath))
            continue;

        const QString libraryPath = TestCasePaths::stepLibraryPath(stepId);
        const bool libraryOk = stepIniHasMeaningfulContent(libraryPath);
        if (libraryOk)
            continue;

        if (QFile::exists(libraryPath))
            QFile::remove(libraryPath);
        QFile::copy(legacyPath, libraryPath);
    }
}

/** 将工站流程中各步骤的 Send/Timing/Gate 参数写入 profiles/{Key}/steps/ 覆盖层 */
void migrateProfileStepOverridesForStation(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return;
    ensureProfileDirectory(key, TestCaseStore::flowStationDisplayName(key), QString());
    QDir().mkpath(TestCasePaths::profileDir(key) + QStringLiteral("/steps"));

    QVector<TestFlowItemEntry> allEntries = TestCaseStore::loadStationFlowItems(key);
    allEntries += TestCaseStore::loadStationFailFlowItems(key);

    for (const TestFlowItemEntry& entry : allEntries) {
        const QString stepId = entry.caseName.trimmed();
        if (stepId.isEmpty())
            continue;

        const QString overlayPath = TestCasePaths::profileStepOverridePath(key, stepId);
        if (stepIniHasMeaningfulContent(overlayPath))
            continue;

        TestCaseDefinition def;
        const QString libraryPath = TestCasePaths::stepLibraryPath(stepId);
        const QString legacyPath = TestCasePaths::caseIniPath(stepId);
        if (!loadCaseDefinitionFromIniFile(libraryPath, stepId, def)
            && !loadCaseDefinitionFromIniFile(legacyPath, stepId, def)) {
            continue;
        }
        writeCaseIniFile(overlayPath, def, false);
    }
}

bool loadCaseDefinitionFromIniFile(const QString& iniPath, const QString& stepId, TestCaseDefinition& out) {
    if (!QFile::exists(iniPath))
        return false;
    QSettings ini(iniPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);

    const QString nameInIni = ini.value(QStringLiteral("Meta/Name"), stepId).toString().trimmed();
    const QString displayInIni = ini.value(QStringLiteral("Meta/DisplayName")).toString().trimmed();
    if (!displayInIni.isEmpty())
        out.meta.name = displayInIni;
    else if (!nameInIni.isEmpty())
        out.meta.name = nameInIni;
    else
        out.meta.name = stepId.trimmed();
    out.meta.displayName = out.meta.name;
    out.meta.mesTag = ini.value(QStringLiteral("Meta/MesTag")).toString().trimmed();
    out.meta.promptEnabled = ini.value(QStringLiteral("Meta/PromptEnabled"), false).toBool();
    out.meta.promptOnly = ini.value(QStringLiteral("Meta/PromptOnly"), false).toBool();
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
    } else if (channelIni.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Fixture;
        out.send.fixtureProtocol = TestCaseFixtureProtocol::UsbCamera;
    } else {
        FixturePcbaCmd inferFixturePcba;
        Asd9026aCmd inferAsd9026a;
        XwdRawFixtureCmd inferXwd;
        JieliBtBoxCmd inferJieliBtBox;
        ProductSerialCmd inferSerial;
        TupleCmd inferTuple;
        DongleCmd inferDongle;
        UsbCameraCmd inferUsbCamera;
        if (Asd9026aCmdCatalog::asd9026aCmdFromName(out.send.deviceCmd, inferAsd9026a)) {
            out.send.channel = TestCaseSendChannel::Fixture;
            out.send.fixtureProtocol = TestCaseFixtureProtocol::Asd9026a;
        } else if (XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(out.send.deviceCmd, inferXwd)) {
            out.send.channel = TestCaseSendChannel::Fixture;
            out.send.fixtureProtocol = TestCaseFixtureProtocol::Xwd;
        } else if (JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(out.send.deviceCmd, inferJieliBtBox)) {
            out.send.channel = TestCaseSendChannel::Fixture;
            out.send.fixtureProtocol = TestCaseFixtureProtocol::JieliBtBox;
        } else if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(out.send.deviceCmd, inferFixturePcba)) {
            out.send.channel = TestCaseSendChannel::Fixture;
        } else if (ProductSerialCmdCatalog::productSerialCmdFromName(out.send.deviceCmd, inferSerial)) {
            out.send.channel = TestCaseSendChannel::ProductSerial;
        } else if (TupleCmdCatalog::tupleCmdFromName(out.send.deviceCmd, inferTuple)) {
            out.send.channel = TestCaseSendChannel::Cloud;
        } else if (DongleCmdCatalog::dongleCmdFromName(out.send.deviceCmd, inferDongle)) {
            out.send.channel = TestCaseSendChannel::Dongle;
        } else if (UsbCameraCmdCatalog::usbCameraCmdFromName(out.send.deviceCmd, inferUsbCamera)) {
            out.send.channel = TestCaseSendChannel::Fixture;
            out.send.fixtureProtocol = TestCaseFixtureProtocol::UsbCamera;
        } else {
            out.send.channel = TestCaseSendChannel::Product;
        }
    }
    // 旧配置把 USB 摄像头步骤写在产品通道或独立通道，加载时并入治具协议
    {
        UsbCameraCmd camCmd;
        if (UsbCameraCmdCatalog::usbCameraCmdFromName(out.send.deviceCmd, camCmd)
            && out.send.channel != TestCaseSendChannel::Fixture) {
            out.send.channel = TestCaseSendChannel::Fixture;
            out.send.fixtureProtocol = TestCaseFixtureProtocol::UsbCamera;
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
        if (out.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a) {
            Asd9026aCmd asdCmd;
            if (Asd9026aCmdCatalog::asd9026aCmdFromName(out.send.deviceCmd, asdCmd)) {
                if (!Asd9026aCmdCatalog::isCmdForAction(asdCmd, out.send.action))
                    out.send.action = Asd9026aCmdCatalog::actionFor(asdCmd);
                Asd9026aCmdCatalog::paramFromIniGroup(ini, asdCmd, out.send.param);
            }
        } else if (out.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd) {
            XwdRawFixtureCmd xwdCmd;
            if (XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(out.send.deviceCmd, xwdCmd)) {
                if (!XwdRawFixtureCmdCatalog::isCmdForAction(xwdCmd, out.send.action))
                    out.send.action = XwdRawFixtureCmdCatalog::actionFor(xwdCmd);
                XwdRawFixtureCmdCatalog::paramFromIniGroup(ini, xwdCmd, out.send.param);
            }
        } else if (out.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox) {
            JieliBtBoxCmd jieliCmd;
            if (JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(out.send.deviceCmd, jieliCmd)) {
                if (!JieliBtBoxCmdCatalog::isCmdForAction(jieliCmd, out.send.action))
                    out.send.action = JieliBtBoxCmdCatalog::actionFor(jieliCmd);
                JieliBtBoxCmdCatalog::paramFromIniGroup(ini, jieliCmd, out.send.param);
            }
        } else if (out.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
            UsbCameraCmd camCmd;
            if (UsbCameraCmdCatalog::usbCameraCmdFromName(out.send.deviceCmd, camCmd)) {
                if (!UsbCameraCmdCatalog::isCmdForAction(camCmd, out.send.action))
                    out.send.action = UsbCameraCmdCatalog::actionFor(camCmd);
                UsbCameraCmdCatalog::paramFromIniGroup(ini, camCmd, out.send.param);
            }
        } else {
        FixturePcbaCmd fixtureCmd;
        if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(out.send.deviceCmd, fixtureCmd)) {
            if (!FixturePcbaCmdCatalog::isCmdForAction(fixtureCmd, out.send.action))
                out.send.action = FixturePcbaCmdCatalog::actionFor(fixtureCmd);
            FixturePcbaCmdCatalog::paramFromIniGroup(ini, fixtureCmd, out.send.param);
            }
        }
    } else if (out.send.channel == TestCaseSendChannel::Modbus || out.send.channel == TestCaseSendChannel::Scpi) {
        const QVariantMap paramMap = readSendParamMap(ini);
        if (!paramMap.isEmpty()) {
            out.send.param = normalizeScpiModbusParamFromMap(paramMap);
        } else {
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
        }
    } else {
        DeviceCmd cmd;
        if (DeviceCmdCatalog::deviceCmdFromName(out.send.deviceCmd, cmd)) {
            if (!DeviceCmdCatalog::isCmdForAction(cmd, out.send.action))
                out.send.action = DeviceCmdCatalog::actionFor(cmd);
            DeviceCmdCatalog::paramFromIniGroup(ini, cmd, out.send.param);
        }
        // Hook 步骤（如 COUNTDOWN_WAIT）的 Param_seconds 等通用键不在 DeviceCmd  schema 内
        mergeSendParamMapInto(out.send.param, readSendParamMap(ini));
    }

    out.timing.delayBeforeMs = ini.value(QStringLiteral("Timing/DelayBeforeMs"), 0).toInt();
    out.timing.delayAfterMs = ini.value(QStringLiteral("Timing/DelayAfterMs"), 0).toInt();
    out.timing.commandTimeoutMs = ini.value(QStringLiteral("Timing/CommandTimeoutMs"), 0).toInt();
    // 缺省 true：兼容旧 ini；进非信令等关机场景可写 WaitReply=false
    out.timing.waitReply = ini.value(QStringLiteral("Timing/WaitReply"), true).toBool();

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

void supplementMissingHookFromLibrary(const QString& stepId, TestCaseDefinition& def) {
    if (def.hook.enabled && !def.hook.hookId.isEmpty())
        return;
    if (!isLegacyHookDeviceCmdPlaceholder(def.send.deviceCmd) && def.hook.hookId.isEmpty())
        return;

    TestCaseDefinition library;
    const QString libraryPath = TestCasePaths::stepLibraryPath(stepId);
    const QString legacyPath = TestCasePaths::caseIniPath(stepId);
    if (!loadCaseDefinitionFromIniFile(libraryPath, stepId, library)
        && !loadCaseDefinitionFromIniFile(legacyPath, stepId, library)) {
        return;
    }
    if (!library.hook.hookId.isEmpty()) {
        def.hook.hookId = library.hook.hookId;
        if (!def.hook.enabled)
            def.hook.enabled = library.hook.enabled;
    }
}

void supplementMissingHookSendParamsFromLibrary(const QString& stepId, TestCaseDefinition& def) {
    if (!hookUsesGenericSendParamMap(def))
        return;
    QVariantMap current;
    if (def.send.param.canConvert<QVariantMap>())
        current = def.send.param.toMap();
    const auto hasSeconds = [&]() {
        if (current.contains(QStringLiteral("seconds")) && current.value(QStringLiteral("seconds")).toInt() > 0)
            return true;
        return current.contains(QStringLiteral("waitSeconds"))
               && current.value(QStringLiteral("waitSeconds")).toInt() > 0;
    };
    if (hasSeconds())
        return;

    TestCaseDefinition library;
    const QString libraryPath = TestCasePaths::stepLibraryPath(stepId);
    const QString legacyPath = TestCasePaths::caseIniPath(stepId);
    if (!loadCaseDefinitionFromIniFile(libraryPath, stepId, library)
        && !loadCaseDefinitionFromIniFile(legacyPath, stepId, library)) {
        return;
    }
    if (library.send.param.canConvert<QVariantMap>())
        mergeSendParamMapInto(def.send.param, library.send.param.toMap());
}

void supplementMissingGateFromLibrary(const QString& stationKey, const QString& stepId, TestCaseDefinition& def) {
    if (profileHasLoadedMultiGateDetails(def))
        return;

    const bool needsMultiFromLibrary =
        def.gate.enabled && isMultiFieldGateReportType(def.gate.reportType) && !profileHasLoadedMultiGateDetails(def);
    if (def.gate.enabled && !needsMultiFromLibrary)
        return;

    TestCaseDefinition library;
    const QString libraryPath = TestCasePaths::stepLibraryPath(stepId);
    const QString legacyPath = TestCasePaths::caseIniPath(stepId);
    if (!loadCaseDefinitionFromIniFile(libraryPath, stepId, library)
        && !loadCaseDefinitionFromIniFile(legacyPath, stepId, library)) {
        return;
    }
    const bool libraryHasMultiGates = library.gates.size() > 1
        || library.gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0;
    if (!libraryHasMultiGates)
        return;

    const QString key = stationKey.trimmed();
    if (!key.isEmpty()) {
        const QString profilePath = TestCasePaths::profileStepOverridePath(key, stepId);
        if (QFile::exists(profilePath)) {
            QSettings profileIni(profilePath, QSettings::IniFormat);
            applyTestCaseIniCodec(profileIni);
            // 工站显式关闭 Gate 时不强行注入库卡控
            if (profileIni.contains(QStringLiteral("Gate/Enabled"))
                && !profileIni.value(QStringLiteral("Gate/Enabled")).toBool()) {
                return;
            }
            const bool profileHasMultiInFile = iniHasMultiGateItems(profileIni);
            if (profileHasMultiInFile && profileHasLoadedMultiGateDetails(def))
                return;
            if (!needsMultiFromLibrary) {
                const bool profileHasOwnGate =
                    !profileIni.value(QStringLiteral("Gate/ReportType")).toString().trimmed().isEmpty()
                    || !profileIni.value(QStringLiteral("Gate/Field")).toString().trimmed().isEmpty();
                if (profileHasOwnGate && !libraryHasMultiGates)
                    return;
            }
        }
    }

    def.gate = library.gate;
    def.gates = library.gates;
}

void applyCaseIniOverlay(QSettings& overlay, TestCaseDefinition& def) {
    // 工站 profiles/.../steps/ 中已存在的键覆盖步骤库占位；未写入的键保留 def（来自步骤库）默认值
    if (overlay.contains(QStringLiteral("Meta/MesTag")))
        def.meta.mesTag = overlay.value(QStringLiteral("Meta/MesTag")).toString().trimmed();
    if (overlay.contains(QStringLiteral("Meta/PromptEnabled")))
        def.meta.promptEnabled = overlay.value(QStringLiteral("Meta/PromptEnabled")).toBool();
    if (overlay.contains(QStringLiteral("Meta/PromptOnly")))
        def.meta.promptOnly = overlay.value(QStringLiteral("Meta/PromptOnly")).toBool();
    if (overlay.contains(QStringLiteral("Meta/PromptText")))
        def.meta.promptText = overlay.value(QStringLiteral("Meta/PromptText")).toString();

    const bool hasSendOverlay = overlay.allKeys().contains(QStringLiteral("Send/Action"))
        || overlay.allKeys().contains(QStringLiteral("Send/Channel"))
        || overlay.allKeys().contains(QStringLiteral("Send/Protocol"))
        || overlay.allKeys().contains(QStringLiteral("Send/DeviceCmd"))
        || overlay.allKeys().contains(QStringLiteral("Send/Device"))
        || !readSendParamMap(overlay).isEmpty()
        || overlay.contains(QStringLiteral("Send/Param"));
    if (hasSendOverlay) {
        if (overlay.contains(QStringLiteral("Send/Action"))) {
            const QString action = overlay.value(QStringLiteral("Send/Action")).toString();
            def.send.action = action.compare(QLatin1String("Get"), Qt::CaseInsensitive) == 0 ? TestCaseSendAction::Get
                                                                                             : TestCaseSendAction::Set;
        }
        if (overlay.contains(QStringLiteral("Send/Channel")))
            applySendChannelIniText(overlay.value(QStringLiteral("Send/Channel")).toString().trimmed(), def);
        if (overlay.contains(QStringLiteral("Send/Protocol")))
            applySendProtocolIniText(overlay.value(QStringLiteral("Send/Protocol")).toString(), def);
        if (overlay.contains(QStringLiteral("Send/Device")))
            def.send.device = overlay.value(QStringLiteral("Send/Device")).toString().trimmed();
        if (overlay.contains(QStringLiteral("Send/DeviceCmd")))
            def.send.deviceCmd = overlay.value(QStringLiteral("Send/DeviceCmd")).toString().trimmed();
        const QVariantMap paramMap = readSendParamMap(overlay);
        if (!paramMap.isEmpty()) {
            if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
                QVariantMap merged;
                if (def.send.param.canConvert<QVariantMap>()) {
                    merged = def.send.param.toMap();
                }
                for (auto it = paramMap.constBegin(); it != paramMap.constEnd(); ++it) {
                    merged.insert(it.key(), it.value());
                }
                def.send.param = normalizeScpiModbusParamFromMap(merged);
            } else if (def.send.channel == TestCaseSendChannel::Product
                       || (def.send.channel == TestCaseSendChannel::Fixture
                           && def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera)) {
                // 编辑/存档侧保持 JsonMap；normalizeSendParam（如 Sn→DeviceSnPayload）仅在下发时做
                QVariantMap merged;
                if (def.send.param.canConvert<QVariantMap>())
                    merged = def.send.param.toMap();
                for (auto it = paramMap.constBegin(); it != paramMap.constEnd(); ++it)
                    merged.insert(it.key(), it.value());
                def.send.param = merged;
            } else if (!def.send.param.canConvert<QVariantMap>()) {
                // Param_string 等叶子读成单键 map；若不解开，String 型执行时 .toString() 会得到空串
                def.send.param = normalizeScpiModbusParamFromMap(paramMap);
            } else {
                QVariantMap merged = def.send.param.toMap();
                for (auto it = paramMap.constBegin(); it != paramMap.constEnd(); ++it)
                    merged.insert(it.key(), it.value());
                def.send.param = normalizeScpiModbusParamFromMap(merged);
            }
        } else if (overlay.contains(QStringLiteral("Send/Param"))) {
            def.send.param = overlay.value(QStringLiteral("Send/Param"));
        } else if (overlayHasSendParamKeys(overlay)) {
            if (def.send.channel == TestCaseSendChannel::Dongle) {
                DongleCmd dongleCmd;
                if (DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd))
                    DongleCmdCatalog::paramFromIniGroup(overlay, dongleCmd, def.send.param);
            } else if (def.send.channel == TestCaseSendChannel::Cloud) {
                TupleCmd tupleCmd;
                if (TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd))
                    TupleCmdCatalog::paramFromIniGroup(overlay, tupleCmd, def.send.param);
            } else if (def.send.channel == TestCaseSendChannel::Fixture) {
                if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a) {
                    Asd9026aCmd asdCmd;
                    if (Asd9026aCmdCatalog::asd9026aCmdFromName(def.send.deviceCmd, asdCmd))
                        Asd9026aCmdCatalog::paramFromIniGroup(overlay, asdCmd, def.send.param);
                } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd) {
                    XwdRawFixtureCmd xwdCmd;
                    if (XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(def.send.deviceCmd, xwdCmd))
                        XwdRawFixtureCmdCatalog::paramFromIniGroup(overlay, xwdCmd, def.send.param);
                } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox) {
                    JieliBtBoxCmd jieliCmd;
                    if (JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(def.send.deviceCmd, jieliCmd))
                        JieliBtBoxCmdCatalog::paramFromIniGroup(overlay, jieliCmd, def.send.param);
                } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
                    UsbCameraCmd camCmd;
                    if (UsbCameraCmdCatalog::usbCameraCmdFromName(def.send.deviceCmd, camCmd))
                        UsbCameraCmdCatalog::paramFromIniGroup(overlay, camCmd, def.send.param);
                } else {
                    FixturePcbaCmd fixtureCmd;
                    if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, fixtureCmd))
                        FixturePcbaCmdCatalog::paramFromIniGroup(overlay, fixtureCmd, def.send.param);
                }
            } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
                // paramMap 已在上方处理
            } else {
                DeviceCmd cmd;
                if (DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd))
                    DeviceCmdCatalog::paramFromIniGroup(overlay, cmd, def.send.param);
            }
        }
    }

    if (overlay.contains(QStringLiteral("Timing/DelayBeforeMs")))
        def.timing.delayBeforeMs = overlay.value(QStringLiteral("Timing/DelayBeforeMs")).toInt();
    if (overlay.contains(QStringLiteral("Timing/DelayAfterMs")))
        def.timing.delayAfterMs = overlay.value(QStringLiteral("Timing/DelayAfterMs")).toInt();
    if (overlay.contains(QStringLiteral("Timing/CommandTimeoutMs")))
        def.timing.commandTimeoutMs = overlay.value(QStringLiteral("Timing/CommandTimeoutMs")).toInt();
    if (overlay.contains(QStringLiteral("Timing/WaitReply")))
        def.timing.waitReply = overlay.value(QStringLiteral("Timing/WaitReply")).toBool();

    if (overlay.contains(QStringLiteral("Gate/Enabled")) || overlay.contains(QStringLiteral("Gate/ReportType"))
        || overlay.contains(QStringLiteral("Gate/Count"))) {
        // 多项卡控：覆盖层无 Gate/Count 时不冲掉步骤库 ItemN_ 明细。
        // 单字段（如读取版本号 Expected）必须合并，否则工站覆盖层写了也不生效。
        const bool overlayHasMulti = iniHasMultiGateItems(overlay);
        const bool libraryHasMultiGates = def.gates.size() > 1
            || def.gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0;
        if (!overlayHasMulti && libraryHasMultiGates) {
            if (overlay.contains(QStringLiteral("Gate/Enabled")))
                def.gate.enabled = overlay.value(QStringLiteral("Gate/Enabled")).toBool();
        } else {
            if (overlay.contains(QStringLiteral("Gate/Enabled")))
                def.gate.enabled = overlay.value(QStringLiteral("Gate/Enabled")).toBool();
            if (overlay.contains(QStringLiteral("Gate/ReportType")))
                def.gate.reportType = overlay.value(QStringLiteral("Gate/ReportType")).toString().trimmed();
            if (overlay.contains(QStringLiteral("Gate/Field")))
                def.gate.field = overlay.value(QStringLiteral("Gate/Field")).toString().trimmed();
            if (overlay.contains(QStringLiteral("Gate/Op")))
                def.gate.op = gateOpFromString(overlay.value(QStringLiteral("Gate/Op")).toString());
            if (overlay.contains(QStringLiteral("Gate/Low")))
                def.gate.low = overlay.value(QStringLiteral("Gate/Low")).toDouble();
            if (overlay.contains(QStringLiteral("Gate/High")))
                def.gate.high = overlay.value(QStringLiteral("Gate/High")).toDouble();
            if (overlay.contains(QStringLiteral("Gate/Expected")))
                def.gate.expected = overlay.value(QStringLiteral("Gate/Expected")).toString();
            if (overlay.contains(QStringLiteral("Gate/ExpectedSettingsKey")))
                def.gate.expectedSettingsKey = overlay.value(QStringLiteral("Gate/ExpectedSettingsKey")).toString();
            if (overlay.contains(QStringLiteral("Gate/LowSettingsKey")))
                def.gate.lowSettingsKey = overlay.value(QStringLiteral("Gate/LowSettingsKey")).toString();
            if (overlay.contains(QStringLiteral("Gate/HighSettingsKey")))
                def.gate.highSettingsKey = overlay.value(QStringLiteral("Gate/HighSettingsKey")).toString();
            loadMultiGatesFromIni(overlay, def);
        }
    }

    if (overlay.contains(QStringLiteral("Hook/Enabled")) || overlay.contains(QStringLiteral("Hook/HookId"))) {
        // 覆盖层未写 HookId（或写空）时保留步骤库钩子，避免残缺 profile 把 KEY_M8_* 冲成空/错项
        const QString overlayHookId = overlay.contains(QStringLiteral("Hook/HookId"))
            ? overlay.value(QStringLiteral("Hook/HookId")).toString().trimmed()
            : QString();
        if (!overlayHookId.isEmpty()) {
            def.hook.hookId = overlayHookId;
            if (overlay.contains(QStringLiteral("Hook/Enabled")))
                def.hook.enabled = overlay.value(QStringLiteral("Hook/Enabled")).toBool();
            else
                def.hook.enabled = true;
        } else if (overlay.contains(QStringLiteral("Hook/Enabled")) && def.hook.hookId.isEmpty()) {
            def.hook.enabled = overlay.value(QStringLiteral("Hook/Enabled")).toBool();
        }
    }
}

void writeNamedFlowItemList(QSettings& ini, const QString& itemsKey, const QString& disabledKey,
                            const QVector<TestFlowItemEntry>& items) {
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
    ini.setValue(itemsKey, names.join(QLatin1Char(',')));
    if (disabledNames.isEmpty()) {
        ini.remove(disabledKey);
    } else {
        ini.setValue(disabledKey, disabledNames.join(QLatin1Char(',')));
    }
}

QVector<TestFlowItemEntry> parseNamedFlowItemList(QSettings& ini, const QString& itemsKey,
                                                  const QString& disabledKey) {
    const QString rawItems = ini.value(itemsKey).toString();
    const QString rawDisabled = ini.value(disabledKey).toString();

    QSet<QString> disabledNames;
    for (const QString& part : rawDisabled.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (!name.isEmpty())
            disabledNames.insert(name);
    }

    QVector<TestFlowItemEntry> entries;
    for (const QString& part : rawItems.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString name = part.trimmed();
        if (name.isEmpty())
            continue;
        TestFlowItemEntry entry;
        entry.caseName = name;
        entry.enabled = !disabledNames.contains(name);
        entries.append(entry);
    }
    return entries;
}

QVector<TestFlowItemEntry> parseFlowItemsFromSettingsGroup(QSettings& ini) {
    const QString rawItems = ini.value(QStringLiteral("Items")).toString();
    const QString rawDisabled = ini.value(QStringLiteral("DisabledItems")).toString();
    const QString rawEnabled = ini.value(QStringLiteral("ItemEnabled")).toString();

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

void writeFlowItemsToSettingsGroup(QSettings& ini, const QVector<TestFlowItemEntry>& items,
                                   const QVector<TestFlowItemEntry>& failItems, bool stopFlowOnTestFail) {
    writeNamedFlowItemList(ini, QStringLiteral("Items"), QStringLiteral("DisabledItems"), items);
    writeNamedFlowItemList(ini, QStringLiteral("FailItems"), QStringLiteral("FailDisabledItems"), failItems);
    ini.remove(QStringLiteral("ItemEnabled"));
    ini.setValue(QStringLiteral("StopFlowOnTestFail"), stopFlowOnTestFail);
    ini.remove(QStringLiteral("StopOnGateFail"));
}

void syncProfileFlowFromLegacyIni(const QString& stationKey) {
    const QString k = stationKey.trimmed();
    if (k.isEmpty())
        return;
    const QString profileFlow = TestCasePaths::profileFlowPath(k);
    if (QFile::exists(profileFlow)) {
        QSettings existing(profileFlow, QSettings::IniFormat);
        applyTestCaseIniCodec(existing);
        existing.beginGroup(QStringLiteral("Flow"));
        const bool hasItems = existing.contains(QStringLiteral("Items"));
        existing.endGroup();
        if (hasItems)
            return;
    }

    QSettings legacy(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(legacy);
    legacy.beginGroup(stationGroup(k));
    const bool hadData = legacy.contains(QStringLiteral("Items")) || legacy.contains(QStringLiteral("StopFlowOnTestFail"))
                         || legacy.contains(QStringLiteral("StopOnGateFail"));
    if (!hadData) {
        legacy.endGroup();
        return;
    }
    const QVector<TestFlowItemEntry> items = parseFlowItemsFromSettingsGroup(legacy);
    bool stopFlow = true;
    if (legacy.contains(QStringLiteral("StopFlowOnTestFail")))
        stopFlow = legacy.value(QStringLiteral("StopFlowOnTestFail"), true).toBool();
    else if (legacy.contains(QStringLiteral("StopOnGateFail")))
        stopFlow = true;
    legacy.endGroup();

    ensureProfileDirectory(k, TestCaseStore::flowStationDisplayName(k), QString());
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("Flow"));
    writeFlowItemsToSettingsGroup(profileIni, items, {}, stopFlow);
    profileIni.endGroup();
    syncTestCaseIni(profileIni, profileFlow);
}

/** 将 profiles/{StationKey} 旧目录重命名为 profiles/{中文显示名} */
void migrateProfileDirToDisplayName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return;
    const QString folder = TestCasePaths::profileFolderName(key);
    if (folder.isEmpty() || folder.compare(key, Qt::CaseInsensitive) == 0)
        return;

    const QString root = TestCasePaths::profilesDir();
    const QString legacyDir = root + QLatin1Char('/') + key;
    const QString targetDir = root + QLatin1Char('/') + folder;
    if (!QDir(legacyDir).exists())
        return;
    if (QDir(targetDir).exists()) {
        if (legacyDir.compare(targetDir, Qt::CaseInsensitive) == 0)
            return;
        copyDirectoryRecursively(legacyDir, targetDir);
        QDir(legacyDir).removeRecursively();
        return;
    }
    QDir().rename(legacyDir, targetDir);
}

/** 扫描 profiles/{中文名}/profile.ini，将复制进来的工站自动登记到 FlowStations（一目录一条，目录名即显示名） */
void registerFlowStationsFromProfileDirs() {
    const QString profilesRoot = TestCasePaths::profilesDir();
    QDir profiles(profilesRoot);
    if (!profiles.exists())
        return;

    QVector<TestFlowStationEntry> catalog = TestCaseStore::loadFlowStationCatalog();
    QHash<QString, int> keyToIndex;
    QHash<QString, int> displayNameToIndex;
    for (int i = 0; i < catalog.size(); ++i) {
        keyToIndex.insert(catalog[i].key.trimmed(), i);
        displayNameToIndex.insert(catalog[i].displayName.trimmed(), i);
    }

    bool catalogChanged = false;
    for (const QFileInfo& fi : profiles.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString folderName = fi.fileName().trimmed();
        if (folderName.isEmpty() || !TestCasePaths::isValidCaseFileName(folderName, nullptr))
            continue;

        const QString profileIniPath = fi.absoluteFilePath() + QStringLiteral("/profile.ini");
        const QString flowIniPath = fi.absoluteFilePath() + QStringLiteral("/flow.ini");
        if (!QFile::exists(profileIniPath) || !QFile::exists(flowIniPath))
            continue;

        QSettings meta(profileIniPath, QSettings::IniFormat);
        applyTestCaseIniCodec(meta);
        const QString profileIniKey = meta.value(QStringLiteral("Profile/StationKey")).toString().trimmed();
        const QString displayName = folderName;

        const QString stationKey =
            resolveStationKeyForProfileFolder(folderName, profileIniKey, catalog, keyToIndex, displayNameToIndex);

        if (profileIniKey != stationKey) {
            meta.setValue(QStringLiteral("Profile/StationKey"), stationKey);
            catalogChanged = true;
        }
        if (meta.value(QStringLiteral("Profile/DisplayName")).toString().trimmed() != displayName) {
            meta.setValue(QStringLiteral("Profile/DisplayName"), displayName);
            catalogChanged = true;
        }
        if (meta.status() == QSettings::NoError)
            syncTestCaseIni(meta, profileIniPath);

        const int byNameIdx = displayNameToIndex.value(displayName, -1);
        if (byNameIdx < 0) {
            catalog.append({stationKey, displayName});
            const int newIdx = catalog.size() - 1;
            keyToIndex.insert(stationKey, newIdx);
            displayNameToIndex.insert(displayName, newIdx);
            catalogChanged = true;
        } else {
            if (catalog[byNameIdx].key != stationKey) {
                catalog[byNameIdx].key = stationKey;
                keyToIndex.insert(stationKey, byNameIdx);
                catalogChanged = true;
            }
        }
    }

    if (catalogChanged)
        TestCaseStore::saveFlowStationCatalog(catalog);
}

void ensureFilesystemLayoutOnce() {
    static bool profileSynced = false;

    TestCasePaths::ensureRootDir();
    migrateLegacyFlatInisToStepLibrary();

    if (profileSynced)
        return;
    profileSynced = true;

    registerFlowStationsFromProfileDirs();

    QSet<QString> stationKeys;
    for (const TestFlowStationEntry& entry : TestCaseStore::loadFlowStationCatalog())
        stationKeys.insert(entry.key.trimmed());
    for (const QString& key : TestCaseStore::listStationKeysFromFlow()) {
        if (!key.trimmed().isEmpty())
            stationKeys.insert(key.trimmed());
    }
    for (const QString& key : stationKeys) {
        migrateProfileDirToDisplayName(key);
        ensureProfileDirectory(key, TestCaseStore::flowStationDisplayName(key), QString());
        syncProfileFlowFromLegacyIni(key);
        migrateProfileStepOverridesForStation(key);
    }

    // 复制进来的 profiles 目录：补登记后再次纳入迁移
    for (const TestFlowStationEntry& entry : TestCaseStore::loadFlowStationCatalog()) {
        const QString key = entry.key.trimmed();
        if (key.isEmpty() || stationKeys.contains(key))
            continue;
        migrateProfileDirToDisplayName(key);
        ensureProfileDirectory(key, entry.displayName, QString());
    }
}

bool writeCaseIniFile(const QString& path, const TestCaseDefinition& def, bool profileOverlayOnly) {
    const QString stepId = def.meta.name.trimmed();
    if (stepId.isEmpty() || path.trimmed().isEmpty())
        return false;

    QSettings ini(path, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.clear();

    ini.setValue(QStringLiteral("Meta/StepId"), stepId);
    ini.setValue(QStringLiteral("Meta/Name"), stepId);

    if (profileOverlayOnly) {
        if (!def.send.device.isEmpty())
            ini.setValue(QStringLiteral("Send/Device"), def.send.device);
        if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
            QString channelStr = def.send.channel == TestCaseSendChannel::Scpi ? QStringLiteral("Scpi")
                                                                               : QStringLiteral("Modbus");
            ini.setValue(QStringLiteral("Send/Channel"), channelStr);
            if (!def.send.deviceCmd.isEmpty())
                ini.setValue(QStringLiteral("Send/DeviceCmd"), def.send.deviceCmd);
            ini.setValue(QStringLiteral("Send/Action"),
                         def.send.action == TestCaseSendAction::Get ? QStringLiteral("Get") : QStringLiteral("Set"));
        }
        if (def.send.channel == TestCaseSendChannel::Dongle) {
            DongleCmd dongleCmd;
            if (DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd))
                DongleCmdCatalog::paramToIniGroup(ini, dongleCmd, def.send.param);
        } else if (def.send.channel == TestCaseSendChannel::Cloud) {
            TupleCmd tupleCmd;
            if (TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd))
                TupleCmdCatalog::paramToIniGroup(ini, tupleCmd, def.send.param);
        } else if (def.send.channel == TestCaseSendChannel::Fixture) {
            if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a) {
                Asd9026aCmd asdCmd;
                if (Asd9026aCmdCatalog::asd9026aCmdFromName(def.send.deviceCmd, asdCmd))
                    Asd9026aCmdCatalog::paramToIniGroup(ini, asdCmd, def.send.param);
            } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd) {
                XwdRawFixtureCmd xwdCmd;
                if (XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(def.send.deviceCmd, xwdCmd))
                    XwdRawFixtureCmdCatalog::paramToIniGroup(ini, xwdCmd, def.send.param);
            } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox) {
                JieliBtBoxCmd jieliCmd;
                if (JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(def.send.deviceCmd, jieliCmd))
                    JieliBtBoxCmdCatalog::paramToIniGroup(ini, jieliCmd, def.send.param);
            } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
                UsbCameraCmd camCmd;
                if (UsbCameraCmdCatalog::usbCameraCmdFromName(def.send.deviceCmd, camCmd))
                    UsbCameraCmdCatalog::paramToIniGroup(ini, camCmd, def.send.param);
            } else {
                FixturePcbaCmd fixtureCmd;
                if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, fixtureCmd))
                    FixturePcbaCmdCatalog::paramToIniGroup(ini, fixtureCmd, def.send.param);
            }
        } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
            writeScpiModbusParamToIni(ini, def.send.param);
        } else if (def.send.channel != TestCaseSendChannel::ProductSerial) {
            DeviceCmd cmd;
            if (DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd))
                DeviceCmdCatalog::paramToIniGroup(ini, cmd, def.send.param);
            writeGenericHookSendParamMap(ini, def);
        }
        ini.setValue(QStringLiteral("Timing/DelayBeforeMs"), def.timing.delayBeforeMs);
        ini.setValue(QStringLiteral("Timing/DelayAfterMs"), def.timing.delayAfterMs);
        ini.setValue(QStringLiteral("Timing/CommandTimeoutMs"), def.timing.commandTimeoutMs);
        ini.setValue(QStringLiteral("Timing/WaitReply"), def.timing.waitReply);
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
        syncTestCaseIni(ini, path);
        return true;
    }

    ini.setValue(QStringLiteral("Meta/DisplayName"), def.meta.name);
    ini.setValue(QStringLiteral("Meta/MesTag"), def.meta.mesTag);
    ini.setValue(QStringLiteral("Meta/PromptEnabled"), def.meta.promptEnabled);
    ini.setValue(QStringLiteral("Meta/PromptOnly"), def.meta.promptOnly);
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
        ini.setValue(QStringLiteral("Send/Protocol"), DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol));
    else if (def.send.channel == TestCaseSendChannel::Fixture) {
        ini.setValue(QStringLiteral("Send/Protocol"), FixturePcbaCmdCatalog::fixtureProtocolToIni(def.send.fixtureProtocol));
        if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a) {
            ini.setValue(QStringLiteral("Send/Device"),
                         def.send.device.isEmpty() ? QStringLiteral("ASD9026A") : def.send.device);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd) {
            ini.setValue(QStringLiteral("Send/Device"),
                         def.send.device.isEmpty() ? QStringLiteral("XWD") : def.send.device);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox) {
            ini.setValue(QStringLiteral("Send/Device"),
                         def.send.device.isEmpty() ? QStringLiteral("JIELI_BT_BOX") : def.send.device);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
            ini.setValue(QStringLiteral("Send/Device"),
                         def.send.device.isEmpty() ? QStringLiteral("USB_CAMERA") : def.send.device);
        }
    }
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
        if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a) {
            Asd9026aCmd asdCmd;
            if (Asd9026aCmdCatalog::asd9026aCmdFromName(def.send.deviceCmd, asdCmd))
                Asd9026aCmdCatalog::paramToIniGroup(ini, asdCmd, def.send.param);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd) {
            XwdRawFixtureCmd xwdCmd;
            if (XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(def.send.deviceCmd, xwdCmd))
                XwdRawFixtureCmdCatalog::paramToIniGroup(ini, xwdCmd, def.send.param);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox) {
            JieliBtBoxCmd jieliCmd;
            if (JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(def.send.deviceCmd, jieliCmd))
                JieliBtBoxCmdCatalog::paramToIniGroup(ini, jieliCmd, def.send.param);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
            UsbCameraCmd camCmd;
            if (UsbCameraCmdCatalog::usbCameraCmdFromName(def.send.deviceCmd, camCmd))
                UsbCameraCmdCatalog::paramToIniGroup(ini, camCmd, def.send.param);
        } else {
        FixturePcbaCmd fixtureCmd;
        if (FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, fixtureCmd))
            FixturePcbaCmdCatalog::paramToIniGroup(ini, fixtureCmd, def.send.param);
        }
    } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        if (!def.send.device.isEmpty())
            ini.setValue(QStringLiteral("Send/Device"), def.send.device);
        writeScpiModbusParamToIni(ini, def.send.param);
    } else if (def.send.channel != TestCaseSendChannel::ProductSerial) {
        DeviceCmd cmd;
        if (DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd))
            DeviceCmdCatalog::paramToIniGroup(ini, cmd, def.send.param);
        writeGenericHookSendParamMap(ini, def);
    }

    ini.setValue(QStringLiteral("Timing/DelayBeforeMs"), def.timing.delayBeforeMs);
    ini.setValue(QStringLiteral("Timing/DelayAfterMs"), def.timing.delayAfterMs);
    ini.setValue(QStringLiteral("Timing/CommandTimeoutMs"), def.timing.commandTimeoutMs);
    ini.setValue(QStringLiteral("Timing/WaitReply"), def.timing.waitReply);

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
    syncTestCaseIni(ini, path);
    return true;
}

} // namespace

void TestCaseStore::ensureFilesystemLayout() {
    ensureFilesystemLayoutOnce();
}

void TestCaseStore::reregisterFlowStationsFromProfiles() {
    TestCasePaths::ensureRootDir();
    registerFlowStationsFromProfileDirs();
}

bool TestCaseStore::loadCaseForStation(const QString& stationKey, const QString& stepId, TestCaseDefinition& out,
                                       QString* errorOut) {
    Q_UNUSED(errorOut);
    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();

    const QString id = stepId.trimmed();
    if (id.isEmpty())
        return false;

    out = TestCaseDefinition{};
    const QString libraryPath = TestCasePaths::stepLibraryPath(id);
    const QString legacyPath = TestCasePaths::caseIniPath(id);
    const QString key = stationKey.trimmed();
    const QString profileStepPath =
        key.isEmpty() ? QString() : TestCasePaths::profileStepOverridePath(key, id);

    // 不做运行时「步骤库+工站」字段覆盖：工站 steps 有完整文件则只读工站；
    // 缺项应事先把步骤库内容补进工站文件（保存/补全），避免每次加载用库盖工站。
    bool loaded = false;
    if (!profileStepPath.isEmpty() && stepIniHasMeaningfulContent(profileStepPath)) {
        loaded = loadCaseDefinitionFromIniFile(profileStepPath, id, out);
    }
    if (!loaded) {
        loaded = loadCaseDefinitionFromIniFile(libraryPath, id, out);
        if (!loaded)
            loaded = loadCaseDefinitionFromIniFile(legacyPath, id, out);
    }
    if (loaded && !key.isEmpty()) {
        supplementMissingHookFromLibrary(id, out);
        supplementMissingHookSendParamsFromLibrary(id, out);
        supplementMissingGateFromLibrary(key, id, out);
    }
    return loaded;
}

bool TestCaseStore::loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut) {
    return loadCaseForStation(QString(), caseName, out, errorOut);
}

bool TestCaseStore::saveCaseForStation(const QString& stationKey, const TestCaseDefinition& def, QString* errorOut) {
    Q_UNUSED(errorOut);
    TestCasePaths::ensureRootDir();
    const QString stepId = def.meta.name.trimmed();
    if (stepId.isEmpty())
        return false;

    const QString key = stationKey.trimmed();
    if (!key.isEmpty()) {
        // 工站上下文只写本工站 profile steps，绝不改动总步骤库模板
        ensureProfileDirectory(key, TestCaseStore::flowStationDisplayName(key), QString());
        QDir().mkpath(TestCasePaths::profileDir(key) + QStringLiteral("/steps"));
        if (!writeCaseIniFile(TestCasePaths::profileStepOverridePath(key, stepId), def, false))
            return false;
    } else {
        // stationKey 为空：仅总步骤库维护入口
        if (!writeCaseIniFile(TestCasePaths::stepLibraryPath(stepId), def, false))
            return false;
        writeCaseIniFile(TestCasePaths::caseIniPath(stepId), def, false);
    }
    invalidateCloudItemNameCache();
    return true;
}

bool TestCaseStore::saveCase(const TestCaseDefinition& def, QString* errorOut) {
    return saveCaseForStation(QString(), def, errorOut);
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
    ensureFilesystemLayout();
    QSet<QString> nameSet;
    const QDir stepsDir(TestCasePaths::stepsDir());
    if (stepsDir.exists()) {
        for (const QFileInfo& fi : stepsDir.entryInfoList({QStringLiteral("*.ini")}, QDir::Files))
            nameSet.insert(fi.completeBaseName());
    }
    QDir root(TestCasePaths::rootDir());
    const QString flowName = TestCasePaths::flowIniFileName();
    for (const QFileInfo& fi : root.entryInfoList({QStringLiteral("*.ini")}, QDir::Files)) {
        if (fi.fileName().compare(flowName, Qt::CaseInsensitive) == 0)
            continue;
        const QString base = fi.completeBaseName();
        if (!TestCasePaths::isReservedCaseName(base))
            nameSet.insert(base);
    }
    QStringList names = nameSet.values();
    names.sort();
    return names;
}

namespace {

QHash<QString, QString>& cloudItemNameMapCache() {
    static QHash<QString, QString> map;
    return map;
}

bool& cloudItemNameMapLoaded() {
    static bool loaded = false;
    return loaded;
}

void registerCloudItemNameAlias(QHash<QString, QString>* map, const QString& key, const QString& display) {
    const QString k = key.trimmed();
    if (k.isEmpty() || display.trimmed().isEmpty()) {
        return;
    }
    map->insert(k, display.trimmed());
}

void rebuildCloudItemNameMap() {
    QHash<QString, QString>& map = cloudItemNameMapCache();
    map.clear();
    TestCasePaths::ensureRootDir();
    for (const QString& caseName : TestCaseStore::listCaseIniNames()) {
        const QString libPath = TestCasePaths::stepLibraryPath(caseName);
        const QString legacyPath = TestCasePaths::caseIniPath(caseName);
        const QString iniPath = QFile::exists(libPath) ? libPath : legacyPath;
        QSettings ini(iniPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
        const QString nameInIni = ini.value(QStringLiteral("Meta/Name"), caseName).toString().trimmed();
        const QString displayInIni = ini.value(QStringLiteral("Meta/DisplayName")).toString().trimmed();
        const QString mesTag = ini.value(QStringLiteral("Meta/MesTag")).toString().trimmed();
        const QString display = !displayInIni.isEmpty() ? displayInIni
                                                        : (!nameInIni.isEmpty() ? nameInIni : caseName.trimmed());
        registerCloudItemNameAlias(&map, mesTag, display);
        registerCloudItemNameAlias(&map, nameInIni, display);
        registerCloudItemNameAlias(&map, caseName, display);
    }
    // 杰理蓝牙盒子多字段卡控拆项后的 MES 键 → 云端中文名
    registerCloudItemNameAlias(&map, QStringLiteral("BT_RSSI"), QStringLiteral("RSSI(dBm)"));
    registerCloudItemNameAlias(&map, QStringLiteral("BT_FREQ_OFFSET"), QStringLiteral("频偏"));
    cloudItemNameMapLoaded() = true;
}

} // namespace

QString TestCaseStore::cloudDisplayNameForItemKey(const QString& itemKey) {
    const QString key = itemKey.trimmed();
    if (key.isEmpty()) {
        return key;
    }
    if (!cloudItemNameMapLoaded()) {
        rebuildCloudItemNameMap();
    }
    const auto it = cloudItemNameMapCache().constFind(key);
    return it != cloudItemNameMapCache().constEnd() ? it.value() : key;
}

void TestCaseStore::invalidateCloudItemNameCache() {
    cloudItemNameMapCache().clear();
    cloudItemNameMapLoaded() = false;
}

void TestCaseStore::migrateLegacyFlowMetaToLocalSettings() {
    if (!SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString().trimmed().isEmpty())
        return;
    TestCasePaths::ensureRootDir();
    const QString flowPath = TestCasePaths::flowIniPath();
    if (!QFile::exists(flowPath))
        return;
    QSettings ini(flowPath, QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    const QString legacyKey = ini.value(QStringLiteral("Meta/SelectedStation")).toString().trimmed();
    const QString legacyName = ini.value(QStringLiteral("Meta/SelectedStationName")).toString().trimmed();
    if (legacyKey.isEmpty() && legacyName.isEmpty())
        return;
    if (!legacyKey.isEmpty())
        SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStation"), legacyKey);
    const QString name = legacyName.isEmpty() ? flowStationDisplayName(legacyKey) : legacyName;
    if (!name.isEmpty())
        SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStationName"), name);
    stripLegacyFlowMetaFromFlowIniFile();
}

QString TestCaseStore::loadSelectedFlowStationKey() {
    migrateLegacyFlowMetaToLocalSettings();
    return SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString().trimmed();
}

QString TestCaseStore::loadSelectedFlowStationName() {
    migrateLegacyFlowMetaToLocalSettings();
    QString name = SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStationName")).toString().trimmed();
    if (name.isEmpty()) {
        const QString key = loadSelectedFlowStationKey();
        if (!key.isEmpty())
            name = flowStationDisplayName(key);
    }
    return name;
}

void TestCaseStore::saveSelectedFlowStation(const QString& stationKey, const QString& displayName) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return;
    QString name = displayName.trimmed();
    if (name.isEmpty())
        name = flowStationDisplayName(key);
    SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStation"), key);
    SETTINGS.setValue(QStringLiteral("TestOrderMeta/SelectedStationName"), name);
}

bool TestCaseStore::loadFlowMeta(TestFlowMeta& out) {
    migrateLegacyFlowMetaToLocalSettings();
    out.version = 1;
    out.selectedStation = loadSelectedFlowStationKey();
    out.selectedStationName = loadSelectedFlowStationName();
    return true;
}

bool TestCaseStore::saveFlowMeta(const TestFlowMeta& meta) {
    Q_UNUSED(meta);
    saveSelectedFlowStation(meta.selectedStation, meta.selectedStationName);
    stripLegacyFlowMetaFromFlowIniFile();
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
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed();

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (QFile::exists(profileFlow)) {
        QSettings profileIni(profileFlow, QSettings::IniFormat);
        applyTestCaseIniCodec(profileIni);
        profileIni.beginGroup(QStringLiteral("Flow"));
        const bool hasItems = profileIni.contains(QStringLiteral("Items"));
        bool result = defaultValue;
        if (profileIni.contains(QStringLiteral("StopFlowOnTestFail")))
            result = profileIni.value(QStringLiteral("StopFlowOnTestFail"), defaultValue).toBool();
        else if (profileIni.contains(QStringLiteral("StopOnGateFail")))
            result = true;
        profileIni.endGroup();
        if (hasItems)
            return result;
    }

    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(key));
    bool result = defaultValue;
    if (ini.contains(QStringLiteral("StopFlowOnTestFail")))
        result = ini.value(QStringLiteral("StopFlowOnTestFail"), defaultValue).toBool();
    else if (ini.contains(QStringLiteral("StopOnGateFail")))
        result = true;
    ini.endGroup();
    return result;
}

namespace {

void readSerialUiFieldsFromGroup(QSettings& ini, TestCaseSerialUiConfig& out) {
    if (ini.contains(QStringLiteral("JigVisible")))
        out.jigVisible = ini.value(QStringLiteral("JigVisible"), true).toBool();
    if (ini.contains(QStringLiteral("ProductVisible")))
        out.productVisible = ini.value(QStringLiteral("ProductVisible"), true).toBool();
    if (ini.contains(QStringLiteral("UsbVisible")))
        out.usbVisible = ini.value(QStringLiteral("UsbVisible"), true).toBool();
    const QString jigLabel = ini.value(QStringLiteral("JigLabel")).toString().trimmed();
    if (!jigLabel.isEmpty())
        out.jigLabel = jigLabel;
    const QString productLabel = ini.value(QStringLiteral("ProductLabel")).toString().trimmed();
    if (!productLabel.isEmpty())
        out.productLabel = productLabel;
    const QString usbLabel = ini.value(QStringLiteral("UsbLabel")).toString().trimmed();
    if (!usbLabel.isEmpty())
        out.usbLabel = usbLabel;
}

} // namespace

TestCaseSerialUiConfig TestCaseStore::loadStationSerialUiConfig(const QString& stationKey) {
    TestCaseSerialUiConfig out;
    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return out;

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (QFile::exists(profileFlow)) {
        QSettings profileIni(profileFlow, QSettings::IniFormat);
        applyTestCaseIniCodec(profileIni);
        profileIni.beginGroup(QStringLiteral("SerialUi"));
        readSerialUiFieldsFromGroup(profileIni, out);
        profileIni.endGroup();
        return out;
    }

    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(key));
    // 旧总流程 ini 可能嵌在 Station/xxx 下；若有 SerialUi 子组则读子组
    if (ini.childGroups().contains(QStringLiteral("SerialUi"))) {
        ini.beginGroup(QStringLiteral("SerialUi"));
        readSerialUiFieldsFromGroup(ini, out);
    ini.endGroup();
    } else {
        readSerialUiFieldsFromGroup(ini, out);
    }
    ini.endGroup();
    return out;
}

bool TestCaseStore::saveStationSerialUiConfig(const QString& stationKey, const TestCaseSerialUiConfig& config) {
    TestCasePaths::ensureRootDir();
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return false;
    ensureProfileDirectory(key, flowStationDisplayName(key), QString());

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("SerialUi"));
    profileIni.setValue(QStringLiteral("JigVisible"), config.jigVisible);
    profileIni.setValue(QStringLiteral("ProductVisible"), config.productVisible);
    profileIni.setValue(QStringLiteral("UsbVisible"), config.usbVisible);
    profileIni.setValue(QStringLiteral("JigLabel"), config.jigLabel);
    profileIni.setValue(QStringLiteral("ProductLabel"), config.productLabel);
    profileIni.setValue(QStringLiteral("UsbLabel"), config.usbLabel);
    profileIni.endGroup();
    syncTestCaseIni(profileIni, profileFlow);
    return true;
}

TestCaseDeviceSideConfig TestCaseStore::loadStationDeviceSideConfig(const QString& stationKey) {
    TestCaseDeviceSideConfig out;
    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return out;

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (!QFile::exists(profileFlow))
        return out;

    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("DeviceSide"));
    out.position = profileIni.value(QStringLiteral("Position")).toString().trimmed();
    bool ok = false;
    const int side = profileIni.value(QStringLiteral("SideId"), -1).toInt(&ok);
    if (ok && side >= 0 && side <= 2)
        out.sideId = side;
    profileIni.endGroup();
    return out;
}

bool TestCaseStore::saveStationDeviceSideConfig(const QString& stationKey, const TestCaseDeviceSideConfig& config) {
    TestCasePaths::ensureRootDir();
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return false;
    ensureProfileDirectory(key, flowStationDisplayName(key), QString());

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("DeviceSide"));
    profileIni.setValue(QStringLiteral("Position"), config.position.trimmed());
    if (config.sideId >= 0 && config.sideId <= 2)
        profileIni.setValue(QStringLiteral("SideId"), config.sideId);
    else
        profileIni.remove(QStringLiteral("SideId"));
    profileIni.endGroup();
    syncTestCaseIni(profileIni, profileFlow);
    return true;
}

QVector<TestFlowItemEntry> TestCaseStore::loadStationFlowItems(const QString& stationKey) {
    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed();

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (QFile::exists(profileFlow)) {
        QSettings profileIni(profileFlow, QSettings::IniFormat);
        applyTestCaseIniCodec(profileIni);
        profileIni.beginGroup(QStringLiteral("Flow"));
        if (profileIni.contains(QStringLiteral("Items"))) {
            const QVector<TestFlowItemEntry> entries = parseFlowItemsFromSettingsGroup(profileIni);
            profileIni.endGroup();
    return entries;
        }
        profileIni.endGroup();
    }

    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(key));
    const QVector<TestFlowItemEntry> entries = parseFlowItemsFromSettingsGroup(ini);
    ini.endGroup();
    return entries;
}

QVector<TestFlowItemEntry> TestCaseStore::loadStationFailFlowItems(const QString& stationKey) {
    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed();

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (QFile::exists(profileFlow)) {
        QSettings profileIni(profileFlow, QSettings::IniFormat);
        applyTestCaseIniCodec(profileIni);
        profileIni.beginGroup(QStringLiteral("Flow"));
        const QVector<TestFlowItemEntry> entries =
            parseNamedFlowItemList(profileIni, QStringLiteral("FailItems"), QStringLiteral("FailDisabledItems"));
        profileIni.endGroup();
        return entries;
    }

    QSettings ini(TestCasePaths::flowIniPath(), QSettings::IniFormat);
    applyTestCaseIniCodec(ini);
    ini.beginGroup(stationGroup(key));
    const QVector<TestFlowItemEntry> entries =
        parseNamedFlowItemList(ini, QStringLiteral("FailItems"), QStringLiteral("FailDisabledItems"));
    ini.endGroup();
    return entries;
}

bool TestCaseStore::saveStationFlowItems(const QString& stationKey, const QVector<TestFlowItemEntry>& items,
                                         bool stopFlowOnTestFail) {
    return saveStationFlowItems(stationKey, items, loadStationFailFlowItems(stationKey), stopFlowOnTestFail);
}

bool TestCaseStore::saveStationFlowItems(const QString& stationKey, const QVector<TestFlowItemEntry>& items,
                                         const QVector<TestFlowItemEntry>& failItems, bool stopFlowOnTestFail) {
    TestCasePaths::ensureRootDir();
    const QString key = stationKey.trimmed();
    ensureProfileDirectory(key, flowStationDisplayName(key), QString());

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("Flow"));
    writeFlowItemsToSettingsGroup(profileIni, items, failItems, stopFlowOnTestFail);
    profileIni.endGroup();
    syncTestCaseIni(profileIni, profileFlow);
    // 保存流程时把步骤库缺项补进本工站 profiles/.../steps/（与 loadCaseForStation 注释一致）
    migrateProfileStepOverridesForStation(key);
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

