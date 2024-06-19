#include "ageingbox.h"
#include "QDesktopWidget"
#include "ui_ageingbox.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

ageingbox::ageingbox(QWidget *parent) : box_base(parent), ui(new Ui::ageingbox)
{
    ui->setupUi(this);
    CreatWindow<ageing>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(
        new QLabel(AGE_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

ageingbox::~ageingbox()
{
    delete ui;
}
