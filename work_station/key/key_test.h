#ifndef KEY_TEST_H
#define KEY_TEST_H

#include "test_base.h"
#include "ui_key_test.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

#include "Abini.h"
#include "testmodel.h"

namespace Ui {
    class key_test;
}

class QMessageBox;

class key_test : public test_base {
    Q_OBJECT

public:
    explicit key_test(int index, QWidget* parent = nullptr);
    ~key_test();
    Ui::key_test* ui;
    ImuDataT orgData;
    void startTask() override;
    void processReceivedData(const QByteArray& data) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshJigUartState(int state) override;
    void refreshProductUartState(int state) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshBleState(int state) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshMusicState(ProtocolMusicStateData data) override;
    void refreshfwVersion(QString data) override;
    void refreshAmmeterData(QString data) override;
    void checkbutton(ProtocolButtonStateData data) override;
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; };          // usb口（治具）
    QComboBox* getJigcomNameCombo() override { return ui->jigComNameCombo; };          // 治具口（治具）
    QComboBox* getProductcomNameCombo() override { return ui->productComNameCombo; };  // 设备口（治具）
    QTableWidget* testResultTable() override { return ui->testResultTable; };          // 测试结果表格输入口
    QLineEdit* getMacLineEdit() override { return ui->snInput; };                      // sn输入口
    QLineEdit* macInputLineEdit() override { return ui->macInput; };                   // mac地址输入口
    QPlainTextEdit* logEdit() override { return ui->log; };                            // mac地址输入口
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; };                        // msg输入口
    QLabel* getMesStateQlabel() override { return ui->mes_state; };                    // mes状态的qlab
    QPushButton* getEndTestButton() override { return ui->stopTest; };                 // 结束测试按钮
    void disconnect_dongle();

private:
    void applyKeyProtocolConfig();
    QByteArray sn;
    QString M_USERNO;
    QString stringsn;
    QString pumpsoft_version;
    QString keytest_focus;

    QLabel* bleStatusLabel;
    QLabel* uartStatusLabel;
    QLabel* frame_rate;
    QLabel* macLabel;
    QLabel* board_sn;
    QLabel* product_sn;
    int periph_state = 0;
    int base_state = 0;
    int fw_state = 0;
    int key_state = 0;
    int KeyPowerState = 0;
    int KeyStartPauseState = 0;
    int KeyModeState = 0;
    int KeySpeedState = 0;
    int KeyProgramState = 0;
    int KeyLeftState = 0;
    int KeyRightState = 0;
    int KeyLeftRotateState = 0;
    int KeyRightRotateState = 0;
    /// 与 keyWaitPrompt 生命周期同步；onKeyWaitPromptDestroyed 内必置 false，避免续测时不弹窗
    bool keyWaitPromptShown = false;
    /// 为 true 时 close 弹窗视为流程内关闭，不记失败
    bool keyWaitPromptProgrammaticClose = false;
    bool factoryModeLogged = false;
    QMessageBox* keyWaitPrompt = nullptr;
    int snCompareOk = 0;
    double measure_ammeter = 0;
    void pcba_test_data_update(const QString& item, const QString& data, const QString& result);
    QString testItem;
    QString testData;
    TestModel* basicInfoModel;
    TestModel* displaybasicInfoModel;
    TestModel* peripheralModel;
    QString logString = "";
    QString totalresult = "";
    Qusb::ProtocolType keyProtocolType = Qusb::ProtocolType::Scpi;
    typedef enum {
        STATE_IDLE,               // 休眠状态
        STATE_WATI_CONNECT,       // 等待 BLE 连接
        STATE_BANDING,            // SN 绑定
        STATE_SET_TEST_MODE,      // 设置测试模式（工厂模式）
        STATE_VERIFY_TEST_MODE,   // 校验测试模式
        STATE_KEY_TEST,            // 按键测试
        STATE_SAVE_RESULT,         // 保存结果 / 界面复位
        STATE_VERIFY_SN,           // 校验SN
        STATE_WATI_GET_KEY_STATE,  // 基础信息获取
        STATE_WAIT_GET_KEY_POWER_STATE,
        STATE_WAIT_GET_KEY_STARTPAUSE_STATE,
        STATE_WAIT_GET_KEY_MODE_STATE,
        STATE_WAIT_GET_KEY_SPEED_STATE,
        STATE_WAIT_GET_KEY_PROGRAM_STATE,
        STATE_WAIT_GET_KEY_LEFT_STATE,
        STATE_WAIT_GET_KEY_RIGHT_STATE,
        STATE_WAIT_GET_KEY_LEFTROTATE_STATE,
        STATE_WAIT_GET_KEY_RIGHTROTATE_STATE,
        STATE_WATI_GET_PERIPHERAL_STATE  // 外设信息获取

    } State;
    QElapsedTimer TestTime;
    State state = STATE_IDLE;
    struct CurrentStats {
        bool valid = false;
        double minValue = 0.0;
        double maxValue = 0.0;
        double avgValue = 0.0;
        double fluctuation = 0.0;
    };
    // 操作员工号
    // 设备编号
    // 制程
    // 机种
    // 线体
    // 动作
    int refresh_base_times;
    int refresh_periph_times;
    int refresh_fw_times;
    int refresh_key_times = 0;
    int firstconnectbrush = 1;
    QTimer* usblogwaittime = new QTimer(this);
    QString last_macAddress = "没有mac地址";
    QTimer* waittime = new QTimer(this);
    QTimer* ble_waittime = new QTimer(this);
    int key_test_wait_time = 15000;
    int disconnect_wait_time = 5000;
    bool isovertime = 0;  // 是否开始发送校验结果
    bool waitingMesInspection = false;
    bool modeCheckPassed = false;
    CurrentStats workCurrentStats;
    CurrentStats chargeCurrentStats;
    void saveImuTestDataToCsv(const QString& macAddress, const QString& result);
    void initBasicInfo();
    void initPeriphState();
    void writePeripheralDataToCSVFile();
    void writeDataToCSVFile();
    void clearDisplay();
    void bandingMacSn(QString bandingmac, QString bandingsn);
    bool validateCompanySnRule(const QString& snValue);
    QString parseMacFromSn(const QString& snValue);
    void startFlowWithMac(const QString& mac);
    bool verifyTestModeState();
    bool controlProgrammablePowerForCharge(bool enable);
    CurrentStats collectCurrentStats(const QString& itemName, int sampleCount, int sampleIntervalMs);
    bool evaluateCurrentStats(const QString& itemName, const CurrentStats& stats, double low, double high);

    void closeKeyWaitPromptProgrammatically();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);

private slots:
    void solveMesSucess(const int mechines) override;
    void solveMesData(const int mechines, QString msg) override;

    void processInspection(QString stringsn);
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void on_usbconnectButton_clicked();
    void on_usbdisconnectButton_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_jigConnectButton_clicked();
    void on_jigDisconnectButton_clicked();
    // void processReceivedData(const QByteArray &data);
    void on_snInput_returnPressed();
    void on_pushButton_clicked();
    void on_macInput_returnPressed();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_stopTest_clicked();
    void onKeyWaitPromptDestroyed();
};

#endif  // KEY_TEST_H
