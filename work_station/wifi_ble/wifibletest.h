
#ifndef WIFIBLETEST_H
#define WIFIBLETEST_H

#include "Abini.h"
#include "test_base.h"
#include "ui_wifibletest.h"

namespace Ui {
    class wifibletest;
}

class wifibletest : public test_base {
    Q_OBJECT
public:
    explicit wifibletest(int index, QWidget* parent = nullptr);
    ~wifibletest();
    Ui::wifibletest* ui;
    void startTask() override;

private:
    bool allow_retry = 1;
    int measure_ammeter_counts;
    QString nfcdataHeadText = "";
    QString getValueBySN(const QString& sn);
    QString receivedData = "";
    double voltage = 0;
    QString chargestate = "";
    int HighRssi = 0;
    int LowRssi = 0;
    int BleHighRssi = 0;
    int BleLowRssi = 0;
    double standbattary = 0;
    int is_battary_test = 0;
    int RssiTestTime = 0;
    QString WIFI_RSSI = "";
    QString BLE_RSSI = "";
    QString stringsn = "";
    QString tailsn = "";
    QString macAddress = "没有mac地址";

    QString wifiresult = "";
    double HighCurrent = 0;
    double LowCurrent = 0;
    int measure_wait_time = 15000;
    double measure_ammeter = 0;
    QString wifiMac = "";

    QString voltageresult = "";
    QString currentresult = "";
    QString charageresult = "";
    QString bleresult = "";
    int rssitestcount = 0;
    int rssitestfailcount = 0;

    int wifistate = 0;

    bool isovertime = 0;         // 是否开始发送校验结果
    bool iscompareovertime = 0;  // 是否开始发送校验结果
    int intwifirssi = 0;
    int intblerssi = 0;

    QTimer* waittime = new QTimer(this);
    QTimer* comparewaittime = new QTimer(this);
    QTime TestTime;
    QString productName;
    QString ReadNfcData = "";
    QString sku = "55040701";
    QString TestResult = "";
    QString product = "";

    typedef enum {
        STATE_IDLE = 0,  // 休眠状态
        STATE_WATI_CONNECT,
        STATE_DISABLE_SLEEP_1,  // 进入禁止休眠
        STATE_FAC_MODE,
        STATE_WATI_BASE_INFO,
        STATE_WATI_WIFI_CONNECT,          // 等待连接
        STATE_WATI_GET_CORRECT_WIFIRSSI,  // 等待正确的wifi信号强度
        STATE_WATI_GET_CORRECT_BLERSSI,   // 等待正确的蓝牙信号强度
        STATE_WATI_CORRECT_BATTARY,       // 等待正确的蓝牙信号强度
        STATE_WATI_CORRECT_CURRENT,
        STATE_SAVE_RESULT  // 保存结果在本地
    } State;
    State state = STATE_IDLE;
    State getNextState(State currentState);
    QMap<QString, QMap<QString, QString>> deviceMap;  // 存储设备信息
    QString snbanding;

protected:
    // virtual void closeEvent(QCloseEvent *);
    void closeEvent(QCloseEvent* event) override;  // 添加 override 关键字
private slots:
    int findNfcDevicePort(QString name);

    void initDate();

    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QComboBox* getNfcComboBox() override { return ui->NfcComboBox; };          // nfc的usb口
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; };  // usb口
    QLineEdit* getMacLineEdit() override { return ui->getMac; };               // sn输入口
    QLineEdit* macInputLineEdit() override { return ui->macInput; };           // mac地址输入口
    QPlainTextEdit* logEdit() override { return ui->log; };                    // mac地址输入口
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; };                // mac地址输入口
    QTableWidget* testResultTable() override { return ui->testResultTable; };  // 测试结果表格输入口
    QLabel* getMesStateQlabel() override { return ui->mes_state; };            // mes状态的qlab
    QPushButton* getEndTestButton() override { return ui->stopTest; };         // 结束测试按钮

    void refreshBleRssi(QString data) override;
    void getWifiMsg(QString data) override;

    void refreshBaseData(FacGetDevBaseInfo data) override;
    void refreshBattaryData(FacDevInfo data) override;
    void refreshSn(FacDevInfo data) override;
    void refreshBleState(int state) override;

    void getDongleWifi(QString data) override;

    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;

    void refreshWifiState(int state);

    void bandingMacSn(QString bandingmac, QString bandingsn);
    void bandingMacSn_mes(QString bandingmac, QString bandingsn);
    void updateComboBox();
    void getmacadress(const QByteArray& byte);
    void processInspection(QString stringsn);
    void processGetMesTestValue();

    void getTestValue(const int mechines, const QString value) override;

    void on_macInput_returnPressed();
    void on_pushButton_clicked();
    void on_disconnectwifi_clicked();
    void on_connectwifi_clicked();
    void refreshAmmeterData(QString data) override;

    // nfc部分
    void on_nfc_write_read_clicked();
    void on_clear_nfc_data_clicked();
    QString generateHexString(int productionNumber);
    QString generateDateCode();
    void on_getMac_returnPressed();

    void on_mac_combo_textActivated(const QString& arg1);
    void on_clear_scan_clicked();

    void on_snbanding_returnPressed();
    void on_pushButton_2_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_stopTest_clicked();
    void on_nfc_read_clicked();
    void on_nfc_decode_clicked();
    void on_nfc_encode_clicked();
    void on_nfcComFresh_clicked();
    void on_get_battery_clicked();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
};

#endif  // IMUCALI_H
