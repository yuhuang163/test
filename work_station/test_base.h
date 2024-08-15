#ifndef TEST_BASE_H
#define TEST_BASE_H

// #include <qat.h>      // 与esp32的at指令
// #include <qpb.h>      // 与牙刷的pb协议
// #include <qusb.h>     // 与治具的测量协议
// #include <qlog.h>
// #include <qjig.h>
// #include <qproduct.h>

#include <qlog.h>

#include "Abini.h"
#include "qheaderview.h"
#include "qtablewidget.h"
extern "C"  // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}
class test_base : public QWidget {
    Q_OBJECT
public:
    test_base();
    virtual void startTest(){};
    virtual void overTask(){};
    virtual void startTask() = 0;
    virtual void endTask(){};
    virtual void useMes(){};

    virtual QComboBox* getComNameCombo() { return nullptr; };         // dongle口
    virtual QComboBox* getUsbcomNameCombo() { return nullptr; };      // usb口
    virtual QComboBox* getJigcomNameCombo() { return nullptr; };      // 治具口
    virtual QComboBox* getProductcomNameCombo() { return nullptr; };  // 牙刷口
    virtual QComboBox* getNfcComboBox() { return nullptr; };          // nfc的usb口
    virtual QLineEdit* getMotorCaliParam() { return nullptr; };       // 电机校准参数
    virtual QLineEdit* getMacLineEdit() { return nullptr; };          // sn输入口
    virtual QLineEdit* macInputLineEdit() { return nullptr; };        // mac地址输入口
    virtual QPlainTextEdit* logEdit() { return nullptr; };            // log输入口
    virtual QPlainTextEdit* msgEdit() { return nullptr; };            // msg输入口
    virtual QTableWidget* testResultTable() { return nullptr; };      // 测试结果表格输入口

    int jigBaudRate = 115200;
    int productBaudRate = 1000000;
    int usbBaudRate = 115200;
    int dongleBaudRate = 921600;
    int dongleOutTime = 10;

    // 通用函数
    void waitWork(int ms);
    void updateMainStyle(QString style);
    int sendCommandWithRetry(std::function<void()> commandFunc);
    void testResultTableUpdate(const QVector<TestItem>& testItems);
    void testResultTableInit();
    void updateTestData(QVector<TestItem>& testItems);
    QString toHex(const QByteArray& data);

private:  // 通用变量
    void initData();

public:
    QString macAddress = "没有mac地址";
    QString productName = "Y20";
    QString result = "";
    QString passValue = "通过";
    QString failValue = "失败";
    QVector<TestItem> testItems;
    MesPacketData pack;
    QString snPattern;
    int testState = 0;

public:
    void signalAndslot();
    Qlog* log;
    QTimer* scanSerialPortsTimer = new QTimer(this);

    QSerialPort* dongleSerialPort;  // dongle硬件层
    Qpb* pb;                        // dongle协议层
    Qat* at;                        // dongle协议层

    QSerialPort* usbSerialPort;  // 通用硬件层
    Qusb* usb;                   // 通用协议层

    QSerialPort* jigSerialPort;  // 治具硬件层
    Qjig* jig;                   // 治具协议层

    QSerialPort* productSerialPort;  // 牙刷硬件层
    Qproduct* product;               // 牙刷协议层

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

    int m_index = 0;

public slots:
    void solveGetBrushResponse(int);
    int getIndex();
    void showlog(QString msg);

    virtual void readDongleSerialPortData(void);
    void handleDongleSerialPortError(QSerialPort::SerialPortError error);
    void openDongleSerialPort(void);
    void closeDongleSerialPort(void);

    void readUsbSerialPortData(void);
    void handleUsbSerialPortError(QSerialPort::SerialPortError error);
    void openUsbSerialPort(void);
    void closeUsbSerialPort(void);

    void readJigSerialPortData(void);
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
    virtual void refreshCameraControl(FacCameraControl){};
    virtual void checkLedControlState(FacLedControl){};
    virtual void checkbutton(FacButtonState){};
    virtual void checkBrushControlState(FacBrushControl){};
    virtual void refreshImuCaliResult(FacImuCalibResult){};
    virtual void getimuData(FacUploadNineAlex){};
    virtual void refreshPbData(QString);
    virtual void refreshMotorCaliMsg(QString){};
    virtual void refreshBleRssi(QString){};
    virtual void getDongleVer(QString){};
    virtual void getDongleWifi(QString){};
    virtual void refreshBleState(int){};
    virtual void getWifiMsg(QString){};
    virtual void refreshBaseData(FacGetDevBaseInfo){};
    virtual void refreshBattaryData(FacDevInfo){};
    virtual void refreshSn(FacDevInfo){};
    virtual void refreshLcdControl(FacLcdControl){};
    virtual void refreshPeriphData(FacGetPeriphState){};
    virtual void refreshAmmeterData(QString){};
    virtual void refreshDongleUartState(int){};
    virtual void refreshUsbUartState(int){};
    virtual void refreshJigUartState(int){};
    virtual void refreshProductUartState(int){};
    virtual void processReceivedData(const QByteArray&){};

signals:
    void send_dongle_serialPort_state(int);
    void refreshUsbSerialPortState(int);
    void refreshJigSerialPortState(int);
    void refreshProductSerialPortState(int);
    void sendProcessInspection(MesPacketData);
    void send_end_testPass(MesPacketData);
    void getMesTestValue(MesPacketData);
};

#endif  // TEST_BASE_H
