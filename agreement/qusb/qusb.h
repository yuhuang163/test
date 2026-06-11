#ifndef QUSB_H
#define QUSB_H

#include <QObject>
#include <QSerialPort>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "huiling_wfp60h_scpi_types.h"
#include "qprotocol_types.h"
#include "qscpimanager.h"
#include "qusb_types.h"

class QModbusManager;

/**
 * COM 电流表工站入口（历史类名 Qusb）。
 * 链路配置 + SCPI/Modbus RTU 路由；设备语义在 scpi/device、modbus/device。
 */
class Qusb : public QObject {
    Q_OBJECT

  public:
    using ProtocolType = QusbProtocolRoute;
    using PowerAction = QusbPowerAction;
    using UsbCmd = QusbCmd;

    struct ProtocolConfig {
        ProtocolType protocol = ProtocolType::Auto;
        int luxshareMachineId = 1;
        QString scpiCurrentType = QStringLiteral("CURRent");
        QString scpiCurrentMode = QStringLiteral("DC");
        QString scpiRange = QStringLiteral("500e-3");
        double scpiPowerVoltageV = 16.8;
        double scpiPowerCurrentA = 1.0;
        QString scpiSetVoltageCmd = QStringLiteral("VOLT %1");
        QString scpiSetCurrentCmd = QStringLiteral("CURR %1");
        QString scpiOutputOnCmd = QStringLiteral("OUTP ON");
        QString scpiOutputOffCmd = QStringLiteral("OUTP OFF");
        QString scpiReadVoltageCmd = QStringLiteral("MEASure:VOLTage:DC?");
        QString scpiReadCurrentCmd = QStringLiteral("MEASure:CURRent:DC? 500e-3");
    };

    explicit Qusb(QSerialPort* port, QObject* parent = nullptr);

    QSerialPort* serialPort() const {
        return serialPort_;
    }

    QScpiManager* scpiManager() {
        return &scpiManager_;
    }

    void setModbusManager(QModbusManager* manager);

    void setProtocolConfig(const ProtocolConfig& config);
    ProtocolConfig protocolConfig() const;

    void setLinkConfig(const QusbLinkConfig& config);
    QusbLinkConfig linkConfig() const;

    bool sendPowerInstruction(QusbPowerAction action);
    void parseCmd(const QByteArray& byte);

    void set(QusbCmd cmd, const QVariant& data = {});
    void get(QusbCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);

    void sendCmd(const QString& cmd) {
        scpiManager_.exec(HuilingScpiCmd::SendRawLine, cmd);
    }
    bool configureProgrammablePower(double voltageV, double currentA);
    bool setProgrammablePowerOutput(bool enable);
    bool readProgrammablePowerVoltage();
    bool readProgrammablePowerCurrent();
    bool initializeProgrammablePower();

  signals:
    void reportReceived(const ProtocolReport& report);
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    void programmablePowerCurrentRead(double valueAmps, bool ok);

  private:
    QusbProtocolRoute resolveRouteForInput(const QByteArray& input) const;
    QusbProtocolRoute resolveRouteForOutput() const;
    void syncModbusManagerRtuRoute();
    void syncScpiDeviceRoute();
    bool handleModbusAction(QusbPowerAction action);
    bool dispatchScpiPowerAction(QusbPowerAction action);
    HuilingScpiCmd scpiCmdFromUsbCmd(QusbCmd cmd) const;

    QSerialPort* serialPort_ = nullptr;
    QScpiManager scpiManager_;
    QModbusManager* modbusManager_ = nullptr;
    QusbLinkConfig linkConfig_;
};

#endif // QUSB_H
