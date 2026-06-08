#ifndef KEY_TEST_BOX_H
#define KEY_TEST_BOX_H
#include "box_base.h"

namespace Ui {
class key_test_box;
}

class key_test_box : public box_base {
    Q_OBJECT
  public:
    explicit key_test_box(QWidget* parent = nullptr);
    ~key_test_box();
    Ui::key_test_box* ui;

  private slots:
    void checkAllover(int fixtureNumber) override;
};

#endif // KEY_TEST_BOX_H
