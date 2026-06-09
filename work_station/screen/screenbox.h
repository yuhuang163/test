#ifndef SCREENBOX_H
#define SCREENBOX_H

#include "box_base.h"

namespace Ui {
class screenbox;
}

class screenbox : public box_base {
    Q_OBJECT
  public:
    explicit screenbox(QWidget* parent = nullptr);
    ~screenbox();
    Ui::screenbox* ui;

  private:
  private slots:

    void checkAllTest(int fixtureNumber) override;
};

#endif // SCREENBOX_H
