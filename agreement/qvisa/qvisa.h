#ifndef QVISA_H
#define QVISA_H

#include <QObject>
#include <QString>

#ifdef HAVE_NI_VISA
#include <visa.h>
#endif

class Qvisa : public QObject {
    Q_OBJECT
public:
    enum class PowerAction {
        ConfigurePowerSupply,
        ReadVoltage,
        ReadCurrent,
        OutputOn,
        OutputOff,
        InitializeDevice
    };

    struct ProtocolConfig {
        bool useVisa = false;
        QString visaAddress = QStringLiteral("GPIB0::7::INSTR");
        int timeoutMs = 3000;
        double powerVoltageV = 16.8;
        double powerCurrentA = 1.0;
        QString setVoltageCmd = QStringLiteral("VOLT %1");
        QString setCurrentCmd = QStringLiteral("CURR %1");
        QString outputOnCmd = QStringLiteral("OUTP ON");
        QString outputOffCmd = QStringLiteral("OUTP OFF");
        QString readVoltageCmd = QStringLiteral("MEASure:VOLTage:DC?");
        QString readCurrentCmd = QStringLiteral("MEASure:CURRent:DC? 500e-3");
    };

    explicit Qvisa(QObject* parent = nullptr);
    ~Qvisa();

    void setProtocolConfig(const ProtocolConfig& config);
    ProtocolConfig protocolConfig() const;
    bool configured() const;

    bool ensureConnected();
    void closeConnection();
    bool writeCommand(const QString& cmd);
    bool queryCommand(const QString& cmd, QString* response);

    bool sendPowerInstruction(PowerAction action);
    bool configureProgrammablePower(double voltageV, double currentA);
    bool setProgrammablePowerOutput(bool enable);
    bool readProgrammablePowerVoltage();
    bool readProgrammablePowerCurrent();
    bool initializeProgrammablePower();

signals:
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    void programmablePowerCurrentRead(double valueAmps, bool ok);

private:
    ProtocolConfig config_;
#ifdef HAVE_NI_VISA
    ViSession visaRm_ = VI_NULL;
    ViSession visaInst_ = VI_NULL;
#endif
};

#endif  // QVISA_H
