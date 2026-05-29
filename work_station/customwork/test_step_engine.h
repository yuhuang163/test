#ifndef TEST_STEP_ENGINE_H
#define TEST_STEP_ENGINE_H

#include <QMap>
#include <QObject>
#include <QString>
#include <QVariant>
#include <functional>

#include "test_step_descriptor.h"
#include "agreement/qProtocol/qprotocol_types.h"

class QProtocolManager;
class Qat;
class Qusb;

/**
 * 通用测试步骤执行引擎。
 *
 * 接收一个 TestStepDescriptor（纯数据），根据 actionType 分发到对应的协议/设备接口执行。
 * 上层工站只需注入依赖（protocolManager, at, usb），然后调用 executeStep()。
 */
class TestStepEngine : public QObject {
    Q_OBJECT
public:
    explicit TestStepEngine(QObject* parent = nullptr);

    // 注入依赖
    void setProtocolManager(QProtocolManager* pm);
    void setAt(Qat* at);
    void setUsb(Qusb* usb);
    void setRetrySender(std::function<int(std::function<void()>, int)> sender);

    // 注册自定义函数（复杂逻辑无法纯数据化的步骤）
    void registerCustomFunction(const QString& name, std::function<void()> func);
    void clearCustomFunctions();

    // 执行一个步骤描述符
    bool executeStep(const TestStepDescriptor& step);

    // DeviceCmd 字符串 → 枚举值
    static DeviceCmd parseDeviceCmd(const QString& name);

    // 获取所有可用的 DeviceCmd 名称（供 UI 下拉选择）
    static QStringList allDeviceCmdNames();

private:
    // 根据 deviceCmd 名称和 params 构建 QVariant 参数
    static QVariant buildVariantFromParams(const QString& cmdName, const QVariantMap& params);

    QProtocolManager* protocolManager_ = nullptr;
    Qat* at_ = nullptr;
    Qusb* usb_ = nullptr;
    std::function<int(std::function<void()>, int)> retrySender_;
    QMap<QString, std::function<void()>> customFunctions_;
};

#endif  // TEST_STEP_ENGINE_H
