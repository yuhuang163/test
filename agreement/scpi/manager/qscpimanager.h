#ifndef QSCPIMANAGER_H
#define QSCPIMANAGER_H

#include "huiling_wfp60h_profile.h"
#include "huiling_wfp60h_scpi_device.h"
#include "huiling_wfp60h_scpi_types.h"
#include "rs_cmw100_scpi_device.h"
#include "rs_cmw100_scpi_types.h"
#include "scpi_types.h"

#include <QObject>
#include <QVariant>
#include <QVariantMap>

class QScpiSerialSession;
class QScpiVisaSession;
class QSerialPort;
class ScpiTransport;

/**
 * SCPI 域工站入口：串口 / VISA 仅驱动不同；各 device 自有 Cmd，经 exec 重载下发。
 * USB 电流表与 VISA 程控电源/CMW 应使用两个 QScpiManager 实例。
 */
class QScpiManager : public QObject {
    Q_OBJECT

  public:
    explicit QScpiManager(QObject* parent = nullptr);
    explicit QScpiManager(QSerialPort* port, QObject* parent = nullptr);

    void attachSerialPort(QSerialPort* port);
    void setVisaConfig(const ScpiVisaLinkConfig& config);
    ScpiVisaLinkConfig visaConfig() const;

    ScpiLinkKind linkKind() const;
    bool ensureConnected();
    void closeConnection();

    QSerialPort* serialPort() const;
    QScpiSerialSession* session();

    void setDeviceRoute(ScpiDeviceRoute route);
    ScpiDeviceRoute deviceRoute() const;
    bool isScpiDeviceRoute() const;

    void setHuilingProfilePatch(const HuilingWfp60hScpiProfile& patch);
    void clearHuilingProfilePatch();
    HuilingWfp60hScpiProfile huilingActiveProfile() const;

    /** 从 SETTINGS 加载 VISA 会凌程控电源路由与 profile。 */
    void loadHuilingVisaFromSettings();
    /** 从 SETTINGS 加载 VISA CMW100 路由。 */
    void loadCmwVisaFromSettings();

    HuilingWfp60hScpiDevice* huilingDevice();
    RsCmw100ScpiDevice* cmwDevice();

    bool exec(HuilingScpiCmd cmd, const QVariant& param = {}, QString* errorMessage = nullptr);
    bool exec(CmwScpiCmd cmd, const QVariant& param = {}, QString* errorMessage = nullptr);

    QString lastQueryResponse() const;

    bool feedRx(const QByteArray& chunk);
    bool sendCustomMessage(const QVariantMap& map);

  signals:
    void ammeterReadingReceived(const QString& valueText);
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    void programmablePowerCurrentRead(double valueAmps, bool ok);

  private:
    void onLineReceived(const QString& line);
    bool isTransportReady(QString* errorMessage) const;
    static bool isHuilingQueryCmd(HuilingScpiCmd cmd);
    void bindActiveTransport(ScpiTransport* transport, ScpiLinkKind kind);

    QScpiSerialSession* serialSession_ = nullptr;
    QScpiVisaSession* visaSession_ = nullptr;
    HuilingWfp60hScpiDevice huilingDevice_;
    RsCmw100ScpiDevice cmwDevice_;
    ScpiTransport* activeTransport_ = nullptr;
    ScpiLinkKind linkKind_ = ScpiLinkKind::Serial;
    ScpiDeviceRoute deviceRoute_ = ScpiDeviceRoute::HuilingWfp60h;
};

#endif // QSCPIMANAGER_H
