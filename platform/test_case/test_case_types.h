#ifndef PLATFORM_TEST_CASE_TYPES_H
#define PLATFORM_TEST_CASE_TYPES_H

#include <QString>
#include <QVariant>
#include <QVector>

// --- 发送通道 / 协议 ---
enum class TestCaseSendAction { Set,
                                Get };

enum class DeviceCmdParamKind { None,
                                Int,
                                UInt,
                                String,
                                JsonMap };

struct DeviceCmdParamSchema {
    DeviceCmdParamKind kind = DeviceCmdParamKind::None;
    QString hint;
};

enum class TestCaseSendChannel { Product,
                                 ProductSerial,
                                 Dongle,
                                 Cloud,
                                 Fixture,
                                 Modbus,
                                 Scpi };

enum class TestCaseProductProtocol { Qfctp,
                                     Qpb,
                                     Qroot };

enum class TestCaseFixtureProtocol { Pcba,
                                     Asd9026a,
                                     XwdBle };

// --- Case 元数据 / 发送 / 时序 ---
struct TestCaseMeta {
    QString name;
    QString displayName;
    QString mesTag;
    bool promptEnabled = false;
    QString promptText;
};

struct TestCaseSend {
    TestCaseSendChannel channel = TestCaseSendChannel::Product;
    TestCaseProductProtocol productProtocol = TestCaseProductProtocol::Qfctp;
    TestCaseFixtureProtocol fixtureProtocol = TestCaseFixtureProtocol::Pcba;
    TestCaseSendAction action = TestCaseSendAction::Set;
    QString device;
    QString deviceCmd;
    QVariant param;
};

struct TestCaseTiming {
    int delayBeforeMs = 0;
    int delayAfterMs = 0;
    int commandTimeoutMs = 0;
};

// --- 卡控 ---
enum class TestCaseGateOp { Range,
                            Gt,
                            Lt,
                            Eq,
                            CompareVersions };

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

// --- 流程编排 ---
struct TestFlowItemEntry {
    QString caseName;
    bool enabled = true;
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
    QVector<TestCaseGate> gates;
    TestCaseHook hook;
};

struct TestFlowMeta {
    int version = 1;
    QString selectedStation;
    QString selectedStationName;
};

struct TestFlowStationEntry {
    QString key;
    QString displayName;
};

#endif // PLATFORM_TEST_CASE_TYPES_H
