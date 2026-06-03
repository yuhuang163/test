#ifndef PLATFORM_TEST_CASE_TYPES_H
#define PLATFORM_TEST_CASE_TYPES_H

#include <QString>
#include <QVariant>

enum class TestCaseSendAction { Set, Get };

enum class TestCaseSendChannel { Product, ProductSerial, Dongle, Cloud };

/** 产品蓝牙通信协议（仅 Send/Channel=Product 时有效；与 QProtocolManager::ProtocolType 对应）。 */
enum class TestCaseProductProtocol { Qfctp, Qpb };

struct TestCaseMeta {
    /** 名称：界面显示、日志、ini 文件名（支持中文） */
    QString name;
    /** 与 name 同步写入，兼容旧 ini 的 Meta/DisplayName */
    QString displayName;
    QString mesTag;
    /** 步骤开始时是否弹出提示框 */
    bool promptEnabled = false;
    /** 弹窗正文（多行） */
    QString promptText;
};

struct TestCaseSend {
    TestCaseSendChannel channel = TestCaseSendChannel::Product;
    TestCaseProductProtocol productProtocol = TestCaseProductProtocol::Qfctp;
    TestCaseSendAction action = TestCaseSendAction::Set;
    QString deviceCmd;
    QVariant param;
};

struct TestCaseTiming {
    int delayBeforeMs = 0;
    int delayAfterMs = 0;
    /** sendCommandWithRetry 定时器间隔(ms)；≤0 表示未配置，运行时按卡控开闭回退 8000/3000 */
    int commandTimeoutMs = 0;
};

enum class TestCaseGateOp { Range, Gt, Lt, Eq, CompareVersions };

struct TestCaseGate {
    bool enabled = false;
    QString reportType;
    QString field;
    TestCaseGateOp op = TestCaseGateOp::Range;
    double low = 0.0;
    double high = 0.0;
    QString expected;
    QString expectedSettingsKey;
    QString lowSettingsKey;
    QString highSettingsKey;
};

/** 流程编排中单步：case 名 */
struct TestFlowItemEntry {
    QString caseName;
};

struct TestCaseHook {
    bool enabled = false;
    QString hookId;
};

struct TestCaseDefinition {
    TestCaseMeta meta;
    TestCaseSend send;
    TestCaseTiming timing;
    TestCaseGate gate;
    TestCaseHook hook;
};

struct TestFlowMeta {
    int version = 1;
    QString selectedStation;
    QString selectedStationName;
};

/** 流程编排工站：key 写入 ini，displayName 为下拉显示名 */
struct TestFlowStationEntry {
    QString key;
    QString displayName;
};

#endif  // PLATFORM_TEST_CASE_TYPES_H
