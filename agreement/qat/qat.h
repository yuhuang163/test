#ifndef QAT_H
#define QAT_H

#include <QMessageBox>
#include <QObject>
#include <QQueue>
#include <QSerialPort>

class Qat : public QSerialPort
{
    Q_OBJECT
public :
    explicit Qat(QSerialPort *parent = nullptr);
    void parseCmd(const QByteArray &byte);
    void sendbleMac(QString mac);
    void sendCmd(QString cmd);
    void sendotaMac(QString mac);
    void sendMac(QString mac);
    void sendBLELOG(int state);
    bool getConnected()
    {
        return isConnected;
    }
    void resetConnected()
    {
        isConnected = false;
    }
    void setConnected()
    {
        isConnected = true;
    }
    void ask_mac();
    bool getwifiConnected()
    {
        return iswifiConnected;
    }
    void resetwifiConnected()
    {
        iswifiConnected = false;
    }
    void setwifiConnected()
    {
        iswifiConnected = true;
    }

signals:
    void command(QString cmd, QString parameter);
    void send_ble_state(int state);
    void send_rssi(QString state);
       void send_dongle_ver(QString state);
    void send_dongle_wifi(QString state);


    void send_wifi_rssi(QString state);
    void send_WIFI_state(int state);
    void sendwifimsg(QString data);
private:
    void waitWork(int ms);
    typedef enum
    {
        STATE_IDLE,
        STATE_RECEIVING_T,
        STATE_RECEIVING_COMMAND,
        STATE_RECEIVING_PARAMETER
    } State;
    State state = STATE_IDLE;
    QString cmd, parameter;
    QSerialPort *serialPort;
    QQueue<char> dataQueue;
    typedef std::function<void(QString)> callback;
    std::map<QString, callback> commandList;
    void SEND_WIFI_DATA(QString p);
    void registerCommand();
    void help(QString p);
    void rssi(QString p);

    void dongle_ver(QString p);
    void dongle_wifi(QString p);

    void wifi_rssi(QString p);
    void connected(QString p);
    void disconnected(QString p);
    void WIFI_connected(QString p);
    void WIFI_disconnected(QString p);
    bool isConnected = 0;
    bool iswifiConnected = 0;

private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif   // QAT_H
