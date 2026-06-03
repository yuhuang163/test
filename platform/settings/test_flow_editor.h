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

class TestCaseEditDialog;



class QComboBox;

class QDragEnterEvent;

class QDragMoveEvent;

class QDropEvent;

class QVBoxLayout;

class QPushButton;

class QScrollArea;

class QWidget;



/** 流程编排区功能块（可拖拽）。 */

class TestCaseBlock : public QCheckBox {

    Q_OBJECT



public:

    explicit TestCaseBlock(const QString& caseName, QWidget* parent = nullptr);

    QString caseName() const;

    void setCaseName(const QString& name);

    bool isBlank() const { return caseName_.isEmpty(); }

    void setSelected(bool selected);



signals:

    void editRequested(TestCaseBlock* block);

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

};



class TestFlowEditor : public QObject {

    Q_OBJECT



public:

    explicit TestFlowEditor(QObject* parent = nullptr);

    void bindUi(QWidget* dialogParent, QComboBox* stationCombo, QScrollArea* scroll, QVBoxLayout* flowLayout,

                QCheckBox* stopFlowOnTestFailCheck, QPushButton* btnSave, QPushButton* btnClear, QPushButton* btnImport,

                QPushButton* btnAdd);

    void reloadCurrentStation();

    /** 当前工站流程相对上次保存/加载是否有改动。 */
    bool hasUnsavedChanges() const;

    /** 有未保存改动时弹窗：保存 / 不保存 / 取消。返回 true 表示可离开。 */
    bool confirmDiscardOrSaveOnLeave();



private:

    void refreshStationCombo(const QString& selectKey = QString());

    void promptAddFlowStation();

    void promptRemoveCurrentFlowStation();

    void clearBlocks();

    void appendBlock(const QString& caseName);

    void setSelectedBlock(TestCaseBlock* block);

    QString currentStationKey() const;

    QVector<TestFlowItemEntry> currentFlowEntries() const;

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

    QCheckBox* stopFlowOnTestFailCheck_ = nullptr;

    TestCaseBlock* selectedBlock_ = nullptr;

    QWidget* flowContainer_ = nullptr;

    bool uiBound_ = false;

    bool suppressStationChange_ = false;

    int stationComboPrevIndex_ = 0;

    QString lastLoadedStationKey_;

    QVector<TestFlowItemEntry> savedEntriesSnapshot_;

    bool savedStopFlowOnTestFail_ = true;

    /** 每个流程块最多一个配置窗；关闭后自动移除。 */
    QHash<TestCaseBlock*, QPointer<TestCaseEditDialog>> openEditDialogs_;

};



#endif  // TEST_FLOW_EDITOR_H

