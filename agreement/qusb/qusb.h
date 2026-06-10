#ifndef QUSB_H
#define QUSB_H
#include <QObject>
#include <QSerialPort>
#include <functional>
#include <map>

#include "qprotocol_types.h"
#include "scpi_line_codec.h"

class QModbusManager;

class Qusb : public QSerialPort {
    Q_OBJECT
  public:
    enum class ProtocolType {
        Auto,
        Scpi,
        HqModbus,
        LxModbus
    };

    enum class PowerAction {
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

    enum class UsbCmd {
        PowerActionCmd,                  // set/get: Qusb::PowerAction(int)
        ProtocolConfigCmd,               // set: QVariantMap
        SendScpiLine,                    // set: QString
        ConfigureProgrammablePower,      // set: QVariantMap{voltage:double,current:double}
        ProgrammablePowerOutput,         // set: bool
        ReadProgrammablePowerVoltageCmd, // get
        ReadProgrammablePowerCurrentCmd, // get
        InitializeProgrammablePowerCmd   // set/get
    };

    struct ProtocolConfig {
        ProtocolType protocol = ProtocolType::Auto;
        int luxshareMachineId = 1;
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

    explicit Qusb(QSerialPort* parent = nullptr);
    ~Qusb();

    void setProtocolConfig(const ProtocolConfig& config);
    ProtocolConfig protocolConfig() const;
    /** 注入 QModbusManager 后，HQ/LX RTU 收发走 modbus manager（拆 qusb 过渡）。 */
    void setModbusManager(QModbusManager* manager);
    bool sendPowerInstruction(PowerAction action);

    void sendCmd(QString cmd);
    void sendCONF(QString current, QString dc, QString range);
    void getMEASure(QString mac);
    void gethqMEASure();
    void getlxMEASure(int mechine);
    void processlxModbusRTUData(const QByteArray& data);
    void sethqMEASure();
    void getCONF(QString mac);
    void parseCmd(const QByteArray& byte);
    void set(UsbCmd cmd, const QVariant& data = {});
    void get(UsbCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);
    bool configureProgrammablePower(double voltageV, double currentA);
    bool setProgrammablePowerOutput(bool enable);
    bool readProgrammablePowerVoltage();
    bool readProgrammablePowerCurrent();
    bool initializeProgrammablePower();

  signals:
    /** 统一上行数据信封（电流表读数等） */
    void reportReceived(const ProtocolReport& report);
    /// 程控电源 SCPI 读电压完成（串口异步回包）。valueVolts 单位 V，ok 为数值解析是否成功
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    /// 程控电源 SCPI 读电流完成，valueAmps 单位 A
    void programmablePowerCurrentRead(double valueAmps, bool ok);

  protected:
    void emitReport(const QString& reportType, const QVariant& payload = QVariant()) {
        emit reportReceived(ProtocolReport(reportType, payload));
    }

  private:
    enum class ProgrammablePowerReadPending {
        None,
        Voltage,
        Current
    };
    void emitProgrammablePowerReadIfPending(const QString& scpiLine);
    ProtocolType resolveProtocolForInput(const QByteArray& input) const;
    ProtocolType resolveProtocolForOutput() const;
    bool handleScpiAction(PowerAction action);
    bool handleHqAction(PowerAction action);
    bool handleLxAction(PowerAction action);
    void processScpiData(const QByteArray& byte);
    void processModbusRTUData(const QByteArray& data);
    /** 程控/量测 SCPI 前合并会凌 WFP60H profile（SETTINGS 与 qvisa 回退链一致）。 */
    void mergeHuilingScpiProfile();
    void syncModbusManagerRtuRoute();
    QString parameter;
    ScpiLineCodec scpiLineCodec_;
    QSerialPort* serialPort;
    int sssss = 255;
    typedef std::function<void(QString)> callback;
    std::map<QString, callback> commandList;
    void registerCommand();
    void CONFigureFUNCtion(QString p);
    QModbusManager* modbusManager_ = nullptr;
    ProtocolConfig protocolConfig_;
    ProgrammablePowerReadPending pendingProgPowerRead_ = ProgrammablePowerReadPending::None;

  private slots:
    void processCmd(QString cmd, QString parameter);
};

#endif // QUSB_H
