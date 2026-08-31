#include "test_case.h"

#include "test_case_send_dispatch.h"

#include <QFile>
#include <QHash>
#include <QSet>
#include <QStringList>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
//「步骤配置能不能保存」+「预置流程 Hook 怎么挂」+「跑起来时这一步该怎么等、超时多久、要不要 BLE」。

// ===================== TestCaseValidator =====================

bool TestCaseValidator::validateCase(const TestCaseDefinition& def, QStringList& errors) {
    errors.clear();
    QString nameErr;
    if (!TestCasePaths::isValidCaseFileName(def.meta.name, &nameErr))
        errors.append(nameErr);
    if (def.meta.mesTag.trimmed().isEmpty())
        errors.append(QStringLiteral("「上报MES的字段」不能为空（测试项信息）"));

    // 纯空白提醒 / 仅等 Gate 上报 / 预置流程 Hook：可不填有效产品指令
    const bool hookStep = def.hook.enabled;
    const bool promptOnlyStep = def.meta.promptEnabled && def.meta.promptOnly && !hookStep;
    const bool gateWaitOnlyStep =
        !hookStep && def.meta.promptOnly && def.gate.enabled && def.send.deviceCmd.trimmed().isEmpty();
    if (hookStep) {
        // Send/DeviceCmd 可为占位 Hook，不校验产品指令 catalog
    } else if (promptOnlyStep || gateWaitOnlyStep) {
        // 不校验 / 不要求测试指令
    } else if (def.send.deviceCmd.trimmed().compare(QLatin1String("Hook"), Qt::CaseInsensitive) == 0) {
        errors.append(QStringLiteral("该步骤 Send/DeviceCmd=Hook，请在「预置流程」勾选启用并选择流程类型（如 MAC_WRITE_ROOT）"));
    } else if (def.send.deviceCmd.isEmpty()) {
        errors.append(QStringLiteral("请选择测试指令"));
    } else if (def.send.channel == TestCaseSendChannel::Modbus || def.send.channel == TestCaseSendChannel::Scpi) {
        TestCaseSendDispatch::appendModbusScpiValidationErrors(def.send, errors);
    } else {
        TestCaseSendDispatch::appendCatalogValidationErrors(def.send, errors);
    }

    if (def.timing.delayBeforeMs < 0 || def.timing.delayAfterMs < 0)
        errors.append(QStringLiteral("延时不能为负数"));
    if (def.timing.commandTimeoutMs < 0)
        errors.append(QStringLiteral("指令超时不能为负数"));
    if (def.timing.commandTimeoutMs > 0 && def.timing.commandTimeoutMs < 100)
        errors.append(QStringLiteral("指令超时须为 0（自动）或不少于 100 毫秒"));

    if (def.meta.promptEnabled && def.meta.promptText.trimmed().isEmpty())
        errors.append(QStringLiteral("已勾选操作提示时须填写提示文字"));

    if (def.gate.enabled) {
        GateTypeDescriptor desc;
        if (!GateRegistry::descriptorFor(def.gate.reportType, desc)) {
            errors.append(QStringLiteral("回传数据类型未登记，请联系工程师"));
        } else if (TestCaseStore::usesMultiFieldGates(def)) {
            const QStringList fields = GateRegistry::fieldsFor(def.gate.reportType);
            for (const TestCaseGate& g : def.gates) {
                if (!fields.contains(g.field))
                    errors.append(QStringLiteral("分项判定字段未登记：%1").arg(g.field));
                if (g.op == TestCaseGateOp::Eq && g.expected.trimmed().isEmpty())
                    errors.append(QStringLiteral("分项「%1」须填写期望值")
                                      .arg(GateRegistry::fieldDisplayName(def.gate.reportType, g.field)));
            }
        } else if (!GateRegistry::isAllFieldsGateField(def.gate.field) && !GateRegistry::fieldsFor(def.gate.reportType).contains(def.gate.field)) {
            errors.append(QStringLiteral("判定项目未登记，请联系工程师"));
        }
    }

    if (def.hook.enabled && !TestCaseHookRegistry::contains(def.hook.hookId))
        errors.append(QStringLiteral("预置流程类型无效，请联系工程师"));

    const QString libraryPath = TestCasePaths::stepLibraryPath(def.meta.name);
    const QString legacyPath = TestCasePaths::caseIniPath(def.meta.name);
    if (QFile::exists(libraryPath) || QFile::exists(legacyPath)) {
        TestCaseDefinition existing;
        TestCaseStore::loadCase(def.meta.name, existing);
        Q_UNUSED(existing);
    }

    return errors.isEmpty();
}

bool TestCaseValidator::validateFlowMesTags(const QString& stationKey, const QVector<TestFlowItemEntry>& entries,
                                            QStringList& errors, const QString& overrideOriginalCaseName,
                                            const TestCaseDefinition* overrideDef) {
    errors.clear();
    const QString key = stationKey.trimmed();
    if (key.isEmpty()) {
        errors.append(QStringLiteral("工站无效，无法校验上报MES的字段"));
        return false;
    }

    struct StepMesInfo {
        int stepNo = 0;
        QString stepName;
        QString mesTag;
    };
    QVector<StepMesInfo> steps;
    steps.reserve(entries.size() + 1);
    bool overrideConsumed = false;
    const QString overrideOrig = overrideOriginalCaseName.trimmed();

    for (int i = 0; i < entries.size(); ++i) {
        const TestFlowItemEntry& entry = entries.at(i);
        const QString caseName = entry.caseName.trimmed();
        if (caseName.isEmpty())
            continue;

        StepMesInfo info;
        info.stepNo = i + 1;
        const bool useOverride =
            overrideDef
            && (( !overrideOrig.isEmpty() && caseName == overrideOrig)
                || caseName == overrideDef->meta.name.trimmed());
        if (useOverride) {
            info.stepName = overrideDef->meta.name.trimmed().isEmpty() ? caseName : overrideDef->meta.name.trimmed();
            info.mesTag = overrideDef->meta.mesTag.trimmed();
            overrideConsumed = true;
        } else {
            TestCaseDefinition def;
            if (!TestCaseStore::loadCaseForStation(key, caseName, def)) {
                errors.append(QStringLiteral("第%1步「%2」：无法读取步骤配置，请先保存该步骤")
                                  .arg(info.stepNo)
                                  .arg(caseName));
                continue;
            }
            info.stepName = def.meta.name.trimmed().isEmpty() ? caseName : def.meta.name.trimmed();
            info.mesTag = def.meta.mesTag.trimmed();
        }
        steps.append(info);
    }

    // 新步骤尚未写入流程列表时，仍纳入本次校验
    if (overrideDef && !overrideConsumed) {
        StepMesInfo info;
        info.stepNo = steps.size() + 1;
        info.stepName = overrideDef->meta.name.trimmed();
        if (info.stepName.isEmpty())
            info.stepName = QStringLiteral("新步骤");
        info.mesTag = overrideDef->meta.mesTag.trimmed();
        steps.append(info);
    }

    QHash<QString, QSet<QString>> tagToDistinctNames;
    QHash<QString, QStringList> tagToStepLabels;
    for (const StepMesInfo& info : steps) {
        const QString stepLabel =
            QStringLiteral("第%1步「%2」").arg(info.stepNo).arg(info.stepName);
        if (info.mesTag.isEmpty()) {
            errors.append(QStringLiteral("%1：未填写「上报MES的字段」").arg(stepLabel));
            continue;
        }
        // 同名步骤在流程中重复出现（如多频点 RX）允许共用同一 MesTag
        tagToDistinctNames[info.mesTag].insert(info.stepName);
        tagToStepLabels[info.mesTag].append(stepLabel);
    }
    for (auto it = tagToDistinctNames.constBegin(); it != tagToDistinctNames.constEnd(); ++it) {
        if (it.value().size() < 2)
            continue;
        errors.append(QStringLiteral("「上报MES的字段」重复为「%1」，冲突步骤：%2")
                          .arg(it.key(), tagToStepLabels.value(it.key()).join(QStringLiteral("、"))));
    }
    return errors.isEmpty();
}

// ===================== TestCaseHookRegistry =====================

static QHash<QString, TestCaseHookFn>& hooks() {
    static QHash<QString, TestCaseHookFn> table;
    return table;
}

void TestCaseHookRegistry::registerHook(const QString& hookId, TestCaseHookFn fn) {
    if (hookId.trimmed().isEmpty() || !fn)
        return;
    hooks().insert(hookId.trimmed(), std::move(fn));
}

bool TestCaseHookRegistry::contains(const QString& hookId) {
    return hooks().contains(hookId.trimmed());
}

QStringList TestCaseHookRegistry::hookIds() {
    return hooks().keys();
}

bool TestCaseHookRegistry::invoke(const QString& hookId, QFreeWork* ctx) {
    const auto it = hooks().constFind(hookId.trimmed());
    if (it == hooks().cend() || !ctx)
        return false;
    it.value()(ctx);
    return true;
}

// ===================== TestCaseRunner =====================

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
