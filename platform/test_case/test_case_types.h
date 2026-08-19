#ifndef PLATFORM_TEST_CASE_TYPES_H
#define PLATFORM_TEST_CASE_TYPES_H

#include <QString>
#include <QVariant>
#include <QVector>

// --- 发送通道 / 协议 ---
// clang-format off
enum class TestCaseSendAction { Set, Get };
enum class DeviceCmdParamKind { None, Int, UInt, String, JsonMap };
// clang-format on

struct DeviceCmdParamSchema {
    DeviceCmdParamKind kind = DeviceCmdParamKind::None;
    QString hint;
};

// clang-format off
enum class TestCaseSendChannel { Product, ProductSerial, Dongle, Cloud, Fixture, Modbus, Scpi };
enum class UsbCameraCmd { ScreenDeadPixelCheck, ScreenDisplayAnomalyCheck };
enum class TestCaseProductProtocol { Qfctp, Qpb, Qroot, Qaiot };
enum class TestCaseFixtureProtocol {
    Pcba,
    Asd9026a,
    Xwd, // 原 XWD_BLE / XWD_SUCTION 同一治具串口物理层
    JieliBtBox,
    UsbCamera
};
// clang-format on

// --- Case 元数据 / 发送 / 时序 ---
struct TestCaseMeta {
    QString name;
    QString displayName;
    QString mesTag;
    bool promptEnabled = false;
    /** true=纯空白提醒（确认后不发 Send）；false=提示同时/仍可执行 Send 指令 */
    bool promptOnly = false;
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
    /** false：只发不收，发完即放行（进非信令关机等）；对应 Timing/WaitReply */
    bool waitReply = true;
};

// --- 卡控 ---
// clang-format off
enum class TestCaseGateOp { Range, Gt, Lt, Eq, CompareVersions };
// clang-format on

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

/** 工站 flow.ini [SerialUi]：工位界面三路串口是否显示及标签文案 */
struct TestCaseSerialUiConfig {
    bool jigVisible = true;
    bool productVisible = true;
    bool usbVisible = true;
    QString jigLabel = QStringLiteral("治具串口");
    QString productLabel = QStringLiteral("产品串口(仪器)");
    QString usbLabel = QStringLiteral("万用表串口");
};

/** 工站 flow.ini [DeviceSide]：三元组/设备数据左右位（与主界面「三元组位置」一致） */
struct TestCaseDeviceSideConfig {
    /** L 左 / R 右 / S 单只 / F 未指定；空表示未配置 */
    QString position;
    /** Qaiot device_side_id：0 Left / 1 Right / 2 Independent；-1 表示未配置 */
    int sideId = -1;
};

#endif // PLATFORM_TEST_CASE_TYPES_H
