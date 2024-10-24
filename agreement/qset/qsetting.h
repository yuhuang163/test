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
protected:
    virtual void closeEvent(QCloseEvent*);
private slots:
    void on_Restore_default_setting_clicked();

};

#endif  // QSETTING_H
