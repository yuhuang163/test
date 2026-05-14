#ifndef QSETTING_H
#define QSETTING_H

#include <QWidget>
#include <QGridLayout>
#include <QLayout>
#include <QPoint>
#include <QRect>
#include <QStringList>
#include <QVector>
#include <QVBoxLayout>

#include "advance/imagewindow/draggablecheckbox.h"
#include "my_set/my_typedef.h"
#include "qbuttongroup.h"

namespace Ui {
    class qsetting;
}

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
    void initFreeWorkTestOrderUi();
    void reorderFreeWorkCheckBoxes();
    void moveToLayout(QLayout* fromLayout, QLayout* toLayout, QWidget* widget);
    void moveToGrid(QGridLayout* layout, QWidget* widget, int row, int col);
    void moveToOptionalByPosition(DraggableCheckBox* checkBox, int row, int col);
    void calculateGridPosition(const QPoint& globalPos, const QRect& area, int& row, int& col) const;
    int getIndexAt(const QPoint& globalPos) const;
    DraggableCheckBox* getConfiguredCheckBoxByIndex(int index) const;
    DraggableCheckBox* getOptionalCheckBoxByIndex(int index) const;
    void initTestOrderStationSelector();
    void initTupleEnvironmentCombo();
    void applyStationUiState(const QString& stationKey);
    QString currentTestOrderStation() const;
    QVector<int> loadTestOrderIndexes(const QString& station) const;
    void saveTestOrderIndexes(const QString& station, const QVector<int>& indexes) const;
    void saveCurrentTestOrder();
    QVBoxLayout* freeWorkConfigLayout_ = nullptr;
    QGridLayout* freeWorkOptionalLayout_ = nullptr;
    QVector<DraggableCheckBox*> freeWorkCheckBoxes_;
    int freeWorkCols_ = 3;
    int freeWorkRows_ = 0;
    QPoint dragPos_;
    QString activeTestOrderStation_;
    QVector<int> savedTestOrderSnapshot_;
    bool testOrderDirty_ = false;
    bool switchingTestOrderStation_ = false;
    QString originalStation_;
    bool stationReloading_ = false;

protected:
    virtual void closeEvent(QCloseEvent*);
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private slots:
    void RestoreProductDefaultSetting();
    void RestoreFacDefaultSetting();
    void on_comboBox_productName_textActivated(const QString& arg1);
    void on_comboBox_factory_textActivated(const QString& arg1);
    void on_comboBox_testOrderStation_currentTextChanged(const QString& text);
    void on_comboBox_tupleEnvironment_currentIndexChanged(int index);
    void on_pushButton_clearConfiguredTestOrder_clicked();
    void on_pushButton_mesConfigFileBrowse_clicked();
};

#endif  // QSETTING_H
