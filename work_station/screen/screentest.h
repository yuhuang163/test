
#ifndef SCREENTEST_H
#define SCREENTEST_H

#include "Abini.h"
#include "test_base.h"
#include "ui_screentest.h"

namespace Ui
{
    class screentest;
}
class screentest : public test_base
{
    Q_OBJECT
public:
    explicit screentest(int index, QWidget *parent = nullptr);
    ~screentest();
    Ui::screentest *ui;
    void start_task() override;
private:
    int getIndex() const
    {
        return m_index;
    }
    int m_index;
    typedef enum
    {
        STATE_IDLE,                // 休眠状态
        STATE_WATI_CONNECT,        // 等待连接
        STATE_DISABLE_SLEEP_1,     // 进入禁止休眠
        STATE_PROCESS_INSPECTION,   // 工序核对检查
        STATE_COLOR1,
        STATE_COLOR2,
        STATE_COLOR3,
        STATE_COLOR4,
        STATE_COLOR5,
        STATE_SAVE_RESULT   // 保存结果在本地
    } State;
    State state = STATE_IDLE;

    QTime TestTime;
    QString result = "";
    QString passValue = "通过";
    QString failValue = "失败";
    bool is_can_go_next = 0;
    QString stringsn;
    QString macAddress = "没有mac地址";
    bool isScreenContinue = 0;
    bool is_lcd_control = 0;

    QString product = "";
    bool mes_set_ok = 0;
protected:
    //  virtual void closeEvent(QCloseEvent *);
    void closeEvent(QCloseEvent *event) override;   // 添加 override 关键字

private slots:

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
    void refresh_Lcd_CONTROL(FacLcdControl style) override;
    void refresh_ble_state(int state) override;
    void refresh_sn(FacDevInfo data) override;
    void get_dongle_ver(QString data) override;
    void refresh_dongle_uart_state(int state) override;
    void showlog(QString msg);
    void processInspection(QString stringsn);
    void can_go_next(int x) override;
    void refreshMesState(int state);
    void set_screen_color(int x);
    void solveMesData(const int mechines, QString msg);
    void solveMesSucess(const int mechines);
    void processGetMesTestValue();
    void getTestValue(const int mechines, const QString value) override;
    void on_pushButton_clicked();
    void on_lcdTestButton_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_macInput_returnPressed();
    void get_mac(QString sn_to_search);
    void on_get_mac_returnPressed();

    void on_stopTest_clicked();

signals:
    void endTest(int data);
    void goNextTest(int data);

    void goNextFocus();
    void startTest(int data);
};

#endif   // SCREENTEST_H
