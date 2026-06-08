#ifndef WIFIBLETEST_H
#define WIFIBLETEST_H

#include <QList>
#include <QSerialPort>
#include <QVariant>

#include "Abini.h"
#include "qtupleservice.h"
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
    void startTask() override;

    Ui::wifibletest* ui;

private:
    // --- 流程状态机 ---
    typedef enum {
        STATE_IDLE = 0,
        STATE_WATI_CONNECT,
        STATE_FAC_MODE,
        STATE_WATI_BASE_INFO,
        STATE_WATI_WIFI_CONNECT,
        STATE_WATI_GET_CORRECT_WIFIRSSI,
        STATE_WATI_GET_CORRECT_BLERSSI,
        STATE_BLE_PER_INIT,
        STATE_BLE_PER_INSTRUMENT_RESET,
        STATE_BLE_PER_START_RX,
        STATE_BLE_PER_CMW_TX,
        STATE_BLE_PER_STOP_RX_PER,
        STATE_WATI_CORRECT_BATTARY,
        STATE_TUPLE_APPLY,
        STATE_TUPLE_WRITE_PRODUCT_KEY,
        STATE_TUPLE_WRITE_DEVICE_NAME,
        STATE_TUPLE_WRITE_DEVICE_SECRET,
        STATE_TUPLE_READ_COMPARE,
        STATE_TUPLE_REPORT_WRITE,
        STATE_WATI_CORRECT_CURRENT,
        STATE_SAVE_RESULT
    } State;
    State state = STATE_IDLE;
    State getNextState(State currentState);

    // --- RSSI / WiFi / 电量 / 电流 ---
    QString receivedData = "";
    double voltage = 0;
    QString chargestate = "";
    int HighRssi = 0;
    int LowRssi = 0;
    int BleHighRssi = 0;
    int BleLowRssi = 0;
    double standbattary = 0;
    int is_battary_test = 0;
    int battary = 0;
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
    int refresh_base_times = 0;
    int refresh_periph_times = 0;
    int wifistate = 0;
    bool isovertime = 0;
    bool iscompareovertime = 0;
    int intwifirssi = 0;
    int intblerssi = 0;
    bool allow_retry = 1;
    int measure_ammeter_counts = 0;

    // --- 计时 / NFC / 结论 ---
    QTimer* waittime = new QTimer(this);
    QTimer* comparewaittime = new QTimer(this);
    QElapsedTimer TestTime;
    QString productName;
    QString ReadNfcData = "";
    QString nfcdataHeadText = "";
    QString sku = "55040701";
    QString TestResult = "";

    // --- BLE PER / CMW ---
    bool blePerRxDone = false;
    bool blePerRxPass = true;
    QString blePerRxMesValue = QStringLiteral("未测");
    QSerialPort* blePerUart = nullptr;
    struct BlePerScenario {
        QString label;
        int frequencyMHz = 0;
        int channelIndex = 0;
        int phyCode = 1;
        QString phyName = QStringLiteral("1M");
    };
    QList<BlePerScenario> blePerRxScenarios_;
    int blePerRxScenarioIndex_ = 0;
    QList<BlePerScenario> parseBlePerScenarioList() const;
    int frequencyToBlePerChannel(int freqMHz) const;
    QByteArray buildBlePerRxStartCommand(int channelIndex, int phyCode) const;
    QByteArray hexStringToBytes(const QString& hex) const;
    QString bytesToCompactHex(const QByteArray& data) const;
    bool containsHexFragment(const QByteArray& response, const QString& fragment) const;
    bool openBlePerUart(QString* errorMessage);
    void closeBlePerUart();
    QByteArray sendBlePerUartCommandHex(const QString& hex, const QString& stageName, QString* errorMessage);
    int parseBlePerRxCount(const QByteArray& response, bool* ok) const;
    void loadWifiBleCmw100Config();
    bool cmwVisaWrite(const QString& cmd);
    bool cmwVisaQuery(const QString& cmd, QString* response);
    bool prepareBlePerCmw(QString* errorMessage);
    bool initializeBlePerCmwGprf(QString* errorMessage);
    bool parseBlePerCmwArbScount(const QString& response, double* countTime, int* cycles, int* samplesCurrent) const;
    bool waitBlePerCmwArbComplete(const BlePerScenario& scenario, QString* errorMessage);
    bool runBlePerCmwScenario(const BlePerScenario& scenario, QString* errorMessage);
    bool runBlePerScenarioAttempt(const BlePerScenario& scenario, double* perOut, int* rxCountOut, QString* errorMessage);
    bool ensureProductSerialForBlePer(const QString& stepName);
    QByteArray brushInstrumentStartCmdForProfile(int profile) const;
    bool waitBrushInstrumentResponse(const QByteArray& prefix, int timeoutMs, QString* errorMessage);
    int blePerProfileForScenario(const BlePerScenario& scenario) const;
    bool prepareBlePerRxStateMachine();
    bool runBlePerInstrumentResetStep();
    bool runBlePerStartRxStep();
    bool runBlePerCmwTxStep();
    bool runBlePerStopRxPerStep();
    void recordBlePerStepFailure(const QString& stepName, const QString& reason);
    void advanceBlePerScenarioOrContinue();
    void continueAfterBlePerRx();

    // --- 三元组 / BYD MES ---
    QMap<QString, QMap<QString, QString>> deviceMap;
    QString snBinding;
    TupleApplyResult tupleData_;
    bool tupleStepStarted_ = false;
    bool tupleReadDone_ = false;
    bool tupleReadPass_ = false;
    QString tupleReadData_;
    QString tupleReadAsk_;
    QString tupleMesItemvalue_;
    bool isWifiBleTupleEnabled() const;
    void resetTupleBurnRuntime();
    bool applyTupleByMacForWifiBle();
    bool failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason);
    void startTupleWriteProductKey();
    void startTupleWriteDeviceName();
    void startTupleWriteDeviceSecret();
    void startTupleReadCompare();
    bool reportTupleWriteRecordForWifiBle();
    void reportBydSfcKey(const QString& dataName, const QVariant& dataValue, int qty = 1);
    void appendTupleMesSegment(const QString& key, const QString& value);
    void appendTupleTestResult(const QString& item, const QString& data, const QString& result, const QString& ask = QStringLiteral("通过"));
    void finishTupleFailure(const QString& item, const QString& data, const QString& ask = QStringLiteral("通过"));

private slots:
    int findNfcDevicePort(QString name);
    void initData();

    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QComboBox* getProductcomNameCombo() override { return ui->productComNameCombo; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QComboBox* getNfcComboBox() override { return ui->NfcComboBox; }
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; }
    QLineEdit* getMacLineEdit() override { return ui->getMac; }
    QLineEdit* macInputLineEdit() override { return ui->macInput; }
    QPlainTextEdit* logEdit() override { return ui->log; }
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; }
    QTableWidget* testResultTable() override { return ui->testResultTable; }
    QLabel* getMesStateQlabel() override { return ui->mes_state; }
    QPushButton* getEndTestButton() override { return ui->stopTest; }

    // 协议上行
    void refreshBleRssi(QString data) override;
    void refreshWifiMsg(QString data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshBattaryData(ProtocolBatteryData data) override;
    void refreshTupleData(ProtocolTupleData data) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshBleState(int state) override;
    void refreshAmmeterData(QString data) override;

    // 串口 / MES / 绑定
    void refreshDongleWifi(QString data) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshProductUartState(int state) override;
    void refreshWifiState(int state);
    void getTestValue(const int mechines, const QString value) override;
    void bindingMacSn(QString bindingMac, QString bindingSn);
    void bindingMacSnMes(QString bindingMac, QString bindingSn);
    void updateComboBox();
    void getMacAddress(const QByteArray& byte);
    void processInspection(QString inputSnText);
    void processGetMesTestValue();

    // UI 槽
    void on_getMac_returnPressed();
    void on_macInput_returnPressed();
    void on_snbanding_returnPressed();
    void on_mac_combo_textActivated(const QString& arg1);
    void on_clear_scan_clicked();
    void on_connectwifi_clicked();
    void on_disconnectwifi_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_usbconnectButton_clicked();
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_get_battery_clicked();
    void on_stopTest_clicked();
    void on_nfc_write_read_clicked();
    void on_clear_nfc_data_clicked();
    void on_nfc_read_clicked();
    void on_nfc_decode_clicked();
    void on_nfc_encode_clicked();
    void on_nfcComFresh_clicked();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
};

#endif  // WIFIBLETEST_H
