#ifndef QCUSTOMWORK_H
#define QCUSTOMWORK_H

#include <QWidget>
#include <QVector>
#include <QElapsedTimer>
#include "test_base.h"
#include "test_step_descriptor.h"
#include "test_step_engine.h"

namespace Ui {
    class QCustomWork;
}

/**
 * 数据驱动可编辑自定义工站。
 *
 * 与 QFreeWork 相比的核心区别：
 * 1. 测试步骤由 JSON 配置文件定义（TestStepDescriptor），而非宏硬编码
 * 2. 用户可在 UI 上选择步骤、编辑 DeviceCmd 和参数、保存/加载配置
 * 3. 执行逻辑由通用引擎 TestStepEngine 驱动
 */
class QCustomWork : public test_base {
    Q_OBJECT
public:
    explicit QCustomWork(int index, QWidget* parent = nullptr);
    ~QCustomWork();

    void startTask() override;

private:
    Ui::QCustomWork* ui;
    TestStepEngine engine_;

    // 测试步骤配置
    QVector<TestStepDescriptor> allSteps_;        // 模板库：所有可选步骤
    QVector<TestStepDescriptor> selectedSteps_;   // 当前选中的有序步骤

    // 测试运行状态
    int testState_ = -1;
    QString testResult_;
    QElapsedTimer testTime_;

    struct StepRuntime {
        bool started = false;
        bool done = false;
        bool pass = true;
        int stepIndex = -1;
        QString testData;
        QString ask = "通过";
        QElapsedTimer caseTimer;

        void reset() {
            started = false;
            done = false;
            pass = true;
            stepIndex = -1;
            testData.clear();
            ask = "通过";
            caseTimer.invalidate();
        }
    } stepRuntime_;

    // 初始化
    void initUI();
    void loadDefaultTemplates();
    void registerCustomFunctions();

    // 步骤列表 UI 操作
    void refreshStepListUI();
    void refreshStepEditorUI(int index);
    void onStepSelectionChanged();

    // 配置管理
    QString configFilePath() const;

    // 协议回调判定（与 QFreeWork 类似的异步回调）
    bool isCurrentStep(const QString& stepName) const;

private slots:
    // UI 控件
    QComboBox* getComNameCombo() override;
    QComboBox* getUsbcomNameCombo() override;
    QComboBox* getJigcomNameCombo() override;
    QComboBox* getProductcomNameCombo() override;
    QLineEdit* getMacLineEdit() override;
    QLineEdit* macInputLineEdit() override;
    QPlainTextEdit* logEdit() override;
    QPlainTextEdit* msgEdit() override;
    QTableWidget* testResultTable() override;
    QLabel* getMesStateQlabel() override;
    QPushButton* getEndTestButton() override;
    QCheckBox* getIsUseMes() override;
    QCheckBox* getIsFormMes() override;

    void updateComboBox() override;

    // 协议数据回调
    void refreshBleState(int state) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshJigUartState(int state) override;
    void refreshProductUartState(int state) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshBattaryData(ProtocolBatteryData data) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshRssiRead(ProtocolRssiData data) override;
    void refreshChargeCurrentRead(ProtocolUInt32ValueData data) override;
    void refreshAmmeterData(QString data) override;

    // 按钮操作
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_usbConnectButton_clicked();
    void on_usbDisconnectButton_clicked();
    void on_macInput_returnPressed();
    void on_getMac_returnPressed();
    void on_stopTest_clicked();

    // 步骤管理按钮
    void on_addStepButton_clicked();
    void on_removeStepButton_clicked();
    void on_moveUpButton_clicked();
    void on_moveDownButton_clicked();
    void on_saveConfigButton_clicked();
    void on_loadConfigButton_clicked();
    void on_applyEditButton_clicked();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
};

#endif  // QCUSTOMWORK_H
