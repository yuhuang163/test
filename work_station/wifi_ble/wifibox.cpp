#include "wifibox.h"

#include "ui_wifibox.h"
#include "wifibletest.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

wifibox::wifibox(QWidget* parent) : box_base(parent), ui(new Ui::wifibox) {
    ui->setupUi(this);
    CreatWindow<wifibletest>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    setWindowTitle("信号测试工站");
    ui->statusbar->addPermanentWidget(new QLabel(SINGLE_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

wifibox::~wifibox() { delete ui; }
