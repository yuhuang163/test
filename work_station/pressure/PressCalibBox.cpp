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

        if (SETTINGS.value("SYSTEM/PressWindow").toBool() &&
            SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() != 0) {  //木星治具是比较简单的不需要对话
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "压感校准测试", "是否开始", QMessageBox::Yes | QMessageBox::No);

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
// void PressCalibBox::send_uart_data(FixturePacketData PacketData) {
//     qDebug() << "开始发送到治具";
//     Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, PacketData.machineNumber);
// }
void PressCalibBox::send_uart_data(FixturePacketData PacketData) {
    qDebug() << "开始发送到治具" << PacketData.machineNumber << PacketData.machine_command_id
             << SETTINGS.value("Mes/Product_Name").toString();
    if ((SETTINGS.value("Mes/Product_Name").toString() == "Y20P" ||
         SETTINGS.value("Mes/Product_Name").toString() == "Y20PS") &&
        SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() != 2) {
        if (1)  //默认y20p非独立
        {
            qDebug() << "当前是"
                     << "Y20P";
            switch (PacketData.machine_command_id) {
                case COMMAND_ID_TRAY_IN:
                case COMMAND_ID_FIXED_BLOCK_DOWN:
                    for (int i = 0; i < testList.size(); i++) {
                        //只要有一个不在状态就返回
                        if (testList[i]->get_independent_state() < STATE_CALIB_STATIC_WAIT) {
                            qDebug() << "STATE_CALIB_STATIC_WAIT这个还不满足" << i;
                            return;
                        }
                    }
                    break;

                case COMMAND_ID_BTH_DOWN_200:
                    for (int i = 0; i < testList.size(); i++) {
                        if (testList[i]->get_independent_state() < STATE_CALIB_CH0) {
                            qDebug() << "STATE_CALIB_CH0这个还不满足" << i;
                            return;
                        }
                    }
                    break;

                case COMMAND_ID_KEY_DOWN_200:
                case COMMAND_ID_BTH_UP:
                    for (int i = 0; i < testList.size(); i++) {
                        if (testList[i]->get_independent_state() < STATE_CALIB_CH1) {
                            qDebug() << "STATE_CALIB_CH1这个还不满足" << i;
                            return;
                        }
                    }
                    break;

                case COMMAND_ID_KEY_UP:
                case COMMAND_ID_FIXED_BLOCK_UP:
                case COMMAND_ID_TRAY_OUT:
                    for (int i = 0; i < testList.size(); i++) {
                        if (testList[i]->get_independent_state() < STATE_CALIB_RESULT) {
                            qDebug() << "STATE_CALIB_RESULT这个还不满足" << i;
                            return;
                        }
                    }
                    break;
            }

            qint64 current_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
            qint64 last_sent_timestamp = Fixture_uart_ui->last_commid_timestamp;

            // 此处处理是为了等治具到位之后才进行下一步
            switch (Fixture_uart_ui->last_commid) {
                case COMMAND_ID_TRAY_IN:
                case COMMAND_ID_TRAY_OUT:
                    if (current_timestamp - last_sent_timestamp < 3000 && last_sent_timestamp != 0) {
                        Fixture_uart_ui->delay_msec(3000);
                    }
                    break;

                case COMMAND_ID_FIXED_BLOCK_DOWN:
                case COMMAND_ID_FIXED_BLOCK_UP:
                case COMMAND_ID_BTH_DOWN_200:
                case COMMAND_ID_KEY_DOWN_200:
                case COMMAND_ID_BTH_UP:
                case COMMAND_ID_KEY_UP:
                    if (current_timestamp - last_sent_timestamp < 2000 && last_sent_timestamp != 0) {
                        Fixture_uart_ui->delay_msec(2000);
                    }
                    break;
            }

            Fixture_uart_ui->last_commid = PacketData.machine_command_id;

            qDebug() << "machine_command_id" << PacketData.machine_command_id << PacketData.machineNumber;
            Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, 0);

            if (PacketData.machine_command_id == COMMAND_ID_FIXED_BLOCK_DOWN) {
                Fixture_uart_ui->delay_msec(3000);
                for (int i = 0; i < testList.size(); i++) {
                    testList[i]->set_independent_state(STATE_CALIB_STATIC_START);
                }
            }
        }
    } else {
        Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, PacketData.machineNumber);
    }
}
