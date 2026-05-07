#ifndef QFREEWORK_H
#define QFREEWORK_H

#include <QWidget>

#include "Abini.h"
#include "qapplication.h"
#include "test_base.h"
#include "ui_qfreework.h"

namespace Ui {
    class QFreeWork;
}

class QFreeWork : public test_base {
    Q_OBJECT
public:
    explicit QFreeWork(int index, QWidget* parent = nullptr);
    ~QFreeWork();
    Ui::QFreeWork* ui;
    void startTask() override;

private:
    int teststate = -1;
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

    QString deviceTailSnFromDevice = "";
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
    QElapsedTimer TestTime;
    QString productName;
    QString TestResult = "";
    QString product = "";

    typedef enum {
        STATE_IDLE = 0,  // 休眠状态
        STATE_WATI_CONNECT,
        STATE_DISABLE_SLEEP_1,  // 进入禁止休眠
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

private:
    void refreshOrderedTestIndexes();
    QVector<int> loadIndexesFromConfig();
    QVector<int> orderedTestIndexes_;
    QByteArray expectedTailSnFromUi;
    void executeFunctionByName(const QString functionName);
    struct NamedFunction {
        int id = -1;
        QString name;
        std::function<void()> function;
        bool needCaseDone = false;
    };
    void createTestFunctions();
    std::vector<NamedFunction> testFunctions;
    // 单个测试步骤的运行态：
    // started: 已经触发过该步骤 action（避免重复发送命令）
    // done/pass: 业务判定完成与结果（例如电量+卡控的异步判定）
    // functionId: 当前正在等待判定的测试项稳定ID
    struct StepRuntime {
        bool started = false;
        bool done = false;
        bool pass = true;
        int functionId = -1;
        QString testData;
        QString ask = "通过";

        // 每进入下一步或流程结束时统一复位
        void reset() {
            started = false;
            done = false;
            pass = true;
            functionId = -1;
            testData.clear();
            ask = "通过";
        }
    } stepRuntime_;
    bool isCurrentStep(const QString& functionName) const;
    void appendPeriphItem(QVector<TestItem>& periphTestItems, bool& pass, const QString& name, const QString& value,
                          const QString& expect, bool needCompare);

private slots:
    void initDate();
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; };  // usb口（治具）
    QLineEdit* getMacLineEdit() override { return ui->getMac; };               // sn输入口
    QLineEdit* macInputLineEdit() override { return ui->macInput; };           // mac地址输入口
    QPlainTextEdit* logEdit() override { return ui->log; };                    // mac地址输入口
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; };                // msg输入口
    QTableWidget* testResultTable() override { return ui->testResultTable; };  // 测试结果表格输入口
    QLabel* getMesStateQlabel() override { return ui->mes_state; };            // mes状态的qlab
    QPushButton* getEndTestButton() override { return ui->stopTest; };         // 结束测试按钮

    void refreshBleRssi(QString data) override;
    void getWifiMsg(QString data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshBattaryData(ProtocolBatteryData data) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshRssiRead(ProtocolRssiData data) override;
    void refreshChargeCurrentRead(ProtocolUInt32ValueData data) override;
    void refreshBleState(int state) override;
    void getDongleWifi(QString data) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshWifiState(int state);
    void bandingMacSn(QString bandingmac, QString bandingsn);
    void bandingMacSn_mes(QString bandingmac, QString bandingsn);
    void updateComboBox() override;
    void getmacadress(const QByteArray& byte);
    void processInspection(QString inputSnText);
    void processGetMesTestValue();
    void getTestValue(const int mechines, const QString value) override;
    void on_macInput_returnPressed();
    void on_pushButton_clicked();
    void on_disconnectwifi_clicked();
    void on_connectwifi_clicked();
    void refreshAmmeterData(QString data) override;
    void on_get_battery_clicked();

    void on_getMac_returnPressed();

    void on_mac_combo_textActivated(const QString& arg1);
    void on_clear_scan_clicked();
    void getMac(QString sn_to_search);
    void on_snbanding_returnPressed();
    void on_pushButton_2_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();

    void on_stopTest_clicked();

    void on_save_config_clicked();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
};

#endif  // QFREEWORK_H
