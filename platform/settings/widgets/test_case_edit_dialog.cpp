#include "test_case_edit_dialog.h"
#include "ui_test_case_edit_dialog.h"

#include "test_case.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QPushButton>

#include <QHash>

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
    box->addItem(QStringLiteral("设置"), QStringLiteral("Set"));
    box->addItem(QStringLiteral("读取"), QStringLiteral("Get"));
}

void fillSendChannelCombo(QComboBox* box) {
    box->clear();
    box->addItem(QStringLiteral("产品通信"), QStringLiteral("Product"));
    box->addItem(QStringLiteral("Dongle通信"), QStringLiteral("Dongle"));
    box->addItem(QStringLiteral("云端交互"), QStringLiteral("Cloud"));
}

TestCaseSendAction sendActionFromComboData(const QString& data) {
    return data.compare(QStringLiteral("Get"), Qt::CaseInsensitive) == 0 ? TestCaseSendAction::Get
                                                                         : TestCaseSendAction::Set;
}

void fillDeviceCmdCombo(QComboBox* box, TestCaseSendChannel channel, TestCaseSendAction action) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    if (channel == TestCaseSendChannel::Dongle) {
        items.reserve(DongleCmdCatalog::allDongleCmdNames(action).size());
        for (const QString& name : DongleCmdCatalog::allDongleCmdNames(action))
            items.append({DongleCmdCatalog::dongleCmdUiLabel(name), name});
    } else if (channel == TestCaseSendChannel::Cloud) {
        items.reserve(TupleCmdCatalog::allTupleCmdNames(action).size());
        for (const QString& name : TupleCmdCatalog::allTupleCmdNames(action))
            items.append({TupleCmdCatalog::tupleCmdUiLabel(name), name});
    } else {
        items.reserve(DeviceCmdCatalog::allDeviceCmdNames(action).size());
        for (const QString& name : DeviceCmdCatalog::allDeviceCmdNames(action))
            items.append({DeviceCmdCatalog::deviceCmdUiLabel(name), name});
    }
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    for (const auto& item : items)
        box->addItem(item.first, item.second);
}

TestCaseSendChannel sendChannelFromComboData(const QString& data) {
    if (data.compare(QStringLiteral("Dongle"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Dongle;
    if (data.compare(QStringLiteral("Cloud"), Qt::CaseInsensitive) == 0)
        return TestCaseSendChannel::Cloud;
    return TestCaseSendChannel::Product;
}

QString sendChannelComboData(TestCaseSendChannel channel) {
    if (channel == TestCaseSendChannel::Dongle)
        return QStringLiteral("Dongle");
    if (channel == TestCaseSendChannel::Cloud)
        return QStringLiteral("Cloud");
    return QStringLiteral("Product");
}

enum class SendCmdParamKind { None, Int, UInt, JsonMap, String };

struct SendCmdParamUi {
    bool valid = false;
    SendCmdParamKind kind = SendCmdParamKind::None;
};

SendCmdParamUi sendCmdParamUiForName(const QString& name, TestCaseSendChannel channel) {
    SendCmdParamUi out;
    if (channel == TestCaseSendChannel::Dongle) {
        DongleCmd dongleCmd;
        if (DongleCmdCatalog::dongleCmdFromName(name, dongleCmd)) {
            DeviceCmdParamSchema schema;
            if (DongleCmdCatalog::paramSchemaFor(dongleCmd, schema)) {
                out.valid = true;
                if (schema.kind == DeviceCmdParamKind::None)
                    out.kind = SendCmdParamKind::None;
                else if (schema.kind == DeviceCmdParamKind::Int)
                    out.kind = SendCmdParamKind::Int;
                else if (schema.kind == DeviceCmdParamKind::String)
                    out.kind = SendCmdParamKind::String;
                else
                    out.kind = SendCmdParamKind::JsonMap;
            }
        }
        return out;
    }
    if (channel == TestCaseSendChannel::Cloud) {
        TupleCmd tupleCmd;
        if (TupleCmdCatalog::tupleCmdFromName(name, tupleCmd)) {
            DeviceCmdParamSchema schema;
            if (TupleCmdCatalog::paramSchemaFor(tupleCmd, schema)) {
                out.valid = true;
                if (schema.kind == DeviceCmdParamKind::None)
                    out.kind = SendCmdParamKind::None;
                else if (schema.kind == DeviceCmdParamKind::Int)
                    out.kind = SendCmdParamKind::Int;
                else if (schema.kind == DeviceCmdParamKind::String)
                    out.kind = SendCmdParamKind::String;
                else
                    out.kind = SendCmdParamKind::JsonMap;
            }
        }
        return out;
    }
    DeviceCmd cmd;
    if (DeviceCmdCatalog::deviceCmdFromName(name, cmd)) {
        DeviceCmdParamSchema schema;
        if (DeviceCmdCatalog::paramSchemaFor(cmd, schema)) {
            out.valid = true;
            if (schema.kind == DeviceCmdParamKind::None)
                out.kind = SendCmdParamKind::None;
            else if (schema.kind == DeviceCmdParamKind::Int)
                out.kind = SendCmdParamKind::Int;
            else if (schema.kind == DeviceCmdParamKind::UInt)
                out.kind = SendCmdParamKind::UInt;
            else if (schema.kind == DeviceCmdParamKind::String)
                out.kind = SendCmdParamKind::String;
            else
                out.kind = SendCmdParamKind::JsonMap;
        }
    }
    return out;
}

void applySendParamToUi(const SendCmdParamUi& uiSchema, const QVariant& param, QWidget* pageNone,
                        QWidget* pageInt, QWidget* pageJson, QStackedWidget* stack, QSpinBox* spinBox,
                        QPlainTextEdit* jsonEdit) {
    if (!uiSchema.valid || uiSchema.kind == SendCmdParamKind::None) {
        stack->setCurrentWidget(pageNone);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::Int || uiSchema.kind == SendCmdParamKind::UInt) {
        stack->setCurrentWidget(pageInt);
        spinBox->setValue(param.toInt());
        return;
    }
    stack->setCurrentWidget(pageJson);
    if (uiSchema.kind == SendCmdParamKind::String)
        jsonEdit->setPlainText(param.toString());
    else
        jsonEdit->setPlainText(
            QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(param.toMap())).toJson()));
}

QVariant readSendParamFromUi(const SendCmdParamUi& uiSchema, QSpinBox* spinBox, QPlainTextEdit* jsonEdit) {
    switch (uiSchema.kind) {
    case SendCmdParamKind::Int:
    case SendCmdParamKind::UInt:
        return spinBox->value();
    case SendCmdParamKind::String:
        return jsonEdit->toPlainText().trimmed();
    case SendCmdParamKind::JsonMap: {
        QVariantMap map;
        const QString text = jsonEdit->toPlainText().trimmed();
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
        return map;
    }
    default:
        return {};
    }
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
    box->addItem(QStringLiteral("版本比对"), QStringLiteral("compareVersions"));
}

const QHash<QString, QString>& hookDisplayNameMap() {
    static const QHash<QString, QString> map = {
        {QStringLiteral("NoOp"), QStringLiteral("空操作（示例）")},
        {QStringLiteral("FreeWorkNoOpDemo"), QStringLiteral("示例步骤")},
        {QStringLiteral("JIG_CURRENT_READ"), QStringLiteral("读取治具电流测量值")},
        {QStringLiteral("WRITE_PRODUCT_KEY"), QStringLiteral("写入 productKey")},
        {QStringLiteral("WRITE_DEVICE_NAME"), QStringLiteral("写入 deviceName")},
        {QStringLiteral("WRITE_DEVICE_SECRET"), QStringLiteral("写入 deviceSecret")},
        {QStringLiteral("SN_WRITE_TAIL"), QStringLiteral("写入 SN 码")},
        {QStringLiteral("PLC_MODBUS_CONN"), QStringLiteral("PLC Modbus 连接")},
        {QStringLiteral("PLC_V3_SWITCH_RIGHT_WHOLE"), QStringLiteral("PLC+V3 旋钮整步右旋")},
        {QStringLiteral("PLC_V3_SWITCH_DONE_RESET_M"), QStringLiteral("PLC+V3 旋钮测试完成 M 复位")},
        {QStringLiteral("KEY_POWER"), QStringLiteral("电源键测试")},
        {QStringLiteral("KEY_START_PAUSE"), QStringLiteral("开始/暂停键测试")},
        {QStringLiteral("KEY_MODE"), QStringLiteral("模式键测试")},
        {QStringLiteral("KEY_SPEED"), QStringLiteral("速度键测试")},
        {QStringLiteral("KEY_PROGRAM"), QStringLiteral("程序键测试")},
        {QStringLiteral("KEY_LEFT"), QStringLiteral("左键测试")},
        {QStringLiteral("KEY_RIGHT"), QStringLiteral("右键测试")},
        {QStringLiteral("KEY_ROT_LEFT"), QStringLiteral("左旋键测试")},
        {QStringLiteral("KEY_ROT_RIGHT"), QStringLiteral("右旋键测试")},
        {QStringLiteral("PLC_V3_KEY_MODE"), QStringLiteral("PLC+V3 模式键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_PROGRAM"), QStringLiteral("PLC+V3 程序键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_SPEED"), QStringLiteral("PLC+V3 速度键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_RIGHT"), QStringLiteral("PLC+V3 右键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_START_PAUSE"), QStringLiteral("PLC+V3 开始暂停键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_LEFT"), QStringLiteral("PLC+V3 左键触摸整步")},
        {QStringLiteral("PLC_V3_KEY_POWER"), QStringLiteral("PLC+V3 电源键触摸整步")},
        {QStringLiteral("PROD_INST_RESET_ACK_1"), QStringLiteral("产品串口仪器复位应答 1")},
        {QStringLiteral("PROD_INST_RESET_ACK_2"), QStringLiteral("产品串口仪器复位应答 2")},
        {QStringLiteral("PROD_INST_RESET_ACK_3"), QStringLiteral("产品串口仪器复位应答 3")},
        {QStringLiteral("PROD_INST_RESET_ACK_4"), QStringLiteral("产品串口仪器复位应答 4")},
        {QStringLiteral("PROD_INST_RESET_ACK_5"), QStringLiteral("产品串口仪器复位应答 5")},
        {QStringLiteral("PROD_INST_RESET_ACK_6"), QStringLiteral("产品串口仪器复位应答 6")},
        {QStringLiteral("PROD_INST_START_RX_2402_1M"), QStringLiteral("产品串口开始接收 2402 BLE1M")},
        {QStringLiteral("PROD_INST_START_RX_2440_1M"), QStringLiteral("产品串口开始接收 2440 BLE1M")},
        {QStringLiteral("PROD_INST_START_RX_2480_1M"), QStringLiteral("产品串口开始接收 2480 BLE1M")},
        {QStringLiteral("PROD_INST_START_RX_2402_2M"), QStringLiteral("产品串口开始接收 2402 BLE2M")},
        {QStringLiteral("PROD_INST_START_RX_2440_2M"), QStringLiteral("产品串口开始接收 2440 BLE2M")},
        {QStringLiteral("PROD_INST_START_RX_2480_2M"), QStringLiteral("产品串口开始接收 2480 BLE2M")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_P0"), QStringLiteral("并联 CMW 播放 Profile0")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_P1"), QStringLiteral("并联 CMW 播放 Profile1")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_P2"), QStringLiteral("并联 CMW 播放 Profile2")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_P3"), QStringLiteral("并联 CMW 播放 Profile3")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_P4"), QStringLiteral("并联 CMW 播放 Profile4")},
        {QStringLiteral("FREE_INSTR_CMW_GPRF_P5"), QStringLiteral("并联 CMW 播放 Profile5")},
        {QStringLiteral("PROD_INST_STOP_RX_PER_1"), QStringLiteral("产品串口停止接收与 PER1")},
        {QStringLiteral("PROD_INST_STOP_RX_PER_2"), QStringLiteral("产品串口停止接收与 PER2")},
        {QStringLiteral("PROD_INST_STOP_RX_PER_3"), QStringLiteral("产品串口停止接收与 PER3")},
        {QStringLiteral("PROD_INST_STOP_RX_PER_4"), QStringLiteral("产品串口停止接收与 PER4")},
        {QStringLiteral("PROD_INST_STOP_RX_PER_5"), QStringLiteral("产品串口停止接收与 PER5")},
        {QStringLiteral("PROD_INST_STOP_RX_PER_6"), QStringLiteral("产品串口停止接收与 PER6")},
    };
    return map;
}

QString hookUiLabel(const QString& hookId) {
    const QString key = hookId.trimmed();
    const auto it = hookDisplayNameMap().constFind(key);
    if (it != hookDisplayNameMap().cend())
        return it.value();
    if (key.isEmpty())
        return QString();
    // 已注册但未维护中文名时显示 HookId，避免与「未实现」混淆
    if (TestCaseHookRegistry::contains(key))
        return QStringLiteral("%1（待补全显示名）").arg(key);
    return QStringLiteral("未登记流程：%1").arg(key);
}

void fillHookCombo(QComboBox* box) {
    box->clear();
    QVector<QPair<QString, QString>> items;
    for (const QString& id : TestCaseHookRegistry::hookIds())
        items.append({hookUiLabel(id), id});
    std::sort(items.begin(), items.end(), [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
        return a.first.localeAwareCompare(b.first) < 0;
    });
    for (const auto& item : items)
        box->addItem(item.first, item.second);
}

}  // namespace

TestCaseEditDialog::TestCaseEditDialog(QWidget* parent) : QDialog(parent), ui(new Ui::TestCaseEditDialog) {
    ui->setupUi(this);

    fillSendChannelCombo(ui->comboBox_sendChannel);
    fillActionCombo(ui->comboBox_action);
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, TestCaseSendChannel::Product, TestCaseSendAction::Set);
    fillGateReportTypeCombo(ui->comboBox_gateReportType);
    fillGateOpCombo(ui->comboBox_gateOp);
    registerFreeWorkTestCaseHooks();
    registerQFreeWorkCatalogTestCaseHooks();
    fillHookCombo(ui->comboBox_hookId);

    if (QPushButton* saveBtn = ui->buttonBox->button(QDialogButtonBox::Save)) {
        saveBtn->setText(QStringLiteral("保存"));
    }
    if (QPushButton* cancelBtn = ui->buttonBox->button(QDialogButtonBox::Cancel)) {
        cancelBtn->setText(QStringLiteral("取消"));
    }

    connect(ui->comboBox_sendChannel, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onSendChannelChanged);
    connect(ui->comboBox_action, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &TestCaseEditDialog::onSendActionChanged);
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
    onDeviceCmdChanged(ui->comboBox_deviceCmd->currentIndex());
}

TestCaseEditDialog::~TestCaseEditDialog() {
    delete ui;
}

void TestCaseEditDialog::updateGateFieldsEnabled() {
    const bool on = ui->checkBox_gateEnabled->isChecked();
    ui->label_gateReportType->setVisible(on);
    ui->comboBox_gateReportType->setVisible(on);
    ui->label_gateField->setVisible(on);
    ui->comboBox_gateField->setVisible(on);
    ui->label_gateOp->setVisible(on);
    ui->comboBox_gateOp->setVisible(on);
    ui->label_gateLow->setVisible(on);
    ui->lineEdit_gateLow->setVisible(on);
    ui->label_gateHigh->setVisible(on);
    ui->lineEdit_gateHigh->setVisible(on);
    ui->label_gateExpected->setVisible(on);
    ui->lineEdit_gateExpected->setVisible(on);
}

void TestCaseEditDialog::updatePromptFieldsEnabled() {
    const bool on = ui->checkBox_promptEnabled->isChecked();
    ui->label_promptText->setVisible(on);
    ui->plainTextEdit_promptText->setVisible(on);
}

void TestCaseEditDialog::updateHookFieldsEnabled() {
    const bool on = ui->checkBox_hookEnabled->isChecked();
    ui->label_hookId->setVisible(on);
    ui->comboBox_hookId->setVisible(on);
    ui->groupBox_send->setVisible(!on);
}

void TestCaseEditDialog::updateSendParamVisibility(bool hasParam) {
    ui->label_param->setVisible(hasParam);
    ui->stackedWidget_param->setVisible(hasParam);
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

    const TestCaseSendAction action = def.send.action;
    ui->comboBox_action->blockSignals(true);
    const int actionIdx = comboIndexByData(ui->comboBox_action,
                                           action == TestCaseSendAction::Get ? QStringLiteral("Get")
                                                                             : QStringLiteral("Set"));
    if (actionIdx >= 0)
        ui->comboBox_action->setCurrentIndex(actionIdx);
    ui->comboBox_action->blockSignals(false);

    const TestCaseSendChannel channel = def.send.channel;
    ui->comboBox_sendChannel->blockSignals(true);
    ui->comboBox_deviceCmd->blockSignals(true);
    const int channelIdx = comboIndexByData(ui->comboBox_sendChannel, sendChannelComboData(channel));
    if (channelIdx >= 0)
        ui->comboBox_sendChannel->setCurrentIndex(channelIdx);
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, channel, action);
    const int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, def.send.deviceCmd);
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    ui->comboBox_sendChannel->blockSignals(false);
    ui->comboBox_deviceCmd->blockSignals(false);
    onDeviceCmdChanged(cmdIdx);

    const SendCmdParamUi uiSchema = sendCmdParamUiForName(def.send.deviceCmd, channel);
    applySendParamToUi(uiSchema, def.send.param, ui->page_paramNone, ui->page_paramInt, ui->page_paramJson,
                       ui->stackedWidget_param, ui->spinBox_intParam, ui->plainTextEdit_jsonParam);

    ui->spinBox_delayBefore->setValue(def.timing.delayBeforeMs);
    ui->spinBox_delayAfter->setValue(def.timing.delayAfterMs);
    {
        int timeoutMs = def.timing.commandTimeoutMs;
        if (timeoutMs <= 0)
            timeoutMs = def.gate.enabled ? 8000 : 300;
        ui->spinBox_commandTimeout->setValue(timeoutMs);
    }

    ui->checkBox_gateEnabled->setChecked(def.gate.enabled);
    const int typeIdx = comboIndexByData(ui->comboBox_gateReportType, def.gate.reportType);
    if (typeIdx >= 0)
        ui->comboBox_gateReportType->setCurrentIndex(typeIdx);
    onGateReportTypeChanged(0);
    const int fieldIdx = comboIndexByData(ui->comboBox_gateField, def.gate.field);
    if (fieldIdx >= 0)
        ui->comboBox_gateField->setCurrentIndex(fieldIdx);

    const QString opKey = def.gate.op == TestCaseGateOp::Gt              ? QStringLiteral("gt")
                          : def.gate.op == TestCaseGateOp::Lt            ? QStringLiteral("lt")
                          : def.gate.op == TestCaseGateOp::Eq            ? QStringLiteral("eq")
                          : def.gate.op == TestCaseGateOp::CompareVersions ? QStringLiteral("compareVersions")
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
    def.send.channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    def.send.deviceCmd = comboData(ui->comboBox_deviceCmd);
    const SendCmdParamUi uiSchema = sendCmdParamUiForName(def.send.deviceCmd, def.send.channel);
    def.send.param = readSendParamFromUi(uiSchema, ui->spinBox_intParam, ui->plainTextEdit_jsonParam);

    def.timing.delayBeforeMs = ui->spinBox_delayBefore->value();
    def.timing.delayAfterMs = ui->spinBox_delayAfter->value();
    def.timing.commandTimeoutMs = ui->spinBox_commandTimeout->value();
    def.gate.enabled = ui->checkBox_gateEnabled->isChecked();
    def.gate.reportType = comboData(ui->comboBox_gateReportType);
    def.gate.field = comboData(ui->comboBox_gateField);
    const QString op = comboData(ui->comboBox_gateOp);
    def.gate.op = op == QLatin1String("gt")              ? TestCaseGateOp::Gt
                  : op == QLatin1String("lt")            ? TestCaseGateOp::Lt
                  : op == QLatin1String("eq")            ? TestCaseGateOp::Eq
                  : op == QLatin1String("compareVersions") ? TestCaseGateOp::CompareVersions
                                                           : TestCaseGateOp::Range;
    def.gate.low = ui->lineEdit_gateLow->text().toDouble();
    def.gate.high = ui->lineEdit_gateHigh->text().toDouble();
    def.gate.expected = ui->lineEdit_gateExpected->text();
    def.hook.enabled = ui->checkBox_hookEnabled->isChecked();
    def.hook.hookId = comboData(ui->comboBox_hookId);
    return def;
}

void TestCaseEditDialog::onSendChannelChanged(int) {
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::onSendActionChanged(int) {
    refreshDeviceCmdCombo();
}

void TestCaseEditDialog::refreshDeviceCmdCombo() {
    const QString previousCmd = comboData(ui->comboBox_deviceCmd);
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const TestCaseSendAction action = sendActionFromComboData(comboData(ui->comboBox_action));
    fillDeviceCmdCombo(ui->comboBox_deviceCmd, channel, action);
    int cmdIdx = comboIndexByData(ui->comboBox_deviceCmd, previousCmd);
    if (cmdIdx < 0 && ui->comboBox_deviceCmd->count() > 0)
        cmdIdx = 0;
    if (cmdIdx >= 0)
        ui->comboBox_deviceCmd->setCurrentIndex(cmdIdx);
    onDeviceCmdChanged(cmdIdx);
}

void TestCaseEditDialog::onDeviceCmdChanged(int) {
    const TestCaseSendChannel channel = sendChannelFromComboData(comboData(ui->comboBox_sendChannel));
    const QString cmdName = comboData(ui->comboBox_deviceCmd);
    const SendCmdParamUi uiSchema = sendCmdParamUiForName(cmdName, channel);
    const bool hasParam = uiSchema.valid && uiSchema.kind != SendCmdParamKind::None;
    updateSendParamVisibility(hasParam);
    if (!uiSchema.valid) {
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramNone);
        return;
    }
    if (uiSchema.kind == SendCmdParamKind::Int || uiSchema.kind == SendCmdParamKind::UInt)
        ui->stackedWidget_param->setCurrentWidget(ui->page_paramInt);
    else if (uiSchema.kind == SendCmdParamKind::JsonMap || uiSchema.kind == SendCmdParamKind::String)
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
