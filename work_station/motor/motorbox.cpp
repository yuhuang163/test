
#include "motorbox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "ui_motorbox.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
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
    // Fixture_uart_ui->raise();
    //      Fixture_uart_ui->activateWindow();

    //  });
    // connect(testList[testList.size() - 1]->getMacLineEdit(), SIGNAL(returnPressed()), this,
    //         SLOT(startAllReturnPressed()));

    connect(testList[testList.size() - 1]->getMacLineEdit(), SIGNAL(returnPressed()), this, SLOT(resetValue()));
}
void motorbox::resetValue() {
    motor_cali_stage = 1;
}
motorbox::~motorbox() {
    // delete Fixture_uart_ui;

    delete ui;
}

void motorbox::checkAllTest(int fixtureNumber) {
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size()) {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates)) {
        for (int i = 0; i < testList.size(); ++i) {
            FixTureStates[i] = 0;
        }

        if (motor_cali_stage == 1) {
            if (pack.factory == "lx") {
                motor_cali_stage++;
            } else {
                motor_cali_stage = 3;
            }
            QMessageBox::warning(NULL, "警告", " 请把所有电机置于0位\t\r\n");
            qDebug() << "弹窗：请把所有电机置于0位";
            emit go_screen_next(0); // 没问题
            return;
        }

        if (motor_cali_stage == 2) {
            motor_cali_stage++;
            QMessageBox::warning(NULL, "警告", " 校准完成,请取出电机\t\r\n");
            qDebug() << "弹窗：校准完成,请取出电机";
            emit go_screen_next(0); // 没问题
            return;
        }

        if (motor_cali_stage == 3) {
            motor_cali_stage = 1;
            QMessageBox::StandardButton reply;
            reply = QMessageBox::question(this, "电机测试", "电机是否都震动吗？", QMessageBox::Yes | QMessageBox::No);

            qDebug() << "弹窗：电机是否都震动吗？";
            if (reply == QMessageBox::No) {
                bool ok;
                QString text =
                    QInputDialog::getText(nullptr, "电机测试", "请输入不良机号", QLineEdit::Normal, QString(), &ok);

                if (ok && !text.isEmpty()) {
                    emit go_screen_next(text.toInt());
                } else {
                    QMessageBox::warning(nullptr, "警告", "没有输入不良机号");
                }
            } else {
                qDebug() << "发送没问题";

                emit go_screen_next(0); // 没问题
            }
            return;
        }
    }
}
