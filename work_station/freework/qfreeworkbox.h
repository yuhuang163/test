#ifndef QFREEWORKBOX_H
#define QFREEWORKBOX_H

#include "box_base.h"

namespace Ui {
    class QFreeWorkBox;
}

class QFreeWorkBox : public box_base {
    Q_OBJECT

public:
    explicit QFreeWorkBox(QWidget* parent = nullptr);
    ~QFreeWorkBox();
    Ui::QFreeWorkBox* ui;
};

#endif  // QFREEWORKBOX_H
