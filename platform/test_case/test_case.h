#ifndef PLATFORM_TEST_CASE_H
#define PLATFORM_TEST_CASE_H

#include "qprotocol_types.h"
#include "test_case_types.h"

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>

#include <functional>

class QFreeWork;

// ---------- 路径 ----------
namespace TestCasePaths {
QString rootDir();
QString flowIniPath();
QString caseIniPath(const QString& caseName);
QString flowIniFileName();
bool ensureRootDir();
bool isValidCaseFileName(const QString& name, QString* errorOut = nullptr);
bool isReservedCaseName(const QString& name);
}  // namespace TestCasePaths

// ---------- 存储 ----------
class TestCaseStore {
public:
    static bool loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut = nullptr);
    static bool saveCase(const TestCaseDefinition& def, QString* errorOut = nullptr);
    static QStringList listCaseIniNames();
    static bool loadFlowMeta(TestFlowMeta& out);
    static bool saveFlowMeta(const TestFlowMeta& meta);
    static QStringList loadStationItems(const QString& stationKey);
    static bool saveStationItems(const QString& stationKey, const QStringList& items);
    static QVector<TestFlowItemEntry> loadStationFlowItems(const QString& stationKey);
    /** 工站级：任一步测试失败是否结束整单流程（默认 true） */
    static bool loadStationStopFlowOnTestFail(const QString& stationKey, bool defaultValue = true);
    static bool saveStationFlowItems(const QString& stationKey, const QVector<TestFlowItemEntry>& items,
                                     bool stopFlowOnTestFail = true);
    static QStringList listStationKeysFromFlow();

    /** 内置工站（与设置页 TestOrder 预设一致，并含 default / FREE_WORK） */
    static QVector<TestFlowStationEntry> defaultFlowStationPresets();
    /** 从 总的测试流程.ini [FlowStations] 读取；无记录时写入预设并返回 */
    static QVector<TestFlowStationEntry> loadFlowStationCatalog();
    static bool saveFlowStationCatalog(const QVector<TestFlowStationEntry>& entries);
    static QString flowStationDisplayName(const QString& stationKey);
    /** 显示名或已有键 → 流程 ini 使用的工站键（预设如 自由工站→FREE_WORK） */
    static QString resolveFlowStationKey(const QString& displayNameOrKey);
    static bool addFlowStation(const QString& displayName, QString* errorOut = nullptr);
    static bool removeFlowStation(const QString& stationKey, QString* errorOut = nullptr);
};

// ---------- 校验 ----------
class TestCaseValidator {
public:
    static bool validateCase(const TestCaseDefinition& def, QStringList& errors);
};

// ---------- 设备指令 ----------
enum class DeviceCmdParamKind { None, Int, UInt, String, JsonMap };

struct DeviceCmdParamSchema {
    DeviceCmdParamKind kind = DeviceCmdParamKind::None;
    QString hint;
};

class DeviceCmdCatalog {
public:
    static QStringList allDeviceCmdNames();
    static QString deviceCmdUiLabel(const QString& enumName);
    static bool deviceCmdFromName(const QString& name, DeviceCmd& out);
    static QString deviceCmdToName(DeviceCmd cmd);
    static bool paramSchemaFor(DeviceCmd cmd, DeviceCmdParamSchema& out);
    static QVariant paramFromSettings(const QSettings& settings, const QString& prefix);
    static void paramToSettings(QSettings& settings, const QString& prefix, const QVariant& value);
    static bool paramFromIniGroup(const QSettings& settings, DeviceCmd cmd, QVariant& out);
    static void paramToIniGroup(QSettings& settings, DeviceCmd cmd, const QVariant& value);
};

// ---------- 卡控 ----------
struct GateFieldDescriptor {
    QString field;
    QString displayName;
};

struct GateTypeDescriptor {
    QString reportType;
    QString displayName;
    QVector<GateFieldDescriptor> fields;
};

class GateRegistry {
public:
    static QStringList reportTypes();
    static QVector<GateTypeDescriptor> allTypeDescriptors();
    static bool descriptorFor(const QString& reportType, GateTypeDescriptor& out);
    static QStringList fieldsFor(const QString& reportType);
    static bool evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                         QString& detailOut);
};

// ---------- 钩子（仅自由工站 QFreeWork 执行） ----------
using TestCaseHookFn = std::function<void(QFreeWork*)>;

class TestCaseHookRegistry {
public:
    static void registerHook(const QString& hookId, TestCaseHookFn fn);
    static bool contains(const QString& hookId);
    static QStringList hookIds();
    static bool invoke(const QString& hookId, QFreeWork* ctx);
};

/** 自由工站 test_case 钩子（幂等）。 */
void registerFreeWorkTestCaseHooks();
/** FREEWORK_TEST_LIST 全量钩子（幂等，实现于 qfreework_case_hooks.cpp）。 */
void registerQFreeWorkCatalogTestCaseHooks();

// ---------- 执行 ----------
class TestCaseRunner {
public:
    static QStringList loadFlowForStation(const QString& stationKey);
    static bool loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut = nullptr);
    static void beginStep(QFreeWork* ctx, const TestCaseDefinition& def);
    static QString stepLabel(const TestCaseDefinition& def);
    static bool needAsyncDone(const TestCaseDefinition& def);
    /** 本 case 指令等待/重试间隔(ms) */
    static int commandTimeoutMs(const TestCaseDefinition& def);
};

#endif  // PLATFORM_TEST_CASE_H
