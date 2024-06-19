#ifndef CAMERABOX_H
#define CAMERABOX_H

#include "box_base.h"
#include "cameratest.h"
#include "ui_cameratest.h"
#include <vector>

namespace Ui
{
    class camerabox;
}

class camerabox : public box_base
{
    Q_OBJECT
public:
    explicit camerabox(QWidget *parent = nullptr);
    ~camerabox();
private:
    Ui::camerabox *ui;
    void checkAllTest(int fixtureNumber)override;

signals:
    void go_camera_next(int);
};

#endif   // CAMERABOX_H
