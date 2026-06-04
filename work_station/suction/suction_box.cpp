#include "suction_box.h"

#include "suction.h"
#include "ui_suction_box.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

suction_box::suction_box(QWidget* parent) : box_base(parent), ui(new Ui::suction_box) {
    ui->setupUi(this);
    CreatWindow<suction>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(new QLabel(SUCTION_VER + QString(__DATE__) + " " + QString(__TIME__)));

    // QAction *jig_connectl_act = ui->menubar->addAction("连接治具串口");
    //  connect(jig_connectl_act, &QAction::triggered, [=]() {

    //      if (jig_uart_ui == NULL) {
    //          jig_uart_ui = new  jig_uart;

    //      }
    //      jig_uart_ui->show();
    //      jig_uart_ui->activateWindow();

    //  });
}

suction_box::~suction_box() { delete ui; }

void suction_box::checkAllover(int fixtureNumber) {
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size()) {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates)) {
        testList[0]->getMacLineEdit()->setFocus();

        testList[testList.size() - 1]->jig->set_cylinder_state(0, testList.size());
        waitWork(500);
        for (int i = 0; i < testList.size(); ++i) {
            testList[i]->closeDongleSerialPort();
            FixTureStates[i] = 0;
        }
    }
}
