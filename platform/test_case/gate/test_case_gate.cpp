#include "test_case.h"

#include "Abini.h"
#include "asd9026a_cmd_manifest.h"
#include "common_utils.h"
#include "device_cmd_manifest.h"
#include "dongle_cmd_manifest.h"
#include "fixture_pcba_cmd_manifest.h"
#include "jieli_bt_box_cmd_manifest.h"
#include "modbus_cmd_manifest.h"
#include "scpi_cmd_manifest.h"
#include "test_case_gate_accessors.h"
#include "usb_camera_cmd_manifest.h"
#include "ves_light_cmd_manifest.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

static constexpr double kGateEqEps = 0.0001;

static GateTypeDescriptor toTypeDescriptor(const GateTypeEntry& t) {
    GateTypeDescriptor d;
    d.reportType = t.reportType;
    d.displayName = t.displayName;
    for (const GateFieldEntry& f : t.fields) {
        GateFieldDescriptor fd;
        fd.field = f.id;
        fd.displayName = f.displayName;
        d.fields.append(fd);
    }
    return d;
}

static QString resolveExpected(const TestCaseGate& gate) {
    const QString fromCase = gate.expected.trimmed();
    if (!fromCase.isEmpty()) {
        qDebug() << QStringLiteral("卡控期望来自 case Gate/Expected:") << fromCase;
        return fromCase;
    }
    if (!gate.expectedSettingsKey.isEmpty()) {
        const QString fromSettings = SETTINGS.value(gate.expectedSettingsKey).toString().trimmed();
        if (fromSettings.isEmpty()) {
            qDebug() << QStringLiteral("卡控期望设置键为空:") << gate.expectedSettingsKey;
        } else {
            qDebug() << QStringLiteral("卡控期望来自设置") << gate.expectedSettingsKey << QStringLiteral("=") << fromSettings;
        }
        return fromSettings;
    }
    return {};
}

/** Gt/Lt/数值 Eq：优先 Expected 解析为阈值，失败则用 low。 */
static double gateCompareThreshold(const TestCaseGate& gate) {
    const QString expected = gate.expected.trimmed();
    if (!expected.isEmpty()) {
        bool ok = false;
        const double parsed = expected.toDouble(&ok);
        if (ok)
            return parsed;
    }
    return gate.low;
}

static QString formatGateFieldValue(const QString& reportType, const QString& field, double value) {
    if (const GateFieldEntry* f = GateAccessorRegistry::findField(reportType, field)) {
        if (f->formatValue)
            return f->formatValue(value);
    }
    if (qAbs(value - qRound(value)) < 1e-9)
        return QString::number(qRound(value));
    return QString::number(value);
}

static QString gateValueLabel(const QString& reportType, const QString& field) {
    if (const GateFieldEntry* f = GateAccessorRegistry::findField(reportType, field)) {
        if (!f->valueLabel.isEmpty())
            return f->valueLabel;
    }
    return QStringLiteral("当前值");
}

static QString withDisplayUnit(const QString& text, const QString& unit) {
    const QString t = text.trimmed();
    const QString u = unit.trimmed();
    if (t.isEmpty() || u.isEmpty() || t == QLatin1String("-") || t == QLatin1String("通过")
        || t == QLatin1String("失败"))
        return text;
    if (t.endsWith(u)
        && (t.size() == u.size() || t.at(t.size() - u.size() - 1).isSpace()
            || !t.at(t.size() - u.size() - 1).isLetterOrNumber()))
        return text;
    return t + QLatin1Char(' ') + u;
}

static QString formatRangeAskBody(const TestCaseGate& gate, const QString& reportType, bool openRange) {
    double low = gate.low;
    double high = gate.high;
    GateRegistry::resolveRangeBounds(gate, low, high);
    return QStringLiteral("%1%2,%3%4")
        .arg(openRange ? QStringLiteral("(") : QStringLiteral("["), formatGateFieldValue(reportType, gate.field, low),
             formatGateFieldValue(reportType, gate.field, high),
             openRange ? QStringLiteral(")") : QStringLiteral("]"));
}

/** 单字段 Ask 正文（不含单位）；多字段时再加字段名前缀。 */
static QString formatAskBody(const TestCaseGate& gate, const QString& reportType, bool multiField) {
    const QString name = multiField ? GateRegistry::fieldDisplayName(reportType, gate.field) : QString();
    if (gate.op == TestCaseGateOp::Range) {
        // 多字段展示始终闭区间；单字段 RSSI 与判定一致用开区间
        const bool openRange = !multiField && CommonUtils::isRssiOpenRangeGate(reportType, gate.field);
        const QString body = formatRangeAskBody(gate, reportType, openRange);
        return multiField ? (name + QLatin1Char('=') + body) : body;
    }
    if (gate.op == TestCaseGateOp::Gt || gate.op == TestCaseGateOp::Lt) {
        const QString thr = formatGateFieldValue(reportType, gate.field, gateCompareThreshold(gate));
        const QChar opCh = (gate.op == TestCaseGateOp::Gt) ? QLatin1Char('>') : QLatin1Char('<');
        return multiField ? (name + opCh + thr) : (QString(opCh) + thr);
    }
    if (gate.op == TestCaseGateOp::Eq) {
        const QString expected = gate.expected.trimmed();
        QString valText;
        if (!expected.isEmpty()) {
            bool ok = false;
            const double parsed = expected.toDouble(&ok);
            valText = ok ? formatGateFieldValue(reportType, gate.field, parsed) : expected;
        } else {
            valText = formatGateFieldValue(reportType, gate.field, gateCompareThreshold(gate));
        }
        return multiField ? (name + QLatin1Char('=') + valText) : valText;
    }
    // CompareVersions 等
    return multiField ? (name + QLatin1Char(':') + gate.expected) : gate.expected.trimmed();
}

static QString primaryFieldTestData(const TestCaseGate& primaryGate, const QString& reportType,
                                    const QVariant& payload) {
    double fromNum = 0;
    if (GateAccessorRegistry::readNumber(reportType, primaryGate.field, payload, &fromNum))
        return formatGateFieldValue(reportType, primaryGate.field, fromNum);
    QString fromField;
    if (GateAccessorRegistry::readText(reportType, primaryGate.field, payload, &fromField) && !fromField.isEmpty())
        return fromField;
    return {};
}

// ===================== 查询 / 绑定 =====================

QStringList GateRegistry::reportTypes() {
    QStringList list;
    for (const GateTypeEntry& t : GateAccessorRegistry::allTypes())
        list.append(t.reportType);
    return list;
}

QVector<GateTypeDescriptor> GateRegistry::allTypeDescriptors() {
    QVector<GateTypeDescriptor> out;
    for (const GateTypeEntry& t : GateAccessorRegistry::allTypes())
        out.append(toTypeDescriptor(t));
    return out;
}

GateSendBinding GateRegistry::bindingForSend(TestCaseSendChannel channel, const QString& protocolOrDevice,
                                             const QString& deviceCmd) {
    GateSendBinding out;
    const QString cmd = deviceCmd.trimmed();
    if (cmd.isEmpty())
        return out;

    const auto applyRow = [&out](const char* typesCsv, const char* defaultField) {
        if (typesCsv && typesCsv[0] != '\0') {
            const QStringList parts = QString::fromLatin1(typesCsv).split(QLatin1Char(','), Qt::SkipEmptyParts);
            for (QString t : parts) {
                t = t.trimmed();
                if (!t.isEmpty())
                    out.reportTypes.append(t);
            }
        }
        if (defaultField && defaultField[0] != '\0')
            out.defaultField = QString::fromLatin1(defaultField);
    };

    switch (channel) {
    case TestCaseSendChannel::Product:
        if (const DeviceCmdManifest::Row* row = DeviceCmdManifest::findByEnumName(cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::Dongle:
        if (const DongleCmdManifest::Row* row = DongleCmdManifest::findByEnumName(cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::Fixture: {
        const TestCaseFixtureProtocol proto = FixturePcbaCmdCatalog::fixtureProtocolFromIni(protocolOrDevice);
        if (proto == TestCaseFixtureProtocol::JieliBtBox) {
            if (const JieliBtBoxCmdManifest::Row* row = JieliBtBoxCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::Asd9026a) {
            if (const Asd9026aCmdManifest::Row* row = Asd9026aCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::Pcba) {
            if (const FixturePcbaCmdManifest::Row* row = FixturePcbaCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::UsbCamera) {
            if (const UsbCameraCmdManifest::Row* row = UsbCameraCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        } else if (proto == TestCaseFixtureProtocol::VesLight) {
            if (const VesLightCmdManifest::Row* row = VesLightCmdManifest::findByEnumName(cmd))
                applyRow(row->gateReportType, row->gateDefaultField);
        }
        break;
    }
    case TestCaseSendChannel::Scpi:
        if (const ScpiCmdManifest::Row* row =
                ScpiCmdManifest::findByDeviceAndName(ScpiPeriphCmdCatalog::deviceFromIni(protocolOrDevice), cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::Modbus:
        if (const ModbusCmdManifest::Row* row =
                ModbusCmdManifest::findByDeviceAndName(ModbusDeviceCatalog::deviceRouteFromIni(protocolOrDevice), cmd))
            applyRow(row->gateReportType, row->gateDefaultField);
        break;
    case TestCaseSendChannel::ProductSerial:
    case TestCaseSendChannel::Cloud:
        break;
    }
    return out;
}

bool GateRegistry::descriptorFor(const QString& reportType, GateTypeDescriptor& out) {
    const GateTypeEntry* t = GateAccessorRegistry::findType(reportType);
    if (!t)
        return false;
    out = toTypeDescriptor(*t);
    return true;
}

QStringList GateRegistry::fieldsFor(const QString& reportType) {
    const GateTypeEntry* t = GateAccessorRegistry::findType(reportType);
    if (!t)
        return {};
    QStringList fields;
    for (const GateFieldEntry& f : t->fields)
        fields.append(f.id);
    return fields;
}

bool GateRegistry::isAllFieldsGateField(const QString& field) {
    const QString f = field.trimmed();
    return f.isEmpty() || f == QLatin1String("*") || f.compare(QLatin1String("all"), Qt::CaseInsensitive) == 0;
}

QString GateRegistry::fieldDisplayName(const QString& reportType, const QString& field) {
    if (const GateFieldEntry* f = GateAccessorRegistry::findField(reportType, field))
        return f->displayName;
    return field;
}

// ===================== 判定 =====================

bool GateRegistry::evaluate(const TestCaseGate& gate, const QString& reportType, const QVariant& payload, bool& passOut,
                            QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (!gate.enabled)
        return true;

    if (isAllFieldsGateField(gate.field)) {
        const QStringList fields = fieldsFor(reportType);
        if (fields.isEmpty()) {
            passOut = false;
            detailOut = QStringLiteral("回传类型无可用判定字段");
            return true;
        }
        QVector<TestCaseGate> subs;
        subs.reserve(fields.size());
        for (const QString& subField : fields) {
            TestCaseGate sub = gate;
            sub.field = subField;
            subs.append(sub);
        }
        return evaluateAll(subs, reportType, payload, passOut, detailOut);
    }

    // Field=multi 是多项卡控在 ini 里的占位，真正判定必须用已展开的 ItemN 分项
    if (gate.field.compare(QLatin1String("multi"), Qt::CaseInsensitive) == 0) {
        passOut = false;
        detailOut = QStringLiteral("多项卡控未展开分项：Field=multi 仅占位，请配置 Gate/Item1_Field 等后再测");
        return true;
    }

    const GateFieldEntry* fieldEntry = GateAccessorRegistry::findField(reportType, gate.field);
    double value = 0;
    const bool numOk = GateAccessorRegistry::readNumber(reportType, gate.field, payload, &value);

    if (gate.op == TestCaseGateOp::CompareVersions) {
        QString actual;
        if (!GateAccessorRegistry::readText(reportType, gate.field, payload, &actual)) {
            passOut = false;
            detailOut = QStringLiteral("上报数据中读不到文本字段 %1（回传=%2）")
                            .arg(gate.field, reportType);
            return true;
        }
        const QString expected = resolveExpected(gate);
        if (expected.isEmpty()) {
            passOut = true;
            detailOut = QStringLiteral("当前=%1, case 未配置 Gate/Expected").arg(actual);
            return true;
        }
        // SoftVersion 勾选关闭且期望仅来自 SettingsKey：放行
        if (reportType == QLatin1String("ProtocolBaseInfoData") && gate.field == QLatin1String("soft_version")
            && gate.expected.isEmpty() && !gate.expectedSettingsKey.isEmpty()
            && !SETTINGS.value(QStringLiteral("ProductInfo/SoftwareVersion_checkBox"), true).toBool()) {
            passOut = true;
            detailOut = QStringLiteral("当前=%1, 未启用软件版本校验").arg(actual);
            return true;
        }
        passOut = CommonUtils::compareVersions(expected, actual);
        detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
        return true;
    }

    // 文本 Eq：能读串则走 MAC / 纯色 / 原文；读不到再落到下方数值 Eq
    if (gate.op == TestCaseGateOp::Eq) {
        QString actual;
        if (GateAccessorRegistry::readText(reportType, gate.field, payload, &actual)) {
            const QString expected = resolveExpected(gate);
            if (expected.isEmpty()) {
                passOut = false;
                detailOut = QStringLiteral("当前=%1, 未配置期望( Gate/Expected 或 MES/UI SN)")
                                .arg(actual.isEmpty() ? QStringLiteral("-") : actual);
            } else if (fieldEntry && fieldEntry->compare == GateCompareMode::MacNormalize) {
                passOut = (CommonUtils::normalizeMac(actual) == CommonUtils::normalizeMac(expected));
                detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
            } else if (fieldEntry && fieldEntry->compare == GateCompareMode::ScreenColor) {
                const double threshold = gateCompareThreshold(gate);
                if (numOk) {
                    passOut = qAbs(value - threshold) < kGateEqEps;
                    detailOut = QStringLiteral("当前=%1, 期望=%2")
                                    .arg(formatGateFieldValue(reportType, gate.field, value),
                                         formatGateFieldValue(reportType, gate.field, threshold));
                } else {
                    passOut = false;
                    detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
                }
            } else {
                passOut = (actual == expected);
                detailOut = QStringLiteral("当前=%1, 期望=%2").arg(actual, expected);
            }
            return true;
        }
    }

    if (!numOk) {
        passOut = false;
        if (!fieldEntry) {
            detailOut = QStringLiteral("卡控字段未登记：%1（回传=%2），请核对 Gate/Field")
                            .arg(gate.field, reportType);
        } else {
            detailOut = QStringLiteral("上报数据中读不到字段 %1（回传=%2），请核对实际上报类型与内容")
                            .arg(gate.field, reportType);
        }
        return true;
    }

    double low = gate.low;
    double high = gate.high;
    resolveRangeBounds(gate, low, high);
    const QString label = gateValueLabel(reportType, gate.field);
    const QString valueText = formatGateFieldValue(reportType, gate.field, value);
    const double threshold = gateCompareThreshold(gate);
    const QString thresholdText = formatGateFieldValue(reportType, gate.field, threshold);

    switch (gate.op) {
    case TestCaseGateOp::Gt:
        passOut = value > threshold;
        detailOut = QStringLiteral("%1=%2, 要求>%3").arg(label, valueText, thresholdText);
        return true;
    case TestCaseGateOp::Lt:
        passOut = value < threshold;
        detailOut = QStringLiteral("%1=%2, 要求<%3").arg(label, valueText, thresholdText);
        return true;
    case TestCaseGateOp::Eq:
        passOut = qAbs(value - threshold) < kGateEqEps;
        detailOut = QStringLiteral("%1=%2, 期望=%3").arg(label, valueText, thresholdText);
        return true;
    default: {
        const bool openRange = CommonUtils::isRssiOpenRangeGate(reportType, gate.field);
        passOut = openRange ? (value > low && value < high) : (value >= low && value <= high);
        detailOut = QStringLiteral("%1=%2, 允许%3")
                        .arg(label, valueText, formatRangeAskBody(gate, reportType, openRange));
        return true;
    }
    }
}

bool GateRegistry::evaluateAll(const QVector<TestCaseGate>& gates, const QString& reportType,
                               const QVariant& payload, bool& passOut, QString& detailOut) {
    passOut = true;
    detailOut.clear();
    if (gates.isEmpty())
        return true;

    bool allPass = true;
    QStringList parts;
    for (const TestCaseGate& g : gates) {
        TestCaseGate ge = g;
        ge.enabled = true;
        ge.reportType = reportType;
        bool subPass = true;
        QString subDetail;
        evaluate(ge, reportType, payload, subPass, subDetail);
        if (!subPass)
            allPass = false;
        parts.append(QStringLiteral("%1(%2)").arg(fieldDisplayName(reportType, ge.field), subDetail));
    }
    passOut = allPass;
    detailOut = parts.join(QStringLiteral("; "));
    return true;
}

void GateRegistry::resolveRangeBounds(const TestCaseGate& gate, double& lowOut, double& highOut) {
    lowOut = gate.low;
    highOut = gate.high;
    if (!gate.lowSettingsKey.isEmpty())
        lowOut = SETTINGS.value(gate.lowSettingsKey, lowOut).toDouble();
    if (!gate.highSettingsKey.isEmpty())
        highOut = SETTINGS.value(gate.highSettingsKey, highOut).toDouble();
}

QString GateRegistry::unitFor(const QString& reportType, const QString& field, const QVariant& payload) {
    return GateAccessorRegistry::unitFor(reportType, field, payload);
}

// ===================== 展示 =====================

QString GateRegistry::formatFieldDisplayValue(const QString& reportType, const QString& field, double value) {
    return formatGateFieldValue(reportType, field, value);
}

QString GateRegistry::formatGateAsk(const TestCaseGate& gate, const QString& reportType, const QVariant& payload) {
    return withDisplayUnit(formatAskBody(gate, reportType, false), unitFor(reportType, gate.field, payload));
}

QString GateRegistry::formatMultiFieldAsk(const QVector<TestCaseGate>& gates, const QString& reportType,
                                          const QVariant& payload) {
    QStringList parts;
    parts.reserve(gates.size());
    for (const TestCaseGate& g : gates)
        parts.append(withDisplayUnit(formatAskBody(g, reportType, true), unitFor(reportType, g.field, payload)));
    return parts.join(QLatin1Char(';'));
}

GateStepDisplay GateRegistry::formatStepDisplay(const TestCaseGate& primaryGate, const QVector<TestCaseGate>& allGates,
                                                const QString& reportType, const QVariant& payload,
                                                bool multiFieldMode) {
    GateStepDisplay out;
    const QString unit = unitFor(reportType, primaryGate.field, payload);
    if (const GateTypeEntry* typeEntry = GateAccessorRegistry::findType(reportType)) {
        if (typeEntry->summary)
            out.testData = typeEntry->summary(payload);
    }
    if (out.testData.isEmpty())
        out.testData = withDisplayUnit(primaryFieldTestData(primaryGate, reportType, payload), unit);

    out.ask = (multiFieldMode && allGates.size() > 1) ? formatMultiFieldAsk(allGates, reportType, payload)
                                                     : formatGateAsk(primaryGate, reportType, payload);
    return out;
}
