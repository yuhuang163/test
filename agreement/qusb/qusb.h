#ifndef QUSB_H
#define QUSB_H
#include <QMessageBox>
#include <QObject>
#include <QQueue>
#include <QSerialPort>
#include <functional>
#include <map>

#ifdef HAVE_NI_VISA
#include <visa.h>
#endif

class Qusb : public QSerialPort
{
    Q_OBJECT
public:
    enum class ProtocolType
    {
        Auto,
        Scpi,
        HqModbus,
        LxModbus,
        Byd
    };

    enum class PowerAction
    {
        ConfigurePowerSupply,
        ReadMeasurement,
        ReadsuctionData,
        ReadConfiguration,
        InitializeDevice
    };

    struct ProtocolConfig
    {
        ProtocolType protocol = ProtocolType::Auto;
        int luxshareMachineId = 1;
        bool scpiUseVisa = false;
        QString scpiVisaAddress = "GPIB0::7::INSTR";
        QString scpiCurrentType = "CURRent";
        QString scpiCurrentMode = "DC";
        QString scpiRange = "500e-3";
        double scpiPowerVoltageV = 16.8;
        double scpiPowerCurrentA = 1.0;
        QString scpiSetVoltageCmd = "VOLT %1";
        QString scpiSetCurrentCmd = "CURR %1";
        QString scpiOutputOnCmd = "OUTP ON";
        QString scpiOutputOffCmd = "OUTP OFF";
        QString scpiReadVoltageCmd = "MEASure:VOLTage:DC?";
        QString scpiReadCurrentCmd = "MEASure:CURRent:DC? 500e-3";
    };

    explicit Qusb(QSerialPort *parent = nullptr);
    ~Qusb();

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
    bool configureProgrammablePower(double voltageV, double currentA);
    bool setProgrammablePowerOutput(bool enable);
    bool readProgrammablePowerVoltage();
    bool readProgrammablePowerCurrent();
    bool initializeProgrammablePower();

signals:
    void send_ammeter_data(QString data);   // 信号声明

private:
    ProtocolType resolveProtocolForInput(const QByteArray &input) const;
    ProtocolType resolveProtocolForOutput() const;
    bool handleScpiAction(PowerAction action);
    bool handleHqAction(PowerAction action);
    bool handleLxAction(PowerAction action);
    bool handlebydAction(PowerAction action);
    bool isVisaScpiEnabled() const;
    bool ensureVisaConnected();
    void closeVisaConnection();
    bool visaWrite(const QString &cmd);
    bool visaQuery(const QString &cmd, QString *response);
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
#ifdef HAVE_NI_VISA
    ViSession visaRm_ = VI_NULL;
    ViSession visaInst_ = VI_NULL;
#endif

private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif   // QUSB_H
