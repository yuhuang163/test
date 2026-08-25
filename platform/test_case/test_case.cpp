#include "test_case.h"

#include "cmd_manifest_common.h"
#include "device_cmd_manifest.h"
#include "dongle_cmd_manifest.h"
#include "fixture_pcba_cmd_manifest.h"
#include "asd9026a_cmd_manifest.h"
#include "xwd_fixture_cmd_manifest.h"
#include "jieli_bt_box_cmd_manifest.h"
#include "product_serial_cmd_manifest.h"
#include "modbus_cmd_manifest.h"
#include "scpi_cmd_manifest.h"
#include "tuple_cmd_manifest.h"
#include "usb_camera_cmd_manifest.h"
#include "ves_light_cmd_manifest.h"
#include "huiling_wfp60h_profile.h"
#include "test_case_ini_param.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <QSettings>
#include <QTextCodec>

#include "Abini.h"
#include "common_utils.h"
#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

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
        } else if (def.send.channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (!DongleCmdCatalog::dongleCmdFromName(def.send.deviceCmd, dongleCmd)) {
            errors.append(QStringLiteral("Dongle 测试指令无效"));
        } else if (!DongleCmdCatalog::isCmdForAction(dongleCmd, def.send.action)) {
            errors.append(QStringLiteral("Dongle 指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!DongleCmdCatalog::paramSchemaFor(dongleCmd, schema))
                errors.append(QStringLiteral("该 Dongle 指令尚未配置参数模板，请联系工程师"));
        }
    } else if (def.send.channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (!TupleCmdCatalog::tupleCmdFromName(def.send.deviceCmd, tupleCmd)) {
            errors.append(QStringLiteral("云端测试指令无效"));
        } else if (!TupleCmdCatalog::isCmdForAction(tupleCmd, def.send.action)) {
            errors.append(QStringLiteral("云端指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!TupleCmdCatalog::paramSchemaFor(tupleCmd, schema))
                errors.append(QStringLiteral("该云端指令尚未配置参数模板，请联系工程师"));
        }
    } else if (def.send.channel == TestCaseSendChannel::ProductSerial) {
        ProductSerialCmd serialCmd;
        if (!ProductSerialCmdCatalog::productSerialCmdFromName(def.send.deviceCmd, serialCmd)) {
            errors.append(QStringLiteral("产品串口测试指令无效"));
        } else if (!ProductSerialCmdCatalog::isCmdForAction(serialCmd, def.send.action)) {
            errors.append(QStringLiteral("产品串口指令仅支持「设置」"));
        }
    } else if (def.send.channel == TestCaseSendChannel::Fixture) {
        if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Asd9026a) {
            Asd9026aCmd asdCmd;
            if (!Asd9026aCmdCatalog::asd9026aCmdFromName(def.send.deviceCmd, asdCmd)) {
                errors.append(QStringLiteral("ASD9026A 治具指令无效"));
            } else if (!Asd9026aCmdCatalog::isCmdForAction(asdCmd, def.send.action)) {
                errors.append(QStringLiteral("ASD9026A 指令与操作方式不匹配"));
            } else {
                DeviceCmdParamSchema schema;
                if (!Asd9026aCmdCatalog::paramSchemaFor(asdCmd, schema))
                    errors.append(QStringLiteral("该 ASD9026A 指令尚未配置参数模板，请联系工程师"));
            }
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::Xwd) {
            XwdRawFixtureCmd xwdCmd;
            if (!XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(def.send.deviceCmd, xwdCmd)) {
                errors.append(QStringLiteral("XWD治具指令无效"));
            } else if (!XwdRawFixtureCmdCatalog::isCmdForAction(xwdCmd, def.send.action)) {
                errors.append(QStringLiteral("XWD治具指令与操作方式不匹配"));
            } else {
                DeviceCmdParamSchema schema;
                if (!XwdRawFixtureCmdCatalog::paramSchemaFor(xwdCmd, schema))
                    errors.append(QStringLiteral("该 XWD治具指令尚未配置参数模板，请联系工程师"));
            }
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::JieliBtBox) {
            JieliBtBoxCmd jieliCmd;
            if (!JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(def.send.deviceCmd, jieliCmd)) {
                errors.append(QStringLiteral("杰理蓝牙盒子指令无效"));
            } else if (!JieliBtBoxCmdCatalog::isCmdForAction(jieliCmd, def.send.action)) {
                errors.append(QStringLiteral("杰理蓝牙盒子指令与操作方式不匹配"));
            } else {
                DeviceCmdParamSchema schema;
                if (!JieliBtBoxCmdCatalog::paramSchemaFor(jieliCmd, schema))
                    errors.append(QStringLiteral("该杰理蓝牙盒子指令尚未配置参数模板，请联系工程师"));
            }
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::UsbCamera) {
            UsbCameraCmd camCmd;
            if (!UsbCameraCmdCatalog::usbCameraCmdFromName(def.send.deviceCmd, camCmd)) {
                errors.append(QStringLiteral("USB 摄像头测试指令无效"));
            } else if (!UsbCameraCmdCatalog::isCmdForAction(camCmd, def.send.action)) {
                errors.append(QStringLiteral("USB 摄像头指令与操作方式不匹配（请选「读取」）"));
            } else {
                DeviceCmdParamSchema schema;
                if (!UsbCameraCmdCatalog::paramSchemaFor(camCmd, schema))
                    errors.append(QStringLiteral("该 USB 摄像头指令尚未配置参数模板，请联系工程师"));
            }
        } else if (def.send.fixtureProtocol == TestCaseFixtureProtocol::VesLight) {
            VesLightCmd vesCmd;
            if (!VesLightCmdCatalog::vesLightCmdFromName(def.send.deviceCmd, vesCmd)) {
                errors.append(QStringLiteral("VES 光源测试指令无效"));
            } else if (!VesLightCmdCatalog::isCmdForAction(vesCmd, def.send.action)) {
                errors.append(QStringLiteral("VES 光源指令与操作方式不匹配（请选「设置」）"));
            } else {
                DeviceCmdParamSchema schema;
                if (!VesLightCmdCatalog::paramSchemaFor(vesCmd, schema))
                    errors.append(QStringLiteral("该 VES 光源指令尚未配置参数模板，请联系工程师"));
            }
        } else if (def.send.fixtureProtocol != TestCaseFixtureProtocol::Pcba) {
            errors.append(QStringLiteral("治具协议类型无效"));
        } else {
        FixturePcbaCmd fixtureCmd;
        if (!FixturePcbaCmdCatalog::fixturePcbaCmdFromName(def.send.deviceCmd, fixtureCmd)) {
            errors.append(QStringLiteral("治具 PCBA 测试指令无效"));
        } else if (!FixturePcbaCmdCatalog::isCmdForAction(fixtureCmd, def.send.action)) {
            errors.append(QStringLiteral("治具指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!FixturePcbaCmdCatalog::paramSchemaFor(fixtureCmd, schema))
                errors.append(QStringLiteral("该治具指令尚未配置参数模板，请联系工程师"));
            }
        }
    } else if (def.send.channel == TestCaseSendChannel::Modbus) {
        if (def.send.device.isEmpty()) {
            errors.append(QStringLiteral("请选择 Modbus 目标外设"));
        } else {
            const ModbusDeviceRoute devRoute = ModbusPeriphCmdCatalog::deviceFromIni(def.send.device);
            if (devRoute == ModbusDeviceRoute::None) {
                errors.append(QStringLiteral("Modbus 目标外设无效"));
            } else if (!ModbusPeriphCmdCatalog::isCmdForDevice(devRoute, def.send.deviceCmd, def.send.action)) {
                errors.append(QStringLiteral("Modbus 测试指令无效或与操作方式不匹配"));
            }
        }
    } else if (def.send.channel == TestCaseSendChannel::Scpi) {
        if (def.send.device.isEmpty()) {
            errors.append(QStringLiteral("请选择 SCPI 目标外设"));
        } else {
            const ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(def.send.device);
            if (devRoute == ScpiDeviceRoute::None) {
                errors.append(QStringLiteral("SCPI 目标外设无效"));
            } else if (!ScpiPeriphCmdCatalog::isCmdForDevice(devRoute, def.send.deviceCmd, def.send.action)) {
                errors.append(QStringLiteral("SCPI 测试指令无效或与操作方式不匹配"));
            }
        }
    } else {
        DeviceCmd cmd;
        if (!DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
            errors.append(QStringLiteral("产品测试指令无效"));
        } else if (!DeviceCmdCatalog::isCmdForAction(cmd, def.send.action)) {
            errors.append(QStringLiteral("产品指令与操作方式不匹配"));
        } else {
            DeviceCmdParamSchema schema;
            if (!DeviceCmdCatalog::paramSchemaFor(cmd, schema))
                errors.append(QStringLiteral("该指令尚未配置参数模板，请联系工程师"));
        }
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

// ===================== DeviceCmdCatalog =====================

namespace {

/** 设置页「指令内容」下拉：去掉与「操作方式」重复的动作前缀。 */
QString cmdPickerDisplayLabel(QString label) {
    label = label.trimmed();
    if (label.startsWith(QStringLiteral("Dongle "), Qt::CaseInsensitive))
        label = label.mid(7).trimmed();
    static const QStringList prefixes = {
        QStringLiteral("设置"),
        QStringLiteral("写入"),
        QStringLiteral("读取"),
        QStringLiteral("获取"),
        QStringLiteral("上报"),
    };
    for (const QString& prefix : prefixes) {
        if (label.startsWith(prefix)) {
            label = label.mid(prefix.size()).trimmed();
            break;
        }
    }
    return label;
}

} // namespace

QStringList DeviceCmdCatalog::allDeviceCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < DeviceCmdManifest::rowCount(); ++i) {
        const DeviceCmdManifest::Row& row = DeviceCmdManifest::rows()[i];
        if (!isCmdForAction(row.cmd, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseProductProtocol DeviceCmdCatalog::productProtocolFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("Qpb"), Qt::CaseInsensitive) == 0 || t.compare(QStringLiteral("PB"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qpb;
    if (t.compare(QStringLiteral("Qroot"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qroot;
    if (t.compare(QStringLiteral("Qaiot"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qaiot;
    return TestCaseProductProtocol::Qfctp;
}

QString DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol protocol) {
    switch (protocol) {
    case TestCaseProductProtocol::Qpb:
        return QStringLiteral("Qpb");
    case TestCaseProductProtocol::Qroot:
        return QStringLiteral("Qroot");
    case TestCaseProductProtocol::Qaiot:
        return QStringLiteral("Qaiot");
    default:
        return QStringLiteral("Qfctp");
    }
}

QString DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol protocol) {
    switch (protocol) {
    case TestCaseProductProtocol::Qpb:
        return QStringLiteral("QPB");
    case TestCaseProductProtocol::Qroot:
        return QStringLiteral("Qroot");
    case TestCaseProductProtocol::Qaiot:
        return QStringLiteral("QAIOT");
    default:
        return QStringLiteral("FCTP");
    }
}

TestCaseSendAction DeviceCmdCatalog::actionFor(DeviceCmd cmd) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool DeviceCmdCatalog::isCmdForAction(DeviceCmd cmd, TestCaseSendAction action) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString DeviceCmdCatalog::deviceCmdUiLabel(const QString& enumName) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记指令");
}

bool DeviceCmdCatalog::deviceCmdFromName(const QString& name, DeviceCmd& out) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString DeviceCmdCatalog::deviceCmdToName(DeviceCmd cmd) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool DeviceCmdCatalog::paramSchemaFor(DeviceCmd cmd, DeviceCmdParamSchema& out) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString DeviceCmdCatalog::paramUiHint(const QString& deviceCmdName) {
    if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(deviceCmdName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    DeviceCmd cmd;
    if (!deviceCmdFromName(deviceCmdName, cmd))
        return QStringLiteral("未知产品指令");
    return QStringLiteral("按协议填写 JSON 或 name=value 行");
}

bool DeviceCmdCatalog::paramFromIniGroup(const QSettings& settings, DeviceCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::Int:
        out = readSendScopedParam(settings, QStringLiteral("int"), 0).toInt();
        return true;
    case DeviceCmdParamKind::UInt:
        out = readSendScopedParam(settings, QStringLiteral("uint"), 0).toUInt();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = jsonMapWithLegacyInt(settings);
        return true;
    }
    return false;
}

QVariant DeviceCmdCatalog::normalizeSendParam(DeviceCmd cmd, const QVariant& param) {
    if (!param.canConvert<QVariantMap>())
        return param;

    const QVariantMap map = param.toMap();
    if (map.isEmpty()) {
        switch (cmd) {
        case DeviceCmd::SoftVersionRead:
        case DeviceCmd::BaseInfo:
        case DeviceCmd::GetBattery:
        case DeviceCmd::DevReset:
        case DeviceCmd::WifiDisconnect:
        case DeviceCmd::ChargeCurrentRead:
        case DeviceCmd::TupleRead:
        case DeviceCmd::PeriphState:
        case DeviceCmd::FactoryReset:
        case DeviceCmd::ShipMode:
        case DeviceCmd::RootEnterOta:
            return QVariant();
        default:
            return QVariant();
        }
    }

    switch (cmd) {
    case DeviceCmd::ForbidSleep:
    case DeviceCmd::FacMode:
        return jsonMapIntValue(map, 1);
    case DeviceCmd::FacResult: {
        if (map.contains(QStringLiteral("done")))
            return map;
        QVariantMap out = map;
        if (!out.contains(QStringLiteral("done")))
            out.insert(QStringLiteral("done"), jsonMapIntValue(map, 1));
        return out;
    }
    case DeviceCmd::Sn: {
        const auto whichFromMap = [&map]() -> int {
            if (map.contains(QStringLiteral("which_sn")))
                return map.value(QStringLiteral("which_sn")).toInt();
            if (map.contains(QStringLiteral("which")))
                return map.value(QStringLiteral("which")).toInt();
            if (map.contains(QStringLiteral("type")))
                return map.value(QStringLiteral("type")).toInt();
            return jsonMapIntValue(map, 0);
        };
        QByteArray snBytes;
        if (map.contains(QStringLiteral("sn")))
            snBytes = map.value(QStringLiteral("sn")).toString().toUtf8();
        else if (map.contains(QStringLiteral("value")))
            snBytes = map.value(QStringLiteral("value")).toString().toUtf8();
        else if (map.contains(QStringLiteral("string")))
            snBytes = map.value(QStringLiteral("string")).toString().toUtf8();
        if (!snBytes.isEmpty()) {
            DeviceSnPayload payload;
            payload.which_sn = static_cast<FacDevInfoType>(whichFromMap());
            payload.sn = snBytes;
            // Qaiot device_side_id：随 Sn 归一化一并带上，避免 Param_side 丢失
            payload.sideId = -1;
            static const QStringList sideKeys = {QStringLiteral("side"),
                                                QStringLiteral("device_side_id"),
                                                QStringLiteral("deviceSideId"),
                                                QStringLiteral("sideId"),
                                                QStringLiteral("position")};
            for (const QString& key : sideKeys) {
                if (!map.contains(key))
                    continue;
                const QString raw = map.value(key).toString().trimmed();
                bool ok = false;
                const uint n = raw.toUInt(&ok);
                if (ok && n <= 2u) {
                    payload.sideId = static_cast<int>(n);
                    break;
                }
                const QString lower = raw.toLower();
                if (lower == QLatin1String("left") || lower == QLatin1String("l") || raw.contains(QStringLiteral("左"))) {
                    payload.sideId = 0;
                    break;
                }
                if (lower == QLatin1String("right") || lower == QLatin1String("r") || raw.contains(QStringLiteral("右"))) {
                    payload.sideId = 1;
                    break;
                }
                if (lower == QLatin1String("independent") || lower == QLatin1String("single")
                    || lower == QLatin1String("s") || raw.contains(QStringLiteral("单"))
                    || raw.contains(QStringLiteral("独立"))) {
                    payload.sideId = 2;
                    break;
                }
                break;
            }
            return QVariant::fromValue(payload);
        }
        if (map.contains(QStringLiteral("which_sn")) || map.contains(QStringLiteral("which")) || map.contains(QStringLiteral("type")))
            return whichFromMap();
        return jsonMapIntValue(map, 0);
    }
    case DeviceCmd::Sleep:
    case DeviceCmd::BurningMode:
    case DeviceCmd::SuctionMode:
    case DeviceCmd::WifiConnect:
    case DeviceCmd::RssiRead:
        return map;
    case DeviceCmd::SoftVersionRead:
    case DeviceCmd::BaseInfo:
    case DeviceCmd::GetBattery:
    case DeviceCmd::DevReset:
    case DeviceCmd::WifiDisconnect:
    case DeviceCmd::ChargeCurrentRead:
    case DeviceCmd::TupleRead:
    case DeviceCmd::PeriphState:
    case DeviceCmd::FactoryReset:
    case DeviceCmd::ShipMode:
    case DeviceCmd::RootEnterOta:
        return map.isEmpty() ? QVariant() : QVariant(map);
    default:
        return map;
    }
}

void DeviceCmdCatalog::paramToIniGroup(QSettings& settings, DeviceCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        writeSendParamLeaf(settings, QStringLiteral("int"), value.toInt());
        break;
    case DeviceCmdParamKind::UInt:
        writeSendParamLeaf(settings, QStringLiteral("uint"), value.toUInt());
        break;
    case DeviceCmdParamKind::String:
        writeSendParamLeaf(settings, QStringLiteral("string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        if (value.canConvert<DeviceSnPayload>()) {
            // 兼容旧加载路径把 Sn 归一成 payload 后写回 ini
            const DeviceSnPayload payload = value.value<DeviceSnPayload>();
            QVariantMap map;
            map.insert(QStringLiteral("which_sn"), static_cast<int>(payload.which_sn));
            map.insert(QStringLiteral("sn"), QString::fromUtf8(payload.sn));
            if (payload.sideId >= 0 && payload.sideId <= 2)
                map.insert(QStringLiteral("side"), payload.sideId);
            writeJsonMap(settings, prefix, map);
        } else if (value.canConvert<QVariantMap>()) {
            writeJsonMap(settings, prefix, value);
        } else if (value.type() == QVariant::String) {
            writeSendParamLeaf(settings, QStringLiteral("value"), value.toString());
        } else {
            writeSendParamLeaf(settings, QStringLiteral("value"), value.toInt());
        }
        break;
    }
}

// ===================== DongleCmdCatalog =====================

QStringList DongleCmdCatalog::allDongleCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < DongleCmdManifest::rowCount(); ++i) {
        const DongleCmdManifest::Row& row = DongleCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction DongleCmdCatalog::actionFor(DongleCmd cmd) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool DongleCmdCatalog::isCmdForAction(DongleCmd cmd, TestCaseSendAction action) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString DongleCmdCatalog::dongleCmdUiLabel(const QString& enumName) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记 Dongle 指令");
}

bool DongleCmdCatalog::dongleCmdFromName(const QString& name, DongleCmd& out) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString DongleCmdCatalog::dongleCmdToName(DongleCmd cmd) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool DongleCmdCatalog::paramSchemaFor(DongleCmd cmd, DeviceCmdParamSchema& out) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString DongleCmdCatalog::paramUiHint(const QString& dongleCmdName) {
    if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(dongleCmdName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    DongleCmd cmd;
    if (!dongleCmdFromName(dongleCmdName, cmd))
        return QStringLiteral("未知 Dongle 指令");
    return QStringLiteral("该 Dongle 指令未登记");
}

bool DongleCmdCatalog::paramFromIniGroup(const QSettings& settings, DongleCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::Int:
        out = readSendScopedParam(settings, QStringLiteral("int"), 0).toInt();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = readSendParamMap(settings);
        return true;
    default:
        return false;
    }
}

void DongleCmdCatalog::paramToIniGroup(QSettings& settings, DongleCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        writeSendParamLeaf(settings, QStringLiteral("int"), value.toInt());
        break;
    case DeviceCmdParamKind::String:
        writeSendParamLeaf(settings, QStringLiteral("string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        writeJsonMap(settings, prefix, value);
        break;
    default:
        break;
    }
}

// ===================== UsbCameraCmdCatalog =====================

QStringList UsbCameraCmdCatalog::allUsbCameraCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < UsbCameraCmdManifest::rowCount(); ++i) {
        const UsbCameraCmdManifest::Row& row = UsbCameraCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction UsbCameraCmdCatalog::actionFor(UsbCameraCmd cmd) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Get;
}

bool UsbCameraCmdCatalog::isCmdForAction(UsbCameraCmd cmd, TestCaseSendAction action) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString UsbCameraCmdCatalog::usbCameraCmdUiLabel(const QString& enumName) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记 USB 摄像头指令");
}

bool UsbCameraCmdCatalog::usbCameraCmdFromName(const QString& name, UsbCameraCmd& out) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString UsbCameraCmdCatalog::usbCameraCmdToName(UsbCameraCmd cmd) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool UsbCameraCmdCatalog::paramSchemaFor(UsbCameraCmd cmd, DeviceCmdParamSchema& out) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString UsbCameraCmdCatalog::paramUiHint(const QString& enumName) {
    if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    UsbCameraCmd cmd;
    if (!usbCameraCmdFromName(enumName, cmd))
        return QStringLiteral("未知 USB 摄像头指令");
    return QStringLiteral("该 USB 摄像头指令未登记");
}

bool UsbCameraCmdCatalog::paramFromIniGroup(const QSettings& settings, UsbCameraCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::JsonMap: {
        QVariantMap map = readSendParamMap(settings);
        map.remove(QStringLiteral("setScreenColor"));
        out = map;
        return true;
    }
    default:
        return false;
    }
}

void UsbCameraCmdCatalog::paramToIniGroup(QSettings& settings, UsbCameraCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    if (schema.kind == DeviceCmdParamKind::JsonMap) {
        QVariant cleaned = value;
        if (cleaned.canConvert<QVariantMap>()) {
            QVariantMap map = cleaned.toMap();
            map.remove(QStringLiteral("setScreenColor"));
            cleaned = map;
        }
        writeJsonMap(settings, sendParamIniPrefix(), cleaned);
    }
}

// ===================== VesLightCmdCatalog =====================

QStringList VesLightCmdCatalog::allVesLightCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < VesLightCmdManifest::rowCount(); ++i) {
        const VesLightCmdManifest::Row& row = VesLightCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction VesLightCmdCatalog::actionFor(VesLightCmd cmd) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool VesLightCmdCatalog::isCmdForAction(VesLightCmd cmd, TestCaseSendAction action) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString VesLightCmdCatalog::vesLightCmdUiLabel(const QString& enumName) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记 VES 光源指令");
}

bool VesLightCmdCatalog::vesLightCmdFromName(const QString& name, VesLightCmd& out) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString VesLightCmdCatalog::vesLightCmdToName(VesLightCmd cmd) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool VesLightCmdCatalog::paramSchemaFor(VesLightCmd cmd, DeviceCmdParamSchema& out) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString VesLightCmdCatalog::paramUiHint(const QString& enumName) {
    if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QStringLiteral("未知 VES 光源指令");
}

bool VesLightCmdCatalog::paramFromIniGroup(const QSettings& settings, VesLightCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = readSendParamMap(settings);
        return true;
    default:
        return false;
    }
}

void VesLightCmdCatalog::paramToIniGroup(QSettings& settings, VesLightCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    if (schema.kind == DeviceCmdParamKind::JsonMap)
        writeJsonMap(settings, sendParamIniPrefix(), value);
}

// ===================== Asd9026aCmdCatalog =====================

QStringList Asd9026aCmdCatalog::allAsd9026aCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < Asd9026aCmdManifest::rowCount(); ++i) {
        const Asd9026aCmdManifest::Row& row = Asd9026aCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction Asd9026aCmdCatalog::actionFor(Asd9026aCmd cmd) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool Asd9026aCmdCatalog::isCmdForAction(Asd9026aCmd cmd, TestCaseSendAction action) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString Asd9026aCmdCatalog::asd9026aCmdUiLabel(const QString& enumName) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return QString::fromUtf8(row->uiLabel);
    }
    return enumName;
}

bool Asd9026aCmdCatalog::asd9026aCmdFromName(const QString& name, Asd9026aCmd& out) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString Asd9026aCmdCatalog::asd9026aCmdToName(Asd9026aCmd cmd) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool Asd9026aCmdCatalog::paramSchemaFor(Asd9026aCmd cmd, DeviceCmdParamSchema& out) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        out.hint = row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
        return true;
    }
    return false;
}

QString Asd9026aCmdCatalog::paramUiHint(const QString& enumName) {
    if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
}

bool Asd9026aCmdCatalog::paramFromIniGroup(const QSettings& settings, Asd9026aCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = readSendParamMap(settings);
        return true;
    default:
        return false;
    }
}

void Asd9026aCmdCatalog::paramToIniGroup(QSettings& settings, Asd9026aCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::String:
        settings.setValue(prefix + QStringLiteral("string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        writeJsonMap(settings, prefix, value);
        break;
    default:
        break;
    }
}

// ===================== XwdRawFixtureCmdCatalog =====================

QStringList XwdRawFixtureCmdCatalog::allXwdRawFixtureCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < XwdRawFixtureCmdManifest::rowCount(); ++i) {
        const XwdRawFixtureCmdManifest::Row& row = XwdRawFixtureCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction XwdRawFixtureCmdCatalog::actionFor(XwdRawFixtureCmd cmd) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool XwdRawFixtureCmdCatalog::isCmdForAction(XwdRawFixtureCmd cmd, TestCaseSendAction action) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString XwdRawFixtureCmdCatalog::xwdRawFixtureCmdUiLabel(const QString& enumName) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return QString::fromUtf8(row->uiLabel);
    }
    return enumName;
}

bool XwdRawFixtureCmdCatalog::xwdRawFixtureCmdFromName(const QString& name, XwdRawFixtureCmd& out) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString XwdRawFixtureCmdCatalog::xwdRawFixtureCmdToName(XwdRawFixtureCmd cmd) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool XwdRawFixtureCmdCatalog::paramSchemaFor(XwdRawFixtureCmd cmd, DeviceCmdParamSchema& out) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        out.hint = row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
        return true;
    }
    return false;
}

QString XwdRawFixtureCmdCatalog::paramUiHint(const QString& enumName) {
    if (const XwdRawFixtureCmdManifest::Row* row = XwdRawFixtureCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
}

bool XwdRawFixtureCmdCatalog::paramFromIniGroup(const QSettings& settings, XwdRawFixtureCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    default:
        return false;
    }
}

void XwdRawFixtureCmdCatalog::paramToIniGroup(QSettings& settings, XwdRawFixtureCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::String:
        writeSendParamLeaf(settings, QStringLiteral("string"), value.toString());
        break;
    default:
        break;
    }
}

// ===================== JieliBtBoxCmdCatalog =====================

QStringList JieliBtBoxCmdCatalog::allJieliBtBoxCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < JieliBtBoxCmdManifest::rowCount(); ++i) {
        const JieliBtBoxCmdManifest::Row& row = JieliBtBoxCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction JieliBtBoxCmdCatalog::actionFor(JieliBtBoxCmd cmd) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Get;
}

bool JieliBtBoxCmdCatalog::isCmdForAction(JieliBtBoxCmd cmd, TestCaseSendAction action) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString JieliBtBoxCmdCatalog::jieliBtBoxCmdUiLabel(const QString& enumName) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return QString::fromUtf8(row->uiLabel);
    }
    return enumName;
}

bool JieliBtBoxCmdCatalog::jieliBtBoxCmdFromName(const QString& name, JieliBtBoxCmd& out) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString JieliBtBoxCmdCatalog::jieliBtBoxCmdToName(JieliBtBoxCmd cmd) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool JieliBtBoxCmdCatalog::paramSchemaFor(JieliBtBoxCmd cmd, DeviceCmdParamSchema& out) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        out.hint = row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
        return true;
    }
    return false;
}

QString JieliBtBoxCmdCatalog::paramUiHint(const QString& enumName) {
    if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
}

bool JieliBtBoxCmdCatalog::paramFromIniGroup(const QSettings& settings, JieliBtBoxCmd cmd, QVariant& out) {
    Q_UNUSED(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    default:
        return false;
    }
}

void JieliBtBoxCmdCatalog::paramToIniGroup(QSettings& settings, JieliBtBoxCmd cmd, const QVariant& value) {
    Q_UNUSED(value);
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
}

// ===================== FixturePcbaCmdCatalog =====================

QStringList FixturePcbaCmdCatalog::allFixturePcbaCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < FixturePcbaCmdManifest::rowCount(); ++i) {
        const FixturePcbaCmdManifest::Row& row = FixturePcbaCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseFixtureProtocol FixturePcbaCmdCatalog::fixtureProtocolFromIni(const QString& text) {
    if (text.compare(QStringLiteral("ASD9026A"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Asd9026a;
    // 兼容旧步骤 ini：XWD_BLE / XWD_SUCTION 与统一后的 XWD 同协议
    if (text.compare(QStringLiteral("XWD"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("Xwd"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XWD_BLE"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XwdBle"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XWD_SUCTION"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XwdSuction"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Xwd;
    if (text.compare(QStringLiteral("JIELI_BT_BOX"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("JieliBtBox"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::JieliBtBox;
    if (text.compare(QStringLiteral("USB_CAMERA"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::UsbCamera;
    if (text.compare(QStringLiteral("VES"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("VES_LIGHT"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("VesLight"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::VesLight;
    if (text.compare(QStringLiteral("Pcba"), Qt::CaseInsensitive) == 0 || text.compare(QStringLiteral("PCBA"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Pcba;
    return TestCaseFixtureProtocol::Pcba;
}

QString FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol protocol) {
    switch (protocol) {
    case TestCaseFixtureProtocol::Asd9026a:
        return QStringLiteral("ASD9026A");
    case TestCaseFixtureProtocol::Xwd:
        return QStringLiteral("XWD");
    case TestCaseFixtureProtocol::JieliBtBox:
        return QStringLiteral("JIELI_BT_BOX");
    case TestCaseFixtureProtocol::UsbCamera:
        return QStringLiteral("USB_CAMERA");
    case TestCaseFixtureProtocol::VesLight:
        return QStringLiteral("VES");
    case TestCaseFixtureProtocol::Pcba:
    default:
    return QStringLiteral("Pcba");
    }
}

QString FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol protocol) {
    switch (protocol) {
    case TestCaseFixtureProtocol::Asd9026a:
        return QStringLiteral("ASD9026A模拟电池");
    case TestCaseFixtureProtocol::Xwd:
        return QStringLiteral("XWD治具");
    case TestCaseFixtureProtocol::JieliBtBox:
        return QStringLiteral("杰理蓝牙盒子");
    case TestCaseFixtureProtocol::UsbCamera:
        return QStringLiteral("USB摄像头");
    case TestCaseFixtureProtocol::VesLight:
        return QStringLiteral("VES光源");
    case TestCaseFixtureProtocol::Pcba:
    default:
    return QStringLiteral("PCBA测试协议");
    }
}

TestCaseSendAction FixturePcbaCmdCatalog::actionFor(FixturePcbaCmd cmd) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool FixturePcbaCmdCatalog::isCmdForAction(FixturePcbaCmd cmd, TestCaseSendAction action) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString FixturePcbaCmdCatalog::fixturePcbaCmdUiLabel(const QString& enumName) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记治具指令");
}

bool FixturePcbaCmdCatalog::fixturePcbaCmdFromName(const QString& name, FixturePcbaCmd& out) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString FixturePcbaCmdCatalog::fixturePcbaCmdToName(FixturePcbaCmd cmd) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool FixturePcbaCmdCatalog::paramSchemaFor(FixturePcbaCmd cmd, DeviceCmdParamSchema& out) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString FixturePcbaCmdCatalog::paramUiHint(const QString& enumName) {
    if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
}

bool FixturePcbaCmdCatalog::paramFromIniGroup(const QSettings& settings, FixturePcbaCmd cmd, QVariant& out) {
    switch (cmd) {
    case FixturePcbaCmd::StartTest:
    case FixturePcbaCmd::StartSleep:
    case FixturePcbaCmd::StartWhiteMode: {
        QVariant machine = readSendScopedParam(settings, QStringLiteral("MachineIndex"), QVariant());
        if (!machine.isValid())
            machine = settings.value(sendParamIniKey(QStringLiteral("MachineIndex")));
        if (!machine.isValid()) {
            const QString legacyKey = QStringLiteral("SendParam/MachineIndex");
            if (settings.contains(legacyKey))
                machine = settings.value(legacyKey);
        }
        if (!machine.isValid()) {
            out = QStringLiteral("$INDEX");
            break;
        }
        if (machine.userType() == QMetaType::QString) {
            const QString s = machine.toString().trimmed();
            if (s.compare(QStringLiteral("$INDEX"), Qt::CaseInsensitive) == 0 || s.compare(QStringLiteral("$SLOT"), Qt::CaseInsensitive) == 0 || s.isEmpty()) {
                out = QStringLiteral("$INDEX");
                break;
            }
        }
        bool ok = false;
        int idx = machine.toInt(&ok);
        if (!ok || idx == 0)
            out = QStringLiteral("$INDEX");
        else
            out = qBound(1, idx, 15);
        break;
    }
    default:
        out = QVariant();
        break;
    }
    return true;
}

void FixturePcbaCmdCatalog::paramToIniGroup(QSettings& settings, FixturePcbaCmd cmd, const QVariant& value) {
    removeKeysWithPrefix(settings, QStringLiteral("SendParam"));
    removeSendParamKeys(settings);
    switch (cmd) {
    case FixturePcbaCmd::StartTest:
    case FixturePcbaCmd::StartSleep:
    case FixturePcbaCmd::StartWhiteMode:
        if (value.userType() == QMetaType::QString)
            writeSendParamLeaf(settings, QStringLiteral("MachineIndex"), value.toString().trimmed());
        else
            writeSendParamLeaf(settings, QStringLiteral("MachineIndex"), value.toInt());
        break;
    default:
        break;
    }
}

// ===================== ProductSerialCmdCatalog =====================

QStringList ProductSerialCmdCatalog::allProductSerialCmdNames() {
    QStringList names;
    for (int i = 0; i < ProductSerialCmdManifest::rowCount(); ++i)
        names.append(QString::fromLatin1(ProductSerialCmdManifest::rows()[i].enumName));
    names.sort();
    return names;
}

TestCaseSendAction ProductSerialCmdCatalog::actionFor(ProductSerialCmd cmd) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool ProductSerialCmdCatalog::isCmdForAction(ProductSerialCmd cmd, TestCaseSendAction action) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString ProductSerialCmdCatalog::productSerialCmdUiLabel(const QString& enumName) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记串口指令");
}

bool ProductSerialCmdCatalog::productSerialCmdFromName(const QString& name, ProductSerialCmd& out) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString ProductSerialCmdCatalog::productSerialCmdToName(ProductSerialCmd cmd) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString();
}

bool ProductSerialCmdCatalog::paramSchemaFor(ProductSerialCmd cmd, DeviceCmdParamSchema& out) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByCmd(cmd)) {
        out.kind = DeviceCmdParamKind::None;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString ProductSerialCmdCatalog::paramUiHint(const QString& enumName) {
    if (const ProductSerialCmdManifest::Row* row = ProductSerialCmdManifest::findByEnumName(enumName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    return QString();
}

int ProductSerialCmdCatalog::brushProfileForCmd(ProductSerialCmd cmd) {
    switch (cmd) {
    case ProductSerialCmd::StartRx2402Ble1M:
        return 0;
    case ProductSerialCmd::StartRx2440Ble1M:
        return 1;
    case ProductSerialCmd::StartRx2480Ble1M:
        return 2;
    case ProductSerialCmd::StartRx2402Ble2M:
        return 3;
    case ProductSerialCmd::StartRx2440Ble2M:
        return 4;
    case ProductSerialCmd::StartRx2480Ble2M:
        return 5;
    default:
        return -1;
    }
}

// ===================== ModbusPeriphCmdCatalog =====================

QStringList ModbusPeriphCmdCatalog::allDeviceKeys() {
    return {ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::InovanceH5uTcp),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::GcSeriesTcp),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::HqAmmeterRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::LxAmmeterRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::MultiTempLoggerRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::XinjiePlcRtu)};
}

QString ModbusPeriphCmdCatalog::deviceUiLabel(ModbusDeviceRoute device) {
    return ModbusDeviceCatalog::deviceRouteUiLabel(device);
}

ModbusDeviceRoute ModbusPeriphCmdCatalog::deviceFromIni(const QString& text) {
    return ModbusDeviceCatalog::deviceRouteFromIni(text);
}

QString ModbusPeriphCmdCatalog::deviceToIni(ModbusDeviceRoute device) {
    return ModbusDeviceCatalog::deviceRouteToIni(device);
}

QStringList ModbusPeriphCmdCatalog::allCmdNames(ModbusDeviceRoute device, TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < ModbusCmdManifest::rowCount(); ++i) {
        const ModbusCmdManifest::Row& row = ModbusCmdManifest::rows()[i];
        if (row.device != device) {
            continue;
        }
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action)) {
            continue;
        }
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

bool ModbusPeriphCmdCatalog::isCmdForDevice(ModbusDeviceRoute device, const QString& enumName,
                                            TestCaseSendAction action) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    if (!row) {
        return false;
    }
    return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
}

QString ModbusPeriphCmdCatalog::cmdUiLabel(ModbusDeviceRoute device, const QString& enumName) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->uiLabel ? QString::fromUtf8(row->uiLabel) : enumName;
}

QString ModbusPeriphCmdCatalog::paramUiHint(ModbusDeviceRoute device, const QString& enumName) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
}

// ===================== ScpiPeriphCmdCatalog =====================

QStringList ScpiPeriphCmdCatalog::allDeviceKeys() {
    return {QStringLiteral("HuilingWfp60h"), QStringLiteral("Agilent66319d"), QStringLiteral("RsCmw100")};
}

QString ScpiPeriphCmdCatalog::deviceUiLabel(ScpiDeviceRoute device) {
    switch (device) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("WFP60H 程控电源");
    case ScpiDeviceRoute::Agilent66319d:
        return QStringLiteral("66319D程控电源");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("罗德与施瓦茨 CMW100");
    default:
        return QStringLiteral("未知设备");
    }
}

ScpiDeviceRoute ScpiPeriphCmdCatalog::deviceFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("HuilingWfp60h"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::HuilingWfp60h;
    if (t.compare(QStringLiteral("Agilent66319d"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::Agilent66319d;
    if (t.compare(QStringLiteral("RsCmw100"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::RsCmw100;
    return ScpiDeviceRoute::None;
}

QString ScpiPeriphCmdCatalog::deviceToIni(ScpiDeviceRoute device) {
    switch (device) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("HuilingWfp60h");
    case ScpiDeviceRoute::Agilent66319d:
        return QStringLiteral("Agilent66319d");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("RsCmw100");
    default:
        return QStringLiteral("None");
    }
}

QStringList ScpiPeriphCmdCatalog::allCmdNames(ScpiDeviceRoute device, TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < ScpiCmdManifest::rowCount(); ++i) {
        const ScpiCmdManifest::Row& row = ScpiCmdManifest::rows()[i];
        if (row.device != device) {
            continue;
        }
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action)) {
            continue;
        }
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

bool ScpiPeriphCmdCatalog::isCmdForDevice(ScpiDeviceRoute device, const QString& enumName,
                                          TestCaseSendAction action) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    if (!row) {
        return false;
    }
    return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
}

QString ScpiPeriphCmdCatalog::cmdUiLabel(ScpiDeviceRoute device, const QString& enumName) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->uiLabel ? QString::fromUtf8(row->uiLabel) : enumName;
}

QString ScpiPeriphCmdCatalog::paramUiHint(ScpiDeviceRoute device, const QString& enumName) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
}

// ===================== TupleCmdCatalog =====================

QStringList TupleCmdCatalog::allTupleCmdNames(TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < TupleCmdManifest::rowCount(); ++i) {
        const TupleCmdManifest::Row& row = TupleCmdManifest::rows()[i];
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action))
            continue;
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

TestCaseSendAction TupleCmdCatalog::actionFor(TupleCmd cmd) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::defaultSendAction(row->sendActions);
    return TestCaseSendAction::Set;
}

bool TupleCmdCatalog::isCmdForAction(TupleCmd cmd, TestCaseSendAction action) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd))
        return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
    return false;
}

QString TupleCmdCatalog::tupleCmdUiLabel(const QString& enumName) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByEnumName(enumName)) {
        if (row->uiLabel && row->uiLabel[0] != '\0')
            return cmdPickerDisplayLabel(QString::fromUtf8(row->uiLabel));
    }
    return QStringLiteral("未登记云端指令");
}

bool TupleCmdCatalog::tupleCmdFromName(const QString& name, TupleCmd& out) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByEnumName(name)) {
        out = row->cmd;
        return true;
    }
    return false;
}

QString TupleCmdCatalog::tupleCmdToName(TupleCmd cmd) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd))
        return QString::fromLatin1(row->enumName);
    return QString::number(static_cast<int>(cmd));
}

bool TupleCmdCatalog::paramSchemaFor(TupleCmd cmd, DeviceCmdParamSchema& out) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByCmd(cmd)) {
        out.kind = row->paramKind;
        if (row->paramHint && row->paramHint[0] != '\0')
            out.hint = QString::fromUtf8(row->paramHint);
        else
            out.hint.clear();
        return true;
    }
    return false;
}

QString TupleCmdCatalog::paramUiHint(const QString& tupleCmdName) {
    if (const TupleCmdManifest::Row* row = TupleCmdManifest::findByEnumName(tupleCmdName)) {
        if (row->paramHint && row->paramHint[0] != '\0')
            return QString::fromUtf8(row->paramHint);
    }
    TupleCmd cmd;
    if (!tupleCmdFromName(tupleCmdName, cmd))
        return QStringLiteral("未知云端指令");
    return QStringLiteral("该云端指令未登记");
}

bool TupleCmdCatalog::paramFromIniGroup(const QSettings& settings, TupleCmd cmd, QVariant& out) {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::Int:
        out = readSendScopedParam(settings, QStringLiteral("int"), 0).toInt();
        return true;
    case DeviceCmdParamKind::String:
        out = readSendScopedParam(settings, QStringLiteral("string"), QString()).toString();
        return true;
    case DeviceCmdParamKind::JsonMap:
        out = readSendParamMap(settings);
        return true;
    default:
        return false;
    }
}

void TupleCmdCatalog::paramToIniGroup(QSettings& settings, TupleCmd cmd, const QVariant& value) {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        writeSendParamLeaf(settings, QStringLiteral("int"), value.toInt());
        break;
    case DeviceCmdParamKind::String:
        writeSendParamLeaf(settings, QStringLiteral("string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        writeJsonMap(settings, prefix, value);
        break;
    default:
        break;
    }
}

// ===================== TestCaseHookRegistry =====================

namespace {
QHash<QString, TestCaseHookFn>& hooks() {
    static QHash<QString, TestCaseHookFn> table;
    return table;
}
} // namespace

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
