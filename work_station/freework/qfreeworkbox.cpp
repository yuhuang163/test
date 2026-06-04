#include "qfreeworkbox.h"

#include <QAction>
#include "qfreework.h"
#include "ui_qfreeworkbox.h"

QFreeWorkBox::QFreeWorkBox(QWidget* parent) : box_base(parent), ui(new Ui::QFreeWorkBox) {
    ui->setupUi(this);
    CreatWindow<QFreeWork>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    setWindowTitle("自由工站");
    ui->statusbar->addPermanentWidget(new QLabel(FREE_VER + QString(__DATE__) + " " + QString(__TIME__)));

    QAction* Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered, [=]() {
        if (Fixture_uart_ui == nullptr) {
            Fixture_uart_ui = new Fixture_uart;
            connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this, SLOT(startTest()));
            Fixture_uart_ui->fixBaudRate = 115200;

            QString masterFixturecomName = SETTINGS.value(QString("mechine/0/masterFixturecomName")).toString();
            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(masterFixturecomName);
        }
        Fixture_uart_ui->raise();
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    });
}

QFreeWorkBox::~QFreeWorkBox() {
    if (Fixture_uart_ui != nullptr)
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"), Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    delete ui;
}

void QFreeWorkBox::startTest() {
    startAllReturnPressed();
}
