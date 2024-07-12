#include "qfreeworkbox.h"
#include "qfreework.h"
#include "ui_qfreeworkbox.h"

QFreeWorkBox::QFreeWorkBox(QWidget *parent)
    : box_base(parent)
    , ui(new Ui::QFreeWorkBox)
{
    ui->setupUi(this);
    CreatWindow<QFreeWork>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    setWindowTitle("自由工站");
    ui->statusbar->addPermanentWidget(
        new QLabel(FREE_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

QFreeWorkBox::~QFreeWorkBox()
{
    delete ui;
}
