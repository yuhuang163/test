#ifndef AGEINGBOX_H
#define AGEINGBOX_H

#include "box_base.h"

namespace Ui {
    class ageingbox;
}

class ageingbox : public box_base {
    Q_OBJECT
public:
    explicit ageingbox(QWidget* parent = nullptr);
    ~ageingbox();

private:
    Ui::ageingbox* ui;
};

#endif  // AGEINGBOX_H
