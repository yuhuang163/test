#include "qfreework.h"

#include "test_case.h"

#include "qatmanager.h"
#include "qfreeworkbox.h"
#include "fixture_uart.h"
#include "fixture_pcba_device.h"
#include "qprotocol_types.h"

#include <QFile>
#include <QTimer>
#include <QElapsedTimer>

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

bool isRuntimeSnPlaceholder(const QString& text) {
    const QString s = text.trimmed();
    return s == QStringLiteral("$SN") || s == QStringLiteral("${SN}") || s == QStringLiteral("{sn}");
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

PlcCmd plcCmdFromName(const QString& name) {
    if (name == QLatin1String("Connect"))
        return PlcCmd::Connect;
    if (name == QLatin1String("Disconnect"))
        return PlcCmd::Disconnect;
    if (name == QLatin1String("IsConnected"))
        return PlcCmd::IsConnected;
    if (name == QLatin1String("ReadCoil"))
        return PlcCmd::ReadCoil;
    if (name == QLatin1String("WriteCoil"))
        return PlcCmd::WriteCoil;
    if (name == QLatin1String("ReadCoils"))
        return PlcCmd::ReadCoils;
    if (name == QLatin1String("WaitCoilTrue"))
        return PlcCmd::WaitCoilTrue;
    if (name == QLatin1String("WaitCoilFalse"))
        return PlcCmd::WaitCoilFalse;
    if (name == QLatin1String("SendStepDone"))
        return PlcCmd::SendStepDone;
    return PlcCmd::IsConnected;
}

HqAmmeterRtuCmd hqAmmeterRtuCmdFromName(const QString& name) {
    if (name == QLatin1String("ReadMeasurement"))
        return HqAmmeterRtuCmd::ReadMeasurement;
    if (name == QLatin1String("SetBaud115200"))
        return HqAmmeterRtuCmd::SetBaud115200;
    return HqAmmeterRtuCmd::ReadMeasurement;
}

LxAmmeterRtuCmd lxAmmeterRtuCmdFromName(const QString& name) {
    if (name == QLatin1String("ReadMeasurement"))
        return LxAmmeterRtuCmd::ReadMeasurement;
    return LxAmmeterRtuCmd::ReadMeasurement;
}

HuilingScpiCmd huilingScpiCmdFromName(const QString& name) {
    if (name == QLatin1String("ConfigureMeasure"))
        return HuilingScpiCmd::ConfigureMeasure;
    if (name == QLatin1String("ReadMeasureCurrent"))
        return HuilingScpiCmd::ReadMeasureCurrent;
    if (name == QLatin1String("ReadMeasureConfiguration"))
        return HuilingScpiCmd::ReadMeasureConfiguration;
    if (name == QLatin1String("InitializeDevice"))
        return HuilingScpiCmd::InitializeDevice;
    if (name == QLatin1String("ConfigureProgrammablePower"))
        return HuilingScpiCmd::ConfigureProgrammablePower;
    if (name == QLatin1String("ProgrammablePowerOutput"))
        return HuilingScpiCmd::ProgrammablePowerOutput;
    if (name == QLatin1String("ReadProgrammableVoltage"))
        return HuilingScpiCmd::ReadProgrammableVoltage;
    if (name == QLatin1String("ReadProgrammableCurrent"))
        return HuilingScpiCmd::ReadProgrammableCurrent;
    if (name == QLatin1String("InitializeProgrammablePower"))
        return HuilingScpiCmd::InitializeProgrammablePower;
    if (name == QLatin1String("SendRawLine"))
        return HuilingScpiCmd::SendRawLine;
    return HuilingScpiCmd::InitializeDevice;
}

CmwScpiCmd cmwScpiCmdFromName(const QString& name) {
    if (name == QLatin1String("ClearStatus"))
        return CmwScpiCmd::ClearStatus;
    if (name == QLatin1String("GenOff"))
        return CmwScpiCmd::GenOff;
    if (name == QLatin1String("GenOn"))
        return CmwScpiCmd::GenOn;
    if (name == QLatin1String("ListOff"))
        return CmwScpiCmd::ListOff;
    if (name == QLatin1String("BbModeArb"))
        return CmwScpiCmd::BbModeArb;
    if (name == QLatin1String("ArbFile"))
        return CmwScpiCmd::ArbFile;
    if (name == QLatin1String("ArbRepetition"))
        return CmwScpiCmd::ArbRepetition;
    if (name == QLatin1String("ArbCycles"))
        return CmwScpiCmd::ArbCycles;
    if (name == QLatin1String("TxLevelDbm"))
        return CmwScpiCmd::TxLevelDbm;
    if (name == QLatin1String("FrequencyMhz"))
        return CmwScpiCmd::FrequencyMhz;
    if (name == QLatin1String("ManualArbTrigger"))
        return CmwScpiCmd::ManualArbTrigger;
    if (name == QLatin1String("WriteLine"))
        return CmwScpiCmd::WriteLine;
    if (name == QLatin1String("Identity"))
        return CmwScpiCmd::Identity;
    if (name == QLatin1String("ArbFilePath"))
        return CmwScpiCmd::ArbFilePath;
    if (name == QLatin1String("ArbScount"))
        return CmwScpiCmd::ArbScount;
    if (name == QLatin1String("GenState"))
        return CmwScpiCmd::GenState;
    if (name == QLatin1String("SystemError"))
        return CmwScpiCmd::SystemError;
    if (name == QLatin1String("QueryLine"))
        return CmwScpiCmd::QueryLine;
    return CmwScpiCmd::ClearStatus;
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
    if (isRuntimeSnPlaceholder(text))
        return resolvedExpectedTailSnText();
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
        if (!g.enabled)
            continue;
        TestCaseGate ge = g;
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

    QVector<TestCaseGate> gatesForEval = TestCaseStore::activeGatesForEvaluation(activeTestCase_);
    if (gatesForEval.isEmpty()) {
        markActiveTestCaseStepDone(false, QStringLiteral("-"), QStringLiteral("失败"));
        showlog(QStringLiteral("卡控失败：未启用任何判定项（请在 case ini 的 Gate/N/Enabled 勾选）"));
        if (commandRetryTimer)
            finishCommandRetryWait(false, QStringLiteral("卡控失败"));
        return true;
    }

    applyRuntimeSnGateExpected(gatesForEval);

    bool pass = true;
    QString detail;
    const bool multiFieldMode = gatesForEval.size() > 1;
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

    if (def.send.channel == TestCaseSendChannel::Modbus) {
        const QString deviceKey = def.send.device;
        ModbusDeviceRoute devRoute = ModbusPeriphCmdCatalog::deviceFromIni(deviceKey);
        ctx->modbusManager.setDeviceRoute(devRoute);

        const QVariant resolvedParam = ctx->resolveTestCaseSendParamTree(def.send.param);
        QString errStr;

        if (devRoute == ModbusDeviceRoute::InovanceH5uTcp) {
            PlcCmd plcCmd = plcCmdFromName(def.send.deviceCmd);
            QVariant resultVal;
            bool ok = ctx->modbusManager.exec(plcCmd, resolvedParam, &resultVal, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("PLC 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                if (def.send.action == TestCaseSendAction::Get) {
                    ProtocolMeasureData measureData;
                    measureData.deviceName = deviceKey;
                    measureData.type = QStringLiteral("PLC_Register");
                    measureData.value = resultVal.toDouble();
                    measureData.valueText = resultVal.toString();
                    measureData.unit = QStringLiteral("");
                    measureData.isOk = true;
                    ctx->onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                              QVariant::fromValue(measureData)));
                } else {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
                }
            }
        } else if (devRoute == ModbusDeviceRoute::HqAmmeterRtu) {
            HqAmmeterRtuCmd cmd = hqAmmeterRtuCmdFromName(def.send.deviceCmd);
            bool ok = ctx->modbusManager.exec(cmd, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("HQ 电流表指令 [%1] 下发失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else if (def.send.action == TestCaseSendAction::Get) {
                const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                    if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                        return;
                    ctx->showlog(QStringLiteral("HQ 电流表等待超时：%1").arg(def.send.deviceCmd));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                });
                ctx->showlog(QStringLiteral("等待 HQ 电流表回包：%1（超时 %2ms）").arg(def.send.deviceCmd).arg(timeoutMs));
            } else {
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else if (devRoute == ModbusDeviceRoute::LxAmmeterRtu) {
            LxAmmeterRtuCmd cmd = lxAmmeterRtuCmdFromName(def.send.deviceCmd);
            bool ok = ctx->modbusManager.exec(cmd, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("LX 电流表指令 [%1] 下发失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else if (def.send.action == TestCaseSendAction::Get) {
                const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                    if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                        return;
                    ctx->showlog(QStringLiteral("LX 电流表等待超时：%1").arg(def.send.deviceCmd));
                    ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                });
                ctx->showlog(QStringLiteral("等待 LX 电流表回包：%1（超时 %2ms）").arg(def.send.deviceCmd).arg(timeoutMs));
            } else {
                ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
            }
        } else {
            ctx->showlog(QStringLiteral("未知 Modbus 设备路由: %1").arg(deviceKey));
            ctx->markActiveTestCaseStepDone(false, deviceKey, QStringLiteral("失败"));
        }
        return;
    }

    if (def.send.channel == TestCaseSendChannel::Scpi) {
        const QString deviceKey = def.send.device;
        ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(deviceKey);
        ctx->scpiVisaManager()->setDeviceRoute(devRoute);

        const QVariant resolvedParam = ctx->resolveTestCaseSendParamTree(def.send.param);
        QString errStr;

        if (devRoute == ScpiDeviceRoute::HuilingWfp60h) {
            HuilingScpiCmd cmd = huilingScpiCmdFromName(def.send.deviceCmd);
            bool ok = ctx->scpiVisaManager()->exec(cmd, resolvedParam, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("会凌电源指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                if (def.send.action == TestCaseSendAction::Get) {
                    const int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
                    QTimer::singleShot(timeoutMs, ctx, [ctx, def]() {
                        if (!ctx->isActiveTestCaseStep(def.meta.name) || ctx->isActiveTestCaseStepDone())
                            return;
                        ctx->showlog(QStringLiteral("SCPI 设备等待超时：%1").arg(def.send.deviceCmd));
                        ctx->markActiveTestCaseStepDone(false, QStringLiteral("超时"), QStringLiteral("失败"));
                    });
                } else {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
                }
            }
        } else if (devRoute == ScpiDeviceRoute::RsCmw100) {
            CmwScpiCmd cmd = cmwScpiCmdFromName(def.send.deviceCmd);
            bool ok = ctx->scpiVisaManager()->exec(cmd, resolvedParam, &errStr);
            if (!ok) {
                ctx->showlog(QStringLiteral("CMW100 指令 [%1] 执行失败: %2").arg(def.send.deviceCmd, errStr));
                ctx->markActiveTestCaseStepDone(false, errStr, QStringLiteral("失败"));
            } else {
                if (def.send.action == TestCaseSendAction::Get) {
                    QString response = ctx->scpiVisaManager()->lastQueryResponse();
                    ProtocolMeasureData measureData;
                    measureData.deviceName = deviceKey;
                    measureData.type = def.send.deviceCmd;
                    measureData.value = response.toDouble();
                    measureData.valueText = response;
                    measureData.unit = QStringLiteral("");
                    measureData.isOk = true;
                    ctx->onUsbInstrumentReport(ProtocolReport(QStringLiteral("ProtocolMeasureData"),
                                                              QVariant::fromValue(measureData)));
                } else {
                    ctx->markActiveTestCaseStepDone(true, QStringLiteral("-"), QStringLiteral("通过"));
                }
            }
        } else {
            ctx->showlog(QStringLiteral("未知 SCPI 设备路由: %1").arg(deviceKey));
            ctx->markActiveTestCaseStepDone(false, deviceKey, QStringLiteral("失败"));
        }
        return;
    }

    DongleCmd dongleCmd = DongleCmd::BleScanConnect;
    if (def.send.channel == TestCaseSendChannel::Dongle) {
        if (!DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd)) {
            ctx->showlog(QStringLiteral("未知 Dongle 指令：%1").arg(def.send.deviceCmd));
            ctx->markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }

        if (dongleCmd == DongleCmd::BleScanConnectByName) {
            const QVariant param = ctx->resolveTestCaseSendParamTree(def.send.param);
            QString targetName = QStringLiteral("Pump-E");
            int rssiThreshold = -50;
            
            if (param.canConvert<QVariantMap>()) {
                QVariantMap map = param.toMap();
                if (map.contains(QStringLiteral("name"))) targetName = map.value(QStringLiteral("name")).toString();
                if (map.contains(QStringLiteral("rssi"))) rssiThreshold = map.value(QStringLiteral("rssi")).toInt();
            } else if (param.type() == QVariant::String) {
                const QString s = param.toString().trimmed();
                if (!s.isEmpty()) targetName = s;
            }

            ctx->showlog(QStringLiteral("开始搜索广播名称: '%1', 最低信号要求: %2").arg(targetName).arg(rssiThreshold));
            
            int timeoutMs = TestCaseRunner::commandTimeoutMs(def);
            if (timeoutMs <= 0) timeoutMs = 6000;
            
            QElapsedTimer timer;
            timer.start();
            QString bestMac;
            int bestRssi = -999;

            while (timer.elapsed() < timeoutMs) {
                bestMac.clear();
                bestRssi = -999;

                for (auto it = ctx->deviceMap.begin(); it != ctx->deviceMap.end(); ++it) {
                    const QString deviceAddress = it.key();
                    const QString deviceName = it.value().value(QStringLiteral("Name"));
                    const int deviceRssi = it.value().value(QStringLiteral("Rssi")).toInt();

                    if (deviceName.contains(targetName) && deviceRssi > rssiThreshold && deviceAddress.length() == 17) {
                        if (deviceRssi > bestRssi) {
                            bestRssi = deviceRssi;
                            bestMac = deviceAddress;
                        }
                    }
                }

                if (!bestMac.isEmpty()) {
                    break;
                }
                ctx->waitWork(100);
            }

            if (bestMac.isEmpty()) {
                ctx->stepRuntime_.done = true;
                ctx->stepRuntime_.pass = false;
                ctx->stepRuntime_.testData = QStringLiteral("未找到匹配广播: ") + targetName;
                ctx->TestResult = ctx->failValue;
                ctx->showlog(QStringLiteral("按名称自动连接失败：轮询了 %3 毫秒，依然没扫到名称包含 %1 且信号大于 %2 的设备")
                                .arg(targetName)
                                .arg(rssiThreshold)
                                .arg(timeoutMs));
                return;
            }

            ctx->showlog(QStringLiteral("按广播名称找到最佳设备，MAC: %1, 信号: %2, 发起连接...")
                            .arg(bestMac)
                            .arg(bestRssi));
            
            ctx->stepRuntime_.testData = bestMac;
            const auto sendFn = [ctx, bestMac]() {
                ctx->at->set(DongleCmd::BleScanConnect, bestMac);
                if (ctx->ui && ctx->ui->mac_combo) {
                    ctx->ui->mac_combo->setCurrentText(bestMac);
                }
            };
            ctx->setCommandWaitSource(CommandWaitSource::DongleAt);
            ctx->sendCommandWithRetry(sendFn, timeoutMs);
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

    ctx->applyTestCaseProductProtocol(def.send.productProtocol);

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
    QString fixtureConnectDetail;
    bool fixtureAutoConnected = false;
    Fixture_uart* uart =
        box ? box->ensureFixtureUartConnected(getIndex(), &fixtureConnectDetail, &fixtureAutoConnected) : nullptr;
    if (!uart || !uart->isFixtureSerialOpen()) {
        const QString msg = fixtureConnectDetail.isEmpty()
            ? QStringLiteral("治具串口未连接，且无法自动连接（请检查配置或菜单「连接治具串口」）")
            : fixtureConnectDetail;
        showlog(msg);
        markActiveTestCaseStepDone(false, QStringLiteral("治具未连接"), QStringLiteral("失败"));
        return;
    }
    if (fixtureAutoConnected)
        showlog(QStringLiteral("已自动连接治具串口：%1").arg(fixtureConnectDetail));

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
            showlog(QStringLiteral("治具 PCBA 组包失败：机号 %1（有效范围 1~15）").arg(machineIndex));
            markActiveTestCaseStepDone(false, def.send.deviceCmd, QStringLiteral("失败"));
            return;
        }
        uart->sendPcbaFrame(frame);
        showlog(QStringLiteral("已发送治具 PCBA：%1，机号 %2，帧 %3")
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
            QStringLiteral("机号=%1 静态 %2uA 工作=%3mA")
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
    } else if (cmd == FixturePcbaCmd::WaitWorkCurrentDoneAck) {
        const auto connPtr = std::make_shared<QMetaObject::Connection>();
        *connPtr = connect(uart, &Fixture_uart::send_data_to_mechine_sleep, this,
                           [this, def, stopWaitTimer, connPtr](const FixturePacketData& pack) {
                               QObject::disconnect(*connPtr);
                               if (!isActiveTestCaseStep(def.meta.name) || testCaseStepResult_.done)
                                   return;
                               stopWaitTimer();
                               const QString detail =
                                   QStringLiteral("工作电流测量完成 机号=%1").arg(pack.machineNumber);
                               markActiveTestCaseStepDone(true, detail, QStringLiteral("通过"));
                               showlog(QStringLiteral("收到治具短包 55 01 05 CC AA：%1").arg(detail));
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
