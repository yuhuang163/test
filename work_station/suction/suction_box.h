#ifndef SUCTION_BOX_H
#define SUCTION_BOX_H
#include "box_base.h"

namespace Ui
{
    class suction_box;
}

class suction_box : public box_base
{
    Q_OBJECT
public:
    explicit suction_box(QWidget *parent = nullptr);
    ~suction_box();
    Ui::suction_box *ui;

private slots:
    void checkAllover(int fixtureNumber) override;
};

#endif   // SUCTION_BOX_H
