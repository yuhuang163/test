#ifndef PLATFORM_TEST_CASE_TYPES_H
#define PLATFORM_TEST_CASE_TYPES_H

#include <QString>
#include <QVariant>

enum class TestCaseSendAction { Set, Get };

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
    TestCaseSendAction action = TestCaseSendAction::Set;
    QString deviceCmd;
    QVariant param;
};

struct TestCaseTiming {
    int delayBeforeMs = 0;
    int delayAfterMs = 0;
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

#endif  // PLATFORM_TEST_CASE_TYPES_H
