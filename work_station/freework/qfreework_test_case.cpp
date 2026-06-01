#include "qfreework.h"

#include "test_case.h"

#include <QFile>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

bool QFreeWork::useTestCaseFlow(const QString& stationKey) const {
    QString key = stationKey.trimmed();
    if (key.isEmpty())
        key = SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString().trimmed();
    if (key.isEmpty())
        key = QStringLiteral("default");
    if (!QFile::exists(TestCasePaths::flowIniPath()))
        return false;
    return !TestCaseStore::loadStationItems(key).isEmpty();
}

QStringList QFreeWork::testCaseFlowItems(const QString& stationKey) const {
    QString key = stationKey.trimmed();
    if (key.isEmpty())
        key = SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString().trimmed();
    if (key.isEmpty())
        key = QStringLiteral("default");
    return TestCaseStore::loadStationItems(key);
}

void QFreeWork::setActiveTestCase(const TestCaseDefinition& def) {
    activeTestCase_ = def;
    activeTestCaseStepLabel_ = def.meta.name.trimmed();
    testCaseStepActive_ = true;
    testCaseStepResult_ = {};
}

void QFreeWork::clearActiveTestCase() {
    activeTestCase_ = {};
    activeTestCaseStepLabel_.clear();
    testCaseStepActive_ = false;
    testCaseStepResult_ = {};
}

bool QFreeWork::isActiveTestCaseStep(const QString& stepLabel) const {
    return testCaseStepActive_ && activeTestCaseStepLabel_ == stepLabel.trimmed();
}

bool QFreeWork::evaluateActiveTestCaseGate(const QString& reportType, const QVariant& payload) {
    if (!testCaseStepActive_ || !activeTestCase_.gate.enabled)
        return false;
    if (activeTestCase_.gate.reportType != reportType)
        return false;

    bool pass = true;
    QString detail;
    GateRegistry::evaluate(activeTestCase_.gate, reportType, payload, pass, detail);
    markActiveTestCaseStepDone(pass, detail);
    if (!pass) {
        result = failValue;
        showlog(QStringLiteral("卡控失败：%1").arg(detail));
    } else {
        showlog(QStringLiteral("卡控通过：%1").arg(detail));
    }
    return true;
}

void QFreeWork::markActiveTestCaseStepDone(bool pass, const QString& testData, const QString& ask) {
    testCaseStepResult_.done = true;
    testCaseStepResult_.pass = pass;
    testCaseStepResult_.testData = testData;
    onTestCaseStepMarkedDone(pass, testData, ask);
}

void TestCaseRunner::beginStep(QFreeWork* ctx, const TestCaseDefinition& def) {
    if (!ctx)
        return;
    ctx->setActiveTestCase(def);
    ctx->showlog(QStringLiteral("执行 case：%1").arg(stepLabel(def)));
    if (def.timing.delayBeforeMs > 0)
        ctx->waitWork(def.timing.delayBeforeMs);

    if (def.hook.enabled) {
        TestCaseHookRegistry::invoke(def.hook.hookId, ctx);
        return;
    }

    const auto sendFn = [ctx, def]() {
        DeviceCmd cmd = DeviceCmd::FacMode;
        if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd))
            return;
        if (def.send.action == TestCaseSendAction::Get)
            ctx->protocolManager.get(cmd, def.send.param);
        else
            ctx->protocolManager.set(cmd, def.send.param);
    };

    const int timeoutMs = def.gate.enabled ? 8000 : 3000;
    ctx->sendCommandWithRetry(sendFn, timeoutMs);
}

void registerFreeWorkTestCaseHooks() {
    static bool registered = false;
    if (registered)
        return;
    registered = true;

    TestCaseHookRegistry::registerHook(QStringLiteral("NoOp"), [](QFreeWork* fw) {
        if (!fw)
            return;
        fw->showlog(QStringLiteral("钩子 NoOp 已执行"));
        fw->markActiveTestCaseStepDone(true, QStringLiteral("noop"), QStringLiteral("通过"));
    });
    TestCaseHookRegistry::registerHook(QStringLiteral("FreeWorkNoOpDemo"), [](QFreeWork* fw) {
        if (!fw)
            return;
        fw->showlog(QStringLiteral("示例钩子 FreeWorkNoOpDemo 已执行"));
        fw->markActiveTestCaseStepDone(true, QStringLiteral("hook_ok"), QStringLiteral("通过"));
    });
}
