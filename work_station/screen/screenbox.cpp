

#include "screenbox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "screentest.h"
#include "ui_screenbox.h"

#if _MSC_VER >= 1600

#pragma execution_character_set(push, "utf-8")
#endif

screenbox::screenbox(QWidget* parent) : box_base(parent), ui(new Ui::screenbox) {
    ui->setupUi(this);
    CreatWindow<screentest>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(new QLabel(SCREEN_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

screenbox::~screenbox() {
    delete ui;
}

#include <QDebug>

void screenbox::checkAllTest(int fixtureNumber) {
    qDebug() << "checkAllTest 被调用，fixtureNumber:" << fixtureNumber;

    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber >= testList.size()) {
        qDebug() << "无效的fixtureNumber，超出范围。";
        return;
    }

    FixTureStates[fixtureNumber] = 1;
    qDebug() << "更新了Fixture状态，FixTureStates:" << FixTureStates;

    if (checkStateReady(FixTureStates)) {
        qDebug() << "所有Fixture已准备好，正在重置状态。";
        for (int i = 0; i < testList.size(); ++i) {
            FixTureStates[i] = 0;
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "屏幕测试", "屏幕正常吗？", QMessageBox::Yes | QMessageBox::No);
        qDebug() << "用户的回答:" << (reply == QMessageBox::Yes ? "是" : "否");

        if (reply == QMessageBox::No) {
            bool ok;
            QString text =
                QInputDialog::getText(nullptr, "屏幕测试", "请输入坏屏号", QLineEdit::Normal, QString(), &ok);

            if (ok && !text.isEmpty()) {
                qDebug() << "用户输入的坏屏号:" << text;
                emit go_screen_next(text.toInt());
            } else {
                qDebug() << "警告: 用户没有输入错误的屏幕编号。";
            }
        } else {
            qDebug() << "用户确认屏幕没有问题，继续下一个步骤。";
            emit go_screen_next(0); // 没问题
        }
    }
}
