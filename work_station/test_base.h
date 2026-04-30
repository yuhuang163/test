#ifndef TEST_BASE_H
#define TEST_BASE_H

// #include <qat.h>      // 与esp32的at指令
// #include <qpb.h>      // 与设备的pb协议
// #include <qusb.h>     // 与治具的测量协议
// #include <qlog.h>
// #include <qjig.h>
// #include <qproduct.h>

#include <qlog.h>

#include "Abini.h"
#include "agreement/qProtocol/qprotocolmanager.h"
#include "qcheckbox.h"
#include "qheaderview.h"
#include "qlabel.h"
#include "qpushbutton.h"
#include "qtablewidget.h"
extern "C"  // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}
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
class test_base : public QWidget {
    Q_OBJECT
public:
    explicit test_base(QWidget* parent = nullptr);

    virtual void startTest(){};
    virtual void overTask(){};
    virtual void startTask() = 0;
    virtual void endTask(){};
    virtual void useMes(){};

    virtual QCheckBox* getIsUseMes() { return nullptr; };  // 获取mes获取接口
    virtual QCheckBox* getIsFormMes() { return nullptr; };
    virtual QComboBox* getComNameCombo() { return nullptr; };         // dongle口
    virtual QComboBox* getUsbcomNameCombo() { return nullptr; };      // usb口
    virtual QComboBox* getJigcomNameCombo() { return nullptr; };      // 治具口
    virtual QComboBox* getProductcomNameCombo() { return nullptr; };  // 设备口
    virtual QComboBox* getNfcComboBox() { return nullptr; };          // nfc的usb口
    virtual QLineEdit* getMotorCaliParam() { return nullptr; };       // 电机校准参数
    virtual QLineEdit* getMacLineEdit() { return nullptr; };          // sn输入口
    virtual QLineEdit* macInputLineEdit() { return nullptr; };        // mac地址输入口
    virtual QPlainTextEdit* logEdit() { return nullptr; };            // log输入口
    virtual QPlainTextEdit* msgEdit() { return nullptr; };            // msg输入口
    virtual QTableWidget* testResultTable() { return nullptr; };      // 测试结果表格输入口
    virtual QPushButton* getEndTestButton() { return nullptr; };      // 结束测试按钮
    virtual QLabel* getMesStateQlabel() { return nullptr; };          // mes状态的qlab
    virtual void updateComboBox(){};
    int jigBaudRate = 115200;
    int productBaudRate = 1000000;

    int usbBaudRate = 115200;
    int dongleBaudRate = 921600;
    int dongleOutTime = 10;

    // 通用函数
    void waitWork(int ms);
    void updateMainStyle(QString style);
    int sendCommandWithRetry(std::function<void()> commandFunc);
    void testResultTableUpdate(QVector<TestItem>& testItems);
    QString exportTableContent();
    void testResultTableInit();
    void updateTestData(QVector<TestItem>& testItems);
    QString toHex(const QByteArray& data);
    QString parseMacFromSn(const QString& snCode);
    void appendStationResult(QVector<TestItem>& testItems, const QString& item, const QString& data, const QString& result);
    void LockProductUI();
    QMap<QString, QMap<QString, QString>> deviceMap;  // 存储设备信息
    void getMac(QString sn_to_search);

private:  // 通用变量
    QString receivedData = "";
    // 各个上位机的状态

    STATE_INDEPENDENT_E independent_state = STATE_INVALID;

    void initData();
    void saveDongleUartLog(QString data);
    void getmacadress(const QByteArray& byte);

public:
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

public:
    void signalAndslot();
    Qlog* log;
    QTimer* scanSerialPortsTimer = new QTimer(this);

    QSerialPort* dongleSerialPort;  // dongle硬件层
    Qpb* pb;                        // dongle协议层
    Qfctp* qfctp;                   // fctp协议层
    QProtocolManager protocolManager;
    Qat* at;                        // dongle协议层

    QSerialPort* usbSerialPort;  // 通用硬件层
    Qusb* usb;                   // 通用协议层

    QSerialPort* jigSerialPort;  // 治具硬件层
    Qjig* jig;                   // 治具协议层

    QSerialPort* productSerialPort;  // 设备硬件层
    Qproduct* product;               // 设备协议层

    QTimer* dongleSerialPortTimer = new QTimer(this);
    QByteArray dongleSerialPortBuf = 0;

    QTimer* usbSerialPortTimer = new QTimer(this);
    QByteArray usbSerialPortBuf = 0;

    QTimer* jigSerialPortTimer = new QTimer(this);
    QByteArray jigSerialPortBuf = 0;

    QTimer* productSerialPortTimer = new QTimer(this);
    QByteArray productSerialPortBuf = 0;

    bool getRespone = 0;
    bool canGoNext = false;
    bool sendRetryOver = false;

    bool isTestContinue = false;  //测试是否继续
    bool bandingresult = false;   // mes绑定结果
    int m_index = 0;

public slots:
    QString getValueBySN(const QString& sn);
    bool compareVersions(const QString& versionList, const QString& versionToCompare);
    void set_independent_state(STATE_INDEPENDENT_E newState) {
        qDebug() << "机器" << getIndex() << "independent_state状态被设置" << newState;
        independent_state = newState;
    };
    STATE_INDEPENDENT_E get_independent_state(void) { return independent_state; };  //获取当前上位机状态
    void solveGetBrushResponse(int);
    int getIndex();
    void showlog(QString msg);
    void solveMesSucess(const int mechines);
    void solveMesData(const int mechines, QString msg);
    virtual void readDongleSerialPortData(void);
    void handleDongleSerialPortError(QSerialPort::SerialPortError error);
    void openDongleSerialPort(void);
    void closeDongleSerialPort(void);

    void readUsbSerialPortData(void);
    void handleUsbSerialPortError(QSerialPort::SerialPortError error);
    void openUsbSerialPort(void);
    void closeUsbSerialPort(void);

    virtual void readJigSerialPortData(void);
    void handleJigSerialPortError(QSerialPort::SerialPortError error);
    void openJigSerialPort(void);
    void closeJigSerialPort(void);

    void readProductSerialPortData(void);
    void handleProductSerialPortError(QSerialPort::SerialPortError error);
    void openProductSerialPort(void);
    void closeProductSerialPort(void);
    void scanSerialPorts();
    void updateHIDComboBox(QComboBox* comboBox);

public slots:
    virtual void getTestValue(const int, const QString){};
    virtual void canGoNextMechine(int){};
    virtual void refreshCameraControl(ProtocolCameraControlData){};
    virtual void checkLedControlState(ProtocolLedControlData){};
    virtual void getPressSensorData(ProtocolPressSampleData){};

    virtual void refreshAmplitudeData(QString ){};
    virtual void checkbutton(ProtocolButtonStateData){};
    virtual void checkBrushControlState(ProtocolBrushControlData){};
    virtual void refreshImuCaliResult(ProtocolImuCalibResultData){};
    virtual void getimuData(ProtocolImuSampleData){};
    virtual void refreshPbData(QString);
    virtual void refreshMotorCaliMsg(QString){};
    virtual void refreshBleRssi(QString){};
    virtual void getPresscalidata(ProtocolPressCalibResultData){};
    virtual void getDongleWifi(QString){};
    virtual void refreshBleState(int){};
    virtual void getWifiMsg(QString){};
    virtual void refreshBaseData(ProtocolBaseInfoData){};
    virtual void refreshBattaryData(ProtocolBatteryData){};
    virtual void refreshMusicState(ProtocolMusicStateData){};

    virtual void refreshSn(ProtocolSnData){};
    virtual void refreshLcdControl(ProtocolLcdControlData){};
    virtual void refreshPeriphData(ProtocolPeriphStateData){};
    virtual void refreshAmmeterData(QString){};
    virtual void refreshDongleUartState(int){};
    virtual void refreshUsbUartState(int){};
    virtual void refreshJigUartState(int){};
    virtual void refreshProductUartState(int){};
    virtual void processReceivedData(const QByteArray&){};

protected:
    void closeEvent(QCloseEvent* event) override;
private slots:
    void getDongleVer(QString);
    void refreshMesState(int state);

signals:
    void send_go_next_focus();
    void send_startTest(int data);
    void send_go_next_test(int data);
    void send_dongle_serialPort_state(int);
    void refreshUsbSerialPortState(int);
    void refreshJigSerialPortState(int);
    void refreshProductSerialPortState(int);
    void sendProcessInspection(MesPacketData);
    void send_end_testPass(MesPacketData);
    void getMesTestValue(MesPacketData);
    void send_kill_test(int data);
    void send_end_test(int data);
};

#endif  // TEST_BASE_H
