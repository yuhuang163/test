#ifndef QSETTING_H
#define QSETTING_H

#include <QWidget>

#include "my_set/my_typedef.h"
#include "qbuttongroup.h"
#include "label_print_service.h"

namespace Ui {
class qsetting;
}

class TestFlowEditor;

class qsetting : public QWidget {
    Q_OBJECT

  public:
    explicit qsetting(QWidget* parent = nullptr);
    ~qsetting();
    void loadConfig();
    /** 仅自由工站打开设置时开启：功能块右键「运行」/双击单步 */
    void setTestCaseSingleStepRunEnabled(bool enabled);

  signals:
    /** 测试流程编排功能块单步运行请求（仅自由工站启用时有效） */
    void runTestCaseStepRequested(const QString& stationKey, const QString& caseName);
    /** 设置已写入 ini（关闭设置页等），主窗口/工站可据此热切换协议等 */
    void settingsSaved();

  private:
    Ui::qsetting* ui;
    QButtonGroup* StationGroup = new QButtonGroup(this);
    void saveConfig();
    void updateMainStyle(QString style);
    void readSubPIDAndFilter();
    void saveSubPIDAndFilter();
    void initSettingTooltips();
    void initTestFlowEditorUi();
    void syncFactoryCloudDerivedUrls();
    void initTupleEnvironmentCombo();
    void loadLabelPrinterConfig();
    void saveLabelPrinterConfig();
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
    void on_pushButton_labelPrinterTestPrint_clicked();
};

#endif // QSETTING_H
