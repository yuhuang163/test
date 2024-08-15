

#include "screenbox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "screentest.h"
#include "ui_screenbox.h"

#if _MSC_VER >= 1600

#    pragma execution_character_set("utf-8")
#endif

screenbox::screenbox(QWidget* parent) : box_base(parent), ui(new Ui::screenbox) {
    ui->setupUi(this);
    CreatWindow<screentest>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(new QLabel(SCREEN_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

screenbox::~screenbox() { delete ui; }

void screenbox::checkAllTest(int fixtureNumber) {
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
        reply = QMessageBox::question(this, "屏幕测试", "屏幕正常吗？", QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No) {
            bool ok;
            QString text =
                QInputDialog::getText(nullptr, "屏幕测试", "请输入坏屏号", QLineEdit::Normal, QString(), &ok);

            if (ok && !text.isEmpty()) {
                emit go_screen_next(text.toInt());
            } else {
                QMessageBox::warning(nullptr, "警告", "没有输入错误的屏幕");
            }
        } else {
            emit go_screen_next(0);  // 没问题
        }
    }
}
