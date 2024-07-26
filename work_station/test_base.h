#ifndef TEST_BASE_H
#define TEST_BASE_H

// #include <qat.h>      // 与esp32的at指令
// #include <qpb.h>      // 与牙刷的pb协议
// #include <qusb.h>     // 与治具的测量协议
// #include <qlog.h>
// #include <qjig.h>
// #include <qproduct.h>

#include "Abini.h"
#include <qlog.h>
extern "C"   // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}
class test_base : public QWidget
{
    Q_OBJECT
public:
    test_base();
    virtual void start_test() {};
    virtual void over_task() {};
    virtual void start_task() = 0;
    virtual void endTask() {};
    virtual void useMes() {};

    virtual QComboBox *getComNameCombo()
    {
        return nullptr;
    };   // dongle口
    virtual QComboBox *getUsbcomNameCombo()
    {
        return nullptr;
    };   // usb口（治具）
    virtual QComboBox *getJigcomNameCombo()
    {
        return nullptr;
    };   // 治具口（治具）
    virtual QComboBox *getProductcomNameCombo()
    {
        return nullptr;
    };   // 牙刷口（治具）
   virtual QComboBox *getNfcComboBox()
    {
        return nullptr;
    };   // nfc的usb口

    virtual QLineEdit *getMotorCaliParam()
    {
        return nullptr;
    };   // 电机校准参数

    virtual QLineEdit *getMacLineEdit()
    {
        return nullptr;
    };   // sn输入口
    virtual QLineEdit *macInputLineEdit()
    {
        return nullptr;
    };   // mac地址输入口
    virtual QPlainTextEdit *logEdit()
    {
        return nullptr;
    };   // log输入口
    virtual QPlainTextEdit *msgEdit()
    {
        return nullptr;
    };   // msg输入口
    int jigBaudRate = 115200;
    int productBaudRate = 1000000;
    int usbBaudRate = 115200;
    int dongleBaudRate = 921600;

    int dongleOutTime = 10;


    // 通用函数
    void waitWork(int ms);
    void update_main_style(QString style);
    int sendCommandWithRetry(std::function<void()> commandFunc);
private:
    void initData();

    // 通用变量
public:
    QString macAddress = "没有mac地址";
    QString productName = "Y20";
    QVector<TestItem> testItems;
    MesPacketData pack;
    QString snPattern;
    int testState=0;

public:
    void signalAndslot();
    Qlog *log;
    QTimer *scanSerialPortsTimer = new QTimer(this);

    QSerialPort *dongleSerialPort;   // dongle硬件层
    Qpb *pb;                         // dongle协议层
    Qat *at;                         // dongle协议层

    QSerialPort *usbSerialPort;   // 通用硬件层
    Qusb *usb;                    // 通用协议层

    QSerialPort *jigSerialPort;   // 治具硬件层
    Qjig *jig;                    // 治具协议层

    QSerialPort *productSerialPort;   // 牙刷硬件层
    Qproduct *product;                // 牙刷协议层

    QTimer *dongleSerialPortTimer = new QTimer(this);
    QByteArray dongleSerialPortBuf = 0;

    QTimer *usbSerialPortTimer = new QTimer(this);
    QByteArray usbSerialPortBuf = 0;

    QTimer *jigSerialPortTimer = new QTimer(this);
    QByteArray jigSerialPortBuf = 0;

    QTimer *productSerialPortTimer = new QTimer(this);
    QByteArray productSerialPortBuf = 0;

    bool getRespone = false;
    bool canGoNext = false;
    bool sendRetryOver = false;



public slots:
    void solveGetBrushResponse(int);


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
    void updateHIDComboBox(QComboBox *comboBox);


public slots:
    virtual void getTestValue(const int , const QString ) {};
    virtual void can_go_next(int ) {};
    virtual void refresh_camera_CONTROL(FacCameraControl) {};
    virtual void check_LED_CONTROL_state(FacLedControl){};
    virtual void checkbutton(FacButtonState){};
    virtual void check_BrushControl_state(FacBrushControl){};
    virtual void update_IMU_CALIB_result(FacImuCalibResult) {};
    virtual void getimuData(FacUploadNineAlex) {};
    virtual void refresh_pb_data(QString);
    virtual void refresh_motor_cali_msg(QString) {};
    virtual void refresh_ble_rssi(QString) {};
    virtual void get_dongle_ver(QString) {};
    virtual void get_dongle_wifi(QString) {};
    virtual void refresh_ble_state(int) {};
    virtual void get_wifi_msg(QString) {};
    virtual void refresh_base_data(FacGetDevBaseInfo) {};
    virtual void refresh_battary_data(FacDevInfo) {};
    virtual void refresh_sn(FacDevInfo) {};
    virtual void refresh_Lcd_CONTROL(FacLcdControl) {};
    virtual void refresh_periph_data(FacGetPeriphState) {};
    virtual void refresh_ammeter_data(QString) {};
    virtual void refresh_dongle_uart_state(int) {};
    virtual void refresh_usb_uart_state(int) {};
    virtual void refresh_jig_uart_state(int) {};
    virtual void refresh_product_uart_state(int ) {};
    virtual void processReceivedData(const QByteArray &) {};


signals:
    void refreshDongleSerialPortState(int);
    void refreshUsbSerialPortState(int);
    void refreshJigSerialPortState(int);
    void refreshProductSerialPortState(int);

    void sendProcessInspection(MesPacketData);
    void sendTestPass(MesPacketData);
    void getMesTestValue(MesPacketData);
};

#endif   // TEST_BASE_H
