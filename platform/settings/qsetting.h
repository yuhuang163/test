#ifndef QSETTING_H
#define QSETTING_H

#include <QWidget>

#include "my_set/my_typedef.h"
#include "qbuttongroup.h"

namespace Ui {
class qsetting;
}

class TestFlowEditor;

class qsetting : public QWidget {
    Q_OBJECT

public:
    explicit qsetting(QWidget* parent = nullptr);
    ~qsetting();

private:
    Ui::qsetting* ui;
    QButtonGroup* StationGroup = new QButtonGroup(this);
    void loadConfig();
    void saveConfig();
    void updateMainStyle(QString style);
    void readSubPIDAndFilter();
    void saveSubPIDAndFilter();
    void initSettingTooltips();
    void initTestFlowEditorUi();
    void initTupleEnvironmentCombo();
    QString originalStation_;
    bool stationReloading_ = false;
    TestFlowEditor* testFlowEditor_ = nullptr;
    int lastSettingsTabIndex_ = 0;

protected:
    virtual void closeEvent(QCloseEvent*);

private slots:
    void RestoreProductDefaultSetting();
    void RestoreFacDefaultSetting();
    void on_comboBox_productName_textActivated(const QString& arg1);
    void on_comboBox_factory_textActivated(const QString& arg1);
    void on_comboBox_tupleEnvironment_currentIndexChanged(int index);
    void on_pushButton_mesConfigFileBrowse_clicked();
};

#endif  // QSETTING_H
