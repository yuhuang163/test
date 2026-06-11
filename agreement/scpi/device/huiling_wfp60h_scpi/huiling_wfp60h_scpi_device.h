#ifndef HUILING_WFP60H_SCPI_DEVICE_H
#define HUILING_WFP60H_SCPI_DEVICE_H

#include "iscpi_device.h"

#include <QObject>
#include <QVariant>
#include <QVariantMap>
#include <functional>
#include <map>
#include <optional>

#include "huiling_wfp60h_profile.h"
#include "huiling_wfp60h_scpi_types.h"
#include "scpi_types.h"

class ScpiTransport;

/** 会凌 WFP60H：HuilingScpiCmd → profile 组行；串口异步收行 / VISA 同步 query。 */
class HuilingWfp60hScpiDevice : public QObject, public IScpiDevice {
    Q_OBJECT

  public:
    explicit HuilingWfp60hScpiDevice(ScpiTransport* transport, QObject* parent = nullptr);

    void setTransport(ScpiTransport* transport);

    void setProfilePatch(const HuilingWfp60hScpiProfile& patch);
    void clearProfilePatch();
    HuilingWfp60hScpiProfile activeProfile() const;

    bool set(HuilingScpiCmd cmd, const QVariant& data = {});
    bool get(HuilingScpiCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);

    // --- IScpiDevice interface ---
    bool set(int cmd, const QVariant& param) override;
    bool get(int cmd, const QVariant& param) override;
    bool isQueryCmd(int cmd) const override;

    void handleLineReceived(const QString& line);

  signals:
    void ammeterReadingReceived(const QString& valueText);
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    void programmablePowerCurrentRead(double valueAmps, bool ok);

  private:
    enum class ProgrammablePowerReadPending {
        None,
        Voltage,
        Current
    };

    bool ensureTransportOpen() const;
    bool queryMeasureLine(const QString& queryLine, bool emitAsAmmeterReading);
    bool queryProgrammablePower(const QString& queryLine, ProgrammablePowerReadPending pending);
    void registerCommand();
    void CONFigureFUNCtion(const QString& p);
    void emitProgrammablePowerReadIfPending(const QString& scpiLine);

    ScpiTransport* transport_ = nullptr;
    using CommandCallback = std::function<void(const QString&)>;
    std::map<QString, CommandCallback> commandList_;
    std::optional<HuilingWfp60hScpiProfile> profilePatch_;
    ProgrammablePowerReadPending pendingProgPowerRead_ = ProgrammablePowerReadPending::None;
};

#endif // HUILING_WFP60H_SCPI_DEVICE_H
