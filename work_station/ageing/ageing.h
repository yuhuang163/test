#ifndef AGEING_H
#define AGEING_H

#include "Abini.h"
#include "test_base.h"
#include "ui_ageing.h"

namespace Ui {
    class ageing;
}
class ageing : public test_base {
    Q_OBJECT
public:
    explicit ageing(int index, QWidget* parent = nullptr);
    ~ageing();
    Ui::ageing* ui;
    void startTask() override;
    void refreshBleState(int state) override;
    void show_product(QString name);
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

private:
    double voltage = 0;
    double standbattary = 0;
    int is_battary_test = 0;

    typedef enum {
        STATE_IDLE,          // 休眠状态
        STATE_WATI_CONNECT,  // 等待连接
        STATE_WAIT_BANDING,
        STATE_WAIT_CORRECT_BANDING,
        STATE_WAIT_BANDING_SUBPID,
        STATE_WAIT_CORRECT_BANDING_SUBPID,
        STATE_WAIT_BANDING_SKUID,
        STATE_WAIT_CORRECT_BANDING_SKUID,
        STATE_DISABLE_SLEEP_1,  // 进入禁止休眠
        STATE_GETBATTERY,
        STATE_PROCESS_INSPECTION,  // 工序核对检查
        STATE_CHECK_FLASH,         // 检查flash资源
        STATE_AGE,                 // 开始老化
        STATE_SAVE_RESULT          // 保存结果在本地
    } State;
    State state = STATE_IDLE;
    QString stringsn;
    QString stringsubpid;
    QString stringSkuid;
    QString macAddress = "没有mac地址";
    QByteArray writesn;
    QByteArray last_sn = QByteArray::number(22);
    QByteArray writesubpid;

    int snCompareOk = 0;
    int skuCompareOk = 0;
    int subpidCompareOk = 0;
    int flash_state = 0;
    int refresh_periph_times;
    QElapsedTimer TestTime;

private slots:

    void refreshSn(ProtocolSnData data) override;
    void refreshPeriphData(FacGetPeriphState) override;
    void refreshBattaryData(FacDevInfo adc) override;
    void refreshDongleUartState(int state) override;
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();
    void processInspection(QString stringsn);
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();

    void on_macInput_returnPressed();

    void on_pushButton_clicked();
    void on_enterBurningMode_clicked();
    void on_exitBurningMode_clicked();
    void on_getMac_returnPressed();
    void on_snInput_returnPressed();

    void on_stopTest_clicked();

signals:

    void send_go_next_test(int data);

    void send_go_next_focus();
    void send_startTest(int data);
};

#endif  // AGEING_H
