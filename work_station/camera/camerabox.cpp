#include "camerabox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "ui_camerabox.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

camerabox::camerabox(QWidget* parent) : box_base(parent), ui(new Ui::camerabox) {
    ui->setupUi(this);
    CreatWindow<cameratest>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    QAction* Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered, [=]() {
        if (Fixture_uart_ui == NULL) {
            Fixture_uart_ui = new Fixture_uart;
            Fixture_uart_ui->fixBaudRate = 115200;
            for (int i = 0; i < testList.size(); i++) {
                connect(testList[i], SIGNAL(send_set_camera_action(camreaFixtureState)), Fixture_uart_ui,
                        SLOT(set_camera_action(camreaFixtureState)));
            }

            QString masterFixturecomName = SETTINGS.value(QString("mechine/0/masterFixturecomName")).toString();
            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(masterFixturecomName);
        }

        Fixture_uart_ui->show();
        Fixture_uart_ui->raise();
        Fixture_uart_ui->activateWindow();
    });
    for (int i = 0; i < testList.size(); i++) {
        connect(this, SIGNAL(go_camera_next(int)), testList[i], SLOT(canGoNextMechine(int)));
    }

    ui->statusbar->addPermanentWidget(new QLabel(CAMERA_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

camerabox::~camerabox() {

    if (Fixture_uart_ui != NULL)
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"), Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    delete ui;
}

void camerabox::checkAllTest(int fixtureNumber) {
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size()) {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates)) {
        for (int i = 0; i < testList.size(); ++i) {
            FixTureStates[i] = 0;
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "摄像头测试", "摄像头正常吗？", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            bool ok;
            QString text =
                QInputDialog::getText(nullptr, "摄像头测试", "请输入坏的摄像头号", QLineEdit::Normal, QString(), &ok);
            if (ok && !text.isEmpty()) {
                emit go_camera_next(text.toInt());
            } else {
                QMessageBox::warning(nullptr, "警告", "没有输入错误的摄像头");
            }
        } else {
            emit go_camera_next(0); // 没问题
        }
    }
}
