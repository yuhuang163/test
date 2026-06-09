#include "qfreework.h"

#include "test_case.h"

#include "qat.h"
#include "qfreeworkbox.h"
#include "fixture_uart.h"
#include "protocol/fixture_pcba_uart_protocol.h"
#include "qprotocol_types.h"

#include <QFile>
#include <QTimer>

#include <memory>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

bool isRuntimeMachineIndexPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    return s.isEmpty() || s == QStringLiteral("$INDEX") || s == QStringLiteral("${INDEX}") || s == QStringLiteral("$SLOT") || s == QStringLiteral("${SLOT}") || s == QStringLiteral("{index}");
}

int fixtureMachineIndexFromParam(const QVariant& param) {
    if (param.canConvert<QVariantMap>()) {
        const QVariantMap map = param.toMap();
        const QStringList keys = {QStringLiteral("MachineIndex"), QStringLiteral("machineIndex"),
                                  QStringLiteral("int"), QStringLiteral("value")};
        for (const QString& key : keys) {
            if (!map.contains(key))
                continue;
            const int v = map.value(key).toInt();
            if (v >= 1 && v <= 15)
                return v;
        }
    }
    bool ok = false;
    int idx = param.toInt(&ok);
    if ((!ok || idx <= 0) && param.type() == QVariant::String)
        idx = param.toString().trimmed().toInt(&ok);
    if (idx < 1)
        idx = 1;
    if (idx > 15)
        idx = 15;
    return idx;
}

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
    if (s.compare(QStringLiteral("$TUPLE_PRODUCT_KEY"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${TUPLE_PRODUCT_KEY}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$PRODUCT_KEY"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::ProductKey;
    if (s.compare(QStringLiteral("$TUPLE_DEVICE_NAME"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${TUPLE_DEVICE_NAME}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$DEVICE_NAME"), Qt::CaseInsensitive) == 0)
        return TuplePlaceholderKind::DeviceName;
    if (s.compare(QStringLiteral("$TUPLE_DEVICE_SECRET"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("${TUPLE_DEVICE_SECRET}"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$DEVICE_SECRET"), Qt::CaseInsensitive) == 0)
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

} // namespace

int QFreeWork::resolveFixtureMachineIndex(const QVariant& param) const {
    const QVariant resolved = resolveTestCaseSendParamTree(param);
    if (resolved.userType() == QMetaType::QString && isRuntimeMachineIndexPlaceholder(resolved.toString())) {
        return qBound(1, getIndex(), 15);
    }
    int idx = fixtureMachineIndexFromParam(resolved);
    if (idx <= 0)
        return qBound(1, getIndex(), 15);
    return idx;
}

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
    testCaseMultiGateTableEmitted_ = false;
}

void QFreeWork::clearActiveTestCase() {
    activeTestCase_ = {};
    activeTestCaseStepLabel_.clear();
    testCaseStepActive_ = false;
    testCaseStepResult_ = {};
    testCaseMultiGateTableEmitted_ = false;
}

void QFreeWork::applyRuntimeSnGateExpected(QVector<TestCaseGate>& gates) {
    if (gates.size() != 1 || gates.first().field != QStringLiteral("value") || !gates.first().expected.trimmed().isEmpty())
        return;
    gates[0].expected = resolvedExpectedTailSnText();
}

void QFreeWork::emitFixtureMultiGateTableRows(const QVector<TestCaseGate>& gates, const QString& reportType,
                                              const QVariant& payload, bool& allPass, QString& detailOut) {
    allPass = true;
    detailOut.clear();
    QVector<TestItem> rows;
    rows.reserve(gates.size());
    const QString stepName = activeTestCaseStepLabel_.trimmed();
    QStringList detailParts;
    for (const TestCaseGate& g : gates) {
        TestCaseGate ge = g;
        ge.enabled = true;
        ge.reportType = reportType;
        bool subPass = true;
        QString subDetail;
        GateRegistry::evaluate(ge, reportType, payload, subPass, subDetail);
        if (!subPass)
            allPass = false;
        detailParts.append(QStringLiteral("%1(%2)")
                               .arg(GateRegistry::fieldDisplayName(reportType, ge.field), subDetail));
        TestItem item;
        item.testItem =
            stepName + QLatin1Char('-') + GateRegistry::fieldDisplayName(reportType, ge.field);
        item.testData = subDetail;
        item.ask = GateRegistry::formatGateAsk(ge, reportType);
        item.testResult = subPass ? passValue : failValue;
        rows.append(item);
    }
    detailOut = detailParts.join(QStringLiteral("; "));
    if (!rows.isEmpty()) {
        testResultTableUpdate(rows);
        testCaseMultiGateTableEmitted_ = true;
    }
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
        if (activeTestCase_.send.deviceCmd != QStringLiteral("Sn") || activeTestCase_.send.action != TestCaseSendAction::Get)
            return false;
        if (!payload.canConvert<ProtocolSnData>()) {
            markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
            showlog(QStringLiteral("卡控失败：SN 回传数据类型无效"));
            if (commandRetryTimer)
                finishCommandRetryWait(false, QStringLiteral("卡控失败"));
            return true;
        }
    }

    QVector<TestCaseGate> gatesForEval = TestCaseStore::effectiveGates(activeTestCase_);
    if (gatesForEval.isEmpty()) {
        markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
        showlog(QStringLiteral("卡控失败：未加载到判定项（请检查 case ini 的 Gate/Count 与 Gate/1..N）"));
        if (commandRetryTimer)
            finishCommandRetryWait(false, QStringLiteral("卡控失败"));
        return true;
    }

    applyRuntimeSnGateExpected(gatesForEval);

    bool pass = true;
    QString detail;
    const bool multiFieldMode = TestCaseStore::usesMultiFieldGates(activeTestCase_);
    const bool fixtureTableMode = reportType == QStringLiteral("ProtocolFixturePcbaData") && multiFieldMode;
    if (fixtureTableMode) {
        emitFixtureMultiGateTableRows(gatesForEval, reportType, payload, pass, detail);
    } else if (gatesForEval.size() > 1) {
        GateRegistry::evaluateAll(gatesForEval, reportType, payload, pass, detail);
    } else {
        GateRegistry::evaluate(gatesForEval.first(), reportType, payload, pass, detail);
    }

    GateStepDisplay display =
        GateRegistry::formatStepDisplay(gatesForEval.first(), gatesForEval, reportType, payload, multiFieldMode);
    if (display.testData.isEmpty())
        display.testData = detail;

    markActiveTestCaseStepDone(pass, display.testData, display.ask);
    if (commandRetryTimer) {
        finishCommandRetryWait(pass, pass ? QStringLiteral("卡控通过，步骤完成") : QStringLiteral("卡控失败"));
    }
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

    if (def.send.channel == TestCaseSendChannel::Product && !ctx->at->getConnected() && !TestCaseRunner::stepRequiresProductBle(def)) {
        ctx->showlog(QStringLiteral("本步不要求蓝牙连接，已跳过产品协议，请点「是」或关闭弹窗后继续"));
        if (!TestCaseRunner::stepWaitsForPromptAck(def))
            ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Fixture) {
        ctx->executeFixturePcbaCase(def);
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

void QFreeWork::executeFixturePcbaCase(const TestCaseDefinition& def) {
    if (def.send.fixtureProtocol != TestCaseFixtureProtocol::Pcba) {
        showlog(QStringLiteral("暂不支持的治具协议类型"));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    FixturePcbaCmd cmd;
    if (!FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, cmd)) {
        showlog(QStringLiteral("未知治具 PCBA 指令：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }
    if (!FixturePcbaCmdCatalog::isCmdForAction(cmd, def.send.action)) {
        showlog(QStringLiteral("治具指令与操作方式不匹配：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    auto* box = qobject_cast<QFreeWorkBox*>(window());
    Fixture_uart* uart = box ? box->fixtureUartWidget() : nullptr;
    if (!uart || !uart->isFixtureSerialOpen()) {
        showlog(QStringLiteral("治具串口未连接，请从菜单打开「连接治具串口」"));
        markActiveTestCaseStepDone(false, QStringLiteral("治具未连接"), QStringLiteral("失败"));
        return;
    }

    const int machineIndex = resolveFixtureMachineIndex(def.send.param);

    if (def.send.action == TestCaseSendAction::Set) {
        QByteArray frame;
        switch (cmd) {
        case FixturePcbaCmd::StartTest:
            frame = FixturePcbaUartProtocol::buildStartTestCommand(machineIndex);
            break;
        case FixturePcbaCmd::StartSleep:
            frame = FixturePcbaUartProtocol::buildSleepCommand(machineIndex);
            break;
        case FixturePcbaCmd::StartWhiteMode:
            frame = FixturePcbaUartProtocol::buildWhiteModeCommand(machineIndex);
            break;
        default:
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        if (frame.isEmpty()) {
            showlog(QStringLiteral("治具 PCBA 组包失败：机位 %1（有效范围 1~15）").arg(machineIndex));
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        uart->sendPcbaFrame(frame);
        showlog(QStringLiteral("已发送治具 PCBA：%1，机位 %2，帧 %3")
                    .arg(FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(def.send.deviceCmd))
                    .arg(machineIndex)
                    .arg(QString::fromLatin1(frame.toHex(' ').toUpper())));
        if (!def.gate.enabled)
            markActiveTestCaseStepDone(true, QString::number(machineIndex), QStringLiteral("通过"));
        return;
    }

    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
    auto* timeoutTimer = new QTimer(this);
    timeoutTimer->setSingleShot(true);
    timeoutTimer->setInterval(timeoutMs);
    const auto waitTimerStopped = std::make_shared<bool>(false);

    const auto stopWaitTimer = [timeoutTimer, waitTimerStopped]() {
        if (*waitTimerStopped)
            return;
        *waitTimerStopped = true;
        timeoutTimer->disconnect();
        timeoutTimer->stop();
        timeoutTimer->deleteLater();
    };

    const auto onFixtureTimeout = [this, def, stopWaitTimer]() {
        if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
            return;
        stopWaitTimer();
        showlog(QStringLiteral("治具等待超时：%1").arg(def.send.deviceCmd));
        markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
    };
    connect(timeoutTimer, &QTimer::timeout, this, onFixtureTimeout);

    const auto handlePacket = [this, def, stopWaitTimer](const FixturePacketData& pack) {
        if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
            return;
        stopWaitTimer();
        const QVariant payload = QVariant::fromValue(pack);
        if (def.gate.enabled && evaluateActiveTestCaseGate(QStringLiteral("ProtocolFixturePcbaData"), payload))
            return;
        const QString detail =
            QStringLiteral("机号=%1 静态=%2uA 工作=%3mA")
                .arg(pack.machineNumber)
                .arg(pack.staticCurrent)
                .arg(pack.workingCurrent);
        markActiveTestCaseStepDone(true, detail, QStringLiteral("通过"));
        showlog(QStringLiteral("治具回包：%1").arg(detail));
    };

    if (cmd == FixturePcbaCmd::WaitFixturePacket) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine, this,
                           [connPtr, handlePacket](const FixturePacketData& pack) {
                               QObject::disconnect(*connPtr);
                               handlePacket(pack);
                           });
    } else if (cmd == FixturePcbaCmd::WaitSleepRequest) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine_sleep, this,
                           [connPtr, handlePacket](const FixturePacketData& pack) {
                               QObject::disconnect(*connPtr);
                               handlePacket(pack);
                           });
    } else if (cmd == FixturePcbaCmd::WaitStartTestAck) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine_start, this,
                           [this, def, stopWaitTimer, connPtr]() {
                               QObject::disconnect(*connPtr);
                               if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
                                   return;
                               stopWaitTimer();
                               if (!def.gate.enabled) {
                                   markActiveTestCaseStepDone(true, QStringLiteral("开始测试应答"),
                                                              QStringLiteral("通过"));
                                   showlog(QStringLiteral("收到治具开始测试应答"));
                               } else {
                                   showlog(QStringLiteral(
                                       "WaitStartTestAck 不支持卡控，请关闭卡控或改用 WaitFixturePacket"));
                                   markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
                               }
                           });
    } else {
        stopWaitTimer();
        markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
        return;
    }

    timeoutTimer->start();
    showlog(QStringLiteral("等待治具回包：%1（超时 %2ms）")
                .arg(FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(def.send.deviceCmd))
                .arg(timeoutMs));
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
