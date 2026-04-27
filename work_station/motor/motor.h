#ifndef MOTOR_H
#define MOTOR_H

#include "Abini.h"
#include "test_base.h"
#include "ui_motor.h"

namespace Ui {
    class motor;
}

class motor : public test_base {
    Q_OBJECT
public:
    explicit motor(int index, QWidget* parent = nullptr);
    ~motor();
    Ui::motor* ui;
    void startTask() override;
    void startTest_task();
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QLineEdit* getMacLineEdit() override { return ui->getMac; };               // sn输入口
    QLineEdit* macInputLineEdit() override { return ui->macInput; };           // mac地址输入口
    QPlainTextEdit* logEdit() override { return ui->log; };                    // mac地址输入口
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; };                // msg输入口
    QTableWidget* testResultTable() override { return ui->testResultTable; };  // 测试结果表格输入口
    QLabel* getMesStateQlabel() override { return ui->mes_state; };            // mes状态的qlab
    QPushButton* getEndTestButton() override { return ui->stopTest; };         // 结束测试按钮

private slots:

    void refreshBaseData(ProtocolBaseInfoData data) override;
    void processInspection(QString stringsn);
    void refreshMotorCaliMsg(QString msg) override;
    void control_motor_cmd(QString cmd);
    void canGoNextMechine(int x) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshBleState(int state) override;
    void refreshDongleUartState(int state) override;
    void refreshBattaryData(ProtocolBatteryData adc) override;
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_pushButton_2_clicked();
    void on_macInput_returnPressed();
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();
    void on_motor_cali_clicked();
    void refreshPbData(QString data) override;
    void on_damping_open_clicked();
    void on_damping_close_clicked();
    void on_getMac_returnPressed();
    void on_end_cali_clicked();
    void on_stopTest_clicked();

private:
    typedef enum {
        STATE_IDLE,          // 休眠状态
        STATE_WATI_CONNECT,  // 等待连接
        STATE_GETBASEDATA,   // 获取基本信息
        STATE_WATI_CORRECT_BATTARY,
        STATE_DISABLE_SLEEP_1,  // 进入禁止休眠
        STATE_SN_CHECK,
        UNLOCK_DAMPING,
        MOTOR_CALI1,
        MOTOR_WAIT_CALI1,
        MOTOR_CALI2,
        MOTOR_WAIT_CALI2,
        STOP_MOTOR_CALI,
        MOTOR_TESTING,
        MOTOR_WAIT_TESTING,
        STATE_SAVE_RESULT  // 保存结果在本地
    } State;
    QElapsedTimer TestTime;
    State state = STATE_IDLE;
    State test_state = STATE_IDLE;
    QByteArray sn;
    QString test_result = "";
    int is_battary_test = 0;
    double standbattary = 0;
    bool is_canGoNext = 0;
    bool is_motor_test_continue = 0;
    QString stringsn;
    QString totalresult = "";
    QString macAddress = "没有mac地址";
    QMap<QString, QMap<QString, QString>> deviceMap;  // 存储设备信息

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
};

#endif  // MOTOR_H
