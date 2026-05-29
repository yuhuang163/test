#include "qcustomwork.h"
#include "ui_qcustomwork.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>
#include <QDateTime>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

QCustomWork::QCustomWork(int index, QWidget* parent)
    : test_base(parent), ui(new Ui::QCustomWork) {
    ui->setupUi(this);
    m_index = index;
    
    // 初始化引擎
    engine_.setProtocolManager(&protocolManager);
    engine_.setAt(at);
    engine_.setUsb(usb);
    engine_.setRetrySender([this](std::function<void()> func, int timeoutMs) -> int {
        return sendCommandWithRetry(func, timeoutMs);
    });

    initUI();
    registerCustomFunctions();
    loadDefaultTemplates();
}

QCustomWork::~QCustomWork() {
    delete ui;
}

void QCustomWork::initUI() {
    // 串口操作
    connect(ui->connectButton, &QPushButton::clicked, this, &QCustomWork::on_connectButton_clicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &QCustomWork::on_disconnectButton_clicked);
    connect(ui->usbConnectButton, &QPushButton::clicked, this, &QCustomWork::on_usbConnectButton_clicked);
    connect(ui->usbDisconnectButton, &QPushButton::clicked, this, &QCustomWork::on_usbDisconnectButton_clicked);
    
    // SN & MAC
    connect(ui->macInput, &QLineEdit::returnPressed, this, &QCustomWork::on_macInput_returnPressed);
    connect(ui->getMac, &QLineEdit::returnPressed, this, &QCustomWork::on_getMac_returnPressed);
    
    // 步骤列表操作
    connect(ui->stepListWidget, &QListWidget::currentRowChanged, this, &QCustomWork::onStepSelectionChanged);
    connect(ui->addStepButton, &QPushButton::clicked, this, &QCustomWork::on_addStepButton_clicked);
    connect(ui->removeStepButton, &QPushButton::clicked, this, &QCustomWork::on_removeStepButton_clicked);
    connect(ui->moveUpButton, &QPushButton::clicked, this, &QCustomWork::on_moveUpButton_clicked);
    connect(ui->moveDownButton, &QPushButton::clicked, this, &QCustomWork::on_moveDownButton_clicked);
    connect(ui->saveConfigButton, &QPushButton::clicked, this, &QCustomWork::on_saveConfigButton_clicked);
    connect(ui->loadConfigButton, &QPushButton::clicked, this, &QCustomWork::on_loadConfigButton_clicked);
    connect(ui->applyEditButton, &QPushButton::clicked, this, &QCustomWork::on_applyEditButton_clicked);
    
    // 参数表添加/删除
    connect(ui->addParamButton, &QPushButton::clicked, this, [this]() {
        int row = ui->paramsTable->rowCount();
        ui->paramsTable->insertRow(row);
        ui->paramsTable->setItem(row, 0, new QTableWidgetItem(""));
        ui->paramsTable->setItem(row, 1, new QTableWidgetItem(""));
    });
    connect(ui->removeParamButton, &QPushButton::clicked, this, [this]() {
        int row = ui->paramsTable->currentRow();
        if (row >= 0) {
            ui->paramsTable->removeRow(row);
        }
    });

    // 初始化下拉框数据
    ui->editActionType->addItems({"ProtocolSet", "ProtocolGet", "AtCommand", "UsbCommand", "CustomFunction"});
    connect(ui->editActionType, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        ui->editDeviceCmd->clear();
        if (text == "ProtocolSet" || text == "ProtocolGet") {
            ui->editDeviceCmd->addItems(TestStepEngine::allDeviceCmdNames());
        } else if (text == "AtCommand") {
            ui->editDeviceCmd->addItems({"sendDcon", "sendMac"});
        } else if (text == "UsbCommand") {
            ui->editDeviceCmd->addItems({"ReadMeasurement"});
        } else if (text == "CustomFunction") {
            ui->editDeviceCmd->addItems({"logTest"});
        }
    });
    // 初次加载列表
    ui->editDeviceCmd->addItems(TestStepEngine::allDeviceCmdNames());
    
    // 初始化参数表头
    ui->paramsTable->setColumnCount(2);
    ui->paramsTable->setHorizontalHeaderLabels({"Key", "Value"});
    ui->paramsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // 默认不运行状态
    testResultTableInit();
}

QString QCustomWork::configFilePath() const {
    return QCoreApplication::applicationDirPath() + "/custom_work_steps.json";
}

void QCustomWork::loadDefaultTemplates() {
    // 尝试从文件加载
    QVector<TestStepDescriptor> steps = TestStepConfig::loadFromFile(configFilePath());
    if (!steps.isEmpty()) {
        selectedSteps_ = steps;
    } else {
        // 提供几个基本的模板
        TestStepDescriptor s1;
        s1.name = "获取基本信息";
        s1.actionType = TestStepDescriptor::ProtocolGet;
        s1.deviceCmd = "BaseInfo";
        s1.needCaseDone = true;
        s1.mesTag = "BASE_INFO";
        
        TestStepDescriptor s2;
        s2.name = "获取电量";
        s2.actionType = TestStepDescriptor::ProtocolGet;
        s2.deviceCmd = "GetBattery";
        s2.needCaseDone = true;
        s2.mesTag = "BATTERY_INFO";

        TestStepDescriptor s3;
        s3.name = "进入厂测模式";
        s3.actionType = TestStepDescriptor::ProtocolSet;
        s3.deviceCmd = "FacMode";
        s3.params["value"] = 1;
        s3.needCaseDone = true;
        s3.mesTag = "FAC_MODE";

        TestStepDescriptor s4;
        s4.name = "读取按键信号";
        s4.actionType = TestStepDescriptor::ProtocolGet;
        s4.deviceCmd = "KeySignalRead";
        s4.needCaseDone = true;
        s4.mesTag = "KEY_SIGNAL";

        selectedSteps_ << s1 << s2 << s3 << s4;
    }
    refreshStepListUI();
}

void QCustomWork::registerCustomFunctions() {
    engine_.registerCustomFunction("logTest", [this]() {
        showlog("执行了自定义函数 logTest!");
        stepRuntime_.pass = true;
        stepRuntime_.done = true;
    });
}

// ---- UI 操作 ----

void QCustomWork::refreshStepListUI() {
    ui->stepListWidget->clear();
    for (int i = 0; i < selectedSteps_.size(); ++i) {
        ui->stepListWidget->addItem(QString("[%1] %2").arg(i+1).arg(selectedSteps_[i].name));
    }
}

void QCustomWork::onStepSelectionChanged() {
    int row = ui->stepListWidget->currentRow();
    refreshStepEditorUI(row);
}

void QCustomWork::refreshStepEditorUI(int index) {
    if (index < 0 || index >= selectedSteps_.size()) {
        ui->editStepName->clear();
        ui->editActionType->setCurrentIndex(0);
        ui->editDeviceCmd->setCurrentIndex(-1);
        ui->editTimeout->setValue(300);
        ui->editNeedCaseDone->setChecked(false);
        ui->editMesTag->clear();
        ui->editCustomFunc->clear();
        ui->paramsTable->setRowCount(0);
        return;
    }
    
    const auto& step = selectedSteps_[index];
    ui->editStepName->setText(step.name);
    ui->editActionType->setCurrentText(TestStepDescriptor::actionTypeToString(step.actionType));
    ui->editDeviceCmd->setCurrentText(step.deviceCmd);
    ui->editTimeout->setValue(step.retryTimeoutMs);
    ui->editNeedCaseDone->setChecked(step.needCaseDone);
    ui->editMesTag->setText(step.mesTag);
    ui->editCustomFunc->setText(step.customFunctionName);
    
    ui->paramsTable->setRowCount(0);
    for (auto it = step.params.constBegin(); it != step.params.constEnd(); ++it) {
        int row = ui->paramsTable->rowCount();
        ui->paramsTable->insertRow(row);
        ui->paramsTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        ui->paramsTable->setItem(row, 1, new QTableWidgetItem(it.value().toString()));
    }
}

void QCustomWork::on_applyEditButton_clicked() {
    int row = ui->stepListWidget->currentRow();
    if (row < 0 || row >= selectedSteps_.size()) return;
    
    TestStepDescriptor& step = selectedSteps_[row];
    step.name = ui->editStepName->text();
    step.actionType = TestStepDescriptor::actionTypeFromString(ui->editActionType->currentText());
    step.deviceCmd = ui->editDeviceCmd->currentText();
    step.retryTimeoutMs = ui->editTimeout->value();
    step.needCaseDone = ui->editNeedCaseDone->isChecked();
    step.mesTag = ui->editMesTag->text();
    step.customFunctionName = ui->editCustomFunc->text();
    
    step.params.clear();
    for (int i = 0; i < ui->paramsTable->rowCount(); ++i) {
        auto keyItem = ui->paramsTable->item(i, 0);
        auto valItem = ui->paramsTable->item(i, 1);
        if (keyItem && valItem && !keyItem->text().isEmpty()) {
            step.params[keyItem->text()] = valItem->text();
        }
    }
    
    refreshStepListUI();
    ui->stepListWidget->setCurrentRow(row);
    showlog(QString("步骤 '%1' 修改已应用。").arg(step.name));
}

void QCustomWork::on_addStepButton_clicked() {
    TestStepDescriptor newStep;
    newStep.name = "新测试步骤";
    selectedSteps_.append(newStep);
    refreshStepListUI();
    ui->stepListWidget->setCurrentRow(selectedSteps_.size() - 1);
}

void QCustomWork::on_removeStepButton_clicked() {
    int row = ui->stepListWidget->currentRow();
    if (row >= 0 && row < selectedSteps_.size()) {
        selectedSteps_.removeAt(row);
        refreshStepListUI();
    }
}

void QCustomWork::on_moveUpButton_clicked() {
    int row = ui->stepListWidget->currentRow();
    if (row > 0 && row < selectedSteps_.size()) {
        qSwap(selectedSteps_[row], selectedSteps_[row - 1]);
        refreshStepListUI();
        ui->stepListWidget->setCurrentRow(row - 1);
    }
}

void QCustomWork::on_moveDownButton_clicked() {
    int row = ui->stepListWidget->currentRow();
    if (row >= 0 && row < selectedSteps_.size() - 1) {
        qSwap(selectedSteps_[row], selectedSteps_[row + 1]);
        refreshStepListUI();
        ui->stepListWidget->setCurrentRow(row + 1);
    }
}

void QCustomWork::on_saveConfigButton_clicked() {
    if (TestStepConfig::saveToFile(configFilePath(), selectedSteps_)) {
        showlog(QString("配置已保存到 %1").arg(configFilePath()));
    } else {
        showlog("保存配置失败！");
    }
}

void QCustomWork::on_loadConfigButton_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "加载配置", QCoreApplication::applicationDirPath(), "JSON Files (*.json)");
    if (!fileName.isEmpty()) {
        auto steps = TestStepConfig::loadFromFile(fileName);
        if (!steps.isEmpty()) {
            selectedSteps_ = steps;
            refreshStepListUI();
            showlog(QString("已加载配置 %1，共 %2 个步骤。").arg(fileName).arg(steps.size()));
        } else {
            showlog("加载配置失败或配置为空！");
        }
    }
}


// ---- 测试执行 ----

void QCustomWork::startTask() {
    if (testState_ >= 0) {
        showlog("测试正在进行中！");
        return;
    }
    
    if (selectedSteps_.isEmpty()) {
        showlog("测试步骤为空，请先配置步骤！");
        return;
    }
    
    testResult_ = passValue;
    ui->test_result->setText("TESTING");
    ui->test_result->setStyleSheet("background-color: yellow; color: black; font-size: 50px; font-weight: bold;");
    
    testItems.clear();
    testResultTableInit();
    
    testState_ = 0;
    testTime_.start();
    
    // 初始化第一个步骤
    stepRuntime_.reset();
    stepRuntime_.stepIndex = 0;
    
    showlog(QString("===== 开始测试 (共 %1 步) =====").arg(selectedSteps_.size()));
    
    // 使用定时器驱动状态机，防止阻塞主线程
    QTimer::singleShot(100, this, [this]() {
        // 这里只是简单示例，后续可封装为一个循环状态机
        // 伪代码: execute step -> check done -> next step
        showlog("由于是示例框架，实际状态机需结合 QTimer 或 EventLoop 驱动");
        
        // 执行第一个
        const auto& step = selectedSteps_[stepRuntime_.stepIndex];
        showlog(QString("-> 执行步骤: %1").arg(step.name));
        stepRuntime_.started = true;
        engine_.executeStep(step);
        
        if (!step.needCaseDone) {
            stepRuntime_.done = true;
        }
    });
}

void QCustomWork::on_stopTest_clicked() {
    if (testState_ >= 0) {
        testState_ = -1;
        ui->test_result->setText("STOPPED");
        ui->test_result->setStyleSheet("background-color: orange; color: white; font-size: 50px; font-weight: bold;");
        showlog("测试已手动停止。");
    }
}

// 模拟协议回调处理：当收到响应时，检查是否是当前需要的
bool QCustomWork::isCurrentStep(const QString& stepName) const {
    if (testState_ < 0 || stepRuntime_.stepIndex < 0 || stepRuntime_.stepIndex >= selectedSteps_.size()) return false;
    return selectedSteps_[stepRuntime_.stepIndex].name == stepName;
}

void QCustomWork::refreshBaseData(ProtocolBaseInfoData data) {
    if (isCurrentStep("获取基本信息")) {
        stepRuntime_.testData = QString("Ver: %1").arg(data.soft_version);
        stepRuntime_.done = true;
        showlog(QString("获取基本信息成功: %1").arg(stepRuntime_.testData));
    }
    // ... 可以添加显示到 UI 的逻辑
}

void QCustomWork::refreshBattaryData(ProtocolBatteryData data) {
    if (isCurrentStep("获取电量")) {
        stepRuntime_.testData = QString("%1%").arg(data.percent);
        stepRuntime_.done = true;
        showlog(QString("获取电量成功: %1").arg(stepRuntime_.testData));
    }
}

// 接口实现，返回 UI 元素指针供 test_base 使用
QComboBox* QCustomWork::getComNameCombo() { return ui->comNameCombo; }
QComboBox* QCustomWork::getUsbcomNameCombo() { return ui->usbcomNameCombo; }
QComboBox* QCustomWork::getJigcomNameCombo() { return nullptr; }
QComboBox* QCustomWork::getProductcomNameCombo() { return nullptr; }
QLineEdit* QCustomWork::getMacLineEdit() { return ui->getMac; }
QLineEdit* QCustomWork::macInputLineEdit() { return ui->macInput; }
QPlainTextEdit* QCustomWork::logEdit() { return ui->log; }
QPlainTextEdit* QCustomWork::msgEdit() { return ui->msgEdit; }
QTableWidget* QCustomWork::testResultTable() { return ui->testResultTable; }
QLabel* QCustomWork::getMesStateQlabel() { return ui->mes_state; }
QPushButton* QCustomWork::getEndTestButton() { return ui->stopTest; }
QCheckBox* QCustomWork::getIsUseMes() { return ui->isusemes; }
QCheckBox* QCustomWork::getIsFormMes() { return ui->isformmes; }
void QCustomWork::updateComboBox() {}

// 按键事件
void QCustomWork::on_connectButton_clicked() { openDongleSerialPort(); }
void QCustomWork::on_disconnectButton_clicked() { closeDongleSerialPort(); }
void QCustomWork::on_usbConnectButton_clicked() { openUsbSerialPort(); }
void QCustomWork::on_usbDisconnectButton_clicked() { closeUsbSerialPort(); }
void QCustomWork::on_macInput_returnPressed() {
    QString mac = ui->macInput->text().trimmed();
    static const QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    if (!macRegex.match(mac).hasMatch()) {
        showlog("MAC地址格式错误: " + mac);
        return;
    }
    macAddress = mac;
    showlog("MAC地址已确认: " + mac);
    pack.sn = ui->getMac->text().trimmed();
    
    // 如果还没启动测试，则启动测试（比如从 MES 拿回 MAC 后，或者直接扫 MAC）
    if (testState_ < 0) {
        startTask();
    }
}

void QCustomWork::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}

void QCustomWork::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = inputSnText;
            pack.mechines = getIndex();
            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit sendProcessInspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }
    if (!ui->isusemes->checkState()) {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid #FFFF00; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void QCustomWork::getTestValue(const int mechines, const QString value) {
    QString mesmacAddress;
    if (pack.factory == "hq") {
        QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");
        QRegularExpressionMatch match = regex.match(value);
        if (match.hasMatch()) {
            mesmacAddress = match.captured(1);
            if (mechines == getIndex()) {
                ui->macInput->setText(mesmacAddress);
                on_macInput_returnPressed();
            }
        } else {
            showlog("mes未找到匹配的MAC地址");
            showlog(value);
        }
    } else if (pack.factory == "lx") {
        mesmacAddress = value;
        mesmacAddress.insert(2, ":");
        mesmacAddress.insert(5, ":");
        mesmacAddress.insert(8, ":");
        mesmacAddress.insert(11, ":");
        mesmacAddress.insert(14, ":");
        mesmacAddress = mesmacAddress.toUpper();
        if (mechines == getIndex()) {
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    } else if (pack.factory.trimmed().compare(QStringLiteral("byd"), Qt::CaseInsensitive) == 0) {
        if (mechines != getIndex()) return;
        mesmacAddress = parseMacFromSn(value.trimmed());
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            return;
        }
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        on_macInput_returnPressed();
    } else {
        if (mechines == getIndex()) {
            ui->macInput->setText(value);
            on_macInput_returnPressed();
        }
    }
}

void QCustomWork::on_getMac_returnPressed() {
    QString sn = ui->getMac->text().trimmed();
    if (sn.isEmpty()) {
        showlog("SN/序列号不能为空");
        return;
    }
    QRegularExpression snRegex(snPattern);
    if (!snPattern.isEmpty() && !snRegex.match(sn).hasMatch()) {
        showlog("序列号格式错误，要求格式为: " + snPattern);
        ui->getMac->clear();
        return;
    }
    
    showlog("校验通过: " + sn);
    
    if (ui->isformmes->checkState()) {
        showlog("正在向 MES 查 MAC...");
        processGetMesTestValue();
    }
    
    if (ui->isusemes->checkState()) {
        processInspection(sn);
        appendStationResult(testItems, "MES启动", "0.0000", passValue);
    }
    
    // 如果没有勾选从 MES 获取，则直接启动测试
    if (!ui->isformmes->checkState()) {
        pack.sn = sn;
        startTask();
    }
}

// 一些基础回调空实现（如有需要可实现）
void QCustomWork::refreshBleState(int) {}
void QCustomWork::refreshDongleUartState(int) {}
void QCustomWork::refreshUsbUartState(int) {}
void QCustomWork::refreshJigUartState(int) {}
void QCustomWork::refreshProductUartState(int) {}
void QCustomWork::refreshSn(ProtocolSnData) {}
void QCustomWork::refreshPeriphData(ProtocolPeriphStateData) {}
void QCustomWork::refreshRssiRead(ProtocolRssiData) {}
void QCustomWork::refreshChargeCurrentRead(ProtocolUInt32ValueData) {}
void QCustomWork::refreshAmmeterData(QString) {}
