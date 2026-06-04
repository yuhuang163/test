#ifndef QUIESCENT_CURRENT_H
#define QUIESCENT_CURRENT_H

#include "test_base.h"
#include "ui_quiescent_current.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

#include "Abini.h"
#include "testmodel.h"

namespace Ui {
    class quiescent_current;
}

class quiescent_current : public test_base {
    Q_OBJECT

public:
    explicit quiescent_current(int index, QWidget* parent = nullptr);
    ~quiescent_current();
    Ui::quiescent_current* ui;
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
    void refreshChargeCurrentRead(ProtocolUInt32ValueData data) override;
    void refreshMusicState(ProtocolMusicStateData data) override;
    // void refreshfwVersion(QString data) override;
    void refreshAmmeterData(QString data) override;
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QComboBox* getUsbcomNameCombo() override { return nullptr; };                      // 万用表改为 VISA 地址配置
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
    void applyCurrentProtocolConfig();
    void syncVisaPowerUiFromSettings();
    bool setProgrammablePowerOutput(bool enable);
    void refreshProgrammablePowerCurrent(double valueAmps, bool ok);
    QByteArray sn;
    double HighCurrent;
    double LowCurrent;
    QString M_USERNO;
    QString stringsn;
    QString pumpsoft_version;
    QString tail_sn_string;

    QLabel* bleStatusLabel;
    QLabel* uartStatusLabel;
    QLabel* frame_rate;
    QLabel* macLabel;
    QLabel* board_sn;
    QLabel* product_sn;
    int periph_state = 0;
    int base_state = 0;
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
    Qusb::ProtocolType currentProtocolType = Qusb::ProtocolType::Scpi;
    bool useProgrammablePower = false;
    double programmablePowerMeasuredCurrentA_ = 0.0;
    bool programmablePowerCurrentReadOk_ = false;
    typedef enum {
        STATE_IDLE,               // 休眠状态
        STATE_WATI_CONNECT,       // 等待 BLE 连接
        STATE_BANDING,            // SN 绑定
        STATE_SET_TEST_MODE,      // 设置测试模式（工厂模式）
        STATE_VERIFY_TEST_MODE,   // 校验测试模式
        STATE_SLEEP_CURRENT_TEST,  // 工作电流等测量
        STATE_SAVE_RESULT,         // 保存结果 / 界面复位
        STATE_VERIFY_SN,           // 校验SN
        STATE_WATI_GET_BASE_STATE,  // 基础信息获取
        STATE_WATI_GET_PERIPHERAL_STATE  // 外设信息获取

    } State;
    QElapsedTimer TestTime;
    State state = STATE_IDLE;
    // 操作员工号
    // 设备编号
    // 制程
    // 机种
    // 线体
    // 动作
    int refresh_base_times;
    int refresh_periph_times;
    int firstconnectbrush = 1;
    QTimer* usblogwaittime = new QTimer(this);
    QString last_macAddress = "没有mac地址";
    QTimer* waittime = new QTimer(this);
    QTimer* ble_waittime = new QTimer(this);
    int measure_wait_time = 15000;
    int disconnect_wait_time = 5000;
    bool isovertime = 0;  // 是否开始发送校验结果
   
    void saveImuTestDataToCsv(const QString& macAddress, const QString& result);
    void initBasicInfo();
    void initPeriphState();
    void writePeripheralDataToCSVFile();
    void writeDataToCSVFile();
    void clearDisplay();
    void bandingMacSn(QString bandingmac, QString bandingsn);
 

    void startFlowWithMac(const QString& mac);



signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);

private slots:
    void getTestValue(const int mechines, const QString value) override;
    void processGetMesTestValue();

    void processInspection(QString stringsn);
    void on_usbconnectButton_clicked();
    void on_usbdisconnectButton_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    // void processReceivedData(const QByteArray &data);
    void on_snInput_returnPressed();
    void on_pushButton_clicked();
    void on_macInput_returnPressed();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_stopTest_clicked();
    void on_visa_test_clicked();
    void on_currentAmmeterVisaApplyButton_clicked();
};

#endif  // QUIESCENT_CURRENT_H
