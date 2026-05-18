#ifndef QFREEWORK_H
#define QFREEWORK_H

#include <QByteArray>
#include <QTimer>
#include <QWidget>

#include <QPair>
#include "Abini.h"
#include "qtupleservice.h"
#include "qapplication.h"
#include "test_base.h"
#include "inovance_h5u_modbus_tcp.h"
#include "ui_qfreework.h"

class QMessageBox;

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
    QString BT_RSSI = "";
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
    QString softwareVersionForReport_;
    bool softwareVersionPassForReport_ = true;
    QString TestResult = "";
  

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
    TupleApplyResult tupleData_;
    QString currentKeyTestName_;
    QString currentKeyExpectedKey_;
    QMessageBox* keyWaitPrompt_ = nullptr;
    bool freeWorkKeyWaiting_ = false;
    bool keyWaitPromptProgrammaticClose_ = false;
    /** 用于取消「PLC 后等 BLE 按键」的超时 singleShot。 */
    quint64 plcKeyBleWaitSeq_ = 0;
    /** PLC 整步成功且仍等协议按键时，保存 PLC 侧摘要供 checkbutton 合并写入 testData。 */
    QString plcKeyBlePlcOkSummary_;
    /** PLC 旋钮整步后仅校验左旋上报（右旋为独立测试项）；phase 3 与 checkbutton 约定一致。 */
    int plcSwitchBlePhase_ = 0;
    /** 仪器应答监听（与 Qproduct::instrument* 信号配合）。 */
    QMetaObject::Connection productInstConn_;
    /** 非空表示正在等「停止接收」应答（PER 步）；与构造函数里长期 connect 的槽配合。 */
    QString productInstrumentStopWaitStepName_;
    void refreshOrderedTestIndexes();
    QVector<int> loadIndexesFromConfig();
    QVector<int> orderedTestIndexes_;
    /** 每步完成追加一条或多条 ASCII 键值（如三元组拆三条），供 MES itemvalue。 */
    QVector<QPair<QString, QString>> freeWorkMesSegments_;
    QByteArray expectedTailSnFromMes;
    void executeFunctionByName(const QString functionName);
    struct NamedFunction {
        int id = -1;
        QString name;
        std::function<void()> function;
        bool needCaseDone = false;
        /** MES 键名，在 testFunction.cpp 的 FREEWORK_TEST_LIST 与本项写在一起（ASCII）。 */
        QString mesTag;
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
        QElapsedTimer caseTimer;

        // 每进入下一步或流程结束时统一复位
        void reset() {
            started = false;
            done = false;
            pass = true;
            functionId = -1;
            testData.clear();
            ask = "通过";
            caseTimer.invalidate();
        }
    } stepRuntime_;
    InovanceH5uModbusTcp inovancePlcTcp_;
    bool isCurrentStep(const QString& functionName) const;
    void appendPeriphItem(QVector<TestItem>& periphTestItems, bool& pass, const QString& name, const QString& value,
                          const QString& expect, bool needCompare);
    void applyTupleByMac();
    /** 三元组未就绪或字段为空时置失败并返回 true（调用方应跳过 sendCommandWithRetry）。 */
    bool failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason);
    void reportTupleWriteRecord();
    void debugUpdateTupleMacStatus();
    void startKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                            const QString& enableKey);
    /** 先 arm 等键与弹窗，再 PLC 整步；PLC 成功后须收到协议按键且 ID 一致才 pass（needCaseDone=true）。 */
    void startPlcKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                               const QString& enableKey, int keyIndex0To6);
    /** PLC 旋钮整步后仅等左旋上报；右旋见独立测试项 startKeyButtonTest。 */
    void startPlcSwitchPlcAndWaitLeftRotate();
    /** 经产品串口发复位帧并等仪器应答；stepName 须与 FREEWORK_TEST_LIST 该项中文名一致（供 isCurrentStep）；空则沿用单步调试用默认名。 */
    void startProductInstrumentResetAndWaitAck(QString stepName = QString());
    /** 发「开始接收」并等 040E0405332000；stepName 须与 FREEWORK_TEST_LIST 中该项中文名一致（供 isCurrentStep）。 */
    void startProductInstrumentStartReceiveForCatalog(const QString& stepName, int profile);
    /** 等待仪器发包后发停止接收并算 PER；stepName 须与 FREEWORK_TEST_LIST 该项中文名一致（供 isCurrentStep）；空则沿用默认名。 */
    void startProductInstrumentStopReceiveAndPer(QString stepName = QString());
    void closeKeyWaitPrompt();
    void runPlcModbusConnectTest();
    /** 与 Untitled-1.cs RunStepActionAsync 一致的单键整步（长连接）；测试项 needCaseDone 须为 true。 */
    void runPlcV3TouchKeyFull(int keyIndex0To6, bool finishStepRuntime = true);
    /** 旋钮整步，与脚本 switch 步一致。finishStepRuntime 为 false 时仅完成 PLC，再由 startPlcSwitchPlcAndWaitLeftRotate 等 BLE。 */
    void runPlcV3TouchSwitchFull(bool finishStepRuntime = true);

    /** 当前自由工位序号（1 起）对应的 PLC M 基底，双工位默认 200 / 220。 */
    int resolvedPlcMBase() const;
    int resolvedPlcSwitchForwardM() const;
    int resolvedPlcSwitchPressM() const;
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
    /** PLC 成功后启动「等协议按键」超时（与 plcKeyBleWaitSeq_ 配合取消）。 */
    void armPlcBleKeyWaitTimeout();
    void syncPlcModbusTraceFromSettings();
    void maybeShowlogPlcSessionSummary(const QString& stepTag);
    /** 每步结束时写入 freeWorkMesSegments_（键名取自 NamedFunction::mesTag）。 */
    void appendFreeWorkMesForCompletedStep(const NamedFunction& nf, bool pass, const QString& testData);
    void clearProductInstrumentWatch();
    bool ensureProductSerialForInstrumentStep(const QString& stepName);
    static QByteArray brushInstrumentStartCmdForProfile(int profile);
    /** 有序队列中 teststate 对应项；越界或未登记返回 nullptr。 */
    const QFreeWork::NamedFunction* currentOrderedNamedFunction() const;
    /** startTask 主循环是否允许 tick：dongle 已连，或无需 dongle 亦可推进的例外（见实现内注释）。 */
    bool canRunOrderedTestStepLoop() const;

private slots:
    void initDate();
    QComboBox* getComNameCombo() override { return ui->comNameCombo; };  // dongle口
    QCheckBox* getIsUseMes() override { return ui->isusemes; };
    QCheckBox* getIsFormMes() override { return ui->isformmes; };
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; };  // usb口（治具）
    QComboBox* getJigcomNameCombo() override { return ui->jigComNameCombo; };  // 治具口
    QComboBox* getProductcomNameCombo() override { return ui->productComNameCombo; };  // 产品串口(仪器)
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
    void refreshTupleData(ProtocolTupleData data) override;
    void checkbutton(ProtocolButtonStateData data) override;
    void refreshBleState(int state) override;
    void getDongleWifi(QString data) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshJigUartState(int state) override;
    void refreshProductUartState(int state) override;
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
    void on_jigConnectButton_clicked();
    void on_jigDisconnectButton_clicked();
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();

    /** 产品仪器停止接收应答（收包数）；逻辑见 qfreework_data.cpp，在构造函数中与 instrumentStopReceiveSeen 连接。 */
    void onProductInstrumentStopReceiveAckForPer(int recvPkts);

    void on_stopTest_clicked();

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
};

#endif  // QFREEWORK_H
