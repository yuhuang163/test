#ifndef QUSB_H
#define QUSB_H
#include <QMessageBox>
#include <QObject>
#include <QSerialPort>
#include <functional>
#include <map>

#include "qvisa.h"

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
        ReadsuctionData,
        ReadConfiguration,
        /// 程控电源 SCPI 读电压（与 ReadMeasurement 量测分流；工站可接 programmablePowerVoltageRead）
        ReadProgrammablePowerVoltage,
        /// 程控电源 SCPI 读电流
        ReadProgrammablePowerCurrent,
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
    /// 程控电源 SCPI 读电压完成：VISA 为同步回包；串口为解析到一行后的异步回包。valueVolts 单位 V，ok 为数值解析是否成功
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    /// 程控电源 SCPI 读电流完成，valueAmps 单位 A
    void programmablePowerCurrentRead(double valueAmps, bool ok);

private:
    enum class ProgrammablePowerReadPending
    {
        None,
        Voltage,
        Current
    };
    void emitProgrammablePowerReadIfPending(const QString& scpiLine);
    ProtocolType resolveProtocolForInput(const QByteArray &input) const;
    ProtocolType resolveProtocolForOutput() const;
    bool handleScpiAction(PowerAction action);
    bool handleHqAction(PowerAction action);
    bool handleLxAction(PowerAction action);
    bool programmablePowerVisaConfigured() const;
    /// 程控电源 SCPI 是否走 VISA（与 protocol 是否为 Scpi 无关，由 scpiUseVisa 决定）
    bool isVisaScpiEnabled() const;
    Qvisa::ProtocolConfig visaConfigFromProtocolConfig() const;
    bool ensureVisaConnected();
    void closeVisaConnection();
    bool visaWrite(const QString &cmd);
    bool visaQuery(const QString &cmd, QString *response);
    void processScpiData(const QByteArray &byte);
    void processModbusRTUData(const QByteArray &data);
    QString cmd, parameter;
    QSerialPort *serialPort;
    int sssss=255;
    typedef std::function<void(QString)> callback;
    std::map<QString, callback> commandList;
    void registerCommand();
    void CONFigureFUNCtion(QString p);
    QByteArray data = 0;
    ProtocolConfig protocolConfig_;
    ProgrammablePowerReadPending pendingProgPowerRead_ = ProgrammablePowerReadPending::None;
    Qvisa visa_;

private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif   // QUSB_H
