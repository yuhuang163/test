#include "test_step_engine.h"

#include <QDebug>
#include "agreement/qProtocol/qprotocolmanager.h"
#include "qat.h"
#include "qusb.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

TestStepEngine::TestStepEngine(QObject* parent) : QObject(parent) {}

void TestStepEngine::setProtocolManager(QProtocolManager* pm) { protocolManager_ = pm; }
void TestStepEngine::setAt(Qat* at) { at_ = at; }
void TestStepEngine::setUsb(Qusb* usb) { usb_ = usb; }
void TestStepEngine::setRetrySender(std::function<int(std::function<void()>, int)> sender) {
    retrySender_ = std::move(sender);
}

void TestStepEngine::registerCustomFunction(const QString& name, std::function<void()> func) {
    customFunctions_[name] = std::move(func);
}

void TestStepEngine::clearCustomFunctions() {
    customFunctions_.clear();
}

// ---- DeviceCmd 字符串 → 枚举映射 ----

struct DeviceCmdEntry {
    const char* name;
    DeviceCmd cmd;
};

// 这个映射表对应 qprotocol_types.h 中的 DeviceCmd 枚举
static const DeviceCmdEntry kDeviceCmdTable[] = {
    {"ForbidSleep",         DeviceCmd::ForbidSleep},
    {"UartReceive",         DeviceCmd::UartReceive},
    {"RgbColor",            DeviceCmd::RgbColor},
    {"MotorCali",           DeviceCmd::MotorCali},
    {"MotorDampingState",   DeviceCmd::MotorDampingState},
    {"MotorTestState",      DeviceCmd::MotorTestState},
    {"MotorCaliState",      DeviceCmd::MotorCaliState},
    {"FacResult",           DeviceCmd::FacResult},
    {"ScreenColor",         DeviceCmd::ScreenColor},
    {"LedColor",            DeviceCmd::LedColor},
    {"ShipMode",            DeviceCmd::ShipMode},
    {"MotorParam",          DeviceCmd::MotorParam},
    {"MotorState",          DeviceCmd::MotorState},
    {"MotorCaliResultParam",DeviceCmd::MotorCaliResultParam},
    {"WifiConnect",         DeviceCmd::WifiConnect},
    {"Music",               DeviceCmd::Music},
    {"BurningMode",         DeviceCmd::BurningMode},
    {"BrushRecord",         DeviceCmd::BrushRecord},
    {"BrushTime",           DeviceCmd::BrushTime},
    {"Sleep",               DeviceCmd::Sleep},
    {"CameraState",         DeviceCmd::CameraState},
    {"ScreenCameraState",   DeviceCmd::ScreenCameraState},
    {"CameraLightState",    DeviceCmd::CameraLightState},
    {"CameraSupportState",  DeviceCmd::CameraSupportState},
    {"CameraExposureTime",  DeviceCmd::CameraExposureTime},
    {"DevReset",            DeviceCmd::DevReset},
    {"BrushReset",          DeviceCmd::BrushReset},
    {"ImuCaliResult",       DeviceCmd::ImuCaliResult},
    {"DeviceMode",          DeviceCmd::DeviceMode},
    {"BrushControl",        DeviceCmd::BrushControl},
    {"FacMode",             DeviceCmd::FacMode},
    {"Sn",                  DeviceCmd::Sn},
    {"BaseInfo",            DeviceCmd::BaseInfo},
    {"CameraPictureState",  DeviceCmd::CameraPictureState},
    {"WifiDisconnect",      DeviceCmd::WifiDisconnect},
    {"SuctionMode",         DeviceCmd::SuctionMode},
    {"BtSignalMode",        DeviceCmd::BtSignalMode},
    {"BtNoSignalMode",      DeviceCmd::BtNoSignalMode},
    {"BtFreqMode",          DeviceCmd::BtFreqMode},
    {"WriteKey",            DeviceCmd::WriteKey},
    {"TrimSet",             DeviceCmd::TrimSet},
    {"MacWrite",            DeviceCmd::MacWrite},
    {"NightLightSet",       DeviceCmd::NightLightSet},
    {"LedTest",             DeviceCmd::LedTest},
    {"FactoryReset",        DeviceCmd::FactoryReset},
    {"LcdBacklight",        DeviceCmd::LcdBacklight},
    {"LightReportControl",  DeviceCmd::LightReportControl},
    {"LightCalibWrite",     DeviceCmd::LightCalibWrite},
    {"CompensationSet",     DeviceCmd::CompensationSet},
    // get commands
    {"GetBattery",          DeviceCmd::GetBattery},
    {"ButtonState",         DeviceCmd::ButtonState},
    {"DeviceInfo",          DeviceCmd::DeviceInfo},
    {"PeriphState",         DeviceCmd::PeriphState},
    {"ConnectInfo",         DeviceCmd::ConnectInfo},
    {"WifiInfo",            DeviceCmd::WifiInfo},
    {"TupleRead",           DeviceCmd::TupleRead},
    {"RssiRead",            DeviceCmd::RssiRead},
    {"KeySignalRead",       DeviceCmd::KeySignalRead},
    {"ChargeCurrentRead",   DeviceCmd::ChargeCurrentRead},
    {"AgingStatusRead",     DeviceCmd::AgingStatusRead},
    {"FactoryDoneRead",     DeviceCmd::FactoryDoneRead},
    {"DeviceExceptionRead", DeviceCmd::DeviceExceptionRead},
    {"TrimRead",            DeviceCmd::TrimRead},
    {"MacRead",             DeviceCmd::MacRead},
    {"LightCalibRead",      DeviceCmd::LightCalibRead},
    {"PressCollect",        DeviceCmd::PressCollect},
    {"ImuCollect",          DeviceCmd::ImuCollect},
};

static constexpr int kDeviceCmdTableSize = sizeof(kDeviceCmdTable) / sizeof(kDeviceCmdTable[0]);

DeviceCmd TestStepEngine::parseDeviceCmd(const QString& name) {
    for (int i = 0; i < kDeviceCmdTableSize; ++i) {
        if (name.compare(QLatin1String(kDeviceCmdTable[i].name), Qt::CaseInsensitive) == 0) {
            return kDeviceCmdTable[i].cmd;
        }
    }
    qWarning() << "[TestStepEngine] 未知的 DeviceCmd:" << name << "，默认返回 ForbidSleep";
    return DeviceCmd::ForbidSleep;
}

QStringList TestStepEngine::allDeviceCmdNames() {
    QStringList names;
    names.reserve(kDeviceCmdTableSize);
    for (int i = 0; i < kDeviceCmdTableSize; ++i) {
        names.append(QLatin1String(kDeviceCmdTable[i].name));
    }
    return names;
}

// ---- 参数构建 ----

QVariant TestStepEngine::buildVariantFromParams(const QString& /*cmdName*/, const QVariantMap& params) {
    // 简单情况：单个 "value" 键 → 直接返回该值
    if (params.size() == 1 && params.contains("value")) {
        return params["value"];
    }
    // 空参数 → 空 QVariant（无参命令如 DevReset）
    if (params.isEmpty()) {
        return {};
    }
    // 多参数 → 返回整个 QVariantMap（如 BurningMode 的 {mode, seconds, switch}）
    return QVariant::fromValue(params);
}

// ---- 执行 ----

bool TestStepEngine::executeStep(const TestStepDescriptor& step) {
    switch (step.actionType) {
    case TestStepDescriptor::ProtocolSet: {
        if (!protocolManager_) {
            qWarning() << "[TestStepEngine] protocolManager 未设置";
            return false;
        }
        const DeviceCmd cmd = parseDeviceCmd(step.deviceCmd);
        const QVariant data = buildVariantFromParams(step.deviceCmd, step.params);
        if (retrySender_) {
            retrySender_([this, cmd, data]() {
                protocolManager_->set(cmd, data);
            }, step.retryTimeoutMs);
        } else {
            protocolManager_->set(cmd, data);
        }
        return true;
    }
    case TestStepDescriptor::ProtocolGet: {
        if (!protocolManager_) {
            qWarning() << "[TestStepEngine] protocolManager 未设置";
            return false;
        }
        const DeviceCmd cmd = parseDeviceCmd(step.deviceCmd);
        const QVariant param = buildVariantFromParams(step.deviceCmd, step.params);
        if (retrySender_) {
            retrySender_([this, cmd, param]() {
                protocolManager_->get(cmd, param);
            }, step.retryTimeoutMs);
        } else {
            protocolManager_->get(cmd, param);
        }
        return true;
    }
    case TestStepDescriptor::AtCommand: {
        if (!at_) {
            qWarning() << "[TestStepEngine] AT 对象未设置";
            return false;
        }
        const QString atCmd = step.params.value("command").toString();
        const QString atParam = step.params.value("param").toString();
        if (atCmd == "sendDcon") {
            if (retrySender_) {
                retrySender_([this, atParam]() { at_->sendDcon(atParam); }, step.retryTimeoutMs);
            } else {
                at_->sendDcon(atParam);
            }
        } else if (atCmd == "sendMac") {
            if (retrySender_) {
                retrySender_([this, atParam]() { at_->sendMac(atParam); }, step.retryTimeoutMs);
            } else {
                at_->sendMac(atParam);
            }
        } else {
            qWarning() << "[TestStepEngine] 未知的 AT 命令:" << atCmd;
            return false;
        }
        return true;
    }
    case TestStepDescriptor::UsbCommand: {
        if (!usb_) {
            qWarning() << "[TestStepEngine] USB 对象未设置";
            return false;
        }
        const QString usbCmd = step.params.value("command").toString();
        if (usbCmd == "ReadMeasurement") {
            if (retrySender_) {
                retrySender_([this]() { usb_->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement); }, step.retryTimeoutMs);
            } else {
                usb_->sendPowerInstruction(Qusb::PowerAction::ReadMeasurement);
            }
        } else {
            qWarning() << "[TestStepEngine] 未知的 USB 命令:" << usbCmd;
            return false;
        }
        return true;
    }
    case TestStepDescriptor::CustomFunction: {
        const QString funcName = step.customFunctionName;
        auto it = customFunctions_.find(funcName);
        if (it == customFunctions_.end()) {
            qWarning() << "[TestStepEngine] 未注册的自定义函数:" << funcName;
            return false;
        }
        it.value()();
        return true;
    }
    }
    return false;
}
