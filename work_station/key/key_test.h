#ifndef KEY_TEST_H
#define KEY_TEST_H

#include "Abini.h"
#include "test_base.h"
#include "testmodel.h"
#include "ui_key_test.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace Ui {
class key_test;
}

class QMessageBox;

class key_test : public test_base {
    Q_OBJECT

  public:
    explicit key_test(int index, QWidget* parent = nullptr);
    ~key_test();
    void startTask() override;
    void disconnect_dongle();

    Ui::key_test* ui;

    // clang-format off
    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; }
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
    // --- 协议 / 界面 ---
    void applyKeyProtocolConfig();
    UsbProtocolRoute keyProtocolType = UsbProtocolRoute::Scpi;

    // --- SN / 绑定 ---
    QByteArray sn;
    QString M_USERNO;
    QString stringsn;
    QString pumpsoft_version;
    QString keytest_focus;
    QString last_macAddress = "没有mac地址";
    int snCompareOk = 0;

    // --- 状态机 ---
    typedef enum {
        STATE_IDLE,
        STATE_WATI_CONNECT,
        STATE_BANDING,
        STATE_SET_TEST_MODE,
        STATE_VERIFY_TEST_MODE,
        STATE_KEY_TEST,
        STATE_SAVE_RESULT,
        STATE_VERIFY_SN,
        STATE_WATI_GET_KEY_STATE,
        STATE_WAIT_GET_KEY_POWER_STATE,
        STATE_WAIT_GET_KEY_STARTPAUSE_STATE,
        STATE_WAIT_GET_KEY_MODE_STATE,
        STATE_WAIT_GET_KEY_SPEED_STATE,
        STATE_WAIT_GET_KEY_PROGRAM_STATE,
        STATE_WAIT_GET_KEY_LEFT_STATE,
        STATE_WAIT_GET_KEY_RIGHT_STATE,
        STATE_WATI_GET_PERIPHERAL_STATE
    } State;
    State state = STATE_IDLE;
    int periph_state = 0;
    int base_state = 0;
    int key_state = 0;
    int KeyPowerState = 0;
    int KeyStartPauseState = 0;
    int KeyModeState = 0;
    int KeySpeedState = 0;
    int KeyProgramState = 0;
    int KeyLeftState = 0;
    int KeyRightState = 0;
    int refresh_base_times = 0;
    int refresh_periph_times = 0;
    int refresh_key_times = 0;
    int firstconnectbrush = 1;

    // --- 按键等待 / PLC ---
    bool keyWaitPromptShown = false;
    bool keyWaitPromptProgrammaticClose = false;
    bool factoryModeLogged = false;
    QMessageBox* keyWaitPrompt = nullptr;
    bool plcKeyActionStarted = false;
    quint64 plcKeyWaitSeq = 0;
    QString keyErrorDetail;
    int lastKeyButtonId = -1;
    qint64 lastKeyButtonMs = 0;
    QElapsedTimer keyButtonDebounceTimer;
    quint32 lowKeyCap_ = 0;
    quint32 highKeyCap_ = 0;
    int currentKeyCapRequestKk_ = -1;
    int currentKeyConfiguredId_ = 0;
    bool plcKeyCapSyncReadPending_ = false;
    bool plcKeyCapSyncReadOk_ = false;
    quint32 plcKeyCapSyncReadValue_ = 0;
    int plcKeyCapSyncReadAuxId_ = -1;
    QString plcKeyCapPassSummary_;
    QString keyPassDetail;

    // --- 量测 / 表格 ---
    double measure_ammeter = 0;
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
    int key_test_wait_time = 15000;
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
    void closeKeyWaitPromptProgrammatically();
    bool usePlcClickerForKeyTest() const;
    void startPlcClickerAndWaitKey(const QString& testName, int keyIndex0To6);
    void failCurrentPlcKeyStep(const QString& testName, const QString& reason);
    QString currentKeyStepName() const;
    QString currentExpectedKeyId() const;
    void setCurrentKeyResult(int result);
    QString currentKeyFailureDetail() const;
    void recordCurrentKeyButtonResult(int keyButtonId);
    void resetPlcKeyCapSyncReadState();
    bool pollKeyCapDuringPress(QString* errOut, QString* outSummary);
    bool runPlcV3TouchKeyFull(int keyIndex0To6, QString* summary);
    int resolvedPlcMBase() const;
    int resolvedPlcConnectVerifyM() const;
    QString resolvedPlcIpAddress() const;
    int resolvedPlcPort() const;
    quint8 resolvedPlcUnitId() const;
    int resolvedPlcMCoilAddressOffset() const;
    int resolvedPlcPositionReadyBase() const;
    int resolvedPlcStepDoneBase() const;
    int resolvedPlcKeyDoneM() const;
    bool plcReadCoil(int absoluteM, bool* value, QString* errorMessage);
    bool plcWriteCoil(int absoluteM, bool value, QString* errorMessage);
    bool plcWaitCoilTrue(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage);
    bool plcWaitCoilFalse(int absoluteM, int timeoutMs, int pollMs, QString* errorMessage);
    bool plcSendStepDone(QString* errorMessage);

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
    void refreshMusicState(ProtocolMusicStateData data) override;
    void refreshAmmeterData(QString data) override;
    void refreshButton(ProtocolButtonStateData data) override;
    void refreshKeySignalRead(ProtocolKeyCapData data) override;

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
    void on_jigConnectButton_clicked();
    void on_jigDisconnectButton_clicked();
    void on_usbconnectButton_clicked();
    void on_usbdisconnectButton_clicked();
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void on_stopTest_clicked();
    void onKeyWaitPromptDestroyed();

  signals:
    void send_go_next_focus();
    void send_start_test(int data);
    void send_go_next_test(int data);
};

#endif // KEY_TEST_H
