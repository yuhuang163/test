#include "test_case_edit_dialog.h"
#include "ui_test_case_edit_dialog.h"

#include "test_case.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

#include <algorithm>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

int comboIndexByData(const QComboBox* box, const QString& data) {
    for (int i = 0; i < box->count(); ++i) {
        if (box->itemData(i).toString() == data)
            return i;
    }
    return -1;
}

QString comboData(const QComboBox* box) {
    return box->currentData().toString();
}

void fillActionCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("下发给设备"), QStringLiteral("Set"));
    box->addItem(QStringLiteral("从设备读取"), QStringLiteral("Get"));
}

void fillDeviceCmdCombo(QComboBox* box) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    items.reserve(DeviceCmdCatalog::allDeviceCmdNames().size());
    for (const QString& name : DeviceCmdCatalog::allDeviceCmdNames())
        items.append({DeviceCmdCatalog::deviceCmdUiLabel(name), name});
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    for (const auto& item : items)
        box->addItem(item.first, item.second);
}

void fillGateReportTypeCombo(QComboBox* box) {
    box->clear();
    for (const GateTypeDescriptor& t : GateRegistry::allTypeDescriptors())
        box->addItem(t.displayName, t.reportType);
}

void fillGateFieldCombo(QComboBox* box, const QString& reportType) {
    box->clear();
    GateTypeDescriptor desc;
    if (!GateRegistry::descriptorFor(reportType, desc))
        return;
    for (const GateFieldDescriptor& f : desc.fields)
        box->addItem(f.displayName, f.field);
}

void fillGateOpCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("在范围内"), QStringLiteral("range"));
    box->addItem(QStringLiteral("大于"), QStringLiteral("gt"));
    box->addItem(QStringLiteral("小于"), QStringLiteral("lt"));
    box->addItem(QStringLiteral("等于"), QStringLiteral("eq"));
}

QString hookUiLabel(const QString& hookId) {
    if (hookId == QLatin1String("NoOp"))
        return QStringLiteral("空操作（示例）");
    if (hookId == QLatin1String("FreeWorkNoOpDemo"))
        return QStringLiteral("示例步骤");
    return QStringLiteral("未登记流程");
}

void fillHookCombo(QComboBox* box) {
    box->clear();
    for (const QString& id : TestCaseHookRegistry::hookIds())
        box->addItem(hookUiLabel(id), id);
}

}  // namespace

TestCaseEditDialog::TestCaseEditDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TestCaseEditDialog) {
    ui->setupUi(this);

    fillActionCombo(ui->comboBox_action);
    fillDeviceCmdCombo(ui->comboBox_deviceCmd);
    fillGateReportTypeCombo(ui->comboBox_gateReportType);
    fillGateOpCombo(ui->comboBox_gateOp);
    registerFreeWorkTestCaseHooks();
    registerQFreeWorkCatalogTestCaseHooks();
    fillHookCombo(ui->comboBox_hookId);

    connect(ui->comboBox_deviceCmd, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onDeviceCmdChanged);
    connect(ui->comboBox_gateReportType, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onGateReportTypeChanged);
    connect(ui->checkBox_gateEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updateGateFieldsEnabled);
    connect(ui->checkBox_promptEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updatePromptFieldsEnabled);
    connect(ui->checkBox_hookEnabled, &QCheckBox::toggled, this, &TestCaseEditDialog::updateHookFieldsEnabled);
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        if (saveValidated())
            accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    updateGateFieldsEnabled();
    updatePromptFieldsEnabled();
    updateHookFieldsEnabled();
}

TestCaseEditDialog::~TestCaseEditDialog() {
    delete ui;
}

void TestCaseEditDialog::updateGateFieldsEnabled() {
    const bool on = ui->checkBox_gateEnabled->isChecked();
    ui->label_gateReportType->setEnabled(on);
    ui->comboBox_gateReportType->setEnabled(on);
    ui->label_gateField->setEnabled(on);
    ui->comboBox_gateField->setEnabled(on);
    ui->label_gateOp->setEnabled(on);
    ui->comboBox_gateOp->setEnabled(on);
    ui->label_gateLow->setEnabled(on);
    ui->lineEdit_gateLow->setEnabled(on);
    ui->label_gateHigh->setEnabled(on);
    ui->lineEdit_gateHigh->setEnabled(on);
    ui->label_gateExpected->setEnabled(on);
    ui->lineEdit_gateExpected->setEnabled(on);
}

void TestCaseEditDialog::updatePromptFieldsEnabled() {
    const bool on = ui->checkBox_promptEnabled->isChecked();
    ui->label_promptText->setEnabled(on);
    ui->plainTextEdit_promptText->setEnabled(on);
}

void TestCaseEditDialog::updateHookFieldsEnabled() {
    const bool on = ui->checkBox_hookEnabled->isChecked();
    ui->label_hookId->setEnabled(on);
    ui->comboBox_hookId->setEnabled(on);
}

void TestCaseEditDialog::onGateReportTypeChanged(int) {
    fillGateFieldCombo(ui->comboBox_gateField, comboData(ui->comboBox_gateReportType));
}

void TestCaseEditDialog::setDefinition(const TestCaseDefinition& def, const QString& storageKey) {
    originalCaseName_ = storageKey.trimmed().isEmpty() ? def.meta.name.trimmed() : storageKey.trimmed();
    ui->lineEdit_caseName->setText(def.meta.name);
    ui->lineEdit_mesTag->setText(def.meta.mesTag);
    ui->checkBox_promptEnabled->setChecked(def.meta.promptEnabled);
    ui->plainTextEdit_promptText->setPlainText(def.meta.promptText);

    const int actionIdx = comboIndexByData(ui->comboBox_action,
                                           def.send.action == TestCaseSendAction::Get ? QStringLiteral("Get")
                                                                                      : QStringLiteral("Set"));
    if (actionIdx >= 0)
        ui->comboBox_action->setCurrentIndex(actionIdx);

    const int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, def.send.deviceCmd);
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    onDeviceCmdChanged(cmdIdx);

    DeviceCmd cmd;
    if (DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
        DeviceCmdParamSchema schema;
        if (DeviceCmdCatalog::paramSchemaFor(cmd, schema)) {
            if (schema.kind == DeviceCmdParamKind::Int || schema.kind == DeviceCmdParamKind::UInt)
                ui->spinBox_intParam->setValue(def.send.param.toInt());
            else if (schema.kind == DeviceCmdParamKind::JsonMap)
                ui->plainTextEdit_jsonParam->setPlainText(
                    QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(def.send.param.toMap())).toJson()));
        }
    }

    ui->spinBox_delayBefore->setValue(def.timing.delayBeforeMs);
    ui->spinBox_delayAfter->setValue(def.timing.delayAfterMs);

    ui->checkBox_gateEnabled->setChecked(def.gate.enabled);
    const int typeIdx = comboIndexByData(ui->comboBox_gateReportType, def.gate.reportType);
    if (typeIdx >= 0)
        ui->comboBox_gateReportType->setCurrentIndex(typeIdx);
    onGateReportTypeChanged(0);
    const int fieldIdx = comboIndexByData(ui->comboBox_gateField, def.gate.field);
    if (fieldIdx >= 0)
        ui->comboBox_gateField->setCurrentIndex(fieldIdx);

    const QString opKey = def.gate.op == TestCaseGateOp::Gt   ? QStringLiteral("gt")
                          : def.gate.op == TestCaseGateOp::Lt ? QStringLiteral("lt")
                          : def.gate.op == TestCaseGateOp::Eq ? QStringLiteral("eq")
                                                             : QStringLiteral("range");
    const int opIdx = comboIndexByData(ui->comboBox_gateOp, opKey);
    if (opIdx >= 0)
        ui->comboBox_gateOp->setCurrentIndex(opIdx);

    ui->lineEdit_gateLow->setText(QString::number(def.gate.low));
    ui->lineEdit_gateHigh->setText(QString::number(def.gate.high));
    ui->lineEdit_gateExpected->setText(def.gate.expected);
    ui->checkBox_hookEnabled->setChecked(def.hook.enabled);
    const int hookIdx = comboIndexByData(ui->comboBox_hookId, def.hook.hookId);
    if (hookIdx >= 0)
        ui->comboBox_hookId->setCurrentIndex(hookIdx);

    updateGateFieldsEnabled();
    updatePromptFieldsEnabled();
    updateHookFieldsEnabled();
}

TestCaseDefinition TestCaseEditDialog::definition() const {
    TestCaseDefinition def;
    def.meta.name = ui->lineEdit_caseName->text().trimmed();
    def.meta.displayName = def.meta.name;
    def.meta.mesTag = ui->lineEdit_mesTag->text().trimmed();
    def.meta.promptEnabled = ui->checkBox_promptEnabled->isChecked();
    def.meta.promptText = ui->plainTextEdit_promptText->toPlainText();

    def.send.action = comboData(ui->comboBox_action) == QLatin1String("Get") ? TestCaseSendAction::Get
                                                                             : TestCaseSendAction::Set;
    def.send.deviceCmd = comboData(ui->comboBox_deviceCmd);
    DeviceCmd cmd;
    if (DeviceCmdCatalog::deviceCmdFromName(def.send.deviceCmd, cmd)) {
        DeviceCmdParamSchema schema;
        if (DeviceCmdCatalog::paramSchemaFor(cmd, schema)) {
            if (schema.kind == DeviceCmdParamKind::Int || schema.kind == DeviceCmdParamKind::UInt)
                def.send.param = ui->spinBox_intParam->value();
            else if (schema.kind == DeviceCmdParamKind::JsonMap) {
                QVariantMap map;
                const QString text = ui->plainTextEdit_jsonParam->toPlainText().trimmed();
                const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8());
                if (doc.isObject())
                    map = doc.object().toVariantMap();
                else {
                    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                        const int eq = line.indexOf(QLatin1Char('='));
                        if (eq > 0)
                            map.insert(line.left(eq).trimmed(), line.mid(eq + 1).trimmed());
                    }
                }
                def.send.param = map;
            }
        }
    }

    def.timing.delayBeforeMs = ui->spinBox_delayBefore->value();
    def.timing.delayAfterMs = ui->spinBox_delayAfter->value();
    def.gate.enabled = ui->checkBox_gateEnabled->isChecked();
    def.gate.reportType = comboData(ui->comboBox_gateReportType);
    def.gate.field = comboData(ui->comboBox_gateField);
    const QString op = comboData(ui->comboBox_gateOp);
    def.gate.op = op == QLatin1String("gt")   ? TestCaseGateOp::Gt
                  : op == QLatin1String("lt") ? TestCaseGateOp::Lt
                  : op == QLatin1String("eq") ? TestCaseGateOp::Eq
                                              : TestCaseGateOp::Range;
    def.gate.low = ui->lineEdit_gateLow->text().toDouble();
    def.gate.high = ui->lineEdit_gateHigh->text().toDouble();
    def.gate.expected = ui->lineEdit_gateExpected->text();
    def.hook.enabled = ui->checkBox_hookEnabled->isChecked();
    def.hook.hookId = comboData(ui->comboBox_hookId);
    return def;
}

void TestCaseEditDialog::onDeviceCmdChanged(int) {
    const QString cmdName = comboData(ui->comboBox_deviceCmd);
    DeviceCmd cmd;
    if (!DeviceCmdCatalog::deviceCmdFromName(cmdName, cmd)) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        return;
    }
    DeviceCmdParamSchema schema;
    if (!DeviceCmdCatalog::paramSchemaFor(cmd, schema)) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        return;
    }
    if (schema.kind == DeviceCmdParamKind::Int || schema.kind == DeviceCmdParamKind::UInt)
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramInt);
    else if (schema.kind == DeviceCmdParamKind::JsonMap)
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramJson);
    else
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
}

bool TestCaseEditDialog::saveValidated() {
    const TestCaseDefinition def = definition();
    QStringList errors;
    if (!TestCaseValidator::validateCase(def, errors)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), errors.join(QLatin1Char('\n')));
        return false;
    }
    if (!TestCaseStore::saveCase(def)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入配置文件"));
        return false;
    }
    if (!originalCaseName_.isEmpty() && originalCaseName_ != def.meta.name) {
        const QString oldPath = TestCasePaths::caseIniPath(originalCaseName_);
        if (QFile::exists(oldPath))
            QFile::remove(oldPath);
    }
    return true;
}
