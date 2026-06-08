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

    // clang-format off
    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QLineEdit* getMacLineEdit() override { return ui->getMac; }
    QLineEdit* macInputLineEdit() override { return ui->macInput; }
    QPlainTextEdit* logEdit() override { return ui->log; }
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; }
    QTableWidget* testResultTable() override { return ui->testResultTable; }
    QLabel* getMesStateQlabel() override { return ui->mes_state; }
    QPushButton* getEndTestButton() override { return ui->stopTest; }
    // clang-format on

  private:
    double voltage = 0;
    double battary = 0;
    double standbattary = 0;
    int is_battary_test = 0;

    typedef enum {
        STATE_IDLE,         // 休眠状态
        STATE_WATI_CONNECT, // 等待连接
        STATE_WAIT_BANDING,
        STATE_WAIT_CORRECT_BANDING,
        STATE_WAIT_BANDING_SUBPID,
        STATE_WAIT_CORRECT_BANDING_SUBPID,
        STATE_WAIT_BANDING_SKUID,
        STATE_WAIT_CORRECT_BANDING_SKUID,
        STATE_DISABLE_SLEEP_1, // 进入禁止休眠
        STATE_GETBATTERY,
        STATE_PROCESS_INSPECTION, // 工序核对检查
        STATE_CHECK_FLASH,        // 检查flash资源
        STATE_AGE,
        STATE_AGE_CHECK,  // 等待老化完成
        STATE_SAVE_RESULT // 保存结果在本地
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
    void refreshPeriphData(ProtocolPeriphStateData) override;
    void refreshBattaryData(ProtocolBatteryData adc) override;
    void refreshDongleUartState(int state) override;
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();
    void processInspection(QString inputSnText);
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
    void send_start_test(int data);
};

#endif // AGEING_H
