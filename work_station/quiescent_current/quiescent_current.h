#ifndef QUIESCENT_CURRENT_H
#define QUIESCENT_CURRENT_H

#include "Abini.h"
#include "test_base.h"
#include "testmodel.h"
#include "ui_quiescent_current.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Ui {
class quiescent_current;
}

class quiescent_current : public test_base {
    Q_OBJECT

  public:
    explicit quiescent_current(int index, QWidget* parent = nullptr);
    ~quiescent_current();
    void startTask() override;
    void disconnect_dongle();

    Ui::quiescent_current* ui;

    // clang-format off
    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QComboBox* getUsbcomNameCombo() override { return nullptr; }
    QComboBox* getJigcomNameCombo() override { return ui->jigComNameCombo; }
    QComboBox* getProductcomNameCombo() override { return ui->productComNameCombo; }
    QTableWidget* testResultTable() override { return ui->testResultTable; }
    QLineEdit* getMacLineEdit() override { return ui->snInput; }
    QLineEdit* macInputLineEdit() override { return ui->macInput; }
    QPlainTextEdit* logEdit() override { return ui->log; }
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; }
    QLabel* getMesStateQlabel() override { return ui->mes_state; }
    QPushButton* getEndTestButton() override { return ui->stopTest; }
    // clang-format on

    ImuDataT orgData;

  private:
    void applyCurrentProtocolConfig();
    void syncVisaPowerUiFromSettings();
    bool setProgrammablePowerOutput(bool enable);
    void refreshProgrammablePowerCurrent(double valueAmps, bool ok);

    // --- SN / 绑定 ---
    QByteArray sn;
    QString M_USERNO;
    QString stringsn;
    QString pumpsoft_version;
    QString tail_sn_string;
    QString last_macAddress = "没有mac地址";
    int snCompareOk = 0;

    // --- 电流卡控 ---
    double HighCurrent = 0;
    double LowCurrent = 0;
    double measure_ammeter = 0;
    UsbProtocolRoute currentProtocolType = UsbProtocolRoute::Scpi;
    bool useProgrammablePower = false;
    double programmablePowerMeasuredCurrentA_ = 0.0;
    bool programmablePowerCurrentReadOk_ = false;

    // --- 状态机 ---
    typedef enum {
        STATE_IDLE,
        STATE_WATI_CONNECT,
        STATE_BANDING,
        STATE_SET_TEST_MODE,
        STATE_VERIFY_TEST_MODE,
        STATE_SLEEP_CURRENT_TEST,
        STATE_SAVE_RESULT,
        STATE_VERIFY_SN,
        STATE_WATI_GET_BASE_STATE,
        STATE_WATI_GET_PERIPHERAL_STATE
    } State;
    State state = STATE_IDLE;
    int periph_state = 0;
    int base_state = 0;
    int refresh_base_times = 0;
    int refresh_periph_times = 0;
    int firstconnectbrush = 1;

    // --- 界面 / 表格 ---
    QString testItem;
    QString testData;
    QString logString = "";
    QString totalresult = "";
    TestModel* basicInfoModel = nullptr;
    TestModel* displaybasicInfoModel = nullptr;
    TestModel* peripheralModel = nullptr;
    QLabel* bleStatusLabel = nullptr;
    QLabel* uartStatusLabel = nullptr;
    QLabel* frame_rate = nullptr;
    QLabel* macLabel = nullptr;
    QLabel* board_sn = nullptr;
    QLabel* product_sn = nullptr;

    // --- 计时 ---
    QElapsedTimer TestTime;
    QTimer* usblogwaittime = new QTimer(this);
    QTimer* waittime = new QTimer(this);
    QTimer* ble_waittime = new QTimer(this);
    int measure_wait_time = 15000;
    int disconnect_wait_time = 5000;
    bool isovertime = 0;

    void pcba_test_data_update(const QString& item, const QString& data, const QString& result);
    void saveImuTestDataToCsv(const QString& macAddress, const QString& result);
    void initBasicInfo();
    void initPeriphState();
    void writePeripheralDataToCSVFile();
    void writeDataToCSVFile();
    void clearDisplay();
    void bindingMacSn(QString bindingMac, QString bindingSn);
    void startFlowWithMac(const QString& mac);

  private slots:
    void processReceivedData(const QByteArray& data) override;

    // 协议上行
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshJigUartState(int state) override;
    void refreshProductUartState(int state) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshBleState(int state) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshChargeCurrentRead(ProtocolChargeCurrentData data) override;
    void refreshMusicState(ProtocolMusicStateData data) override;
    void refreshAmmeterData(QString data) override;

    // MES / 绑定
    void getTestValue(const int mechines, const QString value) override;
    void processInspection(QString inputSnText);
    void processGetMesTestValue();

    // UI 槽
    void on_snInput_returnPressed();
    void on_macInput_returnPressed();
    void on_pushButton_clicked();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_usbconnectButton_clicked();
    void on_usbdisconnectButton_clicked();
    void on_stopTest_clicked();
    void on_visa_test_clicked();
    void on_currentAmmeterVisaApplyButton_clicked();

  signals:
    void send_go_next_focus();
    void send_start_test(int data);
    void send_go_next_test(int data);
};

#endif // QUIESCENT_CURRENT_H
