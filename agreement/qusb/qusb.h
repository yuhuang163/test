#ifndef QUSB_H
#define QUSB_H
#include <QMessageBox>
#include <QObject>
#include <QQueue>
#include <QSerialPort>
#include <functional>
#include <map>

class Qusb : public QSerialPort
{
    Q_OBJECT
public:
    enum class ProtocolType
    {
        Auto,
        Scpi,
        HqModbus,
        LxModbus
    };

    enum class PowerAction
    {
        ConfigurePowerSupply,
        ReadMeasurement,
        ReadConfiguration,
        InitializeDevice
    };

    struct ProtocolConfig
    {
        ProtocolType protocol = ProtocolType::Auto;
        int luxshareMachineId = 1;
        QString scpiCurrentType = "CURRent";
        QString scpiCurrentMode = "DC";
        QString scpiRange = "500e-3";
    };

    explicit Qusb(QSerialPort *parent = nullptr);

    void setProtocolConfig(const ProtocolConfig &config);
    ProtocolConfig protocolConfig() const;
    bool sendPowerInstruction(PowerAction action);

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
    ProtocolType resolveProtocolForInput(const QByteArray &input) const;
    ProtocolType resolveProtocolForOutput() const;
    bool handleScpiAction(PowerAction action);
    bool handleHqAction(PowerAction action);
    bool handleLxAction(PowerAction action);
    void processScpiData(const QByteArray &byte);
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
    ProtocolConfig protocolConfig_;

private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif   // QUSB_H
