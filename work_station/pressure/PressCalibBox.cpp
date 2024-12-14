#include "PressCalibBox.h"
#include "AbIni.h"
#include "QDesktopWidget"
#include "fixture_uart.h"
#include "ui_PressCalibBox.h"
#include "pressuresensorform.h"
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QVBoxLayout>

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

PressCalibBox::PressCalibBox(QWidget *parent) : box_base(parent), ui(new Ui::PressCalibBox)
{
    ui->setupUi(this);
    CreatWindow<PressureSensorForm>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    ui->statusbar->addPermanentWidget(new QLabel(PRESSURE_VER + QString(__DATE__) + " " + QString(__TIME__)));

    // for (int i = 0; i < widgets.size(); i++)
    // {

    //     // other
    //     connect(widgets[i], SIGNAL(operator_instruct(int, int)), this, SLOT(display_on_screen(int, int)));
    // }

    QAction *Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered,[=]()
            {
                if (Fixture_uart_ui == NULL)
                {
                    Fixture_uart_ui = new Fixture_uart;

                    for (int i = 0; i < testList.size(); i++)
                    {
                        // get_mac_state.push_back(0);   // 添加0到vector中
                        // get_result_state.push_back(0);
#if Y20_Pro_PRESS_TEST
                        // get_result_numb.push_back(0);
#endif

                        connect(Fixture_uart_ui, SIGNAL(receive_data_from_mechine(int)), testList[i],
                                SLOT(receive_uart_data(int)));
                        connect(testList[i], SIGNAL(send_data_to_mechine(FixturePacketData)), this,
                                SLOT(send_uart_data(FixturePacketData)));
                    }
                    QSettings settings(SETTING_NAME, QSettings::IniFormat);
                    Fixture_uart_ui->ui->FixturecomNameCombo->setCurrentText(
                        settings.value("mechine/FixturecomName").toString());
                }
                Fixture_uart_ui->show();
                Fixture_uart_ui->activateWindow();
            });

    QAction *end_test_act = ui->menubar->addAction("结束所有测试");
    connect(end_test_act, &QAction::triggered, [=]() {

        for(int i=0;i < widgets.size();i++ )
        {
            widgets[i]-> over_task();
        }

    });

    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    for (int i = 0; i < widgets.size(); ++i)
    {
        QString baseKey = QString("mechine/%1").arg(i);

        // 从设置中读取串口相关信息
        QString comName = settings.value(QString("%1/comName").arg(baseKey)).toString();
        // 将读取的信息填充到界面上
        widgets[i]->ui->comNameCombo->setCurrentText(comName);
    }
}

void PressCalibBox::display_on_screen(int instruct, int test_state)
{
    for (int i = 0; i < widgets.size(); i++)
    {
        if (widgets[i]->get_independent_state() != STATE_WAIT_START) {
            return;
        }
    }

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "压感校准测试", "是否开始测试", QMessageBox::Yes | QMessageBox::No);
    // reply = QMessageBox::Yes;
    if (reply == QMessageBox::Yes) {
        for (int i = 0; i < widgets.size(); i++) {
            widgets[i]->set_independent_state(STATE_CALIB_START);
        }
    } else if (reply == QMessageBox::No) {
        
    }
}

PressCalibBox::~PressCalibBox()
{
    delete ui;
}

void PressCalibBox::resetall()
{
//     for (int i = 0; i < widgets.size(); ++i)
//     {
//         get_mac_state[i] = 0;
//         get_result_state[i] = 0;
//     }
}

void PressCalibBox::send_uart_data(FixturePacketData PacketData)
{
#if Y20_Pro_PRESS_TEST
#if !IS_INDEPENDENT
    switch (PacketData.machine_command_id) {
        case COMMAND_ID_TRAY_IN:
        case COMMAND_ID_FIXED_BLOCK_DOWN:
            for (int i = 0; i < widgets.size(); i++) {
                if(widgets[i]->get_independent_state() < STATE_CALIB_STATIC_WAIT) {
                    return;
                }
            }
            break;

        case COMMAND_ID_BTH_DOWN_200:
            for (int i = 0; i < widgets.size(); i++) {
                if(widgets[i]->get_independent_state() < STATE_CALIB_CH0) {
                    return;
                }
            }
            break;

        case COMMAND_ID_KEY_DOWN_200:
        case COMMAND_ID_BTH_UP:
            for (int i = 0; i < widgets.size(); i++) {
                if(widgets[i]->get_independent_state() < STATE_CALIB_CH1) {
                    return;
                }
            }
            break;

        case COMMAND_ID_KEY_UP:
        case COMMAND_ID_FIXED_BLOCK_UP:
        case COMMAND_ID_TRAY_OUT:
            for (int i = 0; i < widgets.size(); i++) {
                if(widgets[i]->get_independent_state() < STATE_CALIB_RESULT) {
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


    qDebug() << "machine_command_id" <<PacketData.machine_command_id<<PacketData.machine_index;
    Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id,0);

    if (PacketData.machine_command_id == COMMAND_ID_FIXED_BLOCK_DOWN) {
        Fixture_uart_ui->delay_msec(3000);
        for (int i = 0; i < widgets.size(); i++) {
            widgets[i]->set_independent_state(STATE_CALIB_STATIC_START);
        }
    }
#else
    // 复位
    if (PacketData.machine_command_id == COMMAND_ID_BASE && PacketData.argument == 0)
    {
        // get_mac_state[PacketData.machine_index] = 0;
        // get_result_state[PacketData.machine_index] = 0;
        // get_result_numb[PacketData.machine_index] = 0;
        return;
    }

    // 检查mac是不是扫描完成
    if (PacketData.machine_command_id == COMMAND_ID_BASE && PacketData.argument == 1)
    {
        if (check_all_machine_ready(PacketData))
        {
            Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, 1);
        }
        return;
    }

    // 检查测试是否全部完成
    if (PacketData.machine_command_id == COMMAND_ID_RESULT)
    {
        if (check_all_machine_ready(PacketData))
        {
            uint numb = 0;
            for (int i = 0; i < widgets.size(); i++)
            {   // 计算成功个数
                if (get_result_numb[i])
                {
                    numb++;
                }
            }
            if (numb == 3)
            {   // 发送结果
                Fixture_uart_ui->send_command_to_machine(COMMAND_ID_RESULT_SUC, 0);
            }
            else
            {
                Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, numb);
            }
        }
        return;
    }

    Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id,
                                             PacketData.machine_index);
#endif

// #elif F20_PRESS_TEST
//     Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, PacketData.argument);
#else
    Fixture_uart_ui->send_command_to_machine(PacketData.machine_command_id, PacketData.machine_index);
#endif
}

