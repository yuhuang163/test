#ifndef QSETTING_H
#define QSETTING_H

#include <QWidget>

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


protected:
    virtual void closeEvent(QCloseEvent*);
private slots:
    void RestoreDefaultSetting();

    void on_comboBox_productName_textActivated(const QString &arg1);
};

#endif  // QSETTING_H
