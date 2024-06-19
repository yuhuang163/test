#include "pcbabox.h"
#include "QDesktopWidget"
#include "ui_pcbabox.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
pcbabox::pcbabox(QWidget *parent) : box_base(parent), ui(new Ui::pcbabox)

{
    ui->setupUi(this);

    CreatWindow<PcbaForm>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);
    ui->statusbar->addPermanentWidget(new QLabel(
        "电刷 PCBA 测试 V1.2.3 " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")));

    QAction *Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered,
            [=]()
            {
                if (Fixture_uart_ui == NULL)
                {
                    Fixture_uart_ui = new Fixture_uart;
                    connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this,
                            SLOT(start_test()));
                    for (int i = 0; i < testList.size(); i++)
                    {
                        connect(testList[i], SIGNAL(end_total(int)), this, SLOT(checkover(int)));
                        connect(testList[i], SIGNAL(overtask(int)), Fixture_uart_ui,
                                SLOT(send_start_command(int)));
                        connect(testList[i], SIGNAL(start_sleep_command(int)), Fixture_uart_ui,
                                SLOT(send_start_sleep_command(int)));
                        connect(testList[i], SIGNAL(start_white_modle_command(int)),
                                Fixture_uart_ui, SLOT(send_start_white_modle_command(int)));
                        connect(Fixture_uart_ui,
                                SIGNAL(send_data_to_mechine(const FixturePacketData)), testList[i],
                                SLOT(get_remain_data(const FixturePacketData)));
                        connect(Fixture_uart_ui,
                                SIGNAL(send_data_to_mechine_sleep(const FixturePacketData)),
                                testList[i], SLOT(get_remain_data_sleep(const FixturePacketData)));
                    }
                    QSettings settings(SETTING_NAME, QSettings::IniFormat);
                    QString masterFixturecomName =
                        settings.value(QString("0/masterFixturecomName")).toString();
                    Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(masterFixturecomName);

                    // Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(settings.value("mechine/FixturecomName").toString());
                }
                Fixture_uart_ui->show();
                Fixture_uart_ui->activateWindow();
            });

    QAction *start_test_act = ui->menubar->addAction("开始测试");
    connect(start_test_act, &QAction::triggered,
            [=]()
            {
                for (int i = 0; i < testList.size(); i++)
                {
                    testList[i]->start_test();
                }
            });

    QAction *end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered,
            [=]()
            {
                for (int i = 0; i < testList.size(); i++)
                {
                    testList[i]->over_task();
                }
            });
}

void pcbabox::start_test()
{
    for (int i = 0; i < testList.size(); i++)
    {
        testList[i]->start_test();
    }
}

pcbabox::~pcbabox()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    if (Fixture_uart_ui != NULL)
        settings.setValue(QString("0/masterFixturecomName"),
                          Fixture_uart_ui->ui->FixturecomNameCombo->currentText());

    delete ui;
}
