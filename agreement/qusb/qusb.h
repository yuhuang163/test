#ifndef QUSB_H
#define QUSB_H
#include <QMessageBox>
#include <QObject>
#include <QQueue>
#include <QSerialPort>

class Qusb : public QSerialPort
{
    Q_OBJECT
public:
    explicit Qusb(QSerialPort *parent = nullptr);

    void sendCmd(QString cmd);
    void sendCONF(QString current, QString dc, QString range);
    void getMEASure(QString mac);
    void gethqMEASure();
    void getlxMEASure(int mechine);
    void processlxModbusRTUData(const QByteArray &data);
    void sethqMEASure();
    void getCONF(QString mac);
    void parseCmd(const QByteArray &byte);

signals:
    void send_ammeter_data(QString data);   // 信号声明

private:
    void processModbusRTUData(const QByteArray &data);
    QString cmd, parameter;
    QSerialPort *serialPort;
    int sssss=255;
    QQueue<char> dataQueue;
    typedef std::function<void(QString)> callback;
    std::map<QString, callback> commandList;
    void registerCommand();
    void CONFigureFUNCtion(QString p);
    QByteArray data = 0;

private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif   // QUSB_H
