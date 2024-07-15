#ifndef MOTOR_H
#define MOTOR_H

#include "Abini.h"
#include "test_base.h"
#include "ui_motor.h"

namespace Ui
{
    class motor;
}

class motor : public test_base
{
    Q_OBJECT
public:
    explicit motor(int index, QWidget *parent = nullptr);
    ~motor();
    Ui::motor *ui;
    void start_task() override;

    void start_test_task();
    QComboBox *getComNameCombo() override
    {
        return ui->comNameCombo;
    };   // dongle口

    QLineEdit *getMacLineEdit() override
    {
        return ui->get_mac;
    };   // sn输入口
    QLineEdit *macInputLineEdit() override
    {
        return ui->macInput;
    };   // mac地址输入口
    QPlainTextEdit *logEdit() override
    {
        return ui->log;
    };   // mac地址输入口
    QPlainTextEdit *msgEdit() override
    {
        return ui->msgEdit;
    };   // msg输入口
private slots:
    void get_dongle_ver(QString data) override;

    void showlog(QString msg);
    void refresh_base_data(FacGetDevBaseInfo data) override;
    void processInspection(QString stringsn);
    void refresh_motor_cali_msg(QString msg) override;
    void control_motor_cmd(QString cmd);
    void solveMesData(const int mechines, QString msg);
    void solveMesSucess(const int mechines);
    void refreshMesState(int state);
    void refresh_sn(FacDevInfo data) override;

    void refresh_ble_state(int state) override;
    void refresh_dongle_uart_state(int state)override;
    void refresh_battary_data(FacDevInfo adc);

    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_pushButton_2_clicked();
    void on_macInput_returnPressed();
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();
    void on_motor_cali_clicked();
    void refresh_pb_data(QString data)override;
    void on_damping_open_clicked();
    void on_damping_close_clicked();
    void on_get_mac_returnPressed();
    void get_mac(QString sn_to_search);
    void on_end_cali_clicked();

    void on_stopTest_clicked();
private:
    typedef enum
    {
        STATE_IDLE,              // 休眠状态
        STATE_WATI_CONNECT,      // 等待连接
        STATE_GETBASEDATA,       // 获取基本信息
        STATE_WATI_CORRECT_BATTARY,
        STATE_DISABLE_SLEEP_1,   // 进入禁止休眠
        STATE_SN_CHECK,
        UNLOCK_DAMPING,
        MOTOR_CALI1,
        MOTOR_CALI2,
        STOP_MOTOR_CALI,
        MOTOR_TESTING,
        STATE_SAVE_RESULT   // 保存结果在本地
    } State;
    QTime TestTime;
    State state = STATE_IDLE;
    State test_state = STATE_IDLE;
    QByteArray sn;
    QString result = "";
    QString test_result = "";
    QString passValue = "通过";
    QString failValue = "失败";
    int is_battary_test = 0;
    double standbattary = 0;

    bool is_motor_continue = 0;
    bool is_motor_test_continue = 0;
    int m_index;
    int getIndex() const
    {
        return m_index;
    }
    bool mes_set_ok = 0;


          // 动作

    QString stringsn;
    QString totalresult = "";
    QString macAddress = "没有mac地址";
    QMap<QString, QMap<QString, QString>> deviceMap;   // 存储设备信息

signals:
    void goNextFocus();
    void endTest(int data);
    void startTest(int data);
    void goNextTest(int data);

};

#endif   // MOTOR_H
