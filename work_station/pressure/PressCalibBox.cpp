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

PressCalibBox::PressCalibBox(QWidget *parent)
    : box_base(parent), ui(new Ui::PressCalibBox)
{
    ui->setupUi(this);
    CreatWindow<PressureSensorForm>(this);
    signalAndslot();
    recoverCustom();
    ShowData(this);

    update_main_style("Ubuntu.qss");
    QVBoxLayout *vlayout = new QVBoxLayout(this->centralWidget());
    for (int i = 0; i < formRow; i++)
    {
        QHBoxLayout *hlayout = new QHBoxLayout(this);
        for (int j = 0; j < formColumn; j++)
        {
            int index = i * formColumn + j;
            PressureSensorForm *x = new PressureSensorForm(index, this);
            hlayout->addWidget(x);
            widgets << x;
        }
        vlayout->addLayout(hlayout);
    }

    for (int i = 0; i < widgets.size() - 1; i++)
    {
#ifdef FROM_SN
        connect(widgets[i], SIGNAL(gonextfocus()), widgets[i + 1]->ui->get_mac, SLOT(setFocus()));
#else
        connect(widgets[i], SIGNAL(gonextfocus()), widgets[i + 1]->ui->macInput, SLOT(setFocus()));
#endif
    }
#ifdef FROM_SN
    connect(widgets[widgets.size() - 1], SIGNAL(gonextfocus()), widgets[0]->ui->get_mac,
            SLOT(setFocus()));
#else
    connect(widgets[widgets.size() - 1], SIGNAL(gonextfocus()), widgets[0]->ui->macInput,
            SLOT(setFocus()));
#endif

    for (int i = 0; i < widgets.size(); i++)
    {

        // other
        display_state.push_back(0);
        connect(widgets[i], SIGNAL(operator_instruct(int, int)), this,
                SLOT(display_on_screen(int, int)));

        // mes
#ifdef HQSET
        connect(imucaliCurrentList[i], SIGNAL(send_testPass_hq(MesPacketData)), hq_mes,
                SLOT(testPass_hq(MesPacketData)));
        connect(imucaliCurrentList[i], SIGNAL(send_processInspection_hq(MesPacketData)), hq_mes,
                SLOT(processInspection_hq(MesPacketData)));
        connect(hq_mes, SIGNAL(operate_hqmes_error(const int, QString)), imucaliCurrentList[i],
                SLOT(solve_mes_data(const int, QString)));
        connect(hq_mes, SIGNAL(operate_hqmes_sucess(const int)), imucaliCurrentList[i],
                SLOT(solve_mes_sucess(const int)));
        connect(hq_mes, SIGNAL(send_hqmes_testvalue(const int, const QString)),
                imucaliCurrentList[i], SLOT(getTestvalue(const int, const QString)));
#elif defined(LXSET)
        connect(widgets[i], SIGNAL(send_testPass_lx(MesPacketData)), lx_mes,
                SLOT(testPass_lx(MesPacketData)));
        connect(widgets[i], SIGNAL(send_processInspection_lx(MesPacketData)), lx_mes,
                SLOT(processInspection_lx(MesPacketData)));
        connect(lx_mes, SIGNAL(operate_lxmes_error(const int, QString)), widgets[i],
                SLOT(solve_mes_data(const int, QString)));
        connect(lx_mes, SIGNAL(operate_lxmes_sucess(const int)), widgets[i],
                SLOT(solve_mes_sucess(const int)));
        connect(lx_mes, SIGNAL(send_lxmes_testvalue(const int, const QString)), widgets[i],
                SLOT(getTestvalue(const int, const QString)));
        connect(widgets[i], SIGNAL(send_getsn_lx(MesPacketData)), lx_mes,
                SLOT(getBindedMACID(MesPacketData)));

#else

        // mes的信号
        connect(widgets[i],
                SIGNAL(send_testPass(const int, const QString &, const QString &, const QString &,
                                     const QString &, const QString &, const QString &)),
                xwd_mes,
                SLOT(testPass(const int, const QString &, const QString &, const QString &,
                              const QString &, const QString &, const QString &)));
        connect(
            widgets[i],
            SIGNAL(send_processInspection(const int, const QString &, const QString &,
                                          const QString &)),
            xwd_mes,
            SLOT(processInspection(const int, const QString &, const QString &, const QString &)));
        connect(xwd_mes, SIGNAL(operate_sucess(const int)), widgets[i],
                SLOT(solve_mes_sucess(const int)));
        connect(xwd_mes, SIGNAL(operate_error(const int, QString)), widgets[i],
                SLOT(solve_mes_data(const int, QString)));
        connect(xwd_mes, SIGNAL(sendtestvalue(const int, const QString)), widgets[i],
                SLOT(getTestvalue(const int, const QString)));
        connect(widgets[i], SIGNAL(send_getTestData(const int, const QString&, const QString&,const QString&)), xwd_mes,
                SLOT(getTestData(const int,  const QString&, const QString&, const QString&)));

#endif
    }

    QAction *Fixture_connectl_act = ui->menubar->addAction("连接治具串口");
    connect(Fixture_connectl_act, &QAction::triggered,[=]()
            {
                if (Fixture_uart_ui == NULL)
                {
                    Fixture_uart_ui = new Fixture_uart;

                    for (int i = 0; i < widgets.size(); i++)
                    {
                        get_mac_state.push_back(0);   // 添加0到vector中
                        get_result_state.push_back(0);
#if Y20_Pro_PRESS_TEST
                        get_result_numb.push_back(0);
#endif

                        connect(Fixture_uart_ui, SIGNAL(receive_data_from_mechine(int)), widgets[i],
                                SLOT(receive_uart_data(int)));
                        connect(widgets[i], SIGNAL(send_data_to_mechine(FixturePacketData)), this,
                                SLOT(send_uart_data(FixturePacketData)));
                    }
                    QSettings settings(SETTINGS_NAME, QSettings::IniFormat);
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

    QSettings settings(SETTINGS_NAME, QSettings::IniFormat);
    M_USERNO = settings.value("Mes/M_USERNO").toString();
    M_PASSWORD = settings.value("Mes/M_PASSWORD").toString();
    M_MACHINENO = settings.value("Mes/M_MACHINENO").toString();

    pack.machineNo = M_MACHINENO;
    pack.password = M_PASSWORD;
    pack.userNo = M_USERNO;

#ifdef HQSET
    hq_mes->Login(pack);
#elif defined(LXSET)
    lx_mes->url =
        settings.value("Mes/NET", "http://10.16.204.138/Bobcat_CWS_TEST/sfc_response.aspx?")
            .toString();
#else
    xwd_mes->Login(M_USERNO, M_PASSWORD, M_MACHINENO);
#endif

    for (int i = 0; i < widgets.size(); ++i)
    {
        QString baseKey = QString("mechine/%1").arg(i);

        // 从设置中读取串口相关信息
        QString comName = settings.value(QString("%1/comName").arg(baseKey)).toString();
        // 将读取的信息填充到界面上
        widgets[i]->ui->comNameCombo->setCurrentText(comName);
    }

    ui->statusbar->addPermanentWidget(new QLabel(WINDOWS_NAME));
}

void PressCalibBox::display_on_screen(int instruct, int test_state)
{
    if (test_state == 1)
    {
        display_state[instruct] = 1;
    }
    else if (test_state == 0)
    {
        display_state[instruct] = 0;
    }


    for (int i = 0; i < widgets.size(); i++)
    {
        qDebug() << "widgets.size()" << widgets.size() << "i" << i << "display_state[i]"
                 << display_state[i];
        // if (display_state[i] != 1)
        // {
        //     return;
        // }
        if (widgets[i]->get_independent_state() != STATE_WAIT_START) {
            return;
        }
    }
    
#if U7_PRESS_TEST || P30_Pro_PRESS_TEST
    if (widgets[0]->ui->no_use_fixture->isChecked()) {
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
#else
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "压感校准测试", "是否开始测试", QMessageBox::Yes | QMessageBox::No);
    // reply = QMessageBox::Yes;
    if (reply == QMessageBox::Yes) {
        for (int i = 0; i < widgets.size(); i++) {
            widgets[i]->set_independent_state(STATE_CALIB_START);
        }
    } else if (reply == QMessageBox::No) {
        
    }
#endif
    
    // widgets[0]->ui->log->appendPlainText("请按下治具");
}

void PressCalibBox::update_main_style(QString style)
{
    // QSS文件初始化界面样式
    QString stylesheet;
    // QFile qss("qss/" + style);
    QFile qss(style);
    if (qss.open(QFile::ReadOnly))
    {
        qDebug() << "qss load";
        QTextStream filetext(&qss);
        stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    }
    else
    {
        qDebug() << "qss not load";
        qss.setFileName("/qss/" + style);
        if (qss.open(QFile::ReadOnly))
        {
            qDebug() << "qss load";
            QTextStream filetext(&qss);
            stylesheet = filetext.readAll();
            this->setStyleSheet(stylesheet);
            qss.close();
        }
    }
}

PressCalibBox::~PressCalibBox()
{
    delete ui;
}

void PressCalibBox::saveCustom()
{
    QSettings settings(SETTINGS_NAME, QSettings::IniFormat);
    settings.setValue("Window/Size", this->size());
    for (int i = 0; i < widgets.size(); i++)
    {
        QString baseKey = QString("mechine/%1").arg(i);
        // 保存COM口相关信息
        settings.setValue(QString("%1/comName").arg(baseKey),
                          widgets[i]->ui->comNameCombo->currentText());
    }
    if (Fixture_uart_ui != NULL)
    {
        settings.setValue(QString("mechine/FixturecomName"),
                          Fixture_uart_ui->ui->FixturecomNameCombo->currentText());
    }
}

void PressCalibBox::recoverCustom()
{
    QSettings settings(SETTINGS_NAME, QSettings::IniFormat);
    const QSize availableSize = QApplication::desktop()->availableGeometry(this).size();
    QVariant windowSize(availableSize / 4 * 3);
    this->resize(settings.value("Window/Size", windowSize).toSize());
    restoreState(settings.value("Window/windowState").toByteArray());
    bool ok = false;
    formColumn = settings.value("User/formColumn").toInt(&ok);
    if (!ok)
    {
        formColumn = 1;
    }

    formRow = settings.value("User/formRow").toInt(&ok);
    if (!ok)
    {
        formRow = 1;
    }
}

void PressCalibBox::resetall()
{
    for (int i = 0; i < widgets.size(); ++i)
    {
        get_mac_state[i] = 0;
        get_result_state[i] = 0;
    }
}

bool PressCalibBox::check_all_machine_ready(FixturePacketData packet_data)
{
    if (packet_data.machine_get_mac_state)
    {
        get_mac_state[packet_data.machine_index] = 1;
    }

    if (packet_data.machine_result_state)
    {
        get_result_state[packet_data.machine_index] = 1;
        get_result_numb[packet_data.machine_index] = packet_data.argument;
    }
#if Y20_Pro_PRESS_TEST
    switch (packet_data.machine_command_id)
    {
    case COMMAND_ID_BASE:
        for (int i = 0; i < widgets.size(); i++)
        {
            if (get_mac_state[i] != 1)
            {
                return false;
            }
        }
        return true;
        break;

    case COMMAND_ID_RESULT:
        for (int i = 0; i < widgets.size(); i++)
        {
            if (get_result_state[i] != 1)
            {
                return false;
            }
        }
        return true;
        break;

    case COMMAND_ID_RESET:
    case COMMAND_ID_MAX:
        break;
    }
#else
    return false;
#endif
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
        get_mac_state[PacketData.machine_index] = 0;
        get_result_state[PacketData.machine_index] = 0;
        get_result_numb[PacketData.machine_index] = 0;
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

void PressCalibBox::closeEvent(QCloseEvent *)
{
    isTestContinue = false;
    for (auto x : widgets)
    {
        x->close();
    }
    saveCustom();
}
void PressCalibBox::totally_task()
{
    while (isTestContinue)
    {
        for (int i = 0; i < widgets.size(); i++)
        {
            widgets[i]->startTask();
        }

        QCoreApplication::processEvents();
    }

    qDebug() << "已经退出主任务";
}
