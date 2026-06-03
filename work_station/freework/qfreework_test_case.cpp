#include "qfreework.h"

#include "test_case.h"

#include "qat.h"
#include "qprotocol_types.h"

#include <QFile>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

bool isRuntimeMacPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    return s.isEmpty() || s == QStringLiteral("$MAC") || s == QStringLiteral("${MAC}") || s == QStringLiteral("{mac}");
}

enum class TuplePlaceholderKind {
    None,
    ProductKey,
    DeviceName,
    DeviceSecret,
};

TuplePlaceholderKind tuplePlaceholderKind(const QString& text) {
    const QString s = text.trimmed();
    if (s.compare(QStringLiteral("$TUPLE_PRODUCT_KEY"), Qt::CaseInsensitive) == 0
        || s.compare(QStringLiteral("${TUPLE_PRODUCT_KEY}"), Qt::CaseInsensitive) == 0
        || s.compare(QStringLiteral("$PRODUCT_KEY"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::ProductKey;
    if (s.compare(QStringLiteral("$TUPLE_DEVICE_NAME"), Qt::CaseInsensitive) == 0
        || s.compare(QStringLiteral("${TUPLE_DEVICE_NAME}"), Qt::CaseInsensitive) == 0
        || s.compare(QStringLiteral("$DEVICE_NAME"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::DeviceName;
    if (s.compare(QStringLiteral("$TUPLE_DEVICE_SECRET"), Qt::CaseInsensitive) == 0
        || s.compare(QStringLiteral("${TUPLE_DEVICE_SECRET}"), Qt::CaseInsensitive) == 0
        || s.compare(QStringLiteral("$DEVICE_SECRET"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::DeviceSecret;
    return TuplePlaceholderKind::None;
}

bool paramTreeReferencesTuplePlaceholder(const QVariant& param) {
    if (param.userType() == QMetaType::QString)
        return tuplePlaceholderKind(param.toString()) != TuplePlaceholderKind::None;
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            if (paramTreeReferencesTuplePlaceholder(it.value()))
                return true;
        }
    }
    if (param.type() == QVariant::List) {
        const QVariantList list = param.toList();
        for (const QVariant& item : list) {
            if (paramTreeReferencesTuplePlaceholder(item))
                return true;
        }
    }
    return false;
}

}  // namespace

QString QFreeWork::resolveTestCaseSendPlaceholder(const QString& text) const {
    if (isRuntimeMacPlaceholder(text))
        return currentMacAddress();
    switch (tuplePlaceholderKind(text)) {
    case TuplePlaceholderKind::ProductKey:
        return tupleData_.productKey;
    case TuplePlaceholderKind::DeviceName:
        return tupleData_.deviceName;
    case TuplePlaceholderKind::DeviceSecret:
        return tupleData_.deviceSecret;
    default:
        break;
    }
    return text;
}

QVariant QFreeWork::resolveTestCaseSendParamTree(const QVariant& param) const {
    if (param.userType() == QMetaType::QString)
        return resolveTestCaseSendPlaceholder(param.toString());
    if (param.canConvert<QVariantMap>()) {
        QVariantMap map = param.toMap();
        for (auto it = map.begin(); it != map.end(); ++it)
            it.value() = resolveTestCaseSendParamTree(it.value());
        return map;
    }
    if (param.type() == QVariant::List) {
        QVariantList list = param.toList();
        for (int i = 0; i < list.size(); ++i)
            list[i] = resolveTestCaseSendParamTree(list[i]);
        return list;
    }
    return param;
}

bool QFreeWork::prepareTupleProductWriteForTestCase(const TestCaseDefinition& def, DeviceCmd cmd,
                                                    const QVariant& wireParam) {
    if (!paramTreeReferencesTuplePlaceholder(def.send.param))
        return true;

    const QString stepName = def.meta.displayName.trimmed().isEmpty() ? def.meta.name.trimmed()
                                                                       : def.meta.displayName.trimmed();
    if (failTupleWriteIfNoValidField(stepName, tupleData_.success, QStringLiteral("云端三元组未获取成功")))
        return false;

    QString writeText;
    if (cmd == DeviceCmd::Sn && wireParam.canConvert<DeviceSnPayload>()) {
        writeText = QString::fromUtf8(wireParam.value<DeviceSnPayload>().sn);
    } else if (cmd == DeviceCmd::WriteKey) {
        writeText = QString::fromUtf8(wireParam.toMap().value(QStringLiteral("value")).toByteArray());
    }

    if (writeText.trimmed().isEmpty()) {
        failTupleWriteIfNoValidField(stepName, false, QStringLiteral("三元组字段为空"));
        return false;
    }
    stepRuntime_.testData = writeText;
    return true;
}

QString QFreeWork::currentMacAddress() const {
    if (!macAddress.trimmed().isEmpty() && macAddress != QStringLiteral("没有mac地址"))
        return macAddress.trimmed();
    if (ui && ui->macInput)
        return ui->macInput->text().trimmed();
    return macAddress.trimmed();
}

bool QFreeWork::useTestCaseFlow(const QString& stationKey) const {
    QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        key = TestCaseStore::resolveFlowStationKey(SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString());
    if (TestCaseStore::loadStationItems(key).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(
            SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStationName")).toString());
        if (!byName.isEmpty())
            key = byName;
    }
    if (key.isEmpty())
        key = QStringLiteral("default");
    if (!QFile::exists(TestCasePaths::flowIniPath()))
        return false;
    return !TestCaseStore::loadStationItems(key).isEmpty();
}

QStringList QFreeWork::testCaseFlowItems(const QString& stationKey) const {
    QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    if (key.isEmpty())
        key = TestCaseStore::resolveFlowStationKey(SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStation")).toString());
    if (TestCaseStore::loadStationItems(key).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(
            SETTINGS.value(QStringLiteral("TestOrderMeta/SelectedStationName")).toString());
        if (!byName.isEmpty())
            key = byName;
    }
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

    if (reportType == QStringLiteral("ProtocolSnData")) {
        if (activeTestCase_.send.deviceCmd != QStringLiteral("Sn")
            || activeTestCase_.send.action != TestCaseSendAction::Get)
            return false;
        if (!payload.canConvert<ProtocolSnData>()) {
            markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
            showlog(QStringLiteral("卡控失败：SN 回传数据类型无效"));
            return true;
        }
    }

    QVector<TestCaseGate> gatesForEval = TestCaseStore::effectiveGates(activeTestCase_);
    if (gatesForEval.isEmpty()) {
        if (activeTestCase_.gate.enabled) {
            markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
            showlog(QStringLiteral("卡控失败：未加载到判定项（请检查 case ini 的 Gate/Count 与 Gate/1..N）"));
        }
        return activeTestCase_.gate.enabled;
    }

    if (reportType == QStringLiteral("ProtocolSnData") && gatesForEval.size() == 1
        && gatesForEval.first().field == QStringLiteral("value")
        && gatesForEval.first().expected.trimmed().isEmpty()) {
        gatesForEval[0].expected = QString::fromUtf8(expectedTailSnFromMes.trimmed());
        if (gatesForEval[0].expected.isEmpty() && ui && ui->getMac)
            gatesForEval[0].expected = ui->getMac->text().trimmed();
    }

    bool pass = true;
    QString detail;
    if (gatesForEval.size() > 1)
        GateRegistry::evaluateAll(gatesForEval, reportType, payload, pass, detail);
    else
        GateRegistry::evaluate(gatesForEval.first(), reportType, payload, pass, detail);

    const TestCaseGate& primaryGate = gatesForEval.first();
    QString testData = detail;
    QString ask;
    if (reportType == QStringLiteral("ProtocolSnData") && payload.canConvert<ProtocolSnData>()) {
        testData = payload.value<ProtocolSnData>().value.trimmed();
        ask = primaryGate.expected.trimmed();
    } else if (reportType == QStringLiteral("ProtocolRssiData") && payload.canConvert<ProtocolRssiData>()) {
        testData = QString::number(payload.value<ProtocolRssiData>().dbm);
        if (primaryGate.op == TestCaseGateOp::Range) {
            double low = primaryGate.low;
            double high = primaryGate.high;
            GateRegistry::resolveRangeBounds(primaryGate, low, high);
            ask = QStringLiteral("[%1,%2]").arg(low).arg(high);
        }
    } else if (reportType == QStringLiteral("ProtocolBatteryData") && payload.canConvert<ProtocolBatteryData>()) {
        testData = QString::number(payload.value<ProtocolBatteryData>().percent);
        if (primaryGate.op == TestCaseGateOp::Range) {
            double low = primaryGate.low;
            double high = primaryGate.high;
            GateRegistry::resolveRangeBounds(primaryGate, low, high);
            ask = QStringLiteral("[%1,%2]").arg(low).arg(high);
        }
    } else if (reportType == QStringLiteral("ProtocolBaseInfoData") && payload.canConvert<ProtocolBaseInfoData>()) {
        const ProtocolBaseInfoData base = payload.value<ProtocolBaseInfoData>();
        if (primaryGate.field == QStringLiteral("soft_version"))
            testData = base.soft_version.trimmed();
        else if (primaryGate.field == QStringLiteral("res_version"))
            testData = base.res_version.trimmed();
        else if (primaryGate.field == QStringLiteral("product_name"))
            testData = base.product_name.trimmed();
    } else if (reportType == QStringLiteral("ProtocolPeriphStateData")
               && payload.canConvert<ProtocolPeriphStateData>()) {
        const ProtocolPeriphStateData periph = payload.value<ProtocolPeriphStateData>();
        testData = QStringLiteral("press0=%1;press1=%2;battery=%3;touch=%4;led=%5;pd=%6")
                       .arg(periph.press0_state)
                       .arg(periph.press1_state)
                       .arg(periph.battery_ic_state)
                       .arg(periph.touch_ic_state)
                       .arg(periph.led_ic_state)
                       .arg(periph.pd_ic_state);
        if (TestCaseStore::usesMultiFieldGates(activeTestCase_)) {
            QStringList expectedParts;
            for (const TestCaseGate& g : gatesForEval)
                expectedParts.append(QStringLiteral("%1=%2")
                                         .arg(GateRegistry::fieldDisplayName(reportType, g.field), g.expected));
            ask = expectedParts.join(QLatin1Char(';'));
        } else if (primaryGate.op == TestCaseGateOp::Eq) {
            ask = primaryGate.expected;
        } else if (primaryGate.op == TestCaseGateOp::Range) {
            double low = primaryGate.low;
            double high = primaryGate.high;
            GateRegistry::resolveRangeBounds(primaryGate, low, high);
            ask = QStringLiteral("[%1,%2]").arg(low).arg(high);
        }
    }

    markActiveTestCaseStepDone(pass, testData, ask);
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

    if (def.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (!TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd)) {
            ctx->showlog(QStringLiteral("未知云端指令：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        if (!TupleCmdCatalog::isCmdForAction(tupleCmd, def.send.action)) {
            ctx->showlog(QStringLiteral("云端指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        ctx->executeCloudTupleCase(def);
        return;
    }

    if (def.send.channel == TestCaseSendChannel::ProductSerial) {
        ctx->executeProductSerialCase(def);
        return;
    }

    DongleCmd dongleCmd = DongleCmd::BleScanConnect;
    if (def.send.channel == TestCaseSendChannel::Dongle) {
        if (!DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd)) {
            ctx->showlog(QStringLiteral("未知 Dongle 指令：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        const auto sendFn = [ctx, def, dongleCmd]() {
            const QVariant param = ctx->resolveTestCaseSendParamTree(def.send.param);
            if (def.send.action == TestCaseSendAction::Get)
                ctx->at->get(dongleCmd, param);
            else
                ctx->at->set(dongleCmd, param);
        };
        const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
        ctx->setCommandWaitSource(CommandWaitSource::DongleAt);
        ctx->sendCommandWithRetry(sendFn, timeoutMs);
        return;
    }

    DeviceCmd cmd = DeviceCmd::FacMode;
    if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
        ctx->showlog(QStringLiteral("未知指令：%1").arg(def.send.deviceCmd));
        ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    const QVariant resolvedParam = ctx->resolveTestCaseSendParamTree(def.send.param);
    const QVariant wireParam = DeviceCmdCatalog::normalizeSendParam(cmd, resolvedParam);
    if (def.send.action == TestCaseSendAction::Set && !ctx->prepareTupleProductWriteForTestCase(def, cmd, wireParam)) {
        ctx->markActiveTestCaseStepDone(false, ctx->activeTestCaseStepTestData(), QStringLiteral("失败"));
        return;
    }

    const auto sendFn = [ctx, def, cmd, wireParam]() {
        if (def.send.action == TestCaseSendAction::Get)
            ctx->protocolManager.get(cmd, wireParam);
        else
            ctx->protocolManager.set(cmd, wireParam);
    };

    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    ctx->setCommandWaitSource(CommandWaitSource::ProductProtocol);
    ctx->sendCommandWithRetry(sendFn, timeoutMs);
}

void QFreeWork::executeProductSerialCase(const TestCaseDefinition& def) {
    ProductSerialCmd serialCmd;
    if (!ProductSerialCmdCatalog::productSerialCmdFromName(def.send.deviceCmd, serialCmd)) {
        showlog(QStringLiteral("未知产品串口指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!ProductSerialCmdCatalog::isCmdForAction(serialCmd, def.send.action)) {
        showlog(QStringLiteral("产品串口指令仅支持设置：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    switch (serialCmd) {
    case ProductSerialCmd::InstrumentReset:
        startProductInstrumentResetAndWaitAck(QString());
        break;
    case ProductSerialCmd::StopRxAndPer:
        startProductInstrumentStopReceiveAndPer(QString());
        break;
    default: {
        const int profile = ProductSerialCmdCatalog::brushProfileForCmd(serialCmd);
        if (profile < 0) {
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        startProductInstrumentStartReceiveForCatalog(QString(), profile);
        break;
    }
    }
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
