#ifndef SUCTION_H
#define SUCTION_H

#include "test_base.h"
#include "ui_suction.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

#include "Abini.h"
#include "testmodel.h"
#include <QVector>

namespace Ui {
    class suction;
}

class suction : public test_base {
    Q_OBJECT

public:
    explicit suction(int index, QWidget* parent = nullptr);
    ~suction();
    Ui::suction* ui;
    ImuDataT orgData;
    void startTask() override;
    void processReceivedData(const QByteArray& data) override;
    void onUsbSerialFrame(const QByteArray& data) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshJigUartState(int state) override;
    void refreshProductUartState(int state) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshBleState(int state) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshMusicState(ProtocolMusicStateData data) override;
    // void refreshfwVersion(QString data) override;
    void refreshAmmeterData(QString data) override;
    void refreshProgrammablePowerVoltage(double valueVolts, bool ok);
    void refreshProgrammablePowerCurrent(double valueAmps, bool ok);
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
    void applySuctionProtocolConfig();
    void loadSuctionProgrammablePowerConfig();
    /// 外接程控电源输出开关（VISA）
    bool setExternalProgrammablePowerOutput(bool enable);
    QByteArray sn;
    double HighSuction;
    double LowSuction;
    QString M_USERNO;
    QString stringsn;
    QString pumpsoft_version;

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
    Qusb::ProtocolType suctionProtocolType = Qusb::ProtocolType::Scpi;
    // DAM-3158 采集与换算参数（通道为 1-based）
    int damRangeCode = 0x000C;
    double damRawMax = 65535.0;
    double damCurrentFullScale_mA = 10.0;
    double damPressureAtMinCurrent_kPa = -100.0;
    double damPressureAtMaxCurrent_kPa = 0.0;
    QVector<double> damRawChannels_;
    double damLeftKpa_ = 0.0;
    double damRightKpa_ = 0.0;
    bool suctionUsePicoSensor = true;
    QString picoRxBuffer_;

    // 双通道测试参数
    int damLeftChannel = 1;
    int damRightChannel = 2;
    int suctionSampleDurationMs = 10000;
    int suctionSampleIntervalMs = 20;
    double suctionPeakTargetKpa = 36.0;
    double suctionPeakToleranceKpa = 2.6;
    double suctionPeakDiffMaxKpa = 2.6;
    bool suctionExternalPowerEnabled = false;
    int suctionPowerOnWaitMs = 5000;
    Qvisa::ProtocolConfig programmablePowerVisaConfig_;
    double programmablePowerMeasuredVoltageV_ = 0.0;
    double programmablePowerMeasuredCurrentA_ = 0.0;
    bool programmablePowerVoltageReadOk_ = false;
    bool programmablePowerCurrentReadOk_ = false;
    Qusb::ProtocolConfig suctionUsbProtocolConfig_;
    typedef enum {
        STATE_IDLE,               // 休眠状态
        STATE_WATI_CONNECT,       // 等待 BLE 连接
        STATE_BANDING,            // SN 绑定
        STATE_SET_TEST_MODE,      // 设置测试模式（工厂模式）
        STATE_VERIFY_TEST_MODE,   // 校验测试模式
        STATE_VERIFY_SUCTION_MODE,  // 校验吸力模式
        STATE_SUCTION_TEST,  // 吸力等测量
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
    int suction_wait_time = 15000;
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
    /// SN 校验通过后走 MES 站前与 BLE 等；snText 为唯一来源（手输/扫码或 MES 返回的真 SN），不从输入框再读
    void continueSnInputAfterSnValidated(const QString& snText);
    void reportBydSfcKey(const QString& dataName,
                         const QVariant& dataValue,
                         int qty = 1);
    bool sendPicoAtCommand(const QString& cmd);
    void sendPicoSysModeStart();
    void sendPicoSysModeStop();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);

private slots:
    void processInspection(QString stringsn);
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void on_usbconnectButton_clicked();
    void on_usbdisconnectButton_clicked();
    /// 将界面程控电源 VISA 项写入 VisaPower/ 并重新 apply
    void on_suctionPowerVisaApplyButton_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_jigConnectButton_clicked();
    void on_jigDisconnectButton_clicked();
    // void processReceivedData(const QByteArray &data);
    void on_snInput_returnPressed();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_macInput_returnPressed();
    void on_pushButton_3_clicked();
    void on_pushButton_4_clicked();
    void on_stopTest_clicked();
    void processGetMesTestValue();
    void getTestValue(const int mechines, const QString value) override;
    /// 开发用：触发 BYD MES GetCustomData（bydmes::GetTestData）
    void on_snruler_formes_clicked();
    /// 开发用：模拟站前检测（sendProcessInspection → BYD Start）
    void on_start_formes_clicked();
    /// 开发用：模拟 PASS 过站上报（send_end_testPass → BYD TestDataCollect + Complete）
    void on_testdata_formes_clicked();
};

#endif  // SUCTION_H
