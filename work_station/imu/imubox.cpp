#include "imubox.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#include "QDesktopWidget"
#include "ui_imubox.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

imubox::imubox(QWidget* parent) : box_base(parent), ui(new Ui::imubox) {
    ui->setupUi(this);
    CreatWindow<imucali>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    for (int i = 0; i < testList.size(); i++) {
        FixTureStates.push_back(0);  // 添加0到vector中
        stage1States.push_back(0);
        stage2States.push_back(0);
        stage3States.push_back(0);
        fixtureLeftStates.push_back(0);
        fixtureRightStates.push_back(0);
        fixtureDownStates.push_back(0);
        fixtureUpStates.push_back(0);
    }

    for (int i = 0; i < testList.size(); i++) {
        connect(testList[i], SIGNAL(send_kill_test(int)), this, SLOT(set_vector_state(int)));

        // 收集结束信号
        connect(testList[i], SIGNAL(stage1_ok(int)), this, SLOT(check_all_over_stage1(int)));
        connect(testList[i], SIGNAL(stage2_ok(int)), this, SLOT(check_all_over_stage2(int)));
        connect(testList[i], SIGNAL(stage3_ok(int)), this, SLOT(check_all_over_stage3(int)));
        connect(testList[i], SIGNAL(fixture_up(int)), this, SLOT(check_all_over_fixture_up(int)));
        connect(testList[i], SIGNAL(fixture_down(int)), this, SLOT(check_all_over_fixture_down(int)));
        connect(testList[i], SIGNAL(fixture_right(int)), this, SLOT(check_all_over_fixture_right(int)));
        connect(testList[i], SIGNAL(fixture_left(int)), this, SLOT(check_all_over_fixture_left(int)));
        connect(this, SIGNAL(send_fixture_log(QString)), testList[i], SLOT(print_fixture_log(QString)));
    }

    ui->statusbar->addPermanentWidget(new QLabel(IMU_VER + QString(__DATE__) + " " + QString(__TIME__)));
    ui->statusbar->setStyleSheet("QLabel { color: red; }");  // 将文本颜色设置为红色

    QAction* Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered, [=]() {
        if (Fixture_uart_ui == NULL) {
            Fixture_uart_ui = new Fixture_uart;
            connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_start()), this, SLOT(startTest()));
            Fixture_uart_ui->fixBaudRate = 115200;
            for (int i = 0; i < testList.size(); i++) {
                connect(Fixture_uart_ui, SIGNAL(send_data_to_mechine_imu(int)), testList[i], SLOT(set_fix_result(int)));
                connect(Fixture_uart_ui, SIGNAL(start_fix_action(int)), testList[i], SLOT(get_fix_action(int)));
            }

            QString masterFixturecomName = SETTINGS.value(QString("mechine/0/masterFixturecomName")).toString();
            Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(masterFixturecomName);
        }
        Fixture_uart_ui->raise();
        Fixture_uart_ui->show();
        Fixture_uart_ui->activateWindow();
    });

    QAction* end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); ++i) {
            mehineState[i] = 0;
            FixTureStates[i] = 0;
            stage1States[i] = 0;
            stage2States[i] = 0;
            stage3States[i] = 0;
            fixtureLeftStates[i] = 0;
            fixtureRightStates[i] = 0;
            fixtureDownStates[i] = 0;
            fixtureUpStates[i] = 0;
            qDebug() << "初始化vetorok";
        }

        set_cylinder_state(STATE_HOME);
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->endTask();
        }
    });

    QAction* use_mes_act = ui->menubar->addAction("开关MES");
    connect(use_mes_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); i++) {
            testList[i]->useMes();
        }
    });

    QAction* debug_act = ui->menubar->addAction("打印");
    connect(debug_act, &QAction::triggered, [=]() {
        for (int i = 0; i < testList.size(); i++) {
            qDebug() << "FixTureStates" << i << FixTureStates[i];
            qDebug() << "stage1States" << i << stage1States[i];
            qDebug() << "stage2States" << i << stage2States[i];
            qDebug() << "stage3States" << i << stage3States[i];
            qDebug() << "fixtureLeftStates" << i << fixtureLeftStates[i];
            qDebug() << "fixtureRightStates" << i << fixtureRightStates[i];
            qDebug() << "fixtureDownStates" << i << fixtureDownStates[i];
            qDebug() << "fixtureUpStates" << i << fixtureUpStates[i];

            // Creating the message to append
            QString message = QString("FixTureStates%1: %2\n"
                                      "stage1States%1: %3\n"
                                      "stage2States%1: %4\n"
                                      "stage3States%1: %5\n"
                                      "fixtureLeftStates%1: %6\n"
                                      "fixtureRightStates%1: %7\n"
                                      "fixtureDownStates%1: %8\n"
                                      "fixtureUpStates%1: %9\n"
                                      "mehineState%1: %10\n")
                                  .arg(i + 1)
                                  .arg(FixTureStates[i])
                                  .arg(stage1States[i])
                                  .arg(stage2States[i])
                                  .arg(stage3States[i])
                                  .arg(fixtureLeftStates[i])
                                  .arg(fixtureRightStates[i])
                                  .arg(fixtureDownStates[i])
                                  .arg(fixtureUpStates[i])
                                  .arg(mehineState[i]);

            // Append the message to the plain text edit
            testList[i]->msgEdit()->appendPlainText(message);
        }
    });

    setWindowTitle("IMU校准上位机");
}
void imubox::startTest() {
    for (int i = 0; i < testList.size(); i++) {
        qDebug() << "开始测试IMU校准上位机";
        testList[i]->startTest();
    }

    set_cylinder_state(STATE_RESET);
    waitWork(500);

    if (SETTINGS.value("SYSTEM/IMUCalibrationWakeup").toBool()) {
        if (pack.factory == "xwd") {
            set_cylinder_state(STATE_BRUSH_RIGHT);
        } else {
            set_cylinder_state(STATE_BRUSH_LEFT);  //可能是给华勤的
        }

        waitWork(1500);
    }

    set_cylinder_state(STATE_BRUSH_UP);
}

imubox::~imubox() {
    if (Fixture_uart_ui != NULL)
        SETTINGS.setValue(QString("mechine/0/masterFixturecomName"),
                          Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    delete Fixture_uart_ui;
    delete ui;
}
void imubox::resetall() {

    if (SETTINGS.value("SYSTEM/IMULastEnterStartTest").toBool()) {//表示p20p回车开始
        startTest();
    }


}
//把12个的其中1个杀死变成1，跟随测试
void imubox::set_vector_state(int state) {
    state = state - 1;
    if (state >= 0 && state <= 12) {
        stage1States[state] = 1;
        stage2States[state] = 1;
        stage3States[state] = 1;
        fixtureLeftStates[state] = 1;
        fixtureRightStates[state] = 1;
        fixtureDownStates[state] = 1;
        fixtureUpStates[state] = 1;
        qDebug() << "杀死mehineState变成" + QString::number(state);
        testList[state]->msgEdit()->appendPlainText("杀死mehineState变成" + QString::number(state));

        mehineState[state] = 1;
    } else {
        qDebug() << "set_vector_state函数的state超范围了";

        testList[state]->msgEdit()->appendPlainText("set_vector_state函数的state超范围了(state>=0&&state<=12)" +
                                                    QString::number(state));
    }
}

void imubox::check_all_over_fixture_up(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_fixture_up机号错误";
        return;
    }
    fixtureUpStates[testNumber] = 1;

    if (checkStateReady(fixtureUpStates)) {
        qDebug() << "check_all_over_fixture_up结束";
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_UP);
        for (int i = 0; i < testList.size(); ++i) {
            if (mehineState[i] == 0)
                fixtureUpStates[i] = 0;
        }
    }
}

void imubox::check_all_over_fixture_down(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_fixture_down机号错误";
        // emit send_fixture_log("牙刷90度测试完成");
        return;
    }
    fixtureDownStates[testNumber] = 1;

    if (checkStateReady(fixtureDownStates)) {
        qDebug() << "check_all_over_fixture_down结束";
        // emit send_fixture_log("所有牙刷90度测试完成,旋转90度");
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_DOWN);
        for (int i = 0; i < testList.size(); ++i) {
            if (mehineState[i] == 0)
                fixtureDownStates[i] = 0;
        }
    }
}

void imubox::check_all_over_fixture_right(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_fixture_right机号错误";
        // emit send_fixture_log("牙刷180度测试完成");
        return;
    }
    fixtureRightStates[testNumber] = 1;

    if (checkStateReady(fixtureRightStates)) {
        qDebug() << "check_all_over_fixture_right结束";
        // emit send_fixture_log("所有牙刷180度测试完成,旋转90度");
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_RIGHT);
        for (int i = 0; i < testList.size(); ++i) {
            if (mehineState[i] == 0)
                fixtureRightStates[i] = 0;
        }
    }
}
void imubox::check_all_over_fixture_left(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_fixture_left机号错误";
        // emit send_fixture_log("所有牙刷90度测试完成");
        return;
    }
    qDebug() << "收到check_all_over_fixture_left的testNumber" << testNumber;
    fixtureLeftStates[testNumber] = 1;

    if (checkStateReady(fixtureLeftStates)) {
        // emit send_fixture_log("所有牙刷90度测试完成");
        qDebug() << "check_all_over_fixture_left结束";
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_LEFT);
        for (int i = 0; i < testList.size(); ++i) {
            if (mehineState[i] == 0)
                fixtureLeftStates[i] = 0;
        }
    }
}
void imubox::check_all_over_stage1(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_stage1机号错误";
        return;
    }
    stage1States[testNumber] = 1;

    if (checkStateReady(stage1States)) {
        qDebug() << "阶段1结束";
        waitWork(300);
        set_cylinder_state(STATE_40);
        for (int i = 0; i < testList.size(); ++i) {
            if (mehineState[i] == 0) {
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
    }
}
void imubox::check_all_over_stage2(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_stage2机号错误";
        return;
    }
    stage2States[testNumber] = 1;
    if (checkStateReady(stage2States)) {
        waitWork(300);
        qDebug() << "阶段2结束";
        set_cylinder_state(STATE_FU40);
        for (int i = 0; i < testList.size(); ++i) {
            if (mehineState[i] == 0) {
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
    }
}
void imubox::check_all_over_stage3(int testNumber) {
    testNumber = testNumber - 1;
    if (testNumber < 0 || testNumber > testList.size()) {
        qDebug() << "check_all_over_stage3机号错误";
        return;
    }
    stage3States[testNumber] = 1;
    if (checkStateReady(stage3States)) {
        if (pack.factory == "lx") {
            waitWork(300);
            qDebug() << "阶段3结束";
            set_cylinder_state(STATE_BRUSH_UP);
            waitWork(300);
            set_cylinder_state(STATE_40);
        } else {
            waitWork(300);
            qDebug() << "阶段3结束";
            set_cylinder_state(STATE_BRUSH_UP);
            waitWork(300);
            set_cylinder_state(STATE_RESET);
        }

        for (int i = 0; i < testList.size(); ++i) {
            mehineState[i] = 0;
            FixTureStates[i] = 0;
            stage1States[i] = 0;
            stage2States[i] = 0;
            stage3States[i] = 0;
            fixtureLeftStates[i] = 0;
            fixtureRightStates[i] = 0;
            fixtureDownStates[i] = 0;
            fixtureUpStates[i] = 0;
            qDebug() << "初始化vetorok";
        }
    }
}

void imubox::checkAllover(int fixtureNumber) {
    fixtureNumber = fixtureNumber - 1;
    if (fixtureNumber < 0 || fixtureNumber > testList.size()) {
        return;
    }
    FixTureStates[fixtureNumber] = 1;
    if (checkStateReady(FixTureStates)) {
        waitWork(300);
        set_cylinder_state(STATE_BRUSH_UP);
        waitWork(300);
        set_cylinder_state(STATE_END);
        qDebug() << "测试结束";
        for (int i = 0; i < testList.size(); ++i) {
            FixTureStates[i] = 0;
            stage1States[i] = 0;
            stage2States[i] = 0;
            stage3States[i] = 0;
            fixtureLeftStates[i] = 0;
            fixtureRightStates[i] = 0;
            fixtureDownStates[i] = 0;
            fixtureUpStates[i] = 0;
            qDebug() << "checkAllover初始化vetorok";
        }

        testList[0]->getMacLineEdit()->setFocus();
    }
}

void imubox::set_cylinder_state(imuFixtureState state) {
    // emit send_fixture_log(QString::number(state));
    if (Fixture_uart_ui == NULL) {
        return;
    }

    if (state == STATE_START) {
        Fixture_uart_ui->sendimuData(STATE_START);
        qDebug() << "发送治具开始测试";
    }
    if (state == STATE_END) {
        Fixture_uart_ui->sendimuData(STATE_END);
        qDebug() << "发送治具结束测试";
    }

    if (state == STATE_RETURN) {
        Fixture_uart_ui->sendimuData(STATE_RETURN);
        qDebug() << "翻转180";
    }
    if (state == STATE_RESET) {
        Fixture_uart_ui->sendimuData(STATE_RESET);
        qDebug() << "恢复原位";
    }

    if (state == STATE_40) {
        Fixture_uart_ui->sendimuData(STATE_40);
        qDebug() << "刷头朝上40度";
    }

    if (state == STATE_FU40) {
        Fixture_uart_ui->sendimuData(STATE_FU40);
        qDebug() << "刷头朝下40度";
    }

    if (state == STATE_BRUSH_UP) {
        Fixture_uart_ui->sendimuData(STATE_BRUSH_UP);
        qDebug() << "刷头0度";
    }

    if (state == STATE_BRUSH_DOWN) {
        Fixture_uart_ui->sendimuData(STATE_BRUSH_DOWN);
        qDebug() << "刷头180度";
    }
    if (state == STATE_BRUSH_LEFT) {
        // emit send_fixture_log("刷头90度");
        Fixture_uart_ui->sendimuData(STATE_BRUSH_LEFT);
        qDebug() << "刷头90度";
    }
    if (state == STATE_BRUSH_RIGHT) {
        Fixture_uart_ui->sendimuData(STATE_BRUSH_RIGHT);
        qDebug() << "刷头270度";
    }

    if (state == STATE_HOME) {
        Fixture_uart_ui->sendimuData(STATE_HOME);
        qDebug() << "刷头270度";
    }
}
