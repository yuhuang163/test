#include "test_case_store.h"
#include "test_case_paths.h"
#include "test_case_catalog.h"
#include "test_case_gate_api.h"
#include "test_case_ini_param.h"
#include "test_case_send_dispatch.h"

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
#include <QTimer>
#include <qdebug.h>

#include "Abini.h"
#include "common_utils.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// ===================== TestCasePaths（依赖工站目录名） =====================

namespace TestCasePaths {

QString profileFolderName(const QString& stationKey) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return QString();
    QString folder = TestCaseStore::flowStationDisplayName(key);
    if (!isValidCaseFileName(folder, nullptr))
        folder = key;
    return folder;
}

QString profileDir(const QString& stationKey) {
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

QString profileMetaPath(const QString& stationKey) {
    return profileDir(stationKey) + QStringLiteral("/profile.ini");
}

QString profileFlowPath(const QString& stationKey) {
    return profileDir(stationKey) + QStringLiteral("/flow.ini");
}

QString profileStepOverridePath(const QString& stationKey, const QString& stepId) {
    return profileDir(stationKey) + QStringLiteral("/steps/") + stepId.trimmed() + QStringLiteral(".ini");
}

bool stepIniExistsForStation(const QString& stationKey, const QString& stepId) {
    const QString id = stepId.trimmed();
    if (id.isEmpty())
        return false;
    if (QFile::exists(stepLibraryPath(id)))
        return true;
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return false;
    return QFile::exists(profileStepOverridePath(key, id));
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

inline void ensureProfileDirectory(const QString& stationKey, const QString& displayName, const QString& createdFrom) {
    TestCaseStore::ensureProfileDirectory(stationKey, displayName, createdFrom);
}

} // namespace

void TestCaseStore::ensureProfileDirectory(const QString& stationKey, const QString& displayName, const QString& createdFrom) {
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
    QSettings meta(metaPath, QSettings::IniFormat);
    applyTestCaseIniCodec(meta);
    bool dirty = false;
    if (meta.value(QStringLiteral("Profile/StationKey")).toString().trimmed() != key) {
        meta.setValue(QStringLiteral("Profile/StationKey"), key);
        dirty = true;
    }
    if (!displayName.isEmpty()) {
        const QString trimmedName = displayName.trimmed();
        if (meta.value(QStringLiteral("Profile/DisplayName")).toString().trimmed() != trimmedName) {
            meta.setValue(QStringLiteral("Profile/DisplayName"), trimmedName);
            dirty = true;
        }
    }
    if (dirty)
        syncTestCaseIni(meta, metaPath);
}

static void resetCopiedProfileMeta(const QString& stationKey, const QString& displayName, const QString& createdFrom) {
    const QString metaPath = TestCasePaths::profileMetaPath(stationKey);
    QSettings meta(metaPath, QSettings::IniFormat);
    applyTestCaseIniCodec(meta);
    meta.setValue(QStringLiteral("Profile/StationKey"), stationKey);
    meta.setValue(QStringLiteral("Profile/DisplayName"), displayName);
    if (!createdFrom.isEmpty())
        meta.setValue(QStringLiteral("Profile/CreatedFrom"), createdFrom.trimmed());
    meta.setValue(QStringLiteral("Profile/CreatedAt"), QDateTime::currentDateTime().toString(Qt::ISODate));
    meta.setValue(QStringLiteral("Profile/ProfileVersion"), 1);
    meta.setValue(QStringLiteral("Profile/StepsLibraryVersion"), 1);
    syncTestCaseIni(meta, metaPath);
}

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

bool TestCaseStore::addFlowStation(const QString& displayName, QString* errorOut, const QString& preferredKey) {
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
    for (const TestFlowStationEntry& entry : catalog) {
        if (entry.displayName == name) {
            if (errorOut)
                *errorOut = QStringLiteral("工站名称已存在：%1").arg(name);
            return false;
        }
    }

    auto keyTakenByOtherName = [&](const QString& candidate) {
        const QString k = candidate.trimmed();
        if (k.isEmpty())
            return false;
        for (const TestFlowStationEntry& entry : catalog) {
            if (entry.key.compare(k, Qt::CaseInsensitive) == 0 && entry.displayName != name)
                return true;
        }
        return false;
    };

    QString key;
    const QString preferred = preferredKey.trimmed();
    if (!preferred.isEmpty()) {
        if (!isAsciiFlowStationKey(preferred)) {
            if (errorOut)
                *errorOut = QStringLiteral("工站编号格式无效：%1").arg(preferred);
            return false;
        }
        if (keyTakenByOtherName(preferred)) {
            if (errorOut)
                *errorOut = QStringLiteral("工站编号已被其它工站占用：%1").arg(preferred);
            return false;
        }
        key = preferred;
    } else {
        key = presetKey;
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
    }
    catalog.append({key, name});
    if (!saveFlowStationCatalog(catalog))
        return false;
    ensureProfileDirectory(key, name, QString());
    return true;
}

bool TestCaseStore::copyFlowStation(const QString& sourceStationKey, const QString& newDisplayName,
                                    const QVector<TestFlowItemEntry>& items, bool stopFlowOnTestFail,
                                    QString* outNewKey, QString* errorOut, const QString& preferredNewKey) {
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

    if (!addFlowStation(newDisplayName, errorOut, preferredNewKey))
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
        resetCopiedProfileMeta(newKey, newDisplayName.trimmed(), src);
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

bool TestCaseStore::alignFlowStationKey(const QString& displayName, const QString& cloudKey, QString* errorOut) {
    const QString name = displayName.trimmed();
    const QString newKey = cloudKey.trimmed();
    if (name.isEmpty() || newKey.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("工站名称或编号无效");
        return false;
    }
    if (!isAsciiFlowStationKey(newKey)) {
        if (errorOut)
            *errorOut = QStringLiteral("云端工站编号格式无效：%1").arg(newKey);
        return false;
    }

    QVector<TestFlowStationEntry> catalog = loadFlowStationCatalog();
    QString oldKey;
    for (const TestFlowStationEntry& entry : catalog) {
        if (entry.displayName == name) {
            oldKey = entry.key.trimmed();
            break;
        }
    }
    if (oldKey.compare(newKey, Qt::CaseInsensitive) == 0) {
        ensureProfileDirectory(newKey, name, QString());
        const QString metaPath = TestCasePaths::profileMetaPath(newKey);
        if (QFile::exists(metaPath)) {
            QSettings meta(metaPath, QSettings::IniFormat);
            applyTestCaseIniCodec(meta);
            if (meta.value(QStringLiteral("Profile/StationKey")).toString().trimmed() != newKey) {
                meta.setValue(QStringLiteral("Profile/StationKey"), newKey);
                meta.setValue(QStringLiteral("Profile/DisplayName"), name);
                syncTestCaseIni(meta, metaPath);
            }
        }
        return true;
    }

    for (const TestFlowStationEntry& entry : catalog) {
        if (entry.key.compare(newKey, Qt::CaseInsensitive) == 0 && entry.displayName != name) {
            if (errorOut) {
                *errorOut = QStringLiteral("云端编号 %1 已被本机工站「%2」占用，无法对齐")
                                .arg(newKey, entry.displayName);
            }
            return false;
        }
    }

    QVector<TestFlowStationEntry> next;
    next.reserve(catalog.size());
    for (const TestFlowStationEntry& entry : catalog) {
        const bool sameName = entry.displayName == name;
        const bool sameOldKey =
            !oldKey.isEmpty() && entry.key.compare(oldKey, Qt::CaseInsensitive) == 0;
        if (sameName || sameOldKey)
            continue;
        if (entry.key.compare(newKey, Qt::CaseInsensitive) == 0)
            continue;
        next.append(entry);
    }
    next.append({newKey, name});
    if (!saveFlowStationCatalog(next))
        return false;

    if (!oldKey.isEmpty())
        migrateFlowStationIniData(oldKey, newKey, name);

    ensureProfileDirectory(newKey, name, QString());
    const QString metaPath = TestCasePaths::profileMetaPath(newKey);
    QSettings meta(metaPath, QSettings::IniFormat);
    applyTestCaseIniCodec(meta);
    meta.setValue(QStringLiteral("Profile/StationKey"), newKey);
    meta.setValue(QStringLiteral("Profile/DisplayName"), name);
    syncTestCaseIni(meta, metaPath);

    const QString selected = loadSelectedFlowStationKey().trimmed();
    const QString selectedName = loadSelectedFlowStationName().trimmed();
    if ((!oldKey.isEmpty() && selected.compare(oldKey, Qt::CaseInsensitive) == 0) || selectedName == name)
        saveSelectedFlowStation(newKey, name);
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

void saveMultiGatesToIni(QSettings& ini, const TestCaseDefinition& def);

/** 多项卡控：优先 ItemN_Field；现场旧包是 Qt 数组键 1\\Field（QSettings 为 Gate/1/Field）。 */
void loadMultiGatesFromIni(QSettings& ini, TestCaseDefinition& out) {
    out.gates.clear();
    const QString reportType = out.gate.reportType;
    const int count = ini.value(QStringLiteral("Gate/Count"), 0).toInt();
    bool usedLegacyArray = false;
    auto itemVal = [&](int i, const QString& name, const QVariant& defVal) {
        const QString modern = QStringLiteral("Gate/Item%1_%2").arg(i).arg(name);
        if (ini.contains(modern))
            return ini.value(modern, defVal);
        const QString legacy = QStringLiteral("Gate/%1/%2").arg(i).arg(name);
        if (ini.contains(legacy))
            usedLegacyArray = true;
        return ini.value(legacy, defVal);
    };
    for (int i = 1; i <= count; ++i) {
        TestCaseGate g;
        g.reportType = reportType;
        g.field = itemVal(i, QStringLiteral("Field"), QVariant()).toString().trimmed();
        g.enabled = itemVal(i, QStringLiteral("Enabled"), true).toBool();
        g.op = gateOpFromString(itemVal(i, QStringLiteral("Op"), QStringLiteral("range")).toString());
        g.expected = itemVal(i, QStringLiteral("Expected"), QVariant()).toString().trimmed();
        g.low = itemVal(i, QStringLiteral("Low"), 0).toDouble();
        g.high = itemVal(i, QStringLiteral("High"), 0).toDouble();
        g.lowSettingsKey = itemVal(i, QStringLiteral("LowSettingsKey"), QVariant()).toString();
        g.highSettingsKey = itemVal(i, QStringLiteral("HighSettingsKey"), QVariant()).toString();
        g.expectedSettingsKey = itemVal(i, QStringLiteral("ExpectedSettingsKey"), QVariant()).toString();
        if (!g.field.isEmpty() && g.field != QLatin1String("multi"))
            out.gates.append(g);
    }
    if (!out.gates.isEmpty()) {
        if (usedLegacyArray) {
            saveMultiGatesToIni(ini, out);
            ini.sync();
            if (ini.status() == QSettings::NoError)
                qDebug() << QStringLiteral("卡控已从旧数组格式升级：") << ini.fileName();
        }
        return;
    }

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
        || reportType == QLatin1String("ProtocolJieliBtBoxData")
        || reportType == QLatin1String("ProtocolScreenInspectData")
        || reportType == QLatin1String("ProtocolLedInspectData")
        || reportType == QLatin1String("ProtocolAudioExpertData")
        || reportType == QLatin1String("ProtocolPeriphStateData");
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
    } else if (channelIni.compare(QStringLiteral("Software"), Qt::CaseInsensitive) == 0
               || channelIni.compare(QStringLiteral("软件通信"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Software;
        def.send.fixtureProtocol = TestCaseFixtureProtocol::AudioExpert;
    } else if (channelIni.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Modbus;
    } else if (channelIni.compare(QStringLiteral("Scpi"), Qt::CaseInsensitive) == 0) {
        def.send.channel = TestCaseSendChannel::Scpi;
    } else if (channelIni.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0) {
        // 兼容旧 Channel=UsbCamera：并入治具通信（历史默认 GigE）
        def.send.channel = TestCaseSendChannel::Fixture;
        def.send.fixtureProtocol = TestCaseFixtureProtocol::GigECamera;
    }
}

void applySendProtocolIniText(const QString& protocolIni, TestCaseDefinition& def) {
    if (def.send.channel == TestCaseSendChannel::Fixture || def.send.channel == TestCaseSendChannel::Software)
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
    // Name / DisplayName 分开：不要再用 DisplayName 覆盖 Name（否则编辑框看不到真实 Name）
    if (!nameInIni.isEmpty())
        out.meta.name = nameInIni;
    else
        out.meta.name = stepId.trimmed();
    out.meta.displayName = !displayInIni.isEmpty() ? displayInIni : out.meta.name;
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
    } else if (channelIni.compare(QStringLiteral("Software"), Qt::CaseInsensitive) == 0
               || channelIni.compare(QStringLiteral("软件通信"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Software;
        out.send.fixtureProtocol = TestCaseFixtureProtocol::AudioExpert;
    } else if (channelIni.compare(QStringLiteral("Modbus"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Modbus;
    } else if (channelIni.compare(QStringLiteral("Scpi"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Scpi;
    } else if (channelIni.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0) {
        out.send.channel = TestCaseSendChannel::Fixture;
        out.send.fixtureProtocol = TestCaseFixtureProtocol::GigECamera;
    } else {
        TestCaseSendDispatch::inferChannelFromDeviceCmd(out.send.deviceCmd, out.send.channel, out.send.fixtureProtocol);
    }
    // 旧：治具+AudioExpert → 软件通信
    if (out.send.channel == TestCaseSendChannel::Fixture
        && out.send.fixtureProtocol == TestCaseFixtureProtocol::AudioExpert) {
        out.send.channel = TestCaseSendChannel::Software;
    }
    TestCaseSendDispatch::normalizeLegacyUsbCameraSend(out.send);
    TestCaseSendDispatch::loadSendParamFromIni(ini, out.send, true);

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
        if (cmdEnumFromName(DeviceCmdCatalog::catalog(), out.send.deviceCmd, legacyCmd)) {
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
    if (hookUsesGenericSendParamMap(out))
        mergeSendParamMapInto(out.send.param, readSendParamMap(ini));

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
    // 工站已写 HookId（含主动 Enabled=false）一律不改，避免「关掉特殊步骤改成 Dongle 指令」保存后再打开又变回 Hook
    if (!def.hook.hookId.isEmpty())
        return;
    if (def.hook.enabled)
        return;
    if (!isLegacyHookDeviceCmdPlaceholder(def.send.deviceCmd))
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
        def.hook.enabled = library.hook.enabled;
    }
}

void supplementMissingHookSendParamsFromLibrary(const QString& stepId, TestCaseDefinition& def) {
    if (!hookUsesGenericSendParamMap(def))
        return;
    QVariantMap current;
    if (def.send.param.canConvert<QVariantMap>())
        current = def.send.param.toMap();

    TestCaseDefinition library;
    const QString libraryPath = TestCasePaths::stepLibraryPath(stepId);
    const QString legacyPath = TestCasePaths::caseIniPath(stepId);
    if (!loadCaseDefinitionFromIniFile(libraryPath, stepId, library)
        && !loadCaseDefinitionFromIniFile(legacyPath, stepId, library)) {
        return;
    }
    if (!library.send.param.canConvert<QVariantMap>())
        return;
    const QVariantMap defaults = library.send.param.toMap();
    for (auto it = defaults.cbegin(); it != defaults.cend(); ++it) {
        if (!current.contains(it.key()))
            current.insert(it.key(), it.value());
    }
    def.send.param = current;
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
                       || def.send.channel == TestCaseSendChannel::ProductSerial
                       || (def.send.channel == TestCaseSendChannel::Fixture
                           && (FixturePcbaCmdCatalog::isScreenCameraFixture(def.send.fixtureProtocol)
                               || def.send.fixtureProtocol == TestCaseFixtureProtocol::VesLight))) {
                // 编辑/存档侧保持 JsonMap；DeviceCmdManifest::normalizeSendParam（如 Sn→DeviceSnPayload）仅在下发时做
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
            TestCaseSendDispatch::loadSendParamFromIni(overlay, def.send);
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

void migrateOneStationProfile(const QString& stationKey, const QString& displayName) {
    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return;
    migrateProfileDirToDisplayName(key);
    ensureProfileDirectory(key, displayName, QString());
    syncProfileFlowFromLegacyIni(key);
    migrateProfileStepOverridesForStation(key);
}

void migrateAllStationProfiles() {
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
        // 已有 flow.ini 不再 QSettings 扫一遍，避免 Qt 文件缓存挤掉当前工站、保存设置时把中文写成乱码
        if (QFile::exists(TestCasePaths::profileFlowPath(key)))
            continue;
        migrateOneStationProfile(key, TestCaseStore::flowStationDisplayName(key));
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

static bool g_selectedProfileReady = false;
static bool g_profileLayoutSynced = false;

void ensureFilesystemLayoutOnce() {
    TestCasePaths::ensureRootDir();
    migrateLegacyFlatInisToStepLibrary();

    if (!g_selectedProfileReady) {
        g_selectedProfileReady = true;
        registerFlowStationsFromProfileDirs();
        const QString selectedKey = TestCaseStore::loadSelectedFlowStationKey().trimmed();
        if (!selectedKey.isEmpty())
            migrateOneStationProfile(selectedKey, TestCaseStore::flowStationDisplayName(selectedKey));
        if (!g_profileLayoutSynced && QCoreApplication::instance()) {
            QTimer::singleShot(0, QCoreApplication::instance(), []() {
                if (g_profileLayoutSynced)
                    return;
                g_profileLayoutSynced = true;
                migrateAllStationProfiles();
            });
        }
    }

    if (g_profileLayoutSynced)
        return;
    // 启动阶段当前工站已可用，其余工站等事件循环空闲再迁；无 QApp 时（单测）立刻做完
    if (QCoreApplication::instance())
        return;
    g_profileLayoutSynced = true;
    migrateAllStationProfiles();
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
        TestCaseSendDispatch::writeSendParamToIni(ini, def.send);
        if (def.send.channel == TestCaseSendChannel::Product || def.send.channel == TestCaseSendChannel::ProductSerial)
            writeGenericHookSendParamMap(ini, def);
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

    ini.setValue(QStringLiteral("Meta/DisplayName"),
                 def.meta.displayName.trimmed().isEmpty() ? def.meta.name : def.meta.displayName.trimmed());
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
    else if (def.send.channel == TestCaseSendChannel::Software)
        channelStr = QStringLiteral("Software");
    else if (def.send.channel == TestCaseSendChannel::Modbus)
        channelStr = QStringLiteral("Modbus");
    else if (def.send.channel == TestCaseSendChannel::Scpi)
        channelStr = QStringLiteral("Scpi");
    ini.setValue(QStringLiteral("Send/Channel"), channelStr);
    if (def.send.channel == TestCaseSendChannel::Product || def.send.channel == TestCaseSendChannel::ProductSerial)
        ini.setValue(QStringLiteral("Send/Protocol"), DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol));
    else if (def.send.channel == TestCaseSendChannel::Software) {
        ini.setValue(QStringLiteral("Send/Protocol"),
                     FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::AudioExpert));
        ini.setValue(QStringLiteral("Send/Device"),
                     def.send.device.isEmpty() ? QStringLiteral("AUDIO_EXPERT") : def.send.device);
    } else if (def.send.channel == TestCaseSendChannel::Fixture) {
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
                         def.send.device.isEmpty() ? QStringLiteral("USB_WEBCAM") : def.send.device);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::GigECamera) {
            ini.setValue(QStringLiteral("Send/Device"),
                         def.send.device.isEmpty() ? QStringLiteral("GIGE_CAMERA") : def.send.device);
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::VesLight) {
            ini.setValue(QStringLiteral("Send/Device"),
                         def.send.device.isEmpty() ? QStringLiteral("VES") : def.send.device);
        }
    }
    ini.setValue(QStringLiteral("Send/DeviceCmd"), def.send.deviceCmd);
    if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        if (!def.send.device.isEmpty())
            ini.setValue(QStringLiteral("Send/Device"), def.send.device);
    }
    TestCaseSendDispatch::writeSendParamToIni(ini, def.send);
    if (def.send.channel == TestCaseSendChannel::Product || def.send.channel == TestCaseSendChannel::ProductSerial)
        writeGenericHookSendParamMap(ini, def);

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
    // Field=multi 只是多项卡控占位，不能当成可评字段（否则会报「无法从上报数据读取字段」）
    if (def.gate.enabled && def.gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) != 0
        && !def.gate.field.trimmed().isEmpty())
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
        if (!g.enabled)
            continue;
        // 过滤占位/空字段，避免单项回退误评 multi
        if (g.field.trimmed().isEmpty()
            || g.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0)
            continue;
        active.append(g);
    }
    return active;
}

bool TestCaseStore::usesMultiFieldGates(const TestCaseDefinition& def) {
    return def.gates.size() > 1
        || def.gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0
        || (!def.gates.isEmpty() && isMultiFieldGateReportType(def.gate.reportType));
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
    registerCloudItemNameAlias(&map, QStringLiteral("BT_MAC"), QStringLiteral("MAC地址"));
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

int TestCaseStore::loadStationProfileVersion(const QString& stationKey) {
    QString key = stationKey.trimmed();
    if (key.isEmpty())
        key = loadSelectedFlowStationKey();
    if (key.isEmpty())
        return 0;
    const QString metaPath = TestCasePaths::profileMetaPath(key);
    if (!QFile::exists(metaPath))
        return 0;
    QSettings meta(metaPath, QSettings::IniFormat);
    applyTestCaseIniCodec(meta);
    return meta.value(QStringLiteral("Profile/ProfileVersion"), 0).toInt();
}

bool TestCaseStore::loadStationSkipDefaultPreInspection(const QString& stationKey) {
    QString key = stationKey.trimmed();
    if (key.isEmpty())
        key = loadSelectedFlowStationKey();
    if (key.isEmpty())
        return false;
    const QString metaPath = TestCasePaths::profileMetaPath(key);
    if (!QFile::exists(metaPath))
        return false;
    QSettings meta(metaPath, QSettings::IniFormat);
    applyTestCaseIniCodec(meta);
    return meta.value(QStringLiteral("StationCustom/SkipDefaultPreInspection"), false).toBool()
        || meta.value(QStringLiteral("Station/SkipDefaultPreInspection"), false).toBool()
        || meta.value(QStringLiteral("Profile/SkipDefaultPreInspection"), false).toBool();
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
    if (ini.contains(QStringLiteral("DongleVisible")))
        out.dongleVisible = ini.value(QStringLiteral("DongleVisible"), true).toBool();
    if (ini.contains(QStringLiteral("JigVisible")))
        out.jigVisible = ini.value(QStringLiteral("JigVisible"), true).toBool();
    if (ini.contains(QStringLiteral("ProductVisible")))
        out.productVisible = ini.value(QStringLiteral("ProductVisible"), true).toBool();
    if (ini.contains(QStringLiteral("UsbVisible")))
        out.usbVisible = ini.value(QStringLiteral("UsbVisible"), true).toBool();
    if (ini.contains(QStringLiteral("DongleBaudRate")))
        out.dongleBaudRate = ini.value(QStringLiteral("DongleBaudRate"), 921600).toInt();
    const QString jigLabel = ini.value(QStringLiteral("JigLabel")).toString().trimmed();
    if (!jigLabel.isEmpty())
        out.jigLabel = jigLabel;
    const QString productLabel = ini.value(QStringLiteral("ProductLabel")).toString().trimmed();
    if (!productLabel.isEmpty())
        out.productLabel = productLabel;
    if (ini.contains(QStringLiteral("JigBaudRate")))
        out.jigBaudRate = ini.value(QStringLiteral("JigBaudRate"), 115200).toInt();
    if (ini.contains(QStringLiteral("ProductBaudRate")))
        out.productBaudRate = ini.value(QStringLiteral("ProductBaudRate"), 115200).toInt();
    const QString usbLabel = ini.value(QStringLiteral("UsbLabel")).toString().trimmed();
    if (!usbLabel.isEmpty())
        out.usbLabel = usbLabel;
    if (ini.contains(QStringLiteral("UsbBaudRate")))
        out.usbBaudRate = ini.value(QStringLiteral("UsbBaudRate"), 115200).toInt();
    const QString fixtureMenuLabel = ini.value(QStringLiteral("FixtureMenuLabel")).toString().trimmed();
    if (!fixtureMenuLabel.isEmpty())
        out.fixtureMenuLabel = fixtureMenuLabel;
    if (ini.contains(QStringLiteral("FixtureBaudRate")))
        out.fixtureBaudRate = ini.value(QStringLiteral("FixtureBaudRate"), 9600).toInt();
    const QString plcFixtureMenuLabel = ini.value(QStringLiteral("PlcFixtureMenuLabel")).toString().trimmed();
    if (!plcFixtureMenuLabel.isEmpty())
        out.plcFixtureMenuLabel = plcFixtureMenuLabel;
    if (ini.contains(QStringLiteral("PlcFixtureBaudRate")))
        out.plcFixtureBaudRate = ini.value(QStringLiteral("PlcFixtureBaudRate"), 9600).toInt();
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
    profileIni.setValue(QStringLiteral("DongleVisible"), config.dongleVisible);
    profileIni.setValue(QStringLiteral("DongleBaudRate"), config.dongleBaudRate > 0 ? config.dongleBaudRate : 921600);
    profileIni.setValue(QStringLiteral("JigVisible"), config.jigVisible);
    profileIni.setValue(QStringLiteral("JigLabel"), config.jigLabel);
    profileIni.setValue(QStringLiteral("JigBaudRate"), config.jigBaudRate > 0 ? config.jigBaudRate : 115200);
    profileIni.setValue(QStringLiteral("ProductVisible"), config.productVisible);
    profileIni.setValue(QStringLiteral("ProductLabel"), config.productLabel);
    profileIni.setValue(QStringLiteral("ProductBaudRate"), config.productBaudRate > 0 ? config.productBaudRate : 115200);
    profileIni.setValue(QStringLiteral("UsbVisible"), config.usbVisible);
    profileIni.setValue(QStringLiteral("UsbLabel"), config.usbLabel);
    profileIni.setValue(QStringLiteral("UsbBaudRate"), config.usbBaudRate > 0 ? config.usbBaudRate : 115200);
    profileIni.setValue(QStringLiteral("FixtureMenuLabel"), config.fixtureMenuLabel);
    profileIni.setValue(QStringLiteral("FixtureBaudRate"), config.fixtureBaudRate > 0 ? config.fixtureBaudRate : 9600);
    profileIni.setValue(QStringLiteral("PlcFixtureMenuLabel"), config.plcFixtureMenuLabel);
    profileIni.setValue(QStringLiteral("PlcFixtureBaudRate"),
                        config.plcFixtureBaudRate > 0 ? config.plcFixtureBaudRate : 9600);
    profileIni.endGroup();
    syncTestCaseIni(profileIni, profileFlow);
    return true;
}

static QStringList splitStationBindingList(const QString& text) {
    QStringList out;
    const QStringList parts = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (QString p : parts) {
        p = p.trimmed();
        if (!p.isEmpty() && !out.contains(p))
            out.append(p);
    }
    return out;
}

static void appendUniqueBindingToken(QStringList& list, const QString& token) {
    const QString t = token.trimmed();
    if (!t.isEmpty() && !list.contains(t))
        list.append(t);
}

TestCaseStationBindings TestCaseStore::inferStationBindingsDefaults(const QString& stationDisplayName) {
    TestCaseStationBindings out;
    out.configured = false;
    out.enableProduct = true;
    out.enableDongle = true;
    out.enableCloud = true;
    out.enableProductSerial = false;
    const QString n = stationDisplayName.trimmed();

    auto addProductProto = [&out](TestCaseProductProtocol protocol) {
        appendUniqueBindingToken(out.productProtocols, DeviceCmdCatalog::productProtocolToIni(protocol));
    };

    if (n.contains(QStringLiteral("Air1"), Qt::CaseInsensitive)
        || n.contains(QStringLiteral("AIR1"), Qt::CaseInsensitive)
        || n.contains(QStringLiteral("Smart"), Qt::CaseInsensitive)) {
        addProductProto(TestCaseProductProtocol::Qroot2);
        addProductProto(TestCaseProductProtocol::Qroot);
        addProductProto(TestCaseProductProtocol::Qfctp);
    } else if (n.contains(QStringLiteral("M8"), Qt::CaseInsensitive)
               || n.contains(QStringLiteral("M5"), Qt::CaseInsensitive)) {
        addProductProto(TestCaseProductProtocol::Qroot);
        addProductProto(TestCaseProductProtocol::Qfctp);
        if (n.contains(QStringLiteral("吸"), Qt::CaseInsensitive))
            appendUniqueBindingToken(out.fixtures,
                                     FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::Xwd));
    } else if (n.contains(QStringLiteral("Wellness"), Qt::CaseInsensitive)
               || n.contains(QStringLiteral("W1"), Qt::CaseInsensitive)) {
        addProductProto(TestCaseProductProtocol::Qroot);
        addProductProto(TestCaseProductProtocol::Qfctp);
        appendUniqueBindingToken(out.scpiDevices,
                                 ScpiPeriphCmdCatalog::deviceToIni(ScpiDeviceRoute::Agilent66319d));
        appendUniqueBindingToken(out.modbusDevices,
                                 ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::XinjiePlcRtu));
    } else if (n.contains(QStringLiteral("V3"), Qt::CaseInsensitive)) {
        addProductProto(TestCaseProductProtocol::Qfctp);
        appendUniqueBindingToken(out.fixtures,
                                 FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::VesLight));
    } else {
        addProductProto(TestCaseProductProtocol::Qfctp);
    }

    if (!out.productProtocols.isEmpty())
        out.defaultTarget = QStringLiteral("product:") + out.productProtocols.constFirst();
    return out;
}

static bool stationBindingsHasCapabilities(const TestCaseStationBindings& b) {
    return b.enableDongle || b.enableCloud || b.enableProduct || b.enableProductSerial
        || !b.productProtocols.isEmpty() || !b.fixtures.isEmpty() || !b.modbusDevices.isEmpty()
        || !b.scpiDevices.isEmpty();
}

TestCaseStationBindings TestCaseStore::inferStationBindingsFromFlowSteps(const QString& stationKey) {
    TestCaseStationBindings out;
    out.configured = false;
    out.enableProduct = false;
    out.enableProductSerial = false;
    out.enableDongle = false;
    out.enableCloud = false;

    const QString key = stationKey.trimmed();
    if (key.isEmpty())
        return out;

    QVector<TestFlowItemEntry> entries = loadStationFlowItems(key);
    const QVector<TestFlowItemEntry> failEntries = loadStationFailFlowItems(key);
    for (const TestFlowItemEntry& e : failEntries) {
        bool exists = false;
        for (const TestFlowItemEntry& m : entries) {
            if (m.caseName == e.caseName) {
                exists = true;
                break;
            }
        }
        if (!exists)
            entries.append(e);
    }

    QString firstProductTarget;
    QString firstAnyTarget;
    for (const TestFlowItemEntry& entry : entries) {
        if (entry.caseName.trimmed().isEmpty())
            continue;
        TestCaseDefinition def;
        if (!loadCaseForStation(key, entry.caseName, def, nullptr))
            continue;

        switch (def.send.channel) {
        case TestCaseSendChannel::Product: {
            out.enableProduct = true;
            const QString proto = DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol);
            appendUniqueBindingToken(out.productProtocols, proto);
            if (firstProductTarget.isEmpty())
                firstProductTarget = QStringLiteral("product:") + proto;
            if (firstAnyTarget.isEmpty())
                firstAnyTarget = firstProductTarget;
            break;
        }
        case TestCaseSendChannel::ProductSerial: {
            out.enableProductSerial = true;
            const QString proto = DeviceCmdCatalog::productProtocolToIni(def.send.productProtocol);
            appendUniqueBindingToken(out.productProtocols, proto);
            if (firstAnyTarget.isEmpty())
                firstAnyTarget = QStringLiteral("product_serial:") + proto;
            break;
        }
        case TestCaseSendChannel::Dongle:
            out.enableDongle = true;
            if (firstAnyTarget.isEmpty())
                firstAnyTarget = QStringLiteral("dongle");
            break;
        case TestCaseSendChannel::Cloud:
            out.enableCloud = true;
            if (firstAnyTarget.isEmpty())
                firstAnyTarget = QStringLiteral("cloud");
            break;
        case TestCaseSendChannel::Fixture: {
            const QString fix = FixturePcbaCmdCatalog::fixtureProtocolToIni(def.send.fixtureProtocol);
            appendUniqueBindingToken(out.fixtures, fix);
            if (firstAnyTarget.isEmpty())
                firstAnyTarget = QStringLiteral("fixture:") + fix;
            break;
        }
        case TestCaseSendChannel::Modbus: {
            const QString dev = def.send.device.trimmed();
            if (!dev.isEmpty()) {
                appendUniqueBindingToken(out.modbusDevices, dev);
                if (firstAnyTarget.isEmpty())
                    firstAnyTarget = QStringLiteral("modbus:") + dev;
            }
            break;
        }
        case TestCaseSendChannel::Scpi: {
            const QString dev = def.send.device.trimmed();
            if (!dev.isEmpty()) {
                appendUniqueBindingToken(out.scpiDevices, dev);
                if (firstAnyTarget.isEmpty())
                    firstAnyTarget = QStringLiteral("scpi:") + dev;
            }
            break;
        }
        }
    }

    if (!firstProductTarget.isEmpty())
        out.defaultTarget = firstProductTarget;
    else
        out.defaultTarget = firstAnyTarget;
    return out;
}

TestCaseStationBindings TestCaseStore::loadStationBindings(const QString& stationKey) {
    const QString key = stationKey.trimmed().isEmpty() ? resolveActiveFlowStationKey() : stationKey.trimmed();
    TestCaseStationBindings nameDefaults = inferStationBindingsDefaults(flowStationDisplayName(key));
    if (key.isEmpty())
        return nameDefaults;

    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString profileFlow = TestCasePaths::profileFlowPath(key);

    auto tryAutoBindFromSteps = [key]() -> TestCaseStationBindings {
        TestCaseStationBindings fromSteps = inferStationBindingsFromFlowSteps(key);
        if (!stationBindingsHasCapabilities(fromSteps))
            return {};
        // 产线存量工站：按现有步骤自动落盘，免手工逐站重绑
        fromSteps.configured = true;
        saveStationBindings(key, fromSteps);
        fromSteps.configured = true;
        return fromSteps;
    };

    if (!QFile::exists(profileFlow)) {
        TestCaseStationBindings migrated = tryAutoBindFromSteps();
        if (stationBindingsHasCapabilities(migrated))
            return migrated;
        return nameDefaults;
    }

    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("StationBindings"));
    const bool hasConfiguredKey = profileIni.contains(QStringLiteral("Configured"));
    const bool hasLegacyOrData =
        profileIni.contains(QStringLiteral("ProductProtocols"))
        || profileIni.contains(QStringLiteral("ProductProtocol"))
        || profileIni.contains(QStringLiteral("Fixtures")) || profileIni.contains(QStringLiteral("ScpiDevices"))
        || profileIni.contains(QStringLiteral("ModbusDevices")) || profileIni.contains(QStringLiteral("DefaultTarget"))
        || profileIni.contains(QStringLiteral("EnableProduct")) || profileIni.contains(QStringLiteral("EnableDongle"));
    if (!hasConfiguredKey && !hasLegacyOrData) {
        profileIni.endGroup();
        TestCaseStationBindings migrated = tryAutoBindFromSteps();
        if (stationBindingsHasCapabilities(migrated))
            return migrated;
        return nameDefaults;
    }

    TestCaseStationBindings out = nameDefaults;
    out.configured = profileIni.value(QStringLiteral("Configured"), false).toBool();
    if (profileIni.contains(QStringLiteral("ProductProtocols")))
        out.productProtocols = splitStationBindingList(profileIni.value(QStringLiteral("ProductProtocols")).toString());
    else if (profileIni.contains(QStringLiteral("ProductProtocol")))
        out.productProtocols =
            splitStationBindingList(profileIni.value(QStringLiteral("ProductProtocol")).toString());
    out.enableProduct = profileIni.value(QStringLiteral("EnableProduct"), out.enableProduct).toBool();
    out.enableProductSerial =
        profileIni.value(QStringLiteral("EnableProductSerial"), out.enableProductSerial).toBool();
    out.enableDongle = profileIni.value(QStringLiteral("EnableDongle"), out.enableDongle).toBool();
    out.enableCloud = profileIni.value(QStringLiteral("EnableCloud"), out.enableCloud).toBool();
    out.fixtures = splitStationBindingList(profileIni.value(QStringLiteral("Fixtures")).toString());
    // 旧 Fixtures=USB_CAMERA 表示合一设备（默认 GigE）→ 归一为 GIGE_CAMERA
    for (QString& fix : out.fixtures) {
        if (fix.compare(QStringLiteral("USB_CAMERA"), Qt::CaseInsensitive) == 0
            || fix.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0) {
            fix = FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::GigECamera);
        }
    }
    out.modbusDevices = splitStationBindingList(profileIni.value(QStringLiteral("ModbusDevices")).toString());
    out.scpiDevices = splitStationBindingList(profileIni.value(QStringLiteral("ScpiDevices")).toString());
    out.defaultTarget = profileIni.value(QStringLiteral("DefaultTarget")).toString().trimmed();
    if (out.defaultTarget.compare(QStringLiteral("fixture:USB_CAMERA"), Qt::CaseInsensitive) == 0
        || out.defaultTarget.compare(QStringLiteral("fixture:UsbCamera"), Qt::CaseInsensitive) == 0) {
        out.defaultTarget = QStringLiteral("fixture:")
            + FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol::GigECamera);
    }
    profileIni.endGroup();

    // 未确认绑定或内容空洞：用步骤扫描补齐并落盘（工程师已手改且 Configured=true 则不覆盖）
    if (!out.configured || !stationBindingsHasCapabilities(out)) {
        TestCaseStationBindings migrated = tryAutoBindFromSteps();
        if (stationBindingsHasCapabilities(migrated))
            return migrated;
    }

    if (out.productProtocols.isEmpty() && out.configured)
        out.productProtocols = nameDefaults.productProtocols;
    if (out.defaultTarget.isEmpty() && !out.productProtocols.isEmpty())
        out.defaultTarget = QStringLiteral("product:") + out.productProtocols.constFirst();
    return out;
}

bool TestCaseStore::saveStationBindings(const QString& stationKey, const TestCaseStationBindings& config) {
    TestCasePaths::ensureRootDir();
    const QString key = stationKey.trimmed().isEmpty() ? resolveActiveFlowStationKey() : stationKey.trimmed();
    if (key.isEmpty())
        return false;
    ensureProfileDirectory(key, flowStationDisplayName(key), QString());

    TestCaseStationBindings toSave = config;
    toSave.configured = true;
    if (toSave.defaultTarget.trimmed().isEmpty() && !toSave.productProtocols.isEmpty())
        toSave.defaultTarget = QStringLiteral("product:") + toSave.productProtocols.constFirst();

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("StationBindings"));
    profileIni.setValue(QStringLiteral("Configured"), true);
    profileIni.setValue(QStringLiteral("ProductProtocols"), toSave.productProtocols.join(QLatin1Char(',')));
    profileIni.setValue(QStringLiteral("EnableProduct"), toSave.enableProduct);
    profileIni.setValue(QStringLiteral("EnableProductSerial"), toSave.enableProductSerial);
    profileIni.setValue(QStringLiteral("EnableDongle"), toSave.enableDongle);
    profileIni.setValue(QStringLiteral("EnableCloud"), toSave.enableCloud);
    profileIni.setValue(QStringLiteral("Fixtures"), toSave.fixtures.join(QLatin1Char(',')));
    profileIni.setValue(QStringLiteral("ModbusDevices"), toSave.modbusDevices.join(QLatin1Char(',')));
    profileIni.setValue(QStringLiteral("ScpiDevices"), toSave.scpiDevices.join(QLatin1Char(',')));
    profileIni.setValue(QStringLiteral("DefaultTarget"), toSave.defaultTarget.trimmed());
    profileIni.remove(QStringLiteral("ProductProtocol"));
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

QString TestCaseStore::resolveActiveFlowStationKey() {
    QString stationKey = resolveFlowStationKey(loadSelectedFlowStationKey());
    if (stationKey.isEmpty() || loadStationFlowItems(stationKey).isEmpty()) {
        const QString byName = resolveFlowStationKey(loadSelectedFlowStationName());
        if (!byName.isEmpty())
            stationKey = byName;
    }
    if (stationKey.isEmpty())
        stationKey = QStringLiteral("default");
    return stationKey;
}

TestCaseLedInspectConfig TestCaseStore::loadStationLedInspectConfig(const QString& stationKey) {
    TestCaseLedInspectConfig out;
    out.ledCount = qBound(1, SETTINGS.value(QStringLiteral("LedInspect/LedCount"), 4).toInt(), 4);
    out.minLuma = SETTINGS.value(QStringLiteral("LedInspect/MinLuma"), 80).toInt();
    out.mode = SETTINGS.value(QStringLiteral("LedInspect/Mode")).toString().trimmed();
    out.mode1 = SETTINGS.value(QStringLiteral("LedInspect/Mode1")).toString().trimmed();
    out.mode2 = SETTINGS.value(QStringLiteral("LedInspect/Mode2")).toString().trimmed();
    out.mode3 = SETTINGS.value(QStringLiteral("LedInspect/Mode3")).toString().trimmed();
    out.mode4 = SETTINGS.value(QStringLiteral("LedInspect/Mode4")).toString().trimmed();
    out.roi1 = SETTINGS.value(QStringLiteral("LedInspect/Roi1")).toString().trimmed();
    out.roi2 = SETTINGS.value(QStringLiteral("LedInspect/Roi2")).toString().trimmed();
    out.roi3 = SETTINGS.value(QStringLiteral("LedInspect/Roi3")).toString().trimmed();
    out.roi4 = SETTINGS.value(QStringLiteral("LedInspect/Roi4")).toString().trimmed();

    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed().isEmpty() ? resolveActiveFlowStationKey() : stationKey.trimmed();
    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (!QFile::exists(profileFlow))
        return out;

    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("LedInspect"));
    if (profileIni.contains(QStringLiteral("LedCount")))
        out.ledCount = qBound(1, profileIni.value(QStringLiteral("LedCount"), out.ledCount).toInt(), 4);
    if (profileIni.contains(QStringLiteral("MinLuma")))
        out.minLuma = profileIni.value(QStringLiteral("MinLuma"), out.minLuma).toInt();
    if (profileIni.contains(QStringLiteral("Mode")))
        out.mode = profileIni.value(QStringLiteral("Mode")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Mode1")))
        out.mode1 = profileIni.value(QStringLiteral("Mode1")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Mode2")))
        out.mode2 = profileIni.value(QStringLiteral("Mode2")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Mode3")))
        out.mode3 = profileIni.value(QStringLiteral("Mode3")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Mode4")))
        out.mode4 = profileIni.value(QStringLiteral("Mode4")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Roi1")))
        out.roi1 = profileIni.value(QStringLiteral("Roi1")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Roi2")))
        out.roi2 = profileIni.value(QStringLiteral("Roi2")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Roi3")))
        out.roi3 = profileIni.value(QStringLiteral("Roi3")).toString().trimmed();
    if (profileIni.contains(QStringLiteral("Roi4")))
        out.roi4 = profileIni.value(QStringLiteral("Roi4")).toString().trimmed();
    profileIni.endGroup();
    return out;
}

bool TestCaseStore::saveStationLedInspectConfig(const QString& stationKey, const TestCaseLedInspectConfig& config) {
    TestCasePaths::ensureRootDir();
    const QString key = stationKey.trimmed().isEmpty() ? resolveActiveFlowStationKey() : stationKey.trimmed();
    if (key.isEmpty())
        return false;
    ensureProfileDirectory(key, flowStationDisplayName(key), QString());

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("LedInspect"));
    profileIni.setValue(QStringLiteral("LedCount"), qBound(1, config.ledCount, 4));
    profileIni.setValue(QStringLiteral("MinLuma"), config.minLuma);
    profileIni.setValue(QStringLiteral("Mode"), config.mode);
    profileIni.setValue(QStringLiteral("Mode1"), config.mode1);
    profileIni.setValue(QStringLiteral("Mode2"), config.mode2);
    profileIni.setValue(QStringLiteral("Mode3"), config.mode3);
    profileIni.setValue(QStringLiteral("Mode4"), config.mode4);
    profileIni.setValue(QStringLiteral("Roi1"), config.roi1);
    profileIni.setValue(QStringLiteral("Roi2"), config.roi2);
    profileIni.setValue(QStringLiteral("Roi3"), config.roi3);
    profileIni.setValue(QStringLiteral("Roi4"), config.roi4);
    profileIni.endGroup();
    syncTestCaseIni(profileIni, profileFlow);
    return true;
}

TestCaseScreenInspectConfig TestCaseStore::loadStationScreenInspectConfig(const QString& stationKey) {
    TestCaseScreenInspectConfig out;
    out.refCircleCx = SETTINGS.value(QStringLiteral("ScreenInspect/RefCircleCx"), -1).toInt();
    out.refCircleCy = SETTINGS.value(QStringLiteral("ScreenInspect/RefCircleCy"), -1).toInt();
    out.refCircleR = SETTINGS.value(QStringLiteral("ScreenInspect/RefCircleR"), -1).toInt();
    out.reuseCircle = SETTINGS.value(QStringLiteral("ScreenInspect/ReuseCircle"), false).toBool();
    out.gigEIp = SETTINGS.value(QStringLiteral("ScreenInspect/GigEIp"), QStringLiteral("169.254.64.10")).toString().trimmed();
    if (out.gigEIp.isEmpty())
        out.gigEIp = QStringLiteral("169.254.64.10");

    TestCasePaths::ensureRootDir();
    ensureFilesystemLayout();
    const QString key = stationKey.trimmed().isEmpty() ? resolveActiveFlowStationKey() : stationKey.trimmed();
    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    if (!QFile::exists(profileFlow))
        return out;

    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("ScreenInspect"));
    if (profileIni.contains(QStringLiteral("RefCircleCx")))
        out.refCircleCx = profileIni.value(QStringLiteral("RefCircleCx"), out.refCircleCx).toInt();
    if (profileIni.contains(QStringLiteral("RefCircleCy")))
        out.refCircleCy = profileIni.value(QStringLiteral("RefCircleCy"), out.refCircleCy).toInt();
    if (profileIni.contains(QStringLiteral("RefCircleR")))
        out.refCircleR = profileIni.value(QStringLiteral("RefCircleR"), out.refCircleR).toInt();
    if (profileIni.contains(QStringLiteral("ReuseCircle")))
        out.reuseCircle = profileIni.value(QStringLiteral("ReuseCircle"), out.reuseCircle).toBool();
    if (profileIni.contains(QStringLiteral("GigEIp"))) {
        const QString ip = profileIni.value(QStringLiteral("GigEIp")).toString().trimmed();
        if (!ip.isEmpty())
            out.gigEIp = ip;
    }
    profileIni.endGroup();
    return out;
}

bool TestCaseStore::saveStationScreenInspectConfig(const QString& stationKey, const TestCaseScreenInspectConfig& config) {
    TestCasePaths::ensureRootDir();
    const QString key = stationKey.trimmed().isEmpty() ? resolveActiveFlowStationKey() : stationKey.trimmed();
    if (key.isEmpty())
        return false;
    ensureProfileDirectory(key, flowStationDisplayName(key), QString());

    const QString profileFlow = TestCasePaths::profileFlowPath(key);
    QSettings profileIni(profileFlow, QSettings::IniFormat);
    applyTestCaseIniCodec(profileIni);
    profileIni.beginGroup(QStringLiteral("ScreenInspect"));
    profileIni.setValue(QStringLiteral("RefCircleCx"), config.refCircleCx);
    profileIni.setValue(QStringLiteral("RefCircleCy"), config.refCircleCy);
    profileIni.setValue(QStringLiteral("RefCircleR"), config.refCircleR);
    profileIni.setValue(QStringLiteral("ReuseCircle"), config.reuseCircle);
    profileIni.setValue(QStringLiteral("GigEIp"), config.gigEIp.trimmed());
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
