#ifndef QFREEWORK_H
#define QFREEWORK_H

#include <QByteArray>
#include <QEvent>
#include <QHash>
#include <QImage>
#include <QPair>
#include <QElapsedTimer>
#include <QShowEvent>
#include <QTimer>
#include <QWidget>

#include "Abini.h"
#include "cmw_gprf_facade.h"
#include "plc_v3_facade.h"
#include "qapplication.h"
#include "qtupleservice.h"
#include "test_base.h"
#include "test_case_types.h"
#include "ui_qfreework.h"

class QMessageBox;
class QCustomPlot;
class QCPItemText;

namespace Ui {
class QFreeWork;
}

class QFreeWorkTestCaseHookRegistrar;
class TestCaseRunner;

struct QFreeWorkMesSegment {
    QString name;
    QString value;
    QString maxValue;
    QString minValue;
    QString standardValue;
    QString unit;
    QString result;
};

class QFreeWork : public test_base {
    Q_OBJECT
    friend class QFreeWorkTestCaseHookRegistrar;
    friend class TestCaseRunner;
  public:
    explicit QFreeWork(int index, QWidget* parent = nullptr);
    ~QFreeWork();
    void startTask() override;
    void startTest() override;
    /**
     * 设置页功能块右键「运行」：只执行指定步骤（不过站、结束后不断开连接）。
     * @return false 时 errorOut 为原因（如正在测试中、步骤不存在）
     */
    bool runSingleTestCaseStep(const QString& stationKey, const QString& caseName, QString* errorOut = nullptr);
    /** 设置页切换工站后：重载有序步骤与串口显隐（含 applyStationSerialUiConfig） */
    void refreshStationFlowUi() { refreshOrderedTestIndexes(); }

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
    bool isActiveTestCaseStepDone() const {
        return testCaseStepResult_.done;
    }
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
    QString parseMacFromSn(const QString& snCode) const;
    /** 扫码框 / MES 下发的 PCBA SN（非整机 SN） */
    QString resolvedPcbaSnText() const;
    /** 本地整机 SN（三元组申请等写入，可扩展其它来源） */
    QString resolvedWholeMachineSnText() const;
    void setWholeMachineSn(const QString& sn);
    QString resolveTestCaseSendPlaceholder(const QString& text) const;
    QVariant resolveTestCaseSendParamTree(const QVariant& param) const;
    bool prepareTupleProductWriteForTestCase(const TestCaseDefinition& def, DeviceCmd cmd, const QVariant& wireParam);
    bool prepareTailSnWriteForTestCase(const TestCaseDefinition& def, DeviceCmd cmd, const QVariant& wireParam);
    void executeCloudTupleCase(const TestCaseDefinition& def);
    void executeProductSerialCase(const TestCaseDefinition& def);
    void executeFixturePcbaCase(const TestCaseDefinition& def);
    void executeFixtureAsd9026aCase(const TestCaseDefinition& def);
    void executeFixtureXwdCase(const TestCaseDefinition& def);
    void executeFixtureJieliBtBoxCase(const TestCaseDefinition& def);
    int resolveFixtureMachineIndex(const QVariant& param) const;
    QVariantMap cachedHuilingVisaLink() const;
    void updateHuilingVisaLinkCache(const QVariantMap& link);
    void seedHuilingVisaLinkCacheFromFlowOrSettings();
    void onUsbInstrumentReport(const ProtocolReport& report) override;

  private:
    int teststate = -1;

    // --- 扫描 / 绑定 ---
    QString receivedData = "";
    QString snBinding;
    QString deviceTailSnFromDevice = "";
    QString wholeMachineSn_;
    /** BYD 开局扫码过程码：MES Start/Complete/AddSfcKey 的 SFC；勿被 MES 主板 SN / 三元组整机 SN 冲掉 */
    QString mesProcessCode_;
    QString macAddress = "没有mac地址";

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
    /** 界面计时独立刷新：吸力采样等步骤会在一次 startTask 里阻塞十几秒，靠主任务环刷会停表 */
    QTimer* testTimeTicker_ = new QTimer(this);
    QElapsedTimer TestTime;
    QString productName;
    QString softwareVersionForReport_;
    bool softwareVersionPassForReport_ = true;
    QString TestResult = "";

    // --- 三元组 / MES 分段 ---
    TupleApplyResult tupleData_;
    QVector<QFreeWorkMesSegment> freeWorkMesSegments_;

    // --- test_case 运行态 ---
    bool stopFlowOnTestFail_ = true;
    /** 设置页单步调试：结束后跳过 MES 过站与断连 */
    bool singleStepDebugRun_ = false;
    QString activeFlowStationKey_;
    QStringList orderedTestCaseNames_;
    /** 测试失败执行区域（StopFlowOnTestFail 触发后按序执行） */
    QStringList orderedFailCaseNames_;
    /** false=主流程，true=失败执行区 */
    bool runningFailFlow_ = false;
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
    const TestCaseStepResult& testCaseStepResult() const {
        return testCaseStepResult_;
    }

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
    void beginUiStartTest();
    /** 主动 BleDisconnect 后禁止 startTask 里用当前 MAC 自动重连，直到显式扫描/直连或新一轮测试 */
    bool suppressProductBleAutoReconnect_ = false;
    void runTestFlowBootstrap();
    bool tickOrderedTestStepLoop();
    void finalizeTestFlowIfComplete();
    const QStringList& activeOrderedCaseNames() const;
    QStringList& activeOrderedCaseNames();
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
    bool testCaseCommandBegun_ = false;
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
    void startKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                            const QString& enableKey);
    void startPlcKeyButtonTest(const QString& testName, const QString& promptText, const QString& expectedKey,
                               const QString& enableKey, int keyIndex0To6, bool useCapacitanceRead = false);
    void startPlcSwitchPlcAndWaitRightRotate();
    void closeKeyWaitPrompt();
    void showTestCasePromptForStep(const TestCaseDefinition& def);
    void closeTestCasePrompt();
    /** accepted=true 点「是」；false 点「否」或关闭 → 纯弹窗判失败。 */
    void onTestCasePromptAcknowledged(bool accepted = true);
    void onTestCaseStepMarkedDone(bool pass, const QString& testData, const QString& ask);
    void armPlcBleKeyWaitTimeout();
    void waitPlcBleKeyReportBlocking();
    bool pollKeyCapDuringPress(QString* errOut, QString* outSummary);
    void resetPlcKeyCapSyncReadState();

    // --- 业务门面（business/，工站只调度命令） ---
    PlcV3Facade plcFacade_;
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
    void resetTuplePositionHighlight();
    void updateTuplePositionHighlight(const QString& position);
    /** 流程含 ApplyTupleByMac 或 Qaiot 读写设备数据时显示三元组位置行 */
    void updateTuplePositionUiVisible();
    /** 主界面「三元组位置」可点击：写入当前工站配置并同步相关步骤 Param_side */
    void setupTuplePositionClickable();
    void loadAndApplyStationDeviceSide();
    void applyTuplePositionSelection(const QString& positionCode);
    int syncDeviceSideToStationSteps(int sideId, const QString& positionCode);
    /** 按当前工站 flow.ini [SerialUi] 刷新治具/产品/万用表串口行显隐与标签 */
    void applyStationSerialUiConfig();
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void reportBydSfcKey(const QString& dataName, const QVariant& dataValue, int qty = 1);
    void reportBydBluetoothMesKeyMaterials();
    /** MES GetCustomData：取 DATA 中 NAME=ROOTSKU 的 VALUE，写入 pack.sku */
    void fetchMesRootSku();
    /** BYD：日志页右侧显示 mes_config.ini 的 Resource（工单） */
    void refreshBydMesResourceDisplay();
    bool failTupleWriteIfNoValidField(const QString& stepName, bool fieldOk, const QString& emptyReason);
    void reportTupleWriteRecord();
    void debugUpdateTupleMacStatus(const TestCaseDefinition& def);
    void emitFixtureMultiGateTableRows(const QVector<TestCaseGate>& gates, const QString& reportType,
                                       const QVariant& payload, bool& allPass, QString& detailOut);
    void applyRuntimeSnGateExpected(QVector<TestCaseGate>& gates);
    /** Gate/Expected 中的 $MAC/$SN 等占位符展开为运行时值（$MAC=界面 MAC 框） */
    void applyRuntimePlaceholderGateExpected(QVector<TestCaseGate>& gates);
    void appendTestCaseMes(const TestCaseDefinition& def, bool pass, const QString& testData);
    /** 多字段卡控（如杰理 RSSI/频偏）按分项各写一条 MES，与结果表行对齐 */
    void appendMultiGateTestCaseMes(const QVector<TestCaseGate>& gates, const QString& reportType,
                                    const QVariant& payload);
    void applyFreeWorkExtraTabsVisible(bool visible);
    bool isFreeWorkXwdKeyStation() const;
    void loadSuctionGateSettings();
    /** 从步骤 send.param 读取采样时长/通道等（卡控范围以 Gate 为准；无 Gate 时回退 Param）。 */
    void applySuctionGateFromStepParam(const QVariant& param);
    void runDongleSuctionSampleStep();
    /** Dongle 单通道吸力采样；判定走 ProtocolDongleSuctionPeakData Gate。 */
    void runDongleSuctionSampleSingleStep();
    /** 自由工站屏幕检测：GigE/USB 拍照后坏点 / 显示对比 / 位置校准。 */
    void runScreenInspectStep();
    /**
     * 屏幕图像识别自动未通过时弹窗：确认是否显示对应颜色。
     * 点「是」=是该颜色→返回 true（通过）；点「否」=不是→返回 false（不通过）。
     */
    bool screenInspectAskHumanPassOnAutoFail(const QString& expectedColorName);
    void rememberScreenInspectImages(const QImage& capture, const QImage& annotated, const QImage& reference,
                                     const QString& folder, bool calibrationGuides = false);
    void updateScreenInspectPreview();
    void showScreenInspectViewer();
    void openScreenInspectFolder();
    /**
     * 程控电源读电流：连续采样若干秒，期间任一值卡控合格即通过。
     * 每次读数经 ProtocolMeasureData(Current) 上报后由 onUsbInstrumentReport 软判定。
     */
    void runScpiProgrammableCurrentSampleAnyMatch(const TestCaseDefinition& def, const QVariant& commandParam);
    void runAsdProgrammableCurrentSampleAnyMatch(const TestCaseDefinition& def, quint8 moduleAddr, int channel);
    /** 治具/USB 电流表 Hook「读取治具电流测量值」：连续采样，任一值落入 SETTINGS 电流卡控即通过。 */
    void runJigAmmeterCurrentSampleAnyMatch();
    /** XWD 治具读电流：连续下发/收包采样，Gate 任一合格即通过。 */
    void runXwdFixtureCurrentSampleAnyMatch(const TestCaseDefinition& def, const QByteArray& request, int readChannel,
                                            bool dualFixture, int perReadTimeoutMs);
    /** HQ/LX Modbus 电流表：连续读数，Gate 任一合格即通过。 */
    void runModbusAmmeterCurrentSampleAnyMatch(const TestCaseDefinition& def, ModbusDeviceRoute route);
    /**
     * 多路温度仪多通道窗口：在 sampleDurationMs 内轮询本工位通道列表。
     * 默认同轮全部通道落入 Gate 才通过；Param_tempPassMode=any 时任一路达标即通过。
     */
    void runMultiTempLoggerChannelsWindowAllMatch(const TestCaseDefinition& def);
    void setDongleSuctionReadEnabled(bool enabled);
    void initSuctionChart();
    void resetSuctionChart();
    void appendSuctionChartSample(double leftKpa, double rightKpa);
    /** 采样结束做一次完整 setData + 轴范围 + replot，避免过程中每帧全量重绘 */
    void finalizeSuctionChartPlot();
    void updateSuctionPeakLabels();

    double suctionPeakTargetKpa_ = -36.0;
    double suctionPeakToleranceKpa_ = 2.6;
    double suctionPeakDiffMaxKpa_ = 2.6;
    /** 单通道采样口：0=CH1，1=CH2，2=CH3（Param_channel=1|2|3）。 */
    int suctionSingleChannelIndex_ = 0;
    int suctionSampleDurationMs_ = 10000;
    int suctionSampleIntervalMs_ = 20;
    /** 单通道周期峰识别：回到该压力以上视为基线结束本周期（同主界面 Dongle 峰监视缺省）。 */
    double suctionPeakBaselineKpa_ = -8.0;
    /** 低于该压力才计入吸气周期，避免噪声误计为峰。 */
    double suctionPeakDipStartKpa_ = -10.0;
    /** 采样窗口内至少需要的完整「吸→回放」周期峰个数（单/双通道共用，默认 3）。 */
    int suctionMinPeakCount_ = 3;
    /** 吸力修正：界面/曲线/判定 = 原始 kPa + 本值，步骤 Param_offsetKpa，默认 0 */
    double suctionOffsetKpa_ = 0.0;
    bool dongleSuctionReadEnabled_ = false;
    bool dongleSuctionSampleActive_ = false;
    /** 读电流连续采样：窗口内任一卡控合格即通过，不合格不立刻结束步骤。 */
    bool currentSampleAnyMatchActive_ = false;
    int currentSampleCount_ = 0;
    QString currentSampleLastValueText_;
    QVector<double> dongleSuctionCh1Samples_;
    QVector<double> dongleSuctionCh2Samples_;
    QVector<double> dongleSuctionCh3Samples_;
    /** 与 dongleSuctionCh*Samples_ 同索引的采样相对秒；CSV 勿用 suctionChartTimeSec_，
     *  后者在「开启dongle吸力读取」步就开始累积，会比采样窗口的点多而错位 */
    QVector<double> dongleSuctionSampleTimeSec_;
    QElapsedTimer dongleSuctionSampleTimer_;
    double dongleSuctionLastCh1Kpa_ = 0.0;
    double dongleSuctionLastCh2Kpa_ = 0.0;
    double dongleSuctionLastCh3Kpa_ = 0.0;
    QCustomPlot* suctionPlot_ = nullptr;
    /** 采样中在空图上显示的提示，避免被当成曲线画不出来 */
    QCPItemText* suctionChartHintText_ = nullptr;
    QVector<double> suctionChartTimeSec_;
    QVector<double> suctionChartLeftKpa_;
    QVector<double> suctionChartRightKpa_;
    QElapsedTimer suctionChartTimer_;
    bool suctionChartTimerStarted_ = false;
    /** 实时数值标签节流：约 2Hz（曲线不实时画，测完一次性绘制） */
    qint64 suctionChartLastUiMs_ = 0;
    bool suctionLeftPeakInit_ = false;
    bool suctionRightPeakInit_ = false;
    double suctionLeftPeakHigh_ = 0.0;
    double suctionLeftPeakLow_ = 0.0;
    double suctionRightPeakHigh_ = 0.0;
    double suctionRightPeakLow_ = 0.0;
    /** 本轮回放中已配置的会凌 VISA 连接（地址/电压/电流等），开关步骤可复用。 */
    QVariantMap huilingVisaLinkCache_;
    /** 最近一次 USB 摄像头屏幕拍照，供「屏幕拍照」页预览/大图。 */
    QImage screenInspectCapture_;
    QImage screenInspectAnnotated_;
    QImage screenInspectReference_;
    QString screenInspectFolder_;
    /** true=校准步骤：左右均为校准画线对照图。 */
    bool screenInspectCalibGuides_ = false;

  private slots:
    void initData(bool deferDongleAtForVisa = false);

    // 协议上行（实现见 qfreework_data.cpp）
    void refreshBleRssi(QString data) override;
    void refreshWifiMsg(QString data) override;
    void refreshBaseData(ProtocolBaseInfoData data) override;
    void refreshBattaryData(ProtocolBatteryData data) override;
    void refreshSn(ProtocolSnData data) override;
    void refreshMacData(ProtocolMacData data) override;
    void refreshPeriphData(ProtocolPeriphStateData data) override;
    void refreshRssiRead(ProtocolRssiData data) override;
    void refreshChargeCurrentRead(ProtocolChargeCurrentData data) override;
    void refreshKeySignalRead(ProtocolKeyCapData data) override;
    void refreshTupleData(ProtocolTupleData data) override;
    void refreshButton(ProtocolButtonStateData data) override;
    void refreshAiotImuCali(ProtocolAiotImuCaliData data) override;
    void refreshAiotFsensorCali(ProtocolAiotFsensorCaliData data) override;
    void refreshAiotExceptionThreshold(ProtocolAiotExceptionThresholdData data) override;
    void refreshAiotPumpParam(ProtocolAiotPumpParamData data) override;
    void refreshAiotHeatTest(ProtocolAiotHeatTestData data) override;
    void refreshAiotVibrationTest(ProtocolAiotVibrationTestData data) override;
    void refreshAiotCycleReportConfig(ProtocolAiotCycleReportConfigData data) override;
    void refreshAiotCycleReport(ProtocolAiotCycleReportData data) override;
    void refreshRootBatteryTemp(quint8 temp) override;
    void refreshRootHeatTemp(quint8 temp) override;
    void refreshResultCode(ProtocolResultData data) override;
    void refreshFlangeStatus(ProtocolTypeData data) override;
    void refreshPumpStallCurrent(ProtocolPumpStallCurrentData data) override;
    void refreshRootAgingHistory(ProtocolRootAgingHistoryData data) override;
    void refreshTypeStatus(ProtocolTypeData data) override;
    void refreshAmmeterData(QString data) override;
    void refreshDongleSuctionData(ProtocolDongleSuctionData data) override;
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
    void on_usbconnectButton_clicked();
    void on_usbdisconnectButton_clicked();
    void on_productConnectButton_clicked();
    void on_productDisconnectButton_clicked();
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();
    void on_stopTest_clicked();
    void on_toggleExtraTabsButton_clicked();
    void on_clearSuctionChartButton_clicked();
    void on_viewScreenInspectLargeButton_clicked();
    void on_openScreenInspectFolderButton_clicked();

  signals:
    void send_go_next_focus();
    void send_start_test(int data);
    void send_go_next_test(int data);
};

#endif // QFREEWORK_H
