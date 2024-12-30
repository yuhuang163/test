#include "PressCalibBox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "AbIni.h"
#include "QDesktopWidget"
#include "fixture_uart.h"
#include "pressuresensorform.h"
#include "ui_PressCalibBox.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

PressCalibBox::PressCalibBox(QWidget* parent) : box_base(parent), ui(new Ui::PressCalibBox) {
    ui->setupUi(this);
    CreatWindow<PressureSensorForm>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(new QLabel(PRESSURE_VER + QString(__DATE__) + " " + QString(__TIME__)));

    for (int i = 0; i < testList.size(); i++) {
        display_state.push_back(0);
        // other
        connect(testList[i], SIGNAL(operator_instruct(int)), this, SLOT(display_on_screen(int)));
    }

    QAction* Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered, [=]() {
        if (Fixture_uart_ui == NULL) {
            Fixture_uart_ui = new Fixture_uart;

            for (int i = 0; i < testList.size(); i++) {
                connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_press(int)), testList[i],
                        SLOT(receive_uart_data(int)));
                connect(testList[i], SIGNAL(send_data_to_mechine(FixturePacketData)), this,
                        SLOT(send_uart_data(FixturePacketData)));
            }

            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(
                SETTINGS.value("mechine/FixturecomName").toString());
        }
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    });

    QAction* end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->overTask();
        }
    });
}

void PressCalibBox::display_on_screen(int instruct) {
    for (int i = 0; i < testList.size(); i++) {
        qDebug() << "testList.size()" << testList.size() << "i" << i << "display_state[i]" << display_state[i];

        if (testList[i]->get_independent_state() != STATE_WAIT_START) {
            return;
        }
    }

    if (SETTINGS.value("SYSTEM/SimplePcbaTest").toBool()) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "压感校准测试", "是否开始测试", QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::Yes) {
            for (int i = 0; i < testList.size(); i++) {
                testList[i]->set_independent_state(STATE_CALIB_START);
            }
        } else if (reply == QMessageBox::No) {
            qDebug() << "还没准备好，需要重新开始测试";
        }
    }


    // testList[0]->ui->log->appendPlainText("请按下治具");
}
PressCalibBox::~PressCalibBox() { delete ui; }

void PressCalibBox::resetall() {}

void PressCalibBox::send_uart_data(FixturePacketData PacketData) {
    Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, PacketData.machineNumber);
}
