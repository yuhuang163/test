#ifndef AGEING_H
#define AGEING_H

#include "Abini.h"
#include "test_base.h"
#include "ui_ageing.h"

namespace Ui
{
    class ageing;
}
class ageing : public test_base
{
    Q_OBJECT
public:

    explicit ageing(int index, QWidget *parent = nullptr);
    ~ageing();
    Ui::ageing *ui;
    void start_task() override;
    void refresh_ble_state(int state) override;
    void show_product(QString name);
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
private:

    double voltage=0;

    QString result = "";
    double standbattary = 0;
    int is_battary_test = 0;
    QString passValue = "通过";
    QString failValue = "失败";
    typedef enum
    {
        STATE_IDLE,           // 休眠状态
        STATE_WATI_CONNECT,   // 等待连接
        STATE_WAIT_BANDING,
        STATE_WAIT_CORRECT_BANDING,

        STATE_WAIT_BANDING_SUBPID,
        STATE_WAIT_CORRECT_BANDING_SUBPID,


        STATE_DISABLE_SLEEP_1,     // 进入禁止休眠
        STATE_GETBATTERY,
        STATE_PROCESS_INSPECTION,   // 工序核对检查
        STATE_CHECK_FLASH,         // 检查flash资源
        STATE_AGE,                 // 开始老化
        STATE_SAVE_RESULT          // 保存结果在本地
    } State;
    State state = STATE_IDLE;

    QString stringsn;
    QString stringSubpid;

    QString macAddress = "没有mac地址";
    bool isAgeContinue = 0;


    QByteArray sn;
    QByteArray subpid;



    QString getValueBySN(const QString &sn ) ;

        int snCompareOk = 0;
int subpidCompareOk = 0;


    bool mes_set_ok = 0;
    int flash_state = 0;
    int refresh_periph_times;
    QTime TestTime;
    int getIndex() const
    {
        return m_index;
    }
    int m_index;
protected:
    virtual void closeEvent(QCloseEvent *);

private slots:
    void showlog(QString msg);
    void get_dongle_ver(QString data) override;
    void refresh_sn(FacDevInfo data) override;
    void refresh_periph_data(FacGetPeriphState) override;
    void refresh_battary_data(FacDevInfo adc)override;

    void processGetMesTestValue();
    void solveMesData(const int mechines, QString msg);
    void solveMesSucess(const int mechines);
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void refreshMesState(int state);
    void getTestValue(const int mechines, const QString value) override;
    void on_macInput_returnPressed();
    void refresh_dongle_uart_state(int state);

    void on_pushButton_clicked();
    void on_enterBurningMode_clicked();
    void on_exitBurningMode_clicked();
    void on_get_mac_returnPressed();
    void get_mac(QString sn_to_search);
    void on_snInput_returnPressed();
    void processInspection(QString stringsn);
    void on_stopTest_clicked();

signals:

    void endTest(int data);
    void goNextTest(int data);

    void goNextFocus();
    void startTest(int data);
};

#endif   // AGEING_H
