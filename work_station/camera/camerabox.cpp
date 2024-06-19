#include "camerabox.h"
#include "QDesktopWidget"
#include "ui_camerabox.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

camerabox::camerabox(QWidget *parent) : box_base(parent), ui(new Ui::camerabox)

{
    ui->setupUi(this);
    CreatWindow<cameratest>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    for (int i = 0; i < testList.size(); i++)
    {
        connect(this, SIGNAL(go_camera_next(int)), testList[i], SLOT(can_go_next(int)));
    }

    ui->statusbar->addPermanentWidget(
        new QLabel(CAMERA_VER + QString(__DATE__) + " " + QString(__TIME__)));
}

camerabox::~camerabox()
{
    delete ui;
}

void camerabox::checkAllTest(int fixtureNumber)
{
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size())
    {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates))
    {
        for (int i = 0; i < testList.size(); ++i)
        {
            FixTureStates[i] = 0;
        }

        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "摄像头测试", "摄像头正常吗？",
                                      QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::No)
        {
            bool ok;
            QString text = QInputDialog::getText(nullptr, "摄像头测试", "请输入坏的摄像头号",
                                                 QLineEdit::Normal, QString(), &ok);
            if (ok && !text.isEmpty())
            {
                emit go_camera_next(text.toInt());
            }
            else
            {
                QMessageBox::warning(nullptr, "警告", "没有输入错误的摄像头");
            }
        }
        else
        {
            emit go_camera_next(0);   // 没问题
        }
    }
}
