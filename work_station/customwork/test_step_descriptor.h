#ifndef TEST_STEP_DESCRIPTOR_H
#define TEST_STEP_DESCRIPTOR_H

#include <QString>
#include <QVariantMap>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>

/**
 * 测试步骤的纯数据描述。
 *
 * 将原 FREEWORK_TEST_LIST 宏中硬编码的每一行拆分为可序列化、可 UI 编辑的数据结构。
 * 例如原来的：
 *   sendCommandWithRetry([&]() { protocolManager.set(DeviceCmd::ForbidSleep, (int)FacSwitch_OPEN); })
 * 变为：
 *   { actionType: ProtocolSet, deviceCmd: "ForbidSleep", params: {"value": 1} }
 */
struct TestStepDescriptor {
    int id = -1;
    QString name;                  // 步骤显示名称，如 "禁止休眠"
    QString category;              // 分类：product / fixture / dongle / cloud
    bool needCaseDone = false;     // 是否需要异步回调判定（true = 需等 refreshXxx 回调）
    QString mesTag;                // MES 上报键名

    // ===== 核心：将命令逻辑数据化 =====
    enum ActionType {
        ProtocolSet = 0,   // protocolManager.set(cmd, data)
        ProtocolGet,       // protocolManager.get(cmd, param)
        AtCommand,         // AT 指令（如 at->sendDcon）
        UsbCommand,        // 治具 USB 指令
        CustomFunction,    // 自定义函数名（复杂逻辑无法纯数据化的）
    };

    ActionType actionType = ProtocolSet;
    QString deviceCmd;             // DeviceCmd 枚举名字符串，如 "ForbidSleep"
    QVariantMap params;            // 参数映射，如 {"value": 1}
    int retryTimeoutMs = 300;      // sendCommandWithRetry 超时(ms)
    QString customFunctionName;    // actionType==CustomFunction 时的函数名

    // JSON 序列化
    QJsonObject toJson() const;
    static TestStepDescriptor fromJson(const QJsonObject& obj);

    // ActionType ↔ 字符串
    static QString actionTypeToString(ActionType type);
    static ActionType actionTypeFromString(const QString& str);
};

// 整套配置的序列化
namespace TestStepConfig {
    QJsonArray toJsonArray(const QVector<TestStepDescriptor>& steps);
    QVector<TestStepDescriptor> fromJsonArray(const QJsonArray& arr);

    bool saveToFile(const QString& filePath, const QVector<TestStepDescriptor>& steps);
    QVector<TestStepDescriptor> loadFromFile(const QString& filePath);
}

#endif  // TEST_STEP_DESCRIPTOR_H
