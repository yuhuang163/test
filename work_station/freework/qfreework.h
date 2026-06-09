#ifndef QFREEWORK_H
#define QFREEWORK_H

#include <QByteArray>
#include <QHash>
#include <QPair>
#include <QTimer>
#include <QWidget>

#include "Abini.h"
#include "cmw_gprf_facade.h"
#include "plc_v3_fixture.h"
#include "qapplication.h"
#include "qtupleservice.h"
#include "test_base.h"
#include "test_case_types.h"
#include "ui_qfreework.h"

class QMessageBox;

namespace Ui {
class QFreeWork;
}

class QFreeWorkTestCaseHookRegistrar;

class QFreeWork : public test_base {
    Q_OBJECT
    friend class QFreeWorkTestCaseHookRegistrar;

  public:
    explicit QFreeWork(int index, QWidget* parent = nullptr);
    ~QFreeWork();
    void startTask() override;

    Ui::QFreeWork* ui;

    // clang-format off
    // test_base 控件桥接
    QComboBox* getComNameCombo() override { return ui->comNameCombo; }
    QComboBox* getUsbcomNameCombo() override { return ui->usbcomNameCombo; }
    QComboBox* getJigcomNameCombo() override { return ui->jigComNameCombo; }
    QComboBox* getProductcomNameCombo() override { return ui->productComNameCombo; }
    QCheckBox* getIsUseMes() override { return ui->isusemes; }
    QCheckBox* getIsFormMes() override { return ui->isformmes; }
    QLineEdit* getMacLineEdit() override { return ui->getMac; }
    QLineEdit* macInputLineEdit() override { return ui->macInput; }
    QPlainTextEdit* logEdit() override { return ui->log; }
    QPlainTextEdit* msgEdit() override { return ui->msgEdit; }
    QTableWidget* testResultTable() override { return ui->testResultTable; }
    QLabel* getMesStateQlabel() override { return ui->mes_state; }
    QPushButton* getEndTestButton() override { return ui->stopTest; }
    // clang-format on

    // --- test_case 对外接口 ---
    bool useTestCaseFlow(const QString& stationKey = QString()) const;
    QStringList testCaseFlowItems(const QString& stationKey = QString()) const;
    void setActiveTestCase(const TestCaseDefinition& def);
    void clearActiveTestCase();
    bool isActiveTestCaseStep(const QString& stepLabel) const;
    bool evaluateActiveTestCaseGate(const QString& reportType, const QVariant& payload);
    bool tryCompleteActiveTestCaseTupleCompare(const ProtocolTupleData& data);
    void markActiveTestCaseStepDone(bool pass, const QString& testData, const QString& ask = QString());
    QString activeTestCaseStepTestData() const {
        return stepRuntime_.testData;
    }
    const TestCaseDefinition& activeTestCase() const {
        return activeTestCase_;
    }
    QString currentMacAddress() const;
    QString resolveTestCaseSendPlaceholder(const QString& text) const;
    QVariant resolveTestCaseSendParamTree(const QVariant& param) const;
    bool prepareTupleProductWriteForTestCase(const TestCaseDefinition& def, DeviceCmd cmd, const QVariant& wireParam);
    void executeCloudTupleCase(const TestCaseDefinition& def);
    void executeProductSerialCase(const TestCaseDefinition& def);
    void executeFixturePcbaCase(const TestCaseDefinition& def);
    int resolveFixtureMachineIndex(const QVariant& param) const;

  private:
    int teststate = -1;

    // --- 扫描 / 绑定 ---
    QString receivedData = "";
    QMap<QString, QMap<QString, QString>> deviceMap;
    QString snBinding;
    QString deviceTailSnFromDevice = "";
    QString tailsn = "";
    QString macAddress = "没有mac地址";
    QByteArray expectedTailSnFromMes;

    // --- RSSI / WiFi / 电量 / 电流 ---
    QString BT_RSSI = "";
    QString WIFI_RSSI = "";
    QString BLE_RSSI = "";
    QString wifiresult = "";
    QString voltageresult = "";
    QString currentresult = "";
    QString charageresult = "";
    QString bleresult = "";
    int HighRssi = 0;
    int LowRssi = 0;
    int BleHighRssi = 0;
    int BleLowRssi = 0;
    double standbattary = 0;
    int is_battary_test = 0;
    int RssiTestTime = 0;
    double HighCurrent = 0;
    double LowCurrent = 0;
    quint32 lowKeyCap_ = 0;
    quint32 highKeyCap_ = 0;
    int measure_wait_time = 15000;
    double measure_ammeter = 0;
    QString wifiMac = "";
    int rssitestcount = 0;
    int rssitestfailcount = 0;
    int wifistate = 0;
    bool isovertime = 0;
    bool iscompareovertime = 0;
    int intwifirssi = 0;
    int intblerssi = 0;
    double voltage = 0;
    QString chargestate = "";

    // --- 计时与本轮结论 ---
    QTimer* waittime = new QTimer(this);
    QTimer* comparewaittime = new QTimer(this);
    QElapsedTimer TestTime;
    QString productName;
    QString softwareVersionForReport_;
    bool softwareVersionPassForReport_ = true;
    QString TestResult = "";

    // --- 三元组 / MES 分段 ---
    TupleApplyResult tupleData_;
    QVector<QPair<QString, QString>> freeWorkMesSegments_;

    // --- test_case 运行态 ---
    bool stopFlowOnTestFail_ = true;
    QStringList orderedTestCaseNames_;
    TestCaseDefinition activeTestCase_;
    QString activeTestCaseStepLabel_;
    bool testCaseStepActive_ = false;
    bool testCaseMultiGateTableEmitted_ = false;
    struct TestCaseStepResult {
        bool done = false;
        bool pass = true;
        QString testData;
    };
    TestCaseStepResult testCaseStepResult_;

    // --- 有序测试队列（test_case 编排） ---
    void refreshOrderedTestIndexes();
    struct StepRuntime {
        bool started = false;
        bool done = false;
        bool pass = true;
        QString testData;
        QString ask = "通过";
        QElapsedTimer caseTimer;
        void reset() {
            started = false;
            done = false;
            pass = true;
            testData.clear();
            ask = "通过";
            caseTimer.invalidate();
        }
    } stepRuntime_;
    bool currentOrderedStepIsDongleBleConnect() const;
    bool canRunOrderedTestStepLoop() const;
    void runTestFlowBootstrap();
    bool tickOrderedTestStepLoop();
    void finalizeTestFlowIfComplete();
    bool isCurrentStep(const QString& functionName) const;
    bool isCurrentInstrumentStep(const QString& stepName) const;
    bool isBydFactory() const;
    QString resolvedExpectedTailSnText() const;
    QByteArray resolvedTailSnToWrite() const;

    // --- 弹窗 / 按键等待 ---
    QString currentKeyTestName_;
    QString currentKeyExpectedKey_;
    QMessageBox* keyWaitPrompt_ = nullptr;
    QMessageBox* testCasePrompt_ = nullptr;
    bool testCasePromptAcknowledged_ = false;
    bool testCasePromptProgrammaticClose_ = false;
    bool freeWorkKeyWaiting_ = false;
    bool keyWaitPromptProgrammaticClose_ = false;
    quint64 plcKeyBleWaitSeq_ = 0;
    QString plcKeyBlePlcOkSummary_;
    int plcSwitchBlePhase_ = 0;
    bool plcKeyCapPollMode_ = false;
    int currentKeyCapRequestKk_ = -1;
    int currentKeyConfiguredId_ = 0;
    bool plcKeyCapSyncReadPending_ = false;
    bool plcKeyCapSyncReadOk_ = false;
    quint32 plcKeyCapSyncReadValue_ = 0;
    int plcKeyCapSyncReadAuxId_ = -1;
    QString plcKeyCapPassSummary_;
    /** 治具仍下压时多次读电容，取最大值卡控；在 runPlcV3TouchKeyFull 内调用。 */
    bool pollKeyCapDuringPress(QString* errOut, QString* outSummary);
    /** 仪器应答监听（与 Qproduct::instrument* 信号配合）。 */
    QMetaObject::Connection productInstConn_;
    /** 非空表示正在等「停止接收」应答（PER 步）；与构造函数里长期 connect 的槽配合。 */
    QString productInstrumentStopWaitStepName_;
    /** 最近一次「产品串口开始接收」用到的 profile（0～5），供 PER 步与 CMW 切频对齐。 */
    int lastBrushInstrumentProfile_ = -1;
    /** 自上次「开始接收」后是否已在「并联CMW播放*」步成功打过 GPRF；PER 步据此避免重复播放。 */
    bool cmwGprfBurstDoneSinceStartRx_ = false;
    /** 自由工站 VISA：地址仅来自界面；并联 CMW 时强制 RxCmwInstrument profile。 */
    void applyVisaForCmwBurst();
    QString currentVisaAddress() const;
    VisaDeviceProfile currentVisaDeviceProfileFromUi() const;
    void syncVisaUiFromFreeWorkSettings();
    void refreshVisaResourceCombo();
    void applyVisaFromUi(bool testConnection);
    bool freeWorkCmwVisaWrite(const QString& cmd);
    bool freeWorkCmwVisaQuery(const QString& cmd, QString* response);
    /** 并联 GPRF：`alignedPostTrigHoldMs>=0` 时 TRIG 后固定等待使用该值（与 BrushInstrument/PacketPhaseWaitMs 同源）；若为真且 **`outAlignedWaitDoneByCmw` 输出 true**，表示已在仪器路径内阻塞等满，PER 勿再延时 waitPacketMs。仅当 **未** 开 `BlePer/CmwWaitArbScount` 时才能把「对齐等满」记在 CMW 内。 */
    /** `ranCmwBurst`：非空时在入口清零；仅在成功执行完 **`freeWorkRunSingleCmwBurstAtMhz`** 后置真（配置跳过或未打突发时保持假）。 */
    bool freeWorkInstrumentBleBrushCmwBurstIfEnabled(const QString& scenarioLabel, int brushProfile, QString* errorMessage,
                                                     int alignedPostTrigHoldMs = -1, bool* outAlignedWaitDoneByCmw = nullptr,
                                                     bool* ranCmwBurst = nullptr);
    /** 首次按 BlePer/Cmw* 写 ARB（与 wifibletest::initializeBlePerCmwGprf 一致）。 */
    bool freeWorkPrimeInstrumentCmwGprf(QString* errorMessage);
    /** 轮询 SOURce:GPRF:GEN:ARB:SCOunt? 至目标 cycles；`outElapsedMs` 若非空写入从进入轮询到成功返回的总耗时(ms)。 */
    bool freeWorkWaitBleCmwArbComplete(const QString& scenarioLabel, QString* errorMessage, int* outElapsedMs = nullptr);
    /** 单次切频、`TRIGger:...MANual:EXECute`→`STAT ON`（对标 docs/cmw100rx）、可选 SCOunt 轮询、`OFF`。`postTrigHoldMsOverride>=0` 时用于积包毫秒补足。 */
    bool freeWorkRunSingleCmwBurstAtMhz(int freqMhz, const QString& scenarioLabel, QString* errorMessage,
                                        int postTrigHoldMsOverride = -1);
    /** 并联 CMW：对单个 brush profile（0～5）打一发 GPRF；GPRF ARB 侧仅首次会做完整初始化（gInstrumentCmwGprfPrimed）。 */
    bool runFreeInstrumentBleCmwBurstForBrushProfile(QString* detail, int brushProfile);
    void refreshOrderedTestIndexes();
    QVector<int> loadIndexesFromConfig();
    QVector<int> orderedTestIndexes_;
    QStringList orderedTestCaseNames_;
    /** 工站级：任一步失败是否结束整单流程（来自 总的测试流程.ini） */
    bool stopFlowOnTestFail_ = true;
    bool useTestCaseFlow_ = false;
    struct TestCaseStepResult {
        bool done = false;
        bool pass = true;
        QString testData;
    };
    TestCaseDefinition activeTestCase_;
    QString activeTestCaseStepLabel_;
    TestCaseStepResult testCaseStepResult_;
    bool testCaseStepActive_ = false;
    /** 治具/外设多项卡控已在 evaluate 时写入结果表，步骤结束不再追加汇总单行 */
    bool testCaseMultiGateTableEmitted_ = false;
    void onTestCaseStepMarkedDone(bool pass, const QString& testData, const QString& ask);
    void emitFixtureMultiGateTableRows(const QVector<TestCaseGate>& gates, const QString& reportType,
                                       const QVariant& payload, bool& allPass, QString& detailOut);
    void appendTestCaseMes(const TestCaseDefinition& def, bool pass, const QString& testData);
    /** 每步完成追加一条或多条 ASCII 键值（如三元组拆三条），供 MES itemvalue。 */
    QVector<QPair<QString, QString>> freeWorkMesSegments_;
    QByteArray expectedTailSnFromMes;
    /** 写整机 SN：优先 MES 缓存 expectedTailSnFromMes，否则 SN 输入框 getMac。 */
    QByteArray resolvedTailSnToWrite() const;
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
    /** 产品串口仪器步骤：兼容旧宏流程（functionId）与 test_case 流程（activeTestCaseStepLabel_）。 */
    bool isCurrentInstrumentStep(const QString& stepName) const;
    void appendPeriphItem(QVector<TestItem>& periphTestItems, bool& pass, const QString& name, const QString& value,
                          const QString& expect, bool needCompare);
    void applyTupleByMac();
    /** BYD AddSfcKey：单条关键物料（DATA_NAME/DATA_VALUE/QTY）。 */
    void reportBydSfcKey(const QString& dataName, const QVariant& dataValue, int qty = 1);
    /** 蓝牙测试：三元组成功后上报 SN / 三元组 / mac 共 5 条关键物料。 */
    void reportBydBluetoothMesKeyMaterials();
    /** 三元组未就绪或字段为空时置失败并返回 true（调用方应跳过 sendCommandWithRetry）。 */
    bool failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason);
    void reportTupleWriteRecord();
    void debugUpdateTupleMacStatus();
    void startKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                            const QString& enableKey);
    void startPlcKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                               const QString& enableKey, int keyIndex0To6, bool useCapacitanceRead = false);
    void startPlcSwitchPlcAndWaitRightRotate();
    void closeKeyWaitPrompt();
    void showTestCasePromptForStep(const TestCaseDefinition& def);
    void closeTestCasePrompt();
    void onTestCasePromptAcknowledged();
    void onTestCaseStepMarkedDone(bool pass, const QString& testData, const QString& ask);
    void armPlcBleKeyWaitTimeout();
    void waitPlcBleKeyReportBlocking();
    bool pollKeyCapDuringPress(QString* errOut, QString* outSummary);
    void resetPlcKeyCapSyncReadState();

    // --- PLC / CMW 协议（agreement 统一入口，工站只调度命令） ---
    PlcV3Fixture plcFixture_;
    CmwGprfFacade cmwFacade_;
    PlcV3RunParams makePlcRunParams(int keyIndex0To6 = 0);
    CmwGprfRunParams makeCmwRunParams(const QString& scenarioLabel = QString(), int brushProfile = -1);
    void applyPlcStepResult(const PlcV3RunResult& result, PlcV3Command command, bool finishStepRuntime = true);
    PlcV3RunResult runPlcV3(PlcV3Command command, int keyIndex0To6 = 0, bool finishStepRuntime = true);
    CmwGprfRunResult runCmwGprf(CmwGprfCommand command, const QString& scenarioLabel = QString(), int brushProfile = -1,
                                int alignedPostTrigHoldMs = -1, bool* outRanBurst = nullptr);
    void runPlcModbusConnectTest();
    void runPlcSwitchTestDoneResetM();
    void runPlcV3TouchKeyFull(int keyIndex0To6, bool finishStepRuntime = true);
    void runPlcV3TouchSwitchFull(bool finishStepRuntime = true);
    bool runFreeInstrumentBleCmwBurstForBrushProfile(QString* detail, int brushProfile);

    // --- 产品串口 / 仪器步骤 ---
    QMetaObject::Connection productInstConn_;
    QString productInstrumentStopWaitStepName_;
    int lastBrushInstrumentProfile_ = -1;
    void clearProductInstrumentWatch();
    bool ensureProductSerialForInstrumentStep(const QString& stepName);
    static QByteArray brushInstrumentStartCmdForProfile(int profile);
    void startProductInstrumentResetAndWaitAck(QString stepName = QString());
    void startProductInstrumentStartReceiveForCatalog(const QString& stepName, int profile);
    void startProductInstrumentStopReceiveAndPer(QString stepName = QString());

    // --- 协议回包 / 三元组 / BYD（部分实现见 qfreework_data.cpp） ---
    void appendPeriphItem(QVector<TestItem>& periphTestItems, bool& pass, const QString& name, const QString& value,
                          const QString& expect, bool needCompare);
    void applyTupleByMac();
    void reportBydSfcKey(const QString& dataName, const QVariant& dataValue, int qty = 1);
    void reportBydBluetoothMesKeyMaterials();
    bool failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason);
    void reportTupleWriteRecord();
    void debugUpdateTupleMacStatus();
    void emitFixtureMultiGateTableRows(const QVector<TestCaseGate>& gates, const QString& reportType,
                                       const QVariant& payload, bool& allPass, QString& detailOut);
    void applyRuntimeSnGateExpected(QVector<TestCaseGate>& gates);
    void appendTestCaseMes(const TestCaseDefinition& def, bool pass, const QString& testData);

  private slots:
    void initData();

    // 协议上行（实现见 qfreework_data.cpp）
    void refreshBleRssi(QString data) override;
    void refreshWifiMsg(QString data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshBattaryData(ProtocolBatteryData data) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshRssiRead(ProtocolRssiData data) override;
    void refreshChargeCurrentRead(ProtocolChargeCurrentData data) override;
    void refreshKeySignalRead(ProtocolKeyCapData data) override;
    void refreshTupleData(ProtocolTupleData data) override;
    void refreshButton(ProtocolButtonStateData data) override;
    void refreshAmmeterData(QString data) override;
    void refreshWifiState(int state);
    void onProductInstrumentStopReceiveAckForPer(int recvPkts);

    // 串口 / WiFi / MES / 绑定（实现见 qfreework.cpp）
    void refreshDongleWifi(QString data) override;
    void refreshBleState(int state) override;
    void refreshDongleUartState(int state) override;
    void refreshUsbUartState(int state) override;
    void refreshJigUartState(int state) override;
    void refreshProductUartState(int state) override;
    void getTestValue(const int mechines, const QString value) override;
    void bindingMacSn(QString bindingMac, QString bindingSn);
    void bindingMacSnMes(QString bindingMac, QString bindingSn);
    void updateComboBox() override;
    void getMacAddress(const QByteArray& byte);
    void processInspection(QString inputSnText);
    void processGetMesTestValue();
    void getMac(QString sn_to_search);

    // UI 槽（.ui 自动连接 + 手工槽）
    void on_getMac_returnPressed();
    void on_macInput_returnPressed();
    void on_snbanding_returnPressed();
    void on_mac_combo_textActivated(const QString& arg1);
    void on_clear_scan_clicked();
    void on_connectwifi_clicked();
    void on_disconnectwifi_clicked();
    void on_connectButton_clicked();
    void on_disconnectButton_clicked();
    void on_jigConnectButton_clicked();
    void on_jigDisconnectButton_clicked();
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void on_visaRefreshButton_clicked();
    void on_visaApplyButton_clicked();

    /** 产品仪器停止接收应答（收包数）；逻辑见 qfreework_data.cpp，在构造函数中与 instrumentStopReceiveSeen 连接。 */
    void onProductInstrumentStopReceiveAckForPer(int recvPkts);

    void on_stopTest_clicked();

  signals:
    void send_go_next_focus();
    void send_start_test(int data);
    void send_go_next_test(int data);
};

#endif // QFREEWORK_H
