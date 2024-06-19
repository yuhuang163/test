#include "imubox.h"
#include "QDesktopWidget"
#include "ui_imubox.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

imubox::imubox(QWidget *parent) : box_base(parent), ui(new Ui::imubox)
{
    ui->setupUi(this);
    CreatWindow<imucali>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    for (int i = 0; i < testList.size(); i++)
    {
        FixTureStates.push_back(0);   // 添加0到vector中
        stage1States.push_back(0);
        stage2States.push_back(0);
        stage3States.push_back(0);
        fixtureLeftStates.push_back(0);
        fixtureRightStates.push_back(0);
        fixtureDownStates.push_back(0);
        fixtureUpStates.push_back(0);
    }
    for (int i = 0; i < testList.size(); i++)
    {
        testStates.push_back(0);   // 添加0到vector中
    }

    for (int i = 0; i < testList.size(); i++)
    {
        // 收集结束信号
        connect(testList[i], SIGNAL(endcali(int)), this, SLOT(checkAllover(int)));
        connect(testList[i], SIGNAL(endTest(int)), this, SLOT(check_all_over_test(int)));
        connect(testList[i], SIGNAL(stage1_ok(int)), this, SLOT(check_all_over_stage1(int)));
        connect(testList[i], SIGNAL(stage2_ok(int)), this, SLOT(check_all_over_stage2(int)));
        connect(testList[i], SIGNAL(stage3_ok(int)), this, SLOT(check_all_over_stage3(int)));
        connect(testList[i], SIGNAL(fixture_up(int)), this, SLOT(check_all_over_fixture_up(int)));
        connect(testList[i], SIGNAL(fixture_down(int)), this,
                SLOT(check_all_over_fixture_down(int)));

        connect(testList[i], SIGNAL(fixture_right(int)), this,
                SLOT(check_all_over_fixture_right(int)));

        connect(testList[i], SIGNAL(fixture_left(int)), this,
                SLOT(check_all_over_fixture_left(int)));

        connect(this, SIGNAL(send_fixture_log(QString)), testList[i],
                SLOT(print_fixture_log(QString)));
    }

    ui->statusbar->addPermanentWidget(
        new QLabel(IMU_VER + QString(__DATE__) + " " + QString(__TIME__)));
    ui->statusbar->setStyleSheet("QLabel { color: red; }");   // 将文本颜色设置为红色

    QAction *Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered,
            [=]()
            {
                if (Fixture_uart_ui == NULL)
                {
                    Fixture_uart_ui = new Fixture_uart;

                    for (int i = 0; i < testList.size(); i++)
                    {
                        connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_imu(int)), testList[i],
                                SLOT(set_fix_result(int)));
                    }
                    QSettings settings(SETTING_NAME, QSettings::IniFormat);
                    Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(
                        settings.value("mechine/FixturecomName").toString());
                }
                Fixture_uart_ui->show();
                Fixture_uart_ui->activateWindow();
            });

    QAction *start_test_act = ui->menubar->addAction("开始测试");
    connect(start_test_act, &QAction::triggered,
            [=]()
            {
                set_cylinder_state(STATE_START);
                for (int i = 0; i < testList.size(); i++)
                {
                    testList[i]->macInputLineEdit()->returnPressed();
                }
            });

    QAction *end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered,
            [=]()
            {
                set_cylinder_state(STATE_HOME);
                for (int i = 0; i < testList.size(); i++)
                {
                    testList[i]->endTask();
                }
            });

#ifdef NEW_IMU_CALI
    setWindowTitle("三轴校准上位机");
#else
    setWindowTitle("全新六轴校准上位机");
#endif

    // 最后一个回车会清空所有状态重新开始测试
    connect(testList[testList.size() - 1]->macInputLineEdit(), SIGNAL(returnPressed()), this,
            SLOT(resetall()));
}

imubox::~imubox()
{
    delete ui;
}
void imubox::resetall()
{
    set_cylinder_state(STATE_RESET);
    for (int i = 0; i < testList.size(); ++i)
    {
        testStates[i] = 0;
        FixTureStates[i] = 0;
        stage1States[i] = 0;
        stage2States[i] = 0;
        stage3States[i] = 0;
        fixtureLeftStates[i] = 0;
        fixtureRightStates[i] = 0;
        fixtureDownStates[i] = 0;
        fixtureUpStates[i] = 0;
    }
}
void imubox::check_all_over_fixture_up(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_fixture_up机号错误";
        return;
    }
    fixtureUpStates[testNumber] = 1;

    if (checkstateReady(fixtureUpStates))
    {
        qDebug() << "check_all_over_fixture_up结束";
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_UP);
        for (int i = 0; i < testList.size(); ++i)
        {
            fixtureUpStates[i] = 0;
        }
    }
}

void imubox::check_all_over_fixture_down(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_fixture_down机号错误";
        // emit send_fixture_log("牙刷90度测试完成");
        return;
    }
    fixtureDownStates[testNumber] = 1;

    if (checkstateReady(fixtureDownStates))
    {
        qDebug() << "check_all_over_fixture_down结束";
        // emit send_fixture_log("所有牙刷90度测试完成,旋转90度");
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_DOWN);
        for (int i = 0; i < testList.size(); ++i)
        {
            fixtureDownStates[i] = 0;
        }
    }
}

void imubox::check_all_over_fixture_right(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_fixture_right机号错误";
        // emit send_fixture_log("牙刷180度测试完成");
        return;
    }
    fixtureRightStates[testNumber] = 1;

    if (checkstateReady(fixtureRightStates))
    {
        qDebug() << "check_all_over_fixture_right结束";
        // emit send_fixture_log("所有牙刷180度测试完成,旋转90度");
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_RIGHT);
        for (int i = 0; i < testList.size(); ++i)
        {
            fixtureRightStates[i] = 0;
        }
    }
}
void imubox::check_all_over_fixture_left(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_fixture_left机号错误";
        // emit send_fixture_log("所有牙刷90度测试完成");
        return;
    }
    fixtureLeftStates[testNumber] = 1;

    if (checkstateReady(fixtureLeftStates))
    {
        // emit send_fixture_log("所有牙刷90度测试完成");
        qDebug() << "check_all_over_fixture_left结束";
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_LEFT);
        for (int i = 0; i < testList.size(); ++i)
        {
            fixtureLeftStates[i] = 0;
        }
    }
}
void imubox::check_all_over_stage1(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_stage1机号错误";
        return;
    }
    stage1States[testNumber] = 1;

    if (checkstateReady(stage1States))
    {
        qDebug() << "阶段1结束";
        waitWork(300);
        set_cylinder_state(STATE_40);
        for (int i = 0; i < testList.size(); ++i)
        {
            stage1States[i] = 0;
        }
    }
}
void imubox::check_all_over_stage2(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_stage2机号错误";
        return;
    }
    stage2States[testNumber] = 1;
    if (checkstateReady(stage2States))
    {
        waitWork(300);
        qDebug() << "阶段2结束";
        set_cylinder_state(STATE_FU40);
        for (int i = 0; i < testList.size(); ++i)
        {
            stage2States[i] = 0;
        }
    }
}
void imubox::check_all_over_stage3(int testNumber)
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        qDebug() << "check_all_over_stage3机号错误";
        return;
    }
    stage3States[testNumber] = 1;
    if (checkstateReady(stage3States))
    {
        if (pack.factory == "lx")
        {
            waitWork(300);
            qDebug() << "阶段3结束";
            set_cylinder_state(STATE_BRUSH_UP);
            waitWork(300);
            set_cylinder_state(STATE_40);
        }
        else
        {
            waitWork(300);
            qDebug() << "阶段3结束";
            set_cylinder_state(STATE_BRUSH_UP);
            waitWork(300);
            set_cylinder_state(STATE_40);
        }

        for (int i = 0; i < testList.size(); ++i)
        {
            stage3States[i] = 0;
        }
    }
}


void imubox::check_all_over_test(int testNumber)   // 结束测试最后结束
{
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size())
    {
        return;
    }
    testStates[testNumber] = 1;
    if (checkAlltestReady())
    {
        set_cylinder_state(STATE_END);
        qDebug() << "测试结束";
        testList[0]->getMacLineEdit()->setFocus();

        // set_cylinder_state(STATE_HOME);

        for (int i = 0; i < testList.size(); ++i)
        {
            testStates[i] = 0;
        }
    }
}

bool imubox::checkstateReady(std::vector<int> States)
{
    for (int i = 0; i < testList.size(); ++i)
    {
        if (States[i] != 1)
        {
            return false;
        }
    }
    return true;
}

bool imubox::checkAlltestReady()
{
    for (int i = 0; i < testList.size(); ++i)
    {
        if (testStates[i] != 1)
        {
            return false;
        }
    }
    return true;
}

void imubox::set_cylinder_state(imuFixtureState state)
{
    // emit send_fixture_log(QString::number(state));
    if (Fixture_uart_ui == NULL)
    {
        return;
    }

    if (state == STATE_START)
    {
        Fixture_uart_ui->sendimuData(STATE_START);
        qDebug() << "发送治具开始测试";
    }
    if (state == STATE_END)
    {
        Fixture_uart_ui->sendimuData(STATE_END);
        qDebug() << "发送治具结束测试";
    }

    if (state == STATE_RETURN)
    {
        Fixture_uart_ui->sendimuData(STATE_RETURN);
        qDebug() << "翻转180";
    }
    if (state == STATE_RESET)
    {
        Fixture_uart_ui->sendimuData(STATE_RESET);
        qDebug() << "恢复原位";
    }

    if (state == STATE_40)
    {
        Fixture_uart_ui->sendimuData(STATE_40);
        qDebug() << "刷头朝上40度";
    }

    if (state == STATE_FU40)
    {
        Fixture_uart_ui->sendimuData(STATE_FU40);
        qDebug() << "刷头朝下40度";
    }

    if (state == STATE_BRUSH_UP)
    {
        Fixture_uart_ui->sendimuData(STATE_BRUSH_UP);
        qDebug() << "刷头0度";
    }

    if (state == STATE_BRUSH_DOWN)
    {
        Fixture_uart_ui->sendimuData(STATE_BRUSH_DOWN);
        qDebug() << "刷头180度";
    }
    if (state == STATE_BRUSH_LEFT)
    {
        // emit send_fixture_log("刷头90度");
        Fixture_uart_ui->sendimuData(STATE_BRUSH_LEFT);
        qDebug() << "刷头90度";
    }
    if (state == STATE_BRUSH_RIGHT)
    {
        Fixture_uart_ui->sendimuData(STATE_BRUSH_RIGHT);
        qDebug() << "刷头270度";
    }

    if (state == STATE_HOME)
    {
        Fixture_uart_ui->sendimuData(STATE_HOME);
        qDebug() << "刷头270度";
    }
}
void imubox::set_relay_state(int state)
{
    if (Fixture_uart_ui == NULL)
    {
        return;
    }
    if (state == 1)
    {
        Fixture_uart_ui->sendFixtureData(STATE_RELAY1_OPEN);
        waitWork(200);
        Fixture_uart_ui->sendFixtureData(STATE_RELAY1_RESET);
    }
    if (state == 4)
    {
        Fixture_uart_ui->sendFixtureData(STATE_RELAY4_OPEN);
        waitWork(200);
        Fixture_uart_ui->sendFixtureData(STATE_RELAY4_RESET);
    }
}
