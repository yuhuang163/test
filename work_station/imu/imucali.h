#ifndef IMUCALI_H
#define IMUCALI_H

#include "Abini.h"
#include "fixture_uart.h"
#include "imu_calibrate.h"
#include "sensor_hub.h"
#include "test_base.h"
#include "ui_imucali.h"

namespace Ui
{
    class imucali;
}

class imucali : public test_base
{
    Q_OBJECT
public:
    explicit imucali(int index, QWidget *parent = nullptr);
    ~imucali();
    void start_task() override;

    Ui::imucali *ui;
    ImuDataT orgData;
    void end_task();
    QComboBox *getComNameCombo() override
    {
        return ui->comNameCombo;
    };   // dongle口
    QComboBox *getUsbcomNameCombo() override
    {
        return ui->comNameCombo;
    };   // usb口（治具）

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
    QList<QLabel *> labelList;
    QString information;
    int m_index;
    QString result = "";
    QString passValue = "PASS";

    QString product_name;
    QString failValue = "FAIL";
    imu_calibrate *qimuc = nullptr;
    new_imu_calibrate *nqimuc = nullptr;

    QString stringsn;
    QString macAddress = "没有mac地址";
    bool isAgeContinue = 0;
            // 操作员工号
              // 线体
         // 设备编号
             // 动作
       // 制程
             // 机种
          // 机种
    bool mes_set_ok = 0;
    bool isimuCaliContinue = false;
    bool isovertime = 0;              // 是否开始发送校验结果
    bool iscompareovertime = 0;       // 是否开始发送校验结果
    bool isimuCaliOk = 0;             // 是否校准完成
    bool is_start_ium_cali = 0;       // 是否开始六轴校准
    bool is_start_ium_test = 0;       // 是否开始六轴校准
    bool isStartSendCaliResult = 0;   // 是否开始发送校验结果
    bool is_fix_set_ok = 0;           // 是否治具设置ok
    bool is_imu_test_ok = 0;          // 是否测试ok
    bool is_old_cali = 0;             // 是否测试ok
    int count123 = 0;
    int count4567 = 0;
    int count89 = 0;
    int imudata_check = 0;
    QTimer *waittime = new QTimer(this);
    QTimer *comparewaittime = new QTimer(this);
    QTimer *caliwaittime = new QTimer(this);   // 校准超时机制
    int imu_cali_wait_time = 25000;            // 校准超时机制
    int imu_wait_time = 15000;
    int imu_compare_wait_time = 15000;
    int ImuCompareData = 15000;
    typedef enum
    {
        STATE_IDLE,   // 休眠状态
        STATE_PROCESS_INSPECTION,
        STATE_WATI_CONNECT,      // 等待连接
        STATE_DISABLE_SLEEP_1,   // 进入禁止休眠
        STATE_GETBASEDATA,
        STATE_SET_COLLECT_PARAM_1,   // 获取采集参数
        STATE_CAIL,                  // 开始校准
        STATE_SEND_CAIL_RESULT,      // 发送校准结果
        STATE_SENDOK,                // 发送成功检查
        STATE_CHECKOK,               // 数据获取检查
        STATE_SAVE_RESULT,           // 保存结果在本地
        STATE_GETPARAM,
        STATE_TEST,   // 六轴测试
        STATE_WIAT_FIX_SET,
        STATE_COMPARE,
        STATE_END
    } State;
    State state = STATE_IDLE;
    QTime TestTime;
protected:
    virtual void closeEvent(QCloseEvent *);

private slots:
    void get_dongle_ver(QString data) override;

    void showlog(QString msg);
    void print_fixture_log(QString data);
    void refresh_base_data(FacGetDevBaseInfo data) override;
    void refreshMesState(int state);
    void set_fix_result(int state);
    void getimuData(FacUploadNineAlex x) override;
    void update_IMU_CALIB_result(FacImuCalibResult x) override;

    int getIndex() const
    {
        return m_index;
    }

    void solveMesData(const int mechines, QString msg);
    void solveMesSucess(const int mechines);

    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_macInput_returnPressed();
    void refresh_dongle_uart_state(int state);
    void refresh_ble_state(int state);
    void refresh_sn(FacDevInfo data);
    void on_pushButton_clicked();
    void refresh_imu_cali_msg(QString msg);
    void refresh_imu_cali_reslt_msg(QString msg);
    void refresh_imu_data_to_csv(QString imutime, QString msg);
    void on_stopimuCaliButton_clicked();
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();
    void on_pushButton_2_clicked();
    void get_mac(QString sn_to_search);
    void on_get_mac_returnPressed();
    void refresh_imu_cali_position(int position);
    void processInspection(QString stringsn);
signals:
    void goNextTest(int data);

    // void send_GetTestData(MesPacketData);
    // void send_GetTestData(MesPacketData pack);
    // void send_getsn_jj(MesPacketData pack);
    // void processInspection(MesPacketData pack);
    // void processInspection(MesPacketData pack);
    // void send_processInspection_jj(MesPacketData pack);
    // void sendTestPass(MesPacketData pack);
    // void sendTestPass(MesPacketData pack);
    // void sendTestPass(MesPacketData pack);
    // void processInspection(MesPacketData);
    // void sendTestPass(MesPacketData);

    void endcali(int data);
    void endTest(int data);
    void stage1_ok(int data);
    void stage2_ok(int data);
    void stage3_ok(int data);

    void fixture_up(int data);
    void fixture_down(int data);
    void fixture_left(int data);
    void fixture_right(int data);

    void goNextFocus();
    void startTest(int data);
};

#endif   // IMUCALI_H
