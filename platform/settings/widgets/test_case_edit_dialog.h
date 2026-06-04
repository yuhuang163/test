#ifndef TEST_CASE_EDIT_DIALOG_H
#define TEST_CASE_EDIT_DIALOG_H

#include "test_case_types.h"

#include <QDialog>

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
    TestCaseDefinition definition() const;

private slots:
    void onSendChannelChanged(int index);
    void onProductProtocolChanged(int index);
    void onSendActionChanged(int index);
    void onDeviceCmdChanged(int index);
    void onGateReportTypeChanged(int index);
    void updateGateFieldsEnabled();
    bool isMultiGateTableMode() const;
    bool isPeriphMultiGateMode() const;
    bool isFixturePcbaMultiGateMode() const;
    void rebuildMultiGateTable();
    QVector<TestCaseGate> readMultiGatesFromTable() const;
    QVector<TestCaseGate> readPeriphGatesFromTable() const;
    void writeMultiGatesToTable(const QVector<TestCaseGate>& gates);
    void writePeriphGatesToTable(const QVector<TestCaseGate>& gates);
    void updatePromptFieldsEnabled();
    void updateHookFieldsEnabled();

private:
    bool saveValidated();
    void refreshDeviceCmdCombo();
    void updateProductProtocolRowVisible();
    void updateSendParamVisibility(bool hasParam);

    Ui::TestCaseEditDialog* ui = nullptr;
    /** 一项回包多项卡控（外设状态 / PCBA治具数据包） */
    QTableWidget* tableWidget_multiGates_ = nullptr;
    /** 打开对话框时的配置名（用于改名后删除旧 ini） */
    QString originalCaseName_;
};

#endif  // TEST_CASE_EDIT_DIALOG_H
