#include "qcustomworkbox.h"

#include <QAction>
#include "qcustomwork.h"
#include "ui_qcustomworkbox.h"

QCustomWorkBox::QCustomWorkBox(QWidget* parent) : box_base(parent), ui(new Ui::QCustomWorkBox) {
    ui->setupUi(this);
    CreatWindow<QCustomWork>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    setWindowTitle("自定义数据驱动工站");
    ui->statusbar->addPermanentWidget(new QLabel(QString(__DATE__) + " " + QString(__TIME__)));

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

QCustomWorkBox::~QCustomWorkBox() {
    if (Fixture_uart_ui != nullptr)
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"), Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    delete ui;
}

void QCustomWorkBox::startTest() {
    startAllReturnPressed();
}
