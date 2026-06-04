#ifndef QVISA_H
#define QVISA_H

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#ifdef HAVE_NI_VISA
#include <visa.h>
#endif

/** VISA 设备类型：工站 set(DeviceProfile) 后从 SETTINGS 加载，不发 SCPI */
enum class VisaDeviceProfile {
    DefaultPower = 0,  // 未加载，须先 set(DeviceProfile)
    ProgrammablePower,
    RxCmwInstrument,
};

/** VISA 指令（工站经 visa->set/get 下发） */
enum class VisaCmd {
    DeviceProfile,          // set: int(VisaDeviceProfile)，从 SETTINGS 加载连接项与命令模板
    ConfigurePowerSupply,   // set: QVariantMap{voltage?,current?}
    ReadVoltage,            // get
    ReadCurrent,            // get
    PowerOutputOn,          // set
    PowerOutputOff,         // set
    InitializePower,        // set: QVariantMap{voltage?,current?}
    WriteLine,              // set: QString SCPI
    QueryLine,              // get: QString SCPI，应答见 lastQueryResponse()
};

class Qvisa : public QObject {
    Q_OBJECT
public:
    struct ProtocolConfig {
        bool useVisa;
        QString visaAddress;
        VisaDeviceProfile commandProfile;
        int timeoutMs;
        double powerVoltageV;
        double powerCurrentA;
    };

    explicit Qvisa(QObject* parent = nullptr);
    ~Qvisa();

    ProtocolConfig protocolConfig() const;

    bool ensureConnected();
    void closeConnection();

    bool set(VisaCmd cmd, const QVariant& data = {});
    bool get(VisaCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);

    QString lastQueryResponse() const { return lastQueryResponse_; }

signals:
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    void programmablePowerCurrentRead(double valueAmps, bool ok);

private:
    struct CommandSet {
        QString setVoltageCmd;
        QString setCurrentCmd;
        QString outputOnCmd;
        QString outputOffCmd;
        QString readVoltageCmd;
        QString readCurrentCmd;
    };

    void loadCommandSet(VisaDeviceProfile profile);
    bool writeLine(const QString& cmd);
    bool configurePower(double voltageV, double currentA);
    bool readPowerVoltage();
    bool readPowerCurrent();
    bool setPowerOutput(bool enable);
    bool initializePower(double voltageV, double currentA);

    CommandSet commandSet_;
    ProtocolConfig config_;
    QString lastQueryResponse_;
#ifdef HAVE_NI_VISA
    ViSession visaRm_ = VI_NULL;
    ViSession visaInst_ = VI_NULL;
#endif
};

#endif  // QVISA_H
