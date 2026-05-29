#include "pcbabox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "ui_pcbabox.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif
pcbabox::pcbabox(QWidget* parent) :
    box_base(parent), ui(new Ui::pcbabox)

{
    ui->setupUi(this);
    CreatWindow<PcbaForm>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    ui->statusbar->addPermanentWidget(
        new QLabel(PCBA_VER + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));

    QAction* Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered, [=]() {
        if (Fixture_uart_ui == NULL) {
            Fixture_uart_ui = new Fixture_uart;
            connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this, SLOT(startTest()));
            for (int i = 0; i < testList.size(); i++) {
                connect(testList[i], SIGNAL(overtask(int)), Fixture_uart_ui, SLOT(send_start_command(int)));
                connect(testList[i], SIGNAL(start_sleep_command(int)), Fixture_uart_ui,
                        SLOT(send_start_sleep_command(int)));
                connect(testList[i], SIGNAL(start_white_modle_command(int)), Fixture_uart_ui,
                        SLOT(send_start_white_modle_command(int)));
                connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine(const FixturePacketData)), testList[i],
                        SLOT(get_remain_data(const FixturePacketData)));
                connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_sleep(const FixturePacketData)), testList[i],
                        SLOT(get_remain_data_sleep(const FixturePacketData)));
            }

            QString masterFixturecomName = SETTINGS.value(QString("mechine/0/masterFixturecomName")).toString();
            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(masterFixturecomName);

            // Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(SETTINGS.value("mechine/FixturecomName").toString());
        }
        // 注册类型
        qRegisterMetaType<FixturePacketData>("FixturePacketData");
        Fixture_uart_ui->raise();
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    });

    QAction* startTest_act = ui->menubar->addAction("开始测试");
    connect(startTest_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->startTest();
        }
    });

    QAction* end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->overTask();
        }
        testList[0]->getMacLineEdit()->setFocus();
    });

    QAction* clear_sn_act = ui->menubar->addAction("清空所有sn");
    connect(clear_sn_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->getMacLineEdit()->clear();
        }
        testList[0]->getMacLineEdit()->setFocus();
    });
    if (pack.factory == "无mes厂")
        testList[0]->macInputLineEdit()->setFocus();
}
//用于治具的开始测试
void pcbabox::startTest() {
    for (int i = 0; i < testList.size(); i++) {
        testList[i]->startTest();
    }
}

pcbabox::~pcbabox() {
    if (Fixture_uart_ui != NULL)
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"),
                          Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    delete ui;
}
