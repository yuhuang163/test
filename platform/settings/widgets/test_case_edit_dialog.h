#ifndef TEST_CASE_EDIT_DIALOG_H
#define TEST_CASE_EDIT_DIALOG_H

#include "test_case_types.h"

#include <QDialog>

class QShowEvent;
class QTableWidget;

namespace Ui {
class TestCaseEditDialog;
}

class TestCaseEditDialog : public QDialog {
    Q_OBJECT

  public:
    explicit TestCaseEditDialog(QWidget* parent = nullptr);
    ~TestCaseEditDialog() override;
    void setDefinition(const TestCaseDefinition& def, const QString& storageKey = QString());
    void setStationContext(const QString& stationKey);
    /** 当前编排区流程（用于校验「上报MES的字段」在流程内不重复） */
    void setFlowContext(const QVector<TestFlowItemEntry>& entries);
    TestCaseDefinition definition() const;

  protected:
    void showEvent(QShowEvent* event) override;

  private slots:
    void onSendChannelChanged(int index);
    void onProductProtocolChanged(int index);
    void onSendActionChanged(int index);
    void onDeviceCmdChanged(int index);
    void onGateReportTypeChanged(int index);
    void updateGateFieldsEnabled();
    bool isPeriphMultiGateMode() const;
    bool isRangeMultiGateMode() const;
    void rebuildMultiGateTable();
    QVector<TestCaseGate> readMultiGatesFromTable() const;
    QVector<TestCaseGate> readPeriphGatesFromTable() const;
    void writeMultiGatesToTable(const QVector<TestCaseGate>& gates);
    void writePeriphGatesToTable(const QVector<TestCaseGate>& gates);
    void updatePromptFieldsEnabled();
    void updateHookFieldsEnabled();
    void onHookIdChanged(int index);

  private:
    bool saveValidated();
    void refreshDeviceCmdCombo();
    void syncGateToSendCommand(const QString& preferredReportType, const QString& preferredField = QString());
    void updateProductProtocolRowVisible();
    void updateSendParamVisibility(bool hasParam);
    void fitDialogToScreen();

    Ui::TestCaseEditDialog* ui = nullptr;
    /** 一项回包多项卡控（外设状态 / PCBA治具数据包） */
    QTableWidget* tableWidget_multiGates_ = nullptr;
    /** 打开对话框时的配置名（用于改名后删除旧 ini） */
    QString originalCaseName_;
    /** 当前工站 Profile 键；非空时保存工站参数覆盖 */
    QString stationKey_;
    /** 打开对话框时编排区中的流程项（含本步），用于 MesTag 重复校验 */
    QVector<TestFlowItemEntry> flowEntries_;
    /** 避免 setDefinition 末尾再进 onDeviceCmdChanged 时冲掉已加载的参数表 */
    QString lastSendParamCmdKey_;
    bool loadingDefinition_ = false;
    bool gateReportTypeLocked_ = false;
};

#endif // TEST_CASE_EDIT_DIALOG_H
