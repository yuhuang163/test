#ifndef PCBAFORM_H
#define PCBAFORM_H

#include <QWidget>

#include "Abini.h"
#include "fixture_uart.h"
#include "test_base.h"
#include "ui_pcbaform.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#include <QInputDialog>
namespace Ui {
class PcbaForm;
}

class PcbaForm : public test_base {
    Q_OBJECT
  signals:
    void overtask(int);
    void start_sleep_command(int);
    void start_white_modle_command(int);
    void send_go_next_focus();
    void send_go_next_test(int data);
    void send_start_test(int data);

  public:
    explicit PcbaForm(int index, QWidget* parent = nullptr);
    ~PcbaForm();
    Ui::PcbaForm* ui;

    // clang-format off
    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QComboBox* getProductcomNameCombo() override { return ui->productComNameCombo; }
    QLineEdit* getMotorCaliParam() override { return ui->pcba_motor_cali_param; }
    QLineEdit* macInputLineEdit() override { return ui->macInput; }
    QLineEdit* getMacLineEdit() override { return ui->getMac; }
    QPlainTextEdit* logEdit() override { return ui->log; }
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; }
    QTableWidget* testResultTable() override { return ui->testResultTable; }
    QLabel* getMesStateQlabel() override { return ui->mes_state; }
    QPushButton* getEndTestButton() override { return ui->stopTest; }
    // clang-format on
    int firstconnectbrush = 1;
    bool mac_retry_flag = false;
    void startTask() override;
    void overTask() override;
    void startTest() override;
    void updateComboBox() override;

  private slots:

    void processInspection(QString inputSnText);
    void writeToLogFile(const QByteArray& data, QString currentDate, QString macAddress, int machineNumber);
    void refreshImuData(ProtocolImuSampleData x) override;
    void get_remain_data(const FixturePacketData packetData);
    void get_remain_data_sleep(const FixturePacketData packetData);
    void on_stopTest_clicked();
    void refreshDongleWifi(QString data) override;
    void processTestItem(const QString& testItem, int currentValue, int lowValue, int highValue,
                         QList<TestItem>& testItems);
    void processSimpleTestItem(const QString& testItem, int currentValue, int expectedValue,
                               QList<TestItem>& testItems);
    void updateTestResultUI();
    void logPacketData(const FixturePacketData& packetData);
    void processReceivedData(const QByteArray& data) override;
    void refreshWifiMsg(QString data) override;
    void refreshBleRssi(QString data) override;
    void refreshAmmeterData(QString data) override;
    bool deleteFile(const QString& filePath);
    void refreshUsbUartState(int state) override;
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshDongleUartState(int state) override;
    void refreshBleState(int state) override;
    void refreshWifiState(int state);
    void connectwifi();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_macInput_returnPressed();
    void refreshButton(ProtocolButtonStateData data) override;
    void refreshBrushControlState(ProtocolBrushControlData data) override;
    void refreshLedControlState(ProtocolLedControlData data) override;
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_getMac_returnPressed();
    void on_start_scan_clicked();
    void on_pick_device_textActivated(const QString& arg1);

  private:
    QStringList erroContent;
    int ble_wait_time = 0;
    int wifi_connect_waittime = 0;
    QElapsedTimer TestTime;
    int wifi_wait_time = 0;
    bool isbleovertime = 0;  // 是否开始发送校验结果
    bool iswifiovertime = 0; // 是否开始发送校验结果
    int32_t really_accx = 0;
    int32_t really_accy = 0;
    int32_t really_accz = 0;
    int intblerssi = 0;
    int RssiTestTime = 0;
    int rssitestcount = 0;
    int rssitestfailcount = 0;
    int intwifirssi = 0;
    int32_t processIMUData(uint16_t imuData);
    void checkIMUData(const QString& axis, int32_t value, int32_t upper, int32_t lower);
    int is_imu_correct_data = 0;
    bool iswificonnectovertime = 0;   // 是否开始发送校验结果
    bool is_music_play_over_time = 0; // 是否开始发送校验结果
    int passnumber = 0;
    int ngnumber = 0;
    int refresh_times = 1;
    int refresh_periph_state_times = 1;
    int acc_x_up = 0;
    int acc_x_down = 0;
    int acc_y_up = 0;
    int acc_y_down = 0;
    int acc_z_up = 0;
    int acc_z_down = 0;

    QTimer* usblogwaittime = new QTimer(this);
    QTimer* blewaittime = new QTimer(this);
    QTimer* wifiwaittime = new QTimer(this);
    QTimer* wificonnectwaittime = new QTimer(this);
    QTimer* wificonnectwaittimehalf = new QTimer(this);
    QTimer* usb_uart_timer = new QTimer(this);
    QByteArray buffer;
    QTimer* music_play_time = new QTimer(this);

    typedef enum {
        STATE_IDLE,                      // 休眠状态
        STATE_WATI_CONNECT,              // 等待连接
        STATE_WATI_DISABLE_SLEEP,        // 等待进入禁止休眠
        STATE_WATI_CHARGE_CURRENT,       // 测试充电电流
        STATE_WATI_CONNECT_WIFI,         // 等待连接wifi
        STATE_WATI_GET_CORRECT_WIFIRSSI, // 获取正常的wifi强度
        STATE_WATI_GET_CORRECT_MOTOR,
        STATE_WATI_GET_CORRECT_BLERSSI, // 获取正常的wifi强度
        STATE_WATI_CORRECT_IMU_DATA,    // 获取imu准确数据
        STATE_WATI_STOP_IMU_DATA,       // 关闭imu数据
        STATE_WATI_GET_BASE_STATE,
        STATE_WATI_GET_PERIPHERAL_STATE, // 等待获取外设状态
        STATE_WATI_BOTTON_TEST,          // 进行按键状态测试
        STATE_WATI_WORKING_TEST,
        STATE_WATI_LIGHT_TEST,
        STATE_WATI_RETURN_TEST, // 退出工厂模式
        STATE_SLEEP_OPEN,
        STATE_CLOSE_FORBID_SLEEP,
        STATE_WATI_WHITE_MODE,
        STATE_WATI_REMAIN_TEST, // 进行剩余测试
        STATE_WATI_SLEEP,
        STATE_WATI_MOTOR_TEST,    // 开启马达工作电流测试
        STATE_LCD_TEST,           // 系统发送指令开启显示屏测试
        STATE_LED_TEST,           // 补光灯、过压灯，人工查看模块是否正常
        STATE_SLEEP_CURRENT_TEST, // 开启休眠模式测试，系统记录休眠模式电流
        STATE_WAKEUP_TEST,        // 摇晃治具将主板唤醒
        STATE_WATI_SHIP_TEST,     // 船运模式，系统记录电流，完成测试。
        STATE_SAVE_RESULT         // 保存结果在本地

    } State;

    State state = STATE_IDLE;
    int currentTestNumber = 0;
    QString lastMacAddress = "没有mac地址";

    QString stringsn;
    double HighCharCurrent = 0; // 充电电流
    double LowCharCurrent = 0;

    double HighmusicCurrent = 0; // 工作电流
    double LowmusicCurrent = 0;  // 工作电流

    double HighshipCurrent = 0; // 工作电流
    double LowshipCurrent = 0;  // 工作电流

    double HighworkCurrent = 0; // 工作电流
    double LowworkCurrent = 0;  // 工作电流

    double HighstaticCurrent = 0; // 静态电流
    double LowstaticCurrent = 0;  // 静态电流
    double measure_ammeter = 0;
    int music_state;
    int overVoltageLight;
    int button1;
    int button2;
    bool isledcontrol = 0;
    bool isbrushcontrol = 0;
    bool isbuttonTest = 0;
    int sendcount = 0; // 重复发送的次数
    int periph_state = 0;
    int base_state = 0;
    int remain_ok = 0;
    int start_sleep = 0;
    QString totalresult = "";
    QString testItem;
    QString testData;
    QString bleMac;
    QString wifiMac;
    QString WIFI_RSSI;
    QString BLE_RSSI;
    int HighRssi;
    int LowRssi;
    int BleHighRssi;
    int BleLowRssi;
    void borad_test_process();
    void clearDisplay();
    void pcba_test_data_update(const QString& item, const QString& data, const QString& result);
};

#endif // PCBAFORM_H
