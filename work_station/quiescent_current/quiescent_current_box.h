#ifndef QUIESCENT_CURRENT_BOX_H
#define QUIESCENT_CURRENT_BOX_H
#include "box_base.h"

namespace Ui
{
    class quiescent_current_box;
}

class quiescent_current_box : public box_base
{
    Q_OBJECT
public:
    explicit quiescent_current_box(QWidget *parent = nullptr);
    ~quiescent_current_box();
    Ui::quiescent_current_box *ui;

private slots:
    void checkAllover(int fixtureNumber) override;
};

#endif   // QUIESCENT_CURRENT_BOX_H
