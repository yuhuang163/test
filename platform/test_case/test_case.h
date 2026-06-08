#ifndef PLATFORM_TEST_CASE_H
#define PLATFORM_TEST_CASE_H

#include "qprotocol_types.h"
#include "qat.h"
#include "qtupleservice.h"
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
} // namespace TestCasePaths

// ---------- 存储 ----------
class TestCaseStore {
  public:
    static bool loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut = nullptr);
    static bool saveCase(const TestCaseDefinition& def, QString* errorOut = nullptr);
    /** 运行时实际参与判定的卡控列表（gates 优先，否则单项 gate） */
    static QVector<TestCaseGate> effectiveGates(const TestCaseDefinition& def);
    static bool usesMultiFieldGates(const TestCaseDefinition& def);
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

    /** 内置工站（与测试流程编排页预设一致，并含 default / FREE_WORK） */
    static QVector<TestFlowStationEntry> defaultFlowStationPresets();
    /** 从 总的测试流程.ini [FlowStations] 读取；无记录时写入预设并返回 */
    static QVector<TestFlowStationEntry> loadFlowStationCatalog();
    static bool saveFlowStationCatalog(const QVector<TestFlowStationEntry>& entries);
    static QString flowStationDisplayName(const QString& stationKey);
    /** 显示名或已有键 → 流程 ini 使用的工站键（预设如 自由工站→FREE_WORK） */
    static QString resolveFlowStationKey(const QString& displayNameOrKey);
    static bool addFlowStation(const QString& displayName, QString* errorOut = nullptr);
    /** 复制工站流程（功能块列表与 StopFlowOnTestFail）到新工站。 */
    static bool copyFlowStation(const QString& sourceStationKey, const QString& newDisplayName,
                                const QVector<TestFlowItemEntry>& items, bool stopFlowOnTestFail,
                                QString* outNewKey = nullptr, QString* errorOut = nullptr);
    static bool renameFlowStation(const QString& stationKey, const QString& newDisplayName,
                                  QString* errorOut = nullptr);
    static bool removeFlowStation(const QString& stationKey, QString* errorOut = nullptr);
};

// ---------- 校验 ----------
class TestCaseValidator {
  public:
    static bool validateCase(const TestCaseDefinition& def, QStringList& errors);
};

// ---------- 设备指令 ----------
class DeviceCmdCatalog {
  public:
    static QStringList allDeviceCmdNames(TestCaseSendAction action);
    static TestCaseProductProtocol productProtocolFromIni(const QString& text);
    static QString productProtocolToIni(TestCaseProductProtocol protocol);
    static QString productProtocolUiLabel(TestCaseProductProtocol protocol);
    static TestCaseSendAction actionFor(DeviceCmd cmd);
    static bool isCmdForAction(DeviceCmd cmd, TestCaseSendAction action);
    static QString deviceCmdUiLabel(const QString& enumName);
    static bool deviceCmdFromName(const QString& name, DeviceCmd& out);
    static QString deviceCmdToName(DeviceCmd cmd);
    static bool paramSchemaFor(DeviceCmd cmd, DeviceCmdParamSchema& out);
    /** 设置页「指令参数」填写说明（含示例）。 */
    static QString paramUiHint(const QString& deviceCmdName);
    static bool paramFromIniGroup(const QSettings& settings, DeviceCmd cmd, QVariant& out);
    static void paramToIniGroup(QSettings& settings, DeviceCmd cmd, const QVariant& value);
    /** 将 case ini 中的 JsonMap 参数转换为协议 set/get 可接受的 QVariant。 */
    static QVariant normalizeSendParam(DeviceCmd cmd, const QVariant& param);
};

class DongleCmdCatalog {
  public:
    static QStringList allDongleCmdNames(TestCaseSendAction action);
    static TestCaseSendAction actionFor(DongleCmd cmd);
    static bool isCmdForAction(DongleCmd cmd, TestCaseSendAction action);
    static QString dongleCmdUiLabel(const QString& enumName);
    static bool dongleCmdFromName(const QString& name, DongleCmd& out);
    static QString dongleCmdToName(DongleCmd cmd);
    static bool paramSchemaFor(DongleCmd cmd, DeviceCmdParamSchema& out);
    static QString paramUiHint(const QString& dongleCmdName);
    static bool paramFromIniGroup(const QSettings& settings, DongleCmd cmd, QVariant& out);
    static void paramToIniGroup(QSettings& settings, DongleCmd cmd, const QVariant& value);
};

enum class ProductSerialCmd {
    InstrumentReset,
    StartRx2402Ble1M,
    StartRx2440Ble1M,
    StartRx2480Ble1M,
    StartRx2402Ble2M,
    StartRx2440Ble2M,
    StartRx2480Ble2M,
    StopRxAndPer,
};

/** PCBA 治具 0x55 协议指令（Send/Channel=Fixture 且 Send/Protocol=Pcba）。 */
enum class FixturePcbaCmd {
    StartTest,
    StartSleep,
    StartWhiteMode,
    WaitFixturePacket,
    WaitStartTestAck,
    WaitSleepRequest,
};

class FixturePcbaCmdCatalog {
  public:
    static QStringList allFixturePcbaCmdNames(TestCaseSendAction action);
    static TestCaseFixtureProtocol fixtureProtocolFromIni(const QString& text);
    static QString fixtureProtocolToIni(TestCaseFixtureProtocol protocol);
    static QString fixtureProtocolUiLabel(TestCaseFixtureProtocol protocol);
    static TestCaseSendAction actionFor(FixturePcbaCmd cmd);
    static bool isCmdForAction(FixturePcbaCmd cmd, TestCaseSendAction action);
    static QString fixturePcbaCmdUiLabel(const QString& enumName);
    static bool fixturePcbaCmdFromName(const QString& name, FixturePcbaCmd& out);
    static QString fixturePcbaCmdToName(FixturePcbaCmd cmd);
    static bool paramSchemaFor(FixturePcbaCmd cmd, DeviceCmdParamSchema& out);
    static QString paramUiHint(const QString& enumName);
    static bool paramFromIniGroup(const QSettings& settings, FixturePcbaCmd cmd, QVariant& out);
    static void paramToIniGroup(QSettings& settings, FixturePcbaCmd cmd, const QVariant& value);
};

class ProductSerialCmdCatalog {
  public:
    static QStringList allProductSerialCmdNames();
    static TestCaseSendAction actionFor(ProductSerialCmd cmd);
    static bool isCmdForAction(ProductSerialCmd cmd, TestCaseSendAction action);
    static QString productSerialCmdUiLabel(const QString& enumName);
    static bool productSerialCmdFromName(const QString& name, ProductSerialCmd& out);
    static QString productSerialCmdToName(ProductSerialCmd cmd);
    static bool paramSchemaFor(ProductSerialCmd cmd, DeviceCmdParamSchema& out);
    static QString paramUiHint(const QString& enumName);
    /** 开始接收类指令对应的 brush profile 0～5；-1 表示非此类指令。 */
    static int brushProfileForCmd(ProductSerialCmd cmd);
};

class TupleCmdCatalog {
  public:
    static QStringList allTupleCmdNames(TestCaseSendAction action);
    static TestCaseSendAction actionFor(TupleCmd cmd);
    static bool isCmdForAction(TupleCmd cmd, TestCaseSendAction action);
    static QString tupleCmdUiLabel(const QString& enumName);
    static bool tupleCmdFromName(const QString& name, TupleCmd& out);
    static QString tupleCmdToName(TupleCmd cmd);
    static bool paramSchemaFor(TupleCmd cmd, DeviceCmdParamSchema& out);
    static QString paramUiHint(const QString& tupleCmdName);
    static bool paramFromIniGroup(const QSettings& settings, TupleCmd cmd, QVariant& out);
    static void paramToIniGroup(QSettings& settings, TupleCmd cmd, const QVariant& value);
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

/** 卡控步 MES/表格展示用的 testData 与 ask。 */
struct GateStepDisplay {
    QString testData;
    QString ask;
};

class GateRegistry {
  public:
    static QStringList reportTypes();
    static QVector<GateTypeDescriptor> allTypeDescriptors();
    static bool descriptorFor(const QString& reportType, GateTypeDescriptor& out);
    static QStringList fieldsFor(const QString& reportType);
    /** Gate/Field 为 *、all 或空时，对同一回包内全部已登记字段套用相同判定条件。 */
    static bool isAllFieldsGateField(const QString& field);
    static QString fieldDisplayName(const QString& reportType, const QString& field);
    static bool evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                         QString& detailOut);
    /** 多项卡控须全部通过 */
    static bool evaluateAll(const QVector<TestCaseGate>& gates, const QString& reportType, const QVariant& payload,
                            bool& passOut, QString& detailOut);
    /** 解析 range 卡控上下限（含 LowSettingsKey/HighSettingsKey）。 */
    static void resolveRangeBounds(const TestCaseGate& gate, double& lowOut, double& highOut);
    /** 单项卡控的期望展示（范围/比较符/等值）。 */
    static QString formatGateAsk(const TestCaseGate& gate, const QString& reportType);
    /** 多项卡控合并期望展示。 */
    static QString formatMultiFieldAsk(const QVector<TestCaseGate>& gates, const QString& reportType);
    /** 从回包与主卡控项生成步骤 testData/ask（判定逻辑仍用 evaluate/evaluateAll）。 */
    static GateStepDisplay formatStepDisplay(const TestCaseGate& primaryGate, const QVector<TestCaseGate>& allGates,
                                             const QString& reportType, const QVariant& payload, bool multiFieldMode);
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
/** 自由工站目录钩子（幂等，实现于 qfreework_case_hooks.cpp）。 */
void registerQFreeWorkCatalogTestCaseHooks();

// ---------- 执行 ----------
class TestCaseRunner {
  public:
    static QStringList loadFlowForStation(const QString& stationKey);
    static bool loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut = nullptr);
    static void beginStep(QFreeWork* ctx, const TestCaseDefinition& def);
    static QString stepLabel(const TestCaseDefinition& def);
    static bool needAsyncDone(const TestCaseDefinition& def);
    /** Dongle 扫描/直连蓝牙：需等待连接成功，不能发完即过步 */
    static bool isDongleBleConnectStep(const TestCaseDefinition& def);
    /** 本步是否必须通过已连接的产品 BLE 收发协议（仅此类步骤在未连蓝牙时阻塞流程） */
    static bool stepRequiresProductBle(const TestCaseDefinition& def);
    /** 弹窗提示步：须用户点「是」或关闭窗口后才过步 */
    static bool stepWaitsForPromptAck(const TestCaseDefinition& def);
    /** 本 case 指令等待/重试间隔(ms) */
    static int commandTimeoutMs(const TestCaseDefinition& def);
};

#endif // PLATFORM_TEST_CASE_H
