#include "test_case_runner.h"

#include "test_case_store.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QStringList TestCaseRunner::loadFlowForStation(const QString& stationKey) {
    return TestCaseStore::loadStationItems(stationKey.trimmed());
}

bool TestCaseRunner::loadCase(const QString& caseName, TestCaseDefinition& out, QString* errorOut) {
    return TestCaseStore::loadCase(caseName, out, errorOut);
}

bool TestCaseRunner::loadCaseForStation(const QString& stationKey, const QString& stepId, TestCaseDefinition& out,
                                        QString* errorOut) {
    return TestCaseStore::loadCaseForStation(stationKey, stepId, out, errorOut);
}

QString TestCaseRunner::stepLabel(const TestCaseDefinition& def) {
    return def.meta.name.trimmed();
}

bool TestCaseRunner::stepWaitsForPromptAck(const TestCaseDefinition& def) {
    // 有卡控：弹窗只提示、指令立刻发，同步等上报。无卡控：先确认再发，避免弹窗还在指令已出去。
    if (!def.meta.promptEnabled || def.hook.enabled || def.gate.enabled)
        return false;
    if (def.meta.promptText.trimmed().isEmpty())
        return false;
    return true;
}

bool TestCaseRunner::needAsyncDone(const TestCaseDefinition& def) {
    if (def.hook.enabled)
        return true;
    if (def.send.channel == TestCaseSendChannel::ProductSerial)
        return true;
    if (def.send.channel == TestCaseSendChannel::Fixture)
        return true;
    if (def.send.channel == TestCaseSendChannel::Dongle
        && (def.send.deviceCmd == QStringLiteral("SampleSuctionDual")
            || def.send.deviceCmd == QStringLiteral("SampleSuctionSingle")
            || def.send.deviceCmd == QStringLiteral("BleDisconnect")))
        return true;
    if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        if (def.send.action == TestCaseSendAction::Get || def.gate.enabled)
            return true;
    }
    if (isDongleBleConnectStep(def))
        return true;
    if (def.gate.enabled)
        return true;
    if (def.send.action == TestCaseSendAction::Get)
        return true;
    if (stepWaitsForPromptAck(def))
        return true;
    return false;
}

bool TestCaseRunner::isDongleBleConnectStep(const TestCaseDefinition& def) {
    if (def.send.channel != TestCaseSendChannel::Dongle || def.send.action != TestCaseSendAction::Set)
        return false;
    return def.send.deviceCmd == QStringLiteral("BleScanConnect") || def.send.deviceCmd == QStringLiteral("BleDirectConnect");
}

bool TestCaseRunner::stepRequiresProductBle(const TestCaseDefinition& def) {
    if (def.hook.enabled)
        return false;
    if (def.send.channel != TestCaseSendChannel::Product)
        return false;
    // 纯空白提醒（PromptOnly）：不依赖 BLE
    if (def.meta.promptEnabled && def.meta.promptOnly && !def.gate.enabled)
        return false;
    // 兼容旧弹窗提示（未写 PromptOnly）：无卡控时也不强绑 BLE
    if (def.meta.promptEnabled && !def.meta.promptOnly && !def.gate.enabled
        && (def.send.deviceCmd.isEmpty() || def.send.deviceCmd == QStringLiteral("CompensationSet")))
        return false;
    if (def.send.action == TestCaseSendAction::Get)
        return true;
    if (def.gate.enabled)
        return true;
    return true;
}

int TestCaseRunner::commandTimeoutMs(const TestCaseDefinition& def) {
    if (def.timing.commandTimeoutMs > 0)
        return def.timing.commandTimeoutMs;
    if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox
        && def.send.deviceCmd == QStringLiteral("WaitRfInfo"))
        return 10000;
    if (def.send.channel == TestCaseSendChannel::Dongle
        && (def.send.deviceCmd == QStringLiteral("SampleSuctionDual")
            || def.send.deviceCmd == QStringLiteral("SampleSuctionSingle")))
        return 10000;
    if (def.send.channel == TestCaseSendChannel::Fixture
        && def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera)
        return 15000;
    if (def.send.channel == TestCaseSendChannel::Fixture)
        return def.gate.enabled ? 8000 : 5000;
    if (def.send.channel == TestCaseSendChannel::ProductSerial)
        return 30000;
    if (isDongleBleConnectStep(def))
        return 6000;
    return def.gate.enabled ? 8000 : 3000;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
