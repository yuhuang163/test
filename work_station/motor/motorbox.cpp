
#include "motorbox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "ui_motorbox.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

motorbox::motorbox(QWidget* parent) : box_base(parent), ui(new Ui::motorbox) {
    ui->setupUi(this);
    CreatWindow<motor>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(new QLabel(MOTOR_VER));

    // QAction *Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    //  connect(Fixture_connectl_act, &QAction::triggered, [=]() {

    //      if (Fixture_uart_ui == NULL) {
    //          Fixture_uart_ui = new  Fixture_uart;

    //      }
    //      Fixture_uart_ui->show();
    //      Fixture_uart_ui->activateWindow();

    //  });
    connect(testList[testList.size() - 1]->getMacLineEdit(), SIGNAL(returnPressed()), this,
            SLOT(startAllReturnPressed()));
}

motorbox::~motorbox() {
    // delete Fixture_uart_ui;

    delete ui;
}
