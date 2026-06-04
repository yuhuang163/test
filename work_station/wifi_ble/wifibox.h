#ifndef WIFIBOX_H
#define WIFIBOX_H

#include "box_base.h"

namespace Ui
{
    class wifibox;
}

class wifibox : public box_base
{
    Q_OBJECT
public:
    explicit wifibox(QWidget *parent = nullptr);
    ~wifibox();
    Ui::wifibox *ui;
};

#endif   // IMUBOX_H
