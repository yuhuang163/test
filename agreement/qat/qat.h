#ifndef QAT_H
#define QAT_H

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QSerialPort>
#include <QVariant>
#include <QVariantMap>

/** Dongle AT 指令（经 at->set/get 下发） */
enum class DongleCmd {
    BleScanConnect,      // AT+MAC= 扫描/连接，data: MAC；全 0 表示断开
    BleDirectConnect,    // AT+DCON=
    BleOtaConnect,       // AT+OTA=
    BleAppConnect,       // AT+BLE=
    BleMainConnect,      // AT+MAIN=
    OtaDataPassthrough,  // AT+OTADATA= 0/1
    MainDataPassthrough, // AT+MAINDATA= 0/1
    BleLog,              // AT+BLELOG= 0/1
    BleDeviceLog,        // AT+BLEDEVICELOG= 0/1
    Bomb,                // AT+BOMB= QVariantMap{deviceName,rssi,connectionInterval,command}
    GetGmac,             // get: AT+GMAC
};

class Qat : public QObject {
    Q_OBJECT
public:
    explicit Qat(QSerialPort* parent = nullptr);

    void parseCmd(const QByteArray& byte);
    void set(DongleCmd cmd, const QVariant& data = {});
    void get(DongleCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);

    bool getConnected() { return isConnected; }
    void resetConnected() { isConnected = false; }
    void setConnected() { isConnected = true; }
    bool getwifiConnected() { return iswifiConnected; }
    void resetwifiConnected() { iswifiConnected = false; }
    void setwifiConnected() { iswifiConnected = true; }

signals:
    void command(QString cmd, QString parameter);
    void sendGetProductResponse(int data);
    void send_ble_state(int state);
    void send_rssi(QString state);
    void send_dongle_ver(QString state);
    void send_dongle_wifi(QString state);
    void send_wifi_rssi(QString state);
    void send_WIFI_state(int state);
    void sendWifiMsg(QString data);
    void sendWifiIp(QString data);

private:
    void waitWork(int ms);
    void sendAtLine(const QString& line);
    void registerCommand();

    typedef enum { STATE_IDLE, STATE_RECEIVING_T, STATE_RECEIVING_COMMAND, STATE_RECEIVING_PARAMETER } State;
    State state = STATE_IDLE;
    QString cmd, parameter;
    QSerialPort* serialPort = nullptr;
    QQueue<char> dataQueue;
    typedef std::function<void(QString)> callback;
    std::map<QString, callback> commandList;

    void SEND_WIFI_DATA(QString p);
    void SEND_WIFI_IP(QString p);
    void help(QString p);
    void rssi(QString p);
    void dongle_ver(QString p);
    void dongle_wifi(QString p);
    void wifi_rssi(QString p);
    void connected(QString p);
    void disconnected(QString p);
    void WIFI_connected(QString p);
    void WIFI_disconnected(QString p);
    bool isConnected = false;
    bool iswifiConnected = false;

private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif  // QAT_H
