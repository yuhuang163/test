#ifndef TEST_BASE_H
#define TEST_BASE_H

#include <functional>

#include <qlog.h>

#include "Abini.h"
#include "agreement/qProtocol/qprotocolmanager.h"
#include "test_case_types.h"
#include "qcheckbox.h"
#include "qheaderview.h"
#include "qlabel.h"
#include "qpushbutton.h"
#include "qtablewidget.h"
#include "qat.h"
#include "qjig.h"
#include "qusb.h"
#include "qvisa.h"
#include "serial_channel.h"

extern "C" {
#include "lib/nfc/dcrf32.h"
}

/** sendCommandWithRetry 等待期间限定应答来源，避免 dongle AT 与产测 FCTP 共用信号误触发 */
enum class CommandWaitSource {
    Any = 0,
    ProductProtocol,
    DongleAt,
};

typedef enum {
    STATE_INVALID,
    STATE_WAIT_START,
    STATE_CALIB_START,
    STATE_CALIB_STATIC_WAIT,
    STATE_CALIB_STATIC_START,
    STATE_CALIB_CH0,
    STATE_CALIB_CH1,
    STATE_CALIB_RESULT,
    STATE_MAX,
} STATE_INDEPENDENT_E;

class Qfctp;
class Qroot;
class Qaiot;

class test_base : public QWidget {
    Q_OBJECT

  public:
    explicit test_base(QWidget* parent = nullptr);

    // --- 工站虚接口 ---
    // clang-format off
    virtual void startTest() {}
    virtual void overTask() {}
    virtual void startTask() = 0;
    virtual void endTask() {}
    virtual void useMes() {}

    // --- test_base 控件桥接（子类 override） ---
    virtual QCheckBox* getIsUseMes() { return nullptr; }
    virtual QCheckBox* getIsFormMes() { return nullptr; }
    virtual QComboBox* getComNameCombo() { return nullptr; }
    virtual QComboBox* getUsbcomNameCombo() { return nullptr; }
    virtual QComboBox* getJigcomNameCombo() { return nullptr; }
    virtual QComboBox* getProductcomNameCombo() { return nullptr; }
    virtual QComboBox* getNfcComboBox() { return nullptr; }
    virtual QLineEdit* getMotorCaliParam() { return nullptr; }
    virtual QLineEdit* getMacLineEdit() { return nullptr; }
    virtual QLineEdit* macInputLineEdit() { return nullptr; }
    virtual QPlainTextEdit* logEdit() { return nullptr; }
    virtual QPlainTextEdit* msgEdit() { return nullptr; }
    virtual QTableWidget* testResultTable() { return nullptr; }
    virtual QPushButton* getEndTestButton() { return nullptr; }
    virtual QLabel* getMesStateQlabel() { return nullptr; }
    virtual void updateComboBox() {}
    // clang-format on

    // --- 串口波特率 ---
    int jigBaudRate = 115200;
    int productBaudRate = 115200;
    int usbBaudRate = 115200;
    int dongleBaudRate = 921600;
    int dongleOutTime = 10;

    // --- 通用工具 ---
    void waitWork(int ms);
    void updateMainStyle(QString style);
    int sendCommandWithRetry(std::function<void()> commandFunc, int timeoutMs = 300);
    void setCommandWaitSource(CommandWaitSource source) {
        commandWaitSource_ = source;
    }
    void testResultTableUpdate(QVector<TestItem>& testItems);
    QString exportTableContent();
    void testResultTableInit();
    void updateTestData(QVector<TestItem>& testItems);
    QString toHex(const QByteArray& data);
    QString parseMacFromSn(const QString& snCode);
    QString generateDateCode();
    bool applyAdaptiveV3ProductBySn(QLineEdit* snEdit);
    void appendStationResult(QVector<TestItem>& testItems, const QString& item, const QString& data, const QString& result);
    void LockProductUI();
    void getMac(QString sn_to_search);
    void signalAndslot();
    int getIndex() const;
    void showlog(QString msg);
    /** Product 通道：按 test_case ini 的 Protocol= 切换 QProtocolManager（不读 SYSTEM/ProtocolType）。 */
    void applyTestCaseProductProtocol(TestCaseProductProtocol protocol);

    // --- 本轮测试 / MES 包 ---
    QString macAddress = "没有mac地址";
    QString productName = "Y20";
    QString result = "";
    QString passValue = "通过";
    QString failValue = "失败";
    QString upperComputerVer;
    bool isBrushLogGet = 0;
    QVector<TestItem> testItems;
    MesPacketData pack;
    QString snPattern;
    QMap<QString, QMap<QString, QString>> deviceMap;

    // --- 日志 / 串口通道 ---
    Qlog* log;
    QTimer* scanSerialPortsTimer = new QTimer(this);
    SerialChannel* dongleSerialChannel_ = nullptr;
    SerialChannel* usbSerialChannel_ = nullptr;
    SerialChannel* jigSerialChannel_ = nullptr;
    SerialChannel* productSerialChannel_ = nullptr;
    QSerialPort* dongleSerialPort = nullptr;
    Qpb* pb = nullptr;
    Qfctp* qfctp = nullptr;
    Qaiot* qaiot = nullptr;
    Qroot* qroot = nullptr;
    QProtocolManager protocolManager;
    Qat* at = nullptr;
    QSerialPort* usbSerialPort = nullptr;
    Qusb* usb = nullptr;
    Qvisa* visa = nullptr;
    QSerialPort* jigSerialPort = nullptr;
    Qjig* jig = nullptr;
    QSerialPort* productSerialPort = nullptr;
    Qproduct* product = nullptr;

    // --- sendCommandWithRetry 运行态 ---
    bool getRespone = 0;
    bool canGoNext = false;
    bool sendRetryOver = false;
    QTimer* commandRetryTimer = nullptr;
    std::function<void()> commandRetryFunc_;
    int commandRetryCount = 0;
    int commandRetrySendCount = 0;
    int lastCommandRetryCount = 0;
    CommandWaitSource commandWaitSource_ = CommandWaitSource::Any;

    bool isTestContinue = false;
    bool bindingResult = false;
    int m_index = 0;

  public slots:
    QString getValueBySN(const QString& sn);
    bool compareVersions(const QString& versionList, const QString& versionToCompare);
    void set_independent_state(STATE_INDEPENDENT_E newState) {
        qDebug() << "机器" << getIndex() << "independent_state状态被设置" << newState;
        independent_state = newState;
    }
    STATE_INDEPENDENT_E get_independent_state(void) {
        return independent_state;
    }
    void solveGetBrushResponse(int data);
    void solveMesSucess(const int mechines);
    void solveMesData(const int mechines, QString msg);

    // --- 串口打开 / 关闭 / 扫描 ---
    virtual void onDongleSerialFrame(const QByteArray& data);
    void handleDongleSerialPortError(QSerialPort::SerialPortError error, const QString& message);
    void openDongleSerialPort(void);
    void closeDongleSerialPort(void);
    virtual void onUsbSerialFrame(const QByteArray& data);
    void handleUsbSerialPortError(QSerialPort::SerialPortError error, const QString& message);
    void openUsbSerialPort(void);
    void closeUsbSerialPort(void);
    void onJigSerialFrame(const QByteArray& data);
    void handleJigSerialPortError(QSerialPort::SerialPortError error, const QString& message);
    void openJigSerialPort(void);
    void closeJigSerialPort(void);
    void onProductSerialFrame(const QByteArray& data);
    void handleProductSerialPortError(QSerialPort::SerialPortError error, const QString& message);
    void openProductSerialPort(void);
    void closeProductSerialPort(void);
    void scanSerialPorts();
    void updateHIDComboBox(QComboBox* comboBox);

    // --- 协议上行虚槽（子类 override；由 onProtocolReport 按 reportType 分发） ---
    // clang-format off
    virtual void getTestValue(const int, const QString) {}
    virtual void canGoNextMechine(int) {}
    virtual void refreshCameraControl(ProtocolCameraControlData) {}
    virtual void refreshLedControlState(ProtocolLedControlData) {}
    virtual void refreshPressSensorData(ProtocolPressSampleData) {}
    virtual void refreshAmplitudeData(QString) {}
    virtual void refreshButton(ProtocolButtonStateData) {}
    virtual void refreshBrushControlState(ProtocolBrushControlData) {}
    virtual void refreshImuCaliResult(ProtocolImuCalibResultData) {}
    virtual void refreshImuData(ProtocolImuSampleData) {}
    virtual void refreshPbData(QString);
    virtual void refreshMotorCaliMsg(QString) {}
    virtual void refreshBleRssi(QString) {}
    virtual void refreshWifiRssi(QString) {}
    virtual void refreshPressCalibData(ProtocolPressCalibResultData) {}
    virtual void refreshDongleWifi(QString) {}
    virtual void refreshDongleVersion(QString);
    virtual void refreshWifiIp(QString) {}
    virtual void refreshBleState(int) {}
    virtual void refreshWifiMsg(QString) {}
    virtual void refreshWifiState(int) {}
    virtual void refreshBaseData(ProtocolBaseInfoData) {}
    virtual void refreshBattaryData(ProtocolBatteryData) {}
    virtual void refreshMusicState(ProtocolMusicStateData) {}
    virtual void refreshSn(ProtocolSnData) {}
    virtual void refreshLcdControl(ProtocolLcdControlData) {}
    virtual void refreshPeriphData(ProtocolPeriphStateData) {}
    virtual void refreshRssiRead(ProtocolRssiData) {}
    virtual void refreshChargeCurrentRead(ProtocolChargeCurrentData) {}
    virtual void refreshKeySignalRead(ProtocolKeyCapData) {}
    virtual void refreshTupleData(ProtocolTupleData) {}
    virtual void refreshPictureSendOver(ProtocolPictureSendOverData) {}
    virtual void refreshAgingStatus(ProtocolAgingStatusData) {}
    virtual void refreshRootBatteryTemp(quint8 temp) {}
    virtual void refreshResultCode(ProtocolResultData data) {}
    virtual void refreshTypeStatus(ProtocolTypeData data) {}
    virtual void refreshAmmeterData(QString) {}
    virtual void refreshDongleUartState(int) {}
    virtual void refreshUsbUartState(int) {}
    virtual void refreshJigUartState(int) {}
    virtual void refreshProductUartState(int) {}
    virtual void processReceivedData(const QByteArray&) {}
    // clang-format on

  protected:
    /** 产测协议（Qpb/Qfctp/Qaiot/Qroot）上行分发 */
    virtual void onProtocolReport(const ProtocolReport& report);
    /** Dongle AT 上行分发 */
    virtual void onDongleAtReport(const ProtocolReport& report);
    /** USB 电流表上行分发 */
    virtual void onUsbInstrumentReport(const ProtocolReport& report);
    /** 治具摆幅仪上行分发 */
    virtual void onJigInstrumentReport(const ProtocolReport& report);
    /** 结束一次 sendCommandWithRetry 等待；success=false 时 sendRetryOver=1 供工站判失败 */
    void finishCommandRetryWait(bool success, const QString& logMessage);
    /** 测试结束：本地入库 + 云端上报；useMes 为真时再触发 MES 过站 */
    void finishTestRecord(const MesPacketData& pack, bool useMes);
    void closeEvent(QCloseEvent* event) override;
    void resetVisaBackend();

  private:
    QString receivedData = "";
    STATE_INDEPENDENT_E independent_state = STATE_INVALID;
    void initData();
    void saveDongleUartLog(QString data);
    void getMacAddress(const QByteArray& byte);
    bool isCommandRetryResponseAccepted(const QObject* source) const;
    void onCommandRetryTimerTimeout();

  private slots:
    void refreshMesState(int state);

  signals:
    void send_go_next_focus();
    void send_start_test(int data);
    void send_go_next_test(int data);
    void send_dongle_serialPort_state(int);
    void send_usb_serialPort_state(int);
    void send_jig_serialPort_state(int);
    void send_product_serialPort_state(int);
    void send_process_inspection(MesPacketData);
    void send_end_test_pass(MesPacketData);
    void send_mes_test_value(MesPacketData);
    void send_kill_test(int data);
    void send_end_test(int data);
};

#endif // TEST_BASE_H
