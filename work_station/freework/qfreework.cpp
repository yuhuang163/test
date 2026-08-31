#include "qfreework.h"

#include "common_utils.h"
#include "huiling_wfp60h_scpi_types.h"
#include "qfreeworkbox.h"
#include "screen_inspect_analyzer.h"
#include "screen_inspect_capture.h"
#include "screen_inspect_gige_capture.h"
#include "test_case.h"

#include <QMessageBox>
#include <QCloseEvent>
#include <QPushButton>
#include <QAbstractButton>
#include <QLabel>
#include <QPainter>
#include <QTableWidget>
#include <QFont>
#include <QComboBox>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTimer>
#include <QVector>
#include <QStringList>
#include <QFontMetrics>
#include <QRegularExpression>
#include <QIODevice>
#include <algorithm>
#include <QVBoxLayout>
#include <QPen>
#include <QTabWidget>
#include <QThread>
#include <QtGlobal>
#include "dongle_at_types.h"
#include "shared_instrument.h"
#include "visa_channel.h"
#include "qcustomplot.h"
#include "qproduct.h"
#include "test_data_upload_service.h"
#include "test_record_store.h"
#include "agreement/mes_protocol/device/byd_mes/bydmes.h"
#include "ui_qfreework.h"
#include <QShowEvent>
#include <QDesktopServices>
#include <QUrl>
#include <QDialog>
#include <QDialogButtonBox>
#include <QScrollArea>
#include <QPixmap>
#include <QHBoxLayout>
#include <QCursor>

namespace {

/** SetBattery 步骤：把 Param_* 写入结果表「数据」列（如 3700 mV） */
QString setBatteryTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("SetBattery"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    QStringList parts;
    if (map.contains(QStringLiteral("percent")))
        parts << (map.value(QStringLiteral("percent")).toString() + QStringLiteral(" %"));
    if (map.contains(QStringLiteral("voltageMv")) || map.contains(QStringLiteral("voltage"))
        || map.contains(QStringLiteral("mV"))) {
        const QString mv = map.value(QStringLiteral("voltageMv"),
                                     map.value(QStringLiteral("voltage"), map.value(QStringLiteral("mV"))))
                               .toString();
        parts << (mv + QStringLiteral(" mV"));
    }
    if (map.contains(QStringLiteral("currentMa")) || map.contains(QStringLiteral("current"))) {
        const QString ma =
            map.value(QStringLiteral("currentMa"), map.value(QStringLiteral("current"))).toString();
        parts << (ma + QStringLiteral(" mA"));
    }
    if (map.contains(QStringLiteral("temperatureC")) || map.contains(QStringLiteral("temperature"))
        || map.contains(QStringLiteral("temp"))) {
        const QString tc = map.value(QStringLiteral("temperatureC"),
                                     map.value(QStringLiteral("temperature"), map.value(QStringLiteral("temp"))))
                               .toString();
        parts << (tc + QStringLiteral(" °C"));
    }
    if (map.contains(QStringLiteral("value")) && parts.isEmpty())
        parts << (map.value(QStringLiteral("value")).toString() + QStringLiteral(" mV"));
    return parts.join(QLatin1Char(' '));
}

QString lightCalibWriteTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("LightCalibWrite"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    const int type = map.value(QStringLiteral("type"), map.value(QStringLiteral("sensorType"), -1)).toInt();
    if (type == 0 || map.contains(QStringLiteral("kx"))) {
        static const QStringList keys = {QStringLiteral("kx"),  QStringLiteral("ky"),  QStringLiteral("kz"),
                                         QStringLiteral("syx"), QStringLiteral("szx"), QStringLiteral("szy"),
                                         QStringLiteral("bx"),  QStringLiteral("by"),  QStringLiteral("bz")};
        QStringList parts;
        for (const QString& k : keys) {
            if (map.contains(k))
                parts << QStringLiteral("%1=%2").arg(k, map.value(k).toString());
        }
        if (!parts.isEmpty())
            return parts.join(QLatin1Char(' '));
    }
    if (type == 4 || map.contains(QStringLiteral("calibrated")) || map.contains(QStringLiteral("flag"))) {
        const int flag = map.value(QStringLiteral("calibrated"), map.value(QStringLiteral("flag"), 0)).toInt();
        return flag ? QStringLiteral("1(已校准)") : QStringLiteral("0(未校准)");
    }
    const QString data = map.value(QStringLiteral("data"), map.value(QStringLiteral("value"))).toString().trimmed();
    return data;
}

/** ExceptionThresholdWrite：把 Param 写入结果表「数据」列 */
QString exceptionThresholdWriteTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("ExceptionThresholdWrite"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    const int type = map.value(QStringLiteral("type"), map.value(QStringLiteral("exceptionType"), 0)).toInt();
    QStringList parts;
    parts << QStringLiteral("type=0x%1").arg(type, 2, 16, QChar('0'));
    if (map.contains(QStringLiteral("low")) || map.contains(QStringLiteral("tempLow"))) {
        const QString low = map.value(QStringLiteral("low"), map.value(QStringLiteral("tempLow"))).toString();
        const QString high = map.value(QStringLiteral("high"), map.value(QStringLiteral("tempHigh"))).toString();
        parts << QStringLiteral("%1~%2").arg(low, high);
    } else if (map.contains(QStringLiteral("value")) || map.contains(QStringLiteral("percent"))
               || map.contains(QStringLiteral("voltageMv")) || map.contains(QStringLiteral("seconds"))
               || map.contains(QStringLiteral("currentMa"))) {
        const QString v = map.value(QStringLiteral("value"),
                                    map.value(QStringLiteral("percent"),
                                              map.value(QStringLiteral("voltageMv"),
                                                        map.value(QStringLiteral("seconds"),
                                                                  map.value(QStringLiteral("currentMa"))))))
                              .toString();
        parts << v;
    } else if (map.contains(QStringLiteral("data"))) {
        parts << map.value(QStringLiteral("data")).toString();
    }
    return parts.join(QLatin1Char(' '));
}

QString pumpParamWriteTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("PumpParamWrite")
        && def.send.deviceCmd != QStringLiteral("ValveParamWrite"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    QStringList parts;
    const bool isValve = (def.send.deviceCmd == QStringLiteral("ValveParamWrite"));
    const QStringList keys =
        isValve ? QStringList{QStringLiteral("valveEnableTime"), QStringLiteral("valveDisableTime"),
                              QStringLiteral("valvePwm")}
                : QStringList{QStringLiteral("circleNum"), QStringLiteral("durationTime"),
                              QStringLiteral("intervalTime"), QStringLiteral("pumpPwm")};
    for (const QString& k : keys) {
        if (map.contains(k))
            parts << QStringLiteral("%1=%2").arg(k, map.value(k).toString());
    }
    return parts.join(QLatin1Char(' '));
}

QString heatTestWriteTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("HeatTestWrite"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    QStringList parts;
    for (const QString& k : {QStringLiteral("enable"), QStringLiteral("driveStrength"),
                             QStringLiteral("durationTime")}) {
        if (map.contains(k))
            parts << QStringLiteral("%1=%2").arg(k, map.value(k).toString());
    }
    return parts.join(QLatin1Char(' '));
}

QString vibrationTestWriteTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("VibrationTestWrite"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    QStringList parts;
    for (const QString& k : {QStringLiteral("enable"), QStringLiteral("driveStrength"), QStringLiteral("freq"),
                             QStringLiteral("durationTime")}) {
        if (map.contains(k))
            parts << QStringLiteral("%1=%2").arg(k, map.value(k).toString());
    }
    return parts.join(QLatin1Char(' '));
}

QString cycleReportWriteTestDataText(const TestCaseDefinition& def) {
    if (def.send.deviceCmd != QStringLiteral("CycleReportWrite"))
        return {};
    if (!def.send.param.canConvert<QVariantMap>())
        return {};
    const QVariantMap map = def.send.param.toMap();
    QStringList parts;
    for (const QString& k :
         {QStringLiteral("enable"), QStringLiteral("type"), QStringLiteral("types"), QStringLiteral("intervalTime"),
          QStringLiteral("intervals"), QStringLiteral("items")}) {
        if (map.contains(k))
            parts << QStringLiteral("%1=%2").arg(k, map.value(k).toString());
    }
    return parts.join(QLatin1Char(' '));
}

/** 测试项提示弹窗：产线可读性，正文与按钮字号加大 */
void applyTestItemPromptFont(QMessageBox* box) {
    if (!box) {
        return;
    }
    QFont font = box->font();
    font.setPointSize(18);
    box->setFont(font);
    for (QLabel* label : box->findChildren<QLabel*>()) {
        label->setFont(font);
    }
    for (QAbstractButton* btn : box->findChildren<QAbstractButton*>()) {
        btn->setFont(font);
        btn->setMinimumHeight(44);
        btn->setMinimumWidth(96);
    }
}

/** MES 分段用 | 拼接，value 内禁止裸 |，避免解析错位。 */
QString sanitizeMesValuePipes(QString v) {
    v.replace(QLatin1Char('|'), QStringLiteral("｜"));
    return v;
}

static void appendOneMesStep(QVector<QFreeWorkMesSegment>* out, const QString& name,
                              const QString& value, const QString& maxValue, const QString& minValue,
                              const QString& standardValue, const QString& unit, const QString& result,
                              const QString& costTime) {
    const QString n = name.trimmed();
    if (n.isEmpty())
        return;
    out->append({sanitizeMesValuePipes(n), sanitizeMesValuePipes(value), sanitizeMesValuePipes(maxValue),
                 sanitizeMesValuePipes(minValue), sanitizeMesValuePipes(standardValue),
                 sanitizeMesValuePipes(unit), sanitizeMesValuePipes(result), sanitizeMesValuePipes(costTime)});
}

/** 每段格式 NAME:VALUE:MAX:MIN:STANDARD:UNIT:RESULT:COSTTIME，多段用 | 连接。 */
QString joinFreeWorkMesItemvalue(const QVector<QFreeWorkMesSegment>& segments, const QString& overallResult,
                                 const QString& failValueLiteral) {
    QStringList parts;
    parts.reserve(segments.size() + 1);
    for (const auto& s : segments) {
        if (s.name.isEmpty())
            continue;
        parts << s.name + QLatin1Char(':') + s.value + QLatin1Char(':') + s.maxValue + QLatin1Char(':') +
                     s.minValue + QLatin1Char(':') + s.standardValue + QLatin1Char(':') + s.unit +
                     QLatin1Char(':') + s.result + QLatin1Char(':') + s.costTime;
    }
    if (parts.isEmpty()) {
        const QString v = (overallResult == failValueLiteral) ? QStringLiteral("FAIL") : QStringLiteral("PASS");
        parts << QStringLiteral("SUMMARY:") + v + QStringLiteral(":::::");
    }
    return QStringLiteral("|") + parts.join(QStringLiteral("|")) + QStringLiteral("|");
}

/** 取 MES 分项中首个 FAIL，用于 NcComplete 的 NC_CONTEXT / NC_CODE。 */
static bool firstFreeWorkMesFailSegment(const QVector<QFreeWorkMesSegment>& segments, QString* outName,
                                        QString* outValue) {
    for (const auto& s : segments) {
        if (s.result.trimmed().compare(QStringLiteral("FAIL"), Qt::CaseInsensitive) != 0) {
            continue;
        }
        const QString name = s.name.trimmed();
        if (name.isEmpty()) {
            continue;
        }
        if (outName) {
            *outName = name;
        }
        if (outValue) {
            *outValue = s.value.trimmed();
        }
        return true;
    }
    return false;
}

/** 表格兜底：MES 分项未写入时从测试结果表取首个失败行。 */
static bool firstFailedRowFromResultTable(QTableWidget* table, QString* outName, QString* outValue) {
    if (!table) {
        return false;
    }
    for (int row = 0; row < table->rowCount(); ++row) {
        QTableWidgetItem* resultCell = table->item(row, 2);
        if (!resultCell || resultCell->text().trimmed() != QStringLiteral("失败")) {
            continue;
        }
        QTableWidgetItem* itemCell = table->item(row, 0);
        QTableWidgetItem* dataCell = table->item(row, 1);
        const QString name = itemCell ? itemCell->text().trimmed() : QString();
        if (name.isEmpty()) {
            continue;
        }
        if (outName) {
            *outName = name;
        }
        if (outValue) {
            *outValue = dataCell ? dataCell->text().trimmed() : QString();
        }
        return true;
    }
    return false;
}

/** BYD NcComplete NC_CONTEXT 前半段：测试项 + 失败值（内部分隔用中文逗号，避免与模板分号混淆）。 */
static QString buildFreeWorkMesFailRemark(const QString& itemName, const QString& failValue) {
    if (itemName.isEmpty()) {
        return QString();
    }
    if (failValue.isEmpty()) {
        return QStringLiteral("测试项:%1").arg(itemName);
    }
    return QStringLiteral("测试项:%1，失败值:%2").arg(itemName, failValue);
}

/** MES 分项键（MesTag 等）→ 当前工站步骤中文名（Meta/Name，与界面测试项一致）。 */
static QString resolveFreeWorkFailStepDisplayName(const QString& stationKey, const QStringList& orderedNames,
                                                  const QString& itemKey) {
    const QString key = itemKey.trimmed();
    if (key.isEmpty()) {
        return key;
    }
    for (const QString& stepId : orderedNames) {
        TestCaseDefinition def;
        if (!TestCaseRunner::loadCaseForStation(stationKey, stepId, def)) {
            continue;
        }
        const QString mesTag = def.meta.mesTag.trimmed();
        const QString stepName = TestCaseRunner::stepLabel(def);
        if (key == mesTag || key == stepName || key == stepId) {
            return stepName.isEmpty() ? stepId : stepName;
        }
        if (!mesTag.isEmpty() && key.startsWith(mesTag + QLatin1Char('_'))) {
            return stepName.isEmpty() ? stepId : stepName;
        }
        if (!stepName.isEmpty() && key.startsWith(stepName + QLatin1Char('_'))) {
            return stepName;
        }
    }
    const QString cloud = TestCaseStore::cloudDisplayNameForItemKey(key);
    if (!cloud.isEmpty() && cloud != key) {
        return cloud;
    }
    const int us = key.lastIndexOf(QLatin1Char('_'));
    if (us > 0) {
        const QString cloudBase = TestCaseStore::cloudDisplayNameForItemKey(key.left(us));
        if (!cloudBase.isEmpty() && cloudBase != key.left(us)) {
            return cloudBase;
        }
    }
    return key;
}

bool isDongleBleConnectStepName(const QString& name) {
    return name.contains(QStringLiteral("直连接蓝牙")) || name.contains(QStringLiteral("扫描连接蓝牙"));
}

constexpr int kFreeWorkTabMain = 0;

void fitScreenInspectThumb(QLabel* label, const QImage& img, const QString& emptyText,
                           const QRect& roi = QRect()) {
    if (!label)
        return;
    if (img.isNull()) {
        label->setPixmap(QPixmap());
        label->setText(emptyText);
        return;
    }
    label->setText(QString());
    QSize sz = label->size();
    if (sz.width() < 40 || sz.height() < 40)
        sz = QSize(280, 180);
    // 先缩 QImage 再转 QPixmap，避免 20MP 整图进 UI 卡死
    QImage scaled = img.scaled(sz, Qt::KeepAspectRatio, Qt::FastTransformation);
    const QRect overlay = roi.intersected(img.rect());
    if (overlay.width() >= 4 && overlay.height() >= 4 && img.width() > 0 && img.height() > 0) {
        const QRect od(overlay.x() * scaled.width() / img.width(),
                       overlay.y() * scaled.height() / img.height(),
                       qMax(1, overlay.width() * scaled.width() / img.width()),
                       qMax(1, overlay.height() * scaled.height() / img.height()));
        scaled = scaled.convertToFormat(QImage::Format_RGB32);
        QPainter p(&scaled);
        p.setPen(QPen(QColor(0, 220, 255), 3));
        p.drawRect(od.adjusted(0, 0, -1, -1));
        p.end();
    }
    label->setPixmap(QPixmap::fromImage(scaled));
}

/** 按当前标签文字重算 min-width，覆盖全局 Ubuntu.qss 对 Tab 的裁切。 */
void setupFreeWorkTabBar(QTabWidget* tabWidget) {
    if (!tabWidget)
        return;

    tabWidget->setUsesScrollButtons(true);
    QTabBar* bar = tabWidget->tabBar();
    if (!bar)
        return;

    bar->setExpanding(false);
    bar->setElideMode(Qt::ElideNone);
    bar->setUsesScrollButtons(true);

    QFont font = bar->font();
    font.setPixelSize(14);
    bar->setFont(font);

    const QFontMetrics fm(font);
    int maxTextWidth = 0;
    for (int i = 0; i < bar->count(); ++i) {
        if (!bar->isTabVisible(i))
            continue;
        maxTextWidth = qMax(maxTextWidth, fm.horizontalAdvance(bar->tabText(i)));
    }

    constexpr int hPad = 32;
    const int minTabWidth = maxTextWidth + hPad;
    // updateMainStyle 之后覆盖全局 QTabBar 规则
    bar->setStyleSheet(QStringLiteral(
                           "QTabWidget QTabBar::tab {"
                           "  min-width: %1px;"
                           "  padding: 6px 16px;"
                           "  font-size: 14px;"
                           "}"
                           "QTabWidget QTabBar::tab:selected {"
                           "  font-weight: bold;"
                           "}")
                           .arg(minTabWidth));
    bar->updateGeometry();
}

} // namespace

void QFreeWork::onTestCaseStepMarkedDone(bool pass, const QString& testData, const QString& ask) {
    stepRuntime_.done = true;
    stepRuntime_.pass = pass;
    stepRuntime_.testData = testData;
    if (!ask.isEmpty())
        stepRuntime_.ask = ask;
    // 同步收尾步骤（如 ASD9026A）不再走 sendCommandWithRetry，须放开 canGoNext 才能推进
    canGoNext = true;
    // 串口 waitForReadyRead 会泵事件；残留 retry 定时器若在此期间到期会误报超时并打翻本步结果
    if (commandRetryTimer)
        finishCommandRetryWait(true, QString());
    else
        sendRetryOver = false;
}

void QFreeWork::appendTestCaseMes(const TestCaseDefinition& def, bool pass, const QString& testData) {
    const QString tag = def.meta.mesTag.trimmed().isEmpty() ? def.meta.name.trimmed() : def.meta.mesTag.trimmed();
    const bool hasData = !testData.trimmed().isEmpty() && testData != QStringLiteral("-");
    QString value = hasData ? testData.trimmed() : QString();
    const QString resultVal = pass ? QStringLiteral("PASS") : QStringLiteral("FAIL");

    QString maxVal, minVal, stdVal;
    if (def.gate.enabled) {
        switch (def.gate.op) {
        case TestCaseGateOp::Range:
            maxVal = QString::number(def.gate.high);
            minVal = QString::number(def.gate.low);
            break;
        case TestCaseGateOp::Gt:
            stdVal = QStringLiteral(">") + QString::number(def.gate.low);
            break;
        case TestCaseGateOp::Lt:
            stdVal = QStringLiteral("<") + QString::number(def.gate.high);
            break;
        case TestCaseGateOp::Eq:
        case TestCaseGateOp::CompareVersions: {
            stdVal = def.gate.expected.trimmed();
            if (!stdVal.isEmpty()) {
                const QString resolved = resolveTestCaseSendPlaceholder(stdVal);
                if (resolved != stdVal)
                    stdVal = resolved;
            }
            break;
        }
        }
    }
    // MES VALUE 保持无单位；UNIT 单独上报（界面 testData 可能已带单位后缀）
    QString unit = def.gate.enabled ? GateRegistry::unitFor(def.gate.reportType, def.gate.field) : QString();
    if (!unit.isEmpty() && value.endsWith(unit)) {
        value = value.left(value.size() - unit.size()).trimmed();
    } else if (unit.isEmpty() && value.contains(QLatin1Char(' '))) {
        // ProtocolMeasureData 等运行时单位：从「12.3 mA」拆出末段作为 UNIT
        const int sp = value.lastIndexOf(QLatin1Char(' '));
        if (sp > 0) {
            const QString maybeUnit = value.mid(sp + 1).trimmed();
            bool looksNumeric = !maybeUnit.isEmpty();
            for (const QChar c : maybeUnit) {
                if (!(c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char('-') || c == QLatin1Char('+'))) {
                    looksNumeric = false;
                    break;
                }
            }
            if (!maybeUnit.isEmpty() && !looksNumeric && maybeUnit.size() <= 8) {
                unit = maybeUnit;
                value = value.left(sp).trimmed();
            }
        }
    }
    const QString costTime = QString::number(stepRuntime_.caseTimer.isValid() ? stepRuntime_.caseTimer.elapsed() : 0);
    appendOneMesStep(&freeWorkMesSegments_, tag, value, maxVal, minVal, stdVal, unit, resultVal, costTime);
}

void QFreeWork::appendMultiGateTestCaseMes(const QVector<TestCaseGate>& gates, const QString& reportType,
                                           const QVariant& payload) {
    const QString baseTag =
        activeTestCase_.meta.mesTag.trimmed().isEmpty() ? activeTestCase_.meta.name.trimmed()
                                                        : activeTestCase_.meta.mesTag.trimmed();
    for (const TestCaseGate& g : gates) {
        if (!g.enabled)
            continue;
        TestCaseGate ge = g;
        ge.reportType = reportType;
        bool subPass = true;
        QString subDetail;
        GateRegistry::evaluate(ge, reportType, payload, subPass, subDetail);
        Q_UNUSED(subDetail);

        // 与单字段 appendTestCaseMes 一致：VALUE 取实测值，UNIT 单独带上下限
        const GateStepDisplay disp =
            GateRegistry::formatStepDisplay(ge, QVector<TestCaseGate>{ge}, reportType, payload, false);
        QString value = disp.testData.trimmed();
        QString unit = GateRegistry::unitFor(reportType, ge.field, payload);
        if (!unit.isEmpty() && value.endsWith(unit))
            value = value.left(value.size() - unit.size()).trimmed();

        QString maxVal, minVal, stdVal;
        switch (ge.op) {
        case TestCaseGateOp::Range: {
            double low = ge.low;
            double high = ge.high;
            GateRegistry::resolveRangeBounds(ge, low, high);
            maxVal = GateRegistry::formatFieldDisplayValue(reportType, ge.field, high);
            minVal = GateRegistry::formatFieldDisplayValue(reportType, ge.field, low);
            break;
        }
        case TestCaseGateOp::Gt:
            stdVal = QStringLiteral(">")
                     + GateRegistry::formatFieldDisplayValue(reportType, ge.field, ge.low);
            break;
        case TestCaseGateOp::Lt:
            stdVal = QStringLiteral("<")
                     + GateRegistry::formatFieldDisplayValue(reportType, ge.field, ge.high);
            break;
        case TestCaseGateOp::Eq:
        case TestCaseGateOp::CompareVersions:
            stdVal = GateRegistry::formatGateAsk(ge, reportType, payload);
            break;
        }

        // 杰理蓝牙盒子：拆成 BT_RSSI / BT_FREQ_OFFSET / BT_MAC
        QString itemName;
        if (reportType == QStringLiteral("ProtocolJieliBtBoxData")) {
            if (ge.field == QStringLiteral("rssi"))
                itemName = QStringLiteral("BT_RSSI");
            else if (ge.field == QStringLiteral("freqOffset"))
                itemName = QStringLiteral("BT_FREQ_OFFSET");
            else if (ge.field == QStringLiteral("mac"))
                itemName = QStringLiteral("BT_MAC");
        }
        if (itemName.isEmpty())
            itemName = QStringLiteral("%1_%2").arg(baseTag, ge.field);

        const QString costTime =
            QString::number(stepRuntime_.caseTimer.isValid() ? stepRuntime_.caseTimer.elapsed() : 0);
        appendOneMesStep(&freeWorkMesSegments_, itemName, value, maxVal, minVal, stdVal, unit,
                         subPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"), costTime);
    }
}

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

QFreeWork::QFreeWork(int index, QWidget* parent) : test_base(parent), ui(new Ui::QFreeWork) {
    registerQFreeWorkCatalogTestCaseHooks();
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = FREE_VER;

    setupModbusManager();
    plcFacade_.setModbusManager(&modbusManager);

    ui->setupUi(this);
    ui->disconnectButton->setEnabled(false);
    ui->jigDisconnectButton->setEnabled(false);
    ui->usbdisconnectButton->setEnabled(false);
    ui->productDisconnectButton->setEnabled(false);
    updateMainStyle("Ubuntu.qss");
    applyFreeWorkExtraTabsVisible(false);
    setupFreeWorkTabBar(ui->tabWidget);
    scanSerialPorts(); // 要搜索一下一开始
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    refreshBydMesResourceDisplay();

    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");
    resetTuplePositionHighlight();
    setupTuplePositionClickable();
    if (ui->label_screenInspectShot) {
        ui->label_screenInspectShot->setCursor(Qt::PointingHandCursor);
        ui->label_screenInspectShot->installEventFilter(this);
    }
    // 中间「标注图」控件会挤乱标题与缩略图对齐，从布局移除；标注合到「本次拍摄」
    if (ui->horizontalLayout_screenInspectCaptions && ui->label_screenInspectMarkCaption) {
        ui->horizontalLayout_screenInspectCaptions->removeWidget(ui->label_screenInspectMarkCaption);
        ui->label_screenInspectMarkCaption->hide();
    }
    if (ui->horizontalLayout_screenInspectThumbs && ui->label_screenInspectMark) {
        ui->horizontalLayout_screenInspectThumbs->removeWidget(ui->label_screenInspectMark);
        ui->label_screenInspectMark->hide();
    }
    if (ui->label_screenInspectShotCaption)
        ui->label_screenInspectShotCaption->setText(QStringLiteral("本次拍摄（坏点标注）"));
    if (ui->label_screenInspectRefCaption)
        ui->label_screenInspectRefCaption->setText(QStringLiteral("标准参考图（无标注）"));
    if (ui->label_screenInspectRef) {
        ui->label_screenInspectRef->setCursor(Qt::PointingHandCursor);
        ui->label_screenInspectRef->installEventFilter(this);
    }

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
    });

    connect(comparewaittime, &QTimer::timeout, [=]() {
        iscompareovertime = 1;
        comparewaittime->stop();
    });

    // 阻塞步骤里 waitWork 仍在泵事件，QTimer 照常触发，计时不再停在采样前的数值
    testTimeTicker_->setInterval(200);
    connect(testTimeTicker_, &QTimer::timeout, this, [this]() {
        if (isTestContinue && teststate >= 0)
            ui->test_time->setText(CommonUtils::formatElapsedSeconds(TestTime));
    });
    testTimeTicker_->start();

    HighRssi = SETTINGS.value("WIFI/HighRssi").toDouble();
    LowRssi = SETTINGS.value("WIFI/LowRssi").toDouble();
    BleHighRssi = SETTINGS.value("BLE/HighRssi").toDouble();
    BleLowRssi = SETTINGS.value("BLE/LowRssi").toDouble();
    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();
    HighCurrent = SETTINGS.value("Current/HighCharCurrent").toDouble();
    LowCurrent = SETTINGS.value("Current/LowCharCurrent").toDouble();
    lowKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/Low"), 1).toUInt();
    highKeyCap_ = SETTINGS.value(QStringLiteral("KeyCap/High"), 65535).toUInt();
    loadSuctionGateSettings();
    initSuctionChart();

    measure_wait_time = SETTINGS.value("Current/measure_wait_time").toInt();

    RssiTestTime = SETTINGS.value("BLE/RssiCount").toInt();
    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "usmile123").toString());

    showlog("HighCurrent=" + QString::number(HighCurrent));
    showlog("LowCurrent=" + QString::number(LowCurrent));
    showlog("measure_wait_time=" + QString::number(measure_wait_time));

    showlog("machineNo=" + pack.machineNo);
    showlog("standbattary=" + QString::number(standbattary));
    showlog("model=" + pack.model);
    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);

    if (pack.factory == "hq" || pack.factory == "jj") {
        ui->jigComNameCombo->setEnabled(false);
        ui->jigConnectButton->setEnabled(false);
        ui->jigDisconnectButton->setEnabled(false);
    }

    refreshOrderedTestIndexes();
    testResultTableInit();
    if (product) {
        connect(product, &Qproduct::instrumentStopReceiveSeen, this, &QFreeWork::onProductInstrumentStopReceiveAckForPer);
    }
    ui->tabWidget->setCurrentIndex(0); // 设置当前页为第一页
}

void QFreeWork::showEvent(QShowEvent* event) {
    test_base::showEvent(event);
    refreshBydMesResourceDisplay();
}

void QFreeWork::refreshOrderedTestIndexes() {
    const QString stationName = TestCaseStore::loadSelectedFlowStationName();

    orderedTestCaseNames_.clear();
    orderedFailCaseNames_.clear();
    runningFailFlow_ = false;
    stopFlowOnTestFail_ = true;

    QString stationKey = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationKey());
    if (TestCaseStore::loadStationFlowItems(stationKey).isEmpty()) {
        const QString byName = TestCaseStore::resolveFlowStationKey(TestCaseStore::loadSelectedFlowStationName());
        if (!byName.isEmpty())
            stationKey = byName;
    }
    if (stationKey.isEmpty())
        stationKey = QStringLiteral("default");
    activeFlowStationKey_ = stationKey;

    QString tabName = stationName.isEmpty() ? QStringLiteral("自由工站") : stationName;
    const int profileVersion = TestCaseStore::loadStationProfileVersion(stationKey);
    if (profileVersion > 0)
        tabName += QStringLiteral(" v%1").arg(profileVersion);
    ui->tabWidget->setTabText(0, tabName);
    setupFreeWorkTabBar(ui->tabWidget);
    qDebug() << "[FreeWork] refresh tab, SelectedStationName =" << stationName << ", tabName =" << tabName;

    if (!QFile::exists(TestCasePaths::flowIniPath())) {
        showlog(QStringLiteral("未找到测试流程文件，请在设置页「测试流程编排」中配置"));
        qDebug() << "[FreeWork] flow ini missing:" << TestCasePaths::flowIniPath();
        updateTuplePositionUiVisible();
        applyStationSerialUiConfig();
        loadAndApplyStationDeviceSide();
        return;
    }

    stopFlowOnTestFail_ = TestCaseStore::loadStationStopFlowOnTestFail(stationKey, true);
    const QVector<TestFlowItemEntry> flowItems = TestCaseStore::loadStationFlowItems(stationKey);
    for (const TestFlowItemEntry& entry : flowItems) {
        if (entry.enabled)
            orderedTestCaseNames_.append(entry.caseName);
    }
    const QVector<TestFlowItemEntry> failItems = TestCaseStore::loadStationFailFlowItems(stationKey);
    for (const TestFlowItemEntry& entry : failItems) {
        if (entry.enabled)
            orderedFailCaseNames_.append(entry.caseName);
    }

    if (orderedTestCaseNames_.isEmpty()) {
        if (!flowItems.isEmpty()) {
            showlog(QStringLiteral("当前工站流程步骤均已取消勾选，请在设置页「测试流程编排」中重新勾选"));
            qDebug() << "[FreeWork] all flow items disabled, station =" << stationKey;
        } else {
            showlog(QStringLiteral("当前工站未配置测试步骤，请在设置页「测试流程编排」中添加"));
            qDebug() << "[FreeWork] empty flow, station =" << stationKey;
        }
    } else {
        qDebug() << "[FreeWork] 使用 test_case 流程, station =" << stationKey << ", items =" << orderedTestCaseNames_
                 << ", failItems =" << orderedFailCaseNames_;
    }
    updateTuplePositionUiVisible();
    applyStationSerialUiConfig();
    loadAndApplyStationDeviceSide();
}

bool QFreeWork::currentOrderedStepIsDongleBleConnect() const {
    const QStringList& orderedNames = activeOrderedCaseNames();
    if (teststate < 0 || teststate >= orderedNames.count()) {
        return false;
    }
    TestCaseDefinition caseDef;
    if (!TestCaseRunner::loadCaseForStation(activeFlowStationKey_, orderedNames.at(teststate), caseDef)) {
        return false;
    }
    if (TestCaseRunner::isDongleBleConnectStep(caseDef)) {
        return true;
    }
    return isDongleBleConnectStepName(caseDef.meta.name);
}

void QFreeWork::beginUiStartTest() {
    if (!dongleSerialPort->isOpen())
        on_connectButton_clicked();
    if (pack.factory == "lx" || pack.factory == "jj") {
        if (!usbSerialPort->isOpen())
            openUsbSerialPort();
    }
    if (!macAddress.isEmpty() && macAddress != QStringLiteral("没有mac地址"))
        ui->macLabel->setText("蓝牙mac: " + macAddress);

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: "
                                   "10px; padding: 10px; text-align: center; ");

    isTestContinue = true;
    teststate = -1;
    ui->test_time->setText(QStringLiteral("0.0 s"));
    resetLightSensorFivePointState();

    emit send_go_next_focus();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
}

void QFreeWork::startTest() {
    if (isTestContinue)
        return;
    testResultTableInit();
    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    const QString snText = ui->getMac->text().trimmed();
    if (!snText.isEmpty()) {
        mesProcessCode_ = snText;
        pack.sn = mesProcessCode_;
    }
    beginUiStartTest();
}

bool QFreeWork::canRunOrderedTestStepLoop() const {
    if (at->getConnected()) {
        return true;
    }
    if (stepRuntime_.started) {
        return true;
    }
    const QStringList& orderedNames = activeOrderedCaseNames();
    if (teststate >= 0 && teststate < orderedNames.count()) {
        TestCaseDefinition caseDef;
        if (TestCaseRunner::loadCaseForStation(activeFlowStationKey_, orderedNames.at(teststate), caseDef)) {
            return !TestCaseRunner::stepRequiresProductBle(caseDef);
        }
        return true;
    }
    return currentOrderedStepIsDongleBleConnect();
}

bool QFreeWork::isBydFactory() const {
    return pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0;
}

namespace {

QString formatMacFrom12Hex(const QString& macRawUpper) {
    return QStringLiteral("%1:%2:%3:%4:%5:%6")
        .arg(macRawUpper.mid(0, 2), macRawUpper.mid(2, 2), macRawUpper.mid(4, 2), macRawUpper.mid(6, 2),
             macRawUpper.mid(8, 2), macRawUpper.mid(10, 2));
}

QString parseMacFromSnXwdRule(const QString& snCode) {
    QString sn = snCode;
    sn.remove(QRegularExpression(QStringLiteral("\\s+")));
    // 按长度选偏移（与 test_base 一致，并保留失败时另一偏移兜底）：
    // - ≤28：PCBA SN，优先 offset=4；失败再试 11
    // - 35 / >28：整机或长条码，优先 offset=11；失败再试 4
    constexpr int kMacHexLen = 12;
    constexpr int kOffsetPcba = 4;
    constexpr int kOffsetWhole = 11;
    if (sn.length() < kOffsetPcba + kMacHexLen) {
        qDebug() << "[parseMacFromSn/xwd] 长度太短 trimLen=" << sn.length();
        return QStringLiteral("长度太短");
    }

    const auto tryOffset = [&](int offset) -> QString {
        if (sn.length() < offset + kMacHexLen)
            return {};
        const QString raw = sn.mid(offset, kMacHexLen).toUpper();
        if (!QRegularExpression(QStringLiteral("^[0-9A-F]{12}$")).match(raw).hasMatch())
            return {};
        return raw;
    };

    QString macRaw;
    if (sn.length() <= 28) {
        macRaw = tryOffset(kOffsetPcba);
        if (macRaw.isEmpty())
            macRaw = tryOffset(kOffsetWhole);
    } else {
        // 含 35 位整机 SN：从下标 11 取 MAC
        macRaw = tryOffset(kOffsetWhole);
        if (macRaw.isEmpty())
            macRaw = tryOffset(kOffsetPcba);
    }
    if (macRaw.isEmpty()) {
        const int failOffset = sn.length() <= 28 ? kOffsetPcba : kOffsetWhole;
        qDebug() << "[parseMacFromSn/xwd] 不符合规则 macRaw=" << sn.mid(failOffset, kMacHexLen).toUpper()
                 << "snLen=" << sn.length();
        return QStringLiteral("不符合规则");
    }
    const QString mac = formatMacFrom12Hex(macRaw);
    qDebug() << "[parseMacFromSn/xwd] ok" << mac << "snLen=" << sn.length();
    return mac;
}

} // namespace

QString QFreeWork::parseMacFromSn(const QString& snCode) const {
    // 一律按 SN 长度解析：28=PCBA(offset4)，35=整机(offset11)；板厂长码保留双偏移兜底
    return parseMacFromSnXwdRule(snCode);
}

QString QFreeWork::resolvedPcbaSnText() const {
    // 统一从界面 SN 框读取（MES / 三元组回填后也都写到此处）
    if (ui && ui->getMac) {
        return ui->getMac->text().trimmed();
    }
    return {};
}

QString QFreeWork::resolvedWholeMachineSnText() const {
    return wholeMachineSn_.trimmed();
}

void QFreeWork::setWholeMachineSn(const QString& sn) {
    wholeMachineSn_ = sn.trimmed();
    // 三元组整机 SN 只回填界面，供写入/校验；MES 过站 SFC 仍用开局过程码 mesProcessCode_
    if (ui && ui->getMac) {
        ui->getMac->setText(wholeMachineSn_);
    }
}

QString QFreeWork::resolvedExpectedTailSnText() const {
    return resolvedPcbaSnText();
}

QByteArray QFreeWork::resolvedTailSnToWrite() const {
    return resolvedPcbaSnText().toUtf8();
}

void QFreeWork::runTestFlowBootstrap() {
    // 每轮开测清空扫描缓存，避免按广播名连到过期 MAC/旧广播名
    deviceMap.clear();
    if (ui && ui->mac_combo)
        ui->mac_combo->clear();

    const QString sn = ui->getMac->text().trimmed();
    const QString mac = ui->macInput->text().trimmed();
    // initData 会清成员；开局过程码须跨 init 保留，供 BYD Complete/SFC 使用
    const QString processCode = !mesProcessCode_.isEmpty() ? mesProcessCode_ : sn;
    if (!sn.isEmpty() || !mac.isEmpty()) {
        onTestSessionStarting(sn, mac);
    }
    showlog(QStringLiteral("开始测试"));
    // 清掉上一轮残留的指令重试定时器，避免本轮同步治具步骤被误判超时
    if (commandRetryTimer)
        finishCommandRetryWait(true, QString());
    else
        sendRetryOver = false;
    // 先刷新流程再 initData，避免 seed 缓存与 dongle 动作仍用上一轮工站/队列。
    refreshOrderedTestIndexes();
    initData();
    mesProcessCode_ = processCode.trimmed();
    if (!mesProcessCode_.isEmpty()) {
        pack.sn = mesProcessCode_;
        showlog(QStringLiteral("MES 过站过程码已锁定：%1").arg(mesProcessCode_));
    }
    singleStepDebugRun_ = false;
    suppressProductBleAutoReconnect_ = false;
    waitWork(1000);
    showlog(QStringLiteral("MAC地址为：") + ui->macInput->text());
    teststate = 0;
}

bool QFreeWork::runSingleTestCaseStep(const QString& stationKey, const QString& caseName, QString* errorOut) {
    if (isTestContinue) {
        if (errorOut)
            *errorOut = QStringLiteral("当前工位正在测试中，请先点「停止」后再单步运行");
        return false;
    }
    const QString key = TestCaseStore::resolveFlowStationKey(stationKey.trimmed());
    const QString stepId = caseName.trimmed();
    if (key.isEmpty() || stepId.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("工站或步骤无效");
        return false;
    }
    TestCaseDefinition def;
    if (!TestCaseRunner::loadCaseForStation(key, stepId, def)) {
        if (errorOut)
            *errorOut = QStringLiteral("无法加载步骤「%1」").arg(stepId);
        return false;
    }

    showlog(QStringLiteral("单步运行：%1（工站 %2）").arg(TestCaseRunner::stepLabel(def), key));
    singleStepDebugRun_ = true;
    activeFlowStationKey_ = key;
    orderedTestCaseNames_ = QStringList{stepId};
    orderedFailCaseNames_.clear();
    runningFailFlow_ = false;
    stopFlowOnTestFail_ = true;
    freeWorkMesSegments_.clear();
    testResultTableInit();
    stepRuntime_.reset();
    clearActiveTestCase();
    resetLightSensorFivePointState();
    canGoNext = true;
    TestResult = passValue;
    ui->test_result->setText(QStringLiteral("WAIT"));
    ui->test_result->setStyleSheet(
        "font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
        "padding: 10px; text-align: center; ");
    TestTime.start();
    ui->test_time->setText(QStringLiteral("0.0 s"));
    teststate = 0;
    isTestContinue = true;
    return true;
}

const QStringList& QFreeWork::activeOrderedCaseNames() const {
    return runningFailFlow_ ? orderedFailCaseNames_ : orderedTestCaseNames_;
}

QStringList& QFreeWork::activeOrderedCaseNames() {
    return runningFailFlow_ ? orderedFailCaseNames_ : orderedTestCaseNames_;
}

bool QFreeWork::tickOrderedTestStepLoop() {
    const QStringList& orderedNames = activeOrderedCaseNames();
    const int stepCount = orderedNames.count();
    for (; teststate < stepCount;) {
        if (!isTestContinue)
            return false;
        TestCaseDefinition caseDef;
        QString functionName;
        const QString caseName = orderedNames.at(teststate);
        if (!TestCaseRunner::loadCaseForStation(activeFlowStationKey_, caseName, caseDef)) {
            ++teststate;
            stepRuntime_.reset();
            clearActiveTestCase();
            break;
        }
        functionName = TestCaseRunner::stepLabel(caseDef);
        // 须等 markDone 才过步（与 sendCommandWithRetry 的 allowResend 无关）
        const bool needAsyncDone = TestCaseRunner::needAsyncDone(caseDef);

        if (!stepRuntime_.started) {
            if (!canGoNext) {
                break;
            }
            stepRuntime_.started = true;
            stepRuntime_.done = false;
            stepRuntime_.pass = true;
            stepRuntime_.testData = QStringLiteral("-");
            stepRuntime_.ask = QStringLiteral("通过");
            stepRuntime_.caseTimer.restart();
            lastCommandRetryCount = 0;
            lastCommandFailReason.clear();
            testCasePromptAcknowledged_ = false;
            testCasePromptProgrammaticClose_ = false;
            testCaseCommandBegun_ = false;
            showlog((runningFailFlow_ ? QStringLiteral("失败区开始：") : QStringLiteral("开始测试内容："))
                    + functionName);
            setActiveTestCase(caseDef);
            showTestCasePromptForStep(caseDef);
            // 无卡控的提示：先卡住等「是」，点完再 beginStep；有卡控则立刻发、弹窗只提示
            if (TestCaseRunner::stepWaitsForPromptAck(caseDef)) {
                canGoNext = false;
                showlog(QStringLiteral("等待确认后再发送指令"));
                qDebug() << "程序在跑" << teststate << stepCount << (runningFailFlow_ ? "failFlow" : "mainFlow");
                break;
            }
            testCaseCommandBegun_ = true;
            TestCaseRunner::beginStep(this, caseDef);
            qDebug() << "程序在跑" << teststate << stepCount << (runningFailFlow_ ? "failFlow" : "mainFlow");
            break;
        }

        // 本步已 done 时不要再卡 canGoNext（避免异步重试标志残留导致永远不收尾）
        if (!canGoNext && !stepRuntime_.done) {
            break;
        }

        if (sendRetryOver) {
            sendRetryOver = false;
            const QString reason = lastCommandFailReason.trimmed().isEmpty()
                                       ? QStringLiteral("指令失败（原因未知）")
                                       : lastCommandFailReason.trimmed();
            lastCommandFailReason.clear();
            // 同步步骤已 markDone(通过) 后，忽略迟到的 sendCommandWithRetry 超时（勿打翻 pass）
            if (!(stepRuntime_.done && stepRuntime_.pass)) {
                if (!stepRuntime_.done) {
                    stepRuntime_.done = true;
                    stepRuntime_.pass = false;
                } else {
                    stepRuntime_.pass = false;
                }
                if (stepRuntime_.testData == QLatin1String("-") || stepRuntime_.testData.trimmed().isEmpty())
                    stepRuntime_.testData = reason;
                TestResult = failValue;
                showlog(QStringLiteral("步骤失败：%1（%2）").arg(functionName, reason));
            }
        }

        if (!caseDef.gate.enabled && canGoNext && !stepRuntime_.done && !sendRetryOver) {
            const bool dongleBleConnect = TestCaseRunner::isDongleBleConnectStep(caseDef);
            const bool productGet = !caseDef.hook.enabled && !dongleBleConnect && caseDef.send.channel == TestCaseSendChannel::Product && caseDef.send.action == TestCaseSendAction::Get;
            // 治具/产品串口 Get 等异步等待须由回包回调 markActiveTestCaseStepDone，不可在此直接 done
            const bool fixtureOrSerialAsync =
                caseDef.send.channel == TestCaseSendChannel::Fixture ||
                caseDef.send.channel == TestCaseSendChannel::ProductSerial;
            if (!caseDef.hook.enabled && !dongleBleConnect && !productGet && !fixtureOrSerialAsync) {
                if (!TestCaseRunner::stepWaitsForPromptAck(caseDef) || testCasePromptAcknowledged_) {
                    stepRuntime_.done = true;
                    stepRuntime_.pass = true;
                    if (stepRuntime_.testData == QLatin1String("-")) {
                        QString setText = setBatteryTestDataText(caseDef);
                        if (setText.isEmpty())
                            setText = lightCalibWriteTestDataText(caseDef);
                        if (setText.isEmpty())
                            setText = exceptionThresholdWriteTestDataText(caseDef);
                        if (setText.isEmpty())
                            setText = pumpParamWriteTestDataText(caseDef);
                        if (setText.isEmpty())
                            setText = heatTestWriteTestDataText(caseDef);
                        if (setText.isEmpty())
                            setText = vibrationTestWriteTestDataText(caseDef);
                        if (setText.isEmpty())
                            setText = cycleReportWriteTestDataText(caseDef);
                        stepRuntime_.testData = setText.isEmpty() ? QStringLiteral("ok") : setText;
                    }
                }
            } else if (dongleBleConnect && at->getConnected()) {
                stepRuntime_.done = true;
                stepRuntime_.pass = true;
                    stepRuntime_.testData = QStringLiteral("已连接");
            } else if (productGet && lastCommandRetryCount > 0) {
                // 无 Gate 的 Product Get：协议层已应答即可过步（有 Gate 时由 evaluateActiveTestCaseGate 收尾）
                stepRuntime_.done = true;
                stepRuntime_.pass = true;
                if (stepRuntime_.testData == QLatin1String("-") || stepRuntime_.testData.trimmed().isEmpty())
                    stepRuntime_.testData = QStringLiteral("ok");
            } else if (caseDef.hook.enabled && lastCommandRetryCount > 0) {
                // Hook + sendCommandWithRetry：产测应答后须结束步骤（如 MAC_WRITE_ROOT、SN_WRITE_TAIL）；
                // 按键/采样/串口仪器等 Hook 自行维护 stepRuntime_，不会走到 lastCommandRetryCount>0。
                const QString hookId = caseDef.hook.hookId;
                if (hookId == QStringLiteral("MAC_WRITE_ROOT") || hookId == QStringLiteral("SN_WRITE_TAIL")) {
                    stepRuntime_.done = true;
                    stepRuntime_.pass = true;
                }
            }
            // 阻塞型 Hook（如 DONGLE_SUCTION_SAMPLE）在 waitWork/QEventLoop 内会重入 startTask，
            // 不可凭 lastCommandRetryCount 提前 done，否则与采样循环并发导致第二次卡死。
        }

        if (needAsyncDone && !stepRuntime_.done) {
            break;
        }

        if (!stepRuntime_.pass) {
            TestResult = failValue;
        }

        const qint64 caseElapsedMs = stepRuntime_.caseTimer.isValid() ? stepRuntime_.caseTimer.elapsed() : 0;
        const int caseRetryCount = lastCommandRetryCount;
        showlog(QStringLiteral("测试内容完成：%1，重试次数=%2，测试时长=%3ms")
                    .arg(functionName)
                    .arg(caseRetryCount)
                    .arg(caseElapsedMs));
        const bool screenInspectMultiGate =
            testCaseMultiGateTableEmitted_ && caseDef.gate.enabled
            && caseDef.gate.reportType == QLatin1String("ProtocolScreenInspectData");
        // 屏幕图像识别：分项表已写后仍保留步骤汇总行（确认屏幕(颜色)）
        // 多字段卡控已写分项结果表/MES，步骤收尾不再追加汇总行（结果表与云平台/MES 均避免重复）
        const bool multiGateEmitted = testCaseMultiGateTableEmitted_;
        if (!multiGateEmitted || screenInspectMultiGate) {
            TestItem test;
            test.testItem = functionName;
            test.testData = stepRuntime_.testData;
            test.testResult = stepRuntime_.pass ? QStringLiteral("通过") : QStringLiteral("失败");
            test.ask = stepRuntime_.ask;
            testItems.append(test);
        }
        testCaseMultiGateTableEmitted_ = false;
        if (!multiGateEmitted)
            appendTestCaseMes(caseDef, stepRuntime_.pass, stepRuntime_.testData);
        if (caseDef.timing.delayAfterMs > 0)
            waitWork(caseDef.timing.delayAfterMs);
        closeTestCasePrompt();
        closeKeyWaitPrompt();
        clearActiveTestCase();
        testResultTableUpdate(testItems);

        ++teststate;
        if (!runningFailFlow_ && stopFlowOnTestFail_ && !stepRuntime_.pass) {
            if (!orderedFailCaseNames_.isEmpty()) {
                showlog(QStringLiteral("测试失败，执行失败区域步骤（共 %1 项）").arg(orderedFailCaseNames_.count()));
                runningFailFlow_ = true;
                teststate = 0;
            } else {
                showlog(QStringLiteral("测试失败，按流程设置结束后续步骤（失败区为空）"));
            teststate = orderedTestCaseNames_.count();
            }
        }
        stepRuntime_.reset();

        break;
    }
    return true;
}

void QFreeWork::finalizeTestFlowIfComplete() {
    const QStringList& orderedNames = activeOrderedCaseNames();
    const int flowStepCount = orderedNames.count();
    // teststate==0 表示尚未跑完任何步（含刚切入失败区），不收尾
    if (teststate != flowStepCount || teststate == 0) {
        return;
    }

    // 设置页单步调试：只展示结果，不过站、不断开设备连接，便于连续点「运行」
    if (singleStepDebugRun_) {
        singleStepDebugRun_ = false;
        if (TestResult == failValue) {
            ui->test_result->setText(QStringLiteral("FAIL"));
            ui->test_result->setStyleSheet(
                "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                "border-radius: 10px; padding: 10px; text-align: center; ");
        } else {
            ui->test_result->setText(QStringLiteral("PASS"));
            ui->test_result->setStyleSheet(
                "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                "border-radius: 10px; padding: 10px; text-align: center;");
        }
        showlog(QStringLiteral("单步运行结束（不过站、保持连接）"));
        teststate = -1;
        stepRuntime_.reset();
        clearActiveTestCase();
        ScreenInspectGigECapture::releaseSession();
        // 整轮结束后再清历史戳记图（步内不删，避免同一次多色证据被裁掉）
        ScreenInspectAnalyzer::cleanupStoredImages(
            QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect")));
        ui->test_time->setText(CommonUtils::formatElapsedSeconds(TestTime));
        isTestContinue = false;
        refreshOrderedTestIndexes();
        return;
    }

    // testItems 在 testResultTableUpdate 内会 clear；MES 分项与上表同步写入 freeWorkMesSegments_。
    const QString mesItemValue = joinFreeWorkMesItemvalue(freeWorkMesSegments_, TestResult, failValue);
    showlog(QStringLiteral("mesItemValue======") + mesItemValue);
    pack.itemvalue = mesItemValue;
    // BYD Complete/NcComplete 的 SFC 必须用开局过程码，禁止用界面上已被三元组改写的整机 SN
    if (!mesProcessCode_.isEmpty()) {
        pack.sn = mesProcessCode_;
        showlog(QStringLiteral("MES 过站 SFC（过程码）=%1").arg(mesProcessCode_));
    } else {
        pack.sn = ui->getMac->text().trimmed();
        showlog(QStringLiteral("MES 过站 SFC 回退界面 SN=%1（未记录过程码）").arg(pack.sn));
    }
    pack.mac = ui->macInput->text().trimmed();
    pack.product = SETTINGS.value("Mes/Product_Name").toString();
    pack.instruct_num = QStringLiteral("079");
    if (TestResult == failValue) {
        ui->test_result->setText(QStringLiteral("FAIL"));
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
            "border-radius: 10px; padding: 10px; text-align: center; ");
        pack.result = QStringLiteral("NG");
        QString failMesKey;
        QString failVal;
        if (!firstFreeWorkMesFailSegment(freeWorkMesSegments_, &failMesKey, &failVal)) {
            firstFailedRowFromResultTable(testResultTable(), &failMesKey, &failVal);
        }
        const QString failStepName =
            resolveFreeWorkFailStepDisplayName(activeFlowStationKey_, orderedNames, failMesKey);
        pack.remark = buildFreeWorkMesFailRemark(failStepName, failVal);
        if (!failMesKey.isEmpty()) {
            pack.error = failMesKey;
        }
        if (!pack.remark.isEmpty()) {
            showlog(QStringLiteral("MES 不良原因：%1").arg(pack.remark));
        }
    } else {
        ui->test_result->setText(QStringLiteral("PASS"));
        ui->test_result->setStyleSheet(
            "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
            "border-radius: 10px; padding: 10px; text-align: center;");
        pack.result = QStringLiteral("PASS");
        pack.remark.clear(); // 避免上轮 NG 备注残留
    }

    finishTestRecord(pack, ui->isusemes->checkState());

    qDebug() << "测试结束";
    teststate = -1;
    stepRuntime_.reset();
    ScreenInspectGigECapture::releaseSession();
    ScreenInspectAnalyzer::cleanupStoredImages(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect")));
    ui->test_time->setText(CommonUtils::formatElapsedSeconds(TestTime));
    ui->macInput->clear();
    ui->snInput->clear();
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);
    // 先退出本工位测试态；最后一个工位结束时才能安全释放共享 ASD 串口。
    isTestContinue = false;
    on_disconnectButton_clicked();
    if (auto* box = qobject_cast<QFreeWorkBox*>(window())) {
        box->releaseSharedAsd9026aIfIdle();
        box->releaseSharedTempLoggerIfIdle();
    }
    emit send_end_test(getIndex());
    ui->getMac->clear();
    mesProcessCode_.clear();
    // 焦点由 box_base::checkAllover 统一处理
}

void QFreeWork::closeEvent(QCloseEvent* event) {
    closeTestCasePrompt();
    closeKeyWaitPrompt();
    test_base::closeEvent(event);
}

void QFreeWork::startTask() {
    if (!isTestContinue) {
        return;
    }

    if (teststate == -1) {
        runTestFlowBootstrap();
    }
    if (teststate >= 0) {
        ui->test_time->setText(CommonUtils::formatElapsedSeconds(TestTime));
    }
    if (canRunOrderedTestStepLoop()) {
        tickOrderedTestStepLoop();
    } else if (teststate >= 0 && teststate < activeOrderedCaseNames().count() && !at->getConnected()) {
        // 流程里下一步要蓝牙，但当前未连接且没有扫描连接步骤时会静默停住
        TestCaseDefinition nextDef;
        if (TestCaseRunner::loadCaseForStation(activeFlowStationKey_, activeOrderedCaseNames().at(teststate), nextDef)
            && TestCaseRunner::stepRequiresProductBle(nextDef) && !stepRuntime_.started) {
            static qint64 s_lastBleWaitLogMs = 0;
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (nowMs - s_lastBleWaitLogMs > 3000) {
                s_lastBleWaitLogMs = nowMs;
                if (suppressProductBleAutoReconnect_) {
                    showlog(QStringLiteral("已主动断开蓝牙，禁止自动重连；当前步骤「%1」需要 BLE，请在流程中加入「扫描连接蓝牙」或去掉不需要的产品协议步")
                                .arg(TestCaseRunner::stepLabel(nextDef)));
                } else {
                    showlog(QStringLiteral("等待蓝牙连接后再执行：%1（流程需含「扫描连接蓝牙」或先手动连上）")
                                .arg(TestCaseRunner::stepLabel(nextDef)));
                    const QString mac = currentMacAddress();
                    if (!mac.isEmpty() && mac != QStringLiteral("没有mac地址") && at && dongleSerialPort
                        && dongleSerialPort->isOpen()) {
                        at->set(DongleCmd::BleScanConnect, mac);
                    }
                }
            }
        }
    }
    finalizeTestFlowIfComplete();
}

QFreeWork::~QFreeWork() {
    if (jigSerialPort->isOpen()) {
        disconnect(jigSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        jigSerialPort->close();
        qDebug() << getIndex() << "已关闭jig串口";
    }
    // ASRL 等 VISA 资源需显式释放，否则独占串口导致下次连不上
    resetVisaBackend();
    delete ui;
}

void QFreeWork::refreshDongleWifi(QString data) {
    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
}

void QFreeWork::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        // protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
        // showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void QFreeWork::refreshDongleUartState(int state) {
    const bool connected = state != 0;
    if (SETTINGS.value(QStringLiteral("SYSTEM/LockProductUI"), false).toBool()) {
        applySerialPortUiLock();
    } else {
        ui->comNameCombo->setEnabled(!connected);
        ui->connectButton->setEnabled(!connected);
        ui->disconnectButton->setEnabled(connected);
    }
    if (connected)
        showlog("dongle串口连接成功");
    else
        showlog("dongle串口连接断开");
}
void QFreeWork::refreshUsbUartState(int state) {
    const bool connected = state != 0;
    if (SETTINGS.value(QStringLiteral("SYSTEM/LockProductUI"), false).toBool()) {
        applySerialPortUiLock();
    } else {
        ui->usbcomNameCombo->setEnabled(!connected);
        ui->usbconnectButton->setEnabled(!connected);
        ui->usbdisconnectButton->setEnabled(connected);
    }
    if (connected) {
        showlog(QStringLiteral("万用表串口连接成功"));
    } else {
        showlog(QStringLiteral("万用表串口连接断开"));
    }
}

void QFreeWork::refreshJigUartState(int state) {
    const bool connected = state != 0;
    if (SETTINGS.value(QStringLiteral("SYSTEM/LockProductUI"), false).toBool()) {
        applySerialPortUiLock();
    } else {
        ui->jigComNameCombo->setEnabled(!connected);
        ui->jigConnectButton->setEnabled(!connected);
        ui->jigDisconnectButton->setEnabled(connected);
    }
    if (connected)
        showlog("治具串口连接成功");
    else
        showlog("治具串口连接断开");
}

void QFreeWork::refreshProductUartState(int state) {
    const bool connected = state != 0;
    if (SETTINGS.value(QStringLiteral("SYSTEM/LockProductUI"), false).toBool()) {
        applySerialPortUiLock();
    } else {
        ui->productComNameCombo->setEnabled(!connected);
        ui->productConnectButton->setEnabled(!connected);
        ui->productDisconnectButton->setEnabled(connected);
    }
    if (connected) {
        showlog(QStringLiteral("产品串口(仪器)连接成功"));
    } else {
        showlog(QStringLiteral("产品串口(仪器)连接断开"));
    }
}

void QFreeWork::startKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                                   const QString& enableKey) {
    if (!SETTINGS.value(enableKey).toBool()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = "按键配置未启用";
        stepRuntime_.ask = "请检查配置";
        TestResult = failValue;
        showlog(testName + "失败：按键配置未启用");
        return;
    }

    currentKeyTestName_ = testName;
    currentKeyExpectedKey_ = expectedKey;
    freeWorkKeyWaiting_ = true;
    stepRuntime_.done = false;
    stepRuntime_.pass = true;
    stepRuntime_.testData = "等待按键上报";
    stepRuntime_.ask = SETTINGS.value(expectedKey).toString();

    closeTestCasePrompt();
    closeKeyWaitPrompt();
    keyWaitPrompt_ = new QMessageBox(QMessageBox::Information, "按键测试", promptText, QMessageBox::NoButton, this);
    keyWaitPrompt_->setStandardButtons(QMessageBox::NoButton);
    QPushButton* hiddenCloseButton = keyWaitPrompt_->addButton("", QMessageBox::RejectRole);
    hiddenCloseButton->hide();
    keyWaitPrompt_->setAttribute(Qt::WA_DeleteOnClose);
    keyWaitPromptProgrammaticClose_ = false;
    applyTestItemPromptFont(keyWaitPrompt_);
    connect(keyWaitPrompt_, &QObject::destroyed, this, [this]() {
        keyWaitPrompt_ = nullptr;
        if (freeWorkKeyWaiting_ && !keyWaitPromptProgrammaticClose_) {
            ++plcKeyBleWaitSeq_;
            freeWorkKeyWaiting_ = false;
            plcSwitchBlePhase_ = 0;
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = "用户关闭按键弹窗";
            stepRuntime_.ask = SETTINGS.value(currentKeyExpectedKey_).toString();
            TestResult = failValue;
            showlog(currentKeyTestName_ + "失败：用户关闭按键弹窗");
        }
        keyWaitPromptProgrammaticClose_ = false;
    });
    keyWaitPrompt_->show();
    showlog("等待按键测试：" + testName);
}

void QFreeWork::waitPlcBleKeyReportBlocking() {
    const int bleWaitMs = SETTINGS.value(QStringLiteral("KeyTest/TimeoutMs"), 5000).toInt();
    const int pollMs = qMax(20, SETTINGS.value(QStringLiteral("KeyTest/PollIntervalMs"), 50).toInt());
    showlog(currentKeyTestName_ + QStringLiteral("：阻塞等待协议上报（waitWork，超时 %1ms）").arg(bleWaitMs));
    QElapsedTimer timer;
    timer.start();
    while (!stepRuntime_.done && timer.elapsed() < bleWaitMs) {
        waitWork(pollMs);
    }
    if (stepRuntime_.done) {
        return;
    }
    ++plcKeyBleWaitSeq_;
    freeWorkKeyWaiting_ = false;
    const int ph = plcSwitchBlePhase_;
    plcSwitchBlePhase_ = 0;
    stepRuntime_.done = true;
    stepRuntime_.pass = false;
    const QString plcPart = plcKeyBlePlcOkSummary_;
    plcKeyBlePlcOkSummary_.clear();
    QString tail = QStringLiteral("等待设备按键上报超时");
    if (ph == 3) {
        tail = QStringLiteral("等待左旋上报超时");
    } else if (ph == 4) {
        tail = QStringLiteral("等待右旋上报超时");
    }
    stepRuntime_.testData = plcPart.isEmpty() ? tail : plcPart + QStringLiteral("；") + tail;
    TestResult = failValue;
    showlog(currentKeyTestName_ + QStringLiteral("失败：%1").arg(tail));
}

void QFreeWork::armPlcBleKeyWaitTimeout() {
    const int bleWaitMs = SETTINGS.value(QStringLiteral("KeyTest/TimeoutMs"), 5000).toInt();
    const quint64 armSeq = ++plcKeyBleWaitSeq_;
    QTimer::singleShot(bleWaitMs, this, [this, armSeq]() {
        if (armSeq != plcKeyBleWaitSeq_) {
            return;
        }
        if (!freeWorkKeyWaiting_) {
            return;
        }
        const int ph = plcSwitchBlePhase_;
        closeKeyWaitPrompt();
        freeWorkKeyWaiting_ = false;
        plcSwitchBlePhase_ = 0;
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        const QString plcPart = plcKeyBlePlcOkSummary_;
        plcKeyBlePlcOkSummary_.clear();
        QString tail = QStringLiteral("等待设备按键上报超时");
        if (ph == 3) {
            tail = QStringLiteral("等待左旋上报超时");
        } else if (ph == 4) {
            tail = QStringLiteral("等待右旋上报超时");
        }
        stepRuntime_.testData = plcPart.isEmpty() ? tail : plcPart + QStringLiteral("；") + tail;
        TestResult = failValue;
        showlog(currentKeyTestName_ + QStringLiteral("失败：%1").arg(tail));
    });
    showlog(currentKeyTestName_ + QStringLiteral("：等待协议上报（超时 %1ms）").arg(bleWaitMs));
}

void QFreeWork::closeKeyWaitPrompt() {
    if (keyWaitPrompt_ != nullptr) {
        keyWaitPromptProgrammaticClose_ = true;
        keyWaitPrompt_->close();
        keyWaitPrompt_ = nullptr;
    }
}

void QFreeWork::showTestCasePromptForStep(const TestCaseDefinition& def) {
    closeTestCasePrompt();
    // Hook 步骤（按键/PLC 等）在钩子内自行弹窗，避免与 Meta/Prompt 重复叠两个框
    if (def.hook.enabled)
        return;
    if (!def.meta.promptEnabled)
        return;
    const QString text = def.meta.promptText.trimmed();
    if (text.isEmpty())
        return;
    const QString title = def.meta.name.trimmed();
    // 弹窗两种形态：
    // 1) 卡控已开：只提示、立刻发指令并等上报，无按钮防误点，通过/超时后关窗
    // 2) 卡控未开：点「是」再发指令/过步；点「否」本步失败（纯弹窗可人工判 fail）
    const bool waitReportGate = def.gate.enabled;
    testCasePrompt_ = new QMessageBox(QMessageBox::Information, title, text,
                                      waitReportGate ? QMessageBox::NoButton
                                                     : (QMessageBox::Yes | QMessageBox::No),
                                      this);
    if (!waitReportGate) {
    if (QAbstractButton* yesBtn = testCasePrompt_->button(QMessageBox::Yes))
        yesBtn->setText(QStringLiteral("是"));
        if (QAbstractButton* noBtn = testCasePrompt_->button(QMessageBox::No))
            noBtn->setText(QStringLiteral("否"));
    testCasePrompt_->setDefaultButton(QMessageBox::Yes);
    } else {
        QPushButton* hiddenCloseButton = testCasePrompt_->addButton(QString(), QMessageBox::RejectRole);
        if (hiddenCloseButton)
            hiddenCloseButton->setVisible(false);
    }
    testCasePrompt_->setAttribute(Qt::WA_DeleteOnClose);
    testCasePromptProgrammaticClose_ = false;
    applyTestItemPromptFont(testCasePrompt_);
    connect(testCasePrompt_, &QMessageBox::finished, this, [this](int result) {
        if (!testCasePromptProgrammaticClose_)
            onTestCasePromptAcknowledged(result == QMessageBox::Yes);
    });
    connect(testCasePrompt_, &QObject::destroyed, this, [this]() {
        testCasePrompt_ = nullptr;
        testCasePromptProgrammaticClose_ = false;
    });
    testCasePrompt_->show();
}

void QFreeWork::onTestCasePromptAcknowledged(bool accepted) {
    testCasePromptAcknowledged_ = true;
    if (!stepRuntime_.started || stepRuntime_.done || !testCaseStepActive_)
        return;
    if (!TestCaseRunner::stepWaitsForPromptAck(activeTestCase_))
        return;
    // 点「否」：本步失败，不发指令
    if (!accepted) {
        if (!testCaseCommandBegun_)
            testCaseCommandBegun_ = true;
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.ask = QStringLiteral("失败");
        if (stepRuntime_.testData == QLatin1String("-"))
            stepRuntime_.testData = QStringLiteral("人工判定失败");
        TestResult = failValue;
        showlog(QStringLiteral("弹窗确认：否，本步失败"));
        canGoNext = true;
        return;
    }
    // 纯提示：确认即过步。有指令：点「是」后再下发，避免未操作产品就已发出。
    if (activeTestCase_.meta.promptOnly) {
        if (!testCaseCommandBegun_) {
            testCaseCommandBegun_ = true;
            TestCaseRunner::beginStep(this, activeTestCase_);
        }
    stepRuntime_.done = true;
    stepRuntime_.pass = true;
    if (stepRuntime_.testData == QLatin1String("-"))
        stepRuntime_.testData = QStringLiteral("已确认");
        canGoNext = true;
        return;
    }
    if (testCaseCommandBegun_)
        return;
    testCaseCommandBegun_ = true;
    TestCaseRunner::beginStep(this, activeTestCase_);
}

void QFreeWork::closeTestCasePrompt() {
    if (testCasePrompt_ == nullptr)
        return;
    QMessageBox* box = testCasePrompt_;
    testCasePrompt_ = nullptr;
    testCasePromptProgrammaticClose_ = true;
    box->close();
}

void QFreeWork::applyFreeWorkExtraTabsVisible(bool visible) {
    QTabBar* bar = ui->tabWidget->tabBar();
    if (!bar)
        return;

    const int expandIdx = ui->tabWidget->indexOf(ui->tab_2);
    const int chartIdx = ui->tabWidget->indexOf(ui->tab_chart);
    const int screenIdx = ui->tabWidget->indexOf(ui->tab_screenInspect);
    const int bleIdx = ui->tabWidget->indexOf(ui->tab_3);
    bar->setTabVisible(expandIdx, visible);
    bar->setTabVisible(chartIdx, visible);
    bar->setTabVisible(screenIdx, visible);
    bar->setTabVisible(bleIdx, visible);

    if (!visible) {
        const int cur = ui->tabWidget->currentIndex();
        if (cur == expandIdx || cur == chartIdx || cur == screenIdx || cur == bleIdx)
            ui->tabWidget->setCurrentIndex(kFreeWorkTabMain);
    }

    if (ui->toggleExtraTabsButton)
        ui->toggleExtraTabsButton->setText(visible ? QStringLiteral("隐藏扩展页") : QStringLiteral("显示扩展页"));

    setupFreeWorkTabBar(ui->tabWidget);
}

void QFreeWork::on_toggleExtraTabsButton_clicked() {
    QTabBar* bar = ui->tabWidget->tabBar();
    if (!bar)
        return;
    const int expandIdx = ui->tabWidget->indexOf(ui->tab_2);
    applyFreeWorkExtraTabsVisible(!bar->isTabVisible(expandIdx));
}

void QFreeWork::loadSuctionGateSettings() {
    // 仅作未执行吸力采样前的界面缺省；实际卡控以工站 steps/*.ini 中步骤参数为准。
    suctionSampleDurationMs_ = 10000;
    suctionSampleIntervalMs_ = 20;
    suctionPeakTargetKpa_ = -36.0;
    suctionPeakToleranceKpa_ = 2.6;
    suctionPeakDiffMaxKpa_ = 2.6;
    suctionSingleChannelIndex_ = 0;
    suctionPeakBaselineKpa_ = -8.0;
    suctionPeakDipStartKpa_ = -10.0;
    suctionMinPeakCount_ = 3;
    suctionOffsetKpa_ = 0.0;
}

void QFreeWork::applySuctionGateFromStepParam(const QVariant& param) {
    loadSuctionGateSettings();
    if (!param.canConvert<QVariantMap>())
        return;
    const QVariantMap map = param.toMap();
    if (map.contains(QStringLiteral("sampleDurationMs")))
        suctionSampleDurationMs_ = map.value(QStringLiteral("sampleDurationMs")).toInt();
    if (map.contains(QStringLiteral("sampleIntervalMs")))
        suctionSampleIntervalMs_ = map.value(QStringLiteral("sampleIntervalMs")).toInt();
    // 优先用步骤内显式范围；否则兼容旧的 target±tolerance
    if (map.contains(QStringLiteral("peakLowKpa")) && map.contains(QStringLiteral("peakHighKpa"))) {
        const double low = map.value(QStringLiteral("peakLowKpa")).toDouble();
        const double high = map.value(QStringLiteral("peakHighKpa")).toDouble();
        suctionPeakTargetKpa_ = (low + high) / 2.0;
        suctionPeakToleranceKpa_ = qAbs(high - low) / 2.0;
    } else {
        if (map.contains(QStringLiteral("peakTargetKpa")))
            suctionPeakTargetKpa_ = map.value(QStringLiteral("peakTargetKpa")).toDouble();
        if (map.contains(QStringLiteral("peakToleranceKpa")))
            suctionPeakToleranceKpa_ = map.value(QStringLiteral("peakToleranceKpa")).toDouble();
    }
    if (map.contains(QStringLiteral("peakDiffMaxKpa")))
        suctionPeakDiffMaxKpa_ = map.value(QStringLiteral("peakDiffMaxKpa")).toDouble();
    if (map.contains(QStringLiteral("peakBaselineKpa")))
        suctionPeakBaselineKpa_ = map.value(QStringLiteral("peakBaselineKpa")).toDouble();
    if (map.contains(QStringLiteral("peakDipStartKpa")))
        suctionPeakDipStartKpa_ = map.value(QStringLiteral("peakDipStartKpa")).toDouble();
    if (map.contains(QStringLiteral("minPeakCount")))
        suctionMinPeakCount_ = qMax(1, map.value(QStringLiteral("minPeakCount")).toInt());
    if (map.contains(QStringLiteral("offsetKpa")))
        suctionOffsetKpa_ = map.value(QStringLiteral("offsetKpa")).toDouble();
    // channel：1/2/3（兼容旧 left/right → CH1/CH2）
    auto parseChannelIndex = [](const QString& raw) -> int {
        const QString ch = raw.trimmed().toLower();
        if (ch == QStringLiteral("2") || ch == QStringLiteral("ch2") || ch == QStringLiteral("right")
            || ch == QStringLiteral("r") || ch == QStringLiteral("右") || ch == QStringLiteral("右口"))
            return 1;
        if (ch == QStringLiteral("3") || ch == QStringLiteral("ch3") || ch == QStringLiteral("third"))
            return 2;
        return 0;
    };
    if (map.contains(QStringLiteral("channelIndex"))) {
        const int idx = map.value(QStringLiteral("channelIndex")).toInt();
        suctionSingleChannelIndex_ = (idx >= 1 && idx <= 3) ? (idx - 1) : qBound(0, idx, 2);
    } else if (map.contains(QStringLiteral("channel"))) {
        suctionSingleChannelIndex_ = parseChannelIndex(map.value(QStringLiteral("channel")).toString());
    }
}

void QFreeWork::initSuctionChart() {
    if (!ui->suctionPlotHost || suctionPlot_ != nullptr)
        return;

    auto* layout = new QVBoxLayout(ui->suctionPlotHost);
    layout->setContentsMargins(0, 0, 0, 0);
    suctionPlot_ = new QCustomPlot(ui->suctionPlotHost);
    layout->addWidget(suctionPlot_);

    suctionPlot_->legend->setVisible(true);
    suctionPlot_->addGraph();
    suctionPlot_->addGraph();
    suctionPlot_->graph(0)->setPen(QPen(QColor(30, 120, 220), 2));
    suctionPlot_->graph(0)->setName(QStringLiteral("CH1"));
    suctionPlot_->graph(1)->setPen(QPen(QColor(220, 80, 50), 2));
    suctionPlot_->graph(1)->setName(QStringLiteral("CH2"));
    suctionPlot_->xAxis->setLabel(QStringLiteral("时间(s)"));
    suctionPlot_->yAxis->setLabel(QStringLiteral("吸力(kPa)"));
    suctionPlot_->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    suctionPlot_->xAxis->setRange(0, 10);
    suctionPlot_->yAxis->setRange(-40, 0);

    suctionChartHintText_ = new QCPItemText(suctionPlot_);
    suctionChartHintText_->position->setType(QCPItemPosition::ptAxisRectRatio);
    suctionChartHintText_->position->setCoords(0.5, 0.5);
    suctionChartHintText_->setPositionAlignment(Qt::AlignCenter);
    suctionChartHintText_->setColor(QColor(120, 120, 120));
    suctionChartHintText_->setVisible(false);

    suctionPlot_->replot();
}

void QFreeWork::resetSuctionChart() {
    suctionChartTimeSec_.clear();
    suctionChartLeftKpa_.clear();
    suctionChartRightKpa_.clear();
    suctionChartTimerStarted_ = false;
    suctionChartLastUiMs_ = 0;
    suctionLeftPeakInit_ = false;
    suctionRightPeakInit_ = false;
    suctionLeftPeakHigh_ = 0.0;
    suctionLeftPeakLow_ = 0.0;
    suctionRightPeakHigh_ = 0.0;
    suctionRightPeakLow_ = 0.0;

    if (ui->suctionLiveLeftLabel)
        ui->suctionLiveLeftLabel->setText(QStringLiteral("CH1实时：--"));
    if (ui->suctionLiveRightLabel)
        ui->suctionLiveRightLabel->setText(QStringLiteral("CH2实时：--"));
    if (ui->suctionLeftPeakHighLabel)
        ui->suctionLeftPeakHighLabel->setText(QStringLiteral("CH1最高：--"));
    if (ui->suctionLeftPeakLowLabel)
        ui->suctionLeftPeakLowLabel->setText(QStringLiteral("CH1最低：--"));
    if (ui->suctionRightPeakHighLabel)
        ui->suctionRightPeakHighLabel->setText(QStringLiteral("CH2最高：--"));
    if (ui->suctionRightPeakLowLabel)
        ui->suctionRightPeakLowLabel->setText(QStringLiteral("CH2最低：--"));
    if (ui->suctionPeakDiffLabel)
        ui->suctionPeakDiffLabel->setText(QStringLiteral("通道峰差：--"));

    if (suctionPlot_) {
        suctionPlot_->graph(0)->data()->clear();
        suctionPlot_->graph(1)->data()->clear();
        suctionPlot_->xAxis->setRange(0, 10);
        suctionPlot_->yAxis->setRange(-40, 0);
        if (suctionChartHintText_)
            suctionChartHintText_->setVisible(false);
        suctionPlot_->replot(QCustomPlot::rpQueuedReplot);
    }
}

void QFreeWork::updateSuctionPeakLabels() {
    // 采样窗口内：最高=数值最大，最低=数值最小；峰差=CH1/CH2最低值之差的绝对值
    if (ui->suctionLeftPeakHighLabel) {
        ui->suctionLeftPeakHighLabel->setText(suctionLeftPeakInit_
                                                  ? QStringLiteral("CH1最高：%1 kPa").arg(suctionLeftPeakHigh_, 0, 'f', 3)
                                                  : QStringLiteral("CH1最高：--"));
    }
    if (ui->suctionLeftPeakLowLabel) {
        ui->suctionLeftPeakLowLabel->setText(suctionLeftPeakInit_
                                                 ? QStringLiteral("CH1最低：%1 kPa").arg(suctionLeftPeakLow_, 0, 'f', 3)
                                                 : QStringLiteral("CH1最低：--"));
    }
    if (ui->suctionRightPeakHighLabel) {
        ui->suctionRightPeakHighLabel->setText(suctionRightPeakInit_
                                                   ? QStringLiteral("CH2最高：%1 kPa").arg(suctionRightPeakHigh_, 0, 'f', 3)
                                                   : QStringLiteral("CH2最高：--"));
    }
    if (ui->suctionRightPeakLowLabel) {
        ui->suctionRightPeakLowLabel->setText(suctionRightPeakInit_
                                                  ? QStringLiteral("CH2最低：%1 kPa").arg(suctionRightPeakLow_, 0, 'f', 3)
                                                  : QStringLiteral("CH2最低：--"));
    }
    if (ui->suctionPeakDiffLabel) {
        if (suctionLeftPeakInit_ && suctionRightPeakInit_) {
            const double peakDiff = qAbs(suctionLeftPeakLow_ - suctionRightPeakLow_);
            ui->suctionPeakDiffLabel->setText(QStringLiteral("CH1-CH2峰差：%1 kPa").arg(peakDiff, 0, 'f', 3));
        } else {
            ui->suctionPeakDiffLabel->setText(QStringLiteral("通道峰差：--"));
        }
    }
}

void QFreeWork::appendSuctionChartSample(double leftKpa, double rightKpa) {
    if (!suctionChartTimerStarted_) {
        suctionChartTimer_.start();
        suctionChartTimerStarted_ = true;
        suctionChartLastUiMs_ = 0;
    }
    const double tSec = suctionChartTimer_.elapsed() / 1000.0;
    suctionChartTimeSec_.append(tSec);
    suctionChartLeftKpa_.append(leftKpa);
    suctionChartRightKpa_.append(rightKpa);

    if (!suctionLeftPeakInit_) {
        suctionLeftPeakHigh_ = leftKpa;
        suctionLeftPeakLow_ = leftKpa;
        suctionLeftPeakInit_ = true;
    } else {
        suctionLeftPeakHigh_ = qMax(suctionLeftPeakHigh_, leftKpa);
        suctionLeftPeakLow_ = qMin(suctionLeftPeakLow_, leftKpa);
    }
    if (!suctionRightPeakInit_) {
        suctionRightPeakHigh_ = rightKpa;
        suctionRightPeakLow_ = rightKpa;
        suctionRightPeakInit_ = true;
    } else {
        suctionRightPeakHigh_ = qMax(suctionRightPeakHigh_, rightKpa);
        suctionRightPeakLow_ = qMin(suctionRightPeakLow_, rightKpa);
    }

    // 采样期间不 replot：只攒向量，结束时 finalizeSuctionChartPlot 一次画完整条。
    // 吸力曲线页上的 CH1/CH2 标签仍节流刷新，确认在采数；向量按每个 AT 点全量入库。
    constexpr qint64 kLabelThrottleMs = 50;
    const qint64 nowMs = suctionChartTimer_.elapsed();
    if (nowMs - suctionChartLastUiMs_ < kLabelThrottleMs)
        return;
    suctionChartLastUiMs_ = nowMs;

    if (ui->suctionLiveLeftLabel)
        ui->suctionLiveLeftLabel->setText(QStringLiteral("CH1实时：%1 kPa").arg(leftKpa, 0, 'f', 3));
    if (ui->suctionLiveRightLabel)
        ui->suctionLiveRightLabel->setText(QStringLiteral("CH2实时：%1 kPa").arg(rightKpa, 0, 'f', 3));
    updateSuctionPeakLabels();
}

void QFreeWork::finalizeSuctionChartPlot() {
    if (ui->suctionLiveLeftLabel && !suctionChartLeftKpa_.isEmpty())
        ui->suctionLiveLeftLabel->setText(
            QStringLiteral("CH1实时：%1 kPa").arg(suctionChartLeftKpa_.constLast(), 0, 'f', 3));
    if (ui->suctionLiveRightLabel && !suctionChartRightKpa_.isEmpty())
        ui->suctionLiveRightLabel->setText(
            QStringLiteral("CH2实时：%1 kPa").arg(suctionChartRightKpa_.constLast(), 0, 'f', 3));
    updateSuctionPeakLabels();

    if (!suctionPlot_)
        return;
    if (suctionChartHintText_)
        suctionChartHintText_->setVisible(false);
    suctionPlot_->graph(0)->setData(suctionChartTimeSec_, suctionChartLeftKpa_);
    suctionPlot_->graph(1)->setData(suctionChartTimeSec_, suctionChartRightKpa_);
    const double tSec = suctionChartTimeSec_.isEmpty() ? 0.0 : suctionChartTimeSec_.constLast();
    suctionPlot_->xAxis->setRange(0, qMax(10.0, tSec + 1.0));
    if (suctionLeftPeakInit_ && suctionRightPeakInit_) {
        const double yMin = qMin(suctionLeftPeakLow_, suctionRightPeakLow_);
        const double yMax = qMax(suctionLeftPeakHigh_, suctionRightPeakHigh_);
        const double yPad = qMax(0.5, (yMax - yMin) * 0.1);
        suctionPlot_->yAxis->setRange(yMin - yPad, yMax + yPad);
    }
    suctionPlot_->replot();
}

void QFreeWork::on_clearSuctionChartButton_clicked() {
    resetSuctionChart();
}

void QFreeWork::rememberScreenInspectImages(const QImage& capture, const QImage& annotated, const QImage& reference,
                                            const QString& folder, bool calibrationGuides) {
    screenInspectCapture_ = capture;
    screenInspectAnnotated_ = annotated;
    screenInspectReference_ = reference;
    screenInspectFolder_ = folder;
    screenInspectCalibGuides_ = calibrationGuides;
    updateScreenInspectPreview();
}

void QFreeWork::updateScreenInspectPreview() {
    const QImage& shot =
        !screenInspectAnnotated_.isNull() ? screenInspectAnnotated_ : screenInspectCapture_;
    const QRect roi = ScreenInspectAnalyzer::parseManualRoi(
        SETTINGS.value(QStringLiteral("ScreenInspect/Roi")).toString());
    if (ui->label_screenInspectShotCaption) {
        ui->label_screenInspectShotCaption->setText(
            screenInspectCalibGuides_ ? QStringLiteral("本次拍摄（校准线）")
                                      : QStringLiteral("本次拍摄（坏点标注）"));
    }
    if (ui->label_screenInspectRefCaption) {
        ui->label_screenInspectRefCaption->setText(
            screenInspectCalibGuides_ ? QStringLiteral("参考图（校准线）")
                                      : QStringLiteral("标准参考图（无标注）"));
    }
    fitScreenInspectThumb(ui->label_screenInspectShot, shot, QStringLiteral("尚无拍摄图"),
                          screenInspectAnnotated_.isNull() ? roi : QRect());
    // 校准步骤：参考图已带校准线；其它步骤参考图保持干净
    fitScreenInspectThumb(ui->label_screenInspectRef, screenInspectReference_, QStringLiteral("尚无参考图"),
                          screenInspectCalibGuides_ ? QRect() : QRect());
    const bool hasImg = !screenInspectCapture_.isNull();
    if (ui->viewScreenInspectLargeButton)
        ui->viewScreenInspectLargeButton->setEnabled(hasImg);
}

void QFreeWork::showScreenInspectViewer() {
    if (screenInspectCapture_.isNull()) {
        showlog(QStringLiteral("还没有屏幕拍照，请先跑屏幕检测/校准步骤"));
        return;
    }
    auto* dlg = new QDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setWindowTitle(screenInspectCalibGuides_ ? QStringLiteral("屏幕摄像头位置校准对照")
                                                  : QStringLiteral("屏幕拍照查看"));
    dlg->resize(1100, 620);
    auto* root = new QVBoxLayout(dlg);
    auto* pics = new QHBoxLayout();
    auto addPane = [&](const QString& title, const QImage& img) {
        auto* col = new QVBoxLayout();
        col->addWidget(new QLabel(title, dlg));
        auto* scroll = new QScrollArea(dlg);
        scroll->setWidgetResizable(true);
        auto* pic = new QLabel(scroll);
        pic->setAlignment(Qt::AlignCenter);
        if (img.isNull())
            pic->setText(QStringLiteral("无"));
        else {
            const QImage scaled = img.scaled(900, 900, Qt::KeepAspectRatio, Qt::FastTransformation);
            pic->setPixmap(QPixmap::fromImage(scaled));
        }
        scroll->setWidget(pic);
        col->addWidget(scroll, 1);
        pics->addLayout(col, 1);
    };
    addPane(screenInspectCalibGuides_ ? QStringLiteral("本次拍摄（校准线）")
                                      : QStringLiteral("本次拍摄（坏点标注）"),
            !screenInspectAnnotated_.isNull() ? screenInspectAnnotated_ : screenInspectCapture_);
    addPane(screenInspectCalibGuides_ ? QStringLiteral("参考图（校准线）") : QStringLiteral("标准参考图"),
            screenInspectReference_);
    root->addLayout(pics, 1);
    auto* buttons = new QDialogButtonBox(dlg);
    auto* folderBtn = buttons->addButton(QStringLiteral("打开图片文件夹"), QDialogButtonBox::ActionRole);
    buttons->addButton(QDialogButtonBox::Close);
    connect(folderBtn, &QPushButton::clicked, this, &QFreeWork::openScreenInspectFolder);
    connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::close);
    connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::close);
    root->addWidget(buttons);
    dlg->show();
}

void QFreeWork::openScreenInspectFolder() {
    QString dir = screenInspectFolder_;
    if (dir.isEmpty())
        dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
    CommonUtils::ensureDirectory(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void QFreeWork::on_viewScreenInspectLargeButton_clicked() {
    showScreenInspectViewer();
}

void QFreeWork::on_openScreenInspectFolderButton_clicked() {
    openScreenInspectFolder();
}

void QFreeWork::setDongleSuctionReadEnabled(bool enabled) {
    const bool wasEnabled = dongleSuctionReadEnabled_;
    dongleSuctionReadEnabled_ = enabled;
    if (enabled && !wasEnabled) {
        resetSuctionChart();
        // 采样期间空图，给提示避免被当成画不出曲线
        if (suctionPlot_ && suctionChartHintText_) {
            suctionChartHintText_->setText(QStringLiteral("采集中…采样结束后绘制完整曲线"));
            suctionChartHintText_->setVisible(true);
            suctionPlot_->replot(QCustomPlot::rpQueuedReplot);
        }
    }
    if (at && dongleSerialPort && dongleSerialPort->isOpen())
        at->set(DongleCmd::GetSuction, enabled ? 1 : 0);
}

bool QFreeWork::screenInspectAskHumanPassOnAutoFail(const QString& autoFailDetail) {
    const QString detail = autoFailDetail.trimmed().isEmpty() ? QStringLiteral("未通过") : autoFailDetail.trimmed();
    QMessageBox box(QMessageBox::Question, QStringLiteral("屏幕检测人工确认"),
                    QStringLiteral("图像自动识别未通过：%1\n\n是否确认屏幕有问题？\n"
                                   "点「是」=确认有问题（不通过）\n"
                                   "点「否」=目视正常（通过）")
                        .arg(detail),
                    QMessageBox::Yes | QMessageBox::No, this);
    if (QAbstractButton* yesBtn = box.button(QMessageBox::Yes))
        yesBtn->setText(QStringLiteral("是"));
    if (QAbstractButton* noBtn = box.button(QMessageBox::No))
        noBtn->setText(QStringLiteral("否"));
    box.setDefaultButton(QMessageBox::Yes);
    applyTestItemPromptFont(&box);
    return box.exec() == QMessageBox::Yes;
}

void QFreeWork::runScreenInspectStep() {
    const bool deadMode = activeTestCase_.send.deviceCmd == QStringLiteral("ScreenDeadPixelCheck");
    const bool anomalyMode = activeTestCase_.send.deviceCmd == QStringLiteral("ScreenDisplayAnomalyCheck");
    const bool calibMode = activeTestCase_.send.deviceCmd == QStringLiteral("ScreenCameraCalibration");
    if (!deadMode && !anomalyMode && !calibMode) {
        TestResult = failValue;
        markActiveTestCaseStepDone(false, QStringLiteral("未知屏幕检测指令"), QStringLiteral("失败"));
        return;
    }

    QVariantMap map;
    if (activeTestCase_.send.param.canConvert<QVariantMap>())
        map = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();

    auto mapInt = [&](const QString& key, int defVal) {
        if (!map.contains(key) || map.value(key).toString().trimmed().isEmpty())
            return defVal;
        bool ok = false;
        const int v = map.value(key).toInt(&ok);
        return ok ? v : defVal;
    };

    const int cameraIndex = mapInt(QStringLiteral("cameraIndex"),
                                   SETTINGS.value(QStringLiteral("ScreenInspect/CameraIndex"), 0).toInt());
    const QString cameraName = map.value(QStringLiteral("cameraName")).toString().trimmed();
    // 自由工站默认 GigE；Param_cameraSource=usb 时仍走 USB 摄像头
    const QString cameraSource = map.value(QStringLiteral("cameraSource")).toString().trimmed().toLower();
    const bool useUsb = (cameraSource == QLatin1String("usb"));
    QString cameraIp = map.value(QStringLiteral("cameraIp")).toString().trimmed();
    if (cameraIp.isEmpty())
        cameraIp = map.value(QStringLiteral("gigeIp")).toString().trimmed();
    if (cameraIp.isEmpty())
        cameraIp = SETTINGS.value(QStringLiteral("ScreenInspect/GigEIp"), QStringLiteral("169.254.64.10"))
                       .toString()
                       .trimmed();
    const QString cameraSerial = map.value(QStringLiteral("cameraSerial")).toString().trimmed();
    const int warmupMs = qBound(0, mapInt(QStringLiteral("warmupMs"), 450), 8000);
    int expectedColor = -1;
    if (map.contains(QStringLiteral("expectedColor"))) {
        bool colorOk = false;
        expectedColor =
            ScreenInspectAnalyzer::parseColorIndex(map.value(QStringLiteral("expectedColor")).toString(), &colorOk);
        if (!colorOk)
            expectedColor = -1;
    }
    const int deadDiff = mapInt(QStringLiteral("deadDiff"),
                                SETTINGS.value(QStringLiteral("ScreenInspect/DeadPixelDiff"), 35).toInt());
    const int saveCapture = mapInt(QStringLiteral("saveCapture"), 1);
    QString referencePath = map.value(QStringLiteral("referencePath")).toString().trimmed();
    if (referencePath.isEmpty())
        referencePath = SETTINGS.value(QStringLiteral("ScreenInspect/ReferencePath")).toString().trimmed();

    const QString modeName = calibMode   ? QStringLiteral("位置校准")
                             : deadMode  ? QStringLiteral("坏点分析")
                                         : QStringLiteral("显示对比");
    if (useUsb) {
        showlog(QStringLiteral("本步调用 USB 摄像头对屏幕拍照（%1）：index=%2%3 预热%4ms")
                    .arg(modeName)
                    .arg(cameraIndex)
                    .arg(cameraName.isEmpty() ? QString() : QStringLiteral(" name=%1").arg(cameraName))
                    .arg(warmupMs));
    } else {
        showlog(QStringLiteral("本步调用 GigE 相机对屏幕拍照（%1）：IP=%2%3 预热%4ms")
                    .arg(modeName)
                    .arg(cameraIp.isEmpty() ? QStringLiteral("(自动第一台)") : cameraIp)
                    .arg(cameraSerial.isEmpty() ? QString() : QStringLiteral(" SN=%1").arg(cameraSerial))
                    .arg(warmupMs));
    }

    QImage curr;
    QString captureErr;
    QElapsedTimer phaseT;
    phaseT.start();
    const bool grabbed =
        useUsb ? ScreenInspectCapture::grabStill(cameraIndex, cameraName, warmupMs, &curr, &captureErr)
               : ScreenInspectGigECapture::grabStill(cameraIp, cameraSerial, warmupMs, &curr, &captureErr,
                                                    nullptr, true);
    const qint64 msGrab = phaseT.elapsed();
    if (!grabbed) {
        TestResult = failValue;
        showlog(QStringLiteral("屏幕拍照失败：%1").arg(captureErr));
        markActiveTestCaseStepDone(false, captureErr, QStringLiteral("失败"));
        return;
    }

    QImage ref;
    if (anomalyMode || calibMode) {
        QString path = referencePath;
        if (!path.isEmpty() && !QFileInfo(path).isAbsolute())
            path = QDir(QCoreApplication::applicationDirPath()).filePath(path);
        if (path.isEmpty()) {
            path = QDir(QCoreApplication::applicationDirPath())
                       .filePath(QStringLiteral("screen_inspect/参考图/reference.png"));
            if (!QFileInfo::exists(path))
                path = QDir(QCoreApplication::applicationDirPath())
                           .filePath(QStringLiteral("screen_inspect/reference.png"));
        }
        if (!ref.load(path) || ref.isNull()) {
            TestResult = failValue;
            showlog(QStringLiteral("%1失败：未加载参考图（%2）。请在调试页保存参考图或填写 Param_referencePath")
                        .arg(calibMode ? QStringLiteral("位置校准") : QStringLiteral("显示异常检查"))
                        .arg(path));
            markActiveTestCaseStepDone(false, QStringLiteral("无参考图"), QStringLiteral("失败"));
            return;
        }
    }

    QString roiText = map.value(QStringLiteral("roi")).toString().trimmed();
    if (roiText.isEmpty())
        roiText = SETTINGS.value(QStringLiteral("ScreenInspect/Roi")).toString().trimmed();
    const QRect manualRoi = ScreenInspectAnalyzer::parseManualRoi(roiText);
    if (!manualRoi.isNull())
        showlog(QStringLiteral("使用划定检测范围：%1,%2 %3x%4")
                    .arg(manualRoi.x())
                    .arg(manualRoi.y())
                    .arg(manualRoi.width())
                    .arg(manualRoi.height()));

    // 位置校准：左右同套校准框/圆屏线，供操作员目视对位（不做坏点/相似度卡控）
    if (calibMode) {
        phaseT.restart();
        QRect usedRoi;
        const QImage currGuides = ScreenInspectAnalyzer::drawGuides(curr, manualRoi, nullptr, &usedRoi);
        QRect refRoi = usedRoi;
        if (curr.width() > 0 && curr.height() > 0 && ref.width() > 0 && ref.height() > 0
            && curr.size() != ref.size()) {
            refRoi = QRect(usedRoi.x() * ref.width() / curr.width(),
                           usedRoi.y() * ref.height() / curr.height(),
                           qMax(1, usedRoi.width() * ref.width() / curr.width()),
                           qMax(1, usedRoi.height() * ref.height() / curr.height()));
        }
        const QImage refGuides = ScreenInspectAnalyzer::drawGuides(ref, refRoi, &curr, nullptr);
        const qint64 msGuide = phaseT.elapsed();

        const QString dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
        CommonUtils::ensureDirectory(dir);
        QStringList uploadImagePaths;
        phaseT.restart();
        {
            const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
            const QString capPath = dir + QLatin1Char('/') + stamp + QStringLiteral("_calib_capture.jpg");
            const QString refPath = dir + QLatin1Char('/') + stamp + QStringLiteral("_calib_reference.jpg");
            if (currGuides.save(capPath, "JPG", 90))
                uploadImagePaths << capPath;
            if (refGuides.save(refPath, "JPG", 90))
                uploadImagePaths << refPath;
        }
        currGuides.save(dir + QStringLiteral("/last_capture.jpg"), "JPG", 90);
        refGuides.save(dir + QStringLiteral("/last_reference.jpg"), "JPG", 90);
        if (!uploadImagePaths.isEmpty())
            Qlog::addScreenInspectImageFiles(getIndex(), uploadImagePaths);
        rememberScreenInspectImages(curr, currGuides, refGuides, dir, true);
        if (ui->tabWidget && ui->tab_screenInspect)
            ui->tabWidget->setCurrentWidget(ui->tab_screenInspect);
        showlog(QStringLiteral("位置校准对照已刷新：抓图%1ms 画线%2ms。请对比「屏幕拍照」页左右校准线是否重合")
                    .arg(msGrab)
                    .arg(msGuide));

        QMessageBox box(QMessageBox::Question, QStringLiteral("屏幕摄像头位置校准"),
                        QStringLiteral("请对照「屏幕拍照」页：\n"
                                       "左=本次拍摄+校准线，右=参考图+同一套校准线。\n\n"
                                       "位置是否对准？\n"
                                       "点「是」=对准，继续测试\n"
                                       "点「否」=未对准，本步失败"),
                        QMessageBox::Yes | QMessageBox::No, this);
        if (QAbstractButton* yesBtn = box.button(QMessageBox::Yes))
            yesBtn->setText(QStringLiteral("是"));
        if (QAbstractButton* noBtn = box.button(QMessageBox::No))
            noBtn->setText(QStringLiteral("否"));
        box.setDefaultButton(QMessageBox::Yes);
        applyTestItemPromptFont(&box);
        const bool aligned = (box.exec() == QMessageBox::Yes);
        if (aligned) {
            showlog(QStringLiteral("位置校准：操作员确认已对准"));
            markActiveTestCaseStepDone(true, QStringLiteral("已对准"), QStringLiteral("校准"));
        } else {
            TestResult = failValue;
            showlog(QStringLiteral("位置校准：操作员确认未对准"));
            markActiveTestCaseStepDone(false, QStringLiteral("未对准"), QStringLiteral("校准"));
        }
        return;
    }

    ScreenInspectAnalyzer::Params ap;
    ap.deadDiff = deadDiff;
    ap.expectedColor = expectedColor;
    ap.manualRoi = manualRoi;
    // 未启用的卡控项跳过重计算；坏点专用指令仍强制扫坏点
    {
        bool needDead = deadMode;
        bool needSsim = false;
        for (const TestCaseGate& g : TestCaseStore::activeGatesForEvaluation(activeTestCase_)) {
            const QString f = g.field.trimmed();
            if (f == QLatin1String("deadPixels") || f == QLatin1String("muraStd"))
                needDead = true;
            if (f == QLatin1String("ssim") || f == QLatin1String("similarity"))
                needSsim = true;
        }
        ap.enableDeadPixels = needDead;
        ap.enableSsim = needSsim;
        if (!needDead || !needSsim) {
            showlog(QStringLiteral("本步分析裁剪：坏点=%1 相似度=%2")
                        .arg(needDead ? QStringLiteral("开") : QStringLiteral("跳过"),
                             needSsim ? QStringLiteral("开") : QStringLiteral("跳过")));
        }
    }
    phaseT.restart();
    const ScreenInspectAnalyzer::Report report = ScreenInspectAnalyzer::analyze(curr, ref, ap);
    const qint64 msAnalyze = phaseT.elapsed();

    const QString dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect"));
    CommonUtils::ensureDirectory(dir);
    QStringList uploadImagePaths;
    // 参考图也叠同一套识别框/圆屏线，便于归档对照
    QImage refMarked;
    if (!ref.isNull()) {
        QRect refRoi = report.roi;
        if (curr.width() > 0 && curr.height() > 0 && ref.width() > 0 && ref.height() > 0
            && curr.size() != ref.size() && !report.roi.isNull()) {
            refRoi = QRect(report.roi.x() * ref.width() / curr.width(),
                           report.roi.y() * ref.height() / curr.height(),
                           qMax(1, report.roi.width() * ref.width() / curr.width()),
                           qMax(1, report.roi.height() * ref.height() / curr.height()));
        }
        refMarked = ScreenInspectAnalyzer::drawGuides(ref, refRoi, &curr);
    }
    // 高分辨率 PNG 压缩极慢（曾出现分析完后 UI 卡死近 1 分钟）；证据图改 JPEG，识别仍用内存原图
    // 存盘再压长边，避免数千万像素 JPEG 编码拖慢节拍
    auto forSave = [](const QImage& src) -> QImage {
        constexpr int kSaveMaxSide = 1600;
        if (src.isNull() || qMax(src.width(), src.height()) <= kSaveMaxSide)
            return src;
        return src.scaled(kSaveMaxSide, kSaveMaxSide, Qt::KeepAspectRatio, Qt::FastTransformation);
    };
    phaseT.restart();
    {
        const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
        const QImage capSave = forSave(curr);
        const QString capPath = dir + QLatin1Char('/') + stamp + QStringLiteral("_capture.jpg");
        if (capSave.save(capPath, "JPG", 85))
            uploadImagePaths << capPath;
        if (!report.annotated.isNull()) {
            const QImage markSave = forSave(report.annotated);
            const QString markPath = dir + QLatin1Char('/') + stamp + QStringLiteral("_mark.jpg");
            if (markSave.save(markPath, "JPG", 85))
                uploadImagePaths << markPath;
        }
        if (!refMarked.isNull() && saveCapture) {
            const QImage refSave = forSave(refMarked);
            const QString refPath = dir + QLatin1Char('/') + stamp + QStringLiteral("_reference.jpg");
            if (refSave.save(refPath, "JPG", 85))
                uploadImagePaths << refPath;
        }
    }
    forSave(curr).save(dir + QStringLiteral("/last_capture.jpg"), "JPG", 85);
    if (!report.annotated.isNull())
        forSave(report.annotated).save(dir + QStringLiteral("/last_mark.jpg"), "JPG", 85);
    if (!refMarked.isNull())
        forSave(refMarked).save(dir + QStringLiteral("/last_reference.jpg"), "JPG", 85);
    // 步内不清理：同一次多色的 capture/mark 要留到收尾上传；整轮结束/停止测试时再 cleanupStoredImages
    const qint64 msSave = phaseT.elapsed();
    if (!uploadImagePaths.isEmpty())
        Qlog::addScreenInspectImageFiles(getIndex(), uploadImagePaths);
    phaseT.restart();
    rememberScreenInspectImages(curr, report.annotated, ref, dir, false);
    const qint64 msPreview = phaseT.elapsed();
    // 分阶段耗时写入 UI 日志，便于现场确认是拍照还是识别/存图慢
    showlog(QStringLiteral("屏幕检测耗时：拍照(含预热)%1ms 识别分析%2ms 存图%3ms 预览%4ms；图%5x%6 坏点=%7 分析裁剪坏点=%8 相似度=%9")
                .arg(msGrab)
                .arg(msAnalyze)
                .arg(msSave)
                .arg(msPreview)
                .arg(curr.width())
                .arg(curr.height())
                .arg(report.deadPixels)
                .arg(ap.enableDeadPixels ? QStringLiteral("开") : QStringLiteral("跳过"))
                .arg(ap.enableSsim ? QStringLiteral("开") : QStringLiteral("跳过")));
    qDebug().noquote() << QStringLiteral("[ScreenInspectStep]")
                       << QStringLiteral("size=%1x%2 grab=%3ms analyze=%4ms save=%5ms preview=%6ms dead=%7")
                              .arg(curr.width())
                              .arg(curr.height())
                              .arg(msGrab)
                              .arg(msAnalyze)
                              .arg(msSave)
                              .arg(msPreview)
                              .arg(report.deadPixels);

    ProtocolScreenInspectData data;
    data.deadPixels = report.deadPixels;
    data.muraStd = report.muraStd;
    data.ssim = report.ssim;
    data.detectedColor = report.detectedColor;
    data.colorMatch = report.colorMatch;
    const QString ssimText = data.ssim < 0 ? QStringLiteral("未比") : QString::number(data.ssim, 'f', 3);
    const QString detectedName = ScreenInspectAnalyzer::colorName(report.detectedColor);
    QString colorLog;
    if (expectedColor < 0)
        colorLog = QStringLiteral("未指定期望色，实测%1").arg(detectedName);
    else if (report.colorMatch == 1)
        colorLog = QStringLiteral("期望%1，实测%1，颜色匹配")
                       .arg(ScreenInspectAnalyzer::colorName(expectedColor));
    else
        colorLog = QStringLiteral("期望%1，实测%2，颜色不匹配")
                       .arg(ScreenInspectAnalyzer::colorName(expectedColor), detectedName);
    showlog(QStringLiteral("拍照完成并已分析对比：%1；坏点=%2 亮度起伏=%3 相似度=%4 ROI=%5x%6")
                .arg(colorLog)
                .arg(data.deadPixels)
                .arg(data.muraStd, 0, 'f', 1)
                .arg(ssimText)
                .arg(report.roi.width())
                .arg(report.roi.height()));

    const QVariant payload = QVariant::fromValue(data);
    // 已启用 Gate 时统一走 evaluateActiveTestCaseGate（含多字段分项表与人工确认），
    // 不可在颜色不匹配处提前 return，否则坏点/相似度等测试项不会写入结果表。
    if (activeTestCase_.gate.enabled) {
        evaluateActiveTestCaseGate(QStringLiteral("ProtocolScreenInspectData"), payload);
        return;
    }

    if (expectedColor >= 0 && data.colorMatch == 0) {
        showlog(QStringLiteral("屏幕检测自动识别未通过：%1").arg(colorLog));
        const QString askColor = ScreenInspectAnalyzer::colorName(expectedColor);
        bool allowHumanOverride = true;
        if (activeTestCase_.send.param.canConvert<QVariantMap>()) {
            const QVariantMap paramMap = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();
            if (paramMap.contains(QStringLiteral("allowHumanOverride"))) {
                allowHumanOverride = paramMap.value(QStringLiteral("allowHumanOverride")).toBool();
            } else if (paramMap.contains(QStringLiteral("askHumanOnFail"))) {
                allowHumanOverride = paramMap.value(QStringLiteral("askHumanOnFail")).toBool();
            }
        }

        if (allowHumanOverride && screenInspectAskHumanPassOnAutoFail(askColor)) {
            showlog(QStringLiteral("人工确认：屏幕显示%1，本步通过").arg(askColor));
            markActiveTestCaseStepDone(true, detectedName + QStringLiteral("（人工确认通过）"), askColor);
        } else {
            TestResult = failValue;
            if (!allowHumanOverride) {
                showlog(QStringLiteral("自动判定未通过，且该步骤配置为禁用人工确认，直接判定失败"));
            } else {
                showlog(QStringLiteral("人工确认：屏幕未显示%1，本步不通过").arg(askColor));
            }
            markActiveTestCaseStepDone(false, detectedName, askColor);
        }
        return;
    }

    bool pass = true;
    QStringList reasons;
    if (deadMode) {
        const int maxDead = SETTINGS.value(QStringLiteral("ScreenInspect/MaxDeadPixels"), 8).toInt();
        if (data.deadPixels > maxDead) {
            pass = false;
            reasons.append(QStringLiteral("坏点%1 超过上限%2").arg(data.deadPixels).arg(maxDead));
        } else {
            showlog(QStringLiteral("坏点卡控通过：当前%1，允许≤%2").arg(data.deadPixels).arg(maxDead));
        }
    }
    if (anomalyMode) {
        const double minSsim = SETTINGS.value(QStringLiteral("ScreenInspect/MinSimilarity"), 0.90).toDouble();
        if (data.ssim < minSsim) {
            pass = false;
            reasons.append(QStringLiteral("相似度%1 低于下限%2")
                               .arg(data.ssim, 0, 'f', 3)
                               .arg(minSsim, 0, 'f', 2));
        } else {
            showlog(QStringLiteral("相似度卡控通过：当前%1，允许≥%2")
                        .arg(data.ssim, 0, 'f', 3)
                        .arg(minSsim, 0, 'f', 2));
        }
        // 未开 Gate 时显示对比只卡相似度；灰度标准差仅纯色画面有意义，灰阶条纹勿用
    }

    const QString testData = deadMode
                                 ? detectedName
                                 : (anomalyMode ? ssimText
                                                : QStringLiteral("坏点=%1 颜色=%2")
                                                      .arg(data.deadPixels)
                                                      .arg(detectedName));
    const QString askText = expectedColor >= 0 ? ScreenInspectAnalyzer::colorName(expectedColor)
                                               : (anomalyMode ? QStringLiteral("相似度") : QStringLiteral("通过"));
    if (!pass) {
        const QString failReason = reasons.join(QStringLiteral("；"));
        showlog(QStringLiteral("屏幕检测自动识别未通过：%1").arg(failReason));
        const QString askColor = expectedColor >= 0 ? ScreenInspectAnalyzer::colorName(expectedColor)
                                                    : askText;
        bool allowHumanOverride = true;
        if (activeTestCase_.send.param.canConvert<QVariantMap>()) {
            const QVariantMap paramMap = resolveTestCaseSendParamTree(activeTestCase_.send.param).toMap();
            if (paramMap.contains(QStringLiteral("allowHumanOverride"))) {
                allowHumanOverride = paramMap.value(QStringLiteral("allowHumanOverride")).toBool();
            } else if (paramMap.contains(QStringLiteral("askHumanOnFail"))) {
                allowHumanOverride = paramMap.value(QStringLiteral("askHumanOnFail")).toBool();
            }
        }

        if (allowHumanOverride && screenInspectAskHumanPassOnAutoFail(askColor)) {
            showlog(QStringLiteral("人工确认：屏幕显示%1，本步通过").arg(askColor));
            markActiveTestCaseStepDone(true, testData + QStringLiteral("（人工确认通过）"),
                                       askText.isEmpty() ? QStringLiteral("通过") : askText);
        } else {
            TestResult = failValue;
            if (!allowHumanOverride) {
                showlog(QStringLiteral("自动判定未通过，且该步骤配置为禁用人工确认，直接判定失败"));
            } else {
                showlog(QStringLiteral("人工确认：屏幕未显示%1，本步不通过").arg(askColor));
            }
            markActiveTestCaseStepDone(false, testData, askText.isEmpty() ? QStringLiteral("失败") : askText);
        }
        return;
    }
    markActiveTestCaseStepDone(true, testData, askText);
}

void QFreeWork::initData(bool deferDongleAtForVisa) {
    ui->product_sn->setText("芯片存储的整机sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;

    rssitestfailcount = 0;
    wifistate = 0;
    measure_ammeter = 0;
    dongleOutTime = 10;
    canGoNext = 1;
    stepRuntime_.reset();
    suppressProductBleAutoReconnect_ = false;
    isovertime = 0;
    dongleSuctionSampleActive_ = false;
    dongleSuctionCh1Samples_.clear();
    dongleSuctionCh2Samples_.clear();
    dongleSuctionSampleTimeSec_.clear();
    // 一并清 Qlog 暂存：上一轮若已投递但未被导出取走，本轮中止时会误传上一轮采样/截图
    Qlog::setSuctionSamples(getIndex(), {}, {}, {}, {});
    Qlog::addScreenInspectImageFiles(getIndex(), {});
    resetSuctionChart();
    // 首步 GPIB 时勿发 AT+SUCTION=0，与单步一致，避免 USB 与 GPIB 并发 ABORT
    if (deferDongleAtForVisa)
        dongleSuctionReadEnabled_ = false;
    else
        setDongleSuctionReadEnabled(false);
    // 逻辑缓存每轮清空；GPIB/TCPIP 物理会话尽量复用，避免第二次 MAC 开测立刻 viOpen 触发 ABORT
    huilingVisaLinkCache_.clear();
    seedHuilingVisaLinkCacheFromFlowOrSettings();
    BT_RSSI = "";
    BLE_RSSI = "";
    WIFI_RSSI = "";
    softwareVersionForReport_.clear();
    softwareVersionPassForReport_ = true;
    freeWorkKeyWaiting_ = false;
    keyWaitPromptProgrammaticClose_ = false;
    ++plcKeyBleWaitSeq_;
    plcKeyBlePlcOkSummary_.clear();
    plcSwitchBlePhase_ = 0;
    plcKeyCapPollMode_ = false;
    plcKeyCapPassSummary_.clear();
    currentKeyCapRequestKk_ = -1;
    currentKeyConfiguredId_ = 0;
    resetPlcKeyCapSyncReadState();
    currentKeyTestName_.clear();
    currentKeyExpectedKey_.clear();
    closeKeyWaitPrompt();
    closeTestCasePrompt();
    lastBrushInstrumentProfile_ = -1;
    cmwFacade_.run(CmwGprfCommand::ResetSession, makeCmwRunParams());
    plcFacade_.disconnect();
    clearProductInstrumentWatch();
    is_battary_test = 0;
    charageresult = "未测";
    voltageresult = "未测";
    currentresult = "未测";
    pb->reset_all_pb();
    at->resetConnected();
    TestResult = passValue;
    wifiresult = "未测";
    bleresult = "未测";
    ui->battary_state->setText("充电状态为:");
    ui->battary_value->setText("电量为:");
    ui->battary_voltage->setText("电压为:");
    deviceTailSnFromDevice = "";
    wholeMachineSn_.clear();
    mesProcessCode_.clear();
    pack.sku.clear();
    tupleData_ = TupleApplyResult{};
    QTupleService::clearSharedSession();
    resetTuplePositionHighlight();
    freeWorkMesSegments_.clear();
    pack.remark.clear(); // 开测清空，避免上轮 NG 备注带到本轮
    pack.error = QStringLiteral("NULL");
    ui->test_time->setText(QStringLiteral("0.0 s"));
    TestTime.start();
}

PlcV3RunParams QFreeWork::makePlcRunParams(int keyIndex0To6) {
    PlcV3RunParams params;
    params.stationIndex = getIndex();
    params.keyIndex0To6 = keyIndex0To6;
    params.modbus = &modbusManager;
    params.log = [this](const QString& line) { showlog(line); };
    params.isTestContinue = [this]() { return isTestContinue; };
    return params;
}

CmwGprfRunParams QFreeWork::makeCmwRunParams(const QString& scenarioLabel, int brushProfile) {
    CmwGprfRunParams params;
    params.scpi = scpiVisaManager();
    params.scenarioLabel = scenarioLabel;
    params.brushProfile = brushProfile;
    params.log = [this](const QString& line) { showlog(line); };
    params.wait = [this](int ms) { waitWork(ms); };
    return params;
}

void QFreeWork::applyPlcStepResult(const PlcV3RunResult& result, PlcV3Command command, bool finishStepRuntime) {
    if (!result.ok) {
        stepRuntime_.pass = false;
        stepRuntime_.testData = result.summary;
        stepRuntime_.done = true;
        if (!result.summary.isEmpty()) {
            showlog(result.summary);
        }
        return;
    }
    if (command == PlcV3Command::TouchSwitch) {
        return;
    }
    if (command == PlcV3Command::TouchKey) {
        stepRuntime_.pass = true;
        if (finishStepRuntime) {
            stepRuntime_.testData = result.summary;
            stepRuntime_.done = true;
            plcKeyBlePlcOkSummary_.clear();
        } else if (stepRuntime_.done) {
            if (!result.summary.isEmpty()) {
                stepRuntime_.testData = stepRuntime_.testData.isEmpty()
                    ? result.summary
                    : QStringLiteral("%1；%2").arg(result.summary, stepRuntime_.testData);
            }
            plcKeyBlePlcOkSummary_.clear();
        } else {
            stepRuntime_.testData = result.summary;
            stepRuntime_.done = false;
            plcKeyBlePlcOkSummary_ = result.summary;
        }
        if (!result.summary.isEmpty()) {
            showlog(result.summary);
        }
        return;
    }
    stepRuntime_.pass = true;
    stepRuntime_.testData = result.summary;
    stepRuntime_.done = true;
}

PlcV3RunResult QFreeWork::runPlcV3(PlcV3Command command, int keyIndex0To6, bool finishStepRuntime) {
    Q_UNUSED(finishStepRuntime);
    PlcV3RunParams params = makePlcRunParams(keyIndex0To6);
    if (command == PlcV3Command::TouchKey && plcKeyCapPollMode_) {
        params.pollCapDuringPress = [this](QString* errOut, QString* outSummary) {
            return pollKeyCapDuringPress(errOut, outSummary);
        };
    }
    // 业务层 plcFacade_（business/），与 cmwFacade_ 同级。
    return plcFacade_.run(command, params);
}

CmwGprfRunResult QFreeWork::runCmwGprf(CmwGprfCommand command, const QString& scenarioLabel, int brushProfile,
                                       int alignedPostTrigHoldMs, bool* outRanBurst) {
    CmwGprfRunParams params = makeCmwRunParams(scenarioLabel, brushProfile);
    params.alignedPostTrigHoldMs = alignedPostTrigHoldMs;
    params.outRanBurst = outRanBurst;
    return cmwFacade_.run(command, params);
}

void QFreeWork::on_disconnectwifi_clicked() {
    if (at->getConnected()) {
        protocolManager.set(DeviceCmd::WifiDisconnect);
        showlog("已发送断开wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}
void QFreeWork::on_connectwifi_clicked() {
    QString wifiName = SETTINGS.value(QString("WIFI/Name%1").arg(getIndex())).toString();
    QString wifiPassword = SETTINGS.value("WIFI/Password").toString();

    // QString wifiName = SETTINGS.value("WIFI/Name").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected()) {
        protocolManager.set(DeviceCmd::WifiConnect, QVariant::fromValue(WifiConnectPayload{wifiNameBytes, wifiPasswordBytes}));
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接设备后再试");
    }
}

void QFreeWork::on_macInput_returnPressed() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    if (pack.factory == "lx" || pack.factory == "jj") {
        if (!usbSerialPort->isOpen()) {
            openUsbSerialPort();
        }
    }

    // 检查是否是mac格式
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        if (ui->just_banding->checkState()) {
            bindingMacSn(macAddress, ui->getMac->text()); //获取测试数据不要绑定测试mac——sn
            ui->test_result->setText("PASS");
            ui->test_result->setStyleSheet(
                "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                "border-radius: 10px; padding: 10px; text-align: center;");
            ui->getMac->clear();
            ui->getMac->setFocus();

            return;
        }

        beginUiStartTest();
    }
}

void QFreeWork::on_pushButton_clicked() {
    runPlcModbusConnectTest();
}

void QFreeWork::on_pushButton_2_clicked() {
    at->set(DongleCmd::BleLog, 1); // 日志开
}

void QFreeWork::on_getMac_returnPressed() {
    testResultTableInit();

    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");
    applyAdaptiveV3ProductBySn(ui->getMac);

    if (!validateSnFormat(ui->getMac->text())) {
        ui->getMac->clear();
        return;
    }
    // 开局扫码即 MES 过程码（过站 SFC），后续 MES 主板 SN / 三元组整机 SN 不得覆盖此字段
    mesProcessCode_ = ui->getMac->text().trimmed();
    pack.sn = mesProcessCode_;
    showlog("正在查询mac地址");
    processGetMesTestValue(); // mes获取
    // getMac(ui->getMac->text());             // 文件获取
    if (ui->isusemes->checkState()) {
        processInspection(ui->getMac->text());
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
}

void QFreeWork::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            // Start 的 SFC 同样用开局过程码（勿被后续界面改写影响）
            pack.sn = !mesProcessCode_.isEmpty() ? mesProcessCode_ : inputSnText;

            pack.mechines = getIndex();

            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit send_process_inspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState()) // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void QFreeWork::processGetMesTestValue() {
    // 不从 MES 拉 MAC：未勾选「从 mes 获取 mac」、或 xwd（暂不调 getTestData/BTMAC）
    const bool localMacFromSn =
        (ui && ui->isformmes && !ui->isformmes->checkState())
        || pack.factory.trimmed().compare(QStringLiteral("xwd"), Qt::CaseInsensitive) == 0;
    if (localMacFromSn) {
        QString mesmacAddress = parseMacFromSn(ui->getMac->text());
        if (!mesmacAddress.isEmpty()) {
            ui->macInput->setText(mesmacAddress);
            showlog(QStringLiteral("本地 SN 解析 MAC 成功: ") + mesmacAddress);
            on_macInput_returnPressed();
        } else {
            showlog(QStringLiteral("本地从 SN 解析 MAC 失败: ") + ui->getMac->text());
        }
        return;
    }
    if (pack.factory == "hz") {
        pack.sn = !mesProcessCode_.isEmpty() ? mesProcessCode_ : ui->getMac->text();
        pack.mechines = getIndex();
        getTestValue(getIndex(), pack.sn.trimmed());
        return;
    }
    if (ui->isformmes->checkState()) {
        pack.sn = !mesProcessCode_.isEmpty() ? mesProcessCode_ : ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit send_mes_test_value(pack);
    }
}
void QFreeWork::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");             // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) { // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                     // 逐行读取文件
            QString line = in.readLine();         // 读取一行
            QStringList fields = line.split(","); // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {
                QString sn = fields.at(0);  // 第一个字段是sn
                QString mac = fields.at(1); // 第二个字段是mac
                if (sn == sn_to_search) {   // 检查是否是待检索的sn
                    {
                        ui->macInput->setText(mac);
                        on_macInput_returnPressed();
                        showlog("这是从文件获取的mac地址");
                        qDebug() << getIndex() << "The corresponding mac is: " << mac;
                    }

                    break;
                }
            }
        }

        file.close(); // 关闭文件
    }
    if (!ui->isformmes->isChecked() && ui->macInput->text().isEmpty()) {
        ui->getMac->clear();
        showlog("找不到mac地址，清空当前输入的sn");
    }
}
void QFreeWork::getMacAddress(const QByteArray& byte) {
    receivedData = "";
    receivedData = receivedData + QString::fromUtf8(byte);

    if (receivedData.length() >= 2 && receivedData.right(2) == "\r\n") {
        // 使用正则表达式提取设备信息
        static const QRegularExpression regex("deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
        QRegularExpressionMatch match = regex.match(receivedData);
        QString deviceName, deviceAddress, deviceRssi;

        // qDebug() << getIndex()<< "数据长度" << receivedData;
        // 检查是否匹配成功
        if (match.hasMatch()) {
            deviceName = match.captured(1).trimmed();
            deviceAddress = match.captured(2).trimmed();
            deviceRssi = match.captured(3).trimmed();
            // qDebug() << getIndex()<< "设备名称：" << deviceName;
            // qDebug() << getIndex()<< "设备地址：" << deviceAddress;
            // qDebug() << getIndex()<< "设备RSSI：" << deviceRssi;

            deviceMap[deviceAddress]["Name"] = deviceName;
            deviceMap[deviceAddress]["Rssi"] = deviceRssi;

            updateComboBox();
        }
        receivedData.clear();
    }
}
void QFreeWork::on_clear_scan_clicked() {
    deviceMap.clear();
    ui->mac_combo->clear();
}
void QFreeWork::updateComboBox() {
    // 遍历设备信息，根据 rssi 的值进行过滤
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        QString deviceAddress = it.key();
        QString deviceName = it.value()["Name"];
        QString deviceRssi = it.value()["Rssi"];

        if (deviceName.contains(ui->name_range->text()) && deviceRssi.toInt() > ui->rssi_range->text().toInt() &&
            deviceAddress.length() == 17)

        {
            int index = ui->mac_combo->findText(deviceAddress);
            qDebug() << getIndex() << "信号强度:" << deviceRssi;
            if (index == -1) {
                ui->mac_combo->addItem(deviceAddress);
                qDebug() << getIndex() << "有新增" << deviceAddress;
            }
        }
    }
}
void QFreeWork::on_mac_combo_textActivated(const QString& arg1) {
    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = arg1;
        // at->set(DongleCmd::BleScanConnect, macAddress);//发送mac地址
        qDebug() << getIndex() << macAddress;
        bindingMacSn(macAddress, snBinding);
    }
    ui->snbanding->setFocus();
}
void QFreeWork::bindingMacSn(QString bindingMac, QString bindingSn) {
    if (bindingSn == "" || bindingMac == "")
        bindingResult = false;

    // 将网络路径转换为 QFile 能够处理的格式
    QString path;
    if (pack.factory == "xwd")
        path = "\\\\172.30.189.83\\sgpub\\LTC\\MAC\\mac_sn.txt";

    else
        path = "mac_sn.txt";

    // 在 Windows 上，使用 QDir::fromNativeSeparators 将路径中的反斜杠转换为正斜杠
    // path = QDir::fromNativeSeparators(path);
    QFile file(path); // 创建一个文件对象

    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) { //
        QTextStream in(&file);                               // 创建一个文本流对象
        QStringList lines;                                   // 用于存储文件中的每一行数据
        bool found = false;                                  // 标记是否找到了相同的SN
        while (!in.atEnd()) {
            QString line = in.readLine();        // 逐行读取文件
            QStringList parts = line.split(","); // 以逗号分隔每行数据
            if (parts.size() == 2 && parts[0].trimmed() == bindingSn) {
                // 如果找到了相同的SN，替换MAC地址
                lines << (bindingSn + "," + bindingMac);
                found = true;
            } else {
                // 否则，保留原有数据
                lines << line;
            }
        }
        if (!found) {
            // 如果没有找到相同的SN，则追加新的SN和MAC地址
            lines << (bindingSn + "," + bindingMac);
        }
        // 清空文件并写入新的数据
        file.resize(0);
        QTextStream out(&file);
        for (const QString& line : lines) {
            out << line << '\r\n';
        }
        file.close(); // 关闭文件
        showlog("保存mac_sn文件成功");
    } else {
        showlog("保存mac_sn文件失败");
    }

    // bindingMacSnMes(bindingMac, bindingSn);
}

void QFreeWork::bindingMacSnMes(QString bindingMac, QString bindingSn) {
    Q_UNUSED(bindingSn);
    pack.mechines = 1; // 1脱1,1号上位机
    pack.sn = snBinding;
    pack.result = "PASS";
    pack.itemvalue = QString("|BTMAC:%1|").arg(bindingMac);
    pack.instruct_num = "076";
    if (ui->isusemes->checkState()) {
        emit send_end_test_pass(pack);
    }

    if (bindingResult) {
        ui->banding_result->setText("绑定:PASS");
        ui->banding_result->setStyleSheet("font-size: 33px; background-color: #00FF00; color: black; border: 2px solid "
                                          "#00FF00; border-radius: 10px; padding: 10px; text-align: center;");
    } else {
        ui->banding_result->setText("绑定:NG");
        ui->banding_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                          "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
    ui->snbanding->setFocus();
}
void QFreeWork::on_snbanding_returnPressed() {
    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    snBinding = ui->snbanding->text();
    at->set(DongleCmd::BleScanConnect, "00:00:00:00:00:00"); // 发送mac地址
    ui->snbanding->clear();
    bindingResult = true;
}

void QFreeWork::getTestValue(const int mechines, const QString value) {
    if (pack.iskeydata == 2 && mechines == getIndex()) {
        pack.sku = value.trimmed();
        showlog(QStringLiteral("MES GetCustomData 已取到 ROOTSKU=%1").arg(pack.sku));
        return;
    }
    // showlog(value);
    QString mesmacAddress;
    if (pack.factory == "hq") {
        // 定义正则表达式，匹配MAC地址的模式
        QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");

        // 在数据中查找匹配的内容
        QRegularExpressionMatch match = regex.match(value);

        // 检查是否有匹配项
        if (match.hasMatch()) {
            // 提取MAC地址
            mesmacAddress = match.captured(1);
            qDebug() << getIndex() << "MAC地址：" << mesmacAddress;
            if (mechines == getIndex()) {
                ui->macInput->setText(mesmacAddress);
                on_macInput_returnPressed();
            }
        } else {
            showlog("mes未找到匹配的MAC地址");
            showlog(value);
        }
    }
    // showlog(value);
    else if (pack.factory == "lx") {
        mesmacAddress = value;

        // 在2、4、6、8、10的位置插入冒号
        mesmacAddress.insert(2, ":");
        mesmacAddress.insert(5, ":");
        mesmacAddress.insert(8, ":");
        mesmacAddress.insert(11, ":");
        mesmacAddress.insert(14, ":");

        // 将小写字母转换成大写字母
        mesmacAddress = mesmacAddress.toUpper();
        if (mechines == getIndex()) {
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    } else if (pack.factory == "hz") {
        if (mechines != getIndex()) {
            return;
        }
        const QString snFromMes = value.trimmed();
        mesmacAddress = parseMacFromSn(snFromMes);
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            showlog(value);
            return;
        }
        // MES 返回 SN 回填界面，后续写入/校验统一读 ui->getMac
        if (ui && ui->getMac)
            ui->getMac->setText(snFromMes);
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        // on_macInput_returnPressed();
    } else if (pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0) {
        // BYD：过程码换主板 SN 仅用于解析 MAC；界面 SN 框保持过程码，过站 SFC 用 mesProcessCode_
        // （三元组整机 SN 仍由 setWholeMachineSn 回填界面，不在此处理）
        if (mechines != getIndex()) {
            return;
        }
        const QString snFromMes = value.trimmed();
        mesmacAddress = parseMacFromSn(snFromMes);
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            showlog(value);
            return;
        }
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES 主板 SN 已解析 MAC（不回填界面 SN）：%1").arg(mesmacAddress));
        on_macInput_returnPressed();
    } else {
        if (mechines == getIndex()) {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }
}

void QFreeWork::on_connectButton_clicked() {
    openDongleSerialPort();
    if (!dongleSerialPort || !dongleSerialPort->isOpen())
        refreshDongleUartState(0);
}

void QFreeWork::on_disconnectButton_clicked() {
    closeDongleSerialPort();
}

void QFreeWork::on_jigConnectButton_clicked() {
    openJigSerialPort();
    if (!jigSerialPort || !jigSerialPort->isOpen())
        refreshJigUartState(0);
}

void QFreeWork::on_jigDisconnectButton_clicked() {
    closeJigSerialPort();
}

void QFreeWork::on_usbconnectButton_clicked() {
    const QString port = ui->usbcomNameCombo->currentText().trimmed();
    if (port.isEmpty()) {
        showlog(QStringLiteral("请先选择万用表串口"));
        return;
    }
    openUsbSerialPort();
    if (!usbSerialPort || !usbSerialPort->isOpen()) {
        refreshUsbUartState(0);
        showlog(QStringLiteral("万用表串口打开失败：%1 @ %2").arg(port).arg(usbBaudRate));
        return;
    }
    showlog(QStringLiteral("万用表串口已打开：%1 @ %2").arg(port).arg(usbBaudRate));
}

void QFreeWork::on_usbdisconnectButton_clicked() {
    closeUsbSerialPort();
}

void QFreeWork::on_productConnectButton_clicked() {
    openProductSerialPort();
    if (!productSerialPort || !productSerialPort->isOpen())
        refreshProductUartState(0);
}

void QFreeWork::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
}

void QFreeWork::clearProductInstrumentWatch() {
    disconnect(productInstConn_);
    productInstConn_ = QMetaObject::Connection();
    productInstrumentStopWaitStepName_.clear();
}

bool QFreeWork::ensureProductSerialForInstrumentStep(const QString& stepName) {
    if (!product) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("Qproduct未初始化");
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：Qproduct未初始化"));
        return false;
    }
    QComboBox* const prodCombo = getProductcomNameCombo();
    if (!prodCombo || prodCombo->currentText().trimmed().isEmpty()) {
        stepRuntime_.done = true;
        stepRuntime_.pass = false;
        stepRuntime_.testData = QStringLiteral("未选择产品串口COM");
        TestResult = failValue;
        showlog(stepName + QStringLiteral("失败：请先在「产品串口(仪器)」下拉框选择 COM 口"));
        return false;
    }
    // 仪器步骤依赖产品串口：未打开时走与手动点「连接串口」同一槽，避免与界面状态不一致
    if (!productSerialPort || !productSerialPort->isOpen()) {
        showlog(stepName + QStringLiteral("：正在打开产品串口…"));
        on_productConnectButton_clicked();
        if (productSerialPort && productSerialPort->isOpen()) {
            if (product) {
                product->clearProductSerialRxAccum();
            }
            showlog(QStringLiteral("产品串口已打开：%1").arg(prodCombo->currentText()));
        } else {
            stepRuntime_.done = true;
            stepRuntime_.pass = false;
            stepRuntime_.testData = QStringLiteral("产品串口打开失败");
            TestResult = failValue;
            showlog(stepName + QStringLiteral("失败：无法打开产品串口，请检查端口占用或未插入"));
            return false;
        }
    }
    return true;
}

QByteArray QFreeWork::brushInstrumentStartCmdForProfile(int profile) {
    switch (profile) {
    case 1:
        return Qproduct::buildStartReceiveCmd2440Ble1M();
    case 2:
        return Qproduct::buildStartReceiveCmd2480Ble1M();
    case 3:
        return Qproduct::buildStartReceiveCmd2402Ble2M();
    case 4:
        return Qproduct::buildStartReceiveCmd2440Ble2M();
    case 5:
        return Qproduct::buildStartReceiveCmd2480Ble2M();
    case 0:
    default:
        return Qproduct::buildStartReceiveCmd2402Ble1M();
    }
}

void QFreeWork::on_stopTest_clicked() {
    clearProductInstrumentWatch();
    ScreenInspectGigECapture::releaseSession();
    ScreenInspectAnalyzer::cleanupStoredImages(
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("screen_inspect")));
    plcFacade_.disconnect();
    resetVisaBackend();
    isTestContinue = false;
    if (auto* box = qobject_cast<QFreeWorkBox*>(window())) {
        box->releaseSharedAsd9026aIfIdle();
        box->releaseSharedTempLoggerIfIdle();
    }
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    mesProcessCode_.clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

void QFreeWork::resetPlcKeyCapSyncReadState() {
    plcKeyCapSyncReadPending_ = false;
    plcKeyCapSyncReadOk_ = false;
    plcKeyCapSyncReadValue_ = 0;
    plcKeyCapSyncReadAuxId_ = -1;
}

bool QFreeWork::pollKeyCapDuringPress(QString* errOut, QString* outSummary) {
    const int kk = currentKeyCapRequestKk_;
    const int configuredKeyId = currentKeyConfiguredId_;
    const int readCount = qMax(1, SETTINGS.value(QStringLiteral("KeyCap/ReadCount"), 3).toInt());
    const int intervalMs = qMax(0, SETTINGS.value(QStringLiteral("KeyCap/ReadIntervalMs"), 80).toInt());
    const int singleTimeoutMs = qMax(500, SETTINGS.value(QStringLiteral("KeyCap/SingleReadTimeoutMs"), 2000).toInt());

    quint32 bestCap = 0;
    QStringList sampleTexts;

    for (int i = 0; i < readCount; ++i) {
        resetPlcKeyCapSyncReadState();
        plcKeyCapSyncReadPending_ = true;
        QVariantMap m;
        m[QStringLiteral("key")] = kk;
        protocolManager.get(DeviceCmd::KeySignalRead, m);

        QElapsedTimer timer;
        timer.start();
        while (plcKeyCapSyncReadPending_ && isTestContinue && timer.elapsed() < singleTimeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
            QThread::msleep(20);
        }

        if (!isTestContinue) {
            if (errOut)
                *errOut = QStringLiteral("测试中止");
            return false;
        }
        if (plcKeyCapSyncReadPending_) {
            if (errOut) {
                *errOut = QStringLiteral("第%1/%2次读电容超时(%3ms)")
                              .arg(i + 1)
                              .arg(readCount)
                              .arg(singleTimeoutMs);
            }
            return false;
        }
        if (!plcKeyCapSyncReadOk_) {
            if (errOut) {
                *errOut = QStringLiteral("第%1/%2次读电容应答无效").arg(i + 1).arg(readCount);
            }
            return false;
        }
        if (plcKeyCapSyncReadAuxId_ != kk) {
            if (errOut) {
                *errOut = QStringLiteral("按键编号不一致：请求KK=%1 应答KK=%2")
                              .arg(kk)
                              .arg(plcKeyCapSyncReadAuxId_);
            }
            return false;
        }

        bestCap = qMax(bestCap, plcKeyCapSyncReadValue_);
        sampleTexts.append(QString::number(plcKeyCapSyncReadValue_));
        showlog(currentKeyTestName_ + QStringLiteral("：第%1/%2次读电容 KK=%3 值=%4").arg(i + 1).arg(readCount).arg(kk).arg(plcKeyCapSyncReadValue_));

        if (i + 1 < readCount && intervalMs > 0) {
            QThread::msleep(static_cast<unsigned long>(intervalMs));
        }
    }

    const QString expectedKeyId = QString::number(configuredKeyId);
    const bool idOk = compareVersions(expectedKeyId, QString::number(kk + 1));
    const bool capOk = (bestCap >= lowKeyCap_) && (bestCap <= highKeyCap_);
    const QString capAsk = QStringLiteral("[%1,%2]").arg(lowKeyCap_).arg(highKeyCap_);

    stepRuntime_.ask = capAsk;
    const QString summary = QStringLiteral("KK:%1 采样[%2] 最大:%3 ID:%4 期望ID:%5")
                                .arg(kk)
                                .arg(sampleTexts.join(QLatin1Char(',')))
                                .arg(bestCap)
                                .arg(kk + 1)
                                .arg(expectedKeyId);
    if (outSummary) {
        *outSummary = summary;
    }
    stepRuntime_.testData = summary;

    if (!idOk) {
        if (errOut) {
            *errOut = QStringLiteral("按键ID与配置不符，KK=%1 期望ID=%2").arg(kk).arg(expectedKeyId);
        }
        TestResult = failValue;
        showlog(currentKeyTestName_ + QStringLiteral("失败：") + *errOut);
        return false;
    }
    if (!capOk) {
        if (errOut) {
            *errOut = QStringLiteral("电容卡控失败，最大=%1 允许%2").arg(bestCap).arg(capAsk);
        }
        TestResult = failValue;
        showlog(currentKeyTestName_ + QStringLiteral("卡控失败，采样[%1] 最大=%2 允许%3").arg(sampleTexts.join(QLatin1Char(','))).arg(bestCap).arg(capAsk));
        return false;
    }

    showlog(currentKeyTestName_ + QStringLiteral("卡控通过，KK=%1 最大电容=%2").arg(kk).arg(bestCap));
    return true;
}
