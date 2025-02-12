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
#    pragma execution_character_set(push, "utf-8")
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
            Fixture_uart_ui->fixBaudRate = 115200;
            qDebug() << "testList.size" << testList.size();
            for (int i = 0; i < testList.size(); i++) {
                connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_press(int)), testList[i],
                        SLOT(receive_uart_data(int)));
                connect(testList[i], SIGNAL(send_data_to_mechine(FixturePacketData)), this,
                        SLOT(send_uart_data(FixturePacketData)));
                qDebug() << "已绑定治具";
            }

            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(
                SETTINGS.value("mechine/FixturecomName").toString());
        }
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    });

    QAction* end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered, [=]() {
        reset_display_state();
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->overTask();
        }
    });
}

void PressCalibBox::display_on_screen(int instruct) {
    instruct = instruct - 1;
    if (instruct < 0 || instruct > testList.size()) {
        return;
    }
    display_state[instruct] = 1;
    if (checkStateReady(display_state)) {
        for (int i = 0; i < testList.size(); i++) {
            qDebug() << "testList.size()" << testList.size() << "i" << i << "display_state[i]" << display_state[i];

            if (testList[i]->get_independent_state() != STATE_WAIT_START) {
                qDebug() << "还没都准备好";
                return;
            }
        }
        if (SETTINGS.value("SYSTEM/PressWindow").toBool()) {  //木星治具是比较简单的不需要对话
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "压感校准", "是否开始校准", QMessageBox::Yes | QMessageBox::No);

            if (reply == QMessageBox::Yes) {
                for (int i = 0; i < testList.size(); i++) {
                    testList[i]->set_independent_state(STATE_CALIB_START);
                }
            } else if (reply == QMessageBox::No) {
                qDebug() << "还没准备好，需要重新开始测试";
            }
        }
        reset_display_state();
    }

    // testList[0]->ui->log->appendPlainText("请按下治具");
}

PressCalibBox::~PressCalibBox() {
    if (Fixture_uart_ui != NULL)
        SETTINGS.setValue(QString("mechine/FixturecomName"), Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    delete ui;
}

void PressCalibBox::reset_display_state() {
    qDebug() << "复位display_state状态";
    for (int i = 0; i < testList.size(); i++) {
        display_state[i] = 0;
    }
}
void PressCalibBox::checkAllover(int fixtureNumber) {
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size()) {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates)) {
        for (int i = 0; i < testList.size(); ++i) {
            FixTureStates[i] = 0;
        }

        if (pack.factory == "无mes厂")
            testList[0]->macInputLineEdit()->setFocus();
        else
            testList[0]->getMacLineEdit()->setFocus();
        reset_display_state();
    }
}
void PressCalibBox::send_uart_data(FixturePacketData PacketData) {
    qDebug() << "开始发送到治具";
    Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, PacketData.machineNumber);
}
