#ifndef TEST_FLOW_EDITOR_H
#define TEST_FLOW_EDITOR_H

#include "test_case_types.h"

#include <QCheckBox>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QString>
#include <QVector>

class QComboBox;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QMenu;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;

class TestCaseEditDialog;

/** 流程编排区功能块（可拖拽）。 */
class TestCaseBlock : public QCheckBox {
    Q_OBJECT

  public:
    explicit TestCaseBlock(const QString& caseName, QWidget* parent = nullptr);

    QString caseName() const;
    void setCaseName(const QString& name);
    bool isBlank() const {
        return caseName_.isEmpty();
    }
    void setSelected(bool selected);
    /** 自由工站设置页才显示右键「运行」 */
    void setRunMenuVisible(bool visible);
    bool isRunMenuVisible() const {
        return runMenuVisible_;
    }

  signals:
    void editRequested(TestCaseBlock* block);
    void copyRequested(TestCaseBlock* block);
    void runRequested(TestCaseBlock* block);
    void removeFromFlowRequested(TestCaseBlock* block);
    void blockSelected(TestCaseBlock* block);

  protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

  private:
    void performDrag();
    void updateBlockStyle();

    QString caseName_;
    QPoint startPos_;
    bool selected_ = false;
    bool runMenuVisible_ = false;
};

class TestFlowEditor : public QObject {
    Q_OBJECT

  public:
    explicit TestFlowEditor(QObject* parent = nullptr);

    void bindUi(QWidget* dialogParent, QComboBox* stationCombo, QScrollArea* scroll, QVBoxLayout* flowLayout,
                QCheckBox* stopFlowOnTestFailCheck, QPushButton* btnSave, QPushButton* btnClear,
                QPushButton* btnImport, QPushButton* btnAdd, QScrollArea* failScroll, QVBoxLayout* failLayout,
                QPushButton* btnFailImport, QPushButton* btnFailAdd, QPushButton* btnFailClear);
    void reloadCurrentStation();
    /** 产品型号切换后，按 Mes/Product_Name 刷新工站下拉并切到可用工站。 */
    void onProductNameChanged();

    /** 当前工站流程相对上次保存/加载是否有改动。 */
    bool hasUnsavedChanges() const;
    /** 有未保存改动时弹窗：保存 / 取消（不保存并继续离开）。返回 true 表示可离开。 */
    bool confirmDiscardOrSaveOnLeave();
    /** 仅自由工站为 true 时，功能块右键显示「运行」 */
    void setSingleStepRunEnabled(bool enabled);
    bool isSingleStepRunEnabled() const {
        return singleStepRunEnabled_;
    }

  signals:
    /** 功能块右键「运行」：请求自由工站单步执行 */
    void runStepRequested(const QString& stationKey, const QString& caseName);

  private:
    void refreshStationCombo(const QString& selectKey = QString());
    void promptAddFlowStation();
    void promptRenameCurrentFlowStation();
    void promptCopyCurrentFlowStation();
    void promptRemoveCurrentFlowStation();
    void setupStationComboContextMenu();
    QMenu* createFlowStationMenu(QWidget* parent, int hitComboIndex);
    bool activateStationComboIndex(int index);
    void setupFailFlowRegionUi(QPushButton* btnFailImport, QPushButton* btnFailAdd, QPushButton* btnFailClear);
    void clearBlocksInLayout(QVBoxLayout* layout);
    void clearBlocks();
    void clearFailBlocks();
    void appendBlock(const QString& caseName, bool enabled = true);
    void appendFailBlock(const QString& caseName, bool enabled = true);
    TestCaseBlock* createBlock(QVBoxLayout* layout, QWidget* container, const QString& caseName, bool enabled);
    void insertBlockAfter(TestCaseBlock* after, const QString& caseName, bool enabled = true);
    void copyBlock(TestCaseBlock* src);
    void runBlock(TestCaseBlock* src);
    void setSelectedBlock(TestCaseBlock* block);
    void promptImportBlocks(bool toFailRegion);
    QString currentStationKey() const;
    QVector<TestFlowItemEntry> currentFlowEntries() const;
    QVector<TestFlowItemEntry> currentFailFlowEntries() const;
    QVector<TestFlowItemEntry> entriesFromLayout(QVBoxLayout* layout) const;
    QVBoxLayout* layoutOfSelectedBlock() const;
    void saveCurrentFlow();
    bool saveStationFlow(const QString& stationKey);
    void updateSavedSnapshot();
    void persistSelectedStation(const QString& key);
    void openEditDialog(TestCaseBlock* block);
    void moveSelectedBlock(int delta);
    bool eventFilter(QObject* watched, QEvent* event) override;

    QWidget* dialogParent_ = nullptr;
    QComboBox* stationCombo_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QVBoxLayout* flowLayout_ = nullptr;
    QScrollArea* failScroll_ = nullptr;
    QVBoxLayout* failLayout_ = nullptr;
    QCheckBox* stopFlowOnTestFailCheck_ = nullptr;
    TestCaseBlock* selectedBlock_ = nullptr;
    QWidget* flowContainer_ = nullptr;
    QWidget* failContainer_ = nullptr;
    bool uiBound_ = false;
    bool singleStepRunEnabled_ = false;
    bool suppressStationChange_ = false;
    int stationComboPrevIndex_ = 0;
    QString lastLoadedStationKey_;
    QVector<TestFlowItemEntry> savedEntriesSnapshot_;
    QVector<TestFlowItemEntry> savedFailEntriesSnapshot_;
    bool savedStopFlowOnTestFail_ = true;
    /** 每个流程块最多一个配置窗；关闭后自动移除。 */
    QHash<TestCaseBlock*, QPointer<TestCaseEditDialog>> openEditDialogs_;
};

#endif // TEST_FLOW_EDITOR_H
