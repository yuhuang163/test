
#ifndef SCREENTEST_H
#define SCREENTEST_H

#include "Abini.h"
#include "test_base.h"
#include "ui_screentest.h"

namespace Ui {
class screentest;
}
class screentest : public test_base {
    Q_OBJECT
  public:
    explicit screentest(int index, QWidget* parent = nullptr);
    ~screentest();
    Ui::screentest* ui;
    void startTask() override;

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
    typedef enum {
        STATE_IDLE,         // 休眠状态
        STATE_WATI_CONNECT, // 等待连接
        STATE_WATI_DISABLE_SLEEP,
        STATE_WAIT_CORRECT_BANDING,
        STATE_WAIT_BANDING_SUBPID,
        STATE_WAIT_CORRECT_BANDING_SUBPID,
        STATE_DISABLE_SLEEP_1,    // 进入禁止休眠
        STATE_PROCESS_INSPECTION, // 工序核对检查
        STATE_COLOR1,
        STATE_COLOR2,
        STATE_COLOR3,
        STATE_COLOR4,
        STATE_COLOR5,
        STATE_SAVE_RESULT // 保存结果在本地
    } State;
    State state = STATE_IDLE;
    QElapsedTimer TestTime;
    bool is_canGoNext = 0;
    QString stringsn;
    QString macAddress = "没有mac地址";
    bool is_lcd_control = 0;
    int snCompareOk = 0;
    QByteArray subpid;
    QString stringsubpid;
    int subpidCompareOk = 0;
    QByteArray writesn;
    QString product = "";
    QByteArray writesubpid;

  private slots:
    void refreshLcdControl(ProtocolLcdControlData style) override;
    void refreshBleState(int state) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshDongleUartState(int state) override;
    void processInspection(QString inputSnText);
    void canGoNextMechine(int x) override;
    void set_screen_color(int x);
    void processGetMesTestValue();
    void getTestValue(const int mechines, const QString value) override;
    void on_pushButton_clicked();
    void on_lcdTestButton_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_macInput_returnPressed();
    void on_getMac_returnPressed();
    void on_stopTest_clicked();

  signals:
    void send_go_next_test(int data);
    void send_go_next_focus();
    void send_start_test(int data);
};

#endif // SCREENTEST_H
